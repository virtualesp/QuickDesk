# QuickDesk 组件库

## 📊 组件总览（22个组件）

### 🎨 基础组件 (2)
- **Theme** - 主题管理器（6种主题）
- **FluentIconGlyph** - Fluent 图标库

### 🔘 按钮与输入 (7)
- **QDButton** - 按钮
- **QDTextField** - 文本输入框
- **QDCheckBox** - 复选框
- **QDRadioButton** - 单选按钮 ⭐ 新增
- **QDSwitch** - 开关
- **QDSlider** - 滑块
- **QDComboBox** - 下拉选择框

### 📦 容器与布局 (3)
- **QDCard** - 卡片容器
- **QDDialog** - 对话框
- **QDDivider** - 分割线

### 💬 反馈与提示 (6)
- **QDToast** - 消息提示
- **QDMessageBox** - 消息框 ⭐ 新增
- **QDProgressBar** - 进度条
- **QDBadge** - 徽章
- **QDToolTip** - 工具提示
- **QDSpinner** - 加载动画 ⭐ 新增

### 🎭 数据展示 (4)
- **QDAvatar** - 头像 ⭐ 新增
- **QDChip** - 标签芯片 ⭐ 新增
- **QDMenu** - 菜单 ⭐ 新增

---

## 🆕 新增组件详解

### 1. QDRadioButton - 单选按钮

```qml
ButtonGroup {
    id: radioGroup
}

QDRadioButton {
    text: "选项 1"
    checked: true
    ButtonGroup.group: radioGroup
}

QDRadioButton {
    text: "选项 2"
    ButtonGroup.group: radioGroup
}
```

**特性：**
- ✅ 圆形选择器，选中时内圈动画
- ✅ 点击波纹效果
- ✅ 悬停状态
- ✅ 禁用状态

---

### 2. QDSpinner - 加载动画

```qml
QDSpinner {
    size: 32
    spinnerColor: Theme.primary
    running: true
}
```

**属性：**
| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `size` | int | 32 | 尺寸（像素） |
| `spinnerColor` | color | primary | 颜色 |
| `duration` | int | 1000 | 动画周期（毫秒） |
| `running` | bool | true | 是否运行 |

**特性：**
- ✅ 8点圆形加载动画
- ✅ 渐隐渐显效果
- ✅ 可自定义颜色和大小

---

### 3. QDAvatar - 头像

```qml
QDAvatar {
    name: "Zhang San"
    avatarSize: QDAvatar.Size.Medium
    backgroundColor: Theme.primary
    showBadge: true
    badgeColor: Theme.success
}
```

**属性：**
| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `avatarSize` | enum | Medium | 尺寸 |
| `imageSource` | string | "" | 头像图片 |
| `name` | string | "" | 姓名（生成首字母） |
| `backgroundColor` | color | primary | 背景色 |
| `textColor` | color | textOnPrimary | 文字色 |
| `showBadge` | bool | false | 显示状态徽章 |
| `badgeColor` | color | success | 徽章颜色 |

**尺寸枚举：**
- `QDAvatar.Size.Small` - 24x24
- `QDAvatar.Size.Medium` - 32x32
- `QDAvatar.Size.Large` - 48x48
- `QDAvatar.Size.XLarge` - 64x64

**特性：**
- ✅ 支持图片或首字母显示
- ✅ 4种尺寸
- ✅ 圆形头像
- ✅ 状态徽章（在线/离线）
- ✅ 自动生成姓名首字母

---

### 4. QDChip - 标签芯片

```qml
QDChip {
    text: "标签"
    chipType: QDChip.Type.Primary
    iconText: FluentIconGlyph.checkMarkGlyph
    closable: true
    outlined: false
    onCloseClicked: console.log("关闭")
}
```

**属性：**
| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `chipType` | enum | Default | 标签类型 |
| `text` | string | "" | 文本 |
| `iconText` | string | "" | 图标 |
| `closable` | bool | false | 可关闭 |
| `outlined` | bool | false | 轮廓模式 |

**类型枚举：**
- `QDChip.Type.Default` - 默认
- `QDChip.Type.Primary` - 主要
- `QDChip.Type.Success` - 成功
- `QDChip.Type.Warning` - 警告
- `QDChip.Type.Error` - 错误
- `QDChip.Type.Info` - 信息

**信号：**
- `clicked()` - 点击标签
- `closeClicked()` - 点击关闭按钮

**特性：**
- ✅ 6种颜色类型
- ✅ 可添加图标
- ✅ 可关闭
- ✅ 轮廓模式
- ✅ 悬停缩放效果

---

### 5. QDMenu - 菜单

