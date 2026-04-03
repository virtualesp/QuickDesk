# QuickDesk MCP — 典型场景 Demo

本文档展示 AI Agent 使用 QuickDesk MCP 工具的典型工作流。每个场景展示完整的工具调用序列和预期行为。

> **模式升级说明**：本文档已更新为 Phase 5A 事件驱动写法，不再依赖重复截图轮询。核心替换规则：
> - `connect_device` 后 → `wait_for_connection_state`，不截图
> - 读取屏幕文字 → `get_ui_state` / `get_screen_text`，替代截图+视觉模型
> - 找元素并点击 → `find_element` + `click_text`，替代截图→坐标→`mouse_click`
> - 等待 UI 响应 → `wait_for_text` / `wait_for_screen_change`，替代截图轮询
> - 验证操作结果 → `verify_action_result`，替代截图+人工判断

---

## 场景 1：AI 修电脑 — 远程故障诊断

**目标**：连接到一台运行缓慢的 Windows 电脑，诊断根因并给出报告。

**Prompt**：`diagnose_system_issue`

> **用户**：我的工作电脑今天特别卡。设备 ID：999888777，访问码：123456。

### 工具调用序列

```
步骤 1: connect_device
  → device_id: "999888777", access_code: "123456", show_window: true
  ← { deviceId: "999888777" }

步骤 2: wait_for_connection_state          ← 事件驱动，无需截图
  → device_id: "999888777", state: "connected", timeout_ms: 15000
  ← { event: "connectionStateChanged", data: { state: "connected" } }

步骤 3: get_ui_state                       ← 结构化状态，无需视觉模型
  → device_id: "999888777"
  ← { screen: {width:1920,height:1080}, activeWindow: {title:"桌面"},
      ocr: { blocks: [...] } }

步骤 4: keyboard_hotkey                    ← 打开任务管理器
  → device_id: "999888777", keys: ["ctrl","shift","esc"]

步骤 5: wait_for_text                      ← 等待任务管理器出现，不截图
  → device_id: "999888777", text: "任务管理器", timeout_ms: 5000
  ← { found: true, match: { text: "任务管理器", center: {x:960,y:20} } }

步骤 6: get_ui_state                       ← 通过 OCR 读取 CPU/内存数值
  ← { ocr: { blocks: [
       {text:"CPU 92%", ...}, {text:"内存 15.2/16 GB", ...},
       {text:"磁盘 100%", ...}
     ]}}

步骤 7: click_text                         ← 按内存排序，无需猜坐标
  → device_id: "999888777", text: "内存"
  ← { success: true, clickedText: "内存", x: 480, y: 340 }

步骤 8: wait_for_screen_change             ← 等待列表重排完成
  → device_id: "999888777", timeout_ms: 3000
  ← { event: "screenChanged", data: { frameHash: "a1b2c3..." } }

步骤 9: get_screen_text                    ← OCR 读取进程内存占用
  → device_id: "999888777"
  ← { blocks: [
       {text:"chrome.exe"}, {text:"8.3 GB"},
       {text:"Teams.exe"},   {text:"2.1 GB"},
       {text:"outlook.exe"}, {text:"1.8 GB"}
     ]}

步骤 10: click_text → device_id: "999888777", text: "磁盘"

步骤 11: wait_for_screen_change → device_id: "999888777", timeout_ms: 3000

步骤 12: get_screen_text                   ← 读取磁盘占用排行
  → device_id: "999888777"
  ← { blocks: [
       {text:"SearchIndexer.exe"}, {text:"100%"},
       {text:"MsMpEng.exe"},       {text:"50%"}
     ]}

步骤 13: disconnect_device → device_id: "999888777"
```

**截图次数**：0（旧版工作流需 8 次）

**AI 报告**：
> 找到三个根本原因：
> - **内存**：Chrome 占用 8.3 GB（总内存 52%），可能有 40+ 个标签页
> - **磁盘**：Windows 搜索索引器占 100% 磁盘 I/O，正在爆发索引
> - **CPU**：Chrome + Teams + Outlook 合计占用 92%
>
> 建议：关闭 Chrome 多余标签页、重启 SearchIndexer 服务、重启 Teams。

---

## 场景 2：AI 巡检服务器 — 批量磁盘检查（3 台）

**目标**：依次连接 3 台服务器，检查磁盘用量，生成汇总报告。

**Prompt**：`batch_operation`

> **用户**：检查以下 3 台服务器的磁盘空间：
> - 服务器 A：ID 111222333，访问码 111111
> - 服务器 B：ID 444555666，访问码 222222
> - 服务器 C：ID 777888999，访问码 333333

