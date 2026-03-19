// Copyright 2026 QuickDesk Authors
// dep_checker — checks whether required binaries are available on the host.

use std::process::Command;
use tracing::warn;

/// Check which of the required binaries are missing on this system.
/// Returns a list of binary names that could not be found.
pub fn check_bins(bins: &[String]) -> Vec<String> {
    bins.iter()
        .filter(|bin| !is_bin_available(bin))
        .cloned()
        .collect()
}

fn is_bin_available(bin: &str) -> bool {
    let result = if cfg!(target_os = "windows") {
        Command::new("where").arg(bin).output()
    } else {
        Command::new("which").arg(bin).output()
    };

    match result {
        Ok(output) => output.status.success(),
        Err(e) => {
            warn!("failed to check binary '{}': {}", bin, e);
            false
        }
    }
}
