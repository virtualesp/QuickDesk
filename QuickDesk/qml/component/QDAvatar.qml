// Fluent Design Avatar Component
import QtQuick

Item {
    id: root
    
    // ============ Avatar Types ============
    
    enum Size {
        Small,      // 24x24
        Medium,     // 32x32
        Large,      // 48x48
        XLarge      // 64x64
    }
    
    // ============ Custom Properties ============
    
    property int avatarSize: QDAvatar.Size.Medium
    property string imageSource: ""
    property string name: ""
    property color backgroundColor: Theme.primary
    property color textColor: Theme.textOnPrimary
    property bool showBadge: false
    property color badgeColor: Theme.success
    
    // ============ Private Properties ============
    
    readonly property int _size: {
        switch(avatarSize) {
            case QDAvatar.Size.Small: return 24
            case QDAvatar.Size.Medium: return 32
            case QDAvatar.Size.Large: return 48
            case QDAvatar.Size.XLarge: return 64
            default: return 32
        }
    }
    
    readonly property int _fontSize: {
        switch(avatarSize) {
            case QDAvatar.Size.Small: return Theme.fontSizeSmall
            case QDAvatar.Size.Medium: return Theme.fontSizeMedium
            case QDAvatar.Size.Large: return Theme.fontSizeLarge
            case QDAvatar.Size.XLarge: return Theme.fontSizeXLarge
            default: return Theme.fontSizeMedium
        }
    }
    
    readonly property string _initials: {
        if (name === "") return "?"
        var parts = name.trim().split(" ")
        if (parts.length >= 2) {
            return (parts[0][0] + parts[1][0]).toUpperCase()
        }
        return name.substring(0, 2).toUpperCase()
    }
    
    // ============ Size ============
    
    implicitWidth: _size
    implicitHeight: _size
    
    // ============ Avatar Container ============
    
    Rectangle {
        id: avatarContainer
        anchors.fill: parent
        radius: width / 2
        color: root.backgroundColor
        
        // Image (if provided)
        Image {
            anchors.fill: parent
            source: root.imageSource
            fillMode: Image.PreserveAspectCrop
            visible: root.imageSource !== ""
            smooth: true
            
            layer.enabled: true
            layer.effect: ShaderEffect {
                property variant source: parent
            }
            
            // Clip to circle
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
            }
        }
        
        // Initials (if no image)
        Text {
            visible: root.imageSource === ""
            anchors.centerIn: parent
            text: root._initials
            font.family: Theme.fontFamily
            font.pixelSize: root._fontSize
            font.weight: Font.DemiBold
            color: root.textColor
        }
        
        // Border
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.1)
        }
    }
    
    // ============ Status Badge ============
    
    Rectangle {
        visible: root.showBadge
        width: root._size / 3
        height: root._size / 3
        radius: width / 2
        color: root.badgeColor
        border.width: 2
        border.color: Theme.background
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }
}