### 工具调用序列（每台重复）

```
── 服务器 A ──────────────────────────────────────────────

步骤 1: connect_device
  → device_id: "111222333", access_code: "111111", show_window: false
  ← { deviceId: "111222333" }

步骤 2: wait_for_connection_state
  → device_id: "111222333", state: "connected", timeout_ms: 20000
  ← 已连接

步骤 3: keyboard_hotkey → keys: ["win","r"]

步骤 4: wait_for_text → text: "运行", timeout_ms: 3000

步骤 5: keyboard_type → text: "powershell"

步骤 6: keyboard_hotkey → keys: ["enter"]

步骤 7: wait_for_text
  → text: "PS C:\\", timeout_ms: 8000     ← 等待 PS 提示符，不截图轮询
  ← { found: true }

步骤 8: keyboard_type
  → text: "Get-PSDrive C,D | Select Name,Used,Free | Format-Table -Auto"

步骤 9: keyboard_hotkey → keys: ["enter"]

步骤 10: wait_for_screen_change            ← 等待命令输出刷新
  → device_id: "111222333", timeout_ms: 10000

步骤 11: get_screen_text                   ← OCR 读取磁盘数值
  ← { blocks: [
       {text:"C"}, {text:"95.2 GB"}, {text:"4.1 GB"},
       {text:"D"}, {text:"200 GB"},  {text:"120 GB"}
     ]}

步骤 12: verify_action_result              ← 验证确实拿到了数据
  → expectations: [{"type":"text_present","value":"GB"}]
  ← { allPassed: true }

步骤 13: disconnect_device → device_id: "111222333"

── 服务器 B、C 重复以上流程 ──────────────────────────────
```

**每台截图次数**：0（旧版每台需 6+ 次）

**AI 报告**：

| 服务器 | ID | C: 剩余 | D: 剩余 | 状态 |
|--------|-----|--------|--------|------|
| 服务器 A | 111222333 | 4.1 GB / 99.3 GB | 120 GB / 320 GB | ⚠️ C 盘 96% 满 |
| 服务器 B | 444555666 | 45 GB / 120 GB | — | ✅ 正常 |
| 服务器 C | 777888999 | 12 GB / 120 GB | 80 GB / 200 GB | ⚠️ C 盘 90% 满 |

> **需要处理**：服务器 A 和 C 的 C 盘需要清理。

---

## 场景 3：AI 操作老旧 ERP — 无 API 的桌面软件自动化

**目标**：打开传统桌面 ERP，导航到导出页面，导出月度销售报表。没有开放 API，完全通过 GUI 操作。

**Prompt**：`operate_remote_desktop`

> **用户**：帮我从 ERP 系统里导出 6 月的销售报表。设备 ID: 555444333，访问码: 999111。

### 工具调用序列

```
步骤 1: connect_device
  → device_id: "555444333", access_code: "999111", show_window: true
  ← { deviceId: "555444333" }

步骤 2: wait_for_connection_state → state: "connected"

步骤 3: get_ui_state                       ← 了解当前屏幕，不截图
  ← { activeWindow: {title:"桌面"},
      ocr: { blocks: [{text:"ERP System"}, ...] } }

步骤 4: find_element → text: "ERP"         ← 定位 ERP 图标
  ← { found: true, matches: [{text:"ERP System", center:{x:320,y:480}}] }

步骤 5: mouse_double_click → x: 320, y: 480

步骤 6: wait_for_text → text: "登录", timeout_ms: 10000

步骤 7: find_element → text: "用户名"
  ← { found: true, matches: [{center:{x:460,y:310}}] }

步骤 8: mouse_click → x: 560, y: 310

步骤 9: keyboard_type → text: "admin"

步骤 10: find_element → text: "密码"

步骤 11: mouse_click → x: 560, y: 355

步骤 12: keyboard_type → text: "password123"

步骤 13: click_text → text: "登录", exact: true   ← 一步完成查找+点击
  ← { success: true, clickedText: "登录", x: 520, y: 400 }

步骤 14: wait_for_text → text: "主菜单", timeout_ms: 15000

步骤 15: click_text → text: "报表"

步骤 16: wait_for_text → text: "销售报表", timeout_ms: 5000

步骤 17: click_text → text: "销售报表"

步骤 18: wait_for_text → text: "导出", timeout_ms: 5000

步骤 19: screen_diff_summary               ← 记录选择月份前的基准状态
  → from_hash: ""
  ← { frameHash: "baseline_hash_xxx" }

步骤 20: find_element → text: "月份"
  ← { found: true, matches: [{center:{x:350,y:280}}] }

步骤 21: mouse_click → x: 430, y: 280     ← 点开月份下拉框

步骤 22: wait_for_text → text: "六月", timeout_ms: 3000

步骤 23: click_text → text: "六月"

步骤 24: screen_diff_summary               ← 验证月份已切换
  → from_hash: "baseline_hash_xxx"
  ← { added: [{text:"六月"}], removed: [{text:"本月"}], changed: true }

步骤 25: click_text → text: "导出"

步骤 26: wait_for_text
  → text: "导出成功", timeout_ms: 30000   ← 等最多 30s，不轮询截图
  ← { found: true }

步骤 27: verify_action_result              ← 结构化确认
  → expectations: [{"type":"text_present","value":"导出成功"}]
  ← { allPassed: true }

步骤 28: get_clipboard → 获取导出文件路径
  ← { text: "C:\\Reports\\Sales_June_2026.xlsx" }

步骤 29: disconnect_device
```

