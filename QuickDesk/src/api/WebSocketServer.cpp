// Copyright 2026 QuickDesk Authors

#include "WebSocketServer.h"
#include "ApiHandler.h"
#include "infra/log/log.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>

namespace quickdesk {

WebSocketApiServer::WebSocketApiServer(MainController* controller,
                                       QObject* parent)
    : QObject(parent)
    , m_handler(new ApiHandler(controller, this))
    , m_security(new SecurityManager(this)) {
    connect(m_security, &SecurityManager::sessionExpired,
            this, &WebSocketApiServer::onSessionExpired);

    auto logDir = QCoreApplication::applicationDirPath() + "/logs";
    m_security->initAuditLog(logDir);
}

WebSocketApiServer::~WebSocketApiServer() {
    stop();
}

bool WebSocketApiServer::start(quint16 port, const QHostAddress& address) {
    if (m_server) {
        return m_server->isListening();
    }

    m_server = new QWebSocketServer(
        QStringLiteral("QuickDesk API"),
        QWebSocketServer::NonSecureMode, this);

    if (!m_server->listen(address, port)) {
        LOG_ERROR("WebSocket API server failed to listen on port {}: {}",
                  port, m_server->errorString().toStdString());
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QWebSocketServer::newConnection,
            this, &WebSocketApiServer::onNewConnection);

    LOG_INFO("WebSocket API server listening on {}:{}",
             address.toString().toStdString(), m_server->serverPort());
    return true;
}

void WebSocketApiServer::stop() {
    if (!m_server) {
        return;
    }

    for (auto* client : m_clients) {
        client->close();
    }
    m_clients.clear();
    m_authenticatedClients.clear();
    m_clientInfo.clear();
    m_clientIdMap.clear();

    m_server->close();
    delete m_server;
    m_server = nullptr;
    LOG_INFO("WebSocket API server stopped");
}

bool WebSocketApiServer::isListening() const {
    return m_server && m_server->isListening();
}

quint16 WebSocketApiServer::port() const {
    return m_server ? m_server->serverPort() : 0;
}

void WebSocketApiServer::setAuthToken(const QString& token) {
    m_authToken = token;
    m_security->setFullAccessToken(token);
}

void WebSocketApiServer::broadcastEvent(const QString& event,
                                        const QJsonObject& data) {
    QJsonObject msg;
    msg["event"] = event;
    msg["data"] = data;

    auto text = QString::fromUtf8(
        QJsonDocument(msg).toJson(QJsonDocument::Compact));

    auto& targets = m_authToken.isEmpty() ? m_clients : m_authenticatedClients;
    for (auto* client : targets) {
        client->sendTextMessage(text);
    }
}

QString WebSocketApiServer::clientId(QWebSocket* client) const {
    auto it = m_clientInfo.constFind(client);
    if (it != m_clientInfo.constEnd()) {
        return it->id;
    }
    return client->peerAddress().toString();
}

void WebSocketApiServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        auto* client = m_server->nextPendingConnection();
        if (!client) {
            continue;
        }

        connect(client, &QWebSocket::textMessageReceived,
                this, &WebSocketApiServer::onTextMessageReceived);
        connect(client, &QWebSocket::disconnected,
                this, &WebSocketApiServer::onClientDisconnected);

        m_clients.insert(client);

        ClientInfo info;
        info.id = QString("client_%1").arg(m_nextClientId++);
        info.permission = SecurityManager::FullControl;
        m_clientInfo[client] = info;
        m_clientIdMap[info.id] = client;

        if (m_authToken.isEmpty()) {
            m_authenticatedClients.insert(client);
        }

        m_security->recordActivity(info.id);

        LOG_INFO("WebSocket API client connected: {} from {}",
                 info.id.toStdString(),
                 client->peerAddress().toString().toStdString());
    }
}

