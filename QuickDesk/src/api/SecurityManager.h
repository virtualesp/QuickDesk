// Copyright 2026 QuickDesk Authors

#ifndef QUICKDESK_API_SECURITYMANAGER_H
#define QUICKDESK_API_SECURITYMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

namespace spdlog { class logger; }

namespace quickdesk {

class SecurityManager : public QObject {
    Q_OBJECT

public:
    enum PermissionLevel {
        NoAccess = 0,
        ReadOnly = 1,
        FullControl = 2,
    };

    explicit SecurityManager(QObject* parent = nullptr);
    ~SecurityManager() override;

    // --- Authentication & Authorization ---

    void setFullAccessToken(const QString& token);
    void setReadOnlyToken(const QString& token);
    PermissionLevel authenticateToken(const QString& token) const;

    void setAllowedDevices(const QStringList& deviceIds);
    bool isDeviceAllowed(const QString& deviceId) const;

    bool isMethodAllowedForPermission(const QString& method,
                                      PermissionLevel level) const;

    // --- Rate Limiting ---

    void setRateLimitPerMinute(int maxRequests);
    bool checkRateLimit(const QString& clientId);

    // --- Session Timeout ---

    void setSessionTimeoutSecs(int seconds);
    void recordActivity(const QString& clientId);
    void removeClient(const QString& clientId);
    QStringList expiredClients() const;

    // --- Dangerous Operations ---

    bool isDangerousOperation(const QString& method,
                              const QJsonObject& params) const;

    // --- Audit Logging ---

    void initAuditLog(const QString& logDir);
    void logAudit(const QString& clientId,
                  const QString& method,
                  const QJsonObject& params,
                  bool allowed,
                  const QString& denialReason = {});

signals:
    void dangerousOperationBlocked(const QString& clientId,
                                   const QString& method,
                                   const QJsonObject& params);
    void rateLimitExceeded(const QString& clientId,
                           const QString& method);
    void sessionExpired(const QString& clientId);
    void anomalyDetected(const QString& clientId,
                         const QString& description);

private:
    struct ClientRateState {
        QList<qint64> requestTimestamps;
    };

    QString m_fullAccessToken;
    QString m_readOnlyToken;
    QSet<QString> m_allowedDevices;
    bool m_deviceFilterEnabled = false;

    int m_rateLimitPerMinute = 0;
    QMap<QString, ClientRateState> m_rateLimiter;

    int m_sessionTimeoutSecs = 0;
    QMap<QString, qint64> m_lastActivity;
    QTimer m_sessionCheckTimer;

    QSet<QString> m_readOnlyMethods;

    std::shared_ptr<spdlog::logger> m_auditLogger;

    void checkSessionTimeouts();
};

} // namespace quickdesk

#endif // QUICKDESK_API_SECURITYMANAGER_H
