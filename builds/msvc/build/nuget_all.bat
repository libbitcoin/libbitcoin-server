@ECHO OFF
ECHO Downloading libbitcoin vs2017 dependencies from NuGet
CALL nuget.exe install ..\vs2017\bs\packages.config
CALL nuget.exe install ..\vs2017\libbitcoin-server\packages.config
CALL nuget.exe install ..\vs2017\libbitcoin-server-test\packages.config
ECHO.
ECHO Downloading libbitcoin vs2015 dependencies from NuGet
CALL nuget.exe install ..\vs2015\bs\packages.config
CALL nuget.exe install ..\vs2015\libbitcoin-server\packages.config
CALL nuget.exe install ..\vs2015\libbitcoin-server-test\packages.config
ECHO.
ECHO Downloading libbitcoin vs2013 dependencies from NuGet
CALL nuget.exe install ..\vs2013\bs\packages.config
CALL nuget.exe install ..\vs2013\libbitcoin-server\packages.config
CALL nuget.exe install ..\vs2013\libbitcoin-server-test\packages.config
