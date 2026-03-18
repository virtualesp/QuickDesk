// Copyright 2026 QuickDesk Authors
// Client Manager Implementation

#include "ClientManager.h"
#include "NativeMessaging.h"
#include "core/localconfigcenter.h"
#include "infra/log/log.h"
#include <QUuid>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMimeData>
#include <QStandardPaths>
#include <QUrl>

namespace quickdesk {

ClientManager::ClientManager(QObject* parent)
    : QObject(parent)
    , m_sharedMemoryManager(std::make_unique<SharedMemoryManager>(this))
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
    } else {
        // Clear all state when messaging is disconnected (process stopped)
        QStringList ids = m_connections.keys();
        for (const auto& connId : ids) {
            m_connections[connId].rtcState = RtcStatus::Disconnected;
            emit connectionStateChanged(connId, "disconnected", QJsonObject());
            m_sharedMemoryManager->detach(connId);
            m_connections.remove(connId);
            emit connectionRemoved(connId);
        }

        m_activeConnectionId.clear();
        // Note: Don't reset m_connectionCounter to avoid ID conflicts after restart

        if (!ids.isEmpty()) {
            emit connectionCountChanged();
            emit activeConnectionChanged();
            emit connectionListChanged();
        }
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
    conn.rtcState = RtcStatus::Connecting;
    m_connections[connectionId] = conn;

    QJsonObject message;
    message["type"] = "connectToHost";
    message["connectionId"] = connectionId;
    message["deviceId"] = deviceId;
    message["accessCode"] = accessCode;
    message["serverUrl"] = serverUrl;
    
    // Always send the latest video codec preference from settings
    QString videoCodec = core::LocalConfigCenter::instance().preferredVideoCodec();
    if (!videoCodec.isEmpty()) {
        message["preferredVideoCodec"] = videoCodec;
    }
    
    if (!m_iceConfig.isEmpty()) {
        message["iceConfig"] = m_iceConfig;
        QJsonArray servers = m_iceConfig.value("iceServers").toArray();
        LOG_INFO("Client: Sending ICE config with {} server(s), lifetime={}",
                 servers.size(),
                 m_iceConfig.value("lifetimeDuration").toString("unset").toStdString());
    } else {
        LOG_INFO("Client: No ICE config available, client will use defaults");
    }

    LOG_INFO("Connecting to host: {} connectionId: {}", deviceId.toStdString(), connectionId.toStdString());
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

    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].rtcState = RtcStatus::Disconnected;
        emit connectionStateChanged(connectionId, "disconnected", QJsonObject());
    }

    m_sharedMemoryManager->detach(connectionId);

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

    QStringList ids = m_connections.keys();
    for (const auto& connId : ids) {
        m_connections[connId].rtcState = RtcStatus::Disconnected;
        emit connectionStateChanged(connId, "disconnected", QJsonObject());
        m_sharedMemoryManager->detach(connId);
        m_connections.remove(connId);
        emit connectionRemoved(connId);
    }

    m_activeConnectionId.clear();

    emit connectionCountChanged();
    emit activeConnectionChanged();
    emit connectionListChanged();
}

void ClientManager::sendHello(const QString& deviceId,
                              const QString& preferredVideoCodec)
{
    if (!m_messaging || !m_messaging->isReady()) {
        emit errorOccurred("", "NOT_READY", "Client process is not ready");
        return;
    }

    QJsonObject message;
    message["type"] = "hello";
    if (!deviceId.isEmpty()) {
        message["deviceId"] = deviceId;
    }
    if (!preferredVideoCodec.isEmpty()) {
        message["preferredVideoCodec"] = preferredVideoCodec;
    }
    m_messaging->sendMessage(message);
}

void ClientManager::sendMouseMove(const QString& connectionId, int x, int y)
{
    sendMouseEvent(connectionId, "move", x, y, 0, 0, 0);
}

void ClientManager::sendMousePress(const QString& connectionId, int x, int y, int button)
{
    sendMouseEvent(connectionId, "press", x, y, button, 0, 0);
}

