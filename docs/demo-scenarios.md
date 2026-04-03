# QuickDesk MCP — Demo Scenarios

This document demonstrates typical AI agent workflows using QuickDesk MCP tools.
Each scenario shows the exact sequence of tool calls and their expected results,
following the **event-driven** pattern introduced in Phase 5A.

All tool calls that target a remote session use **`device_id`** (MCP snake_case) / **`deviceId`** (JSON). The value is the stable device identifier returned by **`connect_device`**—the same id you used to connect—**not** a separate connection handle.

> **Pattern change from legacy docs**: We no longer poll with repeated `screenshot`
> after every action. Instead, we use:
> - `wait_for_connection_state` — after `connect_device`
> - `get_ui_state` / `find_element` — instead of screenshot+vision for text recognition
> - `wait_for_text` / `wait_for_screen_change` — after actions that trigger UI changes
> - `verify_action_result` — structured success/failure check with timeout
> - `screen_diff_summary` — before/after comparison instead of manual screenshot analysis
> - `click_text` — find-and-click in one call instead of screenshot→coordinates→click

---

## Scenario 1: AI 修电脑 — Remote Health Check & Troubleshooting

**Goal**: Connect to a slow Windows PC, diagnose the issue, and report findings.

**Prompt**: `diagnose_system_issue`

> **User**: My work computer is super slow today. Device ID: 999888777, code: 123456.

### Tool Call Sequence

```
Step 1: connect_device
  → device_id: "999888777", access_code: "123456", show_window: true
  ← { deviceId: "999888777" }

Step 2: wait_for_connection_state          ← event-driven, no screenshot needed
  → device_id: "999888777", state: "connected", timeout_ms: 15000
  ← { event: "connectionStateChanged", data: { state: "connected" } }

Step 3: get_ui_state                       ← structured state, no vision model needed
  → device_id: "999888777"
  ← { screen: {width:1920,height:1080}, activeWindow: {title:"Desktop"},
      ocr: { blocks: [...] } }

Step 4: keyboard_hotkey                    ← open Task Manager
  → device_id: "999888777", keys: ["ctrl","shift","esc"]

Step 5: wait_for_text                      ← wait for Task Manager, not screenshot
  → device_id: "999888777", text: "Task Manager", timeout_ms: 5000
  ← { found: true, match: { text: "Task Manager", center: {x:960,y:20} } }

Step 6: get_ui_state                       ← read memory/CPU values via OCR
  → device_id: "999888777"
  ← { ocr: { blocks: [
       {text:"CPU 92%", ...}, {text:"Memory 15.2/16 GB", ...},
       {text:"Disk 100%", ...}
     ]}}

Step 7: click_text                         ← sort by Memory, no coordinate guessing
  → device_id: "999888777", text: "Memory"
  ← { success: true, clickedText: "Memory", x: 480, y: 340 }

Step 8: wait_for_screen_change             ← wait for list to re-sort
  → device_id: "999888777", timeout_ms: 3000
  ← { event: "screenChanged", data: { frameHash: "a1b2c3..." } }

Step 9: get_screen_text                    ← read top processes by memory
  → device_id: "999888777"
  ← { blocks: [
       {text:"chrome.exe"}, {text:"8.3 GB"},
       {text:"Teams.exe"},   {text:"2.1 GB"},
       {text:"outlook.exe"}, {text:"1.8 GB"}
     ]}

Step 10: click_text
  → device_id: "999888777", text: "Disk"

Step 11: wait_for_screen_change
  → device_id: "999888777", timeout_ms: 3000

Step 12: get_screen_text                   ← read top disk consumers
  → device_id: "999888777"
  ← { blocks: [
       {text:"SearchIndexer.exe"}, {text:"100%"},
       {text:"MsMpEng.exe"},       {text:"50%"}
     ]}

Step 13: disconnect_device
  → device_id: "999888777"
```

**Screenshot count**: 0 (down from 8 in legacy workflow)
**get_ui_state / get_screen_text calls**: 3

**AI Report**:
> Three root causes found:
> - **Memory**: Chrome 8.3 GB (52% of total RAM), likely 40+ tabs open
> - **Disk**: Windows Search Indexer at 100% disk I/O — indexing storm
> - **CPU**: 92% load from Chrome + Teams + Outlook combined
>
> Recommended: close Chrome tabs, restart SearchIndexer service, restart Teams.

