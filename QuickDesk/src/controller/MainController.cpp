// Copyright 2026 QuickDesk Authors

#include "MainController.h"
#include "../manager/ProcessManager.h"
#include "../manager/NativeMessaging.h"
#include "../api/WebSocketServer.h"
#include "../api/OcrEngine.h"
#include "infra/env/applicationcontext.h"
#include "infra/log/log.h"
#include "core/localconfigcenter.h"
#include <QTimer>
#include <QClipboard>
#include <QGuiApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QPointer>
#include <QThreadPool>

namespace quickdesk {

MainController::MainController(QObject* parent)
    : QObject(parent)
    , m_processManager(std::make_unique<ProcessManager>(this))
    , m_serverManager(std::make_unique<ServerManager>(this))
    , m_turnServerManager(std::make_unique<TurnServerManager>(this))
    , m_hostManager(std::make_unique<HostManager>(this))
    , m_clientManager(std::make_unique<ClientManager>(this))
    , m_remoteDeviceManager(std::make_unique<RemoteDeviceManager>(this))
    , m_presetManager(std::make_unique<PresetManager>(m_serverManager.get(), this))
{
    // Connect ProcessManager signals
    connect(m_processManager.get(), &ProcessManager::hostProcessStarted,
            this, &MainController::onHostProcessStarted);
    connect(m_processManager.get(), &ProcessManager::hostProcessStopped,
            this, &MainController::onHostProcessStopped);
    connect(m_processManager.get(), &ProcessManager::hostProcessError,
            this, &MainController::onHostProcessError);
    connect(m_processManager.get(), &ProcessManager::hostProcessRestarting,
            this, &MainController::onHostProcessRestarting);
    connect(m_processManager.get(), &ProcessManager::hostProcessStatusChanged,
            this, &MainController::hostProcessStatusChanged);
    connect(m_processManager.get(), &ProcessManager::hostLaunchModeChanged,
            this, &MainController::hostLaunchModeChanged);
    
    connect(m_processManager.get(), &ProcessManager::clientProcessStarted,
            this, &MainController::onClientProcessStarted);
    connect(m_processManager.get(), &ProcessManager::clientProcessStopped,
            this, &MainController::onClientProcessStopped);
    connect(m_processManager.get(), &ProcessManager::clientProcessError,
            this, &MainController::onClientProcessError);
    connect(m_processManager.get(), &ProcessManager::clientProcessRestarting,
            this, &MainController::onClientProcessRestarting);
    connect(m_processManager.get(), &ProcessManager::clientProcessStatusChanged,
            this, &MainController::clientProcessStatusChanged);

    // Connect HostManager signals
    connect(m_hostManager.get(), &HostManager::hostReady,
            this, &MainController::onHostReady);
    connect(m_hostManager.get(), &HostManager::deviceIdChanged,
            this, &MainController::deviceIdChanged);
    connect(m_hostManager.get(), &HostManager::accessCodeChanged,
            this, &MainController::accessCodeChanged);
    connect(m_hostManager.get(), &HostManager::connectionStatusChanged,
            this, &MainController::hostConnectionChanged);
    connect(m_hostManager.get(), &HostManager::signalingStateChanged,
            this, &MainController::signalingStateChanged);
    
    // Listen to signaling state to update host server status
    connect(m_hostManager.get(), &HostManager::signalingStateChanged,
            this, [this]() {
        QString state = m_hostManager->signalingState();
        if (state == "connected") {
            m_hostServerStatus = ServerStatus::Connected;
        } else if (state == "connecting") {
            m_hostServerStatus = ServerStatus::Connecting;
        } else if (state == "disconnected") {
            m_hostServerStatus = ServerStatus::Disconnected;
        } else if (state == "failed") {
            m_hostServerStatus = ServerStatus::Failed;
        } else if (state == "reconnecting") {
            m_hostServerStatus = ServerStatus::Reconnecting;
        }
        emit hostServerStatusChanged();
    });
    
    // Listen to Client signaling state to update client server status (with connectionId)
    connect(m_clientManager.get(), &ClientManager::signalingStateChanged,
            this, &MainController::onClientSignalingStateChanged);
    
    // Listen to Client connection removed to update primary connection
    connect(m_clientManager.get(), &ClientManager::connectionRemoved,
            this, [this](const QString& connectionId) {
        // If the removed connection was primary, reset and pick new primary
        if (connectionId == m_primaryConnectionId) {
            m_primaryConnectionId.clear();
            
            // If there are other connections, pick the first one as new primary
            QStringList connIds = m_clientManager->connectionIds();
            if (!connIds.isEmpty()) {
                QString newPrimary = connIds.first();
                m_primaryConnectionId = newPrimary;
                LOG_INFO("Primary connection removed, new primary: {}", newPrimary.toStdString());
                
                // Update status with new primary's signaling state
                QString state = m_clientManager->getSignalingState(newPrimary);
                if (state == "connected") {
                    m_clientServerStatus = ServerStatus::Connected;
                } else if (state == "connecting") {
                    m_clientServerStatus = ServerStatus::Connecting;
                } else if (state == "disconnected") {
                    m_clientServerStatus = ServerStatus::Disconnected;
                } else if (state == "failed") {
                    m_clientServerStatus = ServerStatus::Failed;
                } else if (state == "reconnecting") {
                    m_clientServerStatus = ServerStatus::Reconnecting;
                }
                emit clientServerStatusChanged();
            } else {
                // No connections left, set to disconnected
                LOG_INFO("Primary connection removed, no more connections");
                m_clientServerStatus = ServerStatus::Disconnected;
                emit clientServerStatusChanged();
            }
        }
    });
    
    // Listen to access code changes to save when in "never refresh" mode
    connect(m_hostManager.get(), &HostManager::accessCodeChanged,
            this, [this]() {
        QString currentCode = m_hostManager->accessCode();
        LOG_INFO("Host access code changed: {}", currentCode.toStdString());
        if (currentCode.isEmpty()) {
            return;
        }
        core::LocalConfigCenter::instance().setSavedAccessCode(currentCode);
        LOG_INFO("Saved access code for 'never refresh' mode: {}", currentCode.toStdString()); 
    });
    
    // Forward PresetManager signals
    connect(m_presetManager.get(), &PresetManager::presetLoadFailed,
            this, &MainController::presetLoadFailed);
    connect(m_presetManager.get(), &PresetManager::forceUpgradeRequired,
            this, &MainController::forceUpgradeRequired);

    // WebSocket API Server
    m_wsApiServer = std::make_unique<WebSocketApiServer>(this, this);
    connect(m_wsApiServer.get(), &WebSocketApiServer::listeningChanged,
            this, &MainController::mcpServiceRunningChanged);
    connect(m_wsApiServer.get(), &WebSocketApiServer::authenticatedClientCountChanged,
            this, &MainController::mcpConnectedClientsChanged);
    setupWebSocketApiEvents();

    // Setup access code auto-refresh timer
    connect(&m_accessCodeRefreshTimer, &QTimer::timeout,
            this, &MainController::onAccessCodeRefreshTimer);
    
    // Listen to configuration changes
    connect(&core::LocalConfigCenter::instance(), 
            &core::LocalConfigCenter::signalAccessCodeRefreshIntervalChanged,
            this, [this](int interval) {
        LOG_INFO("Access code refresh interval changed to: {} minutes", interval);
        m_accessCodeRefreshIntervalMinutes = interval;
        updateAccessCodeRefreshTimer();
    });
}

MainController::~MainController()
{
    shutdown();
}

void MainController::initialize()
{
    LOG_INFO("MainController::initialize()");
    
    // Initialize RemoteDeviceManager
    if (!m_remoteDeviceManager->init()) {
        LOG_ERROR("Failed to initialize RemoteDeviceManager");
    } else {
        LOG_INFO("RemoteDeviceManager initialized successfully");
    }
    
    // Auto-detect executable paths
    if (!m_processManager->autoDetectPaths()) {
        LOG_WARN("Could not auto-detect all executable paths");
    }

    // Set log and config directories from ApplicationContext
    QString logDir = infra::ApplicationContext::instance().logPath();
    m_processManager->setLogDir(logDir);
    QString configDir = infra::ApplicationContext::instance().localDataPath();
    m_processManager->setConfigDir(configDir);

    // Start preset manager (polls server for preset config)
    m_presetManager->start();

    // Start Host process (status will be managed by ProcessManager)
    if (!m_processManager->startHostProcess()) {
        emit initializationFailed("Failed to start Host process");
    }

    // Start Client process (status will be managed by ProcessManager)
    if (!m_processManager->startClientProcess()) {
        emit initializationFailed("Failed to start Client process");
    }

    // Start WebSocket API server
    if (!m_wsApiServer->start()) {
        LOG_WARN("WebSocket API server failed to start, MCP bridge will not work");
    }
}

void MainController::shutdown()
{
    if (m_isShutdown)
        return;
    m_isShutdown = true;

    LOG_INFO("MainController::shutdown()");

    // Stop MCP HTTP process if running
    stopMcpHttpProcess();

    if (m_wsApiServer) {
        m_wsApiServer->stop();
    }
    
    m_presetManager->stop();
    m_hostManager->disconnectFromServer();
    m_clientManager->disconnectAll();

    // Clear messaging references before stopping processes.
    // stopAllProcesses() → cleanupServiceConnection() destroys NativeMessaging,
    // but HostManager/ClientManager won't be notified (signals are disconnected
    // to prevent re-entrancy). Clearing here prevents dangling pointers.
    m_hostManager->setMessaging(nullptr);
    m_clientManager->setMessaging(nullptr);
    
    m_processManager->stopAllProcesses();
}

QString MainController::connectToRemoteHost(const QString& deviceId,
                                            const QString& accessCode,
                                            const QString& serverUrl)
{
    QString url = serverUrl.isEmpty() ? getDefaultServerUrl() : serverUrl;
    LOG_INFO("Connecting to remote host: {} on {}", deviceId.toStdString(), url.toStdString());
    return m_clientManager->connectToHost(deviceId, accessCode, url);
}

void MainController::disconnectFromRemoteHost(const QString& connectionId)
{
    m_clientManager->disconnectFromHost(connectionId);
}

void MainController::showRemoteWindowForConnection(const QString& connectionId, const QString& deviceId)
{
    emit requestShowRemoteWindow(connectionId, deviceId);
}

void MainController::refreshAccessCode()
{
    m_hostManager->refreshAccessCode();
    
    // Reset auto-refresh timer when user manually refreshes
    resetAccessCodeRefreshTimer();
}

void MainController::resetAccessCodeRefreshTimer()
{
    // Only reset if auto-refresh is enabled
    if (m_accessCodeRefreshIntervalMinutes <= 0) {
        return;
    }
    
    // Restart the timer (will reset the countdown)
    updateAccessCodeRefreshTimer();
    
    LOG_INFO("Access code refresh timer reset after manual refresh, next at {}", 
             m_nextRefreshTime.toString("MM-dd HH:mm:ss").toStdString());
}

void MainController::copyToClipboard(const QString& text)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
        LOG_INFO("Copied to clipboard: {}", text.toStdString());
    }
}

