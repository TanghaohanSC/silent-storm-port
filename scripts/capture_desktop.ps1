param(
  [string]$OutPath = "C:\Users\Haohan\Documents\silent-storm\port\docs\patches\ss_desktop.png"
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int n);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern IntPtr GetDesktopWindow();
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
  [DllImport("gdi32.dll")] public static extern bool BitBlt(IntPtr dst, int dx, int dy, int w, int h, IntPtr src, int sx, int sy, int rop);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
}
"@
$hwnd = [IntPtr]::Zero
foreach ($pr in [Diagnostics.Process]::GetProcessesByName('silent_storm')) {
  if ($pr.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $pr.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { Write-Output "no HWND"; exit 1 }
[W]::ShowWindow($hwnd, 9) | Out-Null
[W]::BringWindowToTop($hwnd) | Out-Null
[W]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800
$cr = New-Object W+RECT
[W]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$pt = New-Object W+POINT
[W]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
$cw = $cr.R - $cr.L; $ch = $cr.B - $cr.T
$bmp = New-Object Drawing.Bitmap $cw, $ch
$g = [Drawing.Graphics]::FromImage($bmp)
$hdc_dst = $g.GetHdc()
$desk = [W]::GetDesktopWindow()
$hdc_src = [W]::GetDC($desk)
# SRCCOPY = 0x00CC0020
[W]::BitBlt($hdc_dst, 0, 0, $cw, $ch, $hdc_src, $pt.X, $pt.Y, 0x00CC0020) | Out-Null
[W]::ReleaseDC($desk, $hdc_src) | Out-Null
$g.ReleaseHdc($hdc_dst)
$g.Dispose()
$bmp.Save($OutPath, [Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved $OutPath ($cw x $ch from screen ($($pt.X),$($pt.Y)))"