void ClientManager::sendMouseRelease(const QString& connectionId, int x, int y, int button)
{
    sendMouseEvent(connectionId, "release", x, y, button, 0, 0);
}

void ClientManager::sendMouseWheel(const QString& connectionId, int x, int y, int deltaX, int deltaY)
{
    sendMouseEvent(connectionId, "wheel", x, y, 0, deltaX, deltaY);
}

void ClientManager::sendKeyPress(const QString& connectionId, int nativeScanCode, int lockStates)
{
    sendKeyboardEvent(connectionId, "press", nativeScanCode, lockStates);
}

void ClientManager::sendKeyRelease(const QString& connectionId, int nativeScanCode, int lockStates)
{
    sendKeyboardEvent(connectionId, "release", nativeScanCode, lockStates);
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

void ClientManager::sendAgentCommand(const QString& connectionId,
                                     const QString& jsonData)
{
    if (!m_messaging || !m_messaging->isReady()) {
        return;
    }

    QJsonObject message;
    message["type"] = "agentBridgeSend";
    message["connectionId"] = connectionId;
    message["data"] = jsonData;
    m_messaging->sendMessage(message);
}

void ClientManager::setTargetFramerate(const QString& connectionId, int framerate)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot set framerate: messaging not ready");
        return;
    }

    // Clamp to valid range
    framerate = qBound(1, framerate, 60);

    LOG_INFO("Setting target framerate for {}: {} FPS", 
             connectionId.toStdString(), framerate);

    QJsonObject message;
    message["type"] = "setFramerate";
    message["connectionId"] = connectionId;
    message["framerate"] = framerate;
    m_messaging->sendMessage(message);
}

void ClientManager::setResolution(const QString& connectionId, int width, int height, int dpi)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot set resolution: messaging not ready");
        return;
    }

    // Validate dimensions
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        LOG_WARN("Invalid resolution: {}x{}", width, height);
        return;
    }

    LOG_INFO("Setting resolution for {}: {}x{} @ {} DPI", 
             connectionId.toStdString(), width, height, dpi);

    QJsonObject message;
    message["type"] = "setResolution";
    message["connectionId"] = connectionId;
    message["width"] = width;
    message["height"] = height;
    message["dpi"] = dpi;
    m_messaging->sendMessage(message);
}

void ClientManager::setFramerateBoost(const QString& connectionId, bool enabled, 
                                      int captureIntervalMs, int boostDurationMs)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot set framerate boost: messaging not ready");
        return;
    }

    // Clamp to valid ranges
    captureIntervalMs = qBound(10, captureIntervalMs, 1000);
    boostDurationMs = qBound(100, boostDurationMs, 1000);

    LOG_INFO("Setting framerate boost for {}: enabled={}, interval={}ms, duration={}ms", 
             connectionId.toStdString(), enabled, captureIntervalMs, boostDurationMs);

    QJsonObject message;
    message["type"] = "setFramerateBoost";
    message["connectionId"] = connectionId;
    message["enabled"] = enabled;
    message["captureIntervalMs"] = captureIntervalMs;
    message["boostDurationMs"] = boostDurationMs;
    m_messaging->sendMessage(message);
}

void ClientManager::setBitrate(const QString& connectionId, int minBitrateBps)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot set bitrate: messaging not ready");
        return;
    }

    // Validate bitrate (allow 0 to disable, or reasonable range)
    if (minBitrateBps < 0) {
        LOG_WARN("Invalid bitrate: {} (must be >= 0)", minBitrateBps);
        return;
    }

    LOG_INFO("Setting bitrate for {}: {} MiB ({} bps)", 
             connectionId.toStdString(), 
             minBitrateBps / 1024.0 / 1024.0, 
             minBitrateBps);

    QJsonObject message;
    message["type"] = "setBitrate";
    message["connectionId"] = connectionId;
    message["minBitrateBps"] = minBitrateBps;
    m_messaging->sendMessage(message);
}

