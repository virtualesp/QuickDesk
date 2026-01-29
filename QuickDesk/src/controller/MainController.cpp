// Copyright 2026 QuickDesk Authors

#include "MainController.h"
#include "../manager/ProcessManager.h"
#include "../manager/NativeMessaging.h"
#include "infra/env/applicationcontext.h"
#include "infra/log/log.h"
#include "core/localconfigcenter.h"
#include <QTimer>
#include <QClipboard>
#include <QGuiApplication>
#include <QDateTime>

namespace quickdesk {

MainController::MainController(QObject* parent)
    : QObject(parent)
    , m_processManager(std::make_unique<ProcessManager>(this))
    , m_serverManager(std::make_unique<ServerManager>(this))
    , m_hostManager(std::make_unique<HostManager>(this))
    , m_clientManager(std::make_unique<ClientManager>(this))
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
    connect(m_processManager.get(), &ProcessManager::hostStatusChanged,
            this, &MainController::onHostStatusChanged);
    
    connect(m_processManager.get(), &ProcessManager::clientProcessStarted,
            this, &MainController::onClientProcessStarted);
    connect(m_processManager.get(), &ProcessManager::clientProcessStopped,
            this, &MainController::onClientProcessStopped);
    connect(m_processManager.get(), &ProcessManager::clientProcessError,
            this, &MainController::onClientProcessError);
    connect(m_processManager.get(), &ProcessManager::clientProcessRestarting,
            this, &MainController::onClientProcessRestarting);
    connect(m_processManager.get(), &ProcessManager::clientStatusChanged,
            this, &MainController::onClientStatusChanged);

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
    updateInitStatus("正在初始化...");

    // Auto-detect executable paths
    if (!m_processManager->autoDetectPaths()) {
        LOG_WARN("Could not auto-detect all executable paths");
    }

    // Set log directory from ApplicationContext
    QString logDir = infra::ApplicationContext::instance().applicationDirPath();
    m_processManager->setLogDir(logDir);

    // Start Host process
    updateInitStatus("启动 Host 进程...");
    if (!m_processManager->startHostProcess()) {
        updateInitStatus("Host 进程启动失败");
        emit initializationFailed("Failed to start Host process");
        return;
    }

    // Start Client process
    updateInitStatus("启动 Client 进程...");
    if (!m_processManager->startClientProcess()) {
        updateInitStatus("Client 进程启动失败");
        emit initializationFailed("Failed to start Client process");
        return;
    }

    // Initialization will complete when both processes are ready
    updateInitStatus("等待进程就绪...");
}

void MainController::shutdown()
{
    LOG_INFO("MainController::shutdown()");
    
    m_hostManager->disconnectFromServer();
    m_clientManager->disconnectAll();
    
    m_processManager->stopAllProcesses();

    m_isInitialized = false;
    emit initializedChanged();
}

void MainController::startHosting(const QString& serverUrl)
{
    QString url = serverUrl.isEmpty() ? getDefaultServerUrl() : serverUrl;
    LOG_INFO("Starting hosting on: {}", url.toStdString());
    m_lastServerUrl = url;
    m_hostWasHosting = true;
    
    // Check if we should use saved access code (never refresh mode)
    int interval = core::LocalConfigCenter::instance().accessCodeRefreshInterval();
    QString savedAccessCode = core::LocalConfigCenter::instance().savedAccessCode();
    
    if (interval == -1 && !savedAccessCode.isEmpty()) {
        // Pass saved access code to host
        LOG_INFO("Using saved access code for 'never refresh' mode");
        m_hostManager->connectToServer(url, savedAccessCode);
    } else {
        // Normal connection (host will generate new access code)
        m_hostManager->connectToServer(url, QString());
    }
}

void MainController::stopHosting()
{
    m_hostWasHosting = false;
    m_hostManager->disconnectFromServer();
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
    
    QString info = QString("设备ID: %1\n访问码: %2").arg(deviceId, accessCode);
    copyToClipboard(info);
}

bool MainController::isInitialized() const
{
    return m_isInitialized;
}

QString MainController::initStatus() const
{
    return m_initStatus;
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
        return "已连接";
    } else if (state == "connecting") {
        return "正在连接...";
    } else if (state == "disconnected") {
        return "未连接";
    } else if (state == "failed") {
        QString msg = "连接失败";
        if (!error.isEmpty()) {
            msg += QString(": %1").arg(error);
        }
        return msg;
    } else if (state == "reconnecting") {
        QString msg = QString("重连中(第%1次)").arg(retryCount);
        if (nextRetry > 0) {
            msg += QString(", %1秒后重试").arg(nextRetry);
        }
        return msg;
    }
    return state;
}

QString MainController::hostProcessStatus() const
{
    QString status = m_processManager->hostStatus();
    if (status == "running") {
        return "运行中";
    } else if (status == "stopped") {
        return "已停止";
    } else if (status == "failed") {
        return "启动失败";
    } else if (status.startsWith("restarting:")) {
        int count = status.mid(11).toInt();
        return QString("重启中(第%1次)").arg(count);
    }
    return status;
}

QString MainController::clientProcessStatus() const
{
    QString status = m_processManager->clientStatus();
    if (status == "running") {
        return "运行中";
    } else if (status == "stopped") {
        return "已停止";
    } else if (status == "failed") {
        return "启动失败";
    } else if (status.startsWith("restarting:")) {
        int count = status.mid(11).toInt();
        return QString("重启中(第%1次)").arg(count);
    }
    return status;
}

