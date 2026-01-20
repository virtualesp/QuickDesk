// Copyright 2026 QuickDesk Authors

#include "MainController.h"
#include "../manager/ProcessManager.h"
#include "../manager/NativeMessaging.h"
#include <QDebug>
#include <QTimer>
#include <QClipboard>
#include <QGuiApplication>

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
    
    connect(m_processManager.get(), &ProcessManager::clientProcessStarted,
            this, &MainController::onClientProcessStarted);
    connect(m_processManager.get(), &ProcessManager::clientProcessStopped,
            this, &MainController::onClientProcessStopped);
    connect(m_processManager.get(), &ProcessManager::clientProcessError,
            this, &MainController::onClientProcessError);

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
}

MainController::~MainController()
{
    shutdown();
}

void MainController::initialize()
{
    qInfo() << "MainController::initialize()";
    updateInitStatus("正在初始化...");

    // Auto-detect executable paths
    if (!m_processManager->autoDetectPaths()) {
        qWarning() << "Could not auto-detect all executable paths";
    }

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
    qInfo() << "MainController::shutdown()";
    
    m_hostManager->disconnectFromServer();
    m_clientManager->disconnectAll();
    
    m_processManager->stopAllProcesses();

    m_isInitialized = false;
    emit initializedChanged();
}

void MainController::startHosting(const QString& serverUrl)
{
    QString url = serverUrl.isEmpty() ? getDefaultServerUrl() : serverUrl;
    qInfo() << "Starting hosting on:" << url;
    m_hostManager->connectToServer(url);
}

void MainController::stopHosting()
{
    m_hostManager->disconnectFromServer();
}

QString MainController::connectToRemoteHost(const QString& deviceId,
                                            const QString& accessCode,
                                            const QString& serverUrl)
{
    QString url = serverUrl.isEmpty() ? getDefaultServerUrl() : serverUrl;
    qInfo() << "Connecting to remote host:" << deviceId << "on" << url;
    return m_clientManager->connectToHost(deviceId, accessCode, url);
}

void MainController::disconnectFromRemoteHost(const QString& connectionId)
{
    m_clientManager->disconnectFromHost(connectionId);
}

void MainController::refreshTempPassword()
{
    m_hostManager->refreshTempPassword();
}

void MainController::copyToClipboard(const QString& text)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
        qInfo() << "Copied to clipboard:" << text;
    }
}

void MainController::copyDeviceInfo()
{
    QString deviceId = m_hostManager->deviceId();
    QString accessCode = m_hostManager->accessCode();
    
    if (deviceId.isEmpty() && accessCode.isEmpty()) {
        qWarning() << "No device info to copy";
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

void MainController::onHostProcessStarted()
{
    qInfo() << "Host process started";
    
    // Set up Native Messaging
    m_hostManager->setMessaging(m_processManager->hostMessaging());
    
    // Send hello to verify communication
    QTimer::singleShot(500, this, [this]() {
        m_hostManager->sendHello();
        checkInitialized();
    });
}

void MainController::onHostProcessStopped(int exitCode)
{
    qInfo() << "Host process stopped with exit code:" << exitCode;
    m_hostManager->setMessaging(nullptr);
}

void MainController::onHostProcessError(const QString& error)
{
    qWarning() << "Host process error:" << error;
    emit initializationFailed(QString("Host error: %1").arg(error));
}

void MainController::onClientProcessStarted()
{
    qInfo() << "Client process started";
    
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
    qInfo() << "Client process stopped with exit code:" << exitCode;
    m_clientManager->setMessaging(nullptr);
}

void MainController::onClientProcessError(const QString& error)
{
    qWarning() << "Client process error:" << error;
    emit initializationFailed(QString("Client error: %1").arg(error));
}

void MainController::onHostReady(const QString& deviceId, const QString& accessCode)
{
    qInfo() << "Host ready - Device ID:" << deviceId << "Access Code:" << accessCode;
    m_deviceId = deviceId;
    m_accessCode = accessCode;
    emit deviceIdChanged();
    emit accessCodeChanged();
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

} // namespace quickdesk
