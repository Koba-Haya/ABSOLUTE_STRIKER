@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
set E=..\..\..\AbsoluteEngine
if not exist run\resources\app mkdir run\resources\app
xcopy "..\..\..\Application\resources\butterfly" "run\resources\app\butterfly" /E /I /Y >nul
xcopy "..\..\..\Application\resources\human" "run\resources\app\human" /E /I /Y >nul
cl /nologo /utf-8 /std:c++17 /EHsc /MTd /I..\..\..\externals\assimp\include /I%E%\graphics\3d\model /I%E%\math /I%E%\base /I%E%\Type animation_probe.cpp %E%\graphics\3d\model\AnimationManager.cpp %E%\base\AssetPath.cpp %E%\base\EnginePath.cpp /Fo:run\ /Fe:run\animation_probe.exe /link /LIBPATH:..\..\..\externals\assimp\lib\Debug assimp-vc143-mtd.lib user32.lib
