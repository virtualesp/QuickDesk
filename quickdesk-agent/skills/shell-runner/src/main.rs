// Copyright 2026 QuickDesk Authors
// shell-runner — MCP server that executes shell commands on the host.

use anyhow::Result;
use mcp_server_common::{McpServer, ToolDef, ToolHandler};
use serde_json::{json, Value};
use std::process::Stdio;
use tokio::process::Command;

const DEFAULT_TIMEOUT_SECS: u64 = 60;
const MAX_OUTPUT_BYTES: usize = 512 * 1024; // 512 KB

struct ShellRunnerHandler;

impl ToolHandler for ShellRunnerHandler {
    fn server_name(&self) -> &str {
        "shell-runner"
    }

    fn server_version(&self) -> &str {
        "0.1.0"
    }

    fn tool_defs(&self) -> Vec<ToolDef> {
        vec![ToolDef {
            name: "run_command",
            description: "Execute a shell command on the remote host. Returns stdout, stderr, and exit code.",
            input_schema: json!({
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "The shell command to execute"
                    },
                    "working_dir": {
                        "type": "string",
                        "description": "Working directory for the command. Default: system default"
                    },
                    "timeout_secs": {
                        "type": "integer",
                        "description": "Timeout in seconds. Default: 60"
                    }
                },
                "required": ["command"]
            }),
        }]
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
                "run_command" => run_command(args).await,
                _ => anyhow::bail!("unknown tool: {}", name),
            }
        })
    }
}

async fn run_command(args: Value) -> Result<Value> {
    let command_str = args
        .get("command")
        .and_then(|v| v.as_str())
        .ok_or_else(|| anyhow::anyhow!("missing required field: command"))?;

    let timeout_secs = args
        .get("timeout_secs")
        .and_then(|v| v.as_u64())
        .unwrap_or(DEFAULT_TIMEOUT_SECS);

    let working_dir = args.get("working_dir").and_then(|v| v.as_str());

    let mut cmd = if cfg!(target_os = "windows") {
        let mut c = Command::new("cmd");
        c.args(["/C", command_str]);
        c
    } else {
        let mut c = Command::new("sh");
        c.args(["-c", command_str]);
        c
    };

    cmd.stdout(Stdio::piped());
    cmd.stderr(Stdio::piped());

    if let Some(dir) = working_dir {
        cmd.current_dir(dir);
    }

    let result = tokio::time::timeout(
        std::time::Duration::from_secs(timeout_secs),
        cmd.output(),
    )
    .await;

    match result {
        Ok(Ok(output)) => {
            let mut stdout = String::from_utf8_lossy(&output.stdout).to_string();
            let mut stderr = String::from_utf8_lossy(&output.stderr).to_string();

            let stdout_truncated = stdout.len() > MAX_OUTPUT_BYTES;
            let stderr_truncated = stderr.len() > MAX_OUTPUT_BYTES;
            if stdout_truncated {
                stdout.truncate(MAX_OUTPUT_BYTES);
                stdout.push_str("\n... [output truncated]");
            }
            if stderr_truncated {
                stderr.truncate(MAX_OUTPUT_BYTES);
                stderr.push_str("\n... [output truncated]");
            }

            Ok(json!({
                "exit_code": output.status.code(),
                "stdout": stdout,
                "stderr": stderr,
                "stdout_truncated": stdout_truncated,
                "stderr_truncated": stderr_truncated,
            }))
        }
        Ok(Err(e)) => {
            anyhow::bail!("failed to execute command: {}", e)
        }
        Err(_) => {
            anyhow::bail!("command timed out after {} seconds", timeout_secs)
        }
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_writer(std::io::stderr)
        .with_env_filter("shell_runner=info")
        .init();

    let server = McpServer::new(ShellRunnerHandler);
    server.run().await
}
