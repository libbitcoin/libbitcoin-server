# Windows twin of profile.sh: emits the identical CSV schema so downstream
# event.log integration and plotting are platform-agnostic. Metric sources
# are the locale-independent Win32_PerfFormattedData CIM classes (perfmon
# counter names localize; CIM class/property names do not). GPU metrics use
# nvidia-smi, which ships with the NVIDIA driver and matches Linux output.
#
# Platform notes (columns that differ from Linux semantics):
# - buffers_kB: no Windows analogue, logs 0.
# - cached_kB: system cache resident bytes; dirty_kB: modified page list.
# - mapped_kB: no direct analogue, logs 0.
# - min_flt_s: process total page faults/s (Windows does not split); the
#   maj_flt_s column logs SYSTEM-WIDE hard faults (Memory PagesInputPersec).
# - rss_anon_kB: working set private; rss_file_kB: sharable working set
#   (working set - private). vm_swap_kB logs 0.
# - psi_*: no Windows analogue, log 0.
#
# .\profile-windows.ps1 -Binary .\bn.exe -Args '-c .\bn.cfg' -Interval 5 -Disk '0 C:' -Nic 'Ethernet'

param(
    [string]$Binary = ".\bn.exe",
    [string]$Args = "",
    [int]$Interval = 5,
    [switch]$Debug_,
    [switch]$LogOutput,
    [Parameter(Mandatory=$true)][string]$Disk,
    [Parameter(Mandatory=$true)][string]$Nic,
    [switch]$Gpu,
    [string]$OutputCsv = "profile.csv"
)

$ErrorActionPreference = "SilentlyContinue"

# Enumerate instances for operator convenience (mirror usage() in bash).
function Show-Devices {
    Write-Host "Available Disks:"
    Get-CimInstance Win32_PerfFormattedData_PerfDisk_PhysicalDisk |
        Where-Object { $_.Name -ne "_Total" } | ForEach-Object { "  " + $_.Name }
    Write-Host "Available NICs:"
    Get-CimInstance Win32_PerfFormattedData_Tcpip_NetworkInterface |
        ForEach-Object { "  " + $_.Name }
}

$numCores = [Environment]::ProcessorCount
if ($Debug_) { Write-Host "Detected $numCores CPU cores" }

# Identical header construction to profile.sh.
$header = "time_s,cpu_usage_total"
for ($i = 0; $i -lt $numCores; $i++) { $header += ",cpu${i}_usage" }
$header += ",mem_total_kB,mem_free_kB,mem_available_kB,cached_kB,buffers_kB,mapped_kB,dirty_kB,swap_total_kB,swap_free_kB"
$header += ",process_io_read_kB_s,process_io_write_kB_s,min_flt_s,maj_flt_s,disk_read_kB_s,disk_write_kB_s"
$header += ",net_rx_kB_s,net_tx_kB_s"
$header += ",rss_anon_kB,rss_file_kB,vm_swap_kB,psi_mem_some10,psi_mem_full10,psi_io_some10,psi_io_full10"
if ($Gpu) {
    $header += ",gpu_usage_pct,gpu_mem_usage_pct,gpu_mem_used_MB,gpu_temp_C,gpu_power_draw_W"
    $header += ",gpu_sm_clock_MHz,gpu_mem_clock_MHz,gpu_pcie_rx_MB_s,gpu_pcie_tx_MB_s,gpu_proc_mem_MB"
}
$header += ",milestone"
Set-Content -Path $OutputCsv -Value $header -Encoding utf8

