// Fluent Design RadioButton Component
import QtQuick
import QtQuick.Controls as Controls

Controls.RadioButton {
    id: control
    
    // ============ Size & Style ============
    
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + topPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)
    
    spacing: Theme.spacingMedium
    padding: Theme.spacingSmall
    
    // ============ Indicator (Radio circle) ============
    
    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 10
        
        color: control.enabled ? Theme.surface : Theme.surfaceVariant
        border.width: Theme.borderWidthMedium
        border.color: {
            if (!control.enabled) {
                return Theme.border
            }
            if (control.checked) {
                return Theme.primary
            }
            if (control.hovered) {
                return Theme.borderHover
            }
            return Theme.border
        }
        
        Behavior on border.color {
            ColorAnimation {
                duration: Theme.animationDurationFast
                easing.type: Theme.animationEasingType
            }
        }
        
        // Inner circle (when checked)
        Rectangle {
            width: 10
            height: 10
            anchors.centerIn: parent
            radius: 5
            color: control.enabled ? Theme.primary : Theme.primaryDisabled
            visible: control.checked
            scale: control.checked ? 1 : 0
            
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.animationDurationFast
                    easing.type: Easing.OutBack
                }
            }
        }
        
        // Ripple effect
        Rectangle {
            id: ripple
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            radius: width / 2
            color: Theme.primary
            opacity: 0
            scale: 1
            
            NumberAnimation on opacity {
                id: rippleAnimation
                from: 0.3
                to: 0
                duration: 400
                running: false
            }
            
            NumberAnimation on scale {
                id: rippleScaleAnimation
                from: 1
                to: 2
                duration: 400
                running: false
            }
        }
    }
    
    // ============ Content (Text label) ============
    
    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeMedium
        color: control.enabled ? Theme.text : Theme.textDisabled
        verticalAlignment: Text.AlignVCenter
        
        Behavior on color {
            ColorAnimation { duration: Theme.animationDurationFast }
        }
    }
    
    // ============ Interaction ============
    
    onPressed: {
        if (enabled) {
            rippleAnimation.start()
            rippleScaleAnimation.start()
        }
    }
    
    // Hover cursor
    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
    }
}