void MainController::copyDeviceInfo()
{
    QString deviceId = m_hostManager->deviceId();
    QString accessCode = m_hostManager->accessCode();
    
    if (deviceId.isEmpty() && accessCode.isEmpty()) {
        LOG_WARN("No device info to copy");
        return;
    }
    
    QString info = tr("Device ID: %1\nAccess Code: %2").arg(deviceId, accessCode);
    copyToClipboard(info);
}

ServerManager* MainController::serverManager() const
{
    return m_serverManager.get();
}

HostManager* MainController::hostManager() const
{
    return m_hostManager.get();
}

ClientManager* MainController::clientManager() const
{
    return m_clientManager.get();
}

TurnServerManager* MainController::turnServerManager() const
{
    return m_turnServerManager.get();
}

RemoteDeviceManager* MainController::remoteDeviceManager() const
{
    return m_remoteDeviceManager.get();
}

PresetManager* MainController::presetManager() const
{
    return m_presetManager.get();
}

QString MainController::deviceId() const
{
    return m_hostManager->deviceId();
}

QString MainController::accessCode() const
{
    return m_hostManager->accessCode();
}

bool MainController::isHostConnected() const
{
    return m_hostManager->isConnected();
}

QString MainController::signalingState() const
{
    return m_hostManager->signalingState();
}

