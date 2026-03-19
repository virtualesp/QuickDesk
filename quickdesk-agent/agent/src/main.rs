// Copyright 2026 QuickDesk Authors
// quickdesk-agent — host-side agent that manages skill MCP servers and
// bridges tool calls between the Qt AgentManager and skill subprocesses.
//
// Communication with Qt (stdin/stdout JSON Lines):
//   Qt → agent:  {"id":"req-1","type":"toolCall","tool":"run_command","args":{...}}
//   Qt → agent:  {"id":"req-2","type":"listTools"}
//   Qt → agent:  {"id":"req-3","type":"reloadSkill","skill":"docker-mcp"}
//   agent → Qt:  {"type":"capabilitiesReady","tools":[...]}
//   agent → Qt:  {"type":"skillLoadFailed","skill":"...","reason":"...","missing":[...]}
//   agent → Qt:  {"id":"req-1","type":"toolResult","result":"..."}

mod capability_reporter;
mod dep_checker;
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

    let mut registry = skill_registry::SkillRegistry::new(&args.skills_dir);
    registry.load().await;

    // Report initial capabilities
    let tools = registry.list_tools();
    send_message(&capability_reporter::capabilities_ready(tools)).await;

    // Report any load errors
    for err in registry.load_errors() {
        send_message(&capability_reporter::skill_load_failed(err)).await;
    }

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
                let responses = dispatch(&mut registry, msg).await;
                for response in responses {
                    send_message(&response).await;
                }
            }
            Err(e) => {
                warn!("invalid JSON from Qt: {} — {}", line, e);
            }
        }
    }

    info!("quickdesk-agent stdin closed, exiting");
    Ok(())
}

async fn dispatch(registry: &mut skill_registry::SkillRegistry, msg: Value) -> Vec<Value> {
    let id = msg.get("id").cloned().unwrap_or(Value::Null);
    let msg_type = msg
        .get("type")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();

    match msg_type.as_str() {
        "listTools" => {
            let tools = registry.list_tools();
            vec![serde_json::json!({
                "id": id,
                "type": "toolResult",
                "tools": tools,
            })]
        }
        "toolCall" => {
            let tool = msg
                .get("tool")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let args = msg
                .get("args")
                .cloned()
                .unwrap_or(Value::Object(Default::default()));

            match registry.call_tool(&tool, args).await {
                Ok(result) => vec![serde_json::json!({
                    "id": id,
                    "type": "toolResult",
                    "result": result,
                })],
                Err(e) => {
                    error!("tool call failed: tool={}, error={}", tool, e);
                    vec![serde_json::json!({
                        "id": id,
                        "type": "toolError",
                        "error": e.to_string(),
                    })]
                }
            }
        }
        "reloadSkill" => {
            let skill_name = msg
                .get("skill")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();

            info!("reloading skill: {}", skill_name);

            let old_tools = registry.list_tools();
            let mut responses = Vec::new();

            match registry.reload_skill(&skill_name).await {
                Ok(()) => {
                    let new_tools = registry.list_tools();
                    responses.push(serde_json::json!({
                        "id": id,
                        "type": "toolResult",
                        "result": "ok",
                    }));
                    // Report capability changes
                    let added: Vec<Value> = new_tools
                        .iter()
                        .filter(|t| !old_tools.contains(t))
                        .cloned()
                        .collect();
                    if !added.is_empty() {
                        responses.push(capability_reporter::capabilities_changed(
                            added,
                            Vec::new(),
                        ));
                    }
                }
                Err(e) => {
                    responses.push(serde_json::json!({
                        "id": id,
                        "type": "toolError",
                        "error": e,
                    }));
                    // Report any new load errors
                    for err in registry.load_errors() {
                        if err.skill_name == skill_name {
                            responses.push(capability_reporter::skill_load_failed(err));
                        }
                    }
                }
            }
            responses
        }
        unknown => {
            warn!("unknown message type from Qt: {}", unknown);
            vec![serde_json::json!({
                "id": id,
                "type": "error",
                "error": format!("unknown message type: {}", unknown),
            })]
        }
    }
}

async fn send_message(msg: &Value) {
    let mut line = serde_json::to_string(msg).unwrap_or_default();
    line.push('\n');

    let mut stdout = tokio::io::stdout();
    if let Err(e) = stdout.write_all(line.as_bytes()).await {
        error!("failed to write to stdout: {}", e);
    }
    let _ = stdout.flush().await;
}
