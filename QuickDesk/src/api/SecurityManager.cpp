// Copyright 2026 QuickDesk Authors

#include "SecurityManager.h"
#include "infra/log/log.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>

namespace quickdesk {

SecurityManager::SecurityManager(QObject* parent)
    : QObject(parent) {
    m_readOnlyMethods = {
        "auth",
        "getHostInfo",
        "getHostClients",
        "getStatus",
        "getSignalingStatus",
        "listConnections",
        "getConnectionInfo",
        "screenshot",
        "getScreenSize",
        "getClipboard",
    };

    connect(&m_sessionCheckTimer, &QTimer::timeout,
            this, &SecurityManager::checkSessionTimeouts);
}

SecurityManager::~SecurityManager() {
    m_sessionCheckTimer.stop();
}

// --- Authentication & Authorization ---

void SecurityManager::setFullAccessToken(const QString& token) {
    m_fullAccessToken = token;
}

void SecurityManager::setReadOnlyToken(const QString& token) {
    m_readOnlyToken = token;
}

SecurityManager::PermissionLevel
SecurityManager::authenticateToken(const QString& token) const {
    if (!m_fullAccessToken.isEmpty() && token == m_fullAccessToken) {
        return FullControl;
    }
    if (!m_readOnlyToken.isEmpty() && token == m_readOnlyToken) {
        return ReadOnly;
    }
    return NoAccess;
}

void SecurityManager::setAllowedDevices(const QStringList& deviceIds) {
    m_allowedDevices.clear();
    for (const auto& id : deviceIds) {
        m_allowedDevices.insert(id);
    }
    m_deviceFilterEnabled = !m_allowedDevices.isEmpty();
}

bool SecurityManager::isDeviceAllowed(const QString& deviceId) const {
    if (!m_deviceFilterEnabled) {
        return true;
    }
    return m_allowedDevices.contains(deviceId);
}

bool SecurityManager::isMethodAllowedForPermission(
    const QString& method, PermissionLevel level) const {
    if (level == FullControl) {
        return true;
    }
    if (level == ReadOnly) {
        return m_readOnlyMethods.contains(method);
    }
    return false;
}

// --- Rate Limiting ---

void SecurityManager::setRateLimitPerMinute(int maxRequests) {
    m_rateLimitPerMinute = maxRequests;
}

bool SecurityManager::checkRateLimit(const QString& clientId) {
    if (m_rateLimitPerMinute <= 0) {
        return true;
    }

    auto now = QDateTime::currentMSecsSinceEpoch();
    auto cutoff = now - 60000;
    auto& state = m_rateLimiter[clientId];

    while (!state.requestTimestamps.isEmpty()
           && state.requestTimestamps.first() < cutoff) {
        state.requestTimestamps.removeFirst();
    }

    if (state.requestTimestamps.size() >= m_rateLimitPerMinute) {
        return false;
    }

    state.requestTimestamps.append(now);
    return true;
}

// --- Session Timeout ---

void SecurityManager::setSessionTimeoutSecs(int seconds) {
    m_sessionTimeoutSecs = seconds;
    if (seconds > 0) {
        m_sessionCheckTimer.start(std::min(seconds * 1000, 30000));
    } else {
        m_sessionCheckTimer.stop();
    }
}

void SecurityManager::recordActivity(const QString& clientId) {
    m_lastActivity[clientId] = QDateTime::currentMSecsSinceEpoch();
}

void SecurityManager::removeClient(const QString& clientId) {
    m_lastActivity.remove(clientId);
    m_rateLimiter.remove(clientId);
}

QStringList SecurityManager::expiredClients() const {
    if (m_sessionTimeoutSecs <= 0) {
        return {};
    }

    auto now = QDateTime::currentMSecsSinceEpoch();
    auto cutoff = now - static_cast<qint64>(m_sessionTimeoutSecs) * 1000;
    QStringList expired;

    for (auto it = m_lastActivity.constBegin();
         it != m_lastActivity.constEnd(); ++it) {
        if (it.value() < cutoff) {
            expired.append(it.key());
        }
    }
    return expired;
}

void SecurityManager::checkSessionTimeouts() {
    auto expired = expiredClients();
    for (const auto& clientId : expired) {
        LOG_INFO("Session timeout for client: {}", clientId.toStdString());
        emit sessionExpired(clientId);
    }
}

// --- Dangerous Operations ---

bool SecurityManager::isDangerousOperation(
    const QString& method, const QJsonObject& params) const {
    if (method == "disconnectAll") {
        return true;
    }

    if (method == "keyboardHotkey") {
        auto keys = params["keys"].toArray();
        QStringList keyList;
        for (const auto& k : keys) {
            keyList.append(k.toString().toLower());
        }
        if (keyList.contains("alt") && keyList.contains("f4")) {
            return true;
        }
        if (keyList.contains("ctrl") && keyList.contains("alt")
            && keyList.contains("delete")) {
            return true;
        }
    }

    if (method == "keyboardType") {
        auto text = params["text"].toString().toLower();
        if (text.contains("shutdown") || text.contains("reboot")
            || text.contains("format") || text.contains("rm -rf")
            || text.contains("del /f /s /q")
            || text.contains("mkfs")) {
            return true;
        }
    }

    return false;
}

// --- Audit Logging ---

void SecurityManager::initAuditLog(const QString& logDir) {
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto path = dir.filePath("quickdesk_audit.log").toStdString();
    try {
        m_auditLogger = spdlog::rotating_logger_mt(
            "audit", path, 10 * 1024 * 1024, 5);
        m_auditLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
        m_auditLogger->flush_on(spdlog::level::info);
        LOG_INFO("Audit log initialized at: {}", path);
    } catch (const spdlog::spdlog_ex& ex) {
        LOG_ERROR("Failed to init audit logger: {}", ex.what());
    }
}

void SecurityManager::logAudit(const QString& clientId,
                               const QString& method,
                               const QJsonObject& params,
                               bool allowed,
                               const QString& denialReason) {
    auto paramsStr = QString::fromUtf8(
        QJsonDocument(params).toJson(QJsonDocument::Compact));
    if (paramsStr.length() > 500) {
        paramsStr = paramsStr.left(497) + "...";
    }

    auto status = allowed ? QStringLiteral("ALLOW") : QStringLiteral("DENY");
    auto reasonPart = denialReason.isEmpty()
                          ? QString()
                          : QStringLiteral(" reason=%1").arg(denialReason);
    auto entry = QStringLiteral("[%1] %2 method=%3 params=%4%5")
                     .arg(status, clientId, method, paramsStr, reasonPart);

    if (m_auditLogger) {
        m_auditLogger->info(entry.toStdString());
    }

    if (!allowed) {
        LOG_WARN("Security: {}", entry.toStdString());
        if (denialReason.contains("rate_limit")) {
            emit rateLimitExceeded(clientId, method);
            emit anomalyDetected(clientId,
                                 QString("Rate limit exceeded on %1").arg(method));
        }
    }
}

} // namespace quickdesk
