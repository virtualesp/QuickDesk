// Copyright 2026 QuickDesk Authors
// ConnectionListModel - QAbstractListModel for managing remote connections
// Provides incremental updates (beginInsertRows/endInsertRows, beginRemoveRows/endRemoveRows)
// so that Repeater/ListView/GridView only create/destroy affected delegates.

#pragma once

#include <QAbstractListModel>
#include <QVector>

namespace quickdesk {

struct ConnectionEntry {
    QString deviceId;
    QString name;
    QString state;
};

class ConnectionListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        DeviceIdRole = Qt::UserRole + 1,
        NameRole,
        StateRole,
    };
    Q_ENUM(Roles)

    explicit ConnectionListModel(QObject* parent = nullptr);
    ~ConnectionListModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_connections.size(); }

    Q_INVOKABLE int addConnection(const QString& deviceId);
    Q_INVOKABLE void removeConnection(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void updateState(const QString& deviceId, const QString& state);

    Q_INVOKABLE int indexOf(const QString& deviceId) const;
    Q_INVOKABLE QString deviceIdAt(int index) const;
    Q_INVOKABLE QString stateAt(int index) const;

signals:
    void countChanged();

private:
    QVector<ConnectionEntry> m_connections;
};

} // namespace quickdesk
