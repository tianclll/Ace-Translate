@echo off
chcp 65001 >nul
setlocal

cd /d "%~dp0"

echo ============================================
echo  编译 CPU 版（无 CUDA 依赖）
echo ============================================

:: 替换为 CPU 版配置
copy /y CMakeLists_cpu.txt CMakeLists.txt >nul
copy /y deps\translator\CMakeLists_cpu.txt deps\translator\CMakeLists.txt >nul
copy /y deps\vlm\CMakeLists_cpu.txt deps\vlm\CMakeLists.txt >nul
copy /y deps\asr\CMakeLists_cpu.txt deps\asr\CMakeLists.txt >nul

:: 编译
if not exist build_cpu mkdir build_cpu
cd build_cpu
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
echo  CPU 版编译完成！
echo  输出: %CD%\Release\
echo ============================================
pause
