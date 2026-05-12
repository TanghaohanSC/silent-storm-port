param(
  [string]$OutPath = "C:\Users\Haohan\Documents\silent-storm\port\docs\patches\ss_capture.png"
)
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT p);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr a, int x, int y, int cx, int cy, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
}
"@
$hwnd = [IntPtr]::Zero
foreach ($pr in [Diagnostics.Process]::GetProcessesByName('silent_storm')) {
  if ($pr.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $pr.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { Write-Output "no HWND"; exit 1 }
# Bring window to foreground
[W]::ShowWindow($hwnd, 9) | Out-Null  # SW_RESTORE
[W]::SetWindowPos($hwnd, [IntPtr]::new(-1), 0,0,0,0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null  # NOMOVE NOSIZE SHOWWINDOW TOPMOST
[W]::BringWindowToTop($hwnd) | Out-Null
[W]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 500
# Now capture client rect off the screen
$cr = New-Object W+RECT
[W]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$pt = New-Object W+POINT
[W]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
$cw = $cr.R - $cr.L; $ch = $cr.B - $cr.T
Write-Output "client at ($($pt.X),$($pt.Y)) size $cw x $ch"
$bmp = New-Object Drawing.Bitmap $cw, $ch
$g = [Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($pt.X, $pt.Y, 0, 0, [Drawing.Size]::new($cw, $ch))
$g.Dispose()
$bmp.Save($OutPath, [Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved $OutPath"
# Reset topmost
[W]::SetWindowPos($hwnd, [IntPtr]::new(-2), 0,0,0,0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
