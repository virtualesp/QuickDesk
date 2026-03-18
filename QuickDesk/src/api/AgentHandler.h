// Copyright 2026 QuickDesk Authors
// Agent bridge handler — routes agentExec / agentListTools from MCP to the
// remote host agent via the qd-agent-bridge WebRTC data channel.

#ifndef QUICKDESK_API_AGENTHANDLER_H
#define QUICKDESK_API_AGENTHANDLER_H

#include <QEventLoop>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

namespace quickdesk {

class MainController;

/**
 * @brief Handles agentExec and agentListTools MCP tool calls.
 *
 * Flow (agentExec):
 *   1. handleAgentExec() serialises the tool call to JSON and calls
 *      ClientManager::sendAgentCommand(connectionId, payload).
 *   2. ClientManager sends the payload to Chromium over NativeMessaging,
 *      which tunnels it through the WebRTC data channel to the host.
 *   3. The host-side quickdesk-agent processes the call and replies.
 *   4. The reply travels back through WebRTC → NativeMessaging →
 *      ClientManager::agentBridgeResponseReceived signal.
 *   5. onAgentResponse() is called; if a matching QEventLoop is waiting
 *      it is woken up and the result is returned to the caller.
 */
class AgentHandler : public QObject {
    Q_OBJECT

public:
    explicit AgentHandler(MainController* controller, QObject* parent = nullptr);

    // Called by ApiHandler to dispatch an agentExec request.
    // Blocks (via QEventLoop) until the remote agent replies or times out.
    QJsonObject handleAgentExec(const QJsonObject& params);

    // Called by ApiHandler to list tools available on the remote agent.
    QJsonObject handleAgentListTools(const QJsonObject& params);

    // Called by ClientManager when an agentBridgeResponse arrives from the
    // host (dispatched via MainController).
    void onAgentResponse(const QString& connectionId,
                         const QJsonObject& response);

private:
    // Send a JSON payload to the host agent and wait for the response that
    // carries a matching "id" field.  Returns an error object on timeout.
    QJsonObject sendAndWait(const QString& connectionId,
                            const QJsonObject& payload,
                            int timeoutMs = 30000);

    // Generate a unique request ID.
    QString nextRequestId();

    MainController* m_controller;
    int             m_nextId = 1;

    struct PendingRequest {
        QEventLoop*  loop   = nullptr;
        QJsonObject  result;
    };
    QMap<QString, PendingRequest*> m_pending;  // requestId → pending
};

} // namespace quickdesk

#endif // QUICKDESK_API_AGENTHANDLER_H