---

## Scenario 2: AI 巡检服务器 — Batch Server Inspection (3 Devices)

**Goal**: Connect to 3 servers sequentially, check disk usage, generate summary report.

**Prompt**: `batch_operation`

> **User**: Check disk space on these 3 servers:
> - Server A: ID 111222333, code 111111
> - Server B: ID 444555666, code 222222
> - Server C: ID 777888999, code 333333

### Tool Call Sequence (repeated per server)

```
── Server A ──────────────────────────────────────────────

Step 1: connect_device
  → device_id: "111222333", access_code: "111111", show_window: false
  ← { deviceId: "111222333" }

Step 2: wait_for_connection_state
  → device_id: "111222333", state: "connected", timeout_ms: 20000
  ← connected

Step 3: keyboard_hotkey  → device_id: "111222333", keys: ["win","r"]

Step 4: wait_for_text
  → device_id: "111222333", text: "Run", timeout_ms: 3000
  ← { found: true }

Step 5: keyboard_type  → device_id: "111222333", text: "powershell"

Step 6: keyboard_hotkey  → device_id: "111222333", keys: ["enter"]

Step 7: wait_for_text
  → device_id: "111222333", text: "PS C:\\", timeout_ms: 8000     ← wait for PS prompt, no screenshot poll
  ← { found: true }

Step 8: keyboard_type
  → device_id: "111222333", text: "Get-PSDrive C,D | Select Name,Used,Free | Format-Table -Auto"

Step 9: keyboard_hotkey  → device_id: "111222333", keys: ["enter"]

Step 10: wait_for_screen_change            ← wait for command output to appear
  → device_id: "111222333", timeout_ms: 10000
  ← { event: "screenChanged", data: { frameHash: "..." } }

Step 11: get_screen_text                   ← read disk values via OCR
  → device_id: "111222333"
  ← { blocks: [
       {text:"C"}, {text:"95.2 GB"}, {text:"4.1 GB"},
       {text:"D"}, {text:"200 GB"},  {text:"120 GB"}
     ]}

Step 12: verify_action_result              ← confirm we got real data (not error)
  → device_id: "111222333", expectations: [{"type":"text_present","value":"GB"}]
  ← { allPassed: true }

Step 13: disconnect_device → device_id: "111222333"

── Repeat for Server B and Server C ──────────────────────
```

**Screenshot count**: 0 per server (down from 6+ in legacy workflow)

**AI Report**:

| Server | ID | C: Free | D: Free | Status |
|--------|-----|---------|---------|--------|
| Server A | 111222333 | 4.1 GB / 99.3 GB | 120 GB / 320 GB | ⚠️ C: 96% full |
| Server B | 444555666 | 45 GB / 120 GB | — | ✅ Normal |
| Server C | 777888999 | 12 GB / 120 GB | 80 GB / 200 GB | ⚠️ C: 90% full |

> **Action required**: Server A and Server C disk C: need cleanup.

---

## Scenario 3: AI 操作老旧 ERP — Legacy Software UI Automation

**Goal**: Open a legacy desktop ERP, navigate to the export screen, and export a monthly report.
No API available — must operate entirely through the GUI.

**Prompt**: `operate_remote_desktop`

> **User**: Export the June sales report from our ERP system.
> Device ID: 555444333, code: 999111.

### Tool Call Sequence

