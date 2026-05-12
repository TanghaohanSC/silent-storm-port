# bik2webm.ps1 — Phase 3 offline transcoder
#
# Falls out of scope if FFmpeg's runtime Bink decoder works (it does, as of
# Phase 3 verification — see docs/phase-3-completion.md).  Kept around for
# the case where a downstream user wants smaller/faster-decoding .webm
# files in place of the original .bik, or for platforms where the runtime
# FFmpeg ends up without Bink enabled.
#
# Usage:
#     pwsh tools\bik2webm\bik2webm.ps1                        # default in/out
#     pwsh tools\bik2webm\bik2webm.ps1 -SrcDir <path> -DstDir <path>
#     pwsh tools\bik2webm\bik2webm.ps1 -Ffmpeg "C:\bin\ffmpeg.exe"
#
# Requires: an ffmpeg binary on PATH (or specified via -Ffmpeg).  Modern
# ffmpeg ships with Bink decode + libvpx-vp9 + libopus, which is what this
# script asks for.

[CmdletBinding()]
param(
    [string] $SrcDir = "C:\Users\Haohan\Documents\silent-storm\upstream\Versions\Current\res\Video",
    [string] $DstDir = "C:\Users\Haohan\Documents\silent-storm\port\data\video",
    [string] $Ffmpeg = "ffmpeg"
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $SrcDir)) {
    throw "SrcDir not found: $SrcDir"
}

if (-not (Test-Path $DstDir)) {
    New-Item -ItemType Directory -Path $DstDir -Force | Out-Null
}

$biks = Get-ChildItem -Path $SrcDir -Filter '*.bik'
if ($biks.Count -eq 0) {
    Write-Host "No .bik files found in $SrcDir"
    exit 0
}

foreach ($bik in $biks) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($bik.Name)
    $out  = Join-Path $DstDir ($base + '.webm')
    Write-Host "==> $($bik.Name) -> $out"
    & $Ffmpeg -y -i $bik.FullName `
        -c:v libvpx-vp9 -b:v 0 -crf 32 -row-mt 1 `
        -c:a libopus -b:a 96k `
        -threads 0 `
        $out
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "ffmpeg exit code $LASTEXITCODE on $($bik.Name)"
    }
}

Write-Host "Done. Output in $DstDir"
