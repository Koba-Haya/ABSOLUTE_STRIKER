# run_and_capture.ps1 のバースト版。SPACE の BurstStartSec 秒後から、待たずに BurstCount 枚を連続で撮る(burst_00.png ...)。
# 連続フレームの差を見るために使う(analyze_burst.py)。撮影間隔は約 40ms で、ゲームの 60fps より粗い。
param(
    [string]$ExeDir = "C:\Users\haya2\source\repos\CG2\project\generated\outputs\Release",
    [string]$OutDir = (Join-Path $env:TEMP "cloud_shots"),
    [int]$TitleWaitSec = 6,
    [int]$BurstStartSec = 7,
    [int]$BurstCount = 12
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
[Win]::SetProcessDPIAware() | Out-Null

New-Item -ItemType Directory -Force $OutDir | Out-Null
Get-ChildItem $OutDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force

$p = Start-Process -FilePath (Join-Path $ExeDir "Application.exe") -PassThru -WorkingDirectory $ExeDir
Start-Sleep -Seconds $TitleWaitSec
$p.Refresh()
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { "no window handle"; Stop-Process -Id $p.Id -Force; exit 1 }
[Win]::ShowWindow($h, 9) | Out-Null
[Win]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 500

function Capture($name) {
    $rc = New-Object Win+RECT
    [Win]::GetClientRect($h, [ref]$rc) | Out-Null
    $pt = New-Object Win+POINT
    [Win]::ClientToScreen($h, [ref]$pt) | Out-Null
    $w = $rc.Right - $rc.Left; $hh = $rc.Bottom - $rc.Top
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, (New-Object System.Drawing.Size $w, $hh))
    $g.Dispose()
    $bmp.Save((Join-Path $OutDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# タイトルで SPACE を押してゲームへ進む
# DirectInput はスキャンコードを見るので、スキャンコード付きで SPACE(0x39)を 150ms 押す
[Win]::keybd_event(0, 0x39, 8, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 150
[Win]::keybd_event(0, 0x39, 10, [UIntPtr]::Zero)
$start = Get-Date
$wait = $BurstStartSec - ((Get-Date) - $start).TotalSeconds
if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
$t0 = Get-Date
for ($i = 0; $i -lt $BurstCount; $i++) { Capture ("burst_{0:D2}.png" -f $i) }
"burst took {0:N0} ms for {1} frames" -f ((Get-Date) - $t0).TotalMilliseconds, $BurstCount
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
"done"
