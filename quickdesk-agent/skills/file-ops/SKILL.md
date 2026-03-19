---
name: file-ops
description: Browse, read, and write files on the remote host machine.
metadata:
  openclaw:
    os: ["win32", "darwin", "linux"]
    install:
      - id: binary
        kind: binary
        package: "file-ops"
---

# file-ops

This skill provides file operation tools for the remote host:

- `read_file` — read the contents of a file
- `write_file` — write content to a file
- `list_directory` — list files and directories at a given path
- `create_directory` — create a directory
- `move_file` — move or rename a file
- `get_file_info` — get file metadata (size, modification time, permissions)

## Usage

The agent starts a Rust MCP server which exposes standard file operation
tools. No external runtime required.
