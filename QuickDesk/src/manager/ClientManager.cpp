// Copyright 2026 QuickDesk Authors

#include "ClientManager.h"
#include "NativeMessaging.h"
#include <QDebug>
#include <QUuid>
#include <QJsonArray>

namespace quickdesk {

ClientManager::ClientManager(QObject* parent)
    : QObject(parent)
{
}

void ClientManager::setMessaging(NativeMessaging* messaging)
{
    if (m_messaging) {
        QObject::disconnect(m_messaging, nullptr, this, nullptr);
    }

    m_messaging = messaging;

    if (m_messaging) {
        connect(m_messaging, &NativeMessaging::messageReceived,
                this, &ClientManager::onMessageReceived);
        connect(m_messaging, &NativeMessaging::errorOccurred,
                this, &ClientManager::onMessagingError);
    }
}

QString ClientManager::connectToHost(const QString& deviceId,
                                     const QString& accessCode,
                                     const QString& serverUrl)
{
    if (!m_messaging || !m_messaging->isReady()) {
        emit errorOccurred("", "NOT_READY", "Client process is not ready");
        return QString();
    }

    QString connectionId = generateConnectionId();

    // Create connection info
    ConnectionInfo conn;
    conn.connectionId = connectionId;
    conn.deviceId = deviceId;
    conn.state = "connecting";
    m_connections[connectionId] = conn;

    // Send connect message
    QJsonObject message;
    message["type"] = "connectToHost";
    message["connectionId"] = connectionId;
    message["deviceId"] = deviceId;
    message["accessCode"] = accessCode;
    message["serverUrl"] = serverUrl;

    qInfo() << "Connecting to host:" << deviceId << "connectionId:" << connectionId;
    m_messaging->sendMessage(message);

    emit connectionCountChanged();
    emit connectionAdded(connectionId);
    emit connectionListChanged();

    // Set as active if first connection
    if (m_activeConnectionId.isEmpty()) {
        setActiveConnectionId(connectionId);
    }

    return connectionId;
}

void ClientManager::disconnectFromHost(const QString& connectionId)
{
    if (!m_messaging || !m_messaging->isReady()) {
        return;
    }

    QJsonObject message;
    message["type"] = "disconnectFromHost";
    message["connectionId"] = connectionId;
    m_messaging->sendMessage(message);

    m_connections.remove(connectionId);

    emit connectionCountChanged();
    emit connectionRemoved(connectionId);
    emit connectionListChanged();

    // Update active connection if needed
    if (m_activeConnectionId == connectionId) {
        if (m_connections.isEmpty()) {
            setActiveConnectionId(QString());
        } else {
            setActiveConnectionId(m_connections.firstKey());
        }
    }
}

void ClientManager::disconnectAll()
{
    if (!m_messaging || !m_messaging->isReady()) {
        return;
    }

    QJsonObject message;
    message["type"] = "disconnectAll";
    m_messaging->sendMessage(message);

    m_connections.clear();
    m_activeConnectionId.clear();

    emit connectionCountChanged();
    emit activeConnectionChanged();
    emit connectionListChanged();
}

void ClientManager::sendHello()
{
    if (!m_messaging || !m_messaging->isReady()) {
        emit errorOccurred("", "NOT_READY", "Client process is not ready");
        return;
    }

    QJsonObject message;
    message["type"] = "hello";
    m_messaging->sendMessage(message);
}

void ClientManager::sendMouseMove(const QString& connectionId, int x, int y)
{
    sendMouseEvent(connectionId, "move", x, y, 0, 0);
}

void ClientManager::sendMousePress(const QString& connectionId, int x, int y, int button)
{
    sendMouseEvent(connectionId, "press", x, y, button, 0);
}

void ClientManager::sendMouseRelease(const QString& connectionId, int x, int y, int button)
{
    sendMouseEvent(connectionId, "release", x, y, button, 0);
}

void ClientManager::sendMouseWheel(const QString& connectionId, int x, int y, int delta)
{
    sendMouseEvent(connectionId, "wheel", x, y, 0, delta);
}

void ClientManager::sendKeyPress(const QString& connectionId, int keyCode, int modifiers)
{
    sendKeyboardEvent(connectionId, "press", keyCode, modifiers);
}

void ClientManager::sendKeyRelease(const QString& connectionId, int keyCode, int modifiers)
{
    sendKeyboardEvent(connectionId, "release", keyCode, modifiers);
}

void ClientManager::syncClipboard(const QString& connectionId, const QString& text)
{
    if (!m_messaging || !m_messaging->isReady()) {
        return;
    }

    QJsonObject message;
    message["type"] = "clipboardSync";
    message["connectionId"] = connectionId;
    message["text"] = text;
    m_messaging->sendMessage(message);
}

int ClientManager::connectionCount() const
{
    return m_connections.size();
}

QString ClientManager::activeConnectionId() const
{
    return m_activeConnectionId;
}

void ClientManager::setActiveConnectionId(const QString& id)
{
    if (m_activeConnectionId != id) {
        m_activeConnectionId = id;
        emit activeConnectionChanged();
    }
}

QList<ConnectionInfo> ClientManager::connections() const
{
    return m_connections.values();
}

ConnectionInfo ClientManager::getConnection(const QString& connectionId) const
{
    return m_connections.value(connectionId);
}

QStringList ClientManager::connectionIds() const
{
    return m_connections.keys();
}

QString ClientManager::getConnectionState(const QString& connectionId) const
{
    if (m_connections.contains(connectionId)) {
        QString state = m_connections[connectionId].state;
        // Translate state to Chinese
        if (state == "connecting") return "连接中...";
        if (state == "connected") return "已连接";
        if (state == "disconnected") return "已断开";
        if (state == "failed") return "连接失败";
        return state;
    }
    return "";
}

void ClientManager::onMessageReceived(const QJsonObject& message)
{
    QString type = message["type"].toString();
    
    qDebug() << "Client received message:" << type;

    if (type == "helloResponse") {
        handleHelloResponse(message);
    } else if (type == "connectionStateChanged") {
        handleConnectionStateChanged(message);
    } else if (type == "connectionListChanged") {
        handleConnectionListChanged(message);
    } else if (type == "videoFrameReady") {
        handleVideoFrameReady(message);
    } else if (type == "clipboardReceived") {
        handleClipboardReceived(message);
    } else if (type == "error") {
        handleError(message);
    } else if (type == "connectionFailed") {
        handleConnectionFailed(message);
    } else if (type == "onHostConnected") {
        handleHostConnected(message);
    } else if (type == "onHostDisconnected") {
        handleHostDisconnected(message);
    } else if (type == "onHostConnectionFailed") {
        handleHostConnectionFailed(message);
    } else {
        qWarning() << "Unknown message type from client:" << type;
    }
}

void ClientManager::onMessagingError(const QString& error)
{
    emit errorOccurred("", "MESSAGING_ERROR", error);
}

QString ClientManager::generateConnectionId()
{
    return QString("conn_%1").arg(++m_connectionCounter);
}

void ClientManager::handleHelloResponse(const QJsonObject& message)
{
    QString version = message["version"].toString();
    qInfo() << "Client hello response, version:" << version;
    emit helloResponseReceived(version);
}

void ClientManager::handleConnectionStateChanged(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString state = message["state"].toString();
    QJsonObject hostInfo = message["hostInfo"].toObject();

    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].state = state;
        
