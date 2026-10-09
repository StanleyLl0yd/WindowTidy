@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
if errorlevel 1 exit /b 1

where cl.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: cl.exe not found. Run from an x64 Native Tools Command Prompt.
  exit /b 1
)
where rc.exe >nul 2>nul
if errorlevel 1 (
  echo ERROR: rc.exe not found. Install Windows SDK.
  exit /b 1
)
if not exist "build\x64" mkdir "build\x64"
if errorlevel 1 exit /b 1

echo Building Window Tidy...
cl /nologo /std:c++20 /W4 /WX /EHsc /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /O2 /MD /Iinclude /c src\main.cpp /Fo"build\x64\main.obj"
if errorlevel 1 exit /b 1
rc /nologo /fo "build\x64\WindowTidy.res" "res\WindowTidy.rc"
if errorlevel 1 exit /b 1
link /nologo /SUBSYSTEM:WINDOWS /MACHINE:X64 /DYNAMICBASE /NXCOMPAT /OPT:REF /OPT:ICF /MANIFEST:NO /OUT:"build\x64\WindowTidy.exe" "build\x64\main.obj" "build\x64\WindowTidy.res" user32.lib shell32.lib advapi32.lib comctl32.lib dwmapi.lib
if errorlevel 1 exit /b 1

if /I not "%~1"=="test" goto done
echo Building tests...
cl /nologo /std:c++20 /W4 /WX /EHsc /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /O2 /MD /Iinclude tests\logic_tests.cpp /Fe:"build\x64\logic_tests.exe" /Fo:"build\x64\logic_tests.obj"
if errorlevel 1 exit /b 1
echo Running tests...
"build\x64\logic_tests.exe"
if errorlevel 1 exit /b 1
:done
echo Build completed.
exit /b 0
