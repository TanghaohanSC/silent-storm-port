# Capture the client area of the silent_storm 'A5' window to a PNG.
# Uses PrintWindow with PW_RENDERFULLCONTENT to capture even when occluded.
param(
    [string]$OutPath = "C:\Users\Haohan\Documents\silent-storm\port\build\msvc-debug\bin\ss_client.png"
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$sig = @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public class W32 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
"@
Add-Type -TypeDefinition $sig -ReferencedAssemblies System.Drawing

$hwnd = [IntPtr]::Zero
foreach ($pr in [System.Diagnostics.Process]::GetProcessesByName('silent_storm')) {
    if ($pr.MainWindowHandle -ne [IntPtr]::Zero) {
        $hwnd = $pr.MainWindowHandle
        Write-Output "HWND=$hwnd title=$($pr.MainWindowTitle) pid=$($pr.Id)"
        break
    }
}
if ($hwnd -eq [IntPtr]::Zero) { Write-Output "no HWND found"; exit 1 }

$wr = New-Object W32+RECT
[W32]::GetWindowRect($hwnd, [ref]$wr) | Out-Null
$ww = $wr.Right - $wr.Left
$wh = $wr.Bottom - $wr.Top
$rc = New-Object W32+RECT
[W32]::GetClientRect($hwnd, [ref]$rc) | Out-Null
$cw = $rc.Right - $rc.Left
$ch = $rc.Bottom - $rc.Top
Write-Output "window=$ww x $wh client=$cw x $ch"

if ($ww -le 0 -or $wh -le 0) { Write-Output "window has zero size"; exit 2 }

# Use the WINDOW rect (PrintWindow renders the full window into hdc starting at 0,0).
$bmp = New-Object System.Drawing.Bitmap $ww, $wh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
# PW_RENDERFULLCONTENT = 0x2; PW_CLIENTONLY = 0x1
$ok = [W32]::PrintWindow($hwnd, $hdc, 0x2)
$g.ReleaseHdc($hdc)
$g.Dispose()
if (-not $ok) {
    Write-Output "PrintWindow PW_RENDERFULLCONTENT failed; trying flags=0"
    $g2 = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc2 = $g2.GetHdc()
    [W32]::PrintWindow($hwnd, $hdc2, 0) | Out-Null
    $g2.ReleaseHdc($hdc2)
    $g2.Dispose()
}

$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)

# Quick stats: sample a 5x5 grid to detect uniform color
$bmp2 = New-Object System.Drawing.Bitmap $OutPath
$ww2 = $bmp2.Width; $wh2 = $bmp2.Height
$samples = @()
for ($y = 0; $y -lt 5; $y++) {
    for ($x = 0; $x -lt 5; $x++) {
        $px = [int](($ww2 - 1) * $x / 4)
        $py = [int](($wh2 - 1) * $y / 4)
        $samples += $bmp2.GetPixel($px, $py)
    }
}
$bmp.Dispose()
$bmp2.Dispose()
$first = $samples[0]
$allSame = $true
foreach ($s in $samples) {
    if ($s.R -ne $first.R -or $s.G -ne $first.G -or $s.B -ne $first.B) { $allSame = $false; break }
}
$colors = ($samples | ForEach-Object { "{0:X2}{1:X2}{2:X2}" -f $_.R, $_.G, $_.B } | Select-Object -Unique)
Write-Output "saved $OutPath ($ww2 x $wh2)"
Write-Output "uniform=$allSame unique-sample-colors=$($colors.Count): $($colors -join ',')"
