call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl fado_converter.cpp ../../ThirdParty/lz4/lz4.c

echo.
echo Compiled fado_converter.cpp.
pause