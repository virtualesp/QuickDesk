// Copyright 2026 QuickDesk Authors
// Client process communication manager

#ifndef QUICKDESK_MANAGER_CLIENTMANAGER_H
#define QUICKDESK_MANAGER_CLIENTMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QStringList>

namespace quickdesk {

class NativeMessaging;

/**
 * @brief Connection information for remote hosts
 */
struct ConnectionInfo {
    QString connectionId;
    QString deviceId;
    QString deviceName;
    QString state;  // connecting, connected, disconnected, failed
    QString connectedAt;
    int width = 0;
    int height = 0;
};

/**
 * @brief Manages communication with the Client process
 */
class ClientManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(int connectionCount READ connectionCount NOTIFY connectionCountChanged)
    Q_PROPERTY(QString activeConnectionId READ activeConnectionId 
               WRITE setActiveConnectionId NOTIFY activeConnectionChanged)
    Q_PROPERTY(QStringList connectionIds READ connectionIds NOTIFY connectionListChanged)

public:
    explicit ClientManager(QObject* parent = nullptr);
    ~ClientManager() override = default;

    // Set the Native Messaging handler
    void setMessaging(NativeMessaging* messaging);

    // Connection management
    Q_INVOKABLE QString connectToHost(const QString& deviceId,
                                      const QString& accessCode,
                                      const QString& serverUrl);
    Q_INVOKABLE void disconnectFromHost(const QString& connectionId);
    Q_INVOKABLE void disconnectAll();
    Q_INVOKABLE void sendHello();

    // Input events
    Q_INVOKABLE void sendMouseMove(const QString& connectionId, int x, int y);
    Q_INVOKABLE void sendMousePress(const QString& connectionId, int x, int y, int button);
    Q_INVOKABLE void sendMouseRelease(const QString& connectionId, int x, int y, int button);
    Q_INVOKABLE void sendMouseWheel(const QString& connectionId, int x, int y, int delta);
    Q_INVOKABLE void sendKeyPress(const QString& connectionId, int keyCode, int modifiers);
    Q_INVOKABLE void sendKeyRelease(const QString& connectionId, int keyCode, int modifiers);

    // Clipboard
    Q_INVOKABLE void syncClipboard(const QString& connectionId, const QString& text);

    // State getters
    int connectionCount() const;
    QString activeConnectionId() const;
    void setActiveConnectionId(const QString& id);
    QList<ConnectionInfo> connections() const;
    ConnectionInfo getConnection(const QString& connectionId) const;
    QStringList connectionIds() const;
    Q_INVOKABLE QString getConnectionState(const QString& connectionId) const;

signals:
    void connectionCountChanged();
    void activeConnectionChanged();
    
    void helloResponseReceived(const QString& version);
    void connectionStateChanged(const QString& connectionId, 
                                const QString& state,
                                const QJsonObject& hostInfo);
    void connectionAdded(const QString& connectionId);
    void connectionRemoved(const QString& connectionId);
    void connectionListChanged();
    void videoFrameReady(const QString& connectionId, int frameIndex);
    void clipboardReceived(const QString& connectionId, const QString& text);
    void errorOccurred(const QString& connectionId, 
                       const QString& code, 
                       const QString& message);

private slots:
    void onMessageReceived(const QJsonObject& message);
    void onMessagingError(const QString& error);

private:
    NativeMessaging* m_messaging = nullptr;
    QMap<QString, ConnectionInfo> m_connections;
    QString m_activeConnectionId;
    int m_connectionCounter = 0;

    QString generateConnectionId();
    void handleHelloResponse(const QJsonObject& message);
    void handleConnectionStateChanged(const QJsonObject& message);
    void handleConnectionListChanged(const QJsonObject& message);
    void handleVideoFrameReady(const QJsonObject& message);
    void handleClipboardReceived(const QJsonObject& message);
    void handleError(const QJsonObject& message);
    void handleConnectionFailed(const QJsonObject& message);
    void handleHostConnected(const QJsonObject& message);
    void handleHostDisconnected(const QJsonObject& message);
    void handleHostConnectionFailed(const QJsonObject& message);
    
    void sendMouseEvent(const QString& connectionId, const QString& eventType,
                        int x, int y, int button, int wheelDelta);
    void sendKeyboardEvent(const QString& connectionId, const QString& eventType,
                           int keyCode, int modifiers);
};

} // namespace quickdesk

#endif // QUICKDESK_MANAGER_CLIENTMANAGER_H
