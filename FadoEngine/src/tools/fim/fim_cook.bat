@echo off
setlocal enabledelayedexpansion

pushd %~dp0..\..\
set ROOT=%CD%\
popd

set CONVERTER=%~dp0fim_converter.exe
set SRC_DIR=%ROOT%assets_src
set OUT_DIR=%ROOT%assets

REM echo ROOT: %ROOT%
REM echo SRC:  %SRC_DIR%
REM echo OUT:  %OUT_DIR%
REM echo CONVERTER: %CONVERTER%
REM echo.

if not exist "%SRC_DIR%" (
    echo ERROR: assets_src not found at %SRC_DIR%
    pause
    exit /b 1
)

if not exist "%CONVERTER%" (
    echo ERROR: fim_converter.exe not found at %CONVERTER%
    pause
    exit /b 1
)

for /r "%SRC_DIR%" %%f in (*.png *.jpg *.tga *.bmp) do (
    set "rel=%%~dpf"
    set "rel=!rel:%SRC_DIR%\=!"

    mkdir "%OUT_DIR%\!rel!" 2>nul

    set "name=%%~nf"
    %CONVERTER% "%%f" "%OUT_DIR%\!rel!!name!.fim"
)

echo.
echo Assets cooked.
pause