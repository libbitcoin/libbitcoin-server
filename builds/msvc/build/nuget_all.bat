@ECHO OFF
ECHO Downloading libbitcoin vs2022 dependencies from NuGet
CALL nuget.exe install ..\vs2022\bs\packages.config
CALL nuget.exe install ..\vs2022\libbitcoin-server\packages.config
CALL nuget.exe install ..\vs2022\libbitcoin-server-test\packages.config
ECHO.
ECHO Downloading libbitcoin vs2019 dependencies from NuGet
CALL nuget.exe install ..\vs2019\bs\packages.config
CALL nuget.exe install ..\vs2019\libbitcoin-server\packages.config
CALL nuget.exe install ..\vs2019\libbitcoin-server-test\packages.config
ECHO.
ECHO Downloading libbitcoin vs2017 dependencies from NuGet
CALL nuget.exe install ..\vs2017\bs\packages.config
CALL nuget.exe install ..\vs2017\libbitcoin-server\packages.config
CALL nuget.exe install ..\vs2017\libbitcoin-server-test\packages.config
ECHO.