@echo off
setlocal
set RES=%1
if "%RES%"=="" set RES=640x480
rem Quincunx 1.1 - Quincunx sharper (master on the centre, slave on the corner, filtered)
set SSTH3_SLI_AA_CONFIGURATION=10
set FX_GLIDE_QX_LOG=%~dp0qx.log
echo ---- %DATE% %TIME% E_sharper %RES% >> "%FX_GLIDE_QX_LOG%"
"%~dp0qxpattern.exe" -r %RES% -p aa2argb8888 %2 %3 %4
endlocal
