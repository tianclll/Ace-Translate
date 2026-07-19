@echo off
chcp 65001 >nul
setlocal

cd /d "%~dp0"

echo ============================================
echo  AceTranslatePro GPU 版编译
echo ============================================

if not exist build_all mkdir build_all
cd build_all
cmake .. -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [错误] CMake 配置失败
    pause
    exit /b 1
)

cmake --build . --config Release
if errorlevel 1 (
    echo [错误] 编译失败
    pause
    exit /b 1
)

echo ============================================
echo  编译完成！
echo  输出: %CD%\Release\
echo ============================================
pause
