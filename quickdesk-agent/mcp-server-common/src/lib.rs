// Copyright 2026 QuickDesk Authors
// Shared MCP stdio server framework for built-in skill MCP servers.
// Each skill binary only needs to implement the `ToolHandler` trait
// and call `McpServer::run()`.

use anyhow::Result;
use serde_json::{json, Value};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tracing::{error, info};

pub struct ToolDef {
    pub name: &'static str,
    pub description: &'static str,
    pub input_schema: Value,
}

pub trait ToolHandler: Send + Sync {
    fn server_name(&self) -> &str;
    fn server_version(&self) -> &str;
    fn tool_defs(&self) -> Vec<ToolDef>;
    fn call_tool(
        &self,
        name: &str,
        args: Value,
    ) -> std::pin::Pin<Box<dyn std::future::Future<Output = Result<Value>> + Send + '_>>;
}

pub struct McpServer<H: ToolHandler> {
    handler: H,
    tools_json: Vec<Value>,
}

impl<H: ToolHandler> McpServer<H> {
    pub fn new(handler: H) -> Self {
        let tools_json = handler
            .tool_defs()
            .into_iter()
            .map(|t| {
                json!({
                    "name": t.name,
                    "description": t.description,
                    "inputSchema": t.input_schema,
                })
            })
            .collect();
        Self {
            handler,
            tools_json,
        }
    }

    pub async fn run(mut self) -> Result<()> {
        let stdin = tokio::io::stdin();
        let mut lines = BufReader::new(stdin).lines();

        while let Some(line) = lines.next_line().await? {
            let line = line.trim().to_string();
            if line.is_empty() {
                continue;
            }

            let msg: Value = match serde_json::from_str(&line) {
                Ok(v) => v,
                Err(e) => {
                    error!("invalid JSON: {}", e);
                    continue;
                }
            };

            let id = msg.get("id").cloned();
            let method = msg
                .get("method")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let params = msg.get("params").cloned().unwrap_or(json!({}));

            if method == "notifications/initialized" {
                continue;
            }

            let response = match method.as_str() {
                "initialize" => self.handle_initialize(&id),
                "tools/list" => self.handle_tools_list(&id),
                "tools/call" => self.handle_tools_call(&id, params).await,
                _ => Self::error_response(&id, -32601, &format!("method not found: {}", method)),
            };

            Self::send(&response).await;
        }

        info!("{} shutting down", self.handler.server_name());
        Ok(())
    }

    fn handle_initialize(&self, id: &Option<Value>) -> Value {
        json!({
            "jsonrpc": "2.0",
            "id": id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {}
                },
                "serverInfo": {
                    "name": self.handler.server_name(),
                    "version": self.handler.server_version()
                }
            }
        })
    }

    fn handle_tools_list(&self, id: &Option<Value>) -> Value {
        json!({
            "jsonrpc": "2.0",
            "id": id,
            "result": {
                "tools": self.tools_json
            }
        })
    }

    async fn handle_tools_call(&mut self, id: &Option<Value>, params: Value) -> Value {
        let tool_name = params
            .get("name")
            .and_then(|v| v.as_str())
            .unwrap_or("");
        let args = params
            .get("arguments")
            .cloned()
            .unwrap_or(json!({}));

        let tool_names: Vec<&str> = self.tools_json.iter()
            .filter_map(|t| t.get("name").and_then(|n| n.as_str()))
            .collect();

        if !tool_names.contains(&tool_name) {
            return Self::error_response(id, -32602, &format!("unknown tool: {}", tool_name));
        }

        match self.handler.call_tool(tool_name, args).await {
            Ok(result) => {
                let text = match result {
                    Value::String(s) => s,
                    other => serde_json::to_string_pretty(&other).unwrap_or_default(),
                };
                json!({
                    "jsonrpc": "2.0",
                    "id": id,
                    "result": {
                        "content": [{
                            "type": "text",
                            "text": text
                        }]
                    }
                })
            }
            Err(e) => {
                json!({
                    "jsonrpc": "2.0",
                    "id": id,
                    "result": {
                        "isError": true,
                        "content": [{
                            "type": "text",
                            "text": e.to_string()
                        }]
                    }
                })
            }
        }
    }

    fn error_response(id: &Option<Value>, code: i32, message: &str) -> Value {
        json!({
            "jsonrpc": "2.0",
            "id": id,
            "error": {
                "code": code,
                "message": message
            }
        })
    }

    async fn send(msg: &Value) {
        let mut line = serde_json::to_string(msg).unwrap_or_default();
        line.push('\n');
        let mut stdout = tokio::io::stdout();
        let _ = stdout.write_all(line.as_bytes()).await;
        let _ = stdout.flush().await;
    }
}
