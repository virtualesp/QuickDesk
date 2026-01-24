// Fluent Design Spinner Component (Loading Animation)
import QtQuick

Item {
    id: root
    
    // ============ Custom Properties ============
    
    property int size: 32
    property color spinnerColor: Theme.primary
    property int duration: 1000
    property bool running: true
    
    // ============ Size ============
    
    implicitWidth: size
    implicitHeight: size
    
    // ============ Spinner Animation ============
    
    Rectangle {
        id: spinner
        anchors.fill: parent
        color: "transparent"
        visible: root.running
        
        // Circular path with dots
        Repeater {
            model: 8
            
            Rectangle {
                width: root.size / 6
                height: root.size / 6
                radius: width / 2
                color: root.spinnerColor
                
                property real angle: index * (360 / 8)
                property real distance: root.size / 2 - width / 2
                
                x: root.width / 2 + Math.cos((angle - 90) * Math.PI / 180) * distance - width / 2
                y: root.height / 2 + Math.sin((angle - 90) * Math.PI / 180) * distance - height / 2
                
                opacity: 1.0 - (index * 0.1)
                
                SequentialAnimation on opacity {
                    running: root.running
                    loops: Animation.Infinite
                    
                    PauseAnimation {
                        duration: index * (root.duration / 8)
                    }
                    
                    NumberAnimation {
                        from: 0.2
                        to: 1.0
                        duration: root.duration / 8
                        easing.type: Easing.InOutQuad
                    }
                    
                    NumberAnimation {
                        from: 1.0
                        to: 0.2
                        duration: root.duration * 7 / 8
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }
    }
}
