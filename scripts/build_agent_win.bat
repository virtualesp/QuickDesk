@echo off

echo=
echo=
echo ---------------------------------------------------------------
echo build quickdesk-agent + built-in skills (Rust workspace)
echo ---------------------------------------------------------------

:: get script absolute path
set script_path=%~dp0
set old_cd=%cd%
cd /d %~dp0

SETLOCAL EnableDelayedExpansion
set build_mode=release
set errno=1

echo=
echo=
echo ---------------------------------------------------------------
echo parse arguments
echo ---------------------------------------------------------------

:parse_args
if "%1"=="" goto args_done
if /i "%1"=="debug" set build_mode=debug
if /i "%1"=="release" set build_mode=release
shift
goto parse_args
:args_done

echo [*] build mode: %build_mode%
echo=

set agent_dir=%script_path%..\quickdesk-agent
set output_path=%script_path%..\output\x64

echo [*] agent workspace dir: %agent_dir%
echo [*] output path: %output_path%

:: check if Rust is installed
where cargo >nul 2>nul
if %errorlevel% neq 0 (
    echo [!] error: cargo not found. Please install Rust: https://rustup.rs
    goto return
)

:: build
cd /d "%agent_dir%"
echo [*] building quickdesk-agent workspace...

if /i "%build_mode%"=="debug" (
    cargo build
    if not %errorlevel%==0 (
        echo [!] cargo build failed
        goto return
    )
    set cargo_out=%agent_dir%\target\debug
    set dest_dir=%output_path%\Debug
) else (
    cargo build --release
    if not %errorlevel%==0 (
        echo [!] cargo build failed
        goto return
    )
    set cargo_out=%agent_dir%\target\release
    set dest_dir=%output_path%\Release
)

:: copy agent binary to output directory
if not exist "!dest_dir!" mkdir "!dest_dir!"
copy /Y "!cargo_out!\quickdesk-agent.exe" "!dest_dir!\" >nul
echo [*] copied quickdesk-agent.exe to !dest_dir!

:: copy skill binaries to output/skills/
if not exist "!dest_dir!\skills" mkdir "!dest_dir!\skills"
copy /Y "!cargo_out!\sys-info.exe" "!dest_dir!\skills\" >nul
echo [*] copied sys-info.exe to !dest_dir!\skills
copy /Y "!cargo_out!\file-ops.exe" "!dest_dir!\skills\" >nul
echo [*] copied file-ops.exe to !dest_dir!\skills
copy /Y "!cargo_out!\shell-runner.exe" "!dest_dir!\skills\" >nul
echo [*] copied shell-runner.exe to !dest_dir!\skills

:: copy SKILL.md files
set skills_src=%agent_dir%\skills
for %%s in (sys-info file-ops shell-runner) do (
    if not exist "!dest_dir!\skills\%%s" mkdir "!dest_dir!\skills\%%s"
    copy /Y "!skills_src!\%%s\SKILL.md" "!dest_dir!\skills\%%s\" >nul
    echo [*] copied %%s/SKILL.md
)

echo=
echo=
echo ---------------------------------------------------------------
echo [*] quickdesk-agent build finished!
echo ---------------------------------------------------------------

set errno=0

:return
cd /d "%old_cd%"
exit /B %errno%

ENDLOCAL
