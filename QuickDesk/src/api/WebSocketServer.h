// Copyright 2026 QuickDesk Authors
// WebSocket API server for external tool integration (MCP bridge, etc.)

#ifndef QUICKDESK_API_WEBSOCKETSERVER_H
#define QUICKDESK_API_WEBSOCKETSERVER_H

#include "SecurityManager.h"

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QMap>
#include <QSet>

namespace quickdesk {

class ApiHandler;
class MainController;

class WebSocketApiServer : public QObject {
    Q_OBJECT

public:
    explicit WebSocketApiServer(MainController* controller,
                                QObject* parent = nullptr);
    ~WebSocketApiServer() override;

    bool start(quint16 port = 9800,
               const QHostAddress& address = QHostAddress::LocalHost);
    void stop();
    bool isListening() const;
    quint16 port() const;

    void setAuthToken(const QString& token);

    SecurityManager* security() const { return m_security; }

    void broadcastEvent(const QString& event, const QJsonObject& data);

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onClientDisconnected();
    void onSessionExpired(const QString& clientId);

private:
    struct ClientInfo {
        SecurityManager::PermissionLevel permission =
            SecurityManager::FullControl;
        QString id;
    };

    bool authenticateClient(QWebSocket* client, const QJsonObject& msg);
    QString clientId(QWebSocket* client) const;

    QWebSocketServer* m_server = nullptr;
    ApiHandler* m_handler = nullptr;
    SecurityManager* m_security = nullptr;
    QSet<QWebSocket*> m_clients;
    QSet<QWebSocket*> m_authenticatedClients;
    QMap<QWebSocket*, ClientInfo> m_clientInfo;
    QMap<QString, QWebSocket*> m_clientIdMap;
    QString m_authToken;
    int m_nextClientId = 1;
};

} // namespace quickdesk

#endif // QUICKDESK_API_WEBSOCKETSERVER_H
