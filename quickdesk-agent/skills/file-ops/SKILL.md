---
name: file-ops
description: Browse, read, and write files on the remote host machine.
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

# file-ops

This skill provides file operation tools for the remote host:

- `list_directory` — list files and directories at a given path
- `read_file` — read the contents of a file
- `write_file` — write content to a file
- `create_directory` — create a directory
- `move_file` — move or rename a file

## Usage

The agent starts the official MCP filesystem server which exposes standard
file operation tools.  The server is restricted to the paths explicitly
configured in the launch arguments.
