# Stage 8A: burst capture - Count frames of the PIE window, GapMs apart, after StartDelay seconds (one process, no per-frame start-up).
param([string]$TitlePrefix = "ThanePlatform9 Preview", [string]$OutPrefix, [int]$Count = 10, [int]$GapMs = 80, [double]$StartDelay = 0)

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
if ([P9Win]::IsIconic($h)) { [P9Win]::ShowWindow($h, 9) | Out-Null }
[P9Win]::SetForegroundWindow($h) | Out-Null
if ($StartDelay -gt 0) { Start-Sleep -Milliseconds ([int]($StartDelay * 1000)) }
$r = New-Object P9Win+RECT
[P9Win]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $hgt = $r.Bottom - $r.Top
$t0 = Get-Date
for ($i = 0; $i -lt $Count; $i++) {
  $bmp = New-Object System.Drawing.Bitmap $w, $hgt
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc(); [P9Win]::PrintWindow($h, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc)
  $ms = [int]((Get-Date) - $t0).TotalMilliseconds
  $bmp.Save(("{0}_{1:D2}_{2}ms.png" -f $OutPrefix, $i, $ms), [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Start-Sleep -Milliseconds $GapMs
}
Write-Output "burst $Count frames -> $OutPrefix"
