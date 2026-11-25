@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Installer was not found. Install Visual Studio Build Tools with the C++ x86 tools.
  exit /b 1
)

set "VSROOT="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (
  echo No Visual Studio installation with the C++ x86 tools was found.
  exit /b 1
)

call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /LD /EHsc /O2 bridge.cpp steam_input_api.cpp ^
  /link /DLL /OUT:SteamControllerInput.asi ^
  advapi32.lib user32.lib shlwapi.lib setupapi.lib hid.lib
exit /b %errorlevel%
