param(
  [string]$OutPath,
  [int]$WinW,
  [int]$WinH,
  [int]$WaitSeconds = 10
)
$wd  = "C:\Users\Haohan\Documents\silent-storm\upstream\Complete"
$exe = "C:\Users\Haohan\Documents\silent-storm\port\build\msvc-debug\bin\silent_storm.exe"

$p = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds $WaitSeconds
if ($p.HasExited) {
  Write-Output "exited code=$($p.ExitCode)"
  exit 1
}

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public class WinUtil {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rc);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int n);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndAfter, int x, int y, int cx, int cy, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@

$proc = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
if (-not $proc) { Write-Output "process gone"; exit 1 }
$hwnd = $proc.MainWindowHandle
$tries = 0
while ($hwnd -eq [IntPtr]::Zero -and $tries -lt 10) {
  Start-Sleep -Milliseconds 500
  $proc = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
  if (-not $proc) { break }
  $hwnd = $proc.MainWindowHandle
  $tries++
}
if ($hwnd -eq [IntPtr]::Zero) {
  Write-Output "couldn't locate main window"
  if (-not $p.HasExited) { $p.Kill() | Out-Null }
  exit 1
}

[WinUtil]::ShowWindow($hwnd, 9) | Out-Null   # SW_RESTORE
[WinUtil]::SetForegroundWindow($hwnd) | Out-Null
[WinUtil]::BringWindowToTop($hwnd) | Out-Null
# Move to top-left, set requested size
[WinUtil]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, $WinW, $WinH, 0x0040) | Out-Null
Start-Sleep -Milliseconds 1500

$rc = New-Object WinUtil+RECT
[WinUtil]::GetWindowRect($hwnd, [ref]$rc) | Out-Null
$w = $rc.R - $rc.L
$h = $rc.B - $rc.T
Write-Output "ss_window rect $($rc.L),$($rc.T) - $($rc.R),$($rc.B)  size $w x $h"

if ($w -le 0 -or $h -le 0) {
  Write-Output "invalid window size, using requested dimensions"
  $w = $WinW
  $h = $WinH
}

$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rc.L, $rc.T, 0, 0, [System.Drawing.Size]::new($w, $h))
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "saved $OutPath"
if (-not $p.HasExited) { $p.Kill() | Out-Null }
