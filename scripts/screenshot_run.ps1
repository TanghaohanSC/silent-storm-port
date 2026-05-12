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
# Locate the silent_storm window
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public class U {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rc);
  [DllImport("user32.dll")] public static extern IntPtr FindWindow(string c, string n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
$hwnd = [U]::FindWindow($null, "A5")
if ($hwnd -eq [IntPtr]::Zero) { $hwnd = [U]::FindWindow("A5", $null) }
if ($hwnd -eq [IntPtr]::Zero) { $hwnd = [U]::GetForegroundWindow() }
[U]::ShowWindow($hwnd, 9) | Out-Null  # SW_RESTORE
[U]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800
$rc = New-Object U+RECT
[U]::GetWindowRect($hwnd, [ref]$rc) | Out-Null
$w = $rc.R - $rc.L
$h = $rc.B - $rc.T
Write-Output "window rect $($rc.L),$($rc.T) - $($rc.R),$($rc.B)  size $w x $h"
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rc.L, $rc.T, 0, 0, [System.Drawing.Size]::new($w, $h))
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "saved $OutPath"
if (-not $p.HasExited) { $p.Kill() | Out-Null }