int MainController::signalingRetryCount() const
{
    return m_hostManager->signalingRetryCount();
}

int MainController::signalingNextRetryIn() const
{
    return m_hostManager->signalingNextRetryIn();
}

QString MainController::signalingError() const
{
    return m_hostManager->signalingError();
}

QString MainController::signalingStatusText() const
{
    QString state = m_hostManager->signalingState();
    int retryCount = m_hostManager->signalingRetryCount();
    int nextRetry = m_hostManager->signalingNextRetryIn();
    QString error = m_hostManager->signalingError();
    
    if (state == "connected") {
        return tr("Connected");
    } else if (state == "connecting") {
        return tr("Connecting...");
    } else if (state == "disconnected") {
        return tr("Disconnected");
    } else if (state == "failed") {
        QString msg = tr("Connection failed");
        if (!error.isEmpty()) {
            msg += QString(": %1").arg(error);
        }
        return msg;
    } else if (state == "reconnecting") {
        QString msg = tr("Reconnecting (attempt %1)").arg(retryCount);
        if (nextRetry > 0) {
            msg += tr(", retry in %1s").arg(nextRetry);
        }
        return msg;
    }
    return state;
}

ProcessStatus::Status MainController::hostProcessStatus() const
{
    return m_processManager->hostProcessStatus();
}

ServerStatus::Status MainController::hostServerStatus() const
{
    return m_hostServerStatus;
}

ProcessStatus::Status MainController::clientProcessStatus() const
{
    return m_processManager->clientProcessStatus();
}

ServerStatus::Status MainController::clientServerStatus() const
{
    return m_clientServerStatus;
}

HostLaunchMode::Mode MainController::hostLaunchMode() const
{
    return m_processManager->hostLaunchMode();
}

QString MainController::nextAccessCodeRefreshTime() const
{
    if (m_accessCodeRefreshIntervalMinutes <= 0) {
        return tr("Never");
    }
    
    if (!m_nextRefreshTime.isValid()) {
        return tr("Never");
    }
    
    // Format: "01-29 09:11"
    return m_nextRefreshTime.toString("MM-dd HH:mm");
}

