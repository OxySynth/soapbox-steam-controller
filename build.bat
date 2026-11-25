@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /LD /EHsc /O2 bridge.cpp steam_input_api.cpp ^
  /link /DLL /OUT:SteamControllerInput.asi ^
  advapi32.lib user32.lib shlwapi.lib setupapi.lib hid.lib
exit /b %errorlevel%