void ClientManager::setAudioEnabled(const QString& connectionId, bool enabled)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot set audio: messaging not ready");
        return;
    }

    LOG_INFO("Setting audio enabled for {}: {}", 
             connectionId.toStdString(), enabled);

    QJsonObject message;
    message["type"] = "setAudioEnabled";
    message["connectionId"] = connectionId;
    message["enabled"] = enabled;
    m_messaging->sendMessage(message);
}

void ClientManager::sendAction(const QString& connectionId, const QString& action)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot send action: messaging not ready");
        return;
    }

    LOG_INFO("Sending action '{}' for {}", action.toStdString(),
             connectionId.toStdString());

    QJsonObject message;
    message["type"] = "sendAction";
    message["connectionId"] = connectionId;
    message["action"] = action;
    m_messaging->sendMessage(message);
}

bool ClientManager::supportsSendAttentionSequence(const QString& connectionId) const
{
    auto it = m_connections.find(connectionId);
    if (it == m_connections.end())
        return false;
    return it.value().supportsSendAttentionSequence;
}

bool ClientManager::supportsLockWorkstation(const QString& connectionId) const
{
    auto it = m_connections.find(connectionId);
    if (it == m_connections.end())
        return false;
    return it.value().supportsLockWorkstation;
}

void ClientManager::startFileUpload(const QString& connectionId, const QUrl& fileUrl)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot start file upload: messaging not ready");
        return;
    }

    QString filePath = fileUrl.toLocalFile();
    LOG_INFO("Starting file upload for {}: {}", connectionId.toStdString(),
             filePath.toStdString());

    QJsonObject message;
    message["type"] = "startFileUpload";
    message["connectionId"] = connectionId;
    message["filePath"] = filePath;
    m_messaging->sendMessage(message);
}

void ClientManager::cancelFileUpload(const QString& connectionId,
                                     const QString& transferId)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot cancel file upload: messaging not ready");
        return;
    }

    LOG_INFO("Cancelling file upload: transfer={} connection={}",
             transferId.toStdString(), connectionId.toStdString());

    QJsonObject message;
    message["type"] = "cancelFileUpload";
    message["connectionId"] = connectionId;
    message["transferId"] = transferId;
    m_messaging->sendMessage(message);
}

void ClientManager::startFileDownload(const QString& connectionId)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot start file download: messaging not ready");
        return;
    }

    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    LOG_INFO("Starting file download for {}, saveDir={}",
             connectionId.toStdString(), saveDir.toStdString());

    QJsonObject message;
    message["type"] = "startFileDownload";
    message["connectionId"] = connectionId;
    message["saveDir"] = saveDir;
    m_messaging->sendMessage(message);
}

void ClientManager::cancelFileDownload(const QString& connectionId,
                                       const QString& transferId)
{
    if (!m_messaging || !m_messaging->isReady()) {
        LOG_WARN("Cannot cancel file download: messaging not ready");
        return;
    }

    LOG_INFO("Cancelling file download: transfer={} connection={}",
             transferId.toStdString(), connectionId.toStdString());

    QJsonObject message;
    message["type"] = "cancelFileDownload";
    message["connectionId"] = connectionId;
    message["transferId"] = transferId;
    m_messaging->sendMessage(message);
}

void ClientManager::openDownloadedFile(const QString& filePath)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void ClientManager::openContainingFolder(const QString& filePath)
{
    QFileInfo fi(filePath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
}

bool ClientManager::deleteDownloadedFile(const QString& filePath)
{
    return QFile::remove(filePath);
}

bool ClientManager::pasteFilesFromClipboard(const QString& connectionId)
{
    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData || !mimeData->hasUrls()) {
        return false;
    }

    QList<QUrl> urls = mimeData->urls();
    bool anyStarted = false;
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            QFileInfo fi(url.toLocalFile());
            if (fi.exists() && fi.isFile()) {
                startFileUpload(connectionId, url);
                anyStarted = true;
            }
        }
    }
    return anyStarted;
}

