// Remote Tab Component - Single tab in the tab bar
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../component"

Rectangle {
    id: control
    
    // Properties
    property string deviceId: ""
    property string deviceName: ""
    property int ping: 0
    property string connectionState: "connected" // connected, connecting, disconnected
    property bool isActive: false
    property int frameWidth: 0
    property int frameHeight: 0
    property int frameRate: 0
    property string routeType: ""  // "direct", "stun", "relay"
    
    // Signals
    signal clicked()
    signal closeRequested()
    
    // Size
    implicitWidth: 240
    implicitHeight: 40
    
    // Style
    color: {
        if (isActive) return Theme.surfaceVariant
        if (mouseArea.containsMouse) return Theme.surfaceHover
        return Theme.surface
    }
    
    border.width: Theme.borderWidthThin
    border.color: isActive ? Theme.primary : Theme.border
    radius: Theme.radiusSmall
    
    Behavior on color {
        ColorAnimation { duration: Theme.animationDurationFast }
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSmall
        spacing: Theme.spacingSmall
        
        // Device Icon
        Text {
            text: FluentIconGlyph.devicesGlyph
            font.family: "Segoe Fluent Icons"
            font.pixelSize: 16
            color: Theme.textSecondary
        }
        
        // Device Name and Status
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            
            // Row 1: Device name + ● P2P + ● 21ms
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXSmall

                Text {
                    text: deviceName || deviceId
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: isActive ? Font.DemiBold : Font.Normal
                    color: Theme.text
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    visible: connectionState === "connected" && routeType !== ""
                    color: (routeType === "direct" || routeType === "stun")
                           ? "#66BB6A" : routeType === "relay" ? "#FFA726"
                           : Theme.textDisabled
                }

                Text {
                    text: routeType === "direct" ? "P2P"
                        : routeType === "stun" ? "STUN"
                        : routeType === "relay" ? "Relay" : ""
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamilyMono
                    color: Theme.textSecondary
                    visible: connectionState === "connected" && routeType !== ""
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    visible: connectionState === "connected"
                    color: {
                        if (connectionState !== "connected") return Theme.textDisabled
                        if (ping < 50) return Theme.success
                        if (ping < 100) return Theme.warning
                        return Theme.error
                    }

                    Behavior on color {
                        ColorAnimation { duration: Theme.animationDurationFast }
                    }
                }

                Text {
                    text: ping + " ms"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamilyMono
                    color: Theme.textSecondary
                    visible: connectionState === "connected"
                }
            }
            
            // Row 2: Resolution + FPS (or Connecting...)
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXSmall

                Text {
                    text: frameWidth + "x" + frameHeight + " " + frameRate + "fps"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamilyMono
                    color: Theme.textSecondary
                    visible: connectionState === "connected" && frameWidth > 0
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: qsTr("Connecting...")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                    visible: connectionState === "connecting"
                }
            }
        }
        
        // Close Button
        Rectangle {
            Layout.minimumWidth: 20
            Layout.minimumHeight: 20
            width: 20
            height: 20
            radius: 10
            color: closeArea.containsMouse ? Theme.error : "transparent"
            
            Behavior on color {
                ColorAnimation { duration: Theme.animationDurationFast }
            }
            
            Text {
                anchors.centerIn: parent
                text: FluentIconGlyph.cancelGlyph
                font.family: "Segoe Fluent Icons"
                font.pixelSize: 10
                color: closeArea.containsMouse ? Theme.textOnPrimary : Theme.textSecondary
            }
            
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                
                onClicked: {
                    control.closeRequested()
                }
            }
        }
    }
    
    // Tab Click Area
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        anchors.rightMargin: 24
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        
        onClicked: {
            control.clicked()
        }
    }
}
