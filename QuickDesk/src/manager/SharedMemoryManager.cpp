// Copyright 2026 QuickDesk Authors

#include "SharedMemoryManager.h"
#include "YUVPlanarVideoBuffer.h"
#include "infra/log/log.h"

#ifndef Q_OS_WIN
#include <QNativeIpcKey>
#endif

namespace quickdesk {

SharedMemoryManager::SharedMemoryManager(QObject* parent)
    : QObject(parent)
{
}

SharedMemoryManager::~SharedMemoryManager()
{
    detachAll();
}

bool SharedMemoryManager::attach(const QString& deviceId, 
                                  const QString& sharedMemoryName)
{
    std::string key = deviceId.toStdString();
    
    auto it = m_handles.find(key);
    if (it != m_handles.end()) {
        auto& existing = it->second;
        if (existing->sharedMemoryName == sharedMemoryName && 
            existing->sharedMemory && existing->sharedMemory->isAttached()) {
            return true;
        }
        detach(deviceId);
    }

    auto shm = std::make_unique<QSharedMemory>();
    
#ifdef Q_OS_WIN
    shm->setNativeKey(sharedMemoryName);
#else
    shm->setNativeKey(QNativeIpcKey(sharedMemoryName, QNativeIpcKey::Type::PosixRealtime));
#endif

    if (!shm->attach(QSharedMemory::ReadOnly)) {
        LOG_WARN("Failed to attach to shared memory '{}' for device {}: {}",
                 sharedMemoryName.toStdString(), deviceId.toStdString(),
                 shm->errorString().toStdString());
        emit frameReadError(deviceId, 
                            QString("Failed to attach: %1").arg(shm->errorString()));
        return false;
    }

    auto handle = std::make_unique<SharedMemoryHandle>();
    handle->deviceId = deviceId;
    handle->sharedMemoryName = sharedMemoryName;
    handle->lastFrameIndex = 0;
    handle->sharedMemory = std::move(shm);

    m_handles[key] = std::move(handle);

    LOG_INFO("Attached to shared memory '{}' for device {}",
             sharedMemoryName.toStdString(), deviceId.toStdString());
    emit attachmentChanged(deviceId, true);
    return true;
}

void SharedMemoryManager::detach(const QString& deviceId)
{
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end()) {
        return;
    }

    closeHandle(*it->second);
    m_handles.erase(it);

    LOG_INFO("Detached from shared memory for device {}", 
             deviceId.toStdString());
    emit attachmentChanged(deviceId, false);
}

void SharedMemoryManager::detachAll()
{
    for (auto& pair : m_handles) {
        closeHandle(*pair.second);
        emit attachmentChanged(pair.second->deviceId, false);
    }
    m_handles.clear();
    LOG_INFO("Detached from all shared memory regions");
}

bool SharedMemoryManager::isAttached(const QString& deviceId) const
{
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end()) {
        return false;
    }
    return it->second->sharedMemory && it->second->sharedMemory->isAttached();
}

QVideoFrame SharedMemoryManager::readVideoFrame(const QString& deviceId)
{
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end()) {
        return QVideoFrame();
    }

    auto& handle = it->second;
    if (!handle->sharedMemory || !handle->sharedMemory->isAttached()) {
        return QVideoFrame();
    }

    if (!handle->sharedMemory->lock()) {
        LOG_WARN("Failed to lock shared memory for device {}: {}",
                 deviceId.toStdString(), 
                 handle->sharedMemory->errorString().toStdString());
        return QVideoFrame();
    }

    QVideoFrame result;
    const void* data = handle->sharedMemory->constData();
    
    if (data) {
        const SharedFrameHeader* header = 
            static_cast<const SharedFrameHeader*>(data);

        if (header->magic == kSharedFrameMagic && 
            header->version == kSharedFrameVersion) {
            
            handle->lastFrameIndex = header->frame_index;

            quint32 width = header->width;
            quint32 height = header->height;
            quint32 dataSize = header->data_size;
            SharedFrameFormat frameFormat = static_cast<SharedFrameFormat>(header->format);
            Q_UNUSED(frameFormat);

            if (width > 0 && height > 0 && width <= 8192 && height <= 8192) {
                const uchar* frameData = static_cast<const uchar*>(data) + 
                                         sizeof(SharedFrameHeader);

                quint32 ySrcStride = header->y_stride;
                quint32 uSrcStride = header->u_stride;
                quint32 vSrcStride = header->v_stride;
                
                quint32 expectedSize = ySrcStride * height + 
                                      uSrcStride * (height / 2) + 
                                      vSrcStride * (height / 2);
                
                if (dataSize == expectedSize) {
                    QVideoFrameFormat format(QSize(static_cast<int>(width), static_cast<int>(height)),
                                            QVideoFrameFormat::Format_YUV420P);
                    format.setColorSpace(QVideoFrameFormat::ColorSpace_BT709);
                    format.setColorRange(QVideoFrameFormat::ColorRange_Video);

                    auto planarBuffer = std::make_unique<YUVPlanarVideoBuffer>(format);
                    
                    int sizeY = ySrcStride * height;
                    planarBuffer->m_data[0] = QByteArray(reinterpret_cast<const char*>(frameData), sizeY);
                    planarBuffer->m_bytesPerLine[0] = ySrcStride;
                    
                    const uchar* uSrc = frameData + ySrcStride * height;
                    int sizeU = uSrcStride * (height / 2);
                    planarBuffer->m_data[1] = QByteArray(reinterpret_cast<const char*>(uSrc), sizeU);
                    planarBuffer->m_bytesPerLine[1] = uSrcStride;
                    
                    const uchar* vSrc = frameData + ySrcStride * height + uSrcStride * (height / 2);
                    int sizeV = vSrcStride * (height / 2);
                    planarBuffer->m_data[2] = QByteArray(reinterpret_cast<const char*>(vSrc), sizeV);
                    planarBuffer->m_bytesPerLine[2] = vSrcStride;
                    
                    planarBuffer->m_planeCount = 3;
                    
                    result = QVideoFrame(std::move(planarBuffer));
                } else {
                    LOG_WARN("YUV I420 data size mismatch for device {}: {} vs expected {}",
                             deviceId.toStdString(), dataSize, expectedSize);
                }
            }
        }
    }

    handle->sharedMemory->unlock();
    return result;
}

