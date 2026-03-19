// Copyright 2026 QuickDesk Authors
// file-ops — MCP server that exposes file operation tools.

use anyhow::Result;
use mcp_server_common::{McpServer, ToolDef, ToolHandler};
use serde_json::{json, Value};
use std::fs;
use std::path::Path;

struct FileOpsHandler;

impl ToolHandler for FileOpsHandler {
    fn server_name(&self) -> &str {
        "file-ops"
    }

    fn server_version(&self) -> &str {
        "0.1.0"
    }

    fn tool_defs(&self) -> Vec<ToolDef> {
        vec![
            ToolDef {
                name: "read_file",
                description: "Read the contents of a file. Returns the text content.",
                input_schema: json!({
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Absolute path to the file"
                        }
                    },
                    "required": ["path"]
                }),
            },
            ToolDef {
                name: "write_file",
                description: "Write content to a file. Creates the file if it doesn't exist, overwrites if it does.",
                input_schema: json!({
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Absolute path to the file"
                        },
                        "content": {
                            "type": "string",
                            "description": "Content to write"
                        }
                    },
                    "required": ["path", "content"]
                }),
            },
            ToolDef {
                name: "list_directory",
                description: "List files and directories at a given path",
                input_schema: json!({
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Absolute path to the directory"
                        }
                    },
                    "required": ["path"]
                }),
            },
            ToolDef {
                name: "create_directory",
                description: "Create a directory (and parent directories if needed)",
                input_schema: json!({
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Absolute path of the directory to create"
                        }
                    },
                    "required": ["path"]
                }),
            },
            ToolDef {
                name: "move_file",
                description: "Move or rename a file or directory",
                input_schema: json!({
                    "type": "object",
                    "properties": {
                        "source": {
                            "type": "string",
                            "description": "Source path"
                        },
                        "destination": {
                            "type": "string",
                            "description": "Destination path"
                        }
                    },
                    "required": ["source", "destination"]
                }),
            },
            ToolDef {
                name: "get_file_info",
                description: "Get file metadata: size, modification time, permissions, type",
                input_schema: json!({
                    "type": "object",
                    "properties": {
                        "path": {
                            "type": "string",
                            "description": "Absolute path to the file or directory"
                        }
                    },
                    "required": ["path"]
                }),
            },
        ]
    }

    fn call_tool(
        &self,
        name: &str,
        args: Value,
    ) -> std::pin::Pin<Box<dyn std::future::Future<Output = Result<Value>> + Send + '_>> {
        let name = name.to_string();
        let args = args.clone();
        Box::pin(async move {
            match name.as_str() {
                "read_file" => read_file(args),
                "write_file" => write_file(args),
                "list_directory" => list_directory(args),
                "create_directory" => create_directory(args),
                "move_file" => move_file(args),
                "get_file_info" => get_file_info(args),
                _ => anyhow::bail!("unknown tool: {}", name),
            }
        })
    }
}

fn require_str<'a>(args: &'a Value, field: &str) -> Result<&'a str> {
    args.get(field)
        .and_then(|v| v.as_str())
        .ok_or_else(|| anyhow::anyhow!("missing required field: {}", field))
}

fn read_file(args: Value) -> Result<Value> {
    let path = require_str(&args, "path")?;
    let content = fs::read_to_string(path)
        .map_err(|e| anyhow::anyhow!("failed to read '{}': {}", path, e))?;
    Ok(json!({
        "path": path,
        "content": content,
        "size": content.len(),
    }))
}

fn write_file(args: Value) -> Result<Value> {
    let path = require_str(&args, "path")?;
    let content = require_str(&args, "content")?;

    if let Some(parent) = Path::new(path).parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, content)?;

    Ok(json!({
        "path": path,
        "bytes_written": content.len(),
    }))
}

fn list_directory(args: Value) -> Result<Value> {
    let path = require_str(&args, "path")?;
    let entries = fs::read_dir(path)
        .map_err(|e| anyhow::anyhow!("failed to read directory '{}': {}", path, e))?;

    let mut items: Vec<Value> = Vec::new();
    for entry in entries.flatten() {
        let meta = entry.metadata();
        let is_dir = meta.as_ref().map(|m| m.is_dir()).unwrap_or(false);
        let size = meta.as_ref().map(|m| m.len()).unwrap_or(0);
        items.push(json!({
            "name": entry.file_name().to_string_lossy(),
            "type": if is_dir { "directory" } else { "file" },
            "size": size,
        }));
    }

    items.sort_by(|a, b| {
        let ta = a["type"].as_str().unwrap_or("");
        let tb = b["type"].as_str().unwrap_or("");
        let na = a["name"].as_str().unwrap_or("");
        let nb = b["name"].as_str().unwrap_or("");
        tb.cmp(ta).then(na.to_lowercase().cmp(&nb.to_lowercase()))
    });

    Ok(json!({
        "path": path,
        "count": items.len(),
        "entries": items,
    }))
}

fn create_directory(args: Value) -> Result<Value> {
    let path = require_str(&args, "path")?;
    fs::create_dir_all(path)?;
    Ok(json!({
        "path": path,
        "created": true,
    }))
}

fn move_file(args: Value) -> Result<Value> {
    let source = require_str(&args, "source")?;
    let destination = require_str(&args, "destination")?;
    fs::rename(source, destination)
        .map_err(|e| anyhow::anyhow!("failed to move '{}' to '{}': {}", source, destination, e))?;
    Ok(json!({
        "source": source,
        "destination": destination,
        "moved": true,
    }))
}

fn get_file_info(args: Value) -> Result<Value> {
    let path = require_str(&args, "path")?;
    let meta = fs::metadata(path)
        .map_err(|e| anyhow::anyhow!("failed to get info for '{}': {}", path, e))?;

    let file_type = if meta.is_dir() {
        "directory"
    } else if meta.is_symlink() {
        "symlink"
    } else {
        "file"
    };

    let modified = meta
        .modified()
        .ok()
        .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
        .map(|d| d.as_secs());

    let mut info = json!({
        "path": path,
        "type": file_type,
        "size": meta.len(),
        "readonly": meta.permissions().readonly(),
    });
    if let Some(ts) = modified {
        info["modified_unix"] = json!(ts);
    }

    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        info["permissions_octal"] = json!(format!("{:o}", meta.permissions().mode()));
    }

    Ok(info)
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_writer(std::io::stderr)
        .with_env_filter("file_ops=info")
        .init();

    let server = McpServer::new(FileOpsHandler);
    server.run().await
}
