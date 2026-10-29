@echo off
setlocal
set RES=%1
if "%RES%"=="" set RES=640x480
rem Quincunx 1.1 - reference without AA (2 chips in SLI)
set SSTH3_SLI_AA_CONFIGURATION=2
set FX_GLIDE_QX_LOG=%~dp0qx.log
echo ---- %DATE% %TIME% A_no_AA %RES% >> "%FX_GLIDE_QX_LOG%"
"%~dp0qxpattern.exe" -r %RES% -p argb8888 %2 %3 %4
endlocal