void MainController::onHostProcessStarted()
{
    LOG_INFO("Host process started");
    
    // Reset retry count on successful start
    m_processManager->resetHostRetryCount();
    
    // Set up Native Messaging
    m_hostManager->setMessaging(m_processManager->hostMessaging());
    
    // Set ICE config (may be STUN-only if server fetch hasn't completed yet)
    QJsonObject iceConfig = m_turnServerManager->getEffectiveIceConfig();
    m_hostManager->setIceConfig(iceConfig);
    
    // Send hello to verify communication and connect to signaling server
    QTimer::singleShot(500, this, [this]() {
        m_hostManager->sendHello();
        
        // Auto-connect to signaling server
        QTimer::singleShot(500, this, [this]() {
            // Update server status to Connecting
            m_hostServerStatus = ServerStatus::Connecting;
            emit hostServerStatusChanged();
            
            QString savedAccessCode;
            int interval = core::LocalConfigCenter::instance().accessCodeRefreshInterval();
            if (interval == -1) {
                // "Never refresh" mode: always use saved access code
                savedAccessCode = core::LocalConfigCenter::instance().savedAccessCode();
                if (!savedAccessCode.isEmpty()) {
                    LOG_INFO("Using saved access code for 'never refresh' mode: {}", savedAccessCode.toStdString());
                }
            } else if (interval > 0) {
                // Timed refresh mode: use saved access code if next refresh time hasn't expired
                QString nextRefreshTimeStr = core::LocalConfigCenter::instance().accessCodeNextRefreshTime();
                QDateTime nextRefreshTime = QDateTime::fromString(nextRefreshTimeStr, Qt::ISODate);
                if (nextRefreshTime.isValid() && nextRefreshTime > QDateTime::currentDateTime()) {
                    savedAccessCode = core::LocalConfigCenter::instance().savedAccessCode();
                    if (!savedAccessCode.isEmpty()) {
                        int remainingSecs = QDateTime::currentDateTime().secsTo(nextRefreshTime);
                        LOG_INFO("Resuming saved access code, remaining {}s until next refresh", remainingSecs);
                    }
                } else {
                    LOG_INFO("Saved refresh time expired or not set, will generate new access code");
                }
            }

            QString serverUrl = getDefaultServerUrl();
            LOG_INFO("Auto-connecting to signaling server: {}", serverUrl.toStdString());
            m_hostManager->connectToServer(serverUrl, savedAccessCode);
        });
    });
}

void MainController::onHostProcessStopped(int exitCode)
{
    LOG_INFO("Host process stopped with exit code: {}", exitCode);
    
    // Update server status
    m_hostServerStatus = ServerStatus::Disconnected;
    emit hostServerStatusChanged();
    
    m_hostManager->setMessaging(nullptr);
    // Clear UI state (will be restored after restart)
    m_deviceId.clear();
    m_accessCode.clear();
    emit deviceIdChanged();
    emit accessCodeChanged();
}

void MainController::onHostProcessError(const QString& error)
{
    LOG_WARN("Host process error: {}", error.toStdString());
    emit initializationFailed(QString("Host error: %1").arg(error));
}

void MainController::onHostProcessRestarting(int retryCount, int maxRetries)
{
    LOG_INFO("Host process restarting, attempt {} of {}", retryCount, maxRetries);
}

void MainController::onClientProcessStarted()
{
    LOG_INFO("Client process started");
    
    // Reset retry count on successful start
    m_processManager->resetClientRetryCount();
    
    // Set up Native Messaging
    m_clientManager->setMessaging(m_processManager->clientMessaging());
    
    // Set ICE config on client
    QJsonObject iceConfig = m_turnServerManager->getEffectiveIceConfig();
    m_clientManager->setIceConfig(iceConfig);
    
    // Send hello to verify communication and pass local device_id + preferred video codec
    // Wait for Host's device_id to be ready before sending
    QTimer::singleShot(500, this, [this]() {
        QString videoCodec = core::LocalConfigCenter::instance().preferredVideoCodec();
        QString localDeviceId = m_hostManager->deviceId();
        if (localDeviceId.isEmpty()) {
            LOG_WARN("Client hello: Host device_id not ready yet, waiting for deviceIdChanged signal");
            QMetaObject::Connection* conn = new QMetaObject::Connection();
            *conn = connect(m_hostManager.get(), &HostManager::deviceIdChanged, this, [this, conn, videoCodec]() {
                QString deviceId = m_hostManager->deviceId();
                if (!deviceId.isEmpty()) {
                    LOG_INFO("Client hello: Received device_id from Host: {}", deviceId.toStdString());
                    m_clientManager->sendHello(deviceId, videoCodec);
                    disconnect(*conn);
                    delete conn;
                }
            });
        } else {
            LOG_INFO("Client hello: Using device_id: {}, videoCodec: {}", 
                     localDeviceId.toStdString(), videoCodec.toStdString());
            m_clientManager->sendHello(localDeviceId, videoCodec);
        }
    });
}

void MainController::onClientProcessStopped(int exitCode)
{
    LOG_INFO("Client process stopped with exit code: {}", exitCode);
    
    // Update server status
    m_clientServerStatus = ServerStatus::Disconnected;
    emit clientServerStatusChanged();
    
    m_clientManager->setMessaging(nullptr);
}

void MainController::onClientProcessError(const QString& error)
{
    LOG_WARN("Client process error: {}", error.toStdString());
    emit initializationFailed(QString("Client error: %1").arg(error));
}

void MainController::onClientProcessRestarting(int retryCount, int maxRetries)
{
    LOG_INFO("Client process restarting, attempt {} of {}", retryCount, maxRetries);
}

void MainController::onClientSignalingStateChanged(const QString& connectionId,
                                                    const QString& state,
                                                    int retryCount,
                                                    int nextRetryIn,
                                                    const QString& error)
{
    Q_UNUSED(retryCount);
    Q_UNUSED(nextRetryIn);
    Q_UNUSED(error);
    
    LOG_INFO("Client signaling state changed: connection={}, state={}",
             connectionId.toStdString(), state.toStdString());
    
    // If this is the first connection, set it as primary
    if (m_primaryConnectionId.isEmpty() && !connectionId.isEmpty()) {
        m_primaryConnectionId = connectionId;
        LOG_INFO("Set primary connection for client signaling status: {}", connectionId.toStdString());
    }
    
    // Only update global client server status if it's the primary connection
    if (connectionId == m_primaryConnectionId) {
        if (state == "connected") {
            m_clientServerStatus = ServerStatus::Connected;
        } else if (state == "connecting") {
            m_clientServerStatus = ServerStatus::Connecting;
        } else if (state == "disconnected") {
            m_clientServerStatus = ServerStatus::Disconnected;
        } else if (state == "failed") {
            m_clientServerStatus = ServerStatus::Failed;
        } else if (state == "reconnecting") {
            m_clientServerStatus = ServerStatus::Reconnecting;
        }
        emit clientServerStatusChanged();
    }
}

