// Copyright 2026 QuickDesk Authors
// Trust layer handler — shows confirmation dialogs for high-risk operations
// and handles emergency stop from the MCP bridge.

#ifndef QUICKDESK_API_TRUSTHANDLER_H
#define QUICKDESK_API_TRUSTHANDLER_H

#include <QAtomicInt>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QWaitCondition>

namespace quickdesk {

class MainController;

class TrustHandler : public QObject {
    Q_OBJECT

public:
    explicit TrustHandler(MainController* controller, QObject* parent = nullptr);

    QJsonObject handleRequestConfirmation(const QJsonObject& params);
    QJsonObject handleEmergencyStop(const QJsonObject& params);
    QJsonObject handleDeactivateEmergency(const QJsonObject& params);

    void resolveConfirmation(const QString& confirmationId, bool approved, const QString& reason);

signals:
    void confirmationRequested(const QString& confirmationId,
                               const QString& deviceId,
                               const QString& toolName,
                               const QString& argumentsJson,
                               const QString& riskLevel,
                               const QStringList& reasons,
                               int timeoutSecs);
    void emergencyStopActivated(const QString& reason);
    void emergencyStopDeactivated();

private:
    MainController* m_controller;

    struct PendingConfirmation {
        QMutex*         mutex    = nullptr;
        QWaitCondition* cond     = nullptr;
        bool*           approved = nullptr;
        QString*        reason   = nullptr;
        bool*           done     = nullptr;
    };
    QMutex m_pendingMutex;
    QMap<QString, PendingConfirmation> m_pending;
    QAtomicInt m_nextId{1};
};

} // namespace quickdesk

#endif // QUICKDESK_API_TRUSTHANDLER_H
