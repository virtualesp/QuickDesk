// Copyright 2026 QuickDesk Authors
// AgentManager implementation

#include "AgentManager.h"
#include "HostManager.h"
#include "infra/log/log.h"

#include <QJsonDocument>
#include <QProcess>

namespace quickdesk {

AgentManager::AgentManager(QObject* parent)
    : QObject(parent)
{}

AgentManager::~AgentManager()
{
    stopAgent();
}

void AgentManager::setHostManager(HostManager* hostManager)
{
    m_hostManager = hostManager;

    // Wire: client → agent
    connect(m_hostManager, &HostManager::agentMessage,
            this, &AgentManager::onMessageFromClient);
}

void AgentManager::startAgent(const QString& agentPath, const QString& skillsDir)
{
    if (m_agentProcess && m_agentProcess->state() != QProcess::NotRunning) {
        LOG_WARN("AgentManager: agent already running");
        return;
    }

    delete m_agentProcess;
    m_agentProcess = new QProcess(this);
    m_agentProcess->setProgram(agentPath);
    m_agentProcess->setArguments({"--skills-dir", skillsDir});

    connect(m_agentProcess, &QProcess::readyReadStandardOutput,
            this, &AgentManager::onAgentStdout);
    connect(m_agentProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AgentManager::onAgentFinished);

    m_agentProcess->start();
    if (!m_agentProcess->waitForStarted(3000)) {
        LOG_ERROR("AgentManager: failed to start agent at {}", agentPath.toStdString());
        return;
    }

    LOG_INFO("AgentManager: agent started (pid={})", m_agentProcess->processId());
}

void AgentManager::stopAgent()
{
    if (!m_agentProcess) return;

    if (m_agentProcess->state() != QProcess::NotRunning) {
        m_agentProcess->terminate();
        if (!m_agentProcess->waitForFinished(3000)) {
            m_agentProcess->kill();
        }
    }

    delete m_agentProcess;
    m_agentProcess = nullptr;
    m_readBuffer.clear();
}

bool AgentManager::isRunning() const
{
    return m_agentProcess && m_agentProcess->state() == QProcess::Running;
}

// ---- Private slots ----

void AgentManager::onAgentStdout()
{
    if (!m_agentProcess) return;

    m_readBuffer += m_agentProcess->readAllStandardOutput();

    // Process complete JSON lines (delimited by '\n')
    while (true) {
        int newlinePos = m_readBuffer.indexOf('\n');
        if (newlinePos < 0) break;

        QByteArray line = m_readBuffer.left(newlinePos).trimmed();
        m_readBuffer.remove(0, newlinePos + 1);

        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN("AgentManager: invalid JSON from agent: {}",
                     line.toStdString());
            continue;
        }

        handleAgentMessage(doc.object());
    }
}

void AgentManager::onAgentFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    LOG_INFO("AgentManager: agent exited with code {}", exitCode);
}

void AgentManager::onMessageFromClient(const QString& jsonData)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN("AgentManager: invalid JSON from client: {}",
                 jsonData.toStdString());
        return;
    }

    sendToAgent(doc.object());
}

// ---- Private helpers ----

void AgentManager::sendToAgent(const QJsonObject& message)
{
    if (!m_agentProcess || m_agentProcess->state() != QProcess::Running) {
        LOG_WARN("AgentManager: agent not running, dropping message");
        return;
    }

    QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact);
    line += '\n';
    m_agentProcess->write(line);
}

void AgentManager::handleAgentMessage(const QJsonObject& message)
{
    QString type = message["type"].toString();

    if (type == "capabilitiesChanged") {
        QJsonArray tools = message["tools"].toArray();
        LOG_INFO("AgentManager: capabilities updated, {} tool(s)", tools.size());
        emit capabilitiesChanged(tools);
        return;
    }

    // All other messages (toolResult, error, etc.) are forwarded to the client.
    if (!m_hostManager) return;

    QByteArray bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QString jsonData = QString::fromUtf8(bytes);

    emit agentResponseReady(message);
    m_hostManager->sendAgentBridgeSend(jsonData);
}

} // namespace quickdesk
