// Copyright 2026 QuickDesk Authors

#include "connectionlistmodel.h"

#include <QDebug>

namespace quickdesk {

ConnectionListModel::ConnectionListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ConnectionListModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_connections.size();
}

QVariant ConnectionListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_connections.size())
        return {};

    const auto& conn = m_connections[index.row()];
    switch (role) {
    case DeviceIdRole:
        return conn.deviceId;
    case NameRole:
        return conn.name;
    case StateRole:
        return conn.state;
    default:
        return {};
    }
}

QHash<int, QByteArray> ConnectionListModel::roleNames() const
{
    return {
        {DeviceIdRole, "deviceId"},
        {NameRole, "name"},
        {StateRole, "state"},
    };
}

int ConnectionListModel::addConnection(const QString& deviceId)
{
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].deviceId == deviceId) {
            qDebug() << "ConnectionListModel: device already exists:" << deviceId;
            return i;
        }
    }

    int row = m_connections.size();
    beginInsertRows(QModelIndex(), row, row);
    m_connections.append({deviceId, deviceId, QStringLiteral("connecting")});
    endInsertRows();

    emit countChanged();
    qDebug() << "ConnectionListModel: added device:" << deviceId << "total:" << m_connections.size();
    return row;
}

void ConnectionListModel::removeConnection(int index)
{
    if (index < 0 || index >= m_connections.size())
        return;

    auto devId = m_connections[index].deviceId;

    beginRemoveRows(QModelIndex(), index, index);
    m_connections.removeAt(index);
    endRemoveRows();

    emit countChanged();
    qDebug() << "ConnectionListModel: removed device:" << devId << "remaining:" << m_connections.size();
}

void ConnectionListModel::clear()
{
    if (m_connections.isEmpty())
        return;

    beginResetModel();
    m_connections.clear();
    endResetModel();

    emit countChanged();
}

void ConnectionListModel::updateState(const QString& deviceId, const QString& state)
{
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].deviceId == deviceId) {
            if (m_connections[i].state != state) {
                m_connections[i].state = state;
                QModelIndex modelIndex = index(i);
                emit dataChanged(modelIndex, modelIndex, {StateRole});
                qDebug() << "ConnectionListModel: updated state:" << deviceId << "->" << state;
            }
            return;
        }
    }
}

int ConnectionListModel::indexOf(const QString& deviceId) const
{
    for (int i = 0; i < m_connections.size(); ++i) {
        if (m_connections[i].deviceId == deviceId)
            return i;
    }
    return -1;
}

QString ConnectionListModel::deviceIdAt(int index) const
{
    if (index < 0 || index >= m_connections.size())
        return {};
    return m_connections[index].deviceId;
}

QString ConnectionListModel::stateAt(int index) const
{
    if (index < 0 || index >= m_connections.size())
        return {};
    return m_connections[index].state;
}

} // namespace quickdesk
