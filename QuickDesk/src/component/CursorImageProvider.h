// Copyright 2026 QuickDesk Authors
// Cursor image provider for QML

#ifndef QUICKDESK_COMPONENT_CURSORIMAGEPROVIDER_H
#define QUICKDESK_COMPONENT_CURSORIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QMap>
#include <QMutex>

namespace quickdesk {

/**
 * @brief Image provider for remote cursor images
 * 
 * Provides cursor images to QML via the "image://cursor/deviceId/version" URL scheme.
 * The version is appended to force QML to reload the image when cursor changes.
 */
class CursorImageProvider : public QQuickImageProvider {
public:
    CursorImageProvider();
    ~CursorImageProvider() override = default;

    QImage requestImage(const QString& id, QSize* size, 
                        const QSize& requestedSize) override;

    void setCursor(const QString& deviceId, const QImage& image, 
                   const QPoint& hotspot);
    
    void clearCursor(const QString& deviceId);
    
    // Get singleton instance
    static CursorImageProvider* instance();

private:
    struct CursorData {
        QImage image;
        QPoint hotspot;
    };
    
    QMap<QString, CursorData> m_cursors;
    mutable QMutex m_mutex;
    
    static CursorImageProvider* s_instance;
};

} // namespace quickdesk

#endif // QUICKDESK_COMPONENT_CURSORIMAGEPROVIDER_H
