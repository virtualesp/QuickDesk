// Copyright 2026 QuickDesk Authors
// sys-info — MCP server that exposes system information tools.

use anyhow::Result;
use mcp_server_common::{McpServer, ToolDef, ToolHandler};
use serde_json::{json, Value};
use sysinfo::System;

struct SysInfoHandler;

impl ToolHandler for SysInfoHandler {
    fn server_name(&self) -> &str {
        "sys-info"
    }

    fn server_version(&self) -> &str {
        "0.1.0"
    }

    fn tool_defs(&self) -> Vec<ToolDef> {
        vec![
            ToolDef {
                name: "get_system_info",
                description: "Get OS version, CPU model, memory total/used, disk usage, hostname, and uptime",
                input_schema: json!({
                    "type": "object",
                    "properties": {},
                    "required": []
                }),
            },
            ToolDef {
                name: "list_processes",
                description: "List running processes with name, pid, CPU%, and memory usage",
                input_schema: json!({
                    "type": "object",
                    "properties": {
                        "sort_by": {
                            "type": "string",
                            "description": "Sort by 'cpu', 'memory', or 'name'. Default: 'cpu'",
                            "enum": ["cpu", "memory", "name"]
                        },
                        "limit": {
                            "type": "integer",
                            "description": "Max number of processes to return. Default: 50"
                        }
                    },
                    "required": []
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
                "get_system_info" => get_system_info().await,
                "list_processes" => list_processes(args).await,
                _ => anyhow::bail!("unknown tool: {}", name),
            }
        })
    }
}

async fn get_system_info() -> Result<Value> {
    let mut sys = System::new_all();
    sys.refresh_all();

    let os_name = System::name().unwrap_or_default();
    let os_version = System::os_version().unwrap_or_default();
    let kernel = System::kernel_version().unwrap_or_default();
    let hostname = System::host_name().unwrap_or_default();

    let cpus = sys.cpus();
    let cpu_name = cpus.first().map(|c| c.brand().to_string()).unwrap_or_default();
    let cpu_count = cpus.len();
    let cpu_usage: f32 = if cpus.is_empty() {
        0.0
    } else {
        cpus.iter().map(|c| c.cpu_usage()).sum::<f32>() / cpus.len() as f32
    };

    let total_mem = sys.total_memory();
    let used_mem = sys.used_memory();
    let total_swap = sys.total_swap();
    let used_swap = sys.used_swap();

    let disks: Vec<Value> = sysinfo::Disks::new_with_refreshed_list()
        .iter()
        .map(|d| {
            json!({
                "mount": d.mount_point().to_string_lossy(),
                "total_gb": d.total_space() as f64 / 1_073_741_824.0,
                "available_gb": d.available_space() as f64 / 1_073_741_824.0,
                "fs": String::from_utf8_lossy(d.file_system().as_encoded_bytes()),
            })
        })
        .collect();

    let uptime = System::uptime();
    let hours = uptime / 3600;
    let minutes = (uptime % 3600) / 60;

    Ok(json!({
        "os": format!("{} {}", os_name, os_version),
        "kernel": kernel,
        "hostname": hostname,
        "cpu": {
            "model": cpu_name,
            "cores": cpu_count,
            "usage_percent": format!("{:.1}", cpu_usage),
        },
        "memory": {
            "total_mb": total_mem / 1_048_576,
            "used_mb": used_mem / 1_048_576,
            "usage_percent": format!("{:.1}", used_mem as f64 / total_mem as f64 * 100.0),
        },
        "swap": {
            "total_mb": total_swap / 1_048_576,
            "used_mb": used_swap / 1_048_576,
        },
        "disks": disks,
        "uptime": format!("{}h {}m", hours, minutes),
    }))
}

async fn list_processes(args: Value) -> Result<Value> {
    let sort_by = args
        .get("sort_by")
        .and_then(|v| v.as_str())
        .unwrap_or("cpu");
    let limit = args
        .get("limit")
        .and_then(|v| v.as_u64())
        .unwrap_or(50) as usize;

    let mut sys = System::new_all();
    sys.refresh_all();
    // Allow CPU usage values to settle
    std::thread::sleep(std::time::Duration::from_millis(200));
    sys.refresh_all();

    let mut procs: Vec<Value> = sys
        .processes()
        .values()
        .map(|p| {
            json!({
                "pid": p.pid().as_u32(),
                "name": p.name().to_string_lossy(),
                "cpu_percent": format!("{:.1}", p.cpu_usage()),
                "memory_mb": p.memory() / 1_048_576,
            })
        })
        .collect();

    match sort_by {
        "memory" => procs.sort_by(|a, b| {
            let ma = a["memory_mb"].as_u64().unwrap_or(0);
            let mb = b["memory_mb"].as_u64().unwrap_or(0);
            mb.cmp(&ma)
        }),
        "name" => procs.sort_by(|a, b| {
            let na = a["name"].as_str().unwrap_or("");
            let nb = b["name"].as_str().unwrap_or("");
            na.to_lowercase().cmp(&nb.to_lowercase())
        }),
        _ => procs.sort_by(|a, b| {
            let ca: f64 = a["cpu_percent"].as_str().unwrap_or("0").parse().unwrap_or(0.0);
            let cb: f64 = b["cpu_percent"].as_str().unwrap_or("0").parse().unwrap_or(0.0);
            cb.partial_cmp(&ca).unwrap_or(std::cmp::Ordering::Equal)
        }),
    }

    procs.truncate(limit);

    Ok(json!({
        "total_processes": sys.processes().len(),
        "showing": procs.len(),
        "sort_by": sort_by,
        "processes": procs,
    }))
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_writer(std::io::stderr)
        .with_env_filter("sys_info=info")
        .init();

    let server = McpServer::new(SysInfoHandler);
    server.run().await
}
