// Remote Desktop Window - Independent window for remote desktop connections
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QuickDesk 1.0

import "../component"
import "../quickdeskcomponent"

Window {
    id: remoteWindow
    width: 1280
    height: 720
    visible: true
    title: qsTr("QuickDesk - Remote Desktop")
    
    // Properties
    property var clientManager: null
    property alias connectionModel: connectionModelObj  // C++ model for incremental updates
    property int currentTabIndex: 0
    property bool hasAutoResized: false  // Only auto-resize once on first frame
    property bool showVideoStats: false  // Toggle video stats overlay
    
    // C++ ConnectionListModel — only affected delegates are created/destroyed
    ConnectionListModel {
        id: connectionModelObj
    }
    
    // Performance stats stored separately to avoid triggering model rebuild
    // Map: connectionId -> { frameWidth, frameHeight, frameRate, ping,
    //   originalWidth, originalHeight, captureMs, encodeMs, decodeMs, paintMs,
    //   totalLatencyMs, roundTripMs, bandwidthKbps, packetRate, codec,
    //   frameQuality, encodedRectWidth, encodedRectHeight }
    property var performanceStatsMap: ({})
    property int statsVersion: 0  // Increment to notify changes
    
    // Get performance stats for a connection
    function getPerformanceStats(connectionId) {
        return performanceStatsMap[connectionId] || {
            frameWidth: 0, frameHeight: 0, frameRate: 0, ping: 0,
            originalWidth: 0, originalHeight: 0,
            captureMs: 0, encodeMs: 0, decodeMs: 0, paintMs: 0,
            totalLatencyMs: 0, roundTripMs: 0,
            bandwidthKbps: 0, packetRate: 0,
            codec: "", frameQuality: -1,
            encodedRectWidth: 0, encodedRectHeight: 0
        }
    }
    
    // Update performance stats without modifying connections model
    function updatePerformanceStats(connectionId, width, height, fps, ping) {
        var stats = performanceStatsMap[connectionId]
        
        // Handle video size update
        if (width !== undefined && height !== undefined && width > 0 && height > 0) {
            // Ensure fps is non-negative and round to integer for comparison
            if (fps !== undefined) {
                fps = Math.max(0, Math.round(fps))
            }
            
            // Check if there's any actual change
            if (stats && stats.frameWidth === width && stats.frameHeight === height && 
                (fps === undefined || stats.frameRate === fps)) {
                // No video change, but might need to update ping
                if (ping === undefined) {
                    return  // Nothing to update
                }
            }
            
            // Record original resolution on first valid frame
            var originalWidth = stats ? stats.originalWidth : 0
            var originalHeight = stats ? stats.originalHeight : 0
            
            if (!stats || (stats.originalWidth === 0 && width > 0 && height > 0)) {
                originalWidth = width
                originalHeight = height
                console.log("✓ Recorded original resolution for", connectionId, ":", width + "x" + height)
            }
            
            // Create new stats object
            var newStatsMap = Object.assign({}, performanceStatsMap)
            newStatsMap[connectionId] = {
                frameWidth: width,
                frameHeight: height,
                frameRate: fps !== undefined ? fps : (stats ? stats.frameRate : 0),
                ping: ping !== undefined ? ping : (stats ? stats.ping : 0),
                originalWidth: originalWidth,
                originalHeight: originalHeight
            }
            performanceStatsMap = newStatsMap
            
            // Only increment version if width or height changed (affects layout)
            if (!stats || stats.frameWidth !== width || stats.frameHeight !== height) {
                statsVersion++
            }
        } 
        // Handle ping-only update
        else if (ping !== undefined && stats) {
            var newStatsMap = Object.assign({}, performanceStatsMap)
            newStatsMap[connectionId] = Object.assign({}, stats, {ping: ping})
            performanceStatsMap = newStatsMap
        }
    }
    
    // Add connection to this window
    function addConnection(connectionId, deviceId) {
        var existingIdx = connectionModel.indexOf(connectionId)
        if (existingIdx >= 0) {
            console.log("Connection already exists in window:", connectionId)
            currentTabIndex = existingIdx
            return
        }
        
        connectionModel.addConnection(connectionId, deviceId)
        
        // Initialize performance stats
        var newStatsMap = Object.assign({}, performanceStatsMap)
        newStatsMap[connectionId] = {
            frameWidth: 0, frameHeight: 0, frameRate: 0, ping: 0,
            originalWidth: 0, originalHeight: 0,
            captureMs: 0, encodeMs: 0, decodeMs: 0, paintMs: 0,
            totalLatencyMs: 0, roundTripMs: 0,
            bandwidthKbps: 0, packetRate: 0,
            codec: "", frameQuality: -1,
            encodedRectWidth: 0, encodedRectHeight: 0
        }
        performanceStatsMap = newStatsMap
        
        currentTabIndex = connectionModel.count - 1
        console.log("Added connection to remote window:", connectionId, "Total tabs:", connectionModel.count)
    }
    
    // Close connection and remove tab (unified function for both scenarios)
    function closeConnection(index) {
        if (index < 0 || index >= connectionModel.count) {
            console.warn("closeConnection: invalid index", index)
            return
        }
        
        var connId = connectionModel.connectionIdAt(index)
        console.log("Closing connection:", connId, "at index:", index)
        
        // 1. Disconnect from host
        if (clientManager) {
            clientManager.disconnectFromHost(connId)
        }
        
        // 2. Remove the tab
        removeConnection(index)
    }
    
    // Remove connection from this window (internal helper)
    function removeConnection(index) {
        if (index < 0 || index >= connectionModel.count) return
        
        var connId = connectionModel.connectionIdAt(index)
        
        // Remove from performance stats map
        var newStatsMap = Object.assign({}, performanceStatsMap)
        delete newStatsMap[connId]
        performanceStatsMap = newStatsMap
        
        // Remove from model — only destroys this one delegate
        connectionModel.removeConnection(index)
        
        // Update current tab index
        if (currentTabIndex >= connectionModel.count) {
            currentTabIndex = Math.max(0, connectionModel.count - 1)
        }
        
        // Close window if no connections left
        if (connectionModel.count === 0) {
            remoteWindow.close()
        }
        
        console.log("Removed connection from remote window:", connId, "Remaining tabs:", connectionModel.count)
    }
    
    // Clean up all connections when window closes
    onClosing: function(close) {
        console.log("RemoteWindow closing, disconnecting all connections")
        for (var i = 0; i < connectionModel.count; i++) {
            if (clientManager) {
                console.log("Disconnecting:", connectionModel.connectionIdAt(i))
                clientManager.disconnectFromHost(connectionModel.connectionIdAt(i))
            }
        }
        connectionModel.clear()
    }
    
    // Core resize logic — resize window to best fit the given remote desktop resolution
    // Can be called manually at any time (e.g. from toolbar "Fit Window" button)
    function resizeToFit(fw, fh) {
        if (fw <= 0 || fh <= 0) return

        var scr = remoteWindow.screen
        if (!scr) {
            console.warn("resizeToFit: screen not available")
            return false
        }

        // Available screen space (leave margin for taskbar, etc.)
        var maxWidth = scr.desktopAvailableWidth * 0.92
        var maxHeight = scr.desktopAvailableHeight * 0.92

        // Account for tab bar height
        var tabBarH = tabBar.height > 0 ? tabBar.height : 36
        var contentMaxHeight = maxHeight - tabBarH

        // Calculate scale factor (never upscale)
        var scale = Math.min(maxWidth / fw, contentMaxHeight / fh, 1.0)

        var newWidth  = Math.round(fw * scale)
        var newHeight = Math.round(fh * scale) + tabBarH

        // Center on screen
        remoteWindow.width  = newWidth
        remoteWindow.height = newHeight
        remoteWindow.x = Math.round((scr.width  - newWidth)  / 2) + scr.virtualX
        remoteWindow.y = Math.round((scr.height - newHeight) / 2) + scr.virtualY

        console.log("Resized window to", newWidth + "x" + newHeight,
                     "for remote desktop", fw + "x" + fh,
                     "(scale:", scale.toFixed(3) + ")",
                     "screen:", scr.width + "x" + scr.height,
                     "available:", scr.desktopAvailableWidth + "x" + scr.desktopAvailableHeight)
        return true
    }

    // Auto-resize window to best fit the remote desktop resolution (called once on first frame)
    // Triggered from onStatsVersionChanged (at window level, not inside Repeater delegate)
    function autoResizeToFit(fw, fh) {
        console.log("autoResizeToFit called:", fw + "x" + fh,
                     "hasAutoResized:", hasAutoResized,
                     "screen:", remoteWindow.screen ? "valid" : "null")

        if (fw <= 0 || fh <= 0) return
        if (hasAutoResized) return

        if (!remoteWindow.screen) {
            // Screen not ready — retry via Timer
            console.log("autoResizeToFit: screen not ready, scheduling retry")
            retryResizeTimer.pendingWidth = fw
            retryResizeTimer.pendingHeight = fh
            retryResizeTimer.retryCount = 0
            retryResizeTimer.start()
            return
        }

        // Mark AFTER screen check so retries work
        hasAutoResized = true
        resizeToFit(fw, fh)
    }

    // When frame dimensions change (statsVersion incremented), try auto-resize
    // Using Qt.callLater ensures execution on a clean call stack,
    // avoiding issues with nested signal handler / delegate destruction races.
    onStatsVersionChanged: {
        if (hasAutoResized) return
        if (connectionModel.count === 0) return

        var connId = currentTabIndex >= 0 && currentTabIndex < connectionModel.count
                     ? connectionModel.connectionIdAt(currentTabIndex) : ""
        if (!connId) return

        var s = getPerformanceStats(connId)
        if (s && s.frameWidth > 0 && s.frameHeight > 0) {
            var w = s.frameWidth
            var h = s.frameHeight
            Qt.callLater(function() {
                autoResizeToFit(w, h)
            })
        }
    }

    // Update connection state
    function updateConnectionState(connectionId, state, ping) {
        // Update state in model (only emits dataChanged for the affected row)
        if (state !== "") {
            connectionModel.updateState(connectionId, state)
        }
        
        // Update ping in performance stats map (doesn't trigger model rebuild)
        if (ping !== undefined) {
            updatePerformanceStats(connectionId, undefined, undefined, undefined, ping)
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Tab Bar
        RemoteTabBar {
            id: tabBar
            Layout.fillWidth: true
            connectionModel: remoteWindow.connectionModel
            currentIndex: remoteWindow.currentTabIndex
            performanceStatsMap: remoteWindow.performanceStatsMap
            statsVersion: remoteWindow.statsVersion
            
            onTabClicked: function(index) {
                remoteWindow.currentTabIndex = index
            }
            
            onTabCloseRequested: function(index) {
                remoteWindow.closeConnection(index)
            }
            
            onNewTabRequested: {
                // TODO: Show quick connect dialog
                console.log("New tab requested")
            }
        }
        
        // Remote Desktop View Stack
        StackLayout {
            id: desktopStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: remoteWindow.currentTabIndex
            
            Repeater {
                model: connectionModel
                
                Item {
                    id: delegateItem
                    required property int index
                    required property string connectionId
                    
                    // Remote desktop video view (ONLY video, no overlay UI)
                    RemoteDesktopView {
                        id: desktopView
                        anchors.fill: parent
                        connectionId: delegateItem.connectionId
                        clientManager: remoteWindow.clientManager
                        active: delegateItem.index === remoteWindow.currentTabIndex
                        
                        // Monitor video size changes (frameRate and ping updated from PerformanceTracker)
                        onFrameWidthChanged: {
                            if (frameWidth > 0 && frameHeight > 0) {
                                var connId = delegateItem.connectionId
                                var stats = remoteWindow.getPerformanceStats(connId)
                                remoteWindow.updatePerformanceStats(connId, frameWidth, frameHeight, stats.frameRate, stats.ping)
                            }
                        }
                        onFrameHeightChanged: {
                            if (frameWidth > 0 && frameHeight > 0) {
                                var connId = delegateItem.connectionId
                                var stats = remoteWindow.getPerformanceStats(connId)
                                remoteWindow.updatePerformanceStats(connId, frameWidth, frameHeight, stats.frameRate, stats.ping)
                            }
                        }
                    }
                }
            }
        }
    }
        
    Item {
        anchors.fill: parent
        anchors.topMargin: tabBar.height  // Offset by tab bar height        
        
        // Single floating button bound to current active connection
        FloatingToolButton {
            x: parent.width - width - Theme.spacingXLarge
            y: Theme.spacingXLarge
            z: 1000
            visible: connectionModel.count > 0
            
            connectionId: currentTabIndex >= 0 && currentTabIndex < connectionModel.count 
                ? connectionModel.connectionIdAt(currentTabIndex) 
                : ""
            clientManager: remoteWindow.clientManager
            videoInfo: {
                var connId = currentTabIndex >= 0 && currentTabIndex < connectionModel.count 
                    ? connectionModel.connectionIdAt(currentTabIndex) 
                    : ""
                return connId ? remoteWindow.getPerformanceStats(connId) : null
            }
            desktopView: {
                // Find the current desktop view
                if (remoteWindow.currentTabIndex >= 0) {
                    var stackItem = desktopStack.children[remoteWindow.currentTabIndex]
                    return stackItem ? stackItem.children[0] : null
                }
                return null
            }
            
            onDisconnectRequested: function(connectionId) {
                console.log("FloatingToolButton disconnect requested for:", connectionId)
                
                // Find the connection index and close it
                var idx = connectionModel.indexOf(connectionId)
                if (idx >= 0) {
                    remoteWindow.closeConnection(idx)
                }
            }
            
            onFitToRemoteDesktopRequested: {
                // Get current tab's frame dimensions and resize window
                var connId = currentTabIndex >= 0 && currentTabIndex < connectionModel.count
                    ? connectionModel.connectionIdAt(currentTabIndex) : ""
                if (!connId) return

                var s = remoteWindow.getPerformanceStats(connId)
                if (s && s.frameWidth > 0 && s.frameHeight > 0) {
                    console.log("Manual fit window to remote desktop:", s.frameWidth + "x" + s.frameHeight)
                    remoteWindow.resizeToFit(s.frameWidth, s.frameHeight)
                }
            }
            
            onToggleVideoStats: {
                remoteWindow.showVideoStats = !remoteWindow.showVideoStats
            }
            
            onShowToast: function(message, toastType) {
                toast.show(message, toastType)
            }
        }
        
        // Video Stats Overlay — semi-transparent panel with detailed stats
        VideoStatsOverlay {
            id: videoStatsOverlay
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.spacingMedium
            z: 999
            visible: remoteWindow.showVideoStats && connectionModel.count > 0
            
            stats: {
                // Force re-evaluation when statsVersion changes
                var _version = remoteWindow.statsVersion
                var connId = currentTabIndex >= 0 && currentTabIndex < connectionModel.count
                    ? connectionModel.connectionIdAt(currentTabIndex) : ""
                return connId ? remoteWindow.getPerformanceStats(connId) : null
            }
        }
    }
    
    // Monitor connection state changes
    Connections {
        target: remoteWindow.clientManager
        
        function onConnectionStateChanged(connectionId, state, hostInfo) {
            console.log("Remote window: connection state changed:", connectionId, state)
            
            // Update connection state
            remoteWindow.updateConnectionState(connectionId, state, 0)
            
            // Auto-close tab when connection is disconnected or failed
            if (state === "disconnected" || state === "failed") {
                var idx = connectionModel.indexOf(connectionId)
                if (idx >= 0) {
                    console.log("Auto-closing tab for", state, "connection:", connectionId, "at index:", idx)
                    // Use Qt.callLater to avoid modifying model during signal handling
                    var capturedIdx = idx
                    Qt.callLater(function() {
                        remoteWindow.closeConnection(capturedIdx)
                    })
                }
            }
        }
    }
    
    // Monitor performance stats updates (detailed stats from C++ PerformanceTracker)
    Connections {
        target: remoteWindow.clientManager
        
        function onPerformanceStatsUpdated(connectionId, detailedStats) {
            var totalLatencyMs = detailedStats.totalLatencyMs || 0
            var frameRate = detailedStats.frameRate || 0
            
            // Update connection latency value (for tab bar display)
            remoteWindow.updateConnectionState(connectionId, "", totalLatencyMs)
            
            // Update frameRate and merge detailed stats
            var existing = remoteWindow.getPerformanceStats(connectionId)
            if (existing && existing.frameWidth > 0 && existing.frameHeight > 0) {
                remoteWindow.updatePerformanceStats(connectionId,
                    existing.frameWidth, existing.frameHeight, frameRate, totalLatencyMs)
            }
            
            // Merge detailed timing/codec stats into performanceStatsMap
            var current = remoteWindow.performanceStatsMap[connectionId]
            if (current) {
                var newStatsMap = Object.assign({}, remoteWindow.performanceStatsMap)
                newStatsMap[connectionId] = Object.assign({}, current, {
                    captureMs:         detailedStats.captureMs || 0,
                    encodeMs:          detailedStats.encodeMs || 0,
                    decodeMs:          detailedStats.decodeMs || 0,
                    paintMs:           detailedStats.paintMs || 0,
                    totalLatencyMs:    totalLatencyMs,
                    roundTripMs:       detailedStats.roundTripMs || 0,
                    bandwidthKbps:     detailedStats.bandwidthKbps || 0,
                    packetRate:        detailedStats.packetRate || 0,
                    codec:             detailedStats.codec || "",
                    frameQuality:      detailedStats.frameQuality !== undefined ? detailedStats.frameQuality : -1,
                    encodedRectWidth:  detailedStats.encodedRectWidth || 0,
                    encodedRectHeight: detailedStats.encodedRectHeight || 0
                })
                remoteWindow.performanceStatsMap = newStatsMap
            }
        }
    }
    
    // Toast for notifications
    QDToast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 50
        z: 9999
    }
}
