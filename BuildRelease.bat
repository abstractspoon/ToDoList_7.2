ECHO OFF
CLS

pushd %~dp0
set REPO=%CD%
ECHO REPO=%REPO%

if NOT EXIST %REPO%\ToDoList exit
if NOT EXIST %REPO%\..\ToDoList_Plugins_7.2 exit

ECHO ON

REM - Build Core App
cd %REPO%\ToDoList
"C:\Program Files (x86)\Microsoft Visual Studio\Common\MSDev98\Bin\msdev.exe" .\ToDoList_All.dsw /MAKE "ALL - Win32 Unicode Release" 

REM - Build Plugins
cd %REPO%\..\ToDoList_Plugins_7.2

REM - Rebuild PluginHelpers by itself because everything else is dependent on it
"C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE\devenv.com" .\PluginHelpers.sln /Rebuild "Release"

REM - Build rest of plugins
"C:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE\devenv.com" .\ToDoList_Plugins.sln /Build "Release"

REM Allow caller to cancel building Zip
pause

CALL %REPO%\BuildReleaseZip.bat

popd
