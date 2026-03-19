// Copyright 2026 QuickDesk Authors
// capability_reporter — builds JSON messages for capability and error reporting.

use serde_json::{json, Value};

use crate::skill_registry::SkillLoadError;

/// Build the initial capabilitiesReady message.
pub fn capabilities_ready(tools: Vec<Value>) -> Value {
    json!({
        "type": "capabilitiesReady",
        "tools": tools,
    })
}

/// Build a capabilitiesChanged message for hot-reload updates.
pub fn capabilities_changed(added: Vec<Value>, removed: Vec<Value>) -> Value {
    json!({
        "type": "capabilitiesChanged",
        "added": added,
        "removed": removed,
    })
}

/// Build a skillLoadFailed message from a SkillLoadError.
pub fn skill_load_failed(error: &SkillLoadError) -> Value {
    let mut msg = json!({
        "type": "skillLoadFailed",
        "skill": error.skill_name,
        "reason": error.reason,
    });
    if !error.missing_bins.is_empty() {
        msg["missing"] = json!(error.missing_bins);
    }
    msg
}
