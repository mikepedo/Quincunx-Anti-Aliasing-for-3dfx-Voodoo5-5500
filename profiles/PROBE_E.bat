@echo off
setlocal
set RES=%1
if "%RES%"=="" set RES=640x480
rem Sample probe, Quincunx sharper: key 0, then F12 (screenshot glide_*.tga)
set SSTH3_SLI_AA_CONFIGURATION=10
set FX_GLIDE_QX_LOG=%~dp0qx.log
set FX_GLIDE_SCREENSHOT_KEY=123
"%~dp0qxpattern.exe" -r %RES% -p aa2argb8888 %2 %3 %4
endlocal
