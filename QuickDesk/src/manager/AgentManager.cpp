// Copyright 2026 QuickDesk Authors
// AgentManager implementation

#include "AgentManager.h"
#include "HostManager.h"
#include "infra/log/log.h"

#include <QJsonDocument>
#include <QProcess>
#include <QTimer>

namespace quickdesk {

AgentManager::AgentManager(QObject* parent)
    : QObject(parent)
{}

AgentManager::~AgentManager()
{
    if (m_agentProcess) {
        if (m_agentProcess->state() != QProcess::NotRunning) {
            m_agentProcess->kill();
            m_agentProcess->waitForFinished(1000);
        }
        delete m_agentProcess;
        m_agentProcess = nullptr;
    }
}

void AgentManager::setHostManager(HostManager* hostManager)
{
    m_hostManager = hostManager;

    // Wire: client → agent
    connect(m_hostManager, &HostManager::agentMessage,
            this, &AgentManager::onMessageFromClient);

    // When a new client connects, push cached agent capabilities
    connect(m_hostManager, &HostManager::clientConnected,
            this, [this](const QString& /*connectionId*/, const QJsonObject& /*info*/) {
        if (m_cachedTools.isEmpty()) return;

        QJsonObject msg;
        msg["type"] = QStringLiteral("capabilitiesReady");
        msg["tools"] = m_cachedTools;

        QByteArray bytes = QJsonDocument(msg).toJson(QJsonDocument::Compact);
        QString jsonData = QString::fromUtf8(bytes);
        m_hostManager->sendAgentBridgeSend(jsonData);

        LOG_INFO("AgentManager: pushed cached capabilities ({} tools) to new client",
                 m_cachedTools.size());
    });
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
    connect(m_agentProcess, &QProcess::started, this, [this]() {
        LOG_INFO("AgentManager: agent started (pid={})", m_agentProcess->processId());
    });
    connect(m_agentProcess, &QProcess::errorOccurred, this, [this, agentPath](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart) {
            LOG_ERROR("AgentManager: failed to start agent at {}", agentPath.toStdString());
        }
    });

    m_agentProcess->start();
}

void AgentManager::stopAgent()
{
    if (!m_agentProcess) return;

    if (m_agentProcess->state() == QProcess::NotRunning) {
        delete m_agentProcess;
        m_agentProcess = nullptr;
        m_readBuffer.clear();
        m_cachedTools = QJsonArray();
        return;
    }

    QProcess* proc = m_agentProcess;
    m_agentProcess = nullptr;
    m_readBuffer.clear();
    m_cachedTools = QJsonArray();

    proc->terminate();
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            proc, &QObject::deleteLater);

    QTimer::singleShot(3000, proc, [proc]() {
        if (proc->state() == QProcess::NotRunning) return;
        LOG_WARN("AgentManager: agent did not terminate, killing...");
        proc->kill();
    });
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

    if (type == "capabilitiesReady" || type == "capabilitiesChanged") {
        QJsonArray tools = message["tools"].toArray();
        m_cachedTools = tools;
        LOG_INFO("AgentManager: capabilities {}, {} tool(s)",
                 type.toStdString(), tools.size());
        emit capabilitiesChanged(tools);
        // Fall through to forward to connected clients
    }

    // Forward all messages (toolResult, capabilitiesReady, error, etc.) to the client.
    if (!m_hostManager) return;

    QByteArray bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QString jsonData = QString::fromUtf8(bytes);

    emit agentResponseReady(message);
    m_hostManager->sendAgentBridgeSend(jsonData);
}

} // namespace quickdesk