void MainController::onHostReady(const QString& deviceId, const QString& accessCode)
{
    LOG_INFO("Host ready - Device ID: {} Access Code: {}", deviceId.toStdString(), accessCode.toStdString());
    m_deviceId = deviceId;
    m_accessCode = accessCode;
    
    // Load access code refresh interval from config
    m_accessCodeRefreshIntervalMinutes = core::LocalConfigCenter::instance().accessCodeRefreshInterval();
    LOG_INFO("Access code refresh interval: {} minutes", m_accessCodeRefreshIntervalMinutes);
    
    if (m_accessCodeRefreshIntervalMinutes == -1) {
        core::LocalConfigCenter::instance().setSavedAccessCode(accessCode);
        LOG_INFO("Saved access code for 'never refresh' mode: {}", accessCode.toStdString());
    } else if (m_accessCodeRefreshIntervalMinutes > 0) {
        // Check if we have a saved next refresh time to resume from
        QString nextRefreshTimeStr = core::LocalConfigCenter::instance().accessCodeNextRefreshTime();
        QDateTime savedNextRefresh = QDateTime::fromString(nextRefreshTimeStr, Qt::ISODate);
        
        if (savedNextRefresh.isValid() && savedNextRefresh > QDateTime::currentDateTime()) {
            int remainingSeconds = QDateTime::currentDateTime().secsTo(savedNextRefresh);
            LOG_INFO("Resuming access code refresh timer with {} seconds remaining", remainingSeconds);
            updateAccessCodeRefreshTimer(remainingSeconds);
        } else {
            LOG_INFO("Starting access code auto-refresh timer: {} minutes", m_accessCodeRefreshIntervalMinutes);
            updateAccessCodeRefreshTimer();
        }
    }

    QTimer::singleShot(0, this, [this]() {
        emit deviceIdChanged();
        emit accessCodeChanged();
    });
}

QString MainController::getDefaultServerUrl() const
{
    return m_serverManager->serverUrl();
}

void MainController::onAccessCodeRefreshTimer()
{
    LOG_INFO("Access code auto-refresh timer triggered");
    
    // Check if host is still connected
    if (!m_hostManager->isConnected()) {
        LOG_WARN("Host not connected, skipping auto-refresh");
        return;
    }
    
    // Call refresh access code
    m_hostManager->refreshAccessCode();
    
    // Restart timer with full interval (important when resuming from remaining time)
    updateAccessCodeRefreshTimer();
}

void MainController::updateAccessCodeRefreshTimer(int remainingSeconds)
{
    // Stop existing timer
    m_accessCodeRefreshTimer.stop();
    m_nextRefreshTime = QDateTime();
    emit nextAccessCodeRefreshTimeChanged();
    
    // -1 means never refresh
    if (m_accessCodeRefreshIntervalMinutes <= 0) {
        LOG_INFO("Access code auto-refresh disabled (interval: {})", m_accessCodeRefreshIntervalMinutes);
        core::LocalConfigCenter::instance().setAccessCodeNextRefreshTime("");
        return;
    }
    
    int timerMs;
    if (remainingSeconds > 0) {
        // Resume with remaining time
        timerMs = remainingSeconds * 1000;
        m_nextRefreshTime = QDateTime::currentDateTime().addSecs(remainingSeconds);
    } else {
        // Start with full interval
        timerMs = m_accessCodeRefreshIntervalMinutes * 60 * 1000;
        m_nextRefreshTime = QDateTime::currentDateTime().addSecs(m_accessCodeRefreshIntervalMinutes * 60);
    }
    
    m_accessCodeRefreshTimer.start(timerMs);
    emit nextAccessCodeRefreshTimeChanged();
    
    // Persist next refresh time so it survives restarts
    core::LocalConfigCenter::instance().setAccessCodeNextRefreshTime(
        m_nextRefreshTime.toString(Qt::ISODate));
    
    LOG_INFO("Access code auto-refresh timer started: {} ms, next at {}", 
             timerMs, m_nextRefreshTime.toString("MM-dd HH:mm:ss").toStdString());
}

