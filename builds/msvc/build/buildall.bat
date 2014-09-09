@ECHO OFF
ECHO.
ECHO Downloading libbitcoin_server dependencies from NuGet
CALL nuget.exe install ..\vs2013\libbitcoin_server\packages.config
ECHO.
CALL buildbase.bat ..\vs2013\libbitcoin_server.sln 12
ECHO.
PAUSE