        if (hostInfo.contains("resolution")) {
            QString resolution = hostInfo["resolution"].toString();
            QStringList parts = resolution.split('x');
            if (parts.size() == 2) {
                m_connections[connectionId].width = parts[0].toInt();
                m_connections[connectionId].height = parts[1].toInt();
            }
        }
        if (hostInfo.contains("deviceName")) {
            m_connections[connectionId].deviceName = hostInfo["deviceName"].toString();
        }
    }

    qInfo() << "Connection" << connectionId << "state changed to:" << state;
    emit connectionStateChanged(connectionId, state, hostInfo);
    emit connectionListChanged();
}

void ClientManager::handleConnectionListChanged(const QJsonObject& message)
{
    m_connections.clear();
    
    QJsonArray connections = message["connections"].toArray();
    for (const QJsonValue& value : connections) {
        QJsonObject obj = value.toObject();
        ConnectionInfo conn;
        conn.connectionId = obj["connectionId"].toString();
        conn.deviceId = obj["deviceId"].toString();
        conn.deviceName = obj["deviceName"].toString();
        conn.state = obj["state"].toString();
        conn.connectedAt = obj["connectedAt"].toString();
        
        m_connections[conn.connectionId] = conn;
    }

    emit connectionCountChanged();
    emit connectionListChanged();
}

void ClientManager::handleVideoFrameReady(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    int frameIndex = message["frameIndex"].toInt();
    
    emit videoFrameReady(connectionId, frameIndex);
}