**截图次数**：0（旧版需 20+ 次）
**click_text 替代坐标猜测**：8 处

**AI 总结**：
> 导出完成。文件已保存至远程机器 `C:\Reports\Sales_June_2026.xlsx`。
> 共 29 步，已完成登录→导航→月份选择→导出全流程验证。

---

## 场景 4：retry_with_alternative — OCR 识别失败兜底示例

**目标**：演示当 OCR 误识别按钮文字时，用备选策略自动兜底。

```
# 主策略：精确匹配点击"保存"
# 备选 1：模糊匹配（应对 OCR 将"保存"识别为"保 存"的情况）
# 备选 2：键盘快捷键 Ctrl+S（与 UI 状态无关，总是有效）

步骤: retry_with_alternative
  → device_id: "123456789"
  → attempts: [
      { method: "clickText",      params: { text: "保存", exact: true } },
      { method: "clickText",      params: { text: "保存", exact: false } },
      { method: "keyboardHotkey", params: { keys: ["ctrl","s"] } }
    ]
  → success_conditions: [{ type: "text_absent", value: "未保存" }]
  → timeout_ms: 2000
  ← {
      success: true,
      attemptIndex: 2,           ← Ctrl+S 在前两次 OCR 尝试失败后成功
      method: "keyboardHotkey",
      triedAttempts: 3
    }
```

---

## 场景 5：跨设备文件传输

**目标**：将办公电脑桌面的报告复制到家里电脑的文档目录。

**Prompt**：`multi_device_workflow`

> **用户**：把办公电脑（111111111 / 999999）桌面的"Q4_Report.xlsx"复制到家里电脑（222222222 / 888888）。

```
步骤 1: connect_device("111111111", "999999", show_window=true) → { deviceId: "111111111" }
步骤 2: wait_for_connection_state → device_id: "111111111", state: "connected"

步骤 3: connect_device("222222222", "888888", show_window=true) → { deviceId: "222222222" }
步骤 4: wait_for_connection_state → device_id: "222222222", state: "connected"

── 办公电脑 ──────────────────────────────────────────────

步骤 5: find_element → device_id: "111111111", text: "Q4_Report"
  ← { found: true, matches: [{center:{x:150,y:320}}] }

步骤 6: mouse_click → device_id: "111111111", button: "right", x: 150, y: 320

步骤 7: wait_for_text → device_id: "111111111", text: "复制", timeout_ms: 2000

步骤 8: click_text → device_id: "111111111", text: "复制为路径"

步骤 9: wait_for_clipboard_change → device_id: "111111111", timeout_ms: 3000
  ← { data: { text: "\"C:\\Users\\user\\Desktop\\Q4_Report.xlsx\"" } }

步骤 10: keyboard_hotkey → device_id: "111111111", keys: ["win","r"]
步骤 11: wait_for_text   → device_id: "111111111", text: "运行", timeout_ms: 3000
步骤 12: keyboard_type   → device_id: "111111111", text: "powershell"
步骤 13: keyboard_hotkey → device_id: "111111111", keys: ["enter"]
步骤 14: wait_for_text   → device_id: "111111111", text: "PS C:\\", timeout_ms: 8000

步骤 15: keyboard_type
  → device_id: "111111111", text: "Copy-Item 'C:\\Users\\user\\Desktop\\Q4_Report.xlsx' '\\\\home-pc\\shared\\'"
步骤 16: keyboard_hotkey → device_id: "111111111", keys: ["enter"]
步骤 17: wait_for_screen_change → device_id: "111111111", timeout_ms: 15000

── 家里电脑 ──────────────────────────────────────────────

步骤 18: keyboard_hotkey → device_id: "222222222", keys: ["win","e"]
步骤 19: wait_for_text   → device_id: "222222222", text: "文件资源管理器", timeout_ms: 5000
步骤 20: find_element    → device_id: "222222222", text: "Q4_Report"
  ← { found: true }

步骤 21: verify_action_result
  → device_id: "222222222", expectations: [{"type":"text_present","value":"Q4_Report"}]
  ← { allPassed: true }

步骤 22: disconnect_device(device_id: "111111111")
步骤 23: disconnect_device(device_id: "222222222")
```