void MainController::setupWebSocketApiEvents() {
    // Host events → WebSocket broadcast
    connect(m_hostManager.get(), &HostManager::hostReady,
            this, [this](const QString& deviceId, const QString& accessCode) {
        m_wsApiServer->broadcastEvent("hostReady", {
            {"deviceId", deviceId}, {"accessCode", accessCode}
        });
    });
    connect(m_hostManager.get(), &HostManager::accessCodeChanged,
            this, [this]() {
        m_wsApiServer->broadcastEvent("accessCodeChanged", {
            {"accessCode", m_hostManager->accessCode()}
        });
    });
    connect(m_hostManager.get(), &HostManager::clientConnected,
            this, [this](const QString& connectionId, const QJsonObject& info) {
        QJsonObject data = info;
        data["connectionId"] = connectionId;
        m_wsApiServer->broadcastEvent("hostClientConnected", data);
    });
    connect(m_hostManager.get(), &HostManager::clientDisconnected,
            this, [this](const QString& connectionId, const QString& reason) {
        m_wsApiServer->broadcastEvent("hostClientDisconnected", {
            {"connectionId", connectionId}, {"reason", reason}
        });
    });
    connect(m_hostManager.get(), &HostManager::signalingStateChanged,
            this, [this]() {
        m_wsApiServer->broadcastEvent("hostSignalingStateChanged", {
            {"state", m_hostManager->signalingState()},
            {"retryCount", m_hostManager->signalingRetryCount()},
            {"nextRetryIn", m_hostManager->signalingNextRetryIn()},
            {"error", m_hostManager->signalingError()}
        });
    });

    // Client events → WebSocket broadcast
    connect(m_clientManager.get(), &ClientManager::connectionAdded,
            this, [this](const QString& connectionId) {
        auto info = m_clientManager->getConnection(connectionId);
        m_wsApiServer->broadcastEvent("connectionAdded", {
            {"connectionId", connectionId},
            {"deviceId", info.deviceId}
        });
    });
    connect(m_clientManager.get(), &ClientManager::connectionRemoved,
            this, [this](const QString& connectionId) {
        m_wsApiServer->broadcastEvent("connectionRemoved", {
            {"connectionId", connectionId}
        });
    });
    connect(m_clientManager.get(), &ClientManager::connectionStateChanged,
            this, [this](const QString& connectionId, const QString& state,
                         const QJsonObject& hostInfo) {
        QJsonObject data;
        data["connectionId"] = connectionId;
        data["state"] = state;
        data["hostInfo"] = hostInfo;
        m_wsApiServer->broadcastEvent("connectionStateChanged", data);
    });
    connect(m_clientManager.get(), &ClientManager::clipboardReceived,
            this, [this](const QString& connectionId, const QString& text) {
        m_wsApiServer->broadcastEvent("clipboardChanged", {
            {"connectionId", connectionId}, {"text", text}
        });
    });
    connect(m_clientManager.get(), &ClientManager::videoLayoutChanged,
            this, [this](const QString& connectionId, int w, int h) {
        m_wsApiServer->broadcastEvent("videoLayoutChanged", {
            {"connectionId", connectionId},
            {"width", w}, {"height", h}
        });
    });

    // screenChanged: throttle videoFrameReady → hash-based event.
    // Strategy: rate-limit check + QImage read on UI thread (fast),
    // then MD5 hash computation offloaded to Qt global thread pool.
    connect(m_clientManager.get(), &ClientManager::videoFrameReady,
            this, [this](const QString& connectionId, int /*frameIndex*/) {
        // ① Rate-limit: skip if last broadcast for this connection was < 200ms ago
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        {
            QMutexLocker locker(&m_screenChangeMutex);
            if (now - m_screenChangeState[connectionId].lastBroadcastMs < 200)
                return;
        }

        // ② Read the current frame on UI thread (shared-memory copy, ~1ms)
        auto* shm = m_clientManager->sharedMemoryManager();
        if (!shm || !shm->isAttached(connectionId)) return;
        QImage img = shm->readVideoFrame(connectionId).toImage();
        if (img.isNull()) return;

        // ③ Offload MD5 computation to thread pool; capture QImage by move
        QPointer<MainController> self(this);
        QThreadPool::globalInstance()->start([self, connectionId, img = std::move(img), now]() {
            if (!self) return;

            const QString hash = OcrEngine::computeFrameHash(img);

            // ④ Back on UI thread: compare hash, broadcast if changed
            QMetaObject::invokeMethod(self, [self, connectionId, hash, now]() {
                if (!self) return;
                QMutexLocker locker(&self->m_screenChangeMutex);
                auto& state = self->m_screenChangeState[connectionId];

                if (hash == state.lastBroadcastHash) return;   // no visual change
                if (now - state.lastBroadcastMs < 200) return;  // lost race

                state.lastBroadcastHash = hash;
                state.lastBroadcastMs   = now;

                self->m_wsApiServer->broadcastEvent("screenChanged", {
                    {"connectionId", connectionId},
                    {"frameHash",    hash},
                    {"timestamp",    now}
                });
            }, Qt::QueuedConnection);
        });
    }, Qt::QueuedConnection);

    // Clean up per-connection throttle state when a connection is removed
    connect(m_clientManager.get(), &ClientManager::connectionRemoved,
            this, [this](const QString& connectionId) {
        QMutexLocker locker(&m_screenChangeMutex);
        m_screenChangeState.remove(connectionId);
    });

    // Process events → WebSocket broadcast
    connect(m_processManager.get(), &ProcessManager::hostProcessStatusChanged,
            this, [this]() {
        QString status;
        switch (m_processManager->hostProcessStatus()) {
        case ProcessStatus::NotStarted: status = "notStarted"; break;
        case ProcessStatus::Starting:   status = "starting"; break;
        case ProcessStatus::Running:    status = "running"; break;
        case ProcessStatus::Failed:     status = "failed"; break;
        case ProcessStatus::Restarting: status = "restarting"; break;
        }
        m_wsApiServer->broadcastEvent("hostProcessStatusChanged", {
            {"status", status}
        });
    });
    connect(m_processManager.get(), &ProcessManager::clientProcessStatusChanged,
            this, [this]() {
        QString status;
        switch (m_processManager->clientProcessStatus()) {
        case ProcessStatus::NotStarted: status = "notStarted"; break;
        case ProcessStatus::Starting:   status = "starting"; break;
        case ProcessStatus::Running:    status = "running"; break;
        case ProcessStatus::Failed:     status = "failed"; break;
        case ProcessStatus::Restarting: status = "restarting"; break;
        }
        m_wsApiServer->broadcastEvent("clientProcessStatusChanged", {
            {"status", status}
        });
    });
}

