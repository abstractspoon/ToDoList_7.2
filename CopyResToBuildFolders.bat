REM - Copy Resources to own build folders

cd ..
echo %cd%

REM - Copy Resources to latest release

xcopy .\ToDoList_Resources\*.* .\ToDoList_7.2\ToDoList\Unicode_Debug\Resources\   /E /EXCLUDE:.\ToDoList_7.2\CopyResToBuildFolders_Exclude.txt /Y 

xcopy .\ToDoList_Resources\*.* .\ToDoList_7.2\ToDoList\Unicode_Release\Resources\ /E /EXCLUDE:.\ToDoList_7.2\CopyResToBuildFolders_Exclude.txt /Y 

REM - Copy Resources to Plugins

xcopy .\ToDoList_Resources\*.* .\ToDoList_Plugins_7.2\Debug\Resources\   /E /EXCLUDE:.\ToDoList_7.2\CopyResToBuildFolders_Exclude.txt /Y 

xcopy .\ToDoList_Resources\*.* .\ToDoList_Plugins_7.2\Release\Resources\ /E /EXCLUDE:.\ToDoList_7.2\CopyResToBuildFolders_Exclude.txt /Y 
