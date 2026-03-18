// Copyright 2026 QuickDesk Authors
// AgentManager — manages the quickdesk-agent subprocess on the host machine.
//
// Communication path:
//   Qt AgentManager ←→ quickdesk-agent (stdin/stdout JSON Lines)
//   Qt AgentManager ←→ HostManager ←→ Chromium ←→ WebRTC ←→ Client

#ifndef QUICKDESK_MANAGER_AGENTMANAGER_H
#define QUICKDESK_MANAGER_AGENTMANAGER_H

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>

namespace quickdesk {

class HostManager;

/**
 * @brief Manages the quickdesk-agent subprocess and bridges messages between
 *        the agent and the remote client (via HostManager → Chromium → WebRTC).
 *
 * Protocol (agent ↔ AgentManager, JSON Lines on stdin/stdout):
 *   Client → Host → AgentManager → agent stdin:
 *     {"id":"req-1","type":"toolCall","tool":"run_shell","args":{"cmd":"..."}}
 *   agent stdout → AgentManager → Host → Client:
 *     {"id":"req-1","type":"toolResult","result":"..."}
 *   agent stdout → AgentManager (capability report):
 *     {"type":"capabilitiesChanged","tools":[...]}
 */
class AgentManager : public QObject {
    Q_OBJECT

public:
    explicit AgentManager(QObject* parent = nullptr);
    ~AgentManager() override;

    // Wire up to the HostManager (must be called before startAgent).
    void setHostManager(HostManager* hostManager);

    // Start the agent subprocess.  agentPath is the path to the quickdesk-agent
    // binary; skillsDir is the directory containing skill SKILL.md files.
    void startAgent(const QString& agentPath, const QString& skillsDir);

    // Stop the agent subprocess gracefully.
    void stopAgent();

    bool isRunning() const;

signals:
    // Emitted when the agent reports its available tools (on start or hot-reload).
    void capabilitiesChanged(const QJsonArray& tools);

    // Emitted when the agent sends a response back to the client.
    // AgentManager automatically forwards it via HostManager::sendAgentBridgeSend.
    void agentResponseReady(const QJsonObject& response);

private slots:
    // Called when the agent process writes to stdout.
    void onAgentStdout();

    // Called when the agent process exits.
    void onAgentFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // Called when HostManager receives an agentMessage from the remote client.
    void onMessageFromClient(const QString& jsonData);

private:
    // Send a JSON object to the agent's stdin as a single JSON Line.
    void sendToAgent(const QJsonObject& message);

    // Handle a complete JSON object received from the agent.
    void handleAgentMessage(const QJsonObject& message);

    QPointer<HostManager> m_hostManager;
    QProcess*             m_agentProcess = nullptr;
    QByteArray            m_readBuffer;  // accumulates partial lines from stdout
};

} // namespace quickdesk

#endif // QUICKDESK_MANAGER_AGENTMANAGER_H
