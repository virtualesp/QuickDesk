#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>

namespace quickdesk {

class SystemTrayManager : public QObject {
    Q_OBJECT

public:
    explicit SystemTrayManager(QObject* parent = nullptr);
    ~SystemTrayManager();

    static SystemTrayManager& instance();

    Q_INVOKABLE void minimizeToTray();
    Q_INVOKABLE void quit();

Q_SIGNALS:
    void showWindowRequested();

private:
    void initTrayIcon();

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_showAction = nullptr;
    QAction* m_quitAction = nullptr;
};

}
