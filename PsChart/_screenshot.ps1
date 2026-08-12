# Capture the demo's client area to PsChart.png.
#
# Three things are load-bearing and a capture that skips any of them produces an image of the
# wrong window, or one offset into the desktop behind it:
#   1. the capturing process must be DPI-aware, or it and the demo disagree about coordinates;
#   2. the window must be found by enumerating the pid's visible top-levels and SKIPPING the
#      console classes -- these demos are console-subsystem exes, so MainWindowHandle returns
#      the CONSOLE window;
#   3. the window must be forced HWND_TOPMOST -- SetForegroundWindow alone leaves the console
#      sitting over it.

Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Win {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x, y; }
}
"@

[void][Win]::SetProcessDPIAware()

$exe = Join-Path $PSScriptRoot "main.exe"
$proc = Start-Process -FilePath $exe -WorkingDirectory $PSScriptRoot -PassThru
Start-Sleep -Milliseconds 2500

$target = [IntPtr]::Zero
$skip = @("ConsoleWindowClass", "CASCADIA_HOSTING_WINDOW_CLASS", "PseudoConsoleWindow")
$cb = [Win+EnumProc]{
  param($h, $l)
  $pid2 = 0
  [void][Win]::GetWindowThreadProcessId($h, [ref]$pid2)
  if ($pid2 -eq $proc.Id -and [Win]::IsWindowVisible($h)) {
    $sb = New-Object System.Text.StringBuilder 256
    [void][Win]::GetClassName($h, $sb, 256)
    if ($skip -notcontains $sb.ToString()) {
      $script:target = $h
      return $false
    }
  }
  return $true
}
[void][Win]::EnumWindows($cb, [IntPtr]::Zero)

if ($target -eq [IntPtr]::Zero) {
  Write-Host "FAILED: no non-console top-level window found for pid $($proc.Id)"
  $proc.Kill(); exit 1
}

# HWND_TOPMOST, SWP_NOMOVE|SWP_NOSIZE
[void][Win]::SetWindowPos($target, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002)
[void][Win]::SetForegroundWindow($target)
Start-Sleep -Milliseconds 900

$rc = New-Object Win+RECT
[void][Win]::GetClientRect($target, [ref]$rc)
$tl = New-Object Win+POINT
$tl.x = $rc.left; $tl.y = $rc.top
[void][Win]::ClientToScreen($target, [ref]$tl)
$w = $rc.right - $rc.left
$h = $rc.bottom - $rc.top

$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($tl.x, $tl.y, 0, 0, (New-Object System.Drawing.Size $w, $h))
$out = Join-Path $PSScriptRoot "PsChart.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

$proc.Kill()
Write-Host "captured ${w}x${h} -> $out"

