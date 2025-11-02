@echo off
REM ollygon Release build script for win64/VS2022

if not exist "build" mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.9.1/msvc2022_64"

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b %errorlevel%
)

cmake --build . --config Release

if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b %errorlevel%
)

echo.
echo Deploying Qt dependencies...
cd Release
C:\Qt\6.9.1\msvc2022_64\bin\windeployqt.exe ollygon.exe --release --no-translations

if %errorlevel% neq 0 (
    echo Warning: windeployqt failed - you may need to adjust the Qt path
    cd ..
    pause
    exit /b %errorlevel%
)

cd ..
echo.
echo Build complete!
echo Run: build\Release\ollygon.exe
pause
