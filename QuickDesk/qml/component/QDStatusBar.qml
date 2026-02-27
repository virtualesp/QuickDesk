// Fluent Design StatusBar Component
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: control
    
    property bool showSeparator: true
    
    default property alias content: contentLayout.data
    
    implicitHeight: Theme.buttonHeightMedium
    color: Theme.surfaceVariant
    
    Rectangle {
        visible: control.showSeparator
        anchors.top: parent.top
        width: parent.width
        height: Theme.borderWidthThin
        color: Theme.border
    }
    
    RowLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMedium
        anchors.rightMargin: Theme.spacingMedium
        spacing: Theme.spacingMedium
    }
    
    Behavior on color {
        ColorAnimation { duration: Theme.animationDurationFast }
    }
}
