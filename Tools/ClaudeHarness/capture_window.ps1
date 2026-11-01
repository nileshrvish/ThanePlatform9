# Captures the on-screen pixels of the first top-level window whose title starts with $TitlePrefix.
param([string]$TitlePrefix = "ThanePlatform9 Preview", [string]$OutFile)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class P9Win {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static IntPtr Find(string prefix) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => {
      var sb = new StringBuilder(512); GetWindowText(h, sb, 512);
      if (IsWindowVisible(h) && sb.ToString().StartsWith(prefix)) { found = h; return false; }
      return true; }, IntPtr.Zero);
    return found;
  }
}
"@
[P9Win]::SetProcessDPIAware() | Out-Null
$h = [P9Win]::Find($TitlePrefix)
if ($h -eq [IntPtr]::Zero) { Write-Output "WINDOW NOT FOUND: $TitlePrefix"; exit 2 }
if ([P9Win]::IsIconic($h)) { [P9Win]::ShowWindow($h, 9) | Out-Null }   # SW_RESTORE if minimised
(New-Object -ComObject WScript.Shell).AppActivate($TitlePrefix) | Out-Null
[P9Win]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 600
$r = New-Object P9Win+RECT
[P9Win]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $hgt = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $hgt
$g = [System.Drawing.Graphics]::FromImage($bmp)
# PW_RENDERFULLCONTENT (2) renders the window's own content even when other windows cover it.
$hdc = $g.GetHdc()
$ok = [P9Win]::PrintWindow($h, $hdc, 2)
$g.ReleaseHdc($hdc)
$bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "captured ${w}x${hgt} at ($($r.Left),$($r.Top)) via PrintWindow=$ok -> $OutFile"