```
Step 1: connect_device
  → device_id: "555444333", access_code: "999111", show_window: true
  ← { deviceId: "555444333" }

Step 2: wait_for_connection_state
  → state: "connected", timeout_ms: 20000

Step 3: get_ui_state                       ← understand current screen without screenshot
  ← { activeWindow: {title: "Desktop"},
      ocr: { blocks: [{text:"ERP System"}, {text:"双击打开"},...] } }

Step 4: find_element                       ← locate ERP icon
  → text: "ERP", exact: false
  ← { found: true, matches: [{text:"ERP System", center:{x:320,y:480}}] }

Step 5: mouse_double_click
  → device_id: "555444333", x: 320, y: 480

Step 6: wait_for_text
  → text: "登录", timeout_ms: 10000         ← wait for login dialog
  ← { found: true }

Step 7: find_element  → text: "用户名"
  ← { found: true, matches: [{center:{x:460,y:310}}] }

Step 8: mouse_click  → x: 560, y: 310      ← click username input field

Step 9: keyboard_type  → text: "admin"

Step 10: find_element  → text: "密码"
  ← { found: true, matches: [{center:{x:460,y:355}}] }

Step 11: mouse_click  → x: 560, y: 355

Step 12: keyboard_type  → text: "password123"

Step 13: click_text                        ← find and click login button in one call
  → text: "登录", exact: true
  ← { success: true, clickedText: "登录", x: 520, y: 400 }

Step 14: wait_for_text
  → text: "主菜单", timeout_ms: 15000       ← wait for main menu after login
  ← { found: true }

Step 15: click_text  → text: "报表"         ← navigate to Reports menu
  ← { success: true }

Step 16: wait_for_text  → text: "销售报表", timeout_ms: 5000

Step 17: click_text  → text: "销售报表"

Step 18: wait_for_text  → text: "导出", timeout_ms: 5000

Step 19: screen_diff_summary               ← capture baseline before date selection
  → from_hash: ""
  ← { frameHash: "baseline_hash_xxx" }

Step 20: find_element  → text: "月份"
  ← { found: true, matches: [{center:{x:350,y:280}}] }

Step 21: mouse_click  → x: 430, y: 280    ← click month dropdown

Step 22: wait_for_text  → text: "六月", timeout_ms: 3000

Step 23: click_text  → text: "六月"

Step 24: screen_diff_summary               ← verify month selection changed
  → from_hash: "baseline_hash_xxx"
  ← { added: [{text:"六月"}], removed: [{text:"本月"}], changed: true }

Step 25: click_text  → text: "导出"

Step 26: wait_for_text
  → text: "导出成功", timeout_ms: 30000     ← wait up to 30s for export to finish
  ← { found: true }

Step 27: verify_action_result              ← structured confirmation
  → expectations: [{"type":"text_present","value":"导出成功"}], timeout_ms: 2000
  ← { allPassed: true }

Step 28: get_clipboard                     ← get the export file path if copied
  → device_id: "555444333"
  ← { text: "C:\\Reports\\Sales_June_2026.xlsx" }

Step 29: disconnect_device → device_id: "555444333"
```

**Screenshot count**: 0 (down from 20+ in legacy screenshot-heavy workflow)
**click_text calls replaced coordinate guessing**: 8 instances

**AI Summary**:
> Export complete. File saved to `C:\Reports\Sales_June_2026.xlsx` on the remote machine.
> Total steps: 29. Login → navigation → date selection → export — all verified.

---

## Scenario 4: retry_with_alternative — OCR Fallback Example

**Goal**: Demonstrates `retry_with_alternative` when OCR fails to recognize a button label.

```
# Primary attempt: click_text with exact match
# Fallback 1: click_text with partial match (handles OCR misread "保 存" vs "保存")
# Fallback 2: keyboard shortcut Ctrl+S (works regardless of UI state)

Step: retry_with_alternative
  → device_id: "123456789"
  → attempts: [
      { method: "clickText",    params: { text: "保存", exact: true } },
      { method: "clickText",    params: { text: "保存", exact: false } },
      { method: "keyboardHotkey", params: { keys: ["ctrl","s"] } }
    ]
  → success_conditions: [{ type: "text_absent", value: "未保存" }]
  → timeout_ms: 2000
  ← {
      success: true,
      attemptIndex: 2,         ← Ctrl+S succeeded after OCR attempts failed
      method: "keyboardHotkey",
      triedAttempts: 3
    }
```

---

## Scenario 5: Cross-Device File Transfer

**Goal**: Copy a report from Device A's desktop to Device B's Documents folder.

**Prompt**: `multi_device_workflow`

> **User**: Copy "Q4_Report.xlsx" from office PC (111111111 / 999999) to home PC (222222222 / 888888).

