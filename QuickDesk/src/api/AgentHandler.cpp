// Copyright 2026 QuickDesk Authors
// Agent bridge handler implementation

#include "AgentHandler.h"

#include <QJsonDocument>
#include <QTimer>
#include <QUuid>

#include "../controller/MainController.h"
#include "../manager/ClientManager.h"
#include "infra/log/log.h"

namespace quickdesk {

AgentHandler::AgentHandler(MainController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
{
    // Forward agentBridgeResponseReceived from ClientManager to onAgentResponse.
    connect(m_controller->clientManager(),
            &ClientManager::agentBridgeResponseReceived,
            this,
            [this](const QString& connectionId, const QJsonObject& response) {
                onAgentResponse(connectionId, response);
            });
}

// ---- Public API ----

QJsonObject AgentHandler::handleAgentExec(const QJsonObject& params)
{
    QString connectionId = params["connection_id"].toString();
    QString tool         = params["tool"].toString();
    QJsonValue args      = params.value("args");

    if (connectionId.isEmpty() || tool.isEmpty()) {
        return {{"error", "agentExec: connection_id and tool are required"}};
    }

    QJsonObject payload;
    payload["id"]   = nextRequestId();
    payload["type"] = "toolCall";
    payload["tool"] = tool;
    payload["args"] = args.isUndefined() ? QJsonValue(QJsonObject{}) : args;

    return sendAndWait(connectionId, payload);
}

QJsonObject AgentHandler::handleAgentListTools(const QJsonObject& params)
{
    QString connectionId = params["connection_id"].toString();

    if (connectionId.isEmpty()) {
        return {{"error", "agentListTools: connection_id is required"}};
    }

    QJsonObject payload;
    payload["id"]   = nextRequestId();
    payload["type"] = "listTools";

    return sendAndWait(connectionId, payload);
}

// ---- Callback from ClientManager ----

void AgentHandler::onAgentResponse(const QString& /*connectionId*/,
                                   const QJsonObject& response)
{
    QString id = response["id"].toString();
    if (id.isEmpty()) {
        return;
    }

    auto it = m_pending.find(id);
    if (it == m_pending.end()) {
        return;  // No one waiting — ignore
    }

    PendingRequest* req = it.value();
    req->result = response;
    if (req->loop && req->loop->isRunning()) {
        req->loop->quit();
    }
}

// ---- Private helpers ----

QJsonObject AgentHandler::sendAndWait(const QString& connectionId,
                                      const QJsonObject& payload,
                                      int timeoutMs)
{
    QString id = payload["id"].toString();

    // Serialise payload to string for the channel
    QByteArray bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QString jsonData = QString::fromUtf8(bytes);

    // Register pending request
    QEventLoop loop;
    PendingRequest req;
    req.loop = &loop;
    m_pending.insert(id, &req);

    // Set up timeout
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    // Send to host
    m_controller->clientManager()->sendAgentCommand(connectionId, jsonData);

    // Block until response or timeout
    loop.exec();

    m_pending.remove(id);

    if (!timer.isActive()) {
        // Timer fired — timed out
        return {{"error", QString("agentExec timed out after %1 ms").arg(timeoutMs)}};
    }

    timer.stop();
    return req.result;
}

QString AgentHandler::nextRequestId()
{
    return QString("agent-%1").arg(m_nextId++);
}

} // namespace quickdesk