QString MainController::nextAccessCodeRefreshTime() const
{
    if (m_accessCodeRefreshIntervalMinutes <= 0) {
        return "永不";
    }
    
    if (!m_nextRefreshTime.isValid()) {
        return "永不";
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
    
    // Send hello to verify communication
    QTimer::singleShot(500, this, [this]() {
        m_hostManager->sendHello();
        checkInitialized();
        
        // Auto-reconnect to signaling server if we were hosting before
        if (m_hostWasHosting && !m_lastServerUrl.isEmpty()) {
            LOG_INFO("Auto-reconnecting to signaling server after Host restart");
            QTimer::singleShot(500, this, [this]() {
                // Check if we should use saved access code (never refresh mode)
                QString savedAccessCode;
                int interval = core::LocalConfigCenter::instance().accessCodeRefreshInterval();
                if (interval == -1) {
                    savedAccessCode = core::LocalConfigCenter::instance().savedAccessCode();
                }

                m_hostManager->connectToServer(m_lastServerUrl, savedAccessCode);
            });
        }
    });
}

void MainController::onHostProcessStopped(int exitCode)
{
    LOG_INFO("Host process stopped with exit code: {}", exitCode);
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
    updateInitStatus(QString("Host 进程重启中 (%1/%2)...").arg(retryCount).arg(maxRetries));
    emit hostProcessRestarting(retryCount, maxRetries);
}

void MainController::onHostStatusChanged()
{
    emit hostProcessStatusChanged();
}

void MainController::onClientProcessStarted()
{
    LOG_INFO("Client process started");
    
    // Reset retry count on successful start
    m_processManager->resetClientRetryCount();
    
    // Set up Native Messaging
    m_clientManager->setMessaging(m_processManager->clientMessaging());
    
    // Send hello to verify communication
    QTimer::singleShot(500, this, [this]() {
        m_clientManager->sendHello();
        checkInitialized();
    });
}

void MainController::onClientProcessStopped(int exitCode)
{
    LOG_INFO("Client process stopped with exit code: {}", exitCode);
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
    emit clientProcessRestarting(retryCount, maxRetries);
}

void MainController::onClientStatusChanged()
{
    emit clientProcessStatusChanged();
}

void MainController::onHostReady(const QString& deviceId, const QString& accessCode)
{
    LOG_INFO("Host ready - Device ID: {} Access Code: {}", deviceId.toStdString(), accessCode.toStdString());
    m_deviceId = deviceId;
    m_accessCode = accessCode;
    
    // Load access code refresh interval from config
    m_accessCodeRefreshIntervalMinutes = core::LocalConfigCenter::instance().accessCodeRefreshInterval();
    LOG_INFO("Access code refresh interval: {} minutes", m_accessCodeRefreshIntervalMinutes);
    
    // Save access code for "never refresh" mode
    if (m_accessCodeRefreshIntervalMinutes == -1) {
        core::LocalConfigCenter::instance().setSavedAccessCode(accessCode);
        LOG_INFO("Saved access code for 'never refresh' mode: {}", accessCode.toStdString());
    } else {
        // Auto-refresh enabled - start timer
        LOG_INFO("Starting access code auto-refresh timer: {} minutes", m_accessCodeRefreshIntervalMinutes);
        updateAccessCodeRefreshTimer();
    }

    QTimer::singleShot(0, this, [this]() {
        emit deviceIdChanged();
        emit accessCodeChanged();
    });
}

void MainController::updateInitStatus(const QString& status)
{
    if (m_initStatus != status) {
        m_initStatus = status;
        emit initStatusChanged();
    }
}

void MainController::checkInitialized()
{
    // Check if both processes are running
    if (m_processManager->isHostRunning() && m_processManager->isClientRunning()) {
        m_isInitialized = true;
        updateInitStatus("已就绪");
        emit initializedChanged();
        
        // Auto-start hosting (connect to signaling server)
        QTimer::singleShot(1000, this, [this]() {
            startHosting();
        });
    }
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
    
    // Update next refresh time
    if (m_accessCodeRefreshIntervalMinutes > 0) {
        m_nextRefreshTime = QDateTime::currentDateTime().addSecs(m_accessCodeRefreshIntervalMinutes * 60);
        emit nextAccessCodeRefreshTimeChanged();
    }
}

void MainController::updateAccessCodeRefreshTimer()
{
    // Stop existing timer
    m_accessCodeRefreshTimer.stop();
    m_nextRefreshTime = QDateTime();
    emit nextAccessCodeRefreshTimeChanged();
    
    // -1 means never refresh
    if (m_accessCodeRefreshIntervalMinutes <= 0) {
        LOG_INFO("Access code auto-refresh disabled (interval: {})", m_accessCodeRefreshIntervalMinutes);
        return;
    }
    
    // Start timer with interval in milliseconds
    int intervalMs = m_accessCodeRefreshIntervalMinutes * 60 * 1000;
    m_accessCodeRefreshTimer.start(intervalMs);
    
    // Set next refresh time
    m_nextRefreshTime = QDateTime::currentDateTime().addSecs(m_accessCodeRefreshIntervalMinutes * 60);
    emit nextAccessCodeRefreshTimeChanged();
    
    LOG_INFO("Access code auto-refresh timer started: {} minutes ({} ms), next at {}", 
             m_accessCodeRefreshIntervalMinutes, intervalMs, 
             m_nextRefreshTime.toString("MM-dd HH:mm:ss").toStdString());
}

} // namespace quickdesk