void ClientManager::handleClipboardReceived(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString text = message["text"].toString();
    
    emit clipboardReceived(connectionId, text);
}

void ClientManager::handleError(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString code = message["code"].toString();
    QString errorMsg = message["message"].toString();
    
    qWarning() << "Client error:" << connectionId << code << errorMsg;
    emit errorOccurred(connectionId, code, errorMsg);
}

void ClientManager::handleConnectionFailed(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString errorCode = message["errorCode"].toString();
    QString errorMsg = message["message"].toString();
    
    qWarning() << "Connection failed:" << connectionId 
               << "error:" << errorCode << "-" << errorMsg;
    
    // Update connection state
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].state = "failed";
        emit connectionStateChanged(connectionId, "failed", QJsonObject());
    }
    
    // Emit error with specific error code
    emit errorOccurred(connectionId, errorCode, errorMsg);
    
    // Remove failed connection from list
    m_connections.remove(connectionId);
    emit connectionCountChanged();
    emit connectionRemoved(connectionId);
    emit connectionListChanged();
    
    // Update active connection if needed
    if (m_activeConnectionId == connectionId) {
        if (m_connections.isEmpty()) {
            setActiveConnectionId(QString());
        } else {
            setActiveConnectionId(m_connections.firstKey());
        }
    }
}

void ClientManager::handleHostConnected(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    
    qInfo() << "Host connected:" << connectionId;
    
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].state = "connected";
        emit connectionStateChanged(connectionId, "connected", QJsonObject());
    }
    emit connectionListChanged();
}

void ClientManager::handleHostDisconnected(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    
    qInfo() << "Host disconnected:" << connectionId;
    
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].state = "disconnected";
        emit connectionStateChanged(connectionId, "disconnected", QJsonObject());
    }
    
    // Remove disconnected connection
    m_connections.remove(connectionId);
    emit connectionCountChanged();
    emit connectionRemoved(connectionId);
    emit connectionListChanged();
    
    // Update active connection if needed
    if (m_activeConnectionId == connectionId) {
        if (m_connections.isEmpty()) {
            setActiveConnectionId(QString());
        } else {
            setActiveConnectionId(m_connections.firstKey());
        }
    }
}

void ClientManager::handleHostConnectionFailed(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    int errorCode = message["errorCode"].toInt();
    
    qWarning() << "Host connection failed:" << connectionId << "error code:" << errorCode;
    
    // Update connection state
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].state = "failed";
        emit connectionStateChanged(connectionId, "failed", QJsonObject());
    }
    
    // Map protocol::ErrorCode to user-friendly message
    QString errorMsg;
    switch (errorCode) {
        case 1: errorMsg = "认证失败"; break;
        case 2: errorMsg = "通道错误"; break;
        case 3: errorMsg = "连接超时"; break;
        case 4: errorMsg = "网络错误"; break;
        default: errorMsg = QString("连接失败 (错误码: %1)").arg(errorCode); break;
    }
    
    emit errorOccurred(connectionId, "CONNECTION_FAILED", errorMsg);
    
    // Remove failed connection
    m_connections.remove(connectionId);
    emit connectionCountChanged();
    emit connectionRemoved(connectionId);
    emit connectionListChanged();
    
    // Update active connection if needed
    if (m_activeConnectionId == connectionId) {
        if (m_connections.isEmpty()) {
            setActiveConnectionId(QString());
        } else {
            setActiveConnectionId(m_connections.firstKey());
        }
    }
}

void ClientManager::sendMouseEvent(const QString& connectionId, const QString& eventType,
                                   int x, int y, int button, int wheelDelta)
{
    if (!m_messaging || !m_messaging->isReady()) {
        return;
    }

    QJsonObject message;
    message["type"] = "mouseEvent";
    message["connectionId"] = connectionId;
    message["eventType"] = eventType;
    message["x"] = x;
    message["y"] = y;
    message["button"] = button;
    message["wheelDelta"] = wheelDelta;
    m_messaging->sendMessage(message);
}

void ClientManager::sendKeyboardEvent(const QString& connectionId, const QString& eventType,
                                      int keyCode, int modifiers)
{
    if (!m_messaging || !m_messaging->isReady()) {
        return;
    }

    QJsonObject message;
    message["type"] = "keyboardEvent";
    message["connectionId"] = connectionId;
    message["eventType"] = eventType;
    message["keyCode"] = keyCode;
    message["modifiers"] = modifiers;
    m_messaging->sendMessage(message);
}

} // namespace quickdesk
