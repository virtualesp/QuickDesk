// Copyright 2026 QuickDesk Authors
// quickdesk-agent — host-side agent that manages skill MCP servers and
// bridges tool calls between the Qt AgentManager and skill subprocesses.
//
// Communication with Qt (stdin/stdout JSON Lines):
//   Qt → agent:  {"id":"req-1","type":"toolCall","tool":"run_shell","args":{"cmd":"..."}}
//   Qt → agent:  {"id":"req-2","type":"listTools"}
//   agent → Qt:  {"id":"req-1","type":"toolResult","result":"..."}
//   agent → Qt:  {"type":"capabilitiesChanged","tools":[...]}

mod capability_reporter;
mod mcp_client;
mod skill_registry;

use clap::Parser;
use serde_json::Value;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tracing::{error, info, warn};

#[derive(Parser, Debug)]
#[command(about = "QuickDesk host agent")]
struct Args {
    /// Directory containing skill sub-directories with SKILL.md files
    #[arg(long, default_value = "skills")]
    skills_dir: String,
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_writer(std::io::stderr)
        .with_env_filter(
            tracing_subscriber::EnvFilter::from_default_env()
                .add_directive("quickdesk_agent=info".parse().unwrap()),
        )
        .init();

    let args = Args::parse();

    info!("quickdesk-agent starting, skills_dir={}", args.skills_dir);

    // Load skills and start their MCP servers.
    let mut registry = skill_registry::SkillRegistry::new(&args.skills_dir);
    registry.load().await;

    // Announce initial capabilities to Qt.
    let tools = registry.list_tools();
    send_message(&serde_json::json!({
        "type": "capabilitiesChanged",
        "tools": tools,
    }))
    .await;

    // Main loop: read JSON Lines from stdin, dispatch, write results to stdout.
    let stdin = tokio::io::stdin();
    let mut lines = BufReader::new(stdin).lines();

    while let Some(line) = lines.next_line().await? {
        let line = line.trim().to_string();
        if line.is_empty() {
            continue;
        }

        match serde_json::from_str::<Value>(&line) {
            Ok(msg) => {
                let response = dispatch(&registry, msg).await;
                send_message(&response).await;
            }
            Err(e) => {
                warn!("invalid JSON from Qt: {} — {}", line, e);
            }
        }
    }

    info!("quickdesk-agent stdin closed, exiting");
    Ok(())
}

/// Dispatch a message from Qt to the appropriate handler.
async fn dispatch(registry: &skill_registry::SkillRegistry, msg: Value) -> Value {
    let id = msg.get("id").cloned().unwrap_or(Value::Null);
    let msg_type = msg
        .get("type")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();

    match msg_type.as_str() {
        "listTools" => {
            let tools = registry.list_tools();
            serde_json::json!({
                "id": id,
                "type": "toolResult",
                "tools": tools,
            })
        }
        "toolCall" => {
            let tool = msg
                .get("tool")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let args = msg.get("args").cloned().unwrap_or(Value::Object(Default::default()));

            match registry.call_tool(&tool, args).await {
                Ok(result) => serde_json::json!({
                    "id": id,
                    "type": "toolResult",
                    "result": result,
                }),
                Err(e) => {
                    error!("tool call failed: tool={}, error={}", tool, e);
                    serde_json::json!({
                        "id": id,
                        "type": "toolError",
                        "error": e.to_string(),
                    })
                }
            }
        }
        unknown => {
            warn!("unknown message type from Qt: {}", unknown);
            serde_json::json!({
                "id": id,
                "type": "error",
                "error": format!("unknown message type: {}", unknown),
            })
        }
    }
}

/// Write a JSON object as a single line to stdout.
async fn send_message(msg: &Value) {
    let mut line = serde_json::to_string(msg).unwrap_or_default();
    line.push('\n');

    let mut stdout = tokio::io::stdout();
    if let Err(e) = stdout.write_all(line.as_bytes()).await {
        error!("failed to write to stdout: {}", e);
    }
    let _ = stdout.flush().await;
}
