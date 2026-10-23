@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d %~dp0
cl /nologo /std:c++17 /EHsc /utf-8 /MTd /I..\..\..\externals\assimp\include assimp_probe.cpp /Fe:assimp_probe.exe /link /LIBPATH:..\..\..\externals\assimp\lib\Debug assimp-vc143-mtd.lib
