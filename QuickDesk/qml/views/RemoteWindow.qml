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
    property var connections: [] // Array of connection objects: [{id, deviceId, name, state}]
    property int currentTabIndex: 0
    
    // Performance stats stored separately to avoid triggering Repeater rebuild
    // Map: connectionId -> {frameWidth, frameHeight, frameRate, ping, originalWidth, originalHeight}
    property var performanceStatsMap: ({})
    property int statsVersion: 0  // Increment to notify changes
    
    // Get performance stats for a connection
    function getPerformanceStats(connectionId) {
        return performanceStatsMap[connectionId] || {
            frameWidth: 0, 
            frameHeight: 0, 
            frameRate: 0, 
            ping: 0,
            originalWidth: 0, 
            originalHeight: 0
        }
    }
    
    // Update performance stats without modifying connections array
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
        // Check if connection already exists
        for (var i = 0; i < connections.length; i++) {
            if (connections[i].id === connectionId) {
                console.log("Connection already exists in window:", connectionId)
                currentTabIndex = i
                return
            }
        }
        
        var conn = {
            id: connectionId,
            deviceId: deviceId,
            name: deviceId,
            state: "connecting"
            // ping removed from here
        }
        
        // Create new array to trigger property binding update
        var newConnections = connections.slice()
        newConnections.push(conn)
        connections = newConnections
        
        // Initialize performance stats
        var newStatsMap = Object.assign({}, performanceStatsMap)
        newStatsMap[connectionId] = {
            frameWidth: 0, 
            frameHeight: 0, 
            frameRate: 0, 
            ping: 0,
            originalWidth: 0, 
            originalHeight: 0
        }
        performanceStatsMap = newStatsMap
        
        currentTabIndex = connections.length - 1
        console.log("Added connection to remote window:", connectionId, "Total tabs:", connections.length)
    }
    
    // Close connection and remove tab (unified function for both scenarios)
    function closeConnection(index) {
        if (index < 0 || index >= connections.length) {
            console.warn("closeConnection: invalid index", index)
            return
        }
        
        var connId = connections[index].id
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
        if (index < 0 || index >= connections.length) return
        
        var connId = connections[index].id
        
        // Remove from performance stats map
        var newStatsMap = Object.assign({}, performanceStatsMap)
        delete newStatsMap[connId]
        performanceStatsMap = newStatsMap
        
        // Create new array to trigger property binding update
        var newConnections = connections.slice()
        newConnections.splice(index, 1)
        connections = newConnections
        
        // Update current tab index
        if (currentTabIndex >= connections.length) {
            currentTabIndex = Math.max(0, connections.length - 1)
        }
        
        // Close window if no connections left
        if (connections.length === 0) {
            remoteWindow.close()
        }
        
        console.log("Removed connection from remote window:", connId, "Remaining tabs:", connections.length)
    }
    
    // Clean up all connections when window closes
    onClosing: function(close) {
        console.log("RemoteWindow closing, disconnecting all connections")
        for (var i = 0; i < connections.length; i++) {
            if (clientManager) {
                console.log("Disconnecting:", connections[i].id)
                clientManager.disconnectFromHost(connections[i].id)
            }
        }
        connections = []
    }
    
    // Update connection state
    function updateConnectionState(connectionId, state, ping) {
        // Update state in connections array (only if state changed)
        if (state !== "") {
            for (var i = 0; i < connections.length; i++) {
                if (connections[i].id === connectionId && connections[i].state !== state) {
                    var newConnections = connections.slice()
                    newConnections[i].state = state
                    connections = newConnections
                    console.log("Updated connection state:", connectionId, "->", state)
                    break
                }
            }
        }
        
        // Update ping in performance stats map (doesn't trigger Repeater rebuild)
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
            connections: remoteWindow.connections
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
                model: remoteWindow.connections
                
                Item {
                    required property int index
                    required property var modelData
                    
                    // Remote desktop video view (ONLY video, no overlay UI)
                    RemoteDesktopView {
                        id: desktopView
                        anchors.fill: parent
                        connectionId: modelData.id
                        clientManager: remoteWindow.clientManager
                        active: index === remoteWindow.currentTabIndex
                        
                        // Monitor video size changes (frameRate and ping updated from PerformanceTracker)
                        onFrameWidthChanged: {
                            if (frameWidth > 0 && frameHeight > 0) {
                                var stats = remoteWindow.getPerformanceStats(modelData.id)
                                remoteWindow.updatePerformanceStats(modelData.id, frameWidth, frameHeight, stats.frameRate, stats.ping)
                            }
                        }
                        onFrameHeightChanged: {
                            if (frameWidth > 0 && frameHeight > 0) {
                                var stats = remoteWindow.getPerformanceStats(modelData.id)
                                remoteWindow.updatePerformanceStats(modelData.id, frameWidth, frameHeight, stats.frameRate, stats.ping)
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
            visible: remoteWindow.connections.length > 0
            
            connectionId: remoteWindow.currentTabIndex >= 0 && remoteWindow.currentTabIndex < remoteWindow.connections.length 
                ? remoteWindow.connections[remoteWindow.currentTabIndex].id 
                : ""
            clientManager: remoteWindow.clientManager
            videoInfo: {
                var connId = remoteWindow.currentTabIndex >= 0 && remoteWindow.currentTabIndex < remoteWindow.connections.length 
                    ? remoteWindow.connections[remoteWindow.currentTabIndex].id 
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
                for (var i = 0; i < remoteWindow.connections.length; i++) {
                    if (remoteWindow.connections[i].id === connectionId) {
                        remoteWindow.closeConnection(i)
                        break
                    }
                }
            }
            
            onShowToast: function(message, toastType) {
                toast.show(message, toastType)
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
                // Find the connection index and close it
                for (var i = 0; i < remoteWindow.connections.length; i++) {
                    if (remoteWindow.connections[i].id === connectionId) {
                        console.log("Auto-closing tab for", state, "connection:", connectionId, "at index:", i)
                        // Use Qt.callLater to avoid modifying array during iteration
                        Qt.callLater(function() {
                            remoteWindow.closeConnection(i)
                        })
                        break
                    }
                }
            }
        }
    }
    
    // Monitor performance stats updates
    Connections {
        target: remoteWindow.clientManager
        
        function onPerformanceStatsUpdated(connectionId, totalLatencyMs, bandwidthKbps, frameRate) {
            // Update connection latency value
            remoteWindow.updateConnectionState(connectionId, "", totalLatencyMs)
            
            // Update frameRate from PerformanceTracker
            var stats = remoteWindow.getPerformanceStats(connectionId)
            if (stats && stats.frameWidth > 0 && stats.frameHeight > 0) {
                remoteWindow.updatePerformanceStats(connectionId, stats.frameWidth, stats.frameHeight, frameRate, totalLatencyMs)
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
