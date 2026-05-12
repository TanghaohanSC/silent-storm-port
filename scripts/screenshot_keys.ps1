param(
  [string]$OutPath = "C:\Users\Haohan\Documents\silent-storm\port\docs\patches\p1_5_r6_iter.png",
  [int]$WaitBefore = 5,
  [int]$WaitAfter  = 3,
  # space-separated tokens passed to SendKeys. Special: "BACKTICK" sends `, "ENTER" sends ~,
  # plain text goes through verbatim, otherwise pass SendKeys macro like "{F1}".
  [string]$Keys = ""
)
$wd  = "C:\Users\Haohan\Documents\silent-storm\upstream\Complete"
$exe = "C:\Users\Haohan\Documents\silent-storm\port\build\msvc-debug\bin\silent_storm.exe"
$p = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds $WaitBefore
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
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
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
[U]::ShowWindow($hwnd, 9) | Out-Null
[U]::SetForegroundWindow($hwnd) | Out-Null
[U]::BringWindowToTop($hwnd) | Out-Null
[U]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 1024, 768, 0x0040) | Out-Null
Start-Sleep -Milliseconds 800

# Win32 keybd_event for low-level key injection (SendKeys is too high-level
# and sometimes loses focus). Maps short tokens to virtual-key codes.
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class K {
  [DllImport("user32.dll")]
  public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  public const uint KEYEVENTF_KEYUP = 0x0002;
  public const uint KEYEVENTF_SCANCODE = 0x0008;
  [DllImport("user32.dll", CharSet = CharSet.Auto)]
  public static extern short VkKeyScan(char ch);
  [DllImport("user32.dll", CharSet = CharSet.Auto)]
  public static extern uint MapVirtualKey(uint code, uint mapType);
}
"@

function Press-Key([byte]$vk) {
  $scan = [byte][K]::MapVirtualKey([uint32]$vk, 0)
  [K]::keybd_event($vk, $scan, [K]::KEYEVENTF_SCANCODE, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 80
  [K]::keybd_event($vk, $scan, [K]::KEYEVENTF_SCANCODE -bor [K]::KEYEVENTF_KEYUP, [IntPtr]::Zero)
}

# Send keys — re-focus before each press
function Send-OneKey([IntPtr]$hwnd, [string]$k) {
  [U]::ShowWindow($hwnd, 9) | Out-Null
  [U]::SetForegroundWindow($hwnd) | Out-Null
  [U]::BringWindowToTop($hwnd) | Out-Null
  Start-Sleep -Milliseconds 200
  if ($k -eq "BACKTICK") {
    # VK_OEM_3 = 0xC0 (backtick/tilde on US layout)
    Press-Key 0xC0
  } elseif ($k -eq "ENTER") {
    Press-Key 0x0D
  } elseif ($k -eq "ESC") {
    Press-Key 0x1B
  } elseif ($k -eq "TAB") {
    Press-Key 0x09
  } elseif ($k -eq "SPACE") {
    Press-Key 0x20
  } elseif ($k.Length -eq 1) {
    $vk = [K]::VkKeyScan([char]$k) -band 0xFF
    Press-Key ([byte]$vk)
  } else {
    Write-Output "unknown key: $k"
  }
  Start-Sleep -Milliseconds 250
}

if ($Keys -ne "") {
  Write-Output "sending keys: $Keys"
  foreach ($k in $Keys.Split(' ')) {
    if ($k -eq "") { continue }
    Send-OneKey -hwnd $hwnd -k $k
  }
}

Start-Sleep -Seconds $WaitAfter
if ($p.HasExited) {
  Write-Output "exited after keys, code=$($p.ExitCode)"
  return
}

# Re-foreground+re-position the window before screenshot (SendKeys can cause
# Windows to background it).
[U]::ShowWindow($hwnd, 9) | Out-Null
[U]::SetForegroundWindow($hwnd) | Out-Null
[U]::BringWindowToTop($hwnd) | Out-Null
[U]::SetWindowPos($hwnd, [IntPtr]::Zero, 0, 0, 1024, 768, 0x0040) | Out-Null
Start-Sleep -Milliseconds 500

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