```qml
QDButton {
    text: "打开菜单"
    onClicked: menu.popup()
    
    QDMenu {
        id: menu
        
        Controls.MenuItem {
            text: "新建"
            onTriggered: console.log("新建")
        }
        
        Controls.MenuSeparator { }
        
        Controls.MenuItem {
            text: "保存"
            checkable: true
        }
    }
}
```

**特性：**
- ✅ Fluent Design 样式
- ✅ 支持勾选项
- ✅ 支持子菜单
- ✅ 分隔线
- ✅ 悬停高亮
- ✅ 平滑动画

---

### 6. QDMessageBox - 消息框

```qml
QDMessageBox {
    id: messageBox
    title: "确认操作"
    message: "确定要删除这些文件吗？"
    detailMessage: "此操作不可撤销"
    messageType: QDMessageBox.Type.Question
    buttons: QDMessageBox.Buttons.YesNo
    
    onAccepted: console.log("已确认")
    onRejected: console.log("已取消")
}

// 使用
messageBox.show()
// 或
messageBox.showMessage("消息", QDMessageBox.Type.Information, QDMessageBox.Buttons.Ok)
```

**属性：**
| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `messageType` | enum | Information | 消息类型 |
| `buttons` | enum | Ok | 按钮组合 |
| `title` | string | "提示" | 标题 |
| `message` | string | "" | 消息内容 |
| `detailMessage` | string | "" | 详细信息 |
| `showing` | bool | false | 显示状态 |

**消息类型：**
- `QDMessageBox.Type.Information` - 信息（蓝色图标）
- `QDMessageBox.Type.Warning` - 警告（橙色图标）
- `QDMessageBox.Type.Error` - 错误（红色图标）
- `QDMessageBox.Type.Question` - 询问（蓝色图标）
- `QDMessageBox.Type.Success` - 成功（绿色图标）

**按钮组合：**
- `QDMessageBox.Buttons.Ok` - 仅确定
- `QDMessageBox.Buttons.OkCancel` - 确定和取消
- `QDMessageBox.Buttons.YesNo` - 是和否
- `QDMessageBox.Buttons.YesNoCancel` - 是、否和取消

**方法：**
- `show()` - 显示消息框
- `hide()` - 隐藏消息框
- `showMessage(msg, type, buttons)` - 快速显示

**信号：**
- `accepted()` - 点击确定/是
- `rejected()` - 点击取消
- `yesClicked()` - 点击是
- `noClicked()` - 点击否
- `closed()` - 关闭时触发

**特性：**
- ✅ 5种消息类型（不同图标和颜色）
- ✅ 4种按钮组合
- ✅ 支持详细信息
- ✅ 模态遮罩
- ✅ 平滑动画
- ✅ 自动图标匹配

---

## 🎨 主题系统

### 6种内置主题

| 主题 | 风格 | 强调色 |
|------|------|--------|
| Fluent Dark | 深色 | 蓝色 #0078D4 |
| Fluent Light | 浅色 | 蓝色 #0078D4 |
| Nord Dark | 深色 | 青色 #88C0D0 |
| Dracula | 深色 | 紫色 #BD93F9 |
| Monokai | 深色 | 绿色 #A6E22E |
| Solarized Light | 浅色 | 橙色 #CB4B16 |

### 主题切换

```qml
Theme.currentTheme = Theme.ThemeType.DraculaDark
```

---

## 📁 文件结构

```
qml/component/
├── Theme.qml              # 主题管理器
├── FluentIconGlyph.qml    # 图标库
├── QDButton.qml           
├── QDTextField.qml        
├── QDCheckBox.qml         
├── QDRadioButton.qml      ⭐ 新增
├── QDSwitch.qml           
├── QDSlider.qml           
├── QDComboBox.qml         
├── QDCard.qml             
├── QDDialog.qml           
├── QDMessageBox.qml       ⭐ 新增
├── QDToast.qml            
├── QDProgressBar.qml      
├── QDBadge.qml            
├── QDChip.qml             ⭐ 新增
├── QDDivider.qml          
├── QDToolTip.qml          
├── QDSpinner.qml          ⭐ 新增
├── QDAvatar.qml           ⭐ 新增
├── QDMenu.qml             ⭐ 新增
├── qmldir                 
└── README.md              
```

---

## 🚀 使用示例

完整示例请查看 `qml/Example.qml`，包含所有组件的演示。

运行项目后可以：
- ✅ 在顶部切换 6 种主题
- ✅ 测试所有 22 个组件
- ✅ 查看各种交互效果
- ✅ 学习组件用法

---

## 📝 开发指南

### 导入组件

```qml
import "component"
```

### 使用组件

所有组件都以 `QD` 前缀命名，避免与 Qt 原生组件冲突。

### 主题定制

修改 `Theme.qml` 中的颜色配置即可自定义主题。

---

**QuickDesk © 2026 - Modern Fluent Design System**