FrameData SharedMemoryManager::lockFrame(const QString& deviceId)
{
    FrameData frameData;
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end()) {
        return frameData;
    }

    auto& handle = it->second;
    if (!handle->sharedMemory || !handle->sharedMemory->isAttached()) {
        return frameData;
    }

    if (handle->isLocked) {
        LOG_WARN("Shared memory already locked for device {}", 
                 deviceId.toStdString());
        return frameData;
    }

    if (!handle->sharedMemory->lock()) {
        LOG_WARN("Failed to lock shared memory for device {}: {}",
                 deviceId.toStdString(), 
                 handle->sharedMemory->errorString().toStdString());
        return frameData;
    }

    handle->isLocked = true;

    const void* data = handle->sharedMemory->constData();
    if (!data) {
        handle->sharedMemory->unlock();
        handle->isLocked = false;
        return frameData;
    }

    const SharedFrameHeader* header = 
        static_cast<const SharedFrameHeader*>(data);

    if (header->magic != kSharedFrameMagic || 
        header->version != kSharedFrameVersion) {
        handle->sharedMemory->unlock();
        handle->isLocked = false;
        return frameData;
    }

    quint32 width = header->width;
    quint32 height = header->height;
    quint32 dataSize = header->data_size;
    quint32 expectedSize = width * height + (width / 2) * (height / 2) * 2;

    if (width == 0 || height == 0 || width > 8192 || height > 8192 ||
        dataSize != expectedSize) {
        handle->sharedMemory->unlock();
        handle->isLocked = false;
        return frameData;
    }

    handle->lastFrameIndex = header->frame_index;

    frameData.valid = true;
    frameData.width = width;
    frameData.height = height;
    frameData.frameIndex = header->frame_index;
    frameData.timestampUs = header->timestamp_us;
    frameData.format = static_cast<SharedFrameFormat>(header->format);
    frameData.data = static_cast<const uchar*>(data) + sizeof(SharedFrameHeader);
    frameData.dataSize = dataSize;

    return frameData;
}

void SharedMemoryManager::unlockFrame(const QString& deviceId)
{
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end()) {
        return;
    }

    auto& handle = it->second;
    if (handle->isLocked && handle->sharedMemory) {
        handle->sharedMemory->unlock();
        handle->isLocked = false;
    }
}

QSize SharedMemoryManager::frameSize(const QString& deviceId) const
{
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end() || !it->second->sharedMemory || 
        !it->second->sharedMemory->isAttached()) {
        return QSize();
    }

    auto& handle = *it->second;
    if (!const_cast<QSharedMemory*>(handle.sharedMemory.get())->lock()) {
        return QSize();
    }

    QSize result;
    const void* data = handle.sharedMemory->constData();
    if (data) {
        const SharedFrameHeader* header = 
            static_cast<const SharedFrameHeader*>(data);
        if (header->magic == kSharedFrameMagic) {
            result = QSize(static_cast<int>(header->width), 
                          static_cast<int>(header->height));
        }
    }

    const_cast<QSharedMemory*>(handle.sharedMemory.get())->unlock();
    return result;
}

quint32 SharedMemoryManager::lastFrameIndex(const QString& deviceId) const
{
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end()) {
        return 0;
    }
    return it->second->lastFrameIndex;
}

bool SharedMemoryManager::isNewFrame(const QString& deviceId, 
                                      quint32 currentFrameIndex) const
{
    std::string key = deviceId.toStdString();
    auto it = m_handles.find(key);
    if (it == m_handles.end()) {
        return true;
    }
    return currentFrameIndex > it->second->lastFrameIndex;
}

void SharedMemoryManager::closeHandle(SharedMemoryHandle& handle)
{
    if (handle.sharedMemory) {
        if (handle.sharedMemory->isAttached()) {
            handle.sharedMemory->detach();
        }
        handle.sharedMemory.reset();
    }
    handle.lastFrameIndex = 0;
}

} // namespace quickdesk