bool ClientManager::supportsFileTransfer(const QString& connectionId) const
{
    auto it = m_connections.find(connectionId);
    if (it == m_connections.end())
        return false;
    return it.value().supportsFileTransfer;
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

RtcStatus::Status ClientManager::getConnectionRtcState(const QString& connectionId) const
{
    if (m_connections.contains(connectionId)) {
        return m_connections[connectionId].rtcState;
    }
    return RtcStatus::Disconnected;
}

QString ClientManager::getSignalingState(const QString& connectionId) const
{
    if (m_connections.contains(connectionId)) {
        return m_connections[connectionId].signalingState;
    }
    return "disconnected";
}

int ClientManager::getSignalingRetryCount(const QString& connectionId) const
{
    if (m_connections.contains(connectionId)) {
        return m_connections[connectionId].signalingRetryCount;
    }
    return 0;
}

int ClientManager::getSignalingNextRetryIn(const QString& connectionId) const
{
    if (m_connections.contains(connectionId)) {
        return m_connections[connectionId].signalingNextRetryIn;
    }
    return 0;
}

QString ClientManager::getSignalingError(const QString& connectionId) const
{
    if (m_connections.contains(connectionId)) {
        return m_connections[connectionId].signalingError;
    }
    return QString();
}

QString ClientManager::getConnectionDeviceId(const QString& connectionId) const
{
    if (m_connections.contains(connectionId)) {
        return m_connections[connectionId].deviceId;
    }
    return QString();
}

void ClientManager::onMessageReceived(const QJsonObject& message)
{
    QString type = message["type"].toString();

    if (type != "videoFrameReady" && type != "performanceStatsUpdate" && type != "videoLayoutChanged" && type != "routeChanged") {
        LOG_DEBUG("Client received message: {}", type.toStdString());
    }

    if (type == "helloResponse") {
        handleHelloResponse(message);
    } else if (type == "signalingStateChanged") {
        handleSignalingStateChanged(message);
    } else if (type == "connectToHostResponse") {
        handleConnectToHostResponse(message);
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
    } else if (type == "disconnectFromHostResponse") {
        handleDisconnectFromHostResponse(message);
    } else if (type == "disconnectAllResponse") {
        handleDisconnectAllResponse(message);
    } else if (type == "cursorShapeChanged") {
        handleCursorShapeChanged(message);
    } else if (type == "performanceStatsUpdate") {
        handlePerformanceStatsUpdate(message);
    } else if (type == "videoLayoutChanged") {
        handleVideoLayoutChanged(message);
    } else if (type == "routeChanged") {
        handleRouteChanged(message);
    } else if (type == "hostCapabilities") {
        handleHostCapabilities(message);
    } else if (type == "fileTransferProgress") {
        handleFileTransferProgress(message);
    } else if (type == "fileTransferComplete") {
        handleFileTransferComplete(message);
    } else if (type == "fileTransferError") {
        handleFileTransferError(message);
    } else if (type == "fileDownloadStarted") {
        handleFileDownloadStarted(message);
    } else if (type == "fileDownloadProgress") {
        handleFileDownloadProgress(message);
    } else if (type == "fileDownloadComplete") {
        handleFileDownloadComplete(message);
    } else if (type == "fileDownloadError") {
        handleFileDownloadError(message);
    } else if (type == "agentBridgeResponse") {
        handleAgentBridgeResponse(message);
    } else if (type == "setFramerateResponse" || type == "setResolutionResponse" || type == "setFramerateBoostResponse" || type == "setBitrateResponse") {
        // Acknowledgement responses - just log success/failure
        bool success = message["success"].toBool();
        if (!success) {
            QString error = message["error"].toString();
            LOG_WARN("{} failed: {}", type.toStdString(), error.toStdString());
        }
    } else {
        LOG_WARN("Unknown message type from client: {}", type.toStdString());
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
    LOG_INFO("Client hello response, version: {}", version.toStdString());
    emit helloResponseReceived(version);
}

void ClientManager::handleSignalingStateChanged(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString state = message["state"].toString();
    int retryCount = message["retryCount"].toInt();
    int nextRetryIn = message["nextRetryIn"].toInt();
    QString error = message["error"].toString();

    LOG_INFO("Client signaling state changed: connection={}, state={}, retry={}, next={}s, error={}",
             connectionId.toStdString(), state.toStdString(), retryCount, nextRetryIn, error.toStdString());

    // Update signaling state for this specific connection
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].signalingState = state;
        m_connections[connectionId].signalingRetryCount = retryCount;
        m_connections[connectionId].signalingNextRetryIn = nextRetryIn;
        m_connections[connectionId].signalingError = error;
        
        // Emit signal with connection ID
        emit signalingStateChanged(connectionId, state, retryCount, nextRetryIn, error);
    } else {
        LOG_WARN("Signaling state changed for unknown connection: {}", connectionId.toStdString());
    }
}

