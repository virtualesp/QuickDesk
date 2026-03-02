mod server;
mod ws_client;

use clap::Parser;
use rmcp::ServiceExt;
use rmcp::transport::stdio;
use tracing_subscriber::EnvFilter;

#[derive(Parser)]
#[command(name = "quickdesk-mcp", about = "MCP bridge for QuickDesk remote desktop")]
struct Cli {
    /// QuickDesk WebSocket server URL
    #[arg(long, default_value = "ws://127.0.0.1:9800")]
    ws_url: String,

    /// Authentication token for full-control access
    #[arg(long, env = "QUICKDESK_TOKEN")]
    token: Option<String>,

    /// Authentication token for read-only access (screenshot, status only — no input)
    #[arg(long, env = "QUICKDESK_READONLY_TOKEN")]
    readonly_token: Option<String>,

    /// Comma-separated list of allowed device IDs (restrict which devices AI can connect to)
    #[arg(long, env = "QUICKDESK_ALLOWED_DEVICES", value_delimiter = ',')]
    allowed_devices: Option<Vec<String>>,

    /// Maximum API requests per minute per client (0 = unlimited)
    #[arg(long, env = "QUICKDESK_RATE_LIMIT", default_value = "0")]
    rate_limit: i32,

    /// Session timeout in seconds (0 = no timeout)
    #[arg(long, env = "QUICKDESK_SESSION_TIMEOUT", default_value = "0")]
    session_timeout: i32,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env())
        .with_writer(std::io::stderr)
        .init();

    let cli = Cli::parse();

    tracing::info!("Connecting to QuickDesk at {}", cli.ws_url);

    let auth_token = cli.token.as_deref().or(cli.readonly_token.as_deref());
    let ws = ws_client::WsClient::connect(&cli.ws_url, auth_token).await?;

    if let Some(ref devices) = cli.allowed_devices {
        tracing::info!("Allowed devices: {:?}", devices);
    }
    if cli.rate_limit > 0 {
        tracing::info!("Rate limit: {} requests/minute", cli.rate_limit);
    }
    if cli.session_timeout > 0 {
        tracing::info!("Session timeout: {}s", cli.session_timeout);
    }

    tracing::info!("Connected. Starting MCP server on stdio...");

    let mcp_server = server::QuickDeskMcpServer::new(
        ws,
        cli.allowed_devices.unwrap_or_default(),
    );
    let service = mcp_server.serve(stdio()).await?;
    service.waiting().await?;

    Ok(())
}
