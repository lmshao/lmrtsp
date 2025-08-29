@echo off
chcp 65001 >nul
echo Starting code formatting...

REM Check if clang-format is available
clang-format --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: clang-format not found, please install and add to PATH
    pause
    exit /b 1
)

REM Check if .clang-format config file exists
if not exist ".clang-format" (
    echo Warning: .clang-format config file not found
)

echo Searching and formatting all .h and .cpp files...

REM Format files in include directory
for /r include %%f in (*.h *.cpp) do (
    echo Formatting: %%~nxf
    clang-format -i "%%f"
)

REM Format files in src directory
for /r src %%f in (*.h *.cpp) do (
    echo Formatting: %%~nxf
    clang-format -i "%%f"
)

REM Format files in examples directory
for /r examples %%f in (*.h *.cpp) do (
    echo Formatting: %%~nxf
    clang-format -i "%%f"
)

echo.
echo Code formatting completed!
echo Used config file: .clang-format