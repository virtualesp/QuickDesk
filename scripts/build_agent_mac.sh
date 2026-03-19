#!/bin/bash

echo
echo
echo "---------------------------------------------------------------"
echo "build quickdesk-agent + built-in skills (Rust workspace)"
echo "---------------------------------------------------------------"

{
    cd "$(dirname "$0")"
    script_path=$(pwd)
    cd - > /dev/null
} &> /dev/null

old_cd=$(pwd)
cd "$(dirname "$0")"

build_mode=release

echo
echo
echo "---------------------------------------------------------------"
echo "parse arguments"
echo "---------------------------------------------------------------"

while [ $# -gt 0 ]; do
    case "$(echo "$1" | tr '[:upper:]' '[:lower:]')" in
        debug)   build_mode=debug ;;
        release) build_mode=release ;;
    esac
    shift
done

echo "[*] build mode: $build_mode"
echo

agent_dir="$script_path/../quickdesk-agent"
output_path="$script_path/../output/arm64"

echo "[*] agent workspace dir: $agent_dir"
echo "[*] output path: $output_path"

if ! command -v cargo &> /dev/null; then
    echo "[!] error: cargo not found. Please install Rust: https://rustup.rs"
    cd "$old_cd"
    exit 1
fi

cd "$agent_dir"
echo "[*] building quickdesk-agent workspace..."

if [ "$build_mode" = "debug" ]; then
    cargo build
    if [ $? -ne 0 ]; then
        echo "[!] cargo build failed"
        cd "$old_cd"
        exit 1
    fi
    cargo_out="$agent_dir/target/debug"
    dest_dir="$output_path/Debug"
else
    cargo build --release
    if [ $? -ne 0 ]; then
        echo "[!] cargo build failed"
        cd "$old_cd"
        exit 1
    fi
    cargo_out="$agent_dir/target/release"
    dest_dir="$output_path/Release"
fi

# copy agent binary
mkdir -p "$dest_dir"
cp "$cargo_out/quickdesk-agent" "$dest_dir/"
echo "[*] copied quickdesk-agent to $dest_dir"

# copy skill binaries
mkdir -p "$dest_dir/skills"
cp "$cargo_out/sys-info" "$dest_dir/skills/"
echo "[*] copied sys-info to $dest_dir/skills"
cp "$cargo_out/file-ops" "$dest_dir/skills/"
echo "[*] copied file-ops to $dest_dir/skills"
cp "$cargo_out/shell-runner" "$dest_dir/skills/"
echo "[*] copied shell-runner to $dest_dir/skills"

# copy SKILL.md files
skills_src="$agent_dir/skills"
for skill in sys-info file-ops shell-runner; do
    mkdir -p "$dest_dir/skills/$skill"
    cp "$skills_src/$skill/SKILL.md" "$dest_dir/skills/$skill/"
    echo "[*] copied $skill/SKILL.md"
done

echo
echo
echo "---------------------------------------------------------------"
echo "[*] quickdesk-agent build finished!"
echo "---------------------------------------------------------------"

cd "$old_cd"
exit 0