void ClientManager::handleConnectToHostResponse(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();

    if (connectionId.isEmpty()) {
        LOG_WARN("connectToHostResponse missing connectionId");
        return;
    }

    if (!m_connections.contains(connectionId)) {
        ConnectionInfo conn;
        conn.connectionId = connectionId;
        conn.rtcState = RtcStatus::Connecting;
        m_connections[connectionId] = conn;

        emit connectionCountChanged();
        emit connectionAdded(connectionId);
        emit connectionListChanged();

        if (m_activeConnectionId.isEmpty()) {
            setActiveConnectionId(connectionId);
        }
    }
}

void ClientManager::handleConnectionStateChanged(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString stateStr = message["state"].toString();
    QJsonObject hostInfo = message["hostInfo"].toObject();

    if (m_connections.contains(connectionId)) {
        // Convert string to enum
        if (stateStr == "connecting") {
            m_connections[connectionId].rtcState = RtcStatus::Connecting;
        } else if (stateStr == "connected") {
            m_connections[connectionId].rtcState = RtcStatus::Connected;
        } else if (stateStr == "disconnected") {
            m_connections[connectionId].rtcState = RtcStatus::Disconnected;
        } else if (stateStr == "failed") {
            m_connections[connectionId].rtcState = RtcStatus::Failed;
        }
        
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

    LOG_INFO("Connection {} RTC state changed to: {}", connectionId.toStdString(), stateStr.toStdString());
    emit connectionStateChanged(connectionId, stateStr, hostInfo);
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
        conn.connectedAt = obj["connectedAt"].toString();
        
        // Convert string state to enum
        QString stateStr = obj["state"].toString();
        if (stateStr == "connecting") {
            conn.rtcState = RtcStatus::Connecting;
        } else if (stateStr == "connected") {
            conn.rtcState = RtcStatus::Connected;
        } else if (stateStr == "disconnected") {
            conn.rtcState = RtcStatus::Disconnected;
        } else if (stateStr == "failed") {
            conn.rtcState = RtcStatus::Failed;
        } else {
            conn.rtcState = RtcStatus::Disconnected;
        }
        
        m_connections[conn.connectionId] = conn;
    }

    emit connectionCountChanged();
    emit connectionListChanged();
}

void ClientManager::handleVideoFrameReady(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    int frameIndex = message["frameIndex"].toInt();
    int width = message["width"].toInt();
    int height = message["height"].toInt();
    QString sharedMemoryName = message["sharedMemoryName"].toString();
    
    // Attach to shared memory if not already attached
    if (!m_sharedMemoryManager->isAttached(connectionId)) {
        if (!m_sharedMemoryManager->attach(connectionId, sharedMemoryName)) {
            LOG_WARN("Failed to attach to shared memory for connection {}", 
                     connectionId.toStdString());
            return;
        }
        LOG_INFO("Attached to shared memory: {} ({}x{})", 
                 sharedMemoryName.toStdString(), width, height);
    }
    
    // Update connection info with resolution
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].width = width;
        m_connections[connectionId].height = height;
    }
    
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
    
    LOG_WARN("Client error: {} {} {}", connectionId.toStdString(), code.toStdString(), errorMsg.toStdString());
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
        m_connections[connectionId].rtcState = RtcStatus::Failed;
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
    
    LOG_INFO("Host connected: {}", connectionId.toStdString());
    
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].rtcState = RtcStatus::Connected;
        emit connectionStateChanged(connectionId, "connected", QJsonObject());
    }
    emit connectionListChanged();
}

