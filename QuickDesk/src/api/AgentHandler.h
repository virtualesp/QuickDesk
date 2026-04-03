// Copyright 2026 QuickDesk Authors
// Agent bridge handler — routes agentExec / agentListTools from MCP to the
// remote host agent via the qd-agent-bridge WebRTC data channel.

#ifndef QUICKDESK_API_AGENTHANDLER_H
#define QUICKDESK_API_AGENTHANDLER_H

#include <QAtomicInt>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QWaitCondition>

namespace quickdesk {

class MainController;

/**
 * @brief Handles agentExec and agentListTools MCP tool calls.
 *
 * Thread safety: handleAgentExec / handleAgentListTools are designed to be
 * called from *worker threads* (e.g. QThreadPool).  They block the calling
 * worker thread via QWaitCondition (NOT QEventLoop) until the remote agent
 * replies or the timeout elapses.
 *
 * onAgentResponse runs on the *main thread* (direct signal connection from
 * ClientManager) and wakes the waiting worker thread.
 */
class AgentHandler : public QObject {
    Q_OBJECT

public:
    explicit AgentHandler(MainController* controller, QObject* parent = nullptr);

    // Thread-safe: can be called from any thread (dispatched via QThreadPool).
    QJsonObject handleAgentExec(const QJsonObject& params);
    QJsonObject handleAgentListTools(const QJsonObject& params);

    // Called on the main thread when an agentBridgeResponse arrives.
    void onAgentResponse(const QString& deviceId,
                         const QJsonObject& response);

private:
    bool isConnectionValid(const QString& deviceId) const;

    QJsonObject sendAndWait(const QString& deviceId,
                            const QJsonObject& payload,
                            int timeoutMs = 10000);

    QString nextRequestId();

    MainController* m_controller;
    QAtomicInt      m_nextId{1};

    struct PendingRequest {
        QMutex*         mutex  = nullptr;
        QWaitCondition* cond   = nullptr;
        QJsonObject*    result = nullptr;
        bool*           done   = nullptr;
    };
    QMutex m_pendingMutex;
    QMap<QString, PendingRequest> m_pending;
};

} // namespace quickdesk

#endif // QUICKDESK_API_AGENTHANDLER_H
