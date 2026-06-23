
@echo off
:: FadoEngine Build Script
:: Place this next to premake5.lua and premake5.exe, in FadoEngine/
:: Run this to regenerate the Visual Studio solution on any machine.
 
echo Generating FadoEngine Visual Studio 2026 solution...
"%~dp0premake5.exe" vs2026
 
if %errorlevel% neq 0 (
    echo Failed to generate solution.
    pause
    exit /b 1
)
 
echo Done. Open FadoEngine.sln.
pause