void ClientManager::handleHostDisconnected(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    
    LOG_INFO("Host disconnected: {}", connectionId.toStdString());
    
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].rtcState = RtcStatus::Disconnected;
        emit connectionStateChanged(connectionId, "disconnected", QJsonObject());
    }
    
    // Detach from shared memory
    m_sharedMemoryManager->detach(connectionId);
    
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
    
    LOG_WARN("Host connection failed: {} error code: {}", connectionId.toStdString(), errorCode);
    
    // Update connection state
    if (m_connections.contains(connectionId)) {
        m_connections[connectionId].rtcState = RtcStatus::Failed;
        emit connectionStateChanged(connectionId, "failed", QJsonObject());
    }
    
    // Map protocol::ErrorCode to user-friendly message
    QString errorMsg;
    switch (errorCode) {
        case 1: errorMsg = tr("Authentication failed"); break;
        case 2: errorMsg = tr("Channel error"); break;
        case 3: errorMsg = tr("Connection timeout"); break;
        case 4: errorMsg = tr("Network error"); break;
        default: errorMsg = tr("Connection failed (error code: %1)").arg(errorCode); break;
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

void ClientManager::handleDisconnectFromHostResponse(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    if (!connectionId.isEmpty()) {
        LOG_INFO("Disconnect response received for connection: {}", connectionId.toStdString());
    }
}

void ClientManager::handleDisconnectAllResponse(const QJsonObject& message)
{
    Q_UNUSED(message);
    LOG_INFO("Disconnect all response received");
}

void ClientManager::handleCursorShapeChanged(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    int width = message["width"].toInt();
    int height = message["height"].toInt();
    int hotspotX = message["hotspotX"].toInt();
    int hotspotY = message["hotspotY"].toInt();
    QString base64Data = message["data"].toString();
    
    // Decode base64 data
    QByteArray data = QByteArray::fromBase64(base64Data.toLatin1());
    
    // LOG_DEBUG("Cursor shape changed for connection {}: {}x{} hotspot({}, {}) data size: {}",
    //           connectionId.toStdString(), width, height, hotspotX, hotspotY, data.size());
    
    emit cursorShapeChanged(connectionId, width, height, hotspotX, hotspotY, data);
}

void ClientManager::handlePerformanceStatsUpdate(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    
    QVariantMap stats;
    // Timing breakdown (ms)
    stats["captureMs"]      = message["captureMs"].toDouble();
    stats["encodeMs"]       = message["encodeMs"].toDouble();
    stats["networkDelayMs"] = message["networkDelayMs"].toDouble();
    stats["decodeMs"]       = message["decodeMs"].toDouble();
    stats["paintMs"]        = message["paintMs"].toDouble();
    stats["totalLatencyMs"] = message["totalLatencyMs"].toDouble();
    stats["inputRoundtripMs"] = message["inputRoundtripMs"].toDouble();
    // Throughput
    stats["bandwidthKbps"]  = message["bandwidthKbps"].toDouble();
    stats["frameRate"]      = message["frameRate"].toDouble();
    stats["packetRate"]     = message["packetRate"].toDouble();
    // Codec info
    stats["codec"]              = message["codec"].toString("Unknown");
    stats["frameQuality"]       = message["frameQuality"].toInt(-1);
    stats["encodedRectWidth"]   = message["encodedRectWidth"].toInt();
    stats["encodedRectHeight"]  = message["encodedRectHeight"].toInt();
    
    emit performanceStatsUpdated(connectionId, stats);
}

void ClientManager::handleVideoLayoutChanged(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    int widthDips = message["widthDips"].toInt();
    int heightDips = message["heightDips"].toInt();

    LOG_DEBUG("VideoLayout changed: connection={}, dips={}x{}",
              connectionId.toStdString(), widthDips, heightDips);

    emit videoLayoutChanged(connectionId, widthDips, heightDips);
}

void ClientManager::handleRouteChanged(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();

    QVariantMap routeInfo;
    routeInfo["routeType"] = message["routeType"].toString();
    routeInfo["transportProtocol"] = message["transportProtocol"].toString();
    routeInfo["localCandidateType"] = message["localCandidateType"].toString();
    routeInfo["remoteCandidateType"] = message["remoteCandidateType"].toString();
    routeInfo["localAddress"] = message["localAddress"].toString();
    routeInfo["remoteAddress"] = message["remoteAddress"].toString();

    QVariantList localCandidates;
    for (const auto& val : message["localCandidates"].toArray()) {
        QJsonObject obj = val.toObject();
        QVariantMap c;
        c["address"] = obj["address"].toString();
        c["type"] = obj["type"].toString();
        c["protocol"] = obj["protocol"].toString();
        c["isIpv6"] = obj["isIpv6"].toBool();
        c["priority"] = obj["priority"].toInt();
        localCandidates.append(c);
    }
    routeInfo["localCandidates"] = localCandidates;

    QVariantList remoteCandidates;
    for (const auto& val : message["remoteCandidates"].toArray()) {
        QJsonObject obj = val.toObject();
        QVariantMap c;
        c["address"] = obj["address"].toString();
        c["type"] = obj["type"].toString();
        c["protocol"] = obj["protocol"].toString();
        c["isIpv6"] = obj["isIpv6"].toBool();
        c["priority"] = obj["priority"].toInt();
        remoteCandidates.append(c);
    }
    routeInfo["remoteCandidates"] = remoteCandidates;

    LOG_INFO("Route changed: connection={}, type={}, local={}, remote={}",
             connectionId.toStdString(),
             routeInfo["routeType"].toString().toStdString(),
             routeInfo["localCandidateType"].toString().toStdString(),
             routeInfo["remoteCandidateType"].toString().toStdString());

    emit routeChanged(connectionId, routeInfo);
}

void ClientManager::sendMouseEvent(const QString& connectionId, const QString& eventType,
                                   int x, int y, int button,
                                   int wheelDeltaX, int wheelDeltaY)
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
    message["wheelDeltaX"] = wheelDeltaX;
    message["wheelDeltaY"] = wheelDeltaY;
    m_messaging->sendMessage(message);
}

