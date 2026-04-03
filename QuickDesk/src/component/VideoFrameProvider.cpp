// Copyright 2026 QuickDesk Authors

#include "VideoFrameProvider.h"
#include "CursorImageProvider.h"
#include "infra/log/log.h"

#include <QDateTime>

namespace quickdesk {

VideoFrameProvider::VideoFrameProvider(QObject* parent)
    : QObject(parent)
{
}

VideoFrameProvider::~VideoFrameProvider()
{
}

void VideoFrameProvider::setVideoSink(QVideoSink* sink)
{
    if (m_videoSink == sink) {
        return;
    }

    m_videoSink = sink;
    emit videoSinkChanged();
    
    LOG_DEBUG("VideoFrameProvider: videoSink set for device {}", 
              m_deviceId.toStdString());
}

void VideoFrameProvider::setDeviceId(const QString& deviceId)
{
    if (m_deviceId == deviceId) {
        return;
    }

    m_deviceId = deviceId;
    emit deviceIdChanged();
    
    LOG_DEBUG("VideoFrameProvider: deviceId set to {}", 
              deviceId.toStdString());
}

void VideoFrameProvider::setSharedMemoryManager(SharedMemoryManager* manager)
{
    if (m_sharedMemoryManager == manager) {
        return;
    }

    m_sharedMemoryManager = manager;
    emit sharedMemoryManagerChanged();
}

void VideoFrameProvider::setActive(bool active)
{
    if (m_active == active) {
        return;
    }

    m_active = active;
    emit activeChanged();
    
    LOG_DEBUG("VideoFrameProvider: active={} for device {}", 
              active, m_deviceId.toStdString());
}

void VideoFrameProvider::onVideoFrameReady(quint32 frameIndex)
{
    Q_UNUSED(frameIndex)
    
    if (!m_active) {
        return;
    }
    
    pushFrame();
}

void VideoFrameProvider::pushFrame()
{
    if (!m_active || !m_videoSink || !m_sharedMemoryManager || 
        m_deviceId.isEmpty()) {
        return;
    }

    if (!m_sharedMemoryManager->isAttached(m_deviceId)) {
        return;
    }

    QVideoFrame frame = m_sharedMemoryManager->readVideoFrame(m_deviceId);
    
    if (!frame.isValid()) {
        return;
    }

    QSize newSize = frame.size();
    if (m_frameSize != newSize) {
        m_frameSize = newSize;
        emit frameSizeChanged();
    }

    m_videoSink->setVideoFrame(frame);

    emit frameReceived();
    
    updateFrameRate();
}

void VideoFrameProvider::updateFrameRate()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    if (m_frameRateStartTime == 0) {
        m_frameRateStartTime = now;
        m_frameCount = 0;
    }
    
    m_frameCount++;
    
    qint64 elapsed = now - m_frameRateStartTime;
    if (elapsed >= 1000) {
        int newFps = static_cast<int>(m_frameCount * 1000 / elapsed);
        if (m_frameRate != newFps) {
            m_frameRate = newFps;
            emit frameRateChanged();
        }
        
        m_frameRateStartTime = now;
        m_frameCount = 0;
    }
    
    m_lastFrameTime = now;
}

void VideoFrameProvider::onCursorShapeChanged(int width, int height,
                                              int hotspotX, int hotspotY,
                                              const QByteArray& data)
{
    if (width <= 0 || height <= 0) {
        m_cursorImage = QImage();
        m_cursorHotspot = QPoint(0, 0);
        emit cursorChanged();
        return;
    }
    
    int expectedSize = width * height * 4;
    if (data.size() < expectedSize) {
        LOG_WARN("Cursor data size mismatch: expected {} got {}", 
                 expectedSize, data.size());
        return;
    }
    
    m_cursorImage = QImage(reinterpret_cast<const uchar*>(data.constData()),
                           width, height, width * 4,
                           QImage::Format_ARGB32);
    m_cursorImage = m_cursorImage.copy();
    
    m_cursorHotspot = QPoint(hotspotX, hotspotY);
    
    if (CursorImageProvider::instance()) {
        CursorImageProvider::instance()->setCursor(m_deviceId, m_cursorImage, m_cursorHotspot);
    }
    
    emit cursorChanged();
}

} // namespace quickdesk
