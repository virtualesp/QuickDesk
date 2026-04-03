use chrono::Utc;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use tracing::info;
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkflowStep {
    pub seq: u32,
    pub tool_name: String,
    pub arguments: Value,
    pub delay_ms: u64,
    pub result_summary: String,
    pub success: bool,
    pub parameterized_keys: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Workflow {
    pub id: String,
    pub name: String,
    pub description: String,
    pub device_id: String,
    pub steps: Vec<WorkflowStep>,
    pub tags: Vec<String>,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[allow(dead_code)]
pub struct ReplayResult {
    pub workflow_id: String,
    pub total_steps: u32,
    pub completed_steps: u32,
    pub failed_step: Option<u32>,
    pub error: Option<String>,
    pub step_results: Vec<StepResult>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[allow(dead_code)]
pub struct StepResult {
    pub seq: u32,
    pub tool_name: String,
    pub success: bool,
    pub result: Value,
    pub error: String,
}

struct RecordingState {
    workflow_id: String,
    name: String,
    device_id: String,
    steps: Vec<WorkflowStep>,
    started_at: String,
    last_step_time: std::time::Instant,
}

pub struct WorkflowStore {
    storage_dir: PathBuf,
    active_recordings: Arc<Mutex<HashMap<String, RecordingState>>>,
}

impl WorkflowStore {
    pub fn new() -> Result<Self, String> {
        let dir = Self::storage_dir();
        std::fs::create_dir_all(&dir)
            .map_err(|e| format!("cannot create workflow dir: {e}"))?;
        info!("WorkflowStore at {}", dir.display());
        Ok(Self {
            storage_dir: dir,
            active_recordings: Arc::new(Mutex::new(HashMap::new())),
        })
    }

    fn storage_dir() -> PathBuf {
        let base = dirs_data_dir().unwrap_or_else(|| PathBuf::from("."));
        base.join(".quickdesk").join("workflows")
    }

    pub fn start_recording(&self, name: &str, device_id: &str) -> Result<String, String> {
        let mut recordings = self.active_recordings.lock().unwrap();
        if recordings.contains_key(device_id) {
            return Err("a recording is already active for this device".to_string());
        }

        let id = Uuid::new_v4().to_string();
        let state = RecordingState {
            workflow_id: id.clone(),
            name: name.to_string(),
            device_id: device_id.to_string(),
            steps: Vec::new(),
            started_at: Utc::now().to_rfc3339(),
            last_step_time: std::time::Instant::now(),
        };
        recordings.insert(device_id.to_string(), state);
        info!("recording started: id={id} name={name} device_id={device_id}");
        Ok(id)
    }

    pub fn is_recording(&self, device_id: &str) -> bool {
        self.active_recordings
            .lock()
            .unwrap()
            .contains_key(device_id)
    }

    pub fn record_step(
        &self,
        device_id: &str,
        tool_name: &str,
        arguments: &Value,
        result_summary: &str,
        success: bool,
    ) {
        let mut recordings = self.active_recordings.lock().unwrap();
        if let Some(state) = recordings.get_mut(device_id) {
            let now = std::time::Instant::now();
            let delay = now.duration_since(state.last_step_time).as_millis() as u64;
            state.last_step_time = now;

            let seq = state.steps.len() as u32 + 1;
            let param_keys = detect_parameterizable(arguments);
            state.steps.push(WorkflowStep {
                seq,
                tool_name: tool_name.to_string(),
                arguments: arguments.clone(),
                delay_ms: delay,
                result_summary: truncate(result_summary, 512),
                success,
                parameterized_keys: param_keys,
            });
        }
    }

    pub fn stop_recording(
        &self,
        device_id: &str,
        description: &str,
        tags: &[String],
    ) -> Result<Workflow, String> {
        let mut recordings = self.active_recordings.lock().unwrap();
        let state = recordings
            .remove(device_id)
            .ok_or_else(|| "no active recording for this device".to_string())?;

        if state.steps.is_empty() {
            return Err("no steps were recorded".to_string());
        }

        let now = Utc::now().to_rfc3339();
        let wf = Workflow {
            id: state.workflow_id,
            name: state.name,
            description: description.to_string(),
            device_id: state.device_id,
            steps: state.steps,
            tags: tags.to_vec(),
            created_at: state.started_at,
            updated_at: now,
        };

        self.save_workflow(&wf)?;
        info!("recording stopped: id={} steps={}", wf.id, wf.steps.len());
        Ok(wf)
    }

    pub fn list_workflows(&self) -> Vec<WorkflowSummary> {
        let mut results = Vec::new();
        let entries = match std::fs::read_dir(&self.storage_dir) {
            Ok(e) => e,
            Err(_) => return results,
        };

        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().map(|e| e == "json").unwrap_or(false) {
                if let Ok(data) = std::fs::read_to_string(&path) {
                    if let Ok(wf) = serde_json::from_str::<Workflow>(&data) {
                        results.push(WorkflowSummary {
                            id: wf.id,
                            name: wf.name,
                            description: wf.description,
                            device_id: wf.device_id,
                            step_count: wf.steps.len() as u32,
                            tags: wf.tags,
                            created_at: wf.created_at,
                        });
                    }
                }
            }
        }

        results.sort_by(|a, b| b.created_at.cmp(&a.created_at));
        results
    }

    pub fn get_workflow(&self, id: &str) -> Option<Workflow> {
        let path = self.workflow_path(id);
        let data = std::fs::read_to_string(&path).ok()?;
        serde_json::from_str(&data).ok()
    }

    pub fn delete_workflow(&self, id: &str) -> bool {
        let path = self.workflow_path(id);
        std::fs::remove_file(&path).is_ok()
    }

    pub fn build_replay_steps(
        &self,
        workflow_id: &str,
        overrides: &HashMap<String, Value>,
    ) -> Result<Vec<(String, Value)>, String> {
        let wf = self
            .get_workflow(workflow_id)
            .ok_or_else(|| "workflow not found".to_string())?;

        let mut steps = Vec::new();
        for step in &wf.steps {
            let mut args = step.arguments.clone();
            if let Value::Object(ref mut map) = args {
                for (key, val) in overrides {
                    if map.contains_key(key) {
                        map.insert(key.clone(), val.clone());
                    }
                }
            }
            steps.push((step.tool_name.clone(), args));
        }
        Ok(steps)
    }

    fn save_workflow(&self, wf: &Workflow) -> Result<(), String> {
        let path = self.workflow_path(&wf.id);
        let data = serde_json::to_string_pretty(wf)
            .map_err(|e| format!("serialize: {e}"))?;
        std::fs::write(&path, data)
            .map_err(|e| format!("write: {e}"))
    }

    fn workflow_path(&self, id: &str) -> PathBuf {
        self.storage_dir.join(format!("{id}.json"))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkflowSummary {
    pub id: String,
    pub name: String,
    pub description: String,
    pub device_id: String,
    pub step_count: u32,
    pub tags: Vec<String>,
    pub created_at: String,
}

fn detect_parameterizable(args: &Value) -> Vec<String> {
    let mut keys = Vec::new();
    if let Value::Object(map) = args {
        for (k, v) in map {
            match v {
                Value::String(s) if looks_like_path(s) => keys.push(k.clone()),
                Value::String(s) if s.len() > 100 => keys.push(k.clone()),
                _ => {}
            }
        }
    }
    keys
}

fn looks_like_path(s: &str) -> bool {
    s.starts_with('/') || s.starts_with("C:\\") || s.starts_with("D:\\") || s.contains("\\")
}

fn truncate(s: &str, max: usize) -> String {
    if s.len() > max {
        format!("{}...", &s[..max])
    } else {
        s.to_string()
    }
}

fn dirs_data_dir() -> Option<PathBuf> {
    #[cfg(target_os = "windows")]
    {
        std::env::var("USERPROFILE").ok().map(PathBuf::from)
    }
    #[cfg(not(target_os = "windows"))]
    {
        std::env::var("HOME").ok().map(PathBuf::from)
    }
}