void ClientManager::sendKeyboardEvent(const QString& connectionId, const QString& eventType,
                                      int nativeScanCode, int lockStates)
{
    if (!m_messaging || !m_messaging->isReady()) {
        return;
    }

    QJsonObject message;
    message["type"] = "keyboardEvent";
    message["connectionId"] = connectionId;
    message["eventType"] = eventType;
    message["nativeScanCode"] = nativeScanCode;
    message["lockStates"] = lockStates;
    m_messaging->sendMessage(message);
}

bool ClientManager::saveFrameToFile(const QString& connectionId, 
                                     const QString& filePath)
{
    if (!m_sharedMemoryManager->isAttached(connectionId)) {
        LOG_WARN("Cannot save frame: not attached to shared memory for {}", 
                 connectionId.toStdString());
        return false;
    }
    
    // Read YUV frame and convert to QImage for saving
    QVideoFrame videoFrame = m_sharedMemoryManager->readVideoFrame(connectionId);
    if (!videoFrame.isValid()) {
        LOG_WARN("Cannot save frame: failed to read video frame for {}", 
                 connectionId.toStdString());
        return false;
    }
    
    // Convert YUV frame to QImage
    QImage frame = videoFrame.toImage();
    if (frame.isNull()) {
        LOG_WARN("Cannot save frame: failed to convert video frame to image for {}", 
                 connectionId.toStdString());
        return false;
    }
    
    // Ensure directory exists
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Save to file
    bool success = frame.save(filePath);
    if (success) {
        LOG_INFO("Saved frame to: {} ({}x{})", 
                 filePath.toStdString(), frame.width(), frame.height());
    } else {
        LOG_WARN("Failed to save frame to: {}", filePath.toStdString());
    }
    
    return success;
}

