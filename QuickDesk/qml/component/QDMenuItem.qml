// Fluent Design Menu Item Component
import QtQuick

QtObject {
    id: control
    
    // ============ Properties ============
    
    property string text: ""
    property bool checkable: false
    property bool checked: false
    property bool enabled: true
    property bool separator: false
    property bool visible: true
    
    // ============ Signals ============
    
    signal triggered()
}
