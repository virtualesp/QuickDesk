---
name: shell-runner
description: Execute shell commands on the remote host machine.
metadata:
  openclaw:
    os: ["win32", "darwin", "linux"]
    install:
      - id: binary
        kind: binary
        package: "shell-runner"
---

# shell-runner

This skill provides shell command execution on the remote host:

- `run_command` — execute a shell command, returns stdout, stderr, and exit code

## Usage

The agent starts a Rust MCP server that executes commands via `cmd /C`
(Windows) or `sh -c` (Unix). Supports configurable timeout and working
directory.
