// Copyright 2026 QuickDesk Authors
// Agent bridge handler implementation

#include "AgentHandler.h"

#include <QJsonDocument>
#include <QMutexLocker>

#include "../controller/MainController.h"
#include "../manager/ClientManager.h"
#include "infra/log/log.h"

namespace quickdesk {

AgentHandler::AgentHandler(MainController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
{
    connect(m_controller->clientManager(),
            &ClientManager::agentBridgeResponseReceived,
            this,
            [this](const QString& connectionId, const QJsonObject& response) {
                onAgentResponse(connectionId, response);
            });
}

// ---- Public API (called from worker threads) ----

QJsonObject AgentHandler::handleAgentExec(const QJsonObject& params)
{
    QString connectionId = params["connection_id"].toString();
    QString tool         = params["tool"].toString();
    QJsonValue args      = params.value("args");

    if (connectionId.isEmpty() || tool.isEmpty()) {
        return {{"error", "agentExec: connection_id and tool are required"}};
    }

    if (!isConnectionValid(connectionId)) {
        return {{"error", QString("agentExec: connection '%1' not found").arg(connectionId)}};
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

    if (!isConnectionValid(connectionId)) {
        return {{"error", QString("agentListTools: connection '%1' not found").arg(connectionId)}};
    }

    QJsonObject payload;
    payload["id"]   = nextRequestId();
    payload["type"] = "listTools";

    return sendAndWait(connectionId, payload);
}

// ---- Callback from ClientManager (main thread) ----

void AgentHandler::onAgentResponse(const QString& /*connectionId*/,
                                   const QJsonObject& response)
{
    QString id = response["id"].toString();
    if (id.isEmpty()) return;

    QMutexLocker pendingLock(&m_pendingMutex);
    auto it = m_pending.find(id);
    if (it == m_pending.end()) return;

    PendingRequest& req = it.value();
    QMutexLocker resultLock(req.mutex);
    *req.result = response;
    *req.done   = true;
    req.cond->wakeOne();
}

// ---- Private helpers ----

bool AgentHandler::isConnectionValid(const QString& connectionId) const
{
    auto* cm = m_controller->clientManager();
    return cm && cm->connectionIds().contains(connectionId);
}

QJsonObject AgentHandler::sendAndWait(const QString& connectionId,
                                      const QJsonObject& payload,
                                      int timeoutMs)
{
    QString id = payload["id"].toString();
    QByteArray bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QString jsonData = QString::fromUtf8(bytes);

    QMutex mutex;
    QWaitCondition cond;
    QJsonObject result;
    bool done = false;

    {
        QMutexLocker locker(&m_pendingMutex);
        m_pending.insert(id, PendingRequest{&mutex, &cond, &result, &done});
    }

    // NativeMessaging is not thread-safe — send from the main thread.
    auto* cm = m_controller->clientManager();
    QMetaObject::invokeMethod(cm, [cm, connectionId, jsonData]() {
        cm->sendAgentCommand(connectionId, jsonData);
    }, Qt::QueuedConnection);

    // Block the *worker thread* (not the main thread) until response or timeout.
    {
        QMutexLocker locker(&mutex);
        if (!done) {
            cond.wait(&mutex, timeoutMs);
        }
    }

    {
        QMutexLocker locker(&m_pendingMutex);
        m_pending.remove(id);
    }

    if (!done) {
        return {{"error", QString("agentExec timed out after %1 ms").arg(timeoutMs)}};
    }
    return result;
}

QString AgentHandler::nextRequestId()
{
    return QString("agent-%1").arg(m_nextId.fetchAndAddRelaxed(1));
}

} // namespace quickdesk
