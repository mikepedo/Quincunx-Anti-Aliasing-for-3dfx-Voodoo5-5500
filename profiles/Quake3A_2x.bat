@echo off
rem Quincunx 1.1 - Quake III Arena, stock 2x AA (for comparison). Copy into the game folder.
rem r_mode 3 = 640x480, 4 = 800x600, 6 = 1024x768, 8 = 1280x1024
setlocal
set SSTH3_SLI_AA_CONFIGURATION=3
set FX_GLIDE_QX_LOG=%~dp0qx.log
echo ---- %DATE% %TIME% Quake3 2x >> "%FX_GLIDE_QX_LOG%"
cd /d "%~dp0"
start "" /wait "%~dp0quake3.exe" +set r_mode 6 +set r_colorbits 32
endlocal
