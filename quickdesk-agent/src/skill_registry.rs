// Copyright 2026 QuickDesk Authors
// SkillRegistry — scans the skills directory, parses SKILL.md frontmatter,
// and starts MCP server subprocesses for each applicable skill.

use serde::Deserialize;
use serde_json::Value;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use tracing::{info, warn};

use crate::mcp_client::McpClient;

/// Parsed frontmatter from a SKILL.md file.
#[derive(Debug, Deserialize, Default)]
pub struct SkillMeta {
    pub name: String,
    #[serde(default)]
    pub description: String,
    #[serde(default)]
    pub metadata: SkillMetadata,
}

#[derive(Debug, Deserialize, Default)]
pub struct SkillMetadata {
    #[serde(default)]
    pub openclaw: Option<OpenClawMeta>,
}

#[derive(Debug, Deserialize, Default, Clone)]
pub struct OpenClawMeta {
    #[serde(default)]
    pub os: Vec<String>,
    #[serde(default)]
    pub requires: OpenClawRequires,
    #[serde(default)]
    pub install: Vec<InstallStep>,
}

#[derive(Debug, Deserialize, Default, Clone)]
pub struct OpenClawRequires {
    #[serde(default)]
    pub bins: Vec<String>,
}

#[derive(Debug, Deserialize, Default, Clone)]
pub struct InstallStep {
    pub id: String,
    pub kind: String,
    #[serde(default)]
    pub package: String,
}

/// A loaded skill, ready to serve tool calls.
pub struct LoadedSkill {
    pub meta: SkillMeta,
    pub client: McpClient,
}

pub struct SkillRegistry {
    skills_dir: PathBuf,
    skills: HashMap<String, LoadedSkill>,
}

impl SkillRegistry {
    pub fn new(skills_dir: &str) -> Self {
        Self {
            skills_dir: PathBuf::from(skills_dir),
            skills: HashMap::new(),
        }
    }

    /// Scan the skills directory and start each applicable skill's MCP server.
    pub async fn load(&mut self) {
        let dir = self.skills_dir.clone();
        if !dir.exists() {
            warn!("skills directory not found: {}", dir.display());
            return;
        }

        let entries = match std::fs::read_dir(&dir) {
            Ok(e) => e,
            Err(e) => {
                warn!("cannot read skills directory: {}", e);
                return;
            }
        };

        for entry in entries.flatten() {
            let skill_dir = entry.path();
            if !skill_dir.is_dir() {
                continue;
            }
            let skill_md = skill_dir.join("SKILL.md");
            if !skill_md.exists() {
                continue;
            }

            match self.load_skill(&skill_dir, &skill_md).await {
                Ok(name) => info!("Loaded skill: {}", name),
                Err(e) => warn!(
                    "Failed to load skill at {}: {}",
                    skill_dir.display(),
                    e
                ),
            }
        }
    }

    /// Return a flat list of all tools from all running skill servers.
    pub fn list_tools(&self) -> Vec<Value> {
        self.skills
            .values()
            .flat_map(|s| s.client.cached_tools())
            .collect()
    }

    /// Call a tool by name.  Finds the owning skill and delegates.
    pub async fn call_tool(
        &self,
        tool_name: &str,
        args: Value,
    ) -> anyhow::Result<Value> {
        for skill in self.skills.values() {
            if skill.client.has_tool(tool_name) {
                return skill.client.call_tool(tool_name, args).await;
            }
        }
        anyhow::bail!("unknown tool: {}", tool_name)
    }

    // ---- private ----

    async fn load_skill(
        &mut self,
        skill_dir: &Path,
        skill_md: &Path,
    ) -> anyhow::Result<String> {
        let content = std::fs::read_to_string(skill_md)?;
        let meta = parse_frontmatter(&content)?;

        // Check OS compatibility
        if let Some(ref oc) = meta.metadata.openclaw {
            if !oc.os.is_empty() {
                let current_os = std::env::consts::OS; // "windows" | "macos" | "linux"
                // Map Rust OS names to OpenClaw convention
                let oc_os = match current_os {
                    "windows" => "win32",
                    "macos" => "darwin",
                    other => other,
                };
                if !oc.os.iter().any(|o| o == oc_os) {
                    warn!(
                        "Skill '{}' does not support OS '{}', skipping",
                        meta.name, oc_os
                    );
                    return Ok(meta.name.clone());
                }
            }
        }

        // Build the start command from the install steps
        let cmd = build_start_command(&meta, skill_dir)?;

        let mut client = McpClient::new(&meta.name, cmd);
        client.start().await?;
        client.fetch_tools().await?;

        let name = meta.name.clone();
        self.skills.insert(name.clone(), LoadedSkill { meta, client });
        Ok(name)
    }
}

/// Parse YAML frontmatter from a SKILL.md file.
/// Frontmatter is delimited by `---` lines.
fn parse_frontmatter(content: &str) -> anyhow::Result<SkillMeta> {
    let mut lines = content.lines();
    // Expect first line to be "---"
    if lines.next().map(str::trim) != Some("---") {
        anyhow::bail!("SKILL.md does not start with ---");
    }

    let mut yaml = String::new();
    for line in lines {
        if line.trim() == "---" {
            break;
        }
        yaml.push_str(line);
        yaml.push('\n');
    }

    let meta: SkillMeta = serde_yaml::from_str(&yaml)
        .map_err(|e| anyhow::anyhow!("YAML parse error: {}", e))?;
    Ok(meta)
}

/// Derive the command to start the MCP server from the skill's install steps.
fn build_start_command(
    meta: &SkillMeta,
    skill_dir: &Path,
) -> anyhow::Result<Vec<String>> {
    let oc = meta
        .metadata
        .openclaw
        .as_ref()
        .ok_or_else(|| anyhow::anyhow!("no openclaw metadata"))?;

    for step in &oc.install {
        match step.kind.as_str() {
            "npm" => {
                // Try npx first, then node_modules/.bin
                let pkg = &step.package;
                return Ok(vec!["npx".to_string(), pkg.clone(), "--stdio".to_string()]);
            }
            "uvx" => {
                let pkg = &step.package;
                return Ok(vec!["uvx".to_string(), pkg.clone()]);
            }
            "binary" => {
                // Local binary in skill directory
                let bin = skill_dir.join(&step.package);
                return Ok(vec![bin.to_string_lossy().to_string()]);
            }
            other => {
                warn!("unknown install kind '{}', trying as command", other);
                return Ok(vec![step.package.clone()]);
            }
        }
    }

    anyhow::bail!("no supported install step found for skill '{}'", meta.name)
}