// MCP Service
bool MainController::mcpServiceRunning() const {
    // WS API server must be running for either mode
    if (!m_wsApiServer || !m_wsApiServer->isListening())
        return false;
    // HTTP mode additionally requires the HTTP process
    if (m_mcpTransportMode == "http") {
        return m_mcpHttpProcess &&
               m_mcpHttpProcess->state() == QProcess::Running;
    }
    return true;
}

int MainController::mcpConnectedClients() const {
    return m_wsApiServer ? m_wsApiServer->authenticatedClientCount() : 0;
}

int MainController::mcpPort() const {
    if (m_mcpTransportMode == "http") {
        return m_mcpHttpPort;
    }
    return m_wsApiServer ? m_wsApiServer->port() : 0;
}

QString MainController::mcpTransportMode() const {
    return m_mcpTransportMode;
}

void MainController::setMcpTransportMode(const QString& mode) {
    if (m_mcpTransportMode == mode) return;
    // Only stop the HTTP process when switching away from HTTP mode.
    // m_wsApiServer stays running — both modes depend on it.
    if (m_mcpTransportMode == "http") {
        stopMcpHttpProcess();
    }
    m_mcpTransportMode = mode;
    // Auto-start the HTTP process when switching to HTTP mode.
    if (mode == "http") {
        startMcpHttpProcess();
    }
    emit mcpTransportModeChanged();
    emit mcpServiceRunningChanged();
}

int MainController::mcpHttpPort() const {
    return m_mcpHttpPort;
}

void MainController::setMcpHttpPort(int port) {
    if (m_mcpHttpPort == port) return;
    m_mcpHttpPort = port;
    emit mcpHttpPortChanged();
}

QString MainController::mcpHttpUrl() const {
    if (m_mcpTransportMode == "http" && mcpServiceRunning()) {
        return QStringLiteral("http://127.0.0.1:%1/mcp").arg(m_mcpHttpPort);
    }
    return QString();
}

void MainController::startMcpService() {
    // WS API server is started at init and stays running.
    // This method only needs to start the HTTP process in HTTP mode.
    if (m_mcpTransportMode == "http") {
        startMcpHttpProcess();
    }
    emit mcpServiceRunningChanged();
}

void MainController::stopMcpService() {
    // Only stop the HTTP process. WS API server stays running.
    if (m_mcpTransportMode == "http") {
        stopMcpHttpProcess();
    }
    emit mcpServiceRunningChanged();
}

void MainController::startMcpHttpProcess() {
    if (m_mcpHttpProcess &&
        m_mcpHttpProcess->state() == QProcess::Running) {
        return;  // Already running
    }

    auto mcpPath = getMcpBinaryPath();
    if (!QFileInfo::exists(mcpPath)) {
        LOG_ERROR("quickdesk-mcp binary not found: {}",
                  mcpPath.toStdString());
        return;
    }

    if (!m_mcpHttpProcess) {
        m_mcpHttpProcess = new QProcess(this);
        connect(m_mcpHttpProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus status) {
            LOG_INFO("quickdesk-mcp HTTP process exited: code={}, status={}",
                     exitCode, static_cast<int>(status));
            emit mcpServiceRunningChanged();
        });
        connect(m_mcpHttpProcess, &QProcess::errorOccurred,
                this, [this](QProcess::ProcessError error) {
            LOG_ERROR("quickdesk-mcp HTTP process error: {}",
                      static_cast<int>(error));
            emit mcpServiceRunningChanged();
        });
    }

    auto wsPort = m_wsApiServer ? m_wsApiServer->port() : 9600;
    QStringList args;
    args << "--transport" << "http"
         << "--port" << QString::number(m_mcpHttpPort)
         << "--ws-url" << QStringLiteral("ws://127.0.0.1:%1").arg(wsPort);

    LOG_INFO("Starting quickdesk-mcp HTTP: {} {}",
             mcpPath.toStdString(),
             args.join(" ").toStdString());

    m_mcpHttpProcess->start(mcpPath, args);
}

void MainController::stopMcpHttpProcess() {
    if (!m_mcpHttpProcess) return;
    if (m_mcpHttpProcess->state() == QProcess::NotRunning) return;

    LOG_INFO("Stopping quickdesk-mcp HTTP process");
    // Close stdin pipe first — the MCP process watches for stdin EOF
    // as its primary graceful-shutdown signal (on Windows, terminate()
    // sends WM_CLOSE which console processes cannot receive).
    m_mcpHttpProcess->closeWriteChannel();
    if (!m_mcpHttpProcess->waitForFinished(2000)) {
        m_mcpHttpProcess->terminate();
        if (!m_mcpHttpProcess->waitForFinished(1000)) {
            m_mcpHttpProcess->kill();
            m_mcpHttpProcess->waitForFinished(1000);
        }
    }
}