---

## 场景 6：屏幕安全审计

**目标**：AI 分析远程桌面上是否有敏感信息暴露。

**Prompt**：`analyze_screen_content`

> **用户**：审计一下开发工作站上是否有敏感信息暴露（ID: 555666777，访问码: 444444）。

```
步骤 1: connect_device → device_id: "555666777", access_code: "444444"
步骤 2: wait_for_connection_state → device_id: "555666777", state: "connected"

步骤 3: screenshot（全分辨率，不加 max_width）
  → device_id: "555666777"
  ← [图片：完整桌面截图，保留所有文字细节]

步骤 4: get_screen_text                    ← OCR 提取全屏文字，便于精确检索
  → device_id: "555666777"
  ← { blocks: [所有可见文字块] }

  （AI 结合图片和文字块进行安全分析）
```

**AI 报告**：

> ## 屏幕内容安全分析
>
> **操作系统**：Windows 11，深色主题
> **打开的应用**：VS Code、Chrome（3 个标签页）、Slack、终端
>
> ### ⚠️ 警告 — 检测到敏感信息
>
> 1. **VS Code**（活动窗口）：`.env` 文件已打开
>    - 第 3 行：`DATABASE_URL=postgres://admin:P@ssw0rd123@prod-db.company.com:5432/main`
>    - 第 5 行：`AWS_SECRET_KEY=AKIA...已脱敏...`
>    - **风险**：数据库凭据和 AWS 密钥以明文可见
>
> 2. **Chrome 标签页**："AWS Console" — 以 root 用户登录
>    - **风险**：使用 root 账户违反最小权限原则
>
> 3. **终端**：`git log` 显示提交信息 "added api key"
>    - **风险**：密钥可能已提交进版本控制
>
> **建议**：立即轮换数据库密码和 AWS 密钥；使用密钥管理器；配置 pre-commit hook。

---

## 快速参考

### 工具选择指南

| 场景 | 用这个工具 | 替代的旧做法 |
|------|-----------|------------|
| `connect_device` 之后 | `wait_for_connection_state` | 截图看桌面 |
| 读取屏幕文字/布局 | `get_ui_state` | 截图+视觉模型 |
| 查找 UI 元素 | `find_element` | 截图估坐标 |
| 查找并点击 | `click_text` | 截图→找元素→`mouse_click` |
| 等待 UI 响应 | `wait_for_text` | 循环截图 |
| 等待任何视觉变化 | `wait_for_screen_change` | 循环截图 |
| 验证操作是否成功 | `verify_action_result` | 截图+人工判断 |
| 前后对比 | `screen_diff_summary` | 两次截图手动比对 |
| 操作可能失败，有兜底 | `retry_with_alternative` | 手动 if/else 重试 |
| 确实需要看图 | `screenshot` | —（AI 需要视觉判断时使用） |

### Prompt → 场景对照

| 场景 | Prompt | 核心新工具 |
|------|--------|----------|
| 服务器健康检查 / 修电脑 | `diagnose_system_issue` | `get_ui_state`, `wait_for_text`, `get_screen_text` |
| 批量设备巡检 | `batch_operation` | `wait_for_connection_state`, `wait_for_screen_change` |
| 老旧 GUI 软件操作 | `operate_remote_desktop` | `click_text`, `find_element`, `screen_diff_summary` |
| 操作后验证 | 任意 | `verify_action_result`, `assert_screen_state` |
| 剪贴板跨设备传输 | `multi_device_workflow` | `wait_for_clipboard_change` |

### 截图使用量对比

| 指标 | 旧模式 | Phase 5A 新模式 |
|------|--------|----------------|
| 10 步任务的截图次数 | 8–12 次 | 0–2 次 |
| Token 消耗 | 高（图片 token） | 低（文字 token） |
| 元素定位方式 | 视觉坐标 | `find_element` / `click_text` OCR |
| 操作结果验证 | 人工看截图 | `verify_action_result` 结构化返回 |