void ClientManager::setIceConfig(const QJsonObject& iceConfig)
{
    m_iceConfig = iceConfig;
    QJsonArray servers = m_iceConfig.value("iceServers").toArray();
    LOG_INFO("Client: ICE config updated: {} server(s)", servers.size());
}

QJsonObject ClientManager::getIceConfig() const
{
    return m_iceConfig;
}

void ClientManager::handleHostCapabilities(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    bool supportsSAS = message["supportsSendAttentionSequence"].toBool();
    bool supportsLock = message["supportsLockWorkstation"].toBool();
    bool supportsFile = message["supportsFileTransfer"].toBool();

    auto it = m_connections.find(connectionId);
    if (it != m_connections.end()) {
        it->supportsSendAttentionSequence = supportsSAS;
        it->supportsLockWorkstation = supportsLock;
        it->supportsFileTransfer = supportsFile;
    }

    LOG_INFO("Host capabilities for {}: SAS={} Lock={} FileTransfer={}",
             connectionId.toStdString(), supportsSAS, supportsLock, supportsFile);

    emit hostCapabilitiesChanged(connectionId, supportsSAS, supportsLock, supportsFile);
}

void ClientManager::handleFileTransferProgress(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString transferId = message["transferId"].toString();
    QString filename = message["filename"].toString();
    double bytesSent = message["bytesSent"].toDouble();
    double totalBytes = message["totalBytes"].toDouble();

    emit fileTransferProgress(connectionId, transferId, filename,
                              bytesSent, totalBytes);
}

void ClientManager::handleFileTransferComplete(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString transferId = message["transferId"].toString();
    QString filename = message["filename"].toString();

    LOG_INFO("File transfer complete: {} (transfer={})",
             filename.toStdString(), transferId.toStdString());

    emit fileTransferComplete(connectionId, transferId, filename);
}

void ClientManager::handleFileTransferError(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString transferId = message["transferId"].toString();
    QString errorMessage = message["errorMessage"].toString();

    LOG_ERROR("File transfer error: {} (transfer={})",
              errorMessage.toStdString(), transferId.toStdString());

    emit fileTransferError(connectionId, transferId, errorMessage);
}

void ClientManager::handleFileDownloadStarted(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString transferId = message["transferId"].toString();
    QString filename = message["filename"].toString();
    double totalBytes = message["totalBytes"].toDouble();

    LOG_INFO("File download started: {} ({} bytes, transfer={})",
             filename.toStdString(), static_cast<uint64_t>(totalBytes),
             transferId.toStdString());

    emit fileDownloadStarted(connectionId, transferId, filename, totalBytes);
}

void ClientManager::handleFileDownloadProgress(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString transferId = message["transferId"].toString();
    QString filename = message["filename"].toString();
    double bytesReceived = message["bytesReceived"].toDouble();
    double totalBytes = message["totalBytes"].toDouble();

    emit fileDownloadProgress(connectionId, transferId, filename,
                              bytesReceived, totalBytes);
}

void ClientManager::handleFileDownloadComplete(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString transferId = message["transferId"].toString();
    QString filename = message["filename"].toString();
    QString savePath = message["savePath"].toString();

    LOG_INFO("File download complete: {} -> {} (transfer={})",
             filename.toStdString(), savePath.toStdString(),
             transferId.toStdString());

    emit fileDownloadComplete(connectionId, transferId, filename, savePath);
}

void ClientManager::handleFileDownloadError(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString transferId = message["transferId"].toString();
    QString errorMessage = message["errorMessage"].toString();

    LOG_ERROR("File download error: {} (transfer={})",
              errorMessage.toStdString(), transferId.toStdString());

    emit fileDownloadError(connectionId, transferId, errorMessage);
}

void ClientManager::handleAgentBridgeResponse(const QJsonObject& message)
{
    QString connectionId = message["connectionId"].toString();
    QString data = message["data"].toString();

    // Parse the JSON data string into an object for the AgentHandler
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject response = doc.isObject() ? doc.object() : QJsonObject{{"raw", data}};

    emit agentBridgeResponseReceived(connectionId, response);
}

} // namespace quickdesk
