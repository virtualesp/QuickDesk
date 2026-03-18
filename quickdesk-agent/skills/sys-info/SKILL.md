---
name: sys-info
description: Get remote machine processes, CPU, memory, and system information.
metadata:
  openclaw:
    os: ["win32", "darwin", "linux"]
    requires:
      bins: [node, npx]
    install:
      - id: npm
        kind: npm
        package: "@modelcontextprotocol/server-filesystem"
---

# sys-info

This skill provides system information tools for the remote host:

- `list_processes` — list running processes (name, pid, cpu%, memory%)
- `get_system_info` — CPU model, memory total/used, OS version, uptime

## Usage

The agent starts a Node.js MCP server that exposes system information via
standard MCP tool calls.  No user interaction required.
