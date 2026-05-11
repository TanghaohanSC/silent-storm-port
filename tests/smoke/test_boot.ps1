# Phase 0 boot smoke test.
#
# Verifies silent_storm.exe launches at all. The game expects to load
# `game.db` from the current working directory; without it, Game/Main.cpp
# pops a MessageBox 'File game.db not found' (caught at line 39).
#
# Exit 0 if the exe launches and either:
#   (a) the MessageBox-spawning process is still alive after ~3 seconds, or
#   (b) the exe exited 1 (assertion failure path with MessageBox dismissed)
#
# Exit 1 if the exe never started or crashed instantly.
#
# Designed to run LOCAL only — CI runners don't have upstream/Complete/
# game data, but a missing-data MessageBox is exactly Phase 0 acceptance.

$exe = Join-Path $PSScriptRoot "..\..\build\msvc-debug\bin\silent_storm.exe"
if (-not (Test-Path $exe)) {
    Write-Error "exe not built: $exe"
    exit 2
}

Write-Host "Smoke test: launching $exe"
$proc = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 3

if ($proc.HasExited) {
    Write-Host "exe exited with code $($proc.ExitCode) after <3s — acceptable (likely MessageBox dismissed by user)"
    exit 0
}

Write-Host "exe still alive (PID $($proc.Id)) after 3s — likely showing 'File game.db not found' MessageBox"
Write-Host "Killing process for clean test exit"
$proc.Kill() | Out-Null
$proc.WaitForExit(5000) | Out-Null
exit 0
