# 蝶のモデルを作り直す(テクスチャ生成 -> Blender で生成と書き出し -> 確認用の画像)。
# 使い方:  powershell -ExecutionPolicy Bypass -File build.ps1 [-Style mix|morpho|sunset] [-NoRender]
# 出力:
#   ..\..\Application\resources\butterfly\  (butterfly.gltf, .bin, テクスチャ 2 枚。ゲームが読むファイル)
#   out\butterfly.blend                     (Blender で開いて手直しできる途中経過)
#   render\                                 (確認用の画像。ゲームには入れない)
param(
    [ValidateSet("mix", "morpho", "sunset")][string]$Style = "mix",
    [switch]$NoRender
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$blender = "C:\Program Files\Blender Foundation\Blender 4.4\blender.exe"
$resources = Join-Path $here "..\..\Application\resources\butterfly"
$tex = Join-Path $here "out_$Style"

Push-Location $here
try {
    python gen_textures.py --style $Style --out $tex
    & $blender -b -P gen_butterfly.py -- --textures $tex --out $resources --blend (Join-Path $here "out\butterfly.blend")
    if (-not $NoRender) {
        & $blender -b -P render_views.py -- --gltf (Join-Path $resources "butterfly.gltf") --out (Join-Path $here "render\$Style")
    }
} finally {
    Pop-Location
}
