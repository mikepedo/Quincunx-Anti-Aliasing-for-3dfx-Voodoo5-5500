@echo off
rem Quincunx 1.1 - starts a Glide or OpenGL (MesaFX) game with Quincunx smoother.
rem From a command prompt:   GAME_smoother.bat "C:\Games\game.exe"
setlocal
if "%~1"=="" (
  echo The game path is missing. Example:
  echo    GAME_smoother.bat "C:\Games\game.exe"
  pause
  goto :eof
)
set SSTH3_SLI_AA_CONFIGURATION=11
set FX_GLIDE_QX_LOG=%~dp0qx.log
echo ---- %DATE% %TIME% game smoother %1 >> "%FX_GLIDE_QX_LOG%"
cd /d "%~dp1"
start "" /wait %*
endlocal
