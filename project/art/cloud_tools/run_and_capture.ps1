# ゲームを起動し、タイトルで SPACE を押してゲームへ進み、指定した秒数で画面(クライアント領域)を PNG に保存する。
# 使い方: .\run_and_capture.ps1 -ExeDir <Application.exe のあるフォルダ> -TitleWaitSec 6 -CaptureAfterStartSec @(5,7,9)
#   ExeDir の既定は Development の出力。保存先 OutDir の既定は %TEMP%\cloud_shots(リポジトリを汚さない)。
# 注意: DirectInput はスキャンコードを見るので、SendKeys ではなく keybd_event(スキャンコード付き)で SPACE を送っている。
#       終了時に、起動した exe だけを Stop-Process する(あなたが起動中の別の exe は止めない)。
param(
    [string]$ExeDir = "C:\Users\haya2\source\repos\CG2\project\generated\outputs\Development",
    [string]$OutDir = (Join-Path $env:TEMP "cloud_shots"),
    [int]$TitleWaitSec = 6,
    [int[]]$CaptureAfterStartSec = @(8, 12, 16, 20)
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
foreach ($s in $CaptureAfterStartSec) {
    $wait = $s - ((Get-Date) - $start).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]($wait * 1000)) }
    Capture ("shot_{0:D2}s.png" -f $s)
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
"done"
