// Copyright 2026 QuickDesk Authors
// TURN Server configuration manager

#ifndef QUICKDESK_MANAGER_TURNSERVERMANAGER_H
#define QUICKDESK_MANAGER_TURNSERVERMANAGER_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace quickdesk {

/**
 * @brief Manages TURN/STUN server configuration
 * 
 * Stores and loads ICE server configurations from QSettings.
 * Supports multiple TURN/STUN servers.
 */
class TurnServerManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QJsonArray servers READ servers NOTIFY serversChanged)

public:
    explicit TurnServerManager(QObject* parent = nullptr);
    ~TurnServerManager() override = default;

    /**
     * @brief Get all configured servers (user-defined only, without built-in)
     */
    QJsonArray servers() const;
    
    /**
     * @brief Set user-configured servers
     */
    void setServers(const QJsonArray& servers);
    
    /**
     * @brief Get effective ICE servers (user-defined + built-in if needed)
     * 
     * If no TURN server in user config, automatically adds built-in TURN server.
     * Built-in server is transparent to UI.
     */
    Q_INVOKABLE QJsonArray getEffectiveServers() const;
    
    /**
     * @brief Add a new TURN server
     */
    Q_INVOKABLE bool addTurnServer(const QString& url,
                                     const QString& username,
                                     const QString& credential,
                                     int maxRateKbps = 8000);
    
    /**
     * @brief Add a new STUN server
     */
    Q_INVOKABLE bool addStunServer(const QString& url);
    
    /**
     * @brief Remove server at index
     */
    Q_INVOKABLE void removeServer(int index);
    
    /**
     * @brief Clear all user-configured servers
     */
    Q_INVOKABLE void clearServers();
    
    /**
     * @brief Validate server URL format
     */
    Q_INVOKABLE static bool validateServerUrl(const QString& url);
    
    /**
     * @brief Check if there are any TURN servers in the configuration
     */
    Q_INVOKABLE bool hasTurnServer() const;
    Q_INVOKABLE bool hasTurnServer(const QJsonArray& servers) const;

    // Load/save settings
    void loadSettings();
    void saveSettings();

signals:
    void serversChanged();

private:
    QJsonArray m_servers;  // User-configured servers only
    
    // Built-in TURN server (transparent to user)
    static constexpr const char* BUILTIN_TURN_URL = "turn:115.190.196.189:3478";
    static constexpr const char* BUILTIN_TURN_USERNAME = "qfturn";
    static constexpr const char* BUILTIN_TURN_CREDENTIAL = "iunngalgag";
    
    QJsonObject createBuiltinTurnServer() const;
};

} // namespace quickdesk

#endif // QUICKDESK_MANAGER_TURNSERVERMANAGER_H