QString MainController::getMcpBinaryPath() const {
    auto appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    auto mcpPath = appDir + "/quickdesk-mcp.exe";
#elif defined(Q_OS_MAC)
    // appDir = Contents/MacOS, mcp binary is in Contents/Frameworks
    auto mcpPath = appDir + "/../Frameworks/quickdesk-mcp";
#else
    auto mcpPath = appDir + "/quickdesk-mcp";
#endif
    return QDir::toNativeSeparators(QDir::cleanPath(mcpPath));
}

QJsonObject MainController::buildMcpServerConfig(const QString& transport) const {
    QJsonObject serverConfig;
    if (transport == "http") {
        // HTTP/SSE mode: URL-based config
        serverConfig["url"] =
            QStringLiteral("http://127.0.0.1:%1/mcp").arg(m_mcpHttpPort);
    } else {
        // stdio mode: command-based config
        auto mcpPath = getMcpBinaryPath();
#ifdef Q_OS_WIN
        serverConfig["command"] = QStringLiteral("cmd");
        serverConfig["args"] = QJsonArray({QStringLiteral("/c"), mcpPath});
#else
        serverConfig["command"] = mcpPath;
        serverConfig["args"] = QJsonArray();
#endif
    }
    return serverConfig;
}

QString MainController::generateMcpConfig(const QString& clientType) const {
    auto serverConfig = buildMcpServerConfig(m_mcpTransportMode);

    // Set transport type field per client convention
    if (m_mcpTransportMode == "http") {
        if (clientType == "vscode") {
            serverConfig["type"] = QStringLiteral("http");
        } else {
            // Cursor, Claude Desktop, Windsurf: use "sse"
            serverConfig["type"] = QStringLiteral("sse");
        }
    }

    QJsonObject servers;
    servers["quickdesk"] = serverConfig;

    QJsonObject root;
    if (clientType == "vscode") {
        root["servers"] = servers;        // VS Code uses "servers"
    } else {
        root["mcpServers"] = servers;     // Cursor, Claude, Windsurf use "mcpServers"
    }

    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void MainController::copyMcpConfig(const QString& clientType) {
    auto config = generateMcpConfig(clientType);
    auto* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(config);
    }
}

QString MainController::getMcpConfigPath(const QString& clientType) const {
    if (clientType == "claude") {
#ifdef Q_OS_WIN
        return QDir::toNativeSeparators(
            qEnvironmentVariable("APPDATA") + "/Claude/claude_desktop_config.json");
#elif defined(Q_OS_MAC)
        return QDir::homePath() +
               "/Library/Application Support/Claude/claude_desktop_config.json";
#endif
    } else if (clientType == "cursor") {
#ifdef Q_OS_WIN
        return QDir::toNativeSeparators(
            QDir::homePath() + "/.cursor/mcp.json");
#elif defined(Q_OS_MAC)
        return QDir::homePath() + "/.cursor/mcp.json";
#endif
    } else if (clientType == "windsurf") {
#ifdef Q_OS_WIN
        return QDir::toNativeSeparators(
            QDir::homePath() + "/.codeium/windsurf/mcp_config.json");
#elif defined(Q_OS_MAC)
        return QDir::homePath() + "/.codeium/windsurf/mcp_config.json";
#endif
    } else if (clientType == "vscode") {
#ifdef Q_OS_WIN
        return QDir::toNativeSeparators(
            qEnvironmentVariable("APPDATA") + "/Code/User/mcp.json");
#elif defined(Q_OS_MAC)
        return QDir::homePath() +
               "/Library/Application Support/Code/User/mcp.json";
#endif
    }
    return QString();
}

int MainController::writeMcpConfig(const QString& clientType) {
    auto configPath = getMcpConfigPath(clientType);
    if (configPath.isEmpty()) {
        return 2;
    }

    QJsonObject existingRoot;
    QFile file(configPath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            existingRoot = doc.object();
        }
        file.close();
    }

    auto serverConfig = buildMcpServerConfig(m_mcpTransportMode);
    if (m_mcpTransportMode == "http") {
        if (clientType == "vscode") {
            serverConfig["type"] = QStringLiteral("http");
        } else {
            serverConfig["type"] = QStringLiteral("sse");
        }
    }

    // VS Code uses "servers" key; others use "mcpServers"
    auto serversKey = (clientType == "vscode")
                          ? QStringLiteral("servers")
                          : QStringLiteral("mcpServers");
    auto servers = existingRoot[serversKey].toObject();
    servers["quickdesk"] = serverConfig;
    existingRoot[serversKey] = servers;

    QDir().mkpath(QFileInfo(configPath).absolutePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR("Failed to write MCP config to: {}",
                  configPath.toStdString());
        return 2;
    }

    file.write(QJsonDocument(existingRoot).toJson(QJsonDocument::Indented));
    file.close();
    LOG_INFO("MCP config written to: {}", configPath.toStdString());
    return 0;
}

} // namespace quickdesk
