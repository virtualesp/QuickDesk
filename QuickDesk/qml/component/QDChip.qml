// Fluent Design Chip/Tag Component
import QtQuick

Rectangle {
    id: root
    
    // ============ Chip Types ============
    
    enum Type {
        Default,
        Primary,
        Success,
        Warning,
        Error,
        Info
    }
    
    // ============ Custom Properties ============
    
    property int chipType: QDChip.Type.Default
    property string text: ""
    property string iconText: ""
    property bool closable: false
    property bool outlined: false
    
    // ============ Signals ============
    
    signal clicked()
    signal closeClicked()
    
    // ============ Size & Style ============
    
    implicitWidth: chipContent.implicitWidth + Theme.spacingMedium * 2
    implicitHeight: Theme.buttonHeightSmall
    radius: height / 2
    
    color: {
        if (outlined) {
            return "transparent"
        }
        switch(chipType) {
            case QDChip.Type.Primary: return Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.15)
            case QDChip.Type.Success: return Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.15)
            case QDChip.Type.Warning: return Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.15)
            case QDChip.Type.Error: return Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.15)
            case QDChip.Type.Info: return Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.15)
            default: return Theme.surfaceVariant
        }
    }
    
    border.width: outlined ? Theme.borderWidthMedium : 0
    border.color: {
        switch(chipType) {
            case QDChip.Type.Primary: return Theme.primary
            case QDChip.Type.Success: return Theme.success
            case QDChip.Type.Warning: return Theme.warning
            case QDChip.Type.Error: return Theme.error
            case QDChip.Type.Info: return Theme.info
            default: return Theme.border
        }
    }
    
    // ============ Content ============
    
    Row {
        id: chipContent
        anchors.centerIn: parent
        spacing: Theme.spacingSmall
        
        // Icon
        Text {
            visible: iconText !== ""
            text: iconText
            font.family: "Segoe Fluent Icons"
            font.pixelSize: Theme.iconSizeSmall
            color: _textColor
            anchors.verticalCenter: parent.verticalCenter
        }
        
        // Text
        Text {
            text: root.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            color: _textColor
            anchors.verticalCenter: parent.verticalCenter
        }
        
        // Close button
        Rectangle {
            visible: root.closable
            width: 16
            height: 16
            radius: 8
            color: closeMouseArea.containsMouse ? Qt.rgba(0, 0, 0, 0.1) : "transparent"
            anchors.verticalCenter: parent.verticalCenter
            
            Text {
                anchors.centerIn: parent
                text: FluentIconGlyph.cancelGlyph
                font.family: "Segoe Fluent Icons"
                font.pixelSize: 10
                color: _textColor
            }
            
            MouseArea {
                id: closeMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    mouse.accepted = true
                    root.closeClicked()
                }
            }
        }
    }
    
    // ============ Private Helper ============
    
    readonly property color _textColor: {
        switch(chipType) {
            case QDChip.Type.Primary: return Theme.primary
            case QDChip.Type.Success: return Theme.success
            case QDChip.Type.Warning: return Theme.warning
            case QDChip.Type.Error: return Theme.error
            case QDChip.Type.Info: return Theme.info
            default: return Theme.text
        }
    }
    
    // ============ Mouse Interaction ============
    
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        
        onEntered: root.scale = 1.05
        onExited: root.scale = 1.0
    }
    
    Behavior on scale {
        NumberAnimation {
            duration: Theme.animationDurationFast
            easing.type: Easing.OutCubic
        }
    }
}