# Launch the profiled binary.
if ($LogOutput) {
    $proc = Start-Process -FilePath $Binary -ArgumentList $Args -PassThru -NoNewWindow `
        -RedirectStandardOutput "bn.stdout.log" -RedirectStandardError "bn.stderr.log"
} else {
    $proc = Start-Process -FilePath $Binary -ArgumentList $Args -PassThru -NoNewWindow
}
if ($null -eq $proc) { Write-Host "Failed to start $Binary"; Show-Devices; exit 1 }
$binaryPid = $proc.Id
if ($Debug_) { Write-Host "Binary PID: $binaryPid" }

Start-Sleep -Seconds 2

$memTotalKb = [long]((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1024)
$startTime = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()

while ($true) {
    $timeS = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $startTime

    if ($proc.HasExited) {
        if ($Debug_) { Write-Host "Binary has exited, stopping profiling." }
        exit 0
    }

    # CPU per core and total (PercentProcessorTime is interval-formatted).
    $cpus = Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor
    $totalUsage = ($cpus | Where-Object { $_.Name -eq "_Total" }).PercentProcessorTime
    if ($null -eq $totalUsage) { $totalUsage = 0 }
    $coreUsage = @(0) * $numCores
    foreach ($c in $cpus) {
        $idx = 0
        if ([int]::TryParse($c.Name, [ref]$idx) -and $idx -lt $numCores) {
            $coreUsage[$idx] = $c.PercentProcessorTime
        }
    }

    # Memory (kB): available/cache/modified from PerfOS_Memory, free from OS.
    $mem = Get-CimInstance Win32_PerfFormattedData_PerfOS_Memory
    $os = Get-CimInstance Win32_OperatingSystem
    $memFree = [long]$os.FreePhysicalMemory
    $memAvailable = [long]$mem.AvailableKBytes
    $cached = [long]($mem.SystemCacheResidentBytes / 1024)
    $buffers = 0
    $mapped = 0
    $dirty = [long]($mem.ModifiedPageListBytes / 1024)
    $pagesInput = [long]$mem.PagesInputPersec

    # Swap (page file, MB to kB).
    $pf = Get-CimInstance Win32_PageFileUsage | Select-Object -First 1
    if ($null -ne $pf) {
        $swapTotal = [long]$pf.AllocatedBaseSize * 1024
        $swapFree = ([long]$pf.AllocatedBaseSize - [long]$pf.CurrentUsage) * 1024
    } else {
        $swapTotal = 0
        $swapFree = 0
    }

    # Process counters, instance resolved by PID (rename-safe per loop).
    $pp = Get-CimInstance Win32_PerfFormattedData_PerfProc_Process |
        Where-Object { $_.IDProcess -eq $binaryPid } | Select-Object -First 1
    if ($null -ne $pp) {
        $procReadKbS = [math]::Round($pp.IOReadBytesPersec / 1000, 2)
        $procWriteKbS = [math]::Round($pp.IOWriteBytesPersec / 1000, 2)
        $minFltS = [long]$pp.PageFaultsPersec
        $rssAnon = [long]($pp.WorkingSetPrivate / 1024)
        $rssFile = [long](($pp.WorkingSet - $pp.WorkingSetPrivate) / 1024)
        if ($rssFile -lt 0) { $rssFile = 0 }
    } else {
        $procReadKbS = 0; $procWriteKbS = 0; $minFltS = 0
        $rssAnon = 0; $rssFile = 0
    }
    # System-wide hard faults stand in for majflt (no per-process split).
    $majFltS = $pagesInput
    $vmSwap = 0

    # Disk (instance name like "0 C:").
    $pd = Get-CimInstance Win32_PerfFormattedData_PerfDisk_PhysicalDisk |
        Where-Object { $_.Name -eq $Disk } | Select-Object -First 1
    if ($null -ne $pd) {
        $diskReadKbS = [math]::Round($pd.DiskReadBytesPersec / 1000, 2)
        $diskWriteKbS = [math]::Round($pd.DiskWriteBytesPersec / 1000, 2)
    } else {
        $diskReadKbS = 0; $diskWriteKbS = 0
    }

    # Network.
    $ni = Get-CimInstance Win32_PerfFormattedData_Tcpip_NetworkInterface |
        Where-Object { $_.Name -like "*$Nic*" } | Select-Object -First 1
    if ($null -ne $ni) {
        $netRxKbS = [math]::Round($ni.BytesReceivedPersec / 1000, 2)
        $netTxKbS = [math]::Round($ni.BytesSentPersec / 1000, 2)
    } else {
        $netRxKbS = 0; $netTxKbS = 0
    }

    # No PSI on Windows.
    $psiMemSome = 0; $psiMemFull = 0; $psiIoSome = 0; $psiIoFull = 0

    # GPU via nvidia-smi (identical to the Linux collection).
    if ($Gpu) {
        $gpuLine = (nvidia-smi --query-gpu=utilization.gpu,utilization.memory,memory.used,temperature.gpu,power.draw,clocks.sm,clocks.mem --format=csv,noheader,nounits 2>$null | Select-Object -First 1)
        if ($gpuLine) {
            $g = $gpuLine -split "," | ForEach-Object { $_.Trim() }
            $gpuUsage = $g[0]; $gpuMemUsage = $g[1]; $gpuMemUsed = $g[2]
            $gpuTemp = $g[3]; $gpuPower = $g[4]; $gpuSmClock = $g[5]; $gpuMemClock = $g[6]
        } else {
            $gpuUsage = 0; $gpuMemUsage = 0; $gpuMemUsed = 0
            $gpuTemp = 0; $gpuPower = 0; $gpuSmClock = 0; $gpuMemClock = 0
        }

        $pcieLine = (nvidia-smi dmon -c 1 -s t 2>$null | Where-Object { $_ -match "^\s*0\s" } | Select-Object -First 1)
        if ($pcieLine) {
            $p = ($pcieLine.Trim() -split "\s+")
            $gpuPcieRx = $p[1]; $gpuPcieTx = $p[2]
        } else {
            $gpuPcieRx = 0; $gpuPcieTx = 0
        }

        $procMemLine = (nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>$null |
            Where-Object { ($_ -split ",")[0].Trim() -eq "$binaryPid" } | Select-Object -First 1)
        if ($procMemLine) {
            $gpuProcMem = (($procMemLine -split ",")[1]).Trim()
        } else {
            $gpuProcMem = 0
        }
    }

    $csvLine = "$timeS,$totalUsage"
    foreach ($u in $coreUsage) { $csvLine += ",$u" }
    $csvLine += ",$memTotalKb,$memFree,$memAvailable,$cached,$buffers,$mapped,$dirty,$swapTotal,$swapFree"
    $csvLine += ",$procReadKbS,$procWriteKbS,$minFltS,$majFltS,$diskReadKbS,$diskWriteKbS"
    $csvLine += ",$netRxKbS,$netTxKbS"
    $csvLine += ",$rssAnon,$rssFile,$vmSwap,$psiMemSome,$psiMemFull,$psiIoSome,$psiIoFull"
    if ($Gpu) {
        $csvLine += ",$gpuUsage,$gpuMemUsage,$gpuMemUsed,$gpuTemp,$gpuPower"
        $csvLine += ",$gpuSmClock,$gpuMemClock,$gpuPcieRx,$gpuPcieTx,$gpuProcMem"
    }
    $csvLine += ",0"

    Add-Content -Path $OutputCsv -Value $csvLine -Encoding utf8

    if ($Debug_) {
        Write-Host "Logged data at ${timeS}s: CPU=${totalUsage}%, Disk R/W=${diskReadKbS}/${diskWriteKbS} kB/s"
    }

    Start-Sleep -Seconds $Interval
}