```
Step 1: connect_device("111111111", "999999", show_window=true) → { deviceId: "111111111" }
Step 2: wait_for_connection_state → device_id: "111111111", state: "connected"

Step 3: connect_device("222222222", "888888", show_window=true) → { deviceId: "222222222" }
Step 4: wait_for_connection_state → device_id: "222222222", state: "connected"

── Office PC ─────────────────────────────────────────────

Step 5: find_element  → device_id: "111111111", text: "Q4_Report"
  ← { found: true, matches: [{center:{x:150,y:320}}] }

Step 6: mouse_click  → device_id: "111111111", button: "right", x: 150, y: 320

Step 7: wait_for_text  → device_id: "111111111", text: "复制", timeout_ms: 2000

Step 8: click_text  → device_id: "111111111", text: "复制为路径"

Step 9: wait_for_clipboard_change  → device_id: "111111111", timeout_ms: 3000
  ← { data: { text: "\"C:\\Users\\user\\Desktop\\Q4_Report.xlsx\"" } }

Step 10: keyboard_hotkey  → device_id: "111111111", keys: ["win","r"]
Step 11: wait_for_text    → device_id: "111111111", text: "运行", timeout_ms: 3000
Step 12: keyboard_type    → device_id: "111111111", text: "powershell"
Step 13: keyboard_hotkey  → device_id: "111111111", keys: ["enter"]
Step 14: wait_for_text    → device_id: "111111111", text: "PS C:\\", timeout_ms: 8000

Step 15: keyboard_type
  → device_id: "111111111", text: "Copy-Item 'C:\\Users\\user\\Desktop\\Q4_Report.xlsx' '\\\\home-pc\\shared\\'"
Step 16: keyboard_hotkey  → device_id: "111111111", keys: ["enter"]
Step 17: wait_for_screen_change  → device_id: "111111111", timeout_ms: 15000

── Home PC ───────────────────────────────────────────────

Step 18: keyboard_hotkey  → device_id: "222222222", keys: ["win","e"]   ← open Explorer
Step 19: wait_for_text    → device_id: "222222222", text: "文件资源管理器", timeout_ms: 5000
Step 20: find_element     → device_id: "222222222", text: "Q4_Report"
  ← { found: true }   ← file confirmed present

Step 21: verify_action_result
  → device_id: "222222222", expectations: [{"type":"text_present","value":"Q4_Report"}]
  ← { allPassed: true }

Step 22: disconnect_device(device_id: "111111111")
Step 23: disconnect_device(device_id: "222222222")
```

---

## Quick Reference: Tool Selection Guide

### When to use which tool

| Situation | Use this tool | Instead of |
|-----------|---------------|-----------|
| After `connect_device` | `wait_for_connection_state` | `screenshot` + check desktop |
| Read screen text/layout | `get_ui_state` | `screenshot` + vision model |
| Find a UI element | `find_element` | `screenshot` + estimate coordinates |
| Find and click | `click_text` | `screenshot` → `find_element` → `mouse_click` |
| Wait for UI change | `wait_for_text` | polling `screenshot` in loop |
| Wait for any visual change | `wait_for_screen_change` | polling `screenshot` in loop |
| After an action, check success | `verify_action_result` | `screenshot` + visual check |
| Compare before/after | `screen_diff_summary` | two `screenshot` calls + diff |
| Action may fail, have fallbacks | `retry_with_alternative` | manual if/else retry logic |
| Need screenshot | `screenshot` | — (use when visual context needed for AI) |

### Prompt → scenario mapping

| Scenario | Prompt | Key new tools |
|----------|--------|---------------|
| Server health check / PC repair | `diagnose_system_issue` | `get_ui_state`, `wait_for_text`, `get_screen_text` |
| Batch device inspection | `batch_operation` | `wait_for_connection_state`, `wait_for_screen_change` |
| Legacy GUI software | `operate_remote_desktop` | `click_text`, `find_element`, `screen_diff_summary` |
| Verify after action | any | `verify_action_result`, `assert_screen_state` |
| Clipboard-based transfer | `multi_device_workflow` | `wait_for_clipboard_change` |

### Expected impact on screenshot usage

| Metric | Legacy | With Phase 5A tools |
|--------|--------|---------------------|
| Screenshots per 10-step task | 8–12 | 0–2 |
| Token cost per task | High (image tokens) | Low (text tokens) |
| OCR-based navigation | None | 100% via `find_element`/`click_text` |
| Post-action verification | Visual/manual | Structured `verify_action_result` |
