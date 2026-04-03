// Copyright 2026 QuickDesk Authors

#include "UiStateService.h"
#include "OcrCache.h"
#include "controller/MainController.h"
#include "manager/ClientManager.h"
#include "manager/SharedMemoryManager.h"
#include "infra/log/log.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QThread>
#include <QVideoFrame>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace quickdesk {

// ---------------------------------------------------------------------------
// UiStateService
// ---------------------------------------------------------------------------

UiStateService::UiStateService(MainController* controller)
    : m_controller(controller)
{}

bool UiStateService::runOcr(const QString& deviceId,
                             OcrResult& result,
                             bool& fromCache,
                             QString& errorOut)
{
    if (!OcrEngine::instance().isInitialized()) {
        errorOut = "OCR engine not ready. Check that model files are present in the models/ directory.";
        return false;
    }

    auto* shm = m_controller->clientManager()->sharedMemoryManager();
    if (!shm || !shm->isAttached(deviceId)) {
        errorOut = QString("No video frame available for: %1").arg(deviceId);
        return false;
    }

    QVideoFrame vf = shm->readVideoFrame(deviceId);
    if (!vf.isValid()) {
        errorOut = "Failed to read video frame";
        return false;
    }

    QImage image = vf.toImage();
    if (image.isNull()) {
        errorOut = "Failed to convert video frame to QImage";
        return false;
    }

    QString frameHash = OcrEngine::computeFrameHash(image);
    OcrResult cached;
    if (OcrCache::instance().get(frameHash, cached)) {
        result    = cached;
        fromCache = true;
        return true;
    }

    result           = OcrEngine::instance().recognize(image);
    result.frameHash = frameHash;
    OcrCache::instance().put(frameHash, result);
    fromCache = false;
    return true;
}

// static
OcrTextBlock UiStateService::findInBlocks(const OcrResult& result,
                                           const QString& text,
                                           bool exact,
                                           bool ignoreCase)
{
    Qt::CaseSensitivity cs = ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
    for (const auto& blk : result.blocks) {
        bool hit = exact ? (blk.text.compare(text, cs) == 0)
                         : blk.text.contains(text, cs);
        if (hit) return blk;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Active window title (Windows-only; other platforms return empty string)
// ---------------------------------------------------------------------------
static QString getActiveWindowTitle()
{
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return {};
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::wstring buf(static_cast<size_t>(len + 1), L'\0');
    GetWindowTextW(hwnd, buf.data(), len + 1);
    return QString::fromStdWString(buf);
#else
    return {};
#endif
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool UiStateService::getUiState(const QString& deviceId,
                                 UiState& out,
                                 QString& errorOut)
{
    auto* client = m_controller->clientManager();
    auto info = client->getConnection(deviceId);
    if (info.deviceId.isEmpty()) {
        errorOut = QString("Device not found: %1").arg(deviceId);
        return false;
    }

    out.deviceId      = deviceId;
    out.screenWidth   = info.width;
    out.screenHeight  = info.height;
    out.activeWindowTitle = getActiveWindowTitle();

    QString ocrError;
    bool fromCache = false;
    if (!runOcr(deviceId, out.ocr, fromCache, ocrError)) {
        LOG_WARN("UiStateService::getUiState: OCR skipped: %s", qPrintable(ocrError));
        // Leave out.ocr empty — caller gets partial state
    }
    out.ocrFromCache = fromCache;
    return true;
}

bool UiStateService::waitForText(const QString& deviceId,
                                  const QString& text,
                                  bool exact,
                                  bool ignoreCase,
                                  int  timeoutMs,
                                  OcrTextBlock& foundBlock,
                                  QString& errorOut)
{
    constexpr int kPollIntervalMs = 200;
    int elapsed = 0;

    while (elapsed <= timeoutMs) {
        OcrResult result;
        bool fromCache = false;
        if (!runOcr(deviceId, result, fromCache, errorOut)) {
            return false;
        }

        OcrTextBlock blk = findInBlocks(result, text, exact, ignoreCase);
        if (!blk.text.isEmpty()) {
            foundBlock = blk;
            return true;
        }

        if (elapsed >= timeoutMs) break;

        QThread::msleep(static_cast<unsigned long>(
            qMin(kPollIntervalMs, timeoutMs - elapsed)));
        elapsed += kPollIntervalMs;
    }

    errorOut = QString("Text \"%1\" did not appear within %2 ms").arg(text).arg(timeoutMs);
    return false;
}

bool UiStateService::assertTextPresent(const QString& deviceId,
                                        const QString& text,
                                        bool exact,
                                        bool ignoreCase,
                                        OcrTextBlock& foundBlock,
                                        QString& errorOut)
{
    OcrResult result;
    bool fromCache = false;
    if (!runOcr(deviceId, result, fromCache, errorOut)) {
        return false;
    }

    OcrTextBlock blk = findInBlocks(result, text, exact, ignoreCase);
    if (!blk.text.isEmpty()) {
        foundBlock = blk;
        return true;
    }
    return false;  // not found — errorOut is NOT set (distinguish from OCR error)
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

QJsonObject ocrBlockToJson(const OcrTextBlock& blk)
{
    return QJsonObject{
        {"text",       blk.text},
        {"confidence", blk.confidence},
        {"bbox",       QJsonObject{
            {"x", blk.bbox.x()}, {"y", blk.bbox.y()},
            {"w", blk.bbox.width()}, {"h", blk.bbox.height()}
        }},
        {"center", QJsonObject{
            {"x", blk.center.x()}, {"y", blk.center.y()}
        }}
    };
}

QJsonObject uiStateToJson(const UiState& state)
{
    // OCR blocks
    QJsonArray blocksArr;
    for (const auto& blk : state.ocr.blocks) {
        blocksArr.append(ocrBlockToJson(blk));
    }

    QJsonObject ocrObj;
    ocrObj["blocks"]    = blocksArr;
    ocrObj["frameHash"] = state.ocr.frameHash;
    ocrObj["fromCache"] = state.ocrFromCache;

    QJsonObject screenObj;
    screenObj["width"]  = state.screenWidth;
    screenObj["height"] = state.screenHeight;

    QJsonObject activeWindowObj;
    activeWindowObj["title"] = state.activeWindowTitle;

    QJsonObject obj;
    obj["deviceId"]      = state.deviceId;
    obj["screen"]        = screenObj;
    obj["ocr"]           = ocrObj;
    obj["activeWindow"]  = activeWindowObj;
    return obj;
}

} // namespace quickdesk
