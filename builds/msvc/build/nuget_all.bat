@ECHO OFF
ECHO Downloading libbitcoin vs2026 dependencies from NuGet
CALL nuget.exe install ..\vs2026\bs\packages.config
CALL nuget.exe install ..\vs2026\libbitcoin-server\packages.config
CALL nuget.exe install ..\vs2026\libbitcoin-server-test\packages.config
