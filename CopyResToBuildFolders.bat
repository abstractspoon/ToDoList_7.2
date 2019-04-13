ECHO OFF

pushd %~dp0
set REPO=%CD%
ECHO REPO=%REPO%

set PLUGINREPO=%REPO%\..\ToDoList_Plugins_7.2
ECHO PLUGINREPO=%PLUGINREPO%

set RESREPO=%REPO%\..\ToDoList_Resources
ECHO RESREPO=%RESREPO%

REM - Copy to Core
xcopy %RESREPO%\*.* %REPO%\ToDoList\Unicode_Debug\Resources\   /E /EXCLUDE:%REPO%\CopyResToBuildFolders_Exclude.txt /Y 
xcopy %RESREPO%\*.* %REPO%\ToDoList\Unicode_Release\Resources\   /E /EXCLUDE:%REPO%\CopyResToBuildFolders_Exclude.txt /Y 

REM - Copy Resources to Plugins
xcopy %RESREPO%\*.* %PLUGINREPO%\Debug\Resources\   /E /EXCLUDE:%REPO%\CopyResToBuildFolders_Exclude.txt /Y 
xcopy %RESREPO%\*.* %PLUGINREPO%\Release\Resources\ /E /EXCLUDE:%REPO%\CopyResToBuildFolders_Exclude.txt /Y 

popd
pause
