@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl /nologo /EHsc /std:c++17 "D:\AceTranslatePro\AceTranslatePro\tools\make_rounded_ico.cpp" /Fe:D:\AceTranslatePro\AceTranslatePro\tools\make_rounded_ico.exe /link gdiplus.lib
if exist "D:\AceTranslatePro\AceTranslatePro\tools\make_rounded_ico.exe" (
    echo Compilation OK
    "D:\AceTranslatePro\AceTranslatePro\tools\make_rounded_ico.exe"
) else (
    echo Compilation FAILED
)
pause
