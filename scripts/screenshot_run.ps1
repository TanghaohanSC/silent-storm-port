param(
  [string]$OutPath = "C:\Users\Haohan\Documents\silent-storm\port\docs\patches\p1_5_r3_iter1.png",
  [int]$WaitSeconds = 8
)
$wd  = "C:\Users\Haohan\Documents\silent-storm\upstream\Complete"
$exe = "C:\Users\Haohan\Documents\silent-storm\port\build\msvc-debug\bin\silent_storm.exe"
$p = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds $WaitSeconds
if ($p.HasExited) {
  Write-Output "exited code=$($p.ExitCode)"
  return
}
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public class U {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rc);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int n);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndAfter, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll", SetLastError=true)]
  public static extern IntPtr GetTopWindow(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError=true)]
  public static extern IntPtr GetWindow(IntPtr hWnd, uint cmd);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
# Find the silent_storm window via its main window handle from the process
$proc = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
if (-not $proc) { Write-Output "process gone"; return }
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
  Write-Output "couldn't locate main window after waiting"
  if (-not $p.HasExited) { $p.Kill() | Out-Null }
  return
}
[U]::ShowWindow($hwnd, 9) | Out-Null  # SW_RESTORE
[U]::SetForegroundWindow($hwnd) | Out-Null
[U]::BringWindowToTop($hwnd) | Out-Null
# move to top-left so we don't capture other windows above
[U]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 1024, 768, 0x0040) | Out-Null  # SWP_SHOWWINDOW
Start-Sleep -Milliseconds 1000
$rc = New-Object U+RECT
[U]::GetWindowRect($hwnd, [ref]$rc) | Out-Null
$w = $rc.R - $rc.L
$h = $rc.B - $rc.T
Write-Output "ss_window rect $($rc.L),$($rc.T) - $($rc.R),$($rc.B)  size $w x $h"
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rc.L, $rc.T, 0, 0, [System.Drawing.Size]::new($w, $h))
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "saved $OutPath"
if (-not $p.HasExited) { $p.Kill() | Out-Null }
