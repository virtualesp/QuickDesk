// Copyright 2026 QuickDesk Authors

#include "TurnServerManager.h"
#include "infra/log/log.h"
#include "../core/localconfigcenter.h"
#include <QJsonDocument>
#include <QUrl>

namespace quickdesk {

TurnServerManager::TurnServerManager(QObject* parent)
    : QObject(parent)
{
    loadSettings();
}

QJsonArray TurnServerManager::servers() const
{
    return m_servers;
}

void TurnServerManager::setServers(const QJsonArray& servers)
{
    if (m_servers != servers) {
        m_servers = servers;
        saveSettings();
        emit serversChanged();
        LOG_INFO("TURN servers updated, count: {}", m_servers.size());
    }
}

QJsonArray TurnServerManager::getEffectiveServers() const
{
    // If user has configured TURN server(s), use their configuration only
    if (hasTurnServer(m_servers)) {
        LOG_INFO("Using user-configured TURN servers: {} server(s)", m_servers.size());
        return m_servers;
    }
    
    // Otherwise, add built-in TURN server
    QJsonArray effectiveServers = m_servers;
    effectiveServers.append(createBuiltinTurnServer());
    
    LOG_INFO("No user TURN server, using built-in TURN + {} user STUN server(s)", 
             m_servers.size());
    return effectiveServers;
}

bool TurnServerManager::addTurnServer(const QString& url,
                                       const QString& username,
                                       const QString& credential,
                                       int maxRateKbps)
{
    if (!validateServerUrl(url)) {
        LOG_WARN("Invalid TURN server URL: {}", url.toStdString());
        return false;
    }
    
    QJsonObject server;
    server["urls"] = QJsonArray{url};
    server["username"] = username;
    server["credential"] = credential;
    if (maxRateKbps > 0) {
        server["maxRateKbps"] = maxRateKbps;
    }
    
    QJsonArray newServers = m_servers;
    newServers.append(server);
    setServers(newServers);
    
    LOG_INFO("Added TURN server: {}", url.toStdString());
    return true;
}

bool TurnServerManager::addStunServer(const QString& url)
{
    if (!validateServerUrl(url)) {
        LOG_WARN("Invalid STUN server URL: {}", url.toStdString());
        return false;
    }
    
    QJsonObject server;
    server["urls"] = QJsonArray{url};
    
    QJsonArray newServers = m_servers;
    newServers.append(server);
    setServers(newServers);
    
    LOG_INFO("Added STUN server: {}", url.toStdString());
    return true;
}

void TurnServerManager::removeServer(int index)
{
    if (index >= 0 && index < m_servers.size()) {
        QJsonArray newServers = m_servers;
        
        // Get URL for logging
        auto serverObj = newServers[index].toObject();
        QString url = serverObj.value("urls").toArray().first().toString();
        
        newServers.removeAt(index);
        setServers(newServers);
        
        LOG_INFO("Removed server at index {}: {}", index, url.toStdString());
    }
}

void TurnServerManager::clearServers()
{
    if (!m_servers.isEmpty()) {
        setServers(QJsonArray());
        LOG_INFO("Cleared all user-configured servers");
    }
}

bool TurnServerManager::validateServerUrl(const QString& url)
{
    if (url.isEmpty()) {
        return false;
    }
    
    // Check protocol
    if (!url.startsWith("stun:", Qt::CaseInsensitive) &&
        !url.startsWith("turn:", Qt::CaseInsensitive) &&
        !url.startsWith("turns:", Qt::CaseInsensitive)) {
        return false;
    }
    
    // Basic URL validation
    QUrl qurl(url);
    if (!qurl.isValid()) {
        return false;
    }
    
    return true;
}

bool TurnServerManager::hasTurnServer() const
{
    return hasTurnServer(m_servers);
}

bool TurnServerManager::hasTurnServer(const QJsonArray& servers) const
{
    for (const auto& serverValue : servers) {
        auto serverObj = serverValue.toObject();
        auto urls = serverObj.value("urls").toArray();
        
        for (const auto& urlValue : urls) {
            QString url = urlValue.toString();
            if (url.startsWith("turn:", Qt::CaseInsensitive) ||
                url.startsWith("turns:", Qt::CaseInsensitive)) {
                return true;
            }
        }
    }
    return false;
}

void TurnServerManager::loadSettings()
{
    // Load from database using LocalConfigCenter
    QString jsonStr = core::LocalConfigCenter::instance().turnServersJson("");
    
    if (jsonStr.isEmpty()) {
        m_servers = QJsonArray();
        LOG_INFO("No TURN/STUN servers configured, using defaults");
        return;
    }
    
    // Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (doc.isArray()) {
        m_servers = doc.array();
        LOG_INFO("Loaded {} TURN/STUN server(s) from database", m_servers.size());
    } else {
        m_servers = QJsonArray();
        LOG_WARN("Failed to parse TURN servers JSON, using empty array");
    }
}

void TurnServerManager::saveSettings()
{
    // Save to database using LocalConfigCenter
    QJsonDocument doc(m_servers);
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    
    core::LocalConfigCenter::instance().setTurnServersJson(jsonStr);
    
    LOG_INFO("Saved {} TURN/STUN server(s) to database", m_servers.size());
}

QJsonObject TurnServerManager::createBuiltinTurnServer() const
{
    QJsonObject server;
    server["urls"] = QJsonArray{QString(BUILTIN_TURN_URL)};
    server["username"] = QString(BUILTIN_TURN_USERNAME);
    server["credential"] = QString(BUILTIN_TURN_CREDENTIAL);
    server["maxRateKbps"] = 8000;
    return server;
}

} // namespace quickdesk
