@ECHO OFF
ECHO.
ECHO Downloading obelisk dependencies from NuGet
CALL nuget.exe install ..\vs2013\obelisk\packages.config
ECHO.
CALL buildbase.bat ..\vs2013\obelisk.sln 12
ECHO.
PAUSE