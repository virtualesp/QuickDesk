// Remote Tab Bar Component - Tab bar for remote desktop windows
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../component"
import "."

Rectangle {
    id: control
    
    // Properties
    property var connections: [] // Array of connection objects
    property int currentIndex: 0
    property var performanceStatsMap: ({})
    property int statsVersion: 0  // Used to trigger updates
    
    // Signals
    signal tabClicked(int index)
    signal tabCloseRequested(int index)
    signal newTabRequested()
    
    // Style
    color: Theme.surface
    border.width: Theme.borderWidthThin
    border.color: Theme.border
    
    implicitHeight: 60
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingSmall
        
        // Scrollable tab area
        ListView {
            id: tabListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            orientation: ListView.Horizontal
            spacing: Theme.spacingSmall
            clip: true
            
            model: control.connections
            
            delegate: RemoteTab {
                required property int index
                required property var modelData
                
                connectionId: modelData.id || ""
                deviceName: modelData.name || modelData.deviceId || ""
                connectionState: modelData.state || "connected"
                isActive: index === control.currentIndex
                
                // Get performance stats from map
                property var stats: {
                    var _ = control.statsVersion  // Trigger update when version changes
                    return control.performanceStatsMap[modelData.id] || {
                        frameWidth: 0, 
                        frameHeight: 0, 
                        frameRate: 0,
                        ping: 0
                    }
                }
                frameWidth: stats.frameWidth
                frameHeight: stats.frameHeight
                frameRate: stats.frameRate
                ping: stats.ping
                
                onClicked: {
                    control.tabClicked(index)
                }
                
                onCloseRequested: {
                    control.tabCloseRequested(index)
                }
            }
            
            // Scroll buttons (if needed)
            ScrollBar.horizontal: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }       
    }
}
