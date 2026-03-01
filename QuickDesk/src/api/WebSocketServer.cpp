// Copyright 2026 QuickDesk Authors

#include "WebSocketServer.h"
#include "ApiHandler.h"
#include "infra/log/log.h"
#include <QJsonDocument>
#include <QJsonObject>

namespace quickdesk {

WebSocketApiServer::WebSocketApiServer(MainController* controller,
                                       QObject* parent)
    : QObject(parent)
    , m_handler(new ApiHandler(controller, this)) {}

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

        if (m_authToken.isEmpty()) {
            m_authenticatedClients.insert(client);
        }

        LOG_INFO("WebSocket API client connected from {}",
                 client->peerAddress().toString().toStdString());
    }
}

void WebSocketApiServer::onTextMessageReceived(const QString& message) {
    auto* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }

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

    if (!m_authToken.isEmpty() && !m_authenticatedClients.contains(client)) {
        if (!authenticateClient(client, request)) {
            return;
        }
    }

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

    m_clients.remove(client);
    m_authenticatedClients.remove(client);
    client->deleteLater();
    LOG_INFO("WebSocket API client disconnected");
}

bool WebSocketApiServer::authenticateClient(QWebSocket* client,
                                            const QJsonObject& msg) {
    auto method = msg["method"].toString();
    if (method != "auth") {
        QJsonObject err;
        err["error"] = QJsonObject{
            {"code", 401},
            {"message", "Authentication required. Send {\"method\":\"auth\",\"params\":{\"token\":\"...\"}} first."}
        };
        if (msg.contains("id")) {
            err["id"] = msg["id"];
        }
        client->sendTextMessage(
            QString::fromUtf8(QJsonDocument(err).toJson(QJsonDocument::Compact)));
        return false;
    }

    auto token = msg["params"].toObject()["token"].toString();
    if (token != m_authToken) {
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

    QJsonObject resp;
    resp["result"] = QJsonObject{{"authenticated", true}};
    if (msg.contains("id")) {
        resp["id"] = msg["id"];
    }
    client->sendTextMessage(
        QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
    return true;
}

} // namespace quickdesk