void WebSocketApiServer::onTextMessageReceived(const QString& message) {
    auto* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }

    auto cid = clientId(client);
    m_security->recordActivity(cid);

    auto doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        QJsonObject err;
        err["error"] = QJsonObject{
            {"code", -32700},
            {"message", "Parse error"}
        };
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        return;
    }

    auto request = doc.object();
    auto method = request["method"].toString();
    auto params = request["params"].toObject();

    if (!m_authToken.isEmpty() && !m_authenticatedClients.contains(client)) {
        if (!authenticateClient(client, request)) {
            return;
        }
        if (method == "auth") {
            return;
        }
    }

    auto permission = m_clientInfo.value(client).permission;

    if (!m_security->checkRateLimit(cid)) {
        m_security->logAudit(cid, method, params, false, "rate_limit");
        QJsonObject err;
        err["error"] = QJsonObject{
            {"code", 429},
            {"message", "Rate limit exceeded. Please slow down."}
        };
        if (request.contains("id")) {
            err["id"] = request["id"];
        }
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        return;
    }

    if (!m_security->isMethodAllowedForPermission(method, permission)) {
        m_security->logAudit(cid, method, params, false, "permission_denied");
        QJsonObject err;
        err["error"] = QJsonObject{
            {"code", 403},
            {"message",
             QString("Permission denied. Method '%1' requires full control access.")
                 .arg(method)}
        };
        if (request.contains("id")) {
            err["id"] = request["id"];
        }
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        return;
    }

    if (method == "connectToHost") {
        auto deviceId = params["deviceId"].toString();
        if (!m_security->isDeviceAllowed(deviceId)) {
            m_security->logAudit(cid, method, params, false, "device_not_allowed");
            QJsonObject err;
            err["error"] = QJsonObject{
                {"code", 403},
                {"message",
                 QString("Device '%1' is not in the allowed device list.")
                     .arg(deviceId)}
            };
            if (request.contains("id")) {
                err["id"] = request["id"];
            }
            client->sendTextMessage(
                QString::fromUtf8(
                    QJsonDocument(err).toJson(QJsonDocument::Compact)));
            return;
        }
    }

    if (m_security->isDangerousOperation(method, params)) {
        m_security->logAudit(cid, method, params, false, "dangerous_operation");
        emit m_security->dangerousOperationBlocked(cid, method, params);
        QJsonObject err;
        err["error"] = QJsonObject{
            {"code", 403},
            {"message",
             "This operation has been blocked for safety. Dangerous operations "
             "(shutdown, format, rm -rf, Alt+F4, Ctrl+Alt+Delete, disconnect all) "
             "are not allowed via the API."}
        };
        if (request.contains("id")) {
            err["id"] = request["id"];
        }
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        return;
    }

    m_security->logAudit(cid, method, params, true);

    auto response = m_handler->handleRequest(request);
    if (request.contains("id")) {
        response["id"] = request["id"];
    }

    client->sendTextMessage(
        QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
}

void WebSocketApiServer::onClientDisconnected() {
    auto* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }

    auto cid = clientId(client);
    m_security->removeClient(cid);
    m_clients.remove(client);
    m_authenticatedClients.remove(client);
    m_clientIdMap.remove(cid);
    m_clientInfo.remove(client);
    client->deleteLater();
    LOG_INFO("WebSocket API client disconnected: {}", cid.toStdString());
}

void WebSocketApiServer::onSessionExpired(const QString& expiredClientId) {
    auto it = m_clientIdMap.find(expiredClientId);
    if (it == m_clientIdMap.end()) {
        return;
    }

    auto* client = it.value();
    m_security->logAudit(expiredClientId, "SESSION_TIMEOUT", {}, false,
                         "session_timeout");

    QJsonObject msg;
    msg["event"] = "sessionTimeout";
    msg["data"] = QJsonObject{
        {"message", "Session timed out due to inactivity."}
    };
    client->sendTextMessage(
        QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
    client->close();
}

bool WebSocketApiServer::authenticateClient(QWebSocket* client,
                                            const QJsonObject& msg) {
    auto method = msg["method"].toString();
    if (method != "auth") {
        QJsonObject err;
        err["error"] = QJsonObject{
            {"code", 401},
            {"message",
             "Authentication required. Send "
             "{\"method\":\"auth\",\"params\":{\"token\":\"...\"}} first."}
        };
        if (msg.contains("id")) {
            err["id"] = msg["id"];
        }
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        return false;
    }

    auto token = msg["params"].toObject()["token"].toString();
    auto permission = m_security->authenticateToken(token);

    if (permission == SecurityManager::NoAccess) {
        auto cid = clientId(client);
        m_security->logAudit(cid, "auth", {}, false, "invalid_token");
        emit m_security->anomalyDetected(cid, "Failed authentication attempt");

        QJsonObject err;
        err["error"] = QJsonObject{
            {"code", 401},
            {"message", "Invalid token"}
        };
        if (msg.contains("id")) {
            err["id"] = msg["id"];
        }
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        client->close();
        return false;
    }

    m_authenticatedClients.insert(client);
    m_clientInfo[client].permission = permission;

    auto cid = clientId(client);
    m_security->logAudit(cid, "auth", {}, true);

    QJsonObject resp;
    resp["result"] = QJsonObject{
        {"authenticated", true},
        {"permission", permission == SecurityManager::FullControl
                           ? "full_control"
                           : "read_only"},
    };
    if (msg.contains("id")) {
        resp["id"] = msg["id"];
    }
    client->sendTextMessage(
        QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
    return true;
}

} // namespace quickdesk
