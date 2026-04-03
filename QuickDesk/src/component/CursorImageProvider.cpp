// Copyright 2026 QuickDesk Authors

#include "CursorImageProvider.h"

namespace quickdesk {

CursorImageProvider* CursorImageProvider::s_instance = nullptr;

CursorImageProvider::CursorImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
    s_instance = this;
}

CursorImageProvider* CursorImageProvider::instance()
{
    return s_instance;
}

QImage CursorImageProvider::requestImage(const QString& id, QSize* size, 
                                          const QSize& requestedSize)
{
    Q_UNUSED(requestedSize);
    
    // Parse id: "deviceId/version"
    QStringList parts = id.split('/');
    if (parts.isEmpty()) {
        return QImage();
    }
    
    QString deviceId = parts.first();
    
    QMutexLocker locker(&m_mutex);
    
    auto it = m_cursors.find(deviceId);
    if (it == m_cursors.end()) {
        return QImage();
    }
    
    const QImage& image = it.value().image;
    if (size) {
        *size = image.size();
    }
    
    return image;
}

void CursorImageProvider::setCursor(const QString& deviceId, 
                                    const QImage& image,
                                    const QPoint& hotspot)
{
    QMutexLocker locker(&m_mutex);
    
    CursorData data;
    data.image = image;
    data.hotspot = hotspot;
    m_cursors[deviceId] = data;
}

void CursorImageProvider::clearCursor(const QString& deviceId)
{
    QMutexLocker locker(&m_mutex);
    m_cursors.remove(deviceId);
}

} // namespace quickdesk
