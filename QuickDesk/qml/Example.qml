// Fluent Design Components Example
import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import "component"

Window {
    width: 1200
    height: 800
    visible: true
Item {
    id: root
    anchors.fill: parent
    
    // ============ Global Toast ============
    
    QDToast {
        id: globalToast
    }
    
    // ============ Global MessageBox ============
    
    QDMessageBox {
        id: globalMessageBox
    }
    
    // ============ Example Dialog ============
    
    QDDialog {
        id: exampleDialog
        title: "示例对话框"
        dialogWidth: 500
        dialogHeight: 400
        
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingLarge
            
            Text {
                text: "这是一个 Fluent Design 风格的对话框示例"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.text
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            
            QDTextField {
                placeholderText: "输入一些内容..."
                Layout.fillWidth: true
            }
            
            QDCheckBox {
                text: "记住我的选择"
            }
            
            Item { Layout.fillHeight: true }
        }
        
        footer: [
            QDButton {
                text: "取消"
                buttonType: QDButton.Type.Secondary
                onClicked: exampleDialog.reject()
            },
            QDButton {
                text: "确认"
                buttonType: QDButton.Type.Primary
                onClicked: {
                    exampleDialog.accept()
                    globalToast.show("操作已确认", QDToast.Type.Success)
                }
            }
        ]
    }
    
    // ============ Main Content ============
    
    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }
    
    QQC.ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        
        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingXXLarge
            
            // Header
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Theme.primary }
                        GradientStop { position: 1.0; color: Theme.accent }
                    }
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: Theme.spacingMedium
                        
                        Text {
                            text: "Fluent Design 组件库"
                            font.family: Theme.fontFamily
                            font.pixelSize: 32
                            font.weight: Font.Bold
                            color: Theme.textOnPrimary
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Text {
                            text: "Modern Fluent Design System for QuickDesk"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMedium
                            color: Qt.rgba(1, 1, 1, 0.8)
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        // Theme Switcher
                        Row {
                            spacing: Theme.spacingSmall
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: Theme.spacingMedium
                            
                            Text {
                                text: "主题: "
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.textOnPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            
                            Repeater {
                                model: [
                                    { name: "Fluent Dark", type: Theme.ThemeType.FluentDark },
                                    { name: "Fluent Light", type: Theme.ThemeType.FluentLight },
                                    { name: "Nord", type: Theme.ThemeType.NordDark },
                                    { name: "Dracula", type: Theme.ThemeType.DraculaDark },
                                    { name: "Monokai", type: Theme.ThemeType.MonokaiDark },
                                    { name: "Solarized", type: Theme.ThemeType.SolarizedLight }
                                ]
                                
                                Rectangle {
                                    width: 90
                                    height: 28
                                    radius: Theme.radiusSmall
                                    color: Theme.currentTheme === modelData.type ? 
                                           Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(0, 0, 0, 0.2)
                                    border.width: Theme.currentTheme === modelData.type ? 2 : 1
                                    border.color: Qt.rgba(1, 1, 1, Theme.currentTheme === modelData.type ? 0.6 : 0.3)
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.name
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.weight: Theme.currentTheme === modelData.type ? Font.DemiBold : Font.Normal
                                        color: Theme.textOnPrimary
                                    }
                                    
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            Theme.currentTheme = modelData.type
                                            globalToast.show("已切换到 " + modelData.name + " 主题", QDToast.Type.Success)
                                        }
                                    }
                                    
                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }
                                    
                                    Behavior on border.color {
                                        ColorAnimation { duration: 150 }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Content sections
            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Theme.spacingXXLarge
                spacing: Theme.spacingXXLarge
                
                // ============ Buttons Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "按钮 (Buttons)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: buttonContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: buttonContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingLarge
                            
                            // Primary buttons
                            RowLayout {
                                spacing: Theme.spacingMedium
                                
                                QDButton {
                                    text: "Primary"
                                    buttonType: QDButton.Type.Primary
                                    onClicked: globalToast.show("Primary 按钮被点击", QDToast.Type.Info)
                                }
                                
                                QDButton {
                                    text: "带图标"
                                    buttonType: QDButton.Type.Primary
                                    iconText: FluentIconGlyph.checkMarkGlyph
                                    onClicked: globalToast.show("操作成功", QDToast.Type.Success)
                                }
                                
                                QDButton {
                                    text: "加载中..."
                                    buttonType: QDButton.Type.Primary
                                    loading: true
                                }
                                
                                QDButton {
                                    text: "禁用"
                                    buttonType: QDButton.Type.Primary
                                    enabled: false
                                }
                            }
                            
                            // Secondary & Other buttons
                            RowLayout {
                                spacing: Theme.spacingMedium
                                
                                QDButton {
                                    text: "Secondary"
                                    buttonType: QDButton.Type.Secondary
                                    onClicked: globalToast.show("Secondary 按钮", QDToast.Type.Info)
                                }
                                
                                QDButton {
                                    text: "Success"
                                    buttonType: QDButton.Type.Success
                                    iconText: FluentIconGlyph.acceptGlyph
                                    onClicked: globalToast.show("成功操作", QDToast.Type.Success)
                                }
                                
                                QDButton {
                                    text: "Danger"
                                    buttonType: QDButton.Type.Danger
                                    iconText: FluentIconGlyph.deleteGlyph
                                    onClicked: globalToast.show("危险操作", QDToast.Type.Error)
                                }
                                
                                QDButton {
                                    text: "Ghost"
                                    buttonType: QDButton.Type.Ghost
                                    onClicked: globalToast.show("Ghost 按钮", QDToast.Type.Info)
                                }
                            }
                        }
                    }
                }
                
                // ============ Input Fields Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "文本输入 (Text Fields)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: inputContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        GridLayout {
                            id: inputContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            columns: 2
                            rowSpacing: Theme.spacingLarge
                            columnSpacing: Theme.spacingXLarge
                            
                            QDTextField {
                                placeholderText: "普通输入框"
                                Layout.fillWidth: true
                            }
                            
                            QDTextField {
                                placeholderText: "带前缀图标"
                                prefixIcon: FluentIconGlyph.searchGlyph
                                Layout.fillWidth: true
                            }
                            
                            QDTextField {
                                placeholderText: "带后缀图标"
                                suffixIcon: FluentIconGlyph.settingsGlyph
                                Layout.fillWidth: true
                            }
                            
                            QDTextField {
                                placeholderText: "密码输入"
                                echoMode: TextInput.Password
                                prefixIcon: FluentIconGlyph.lockGlyph
                                Layout.fillWidth: true
                            }
                            
                            QDTextField {
                                text: "错误状态"
                                error: true
                                errorText: "输入不合法"
                                Layout.fillWidth: true
                            }
                            
                            QDTextField {
                                placeholderText: "禁用状态"
                                enabled: false
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
                
                // ============ Checkboxes Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "复选框 (CheckBoxes)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: checkboxContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: checkboxContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingMedium
                            
                            QDCheckBox {
                                text: "默认复选框"
                            }
                            
                            QDCheckBox {
                                text: "已选中"
                                checked: true
                            }
                            
                            QDCheckBox {
                                text: "部分选中"
                                checkState: Qt.PartiallyChecked
                            }
                            
                            QDCheckBox {
                                text: "禁用状态"
                                enabled: false
                            }
                            
                            QDCheckBox {
                                text: "禁用且选中"
                                enabled: false
                                checked: true
                            }
                        }
                    }
                }
                
                // ============ Cards Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "卡片 (Cards)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        rowSpacing: Theme.spacingLarge
                        columnSpacing: Theme.spacingLarge
                        
                        QDCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            elevation: 1
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Theme.spacingSmall
                                
                                Text {
                                    text: FluentIconGlyph.cloudGlyph
                                    font.family: "Segoe Fluent Icons"
                                    font.pixelSize: 32
                                    color: Theme.accent
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                
                                Text {
                                    text: "普通卡片"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMedium
                                    color: Theme.text
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                        
                        QDCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            elevation: 2
                            hoverable: true
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Theme.spacingSmall
                                
                                Text {
                                    text: FluentIconGlyph.heartGlyph
                                    font.family: "Segoe Fluent Icons"
                                    font.pixelSize: 32
                                    color: Theme.error
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                
                                Text {
                                    text: "可悬停卡片"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMedium
                                    color: Theme.text
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                        
                        QDCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            elevation: 3
                            hoverable: true
                            clickable: true
                            
                            onClicked: globalToast.show("卡片被点击了", QDToast.Type.Info)
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Theme.spacingSmall
                                
                                Text {
                                    text: FluentIconGlyph.clickGlyph
                                    font.family: "Segoe Fluent Icons"
                                    font.pixelSize: 32
                                    color: Theme.success
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                
                                Text {
                                    text: "可点击卡片"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeMedium
                                    color: Theme.text
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }
                }
                
                // ============ Toast & Dialog Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "提示与对话框 (Toast & Dialog)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: toastContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: toastContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingLarge
                            
                            Text {
                                text: "Toast 消息提示"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            RowLayout {
                                spacing: Theme.spacingMedium
                                
                                QDButton {
                                    text: "Success"
                                    buttonType: QDButton.Type.Success
                                    onClicked: globalToast.show("操作成功完成！", QDToast.Type.Success)
                                }
                                
                                QDButton {
                                    text: "Error"
                                    buttonType: QDButton.Type.Danger
                                    onClicked: globalToast.show("发生错误，请重试", QDToast.Type.Error)
                                }
                                
                                QDButton {
                                    text: "Warning"
                                    buttonType: QDButton.Type.Secondary
                                    iconText: FluentIconGlyph.warningGlyph
                                    onClicked: globalToast.show("这是一个警告消息", QDToast.Type.Warning)
                                }
                                
                                QDButton {
                                    text: "Info"
                                    buttonType: QDButton.Type.Primary
                                    onClicked: globalToast.show("这是一条普通信息", QDToast.Type.Info)
                                }
                            }
                            
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: Theme.border
                            }
                            
                            Text {
                                text: "Dialog 对话框"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            QDButton {
                                text: "打开对话框"
                                buttonType: QDButton.Type.Primary
                                iconText: FluentIconGlyph.openPaneGlyph
                                onClicked: exampleDialog.show()
                            }
                        }
                    }
                }
                
                // ============ Switch & Slider Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "开关与滑块 (Switch & Slider)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: switchContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: switchContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingLarge
                            
                            Text {
                                text: "Switch 开关"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            RowLayout {
                                spacing: Theme.spacingXLarge
                                
                                QDSwitch {
                                    text: "默认开关"
                                }
                                
                                QDSwitch {
                                    text: "已开启"
                                    checked: true
                                }
                                
                                QDSwitch {
                                    text: "禁用"
                                    enabled: false
                                }
                            }
                            
                            QDDivider {
                                Layout.fillWidth: true
                            }
                            
                            Text {
                                text: "Slider 滑块"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            ColumnLayout {
                                spacing: Theme.spacingMedium
                                
                                RowLayout {
                                    spacing: Theme.spacingMedium
                                    
                                    Text {
                                        text: "音量:"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMedium
                                        color: Theme.text
                                    }
                                    
                                    QDSlider {
                                        id: volumeSlider
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 100
                                        value: 50
                                        showValue: true
                                        decimals: 0
                                    }
                                    
                                    Text {
                                        text: Math.round(volumeSlider.value) + "%"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMedium
                                        color: Theme.textSecondary
                                        Layout.preferredWidth: 40
                                    }
                                }
                                
                                QDSlider {
                                    Layout.fillWidth: true
                                    enabled: false
                                    value: 0.3
                                }
                            }
                        }
                    }
                }
                
                // ============ Progress & ComboBox Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "进度条与下拉框 (Progress & ComboBox)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: progressContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: progressContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingLarge
                            
                            Text {
                                text: "ProgressBar 进度条"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium
                                
                                RowLayout {
                                    spacing: Theme.spacingMedium
                                    
                                    Text {
                                        text: "下载进度:"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMedium
                                        color: Theme.text
                                    }
                                    
                                    QDProgressBar {
                                        Layout.fillWidth: true
                                        value: progressTimer.progress
                                        
                                        Timer {
                                            id: progressTimer
                                            property real progress: 0
                                            interval: 50
                                            repeat: true
                                            running: true
                                            onTriggered: {
                                                progress += 0.005
                                                if (progress > 1) progress = 0
                                            }
                                        }
                                    }
                                    
                                    Text {
                                        text: Math.round(progressTimer.progress * 100) + "%"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMedium
                                        color: Theme.textSecondary
                                        Layout.preferredWidth: 40
                                    }
                                }
                                
                                RowLayout {
                                    spacing: Theme.spacingMedium
                                    
                                    Text {
                                        text: "加载中:"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMedium
                                        color: Theme.text
                                    }
                                    
                                    QDProgressBar {
                                        Layout.fillWidth: true
                                        indeterminate: true
                                    }
                                }
                            }
                            
                            QDDivider {
                                Layout.fillWidth: true
                            }
                            
                            Text {
                                text: "ComboBox 下拉框"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            RowLayout {
                                spacing: Theme.spacingXLarge
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    Text {
                                        text: "选择语言"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                    }
                                    
                                    QDComboBox {
                                        model: ["简体中文", "English", "日本語", "한국어"]
                                    }
                                }
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    Text {
                                        text: "选择分辨率"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                    }
                                    
                                    QDComboBox {
                                        model: ["1920x1080", "2560x1440", "3840x2160"]
                                        currentIndex: 1
                                    }
                                }
                            }
                        }
                    }
                }
                
                // ============ Badge Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "徽章 (Badge)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: badgeContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: badgeContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingLarge
                            
                            RowLayout {
                                spacing: Theme.spacingXLarge
                                
                                // Badge with count
                                Item {
                                    width: 60
                                    height: 40
                                    
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 40
                                        height: 40
                                        radius: Theme.radiusMedium
                                        color: Theme.surface
                                        border.width: 1
                                        border.color: Theme.border
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: FluentIconGlyph.mailGlyph
                                            font.family: "Segoe Fluent Icons"
                                            font.pixelSize: 20
                                            color: Theme.text
                                        }
                                    }
                                    
                                    QDBadge {
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        count: 5
                                        badgeType: QDBadge.Type.Error
                                    }
                                }
                                
                                // Badge with large count
                                Item {
                                    width: 60
                                    height: 40
                                    
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 40
                                        height: 40
                                        radius: Theme.radiusMedium
                                        color: Theme.surface
                                        border.width: 1
                                        border.color: Theme.border
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: FluentIconGlyph.chatBubblesGlyph
                                            font.family: "Segoe Fluent Icons"
                                            font.pixelSize: 20
                                            color: Theme.text
                                        }
                                    }
                                    
                                    QDBadge {
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        count: 128
                                        badgeType: QDBadge.Type.Primary
                                    }
                                }
                                
                                // Dot badge
                                Item {
                                    width: 60
                                    height: 40
                                    
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 40
                                        height: 40
                                        radius: Theme.radiusMedium
                                        color: Theme.surface
                                        border.width: 1
                                        border.color: Theme.border
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: FluentIconGlyph.settingsGlyph
                                            font.family: "Segoe Fluent Icons"
                                            font.pixelSize: 20
                                            color: Theme.text
                                        }
                                    }
                                    
                                    QDBadge {
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        dot: true
                                        badgeType: QDBadge.Type.Success
                                    }
                                }
                                
                                // Text badges
                                QDBadge {
                                    text: "New"
                                    badgeType: QDBadge.Type.Success
                                }
                                
                                QDBadge {
                                    text: "Beta"
                                    badgeType: QDBadge.Type.Warning
                                }
                                
                                QDBadge {
                                    text: "Error"
                                    badgeType: QDBadge.Type.Error
                                }
                            }
                        }
                    }
                }
                
                // ============ Radio & Chip & Avatar Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "单选、标签与头像 (Radio, Chip & Avatar)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: radioContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: radioContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingLarge
                            
                            Text {
                                text: "RadioButton 单选按钮"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            ColumnLayout {
                                spacing: Theme.spacingMedium
                                
                                QQC.ButtonGroup {
                                    id: radioGroup
                                }
                                
                                QDRadioButton {
                                    text: "选项 1"
                                    checked: true
                                    QQC.ButtonGroup.group: radioGroup
                                }
                                
                                QDRadioButton {
                                    text: "选项 2"
                                    QQC.ButtonGroup.group: radioGroup
                                }
                                
                                QDRadioButton {
                                    text: "选项 3"
                                    QQC.ButtonGroup.group: radioGroup
                                }
                                
                                QDRadioButton {
                                    text: "禁用选项"
                                    enabled: false
                                }
                            }
                            
                            QDDivider {
                                Layout.fillWidth: true
                            }
                            
                            Text {
                                text: "Chip 标签"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            Flow {
                                Layout.fillWidth: true
                                spacing: Theme.spacingMedium
                                
                                QDChip {
                                    text: "默认"
                                    chipType: QDChip.Type.Default
                                }
                                
                                QDChip {
                                    text: "主要"
                                    chipType: QDChip.Type.Primary
                                }
                                
                                QDChip {
                                    text: "成功"
                                    chipType: QDChip.Type.Success
                                    iconText: FluentIconGlyph.checkMarkGlyph
                                }
                                
                                QDChip {
                                    text: "警告"
                                    chipType: QDChip.Type.Warning
                                    iconText: FluentIconGlyph.warningGlyph
                                }
                                
                                QDChip {
                                    text: "错误"
                                    chipType: QDChip.Type.Error
                                    iconText: FluentIconGlyph.errorGlyph
                                }
                                
                                QDChip {
                                    text: "信息"
                                    chipType: QDChip.Type.Info
                                    iconText: FluentIconGlyph.infoGlyph
                                }
                                
                                QDChip {
                                    text: "可关闭"
                                    chipType: QDChip.Type.Primary
                                    closable: true
                                    onCloseClicked: globalToast.show("标签已关闭", QDToast.Type.Info)
                                }
                                
                                QDChip {
                                    text: "轮廓"
                                    chipType: QDChip.Type.Primary
                                    outlined: true
                                }
                            }
                            
                            QDDivider {
                                Layout.fillWidth: true
                            }
                            
                            Text {
                                text: "Avatar 头像"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            Row {
                                spacing: Theme.spacingLarge
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    QDAvatar {
                                        name: "Zhang San"
                                        avatarSize: QDAvatar.Size.Small
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    
                                    Text {
                                        text: "Small"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    QDAvatar {
                                        name: "Li Si"
                                        avatarSize: QDAvatar.Size.Medium
                                        backgroundColor: Theme.success
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    
                                    Text {
                                        text: "Medium"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    QDAvatar {
                                        name: "Wang Wu"
                                        avatarSize: QDAvatar.Size.Large
                                        backgroundColor: Theme.warning
                                        showBadge: true
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    
                                    Text {
                                        text: "Large (在线)"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    QDAvatar {
                                        name: "Zhao Liu"
                                        avatarSize: QDAvatar.Size.XLarge
                                        backgroundColor: Theme.error
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    
                                    Text {
                                        text: "XLarge"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                            }
                        }
                    }
                }
                
                // ============ Spinner & Menu & MessageBox Section ============
                
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge
                    
                    Text {
                        text: "加载、菜单与消息框 (Spinner, Menu & MessageBox)"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeHeading
                        font.weight: Font.Bold
                        color: Theme.text
                    }
                    
                    QDCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: spinnerContent.implicitHeight + Theme.spacingXLarge * 2
                        elevation: 1
                        
                        ColumnLayout {
                            id: spinnerContent
                            anchors.fill: parent
                            anchors.margins: Theme.spacingXLarge
                            spacing: Theme.spacingLarge
                            
                            Text {
                                text: "Spinner 加载动画"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            Row {
                                spacing: Theme.spacingXLarge
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    QDSpinner {
                                        size: 24
                                        spinnerColor: Theme.primary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    
                                    Text {
                                        text: "主色"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    QDSpinner {
                                        size: 32
                                        spinnerColor: Theme.success
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    
                                    Text {
                                        text: "成功色"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                                
                                ColumnLayout {
                                    spacing: Theme.spacingSmall
                                    
                                    QDSpinner {
                                        size: 40
                                        spinnerColor: Theme.warning
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    
                                    Text {
                                        text: "警告色"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textSecondary
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                            }
                            
                            QDDivider {
                                Layout.fillWidth: true
                            }
                            
                            Text {
                                text: "Menu 菜单"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            QDButton {
                                text: "打开菜单"
                                iconText: FluentIconGlyph.moreGlyph
                                buttonType: QDButton.Type.Secondary
                                onClicked: contextMenu.open()
                                
                                QDMenu {
                                    id: contextMenu
                                    x: 0  // 左对齐
                                    y: parent.height
                                    
                                    QDMenuItem {
                                        text: "新建"
                                        onTriggered: globalToast.show("点击了新建", QDToast.Type.Info)
                                    }
                                    
                                    QDMenuItem {
                                        text: "打开"
                                        onTriggered: globalToast.show("点击了打开", QDToast.Type.Info)
                                    }
                                    
                                    QDMenuSeparator { }
                                    
                                    QDMenuItem {
                                        text: "保存"
                                        checkable: true
                                        checked: true
                                    }
                                    
                                    QDMenuItem {
                                        text: "另存为"
                                        checkable: true
                                    }
                                    
                                    QDMenuSeparator { }
                                    
                                    QDMenuItem {
                                        text: "退出"
                                        onTriggered: globalToast.show("点击了退出", QDToast.Type.Warning)
                                    }
                                }
                            }
                            
                            QDDivider {
                                Layout.fillWidth: true
                            }
                            
                            Text {
                                text: "MessageBox 消息框"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }
                            
                            Row {
                                spacing: Theme.spacingMedium
                                
                                QDButton {
                                    text: "信息"
                                    buttonType: QDButton.Type.Primary
                                    onClicked: {
                                        globalMessageBox.title = "信息"
                                        globalMessageBox.message = "这是一条信息消息"
                                        globalMessageBox.messageType = QDMessageBox.Type.Information
                                        globalMessageBox.buttons = QDMessageBox.Buttons.Ok
                                        globalMessageBox.show()
                                    }
                                }
                                
                                QDButton {
                                    text: "警告"
                                    buttonType: QDButton.Type.Secondary
                                    onClicked: {
                                        globalMessageBox.title = "警告"
                                        globalMessageBox.message = "这是一条警告消息"
                                        globalMessageBox.detailMessage = "请注意检查相关设置"
                                        globalMessageBox.messageType = QDMessageBox.Type.Warning
                                        globalMessageBox.buttons = QDMessageBox.Buttons.OkCancel
                                        globalMessageBox.show()
                                    }
                                }
                                
                                QDButton {
                                    text: "错误"
                                    buttonType: QDButton.Type.Danger
                                    onClicked: {
                                        globalMessageBox.title = "错误"
                                        globalMessageBox.message = "操作失败"
                                        globalMessageBox.messageType = QDMessageBox.Type.Error
                                        globalMessageBox.buttons = QDMessageBox.Buttons.Ok
                                        globalMessageBox.show()
                                    }
                                }
                                
                                QDButton {
                                    text: "询问"
                                    buttonType: QDButton.Type.Secondary
                                    onClicked: {
                                        globalMessageBox.title = "确认操作"
                                        globalMessageBox.message = "确定要删除这些文件吗？"
                                        globalMessageBox.messageType = QDMessageBox.Type.Question
                                        globalMessageBox.buttons = QDMessageBox.Buttons.YesNo
                                        globalMessageBox.accepted.connect(function() {
                                            globalToast.show("已确认删除", QDToast.Type.Success)
                                        })
                                        globalMessageBox.show()
                                    }
                                }
                                
                                QDButton {
                                    text: "成功"
                                    buttonType: QDButton.Type.Success
                                    onClicked: {
                                        globalMessageBox.title = "操作成功"
                                        globalMessageBox.message = "所有文件已成功上传"
                                        globalMessageBox.messageType = QDMessageBox.Type.Success
                                        globalMessageBox.buttons = QDMessageBox.Buttons.Ok
                                        globalMessageBox.show()
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Footer
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    
                    Text {
                        anchors.centerIn: parent
                        text: "QuickDesk © 2026 - Modern Fluent Design"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }
}
}