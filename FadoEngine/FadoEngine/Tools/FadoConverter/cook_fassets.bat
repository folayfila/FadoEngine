@echo off
setlocal enabledelayedexpansion

pushd %~dp0..\..\
set ROOT=%CD%\
popd

set CONVERTER=%~dp0fado_converter.exe
set SRC_DIR=%ROOT%AssetsSource
set OUT_DIR=%ROOT%Assets

if not exist "%SRC_DIR%" (
    echo ERROR: assets_src not found at %SRC_DIR%
    pause
    exit /b 1
)

if not exist "%CONVERTER%" (
    echo ERROR: fado_converter.exe not found at %CONVERTER%
    pause
    exit /b 1
)

for /r "%SRC_DIR%" %%f in (*.png *.jpg *.tga *.bmp *.ttf) do (
    set "rel=%%~dpf"
    set "rel=!rel:%SRC_DIR%\=!"

    mkdir "%OUT_DIR%\!rel!" 2>nul

    set "name=%%~nf"
    %CONVERTER% "%%f" "%OUT_DIR%\!rel!!name!.fasset"
)

echo.
echo Assets cooked.
pause