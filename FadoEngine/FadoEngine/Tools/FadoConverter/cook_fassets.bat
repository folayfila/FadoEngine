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

set FORMATS=*.png *.jpg *.tga *.bmp *.glb *.ttf *.wav
for /r "%SRC_DIR%" %%f in (%FORMATS%) do (
    set "rel=%%~dpf"
    set "rel=!rel:%SRC_DIR%\=!"

    mkdir "%OUT_DIR%\!rel!" 2>nul

    set "name=%%~nf"
    set "ext=%%~xf"

    rem Default extension
    set "outExt=.fasset"

    if /I "!ext!"==".png" set "outExt=.fimage"
    if /I "!ext!"==".jpg" set "outExt=.fimage"
    if /I "!ext!"==".tga" set "outExt=.fimage"
    if /I "!ext!"==".bmp" set "outExt=.fimage"

    if /I "!ext!"==".glb" set "outExt=.fmodel"

    if /I "!ext!"==".ttf" set "outExt=.ffont"

    if /I "!ext!"==".wav" set "outExt=.fsound"

    %CONVERTER% "%%f" "%OUT_DIR%\!rel!!name!!outExt!"
)

echo.
echo Assets cooked.
pause