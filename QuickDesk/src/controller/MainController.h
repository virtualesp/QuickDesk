// Copyright 2026 QuickDesk Authors
// Main controller for QuickDesk Qt application

#ifndef QUICKDESK_CONTROLLER_MAINCONTROLLER_H
#define QUICKDESK_CONTROLLER_MAINCONTROLLER_H

#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <memory>

#include "../manager/ServerManager.h"
#include "../manager/HostManager.h"
#include "../manager/ClientManager.h"
#include "../manager/TurnServerManager.h"
#include "../manager/RemoteDeviceManager.h"
#include "../manager/PresetManager.h"
#include "../common/ProcessStatus.h"

namespace quickdesk {

class ProcessManager;
class WebSocketApiServer;

/**
 * @brief Main controller that coordinates all managers
 * 
 * Exposed to QML as the primary interface for the application.
 */
class MainController : public QObject {
    Q_OBJECT
    Q_PROPERTY(ServerManager* serverManager READ serverManager CONSTANT)
    Q_PROPERTY(HostManager* hostManager READ hostManager CONSTANT)
    Q_PROPERTY(ClientManager* clientManager READ clientManager CONSTANT)
    Q_PROPERTY(TurnServerManager* turnServerManager READ turnServerManager CONSTANT)
    Q_PROPERTY(RemoteDeviceManager* remoteDeviceManager READ remoteDeviceManager CONSTANT)
    Q_PROPERTY(PresetManager* presetManager READ presetManager CONSTANT)
    
    // Host status
    Q_PROPERTY(ProcessStatus::Status hostProcessStatus READ hostProcessStatus NOTIFY hostProcessStatusChanged)
    Q_PROPERTY(ServerStatus::Status hostServerStatus READ hostServerStatus NOTIFY hostServerStatusChanged)
    Q_PROPERTY(HostLaunchMode::Mode hostLaunchMode READ hostLaunchMode NOTIFY hostLaunchModeChanged)
    
    // Client status
    Q_PROPERTY(ProcessStatus::Status clientProcessStatus READ clientProcessStatus NOTIFY clientProcessStatusChanged)
    Q_PROPERTY(ServerStatus::Status clientServerStatus READ clientServerStatus NOTIFY clientServerStatusChanged)
    
    // Access code auto-refresh info
    Q_PROPERTY(QString nextAccessCodeRefreshTime READ nextAccessCodeRefreshTime NOTIFY nextAccessCodeRefreshTimeChanged)

    // Host properties (convenience for QML)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY deviceIdChanged)
    Q_PROPERTY(QString accessCode READ accessCode NOTIFY accessCodeChanged)
    Q_PROPERTY(bool isHostConnected READ isHostConnected NOTIFY hostConnectionChanged)
    
    // MCP Service status
    Q_PROPERTY(bool mcpServiceRunning READ mcpServiceRunning NOTIFY mcpServiceRunningChanged)
    Q_PROPERTY(int mcpConnectedClients READ mcpConnectedClients NOTIFY mcpConnectedClientsChanged)
    Q_PROPERTY(int mcpPort READ mcpPort NOTIFY mcpServiceRunningChanged)
    Q_PROPERTY(QString mcpTransportMode READ mcpTransportMode WRITE setMcpTransportMode NOTIFY mcpTransportModeChanged)
    Q_PROPERTY(int mcpHttpPort READ mcpHttpPort WRITE setMcpHttpPort NOTIFY mcpHttpPortChanged)
    Q_PROPERTY(QString mcpHttpUrl READ mcpHttpUrl NOTIFY mcpServiceRunningChanged)

    // Signaling state properties (convenience for QML)
    Q_PROPERTY(QString signalingState READ signalingState NOTIFY signalingStateChanged)
    Q_PROPERTY(int signalingRetryCount READ signalingRetryCount NOTIFY signalingStateChanged)
    Q_PROPERTY(int signalingNextRetryIn READ signalingNextRetryIn NOTIFY signalingStateChanged)
    Q_PROPERTY(QString signalingError READ signalingError NOTIFY signalingStateChanged)
    Q_PROPERTY(QString signalingStatusText READ signalingStatusText NOTIFY signalingStateChanged)

public:
    explicit MainController(QObject* parent = nullptr);
    ~MainController() override;

    /**
     * @brief Initialize all components
     * 
     * Starts Host and Client processes.
     */
    Q_INVOKABLE void initialize();

    /**
     * @brief Shutdown all components
     */
    Q_INVOKABLE void shutdown();

    /**
     * @brief Connect to a remote host
     * @return Connection ID
     */
    Q_INVOKABLE QString connectToRemoteHost(const QString& deviceId,
                                            const QString& accessCode,
                                            const QString& serverUrl = QString());

    /**
     * @brief Disconnect from a remote host
     */
    Q_INVOKABLE void disconnectFromRemoteHost(const QString& connectionId);

    /**
     * @brief Refresh access code
     */
    Q_INVOKABLE void refreshAccessCode();
    
    /**
     * @brief Reset access code refresh timer (called after manual refresh)
     */
    void resetAccessCodeRefreshTimer();

    /**
     * @brief Copy text to clipboard
     */
    Q_INVOKABLE void copyToClipboard(const QString& text);

    /**
     * @brief Copy device info (ID and access code) to clipboard
     */
    Q_INVOKABLE void copyDeviceInfo();

