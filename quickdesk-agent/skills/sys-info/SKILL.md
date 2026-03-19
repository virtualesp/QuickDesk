---
name: sys-info
description: Get remote machine processes, CPU, memory, and system information.
metadata:
  openclaw:
    os: ["win32", "darwin", "linux"]
    install:
      - id: binary
        kind: binary
        package: "sys-info"
---

# sys-info

This skill provides system information tools for the remote host:

- `get_system_info` — OS version, CPU model, memory total/used, disk usage, hostname, uptime
- `list_processes` — list running processes (name, pid, cpu%, memory)

## Usage

The agent starts a Rust MCP server that exposes system information via
standard MCP tool calls. No user interaction required.
