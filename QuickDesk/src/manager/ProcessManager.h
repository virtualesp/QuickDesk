// Copyright 2026 QuickDesk Authors
// Process lifecycle management

#ifndef QUICKDESK_MANAGER_PROCESSMANAGER_H
#define QUICKDESK_MANAGER_PROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <memory>

namespace quickdesk {

class NativeMessaging;

/**
 * @brief Manages the lifecycle of Host and Client processes
 */
class ProcessManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hostAutoRestart READ hostAutoRestart WRITE setHostAutoRestart NOTIFY hostAutoRestartChanged)
    Q_PROPERTY(bool clientAutoRestart READ clientAutoRestart WRITE setClientAutoRestart NOTIFY clientAutoRestartChanged)
    Q_PROPERTY(QString hostStatus READ hostStatus NOTIFY hostStatusChanged)
    Q_PROPERTY(QString clientStatus READ clientStatus NOTIFY clientStatusChanged)

public:
    explicit ProcessManager(QObject* parent = nullptr);
    ~ProcessManager() override;

    // Process management
    bool startHostProcess();
    bool startClientProcess();
    
    void stopHostProcess();
    void stopClientProcess();
    void stopAllProcesses();

    bool isHostRunning() const;
    bool isClientRunning() const;

    // Get Native Messaging handlers
    NativeMessaging* hostMessaging() const;
    NativeMessaging* clientMessaging() const;

    // Executable paths
    void setHostExePath(const QString& path);
    void setClientExePath(const QString& path);
    QString hostExePath() const;
    QString clientExePath() const;

    // Log directory
    void setLogDir(const QString& logDir);
    QString logDir() const;

    // Auto-detect executable paths
    bool autoDetectPaths();

    // Auto-restart settings
    bool hostAutoRestart() const;
    void setHostAutoRestart(bool enabled);
    bool clientAutoRestart() const;
    void setClientAutoRestart(bool enabled);

    // Status
    QString hostStatus() const;
    QString clientStatus() const;

    // Reset retry counts (call after successful connection)
    void resetHostRetryCount();
    void resetClientRetryCount();

signals:
    void hostProcessStarted();
    void hostProcessStopped(int exitCode);
    void hostProcessError(const QString& error);
    void hostProcessRestarting(int retryCount, int maxRetries);
    void hostAutoRestartChanged();
    void hostStatusChanged();

    void clientProcessStarted();
    void clientProcessStopped(int exitCode);
    void clientProcessError(const QString& error);
    void clientProcessRestarting(int retryCount, int maxRetries);
    void clientAutoRestartChanged();
    void clientStatusChanged();

private slots:
    void onHostProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onClientProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onHostRestartTimer();
    void onClientRestartTimer();

private:
    std::unique_ptr<QProcess> m_hostProcess;
    std::unique_ptr<QProcess> m_clientProcess;
    
    std::unique_ptr<NativeMessaging> m_hostMessaging;
    std::unique_ptr<NativeMessaging> m_clientMessaging;

    QString m_hostExePath;
    QString m_clientExePath;
    QString m_logDir;

    // Auto-restart settings
    bool m_hostAutoRestart = true;
    bool m_clientAutoRestart = true;
    static const int MAX_RESTART_ATTEMPTS = 5;
    static const int BASE_RESTART_DELAY_MS = 2000; // 2 seconds

    // Host restart state
    int m_hostRestartCount = 0;
    bool m_hostStoppingIntentionally = false;
    QTimer m_hostRestartTimer;
    QString m_hostStatus = "stopped";

    // Client restart state
    int m_clientRestartCount = 0;
    bool m_clientStoppingIntentionally = false;
    QTimer m_clientRestartTimer;
    QString m_clientStatus = "stopped";

    bool startProcess(QProcess* process, const QString& exePath, 
                      const QString& processName, const QString& logDir);
    QString findExecutable(const QString& name);
    int calculateRestartDelay(int retryCount) const;
    void setHostStatus(const QString &status);
    void setClientStatus(const QString &status);
};

} // namespace quickdesk

#endif // QUICKDESK_MANAGER_PROCESSMANAGER_H
