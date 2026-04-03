// Copyright 2026 QuickDesk Authors
// Shared memory manager for video frame transfer

#ifndef QUICKDESK_MANAGER_SHAREDMEMORYMANAGER_H
#define QUICKDESK_MANAGER_SHAREDMEMORYMANAGER_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QSharedMemory>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <map>
#include <memory>
#include <string>

namespace quickdesk {

// Magic number for shared frame memory: "QDVF" (QuickDesk Video Frame)
constexpr quint32 kSharedFrameMagic = 0x51444656;

// Current version of the shared frame header
constexpr quint32 kSharedFrameVersion = 1;

// Pixel format enumeration (I420 only, must match C++ client)
enum class SharedFrameFormat : quint32 {
    I420 = 3,  // YUV I420 (Y plane + U plane + V plane) - GPU rendering
};

// Header structure for shared memory video frames (64 bytes)
// Must match the C++ client's SharedFrameHeader exactly
#pragma pack(push, 1)
struct SharedFrameHeader {
    quint32 magic;        // Magic number (kSharedFrameMagic)
    quint32 version;      // Header version (kSharedFrameVersion)
    quint32 width;        // Frame width in pixels
    quint32 height;       // Frame height in pixels
    quint32 format;       // Pixel format (always SharedFrameFormat::I420)
    quint32 frame_index;  // Incrementing frame counter
    quint64 timestamp_us; // Frame timestamp in microseconds
    quint32 data_size;    // Size of YUV data in bytes (including stride padding)
    quint32 y_stride;     // Y plane bytes per line (width + optional padding)
    quint32 u_stride;     // U plane bytes per line (width/2 + optional padding)
    quint32 v_stride;     // V plane bytes per line (width/2 + optional padding)
    quint32 reserved[4];  // Reserved for future use
};
#pragma pack(pop)

static_assert(sizeof(SharedFrameHeader) == 64,
              "SharedFrameHeader must be exactly 64 bytes");

/**
 * @brief Frame data structure for efficient GPU rendering (YUV I420 only)
 * Contains raw YUV frame data and metadata without copying
 */
struct FrameData {
    bool valid = false;
    quint32 width = 0;
    quint32 height = 0;
    quint32 frameIndex = 0;
    quint64 timestampUs = 0;
    SharedFrameFormat format = SharedFrameFormat::I420;
    const uchar* data = nullptr;  // Pointer to YUV data (valid only while locked)
    quint32 dataSize = 0;
};

/**
 * @brief Per-device shared memory handle using QSharedMemory
 */
struct SharedMemoryHandle {
    QString deviceId;
    QString sharedMemoryName;
    quint32 lastFrameIndex = 0;
    std::unique_ptr<QSharedMemory> sharedMemory;
    bool isLocked = false;  // Track lock state for lockFrame/unlockFrame
};

/**
 * @brief Manages shared memory regions for video frame transfer
 * 
 * Each device connection has its own shared memory region.
 * This class handles attaching to, reading from, and detaching from
 * these shared memory regions.
 * 
 * Uses QSharedMemory with setNativeKey() for cross-platform compatibility
 * with the C++ client which uses platform-specific shared memory APIs.
 */
class SharedMemoryManager : public QObject {
    Q_OBJECT

public:
    explicit SharedMemoryManager(QObject* parent = nullptr);
    ~SharedMemoryManager() override;

    /**
     * @brief Attach to shared memory for a device
     * @param deviceId The device identifier
     * @param sharedMemoryName The native name of the shared memory region
     * @return true if successfully attached
     */
    bool attach(const QString& deviceId, const QString& sharedMemoryName);

    /**
     * @brief Detach from shared memory for a device
     * @param deviceId The device identifier
     */
    void detach(const QString& deviceId);

    /**
     * @brief Detach from all shared memory regions
     */
    void detachAll();

    /**
     * @brief Check if attached to shared memory for a device
     * @param deviceId The device identifier
     * @return true if attached
     */
    bool isAttached(const QString& deviceId) const;

    /**
     * @brief Read the current frame as QVideoFrame for GPU rendering (YUV I420)
     * @param deviceId The device identifier
     * @return QVideoFrame ready for VideoOutput, or invalid frame if failed
     */
    QVideoFrame readVideoFrame(const QString& deviceId);

    /**
     * @brief Lock shared memory and get frame data without copying
     * @param deviceId The device identifier
     * @return FrameData with pointer to raw data, call unlockFrame() when done
     * @note The returned data pointer is only valid until unlockFrame() is called
     */
    FrameData lockFrame(const QString& deviceId);

    /**
     * @brief Unlock shared memory after reading frame data
     * @param deviceId The device identifier
     */
    void unlockFrame(const QString& deviceId);

    /**
     * @brief Get the last frame index for a device
     * @param deviceId The device identifier
     * @return The last frame index, or 0 if not attached
     */
    quint32 lastFrameIndex(const QString& deviceId) const;

    /**
     * @brief Check if a new frame is available (frame index changed)
     * @param deviceId The device identifier
     * @param currentFrameIndex The current frame index from notification
     * @return true if the frame is new
     */
    bool isNewFrame(const QString& deviceId, quint32 currentFrameIndex) const;

    /**
     * @brief Get frame dimensions for a device
     * @param deviceId The device identifier
     * @return QSize with width and height, or empty size if not available
     */
    QSize frameSize(const QString& deviceId) const;

signals:
    void attachmentChanged(const QString& deviceId, bool attached);
    void frameReadError(const QString& deviceId, const QString& error);

private:
    // Use std::map with unique_ptr to avoid copy issues
    std::map<std::string, std::unique_ptr<SharedMemoryHandle>> m_handles;

    void closeHandle(SharedMemoryHandle& handle);
};

} // namespace quickdesk

#endif // QUICKDESK_MANAGER_SHAREDMEMORYMANAGER_H