    // Property getters
    ServerManager* serverManager() const;
    HostManager* hostManager() const;
    ClientManager* clientManager() const;
    TurnServerManager* turnServerManager() const;
    RemoteDeviceManager* remoteDeviceManager() const;
    PresetManager* presetManager() const;

    // Host convenience properties
    QString deviceId() const;
    QString accessCode() const;
    bool isHostConnected() const;
    
    // Signaling state convenience properties
    QString signalingState() const;
    int signalingRetryCount() const;
    int signalingNextRetryIn() const;
    QString signalingError() const;
    QString signalingStatusText() const;
    
    // Status getters
    ProcessStatus::Status hostProcessStatus() const;
    ServerStatus::Status hostServerStatus() const;
    ProcessStatus::Status clientProcessStatus() const;
    ServerStatus::Status clientServerStatus() const;
    HostLaunchMode::Mode hostLaunchMode() const;
    
    // MCP service getters
    bool mcpServiceRunning() const;
    int mcpConnectedClients() const;
    int mcpPort() const;
    QString mcpTransportMode() const;
    void setMcpTransportMode(const QString& mode);
    int mcpHttpPort() const;
    void setMcpHttpPort(int port);
    QString mcpHttpUrl() const;

    // Access code auto-refresh info
    QString nextAccessCodeRefreshTime() const;

    Q_INVOKABLE void showRemoteWindowForConnection(const QString& connectionId, const QString& deviceId);

    // MCP Service control
    Q_INVOKABLE void startMcpService();
    Q_INVOKABLE void stopMcpService();
    Q_INVOKABLE QString getMcpBinaryPath() const;
    QJsonObject buildMcpServerConfig(const QString& transport = "stdio") const;
    Q_INVOKABLE QString generateMcpConfig(const QString& clientType) const;
    Q_INVOKABLE void copyMcpConfig(const QString& clientType);
    Q_INVOKABLE QString getMcpConfigPath(const QString& clientType) const;    

    // Returns: 0 = success, 1 = client not installed, 2 = write failed
    Q_INVOKABLE int writeMcpConfig(const QString& clientType);

signals:
    void initializationFailed(const QString& error);
    void deviceIdChanged();
    void accessCodeChanged();
    void hostConnectionChanged();
    void signalingStateChanged();
    void hostProcessStatusChanged();
    void hostServerStatusChanged();
    void hostLaunchModeChanged();
    void clientProcessStatusChanged();
    void clientServerStatusChanged();
    void nextAccessCodeRefreshTimeChanged();
    void presetLoadFailed(const QString& error);
    void forceUpgradeRequired(const QString& minVersion);
    void requestShowRemoteWindow(const QString& connectionId, const QString& deviceId);
    void mcpServiceRunningChanged();
    void mcpConnectedClientsChanged();
    void mcpTransportModeChanged();
    void mcpHttpPortChanged();

private slots:
    void onHostProcessStarted();
    void onHostProcessStopped(int exitCode);
    void onHostProcessError(const QString& error);
    void onHostProcessRestarting(int retryCount, int maxRetries);
    
    void onClientProcessStarted();
    void onClientProcessStopped(int exitCode);
    void onClientProcessError(const QString& error);
    void onClientProcessRestarting(int retryCount, int maxRetries);
    void onClientSignalingStateChanged(const QString& connectionId,
                                       const QString& state,
                                       int retryCount,
                                       int nextRetryIn,
                                       const QString& error);
    
    void onHostReady(const QString& deviceId, const QString& accessCode);

private:
    std::unique_ptr<ProcessManager> m_processManager;
    std::unique_ptr<ServerManager> m_serverManager;
    std::unique_ptr<TurnServerManager> m_turnServerManager;
    std::unique_ptr<HostManager> m_hostManager;
    std::unique_ptr<ClientManager> m_clientManager;
    std::unique_ptr<RemoteDeviceManager> m_remoteDeviceManager;
    std::unique_ptr<PresetManager> m_presetManager;
    std::unique_ptr<WebSocketApiServer> m_wsApiServer;

    QString m_deviceId;
    QString m_accessCode;
    
    // Server status (managed by MainController)
    ServerStatus::Status m_hostServerStatus = ServerStatus::Disconnected;
    ServerStatus::Status m_clientServerStatus = ServerStatus::Disconnected;
    QString m_primaryConnectionId;  // Track primary connection for client signaling status
    
    // Access code auto-refresh timer
    QTimer m_accessCodeRefreshTimer;
    int m_accessCodeRefreshIntervalMinutes = -1;  // -1 = disabled
    QDateTime m_nextRefreshTime;  // Next scheduled refresh time
    
    void onAccessCodeRefreshTimer();
    void updateAccessCodeRefreshTimer(int remainingSeconds = -1);
    QString getDefaultServerUrl() const;
    void setupWebSocketApiEvents();

    // MCP HTTP process management
    void startMcpHttpProcess();
    void stopMcpHttpProcess();

    QProcess* m_mcpHttpProcess = nullptr;
    QString m_mcpTransportMode = QStringLiteral("stdio");
    int m_mcpHttpPort = 18080;
    bool m_isShutdown = false;

    // screenChanged event: per-connection throttle state
    struct ScreenChangeState {
        QString lastBroadcastHash;
        qint64  lastBroadcastMs = 0;
    };
    QHash<QString, ScreenChangeState> m_screenChangeState;
    QMutex m_screenChangeMutex;
};

} // namespace quickdesk

#endif // QUICKDESK_CONTROLLER_MAINCONTROLLER_H
