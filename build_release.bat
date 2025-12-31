@echo off
echo Building WorldEditor in Release mode (without GPU validation)...

if not exist build_release mkdir build_release
cd build_release

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

echo.
echo Build complete! Run with:
echo .\build_release\bin\Release\WorldEditor.exe
pause