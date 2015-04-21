@ECHO OFF
ECHO.
ECHO Downloading libbitcoin-server dependencies from NuGet
CALL nuget.exe install ..\vs2013\bs\packages.config
ECHO.
CALL buildbase.bat ..\vs2013\libbitcoin-server.sln 12
ECHO.
PAUSE
