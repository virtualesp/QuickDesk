// Copyright 2026 QuickDesk Authors
// WebSocket API server for external tool integration (MCP bridge, etc.)

#ifndef QUICKDESK_API_WEBSOCKETSERVER_H
#define QUICKDESK_API_WEBSOCKETSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
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

    void broadcastEvent(const QString& event, const QJsonObject& data);

private slots:
    void onNewConnection();
    void onTextMessageReceived(const QString& message);
    void onClientDisconnected();

private:
    bool authenticateClient(QWebSocket* client, const QJsonObject& msg);

    QWebSocketServer* m_server = nullptr;
    ApiHandler* m_handler = nullptr;
    QSet<QWebSocket*> m_clients;
    QSet<QWebSocket*> m_authenticatedClients;
    QString m_authToken;
};

} // namespace quickdesk

#endif // QUICKDESK_API_WEBSOCKETSERVER_H
