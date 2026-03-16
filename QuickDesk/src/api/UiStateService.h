// Copyright 2026 QuickDesk Authors
// Aggregated UI state service: combines OCR, screen size, and active window info

#ifndef QUICKDESK_API_UISTATESERVICE_H
#define QUICKDESK_API_UISTATESERVICE_H

#include "OcrEngine.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace quickdesk {

class MainController;

// ---------------------------------------------------------------------------
// UiState — snapshot of the full UI state for a connection
// ---------------------------------------------------------------------------
struct UiState {
    QString connectionId;

    // Screen resolution (from ConnectionInfo)
    int screenWidth  = 0;
    int screenHeight = 0;

    // OCR result (may be empty if OCR not ready or frame unavailable)
    OcrResult ocr;

    // Active window title on the remote desktop (via Win32 GetForegroundWindow)
    // Empty string if unavailable.
    QString activeWindowTitle;

    // Whether the OCR result came from cache
    bool ocrFromCache = false;
};

// ---------------------------------------------------------------------------
// UiStateService — Qt-side helper that builds UiState objects and implements
//                  waitForText / assertTextPresent.
//
// Not a QObject; owned by ApiHandler and called synchronously from WS thread.
// ---------------------------------------------------------------------------
class UiStateService {
public:
    explicit UiStateService(MainController* controller);

    // Build a full UI state snapshot for the given connection.
    // Returns false and sets errorOut if the frame is unavailable or OCR is not ready.
    bool getUiState(const QString& connectionId,
                    UiState& out,
                    QString& errorOut);

    // Block (poll) until the given text appears on screen or timeoutMs elapses.
    // Returns true if text was found; errorOut is set on error (e.g. no frame).
    // ignoreCase and exact follow the same semantics as findElement.
    bool waitForText(const QString& connectionId,
                     const QString& text,
                     bool exact,
                     bool ignoreCase,
                     int  timeoutMs,
                     OcrTextBlock& foundBlock,
                     QString& errorOut);

    // Non-blocking: check if the given text is currently on screen.
    // Returns true if found, false if not found.
    // errorOut is set on OCR/frame error (distinct from "not found").
    bool assertTextPresent(const QString& connectionId,
                           const QString& text,
                           bool exact,
                           bool ignoreCase,
                           OcrTextBlock& foundBlock,
                           QString& errorOut);

private:
    // Run OCR on the current frame (uses cache).
    // Returns false if frame unavailable or OCR not ready.
    bool runOcr(const QString& connectionId,
                OcrResult& result,
                bool& fromCache,
                QString& errorOut);

    // Search result.blocks for text matching query/exact/ignoreCase.
    // Returns first match or a default-constructed OcrTextBlock.
    static OcrTextBlock findInBlocks(const OcrResult& result,
                                     const QString& text,
                                     bool exact,
                                     bool ignoreCase);

    MainController* m_controller;
};

// ---------------------------------------------------------------------------
// JSON serialisation helpers (used by ApiHandler)
// ---------------------------------------------------------------------------
QJsonObject uiStateToJson(const UiState& state);
QJsonObject ocrBlockToJson(const OcrTextBlock& blk);

} // namespace quickdesk

#endif // QUICKDESK_API_UISTATESERVICE_H
