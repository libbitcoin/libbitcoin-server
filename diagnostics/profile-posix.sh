#!/bin/bash

# Force a fixed locale so tools always print '.' decimals (a ',' would be
# misread as a CSV field separator and shift every column after it).
export LC_ALL=C

# POSIX sampler (Linux and macOS): profiles a binary by starting it and
# logging system and process metrics to a CSV with a fixed cross-platform
# schema (profile-windows.ps1 emits the same schema). Platform collection is
# dispatched on uname; columns with no platform analogue log 0.
#
# Platform notes (macOS columns that differ from Linux semantics):
# - per-core CPU: unavailable without sudo powermetrics; cores log 0 and
#   cpu_usage_total is real (derived from top).
# - buffers_kB, mapped_kB, dirty_kB: no macOS analogue, log 0.
# - process_io_*: not exposed per-process without dtrace, log 0.
# - min_flt_s/maj_flt_s: top faults/pageins deltas (pageins ~ major).
# - disk_read_kB_s: macOS iostat reports COMBINED throughput; it is logged
#   here and disk_write_kB_s logs 0.
# - rss_anon_kB: physical footprint (top mem); rss_file_kB is rss - footprint
#   clamped at zero; vm_swap_kB logs 0.
# - psi_mem_some10: kern.memorystatus_vm_pressure_level (1=normal 2=warn
#   4=critical), not a stall percentage; other psi columns log 0.
# - GPU (-g): powermetrics residency/frequency (needs sudo); pcie and proc
#   mem log 0 (on-die GPU, unified memory).
#
# ./profile-posix.sh -b './bn' -a '-c ./bn.cfg' -i 5 -D nvme0n1p6 -n wlp2s0
# ./profile-posix.sh -b './bs' -a '--config ./bs.cfg' -i 5 -D disk0 -n en0

os=$(uname -s)   # Linux | Darwin

usage() {
    echo "Usage: ./profile-posix.sh [-b <binary>] [-a <args>] [-i <interval_secs>] [-d] [-l] -D <disk> -n <nic> [-g] [-o <output_csv>]"
    echo "  -b: Binary to profile (default: ./bn)"
    echo "  -a: Arguments for binary (quote if multi-word, e.g., '-c ./bn.cfg')"
    echo "  -i: Logging frequency in seconds (default: 5)"
    echo "  -d: Enable debug output (default: no)"
    echo "  -l: Log binary stdout/stderr to bn.stdout.log / bn.stderr.log (default: no)"
    echo "  -D: Disk/SSD device name (mandatory, e.g., sda or disk0)"
    echo "  -n: Network interface name (mandatory, e.g., eth0 or en0)"
    echo "  -g: Enable GPU profiling (nvidia-smi; powermetrics on macOS, needs sudo)"
    echo "  -o: Output CSV file (default: profile.csv)"
    echo ""
    echo "Available Disks:"
    if [ "$os" = "Darwin" ]; then
        diskutil list physical 2>/dev/null | grep "^/dev/" | awk '{print $1}' | sed 's|/dev/||' | head -10
    else
        lsblk -o NAME -n -d | grep -v loop | sed 's/^[[:space:]]*//' | head -10
    fi
    echo ""
    echo "Available NICs:"
    if [ "$os" = "Darwin" ]; then
        ifconfig -l | tr ' ' '\n' | grep -v "^lo" | head -10
    else
        ip -o link show | awk '{print $2}' | sed 's/://' | grep -v lo | head -10
    fi
    exit 1
}

# Parse options
binary="./bn"
args=""
interval=5
debug=0
log_output=0
disk=""
nic=""
gpu=0
output_csv="profile.csv"

while getopts ":b:a:i:dlD:n:go:" opt; do
    case $opt in
        b) binary="$OPTARG" ;;
        a) args="$OPTARG" ;;
        i) interval="$OPTARG" ;;
        d) debug=1 ;;
        l) log_output=1 ;;
        D) disk="$OPTARG" ;;
        n) nic="$OPTARG" ;;
        g) gpu=1 ;;
        o) output_csv="$OPTARG" ;;
        \?) echo "Invalid option -$OPTARG"; usage ;;
    esac
done

if [ -z "$disk" ] || [ -z "$nic" ]; then
    echo "Error: Disk and NIC parameters are mandatory."
    usage
fi

# GPU on Linux needs nvidia-smi; on macOS powermetrics needs root.
if [ $gpu -eq 1 ]; then
    if [ "$os" = "Darwin" ]; then
        sudo -v
    elif ! command -v nvidia-smi >/dev/null 2>&1; then
        echo "Error: -g given but nvidia-smi was not found in PATH."
        usage
    fi
fi

# Authenticate sudo up front on Linux (as before, for tool access if needed).
if [ "$os" = "Linux" ]; then sudo -v; fi

mkdir -p ./tmp
rm -f ./tmp/*.tmp

if [ "$os" = "Darwin" ]; then
    num_cores=$(sysctl -n hw.ncpu)
else
    num_cores=$(nproc 2>/dev/null || grep -c '^cpu[0-9]' /proc/stat)
fi
if [ $debug -eq 1 ]; then echo "Detected $num_cores CPU cores ($os)"; fi

# Cross-platform CSV schema (profile-windows.ps1 must match).
header="time_s,cpu_usage_total"
for ((i=0; i<num_cores; i++)); do
    header="$header,cpu${i}_usage"
done
header="$header,mem_total_kB,mem_free_kB,mem_available_kB,cached_kB,buffers_kB,mapped_kB,dirty_kB,swap_total_kB,swap_free_kB"
header="$header,process_io_read_kB_s,process_io_write_kB_s,min_flt_s,maj_flt_s,disk_read_kB_s,disk_write_kB_s"
header="$header,net_rx_kB_s,net_tx_kB_s"
header="$header,rss_anon_kB,rss_file_kB,vm_swap_kB,psi_mem_some10,psi_mem_full10,psi_io_some10,psi_io_full10"
if [ $gpu -eq 1 ]; then
    header="$header,gpu_usage_pct,gpu_mem_usage_pct,gpu_mem_used_MB,gpu_temp_C,gpu_power_draw_W"
    header="$header,gpu_sm_clock_MHz,gpu_mem_clock_MHz,gpu_pcie_rx_MB_s,gpu_pcie_tx_MB_s,gpu_proc_mem_MB"
fi
header="$header,milestone"

echo "$header" > "$output_csv"

trap 'echo "Caught SIGINT, stopping..."; kill -INT $binary_pid 2>/dev/null; kill $iostat_pid 2>/dev/null; wait $binary_pid 2>/dev/null; rm -f ./tmp/*.tmp; exit 0' SIGINT

# Start the binary without root
if [ $log_output -eq 1 ]; then
    if [ $debug -eq 1 ]; then echo "Starting binary with logging: $binary $args"; fi
    $binary $args > bn.stdout.log 2> bn.stderr.log &
else
    if [ $debug -eq 1 ]; then echo "Starting binary: $binary $args"; fi
    $binary $args &
fi
binary_pid=$!
if [ $debug -eq 1 ]; then echo "Binary PID: $binary_pid"; fi

sleep 2

# Continuous iostat on the target disk (skip first report; Linux extended
# stats split read/write, macOS reports combined MB/s).
if [ "$os" = "Darwin" ]; then
    iostat -d -w $interval $disk > ./tmp/iostat.tmp 2>/dev/null &
else
    iostat -d -k -x -y -p $disk $interval > ./tmp/iostat.tmp 2>/dev/null &
fi
iostat_pid=$!
if [ $debug -eq 1 ]; then echo "iostat PID: $iostat_pid"; fi

sleep $((interval + 1))

start_time=$(date +%s)

# Platform-specific initialization and previous-counter state.
if [ "$os" = "Darwin" ]; then
    page_size=$(sysctl -n vm.pagesize)
    mem_total=$(( $(sysctl -n hw.memsize) / 1024 ))
    prev_net_rx=0
    prev_net_tx=0
    prev_faults=0
    prev_pageins=0
    first_sample=1
else
    echo "settings at start" > dirty_paging_settings.log
    sysctl vm.dirty_background_ratio vm.dirty_ratio vm.dirty_expire_centisecs vm.dirty_writeback_centisecs > ./dirty_paging_settings.log
    prev_stat="./tmp/prev_stat.tmp"
    prev_net="./tmp/prev_net.tmp"
    cat /proc/stat > "$prev_stat"
    cat /proc/net/dev > "$prev_net"
fi

# ---------------------------------------------------------------------------
# Linux collectors
# ---------------------------------------------------------------------------

collect_cpu_Linux() {
    cat /proc/stat > ./tmp/stat.tmp
    cpu_usages=()
    local total_diff=0
    local total_idle_diff=0
    total_usage=0.00
    if command -v bc >/dev/null 2>&1; then
        for (( core=0; core<num_cores; core++ )); do
            local prev_line=$(grep "^cpu${core} " "$prev_stat")
            local curr_line=$(grep "^cpu${core} " ./tmp/stat.tmp)
            if [ -n "$prev_line" ] && [ -n "$curr_line" ]; then
                local prev_fields=($prev_line)
                local curr_fields=($curr_line)
                local user_diff=$(( ${curr_fields[1]} - ${prev_fields[1]} ))
                local nice_diff=$(( ${curr_fields[2]} - ${prev_fields[2]} ))
                local system_diff=$(( ${curr_fields[3]} - ${prev_fields[3]} ))
                local idle_diff=$(( ${curr_fields[4]} - ${prev_fields[4]} ))
                local iowait_diff=$(( ${curr_fields[5]} - ${prev_fields[5]} ))
                local irq_diff=$(( ${curr_fields[6]} - ${prev_fields[6]} ))
                local softirq_diff=$(( ${curr_fields[7]} - ${prev_fields[7]} ))
                local steal_diff=$(( ${curr_fields[8]:-0} - ${prev_fields[8]:-0} ))
                local total_core_diff=$(( user_diff + nice_diff + system_diff + idle_diff + iowait_diff + irq_diff + softirq_diff + steal_diff ))
                local usage="0.00"
                if [ $total_core_diff -gt 0 ]; then
                    usage=$(echo "scale=2; 100 * (1 - $idle_diff / $total_core_diff)" | bc -l 2>/dev/null || echo "0.00")
                fi
                cpu_usages+=("$usage")
                total_diff=$((total_diff + total_core_diff))
                total_idle_diff=$((total_idle_diff + idle_diff))
            else
                cpu_usages+=("0.00")
            fi
        done
        if [ $total_diff -gt 0 ]; then
            total_usage=$(echo "scale=2; 100 * (1 - $total_idle_diff / $total_diff)" | bc -l 2>/dev/null || echo "0.00")
        fi
    else
        for (( core=0; core<num_cores; core++ )); do
            cpu_usages+=("0.00")
        done
    fi
    cp ./tmp/stat.tmp "$prev_stat"
}

collect_memory_Linux() {
    cat /proc/meminfo > ./tmp/meminfo.tmp
    mem_total=$(grep '^MemTotal:' ./tmp/meminfo.tmp | awk '{print $2}')
    mem_free=$(grep '^MemFree:' ./tmp/meminfo.tmp | awk '{print $2}')
    mem_available=$(grep '^MemAvailable:' ./tmp/meminfo.tmp | awk '{print $2}')
    cached=$(grep '^Cached:' ./tmp/meminfo.tmp | awk '{print $2}')
    buffers=$(grep '^Buffers:' ./tmp/meminfo.tmp | awk '{print $2}')
    mapped=$(grep '^Mapped:' ./tmp/meminfo.tmp | awk '{print $2}')
    dirty=$(grep '^Dirty:' ./tmp/meminfo.tmp | awk '{print $2}')
    swap_total=$(grep '^SwapTotal:' ./tmp/meminfo.tmp | awk '{print $2}')
    swap_free=$(grep '^SwapFree:' ./tmp/meminfo.tmp | awk '{print $2}')
}

collect_process_Linux() {
    # Match on the PID column: with LC_ALL=C the timestamp is a single field,
    # so process rows are Time UID PID <data>. Process rows aggregate threads.
    pidstat -d 1 1 -p $binary_pid 2>/dev/null | awk -v pid=$binary_pid '$3 == pid {print $4,$5; exit}' > ./tmp/pidstat_io.tmp 2>/dev/null
    read process_read_kb_s process_write_kb_s < ./tmp/pidstat_io.tmp
    process_read_kb_s=${process_read_kb_s:-0}
    process_write_kb_s=${process_write_kb_s:-0}

    # majflt/s is the mapped-read signal.
    pidstat -r 1 1 -p $binary_pid 2>/dev/null | awk -v pid=$binary_pid '$3 == pid {print $4,$5; exit}' > ./tmp/pidstat_page.tmp 2>/dev/null
    read min_flt_s maj_flt_s < ./tmp/pidstat_page.tmp
    min_flt_s=${min_flt_s:-0}
    maj_flt_s=${maj_flt_s:-0}

    # Residency split (RssAnon holds staging and heads, RssFile the mapped
    # store) and swap.
    rss_anon=$(awk '/^RssAnon:/ {print $2}' /proc/$binary_pid/status 2>/dev/null)
    rss_file=$(awk '/^RssFile:/ {print $2}' /proc/$binary_pid/status 2>/dev/null)
    vm_swap=$(awk '/^VmSwap:/ {print $2}' /proc/$binary_pid/status 2>/dev/null)
    rss_anon=${rss_anon:-0}
    rss_file=${rss_file:-0}
    vm_swap=${vm_swap:-0}

    # PSI stall averages (memory and io, some and full, avg10).
    psi_mem_some=$(awk '/^some/ {split($2,a,"="); print a[2]}' /proc/pressure/memory 2>/dev/null)
    psi_mem_full=$(awk '/^full/ {split($2,a,"="); print a[2]}' /proc/pressure/memory 2>/dev/null)
    psi_io_some=$(awk '/^some/ {split($2,a,"="); print a[2]}' /proc/pressure/io 2>/dev/null)
    psi_io_full=$(awk '/^full/ {split($2,a,"="); print a[2]}' /proc/pressure/io 2>/dev/null)
    psi_mem_some=${psi_mem_some:-0}
    psi_mem_full=${psi_mem_full:-0}
    psi_io_some=${psi_io_some:-0}
    psi_io_full=${psi_io_full:-0}
}

collect_disk_Linux() {
    local latest_iostat=$(grep "^$disk " ./tmp/iostat.tmp | tail -n 1)
    if [ $debug -eq 1 ]; then echo "$latest_iostat"; fi
    disk_read_kb_s=$(echo "$latest_iostat" | awk '{print $3; exit}')
    disk_write_kb_s=$(echo "$latest_iostat" | awk '{print $9; exit}')
    disk_read_kb_s=${disk_read_kb_s:-0}
    disk_write_kb_s=${disk_write_kb_s:-0}
}

collect_net_Linux() {
    cat /proc/net/dev > ./tmp/netdev.tmp
    local prev_net_line=$(grep "${nic}:" "$prev_net" || echo "${nic}: 0 0 0 0 0 0 0 0 0 0")
    local curr_net_line=$(grep "${nic}:" ./tmp/netdev.tmp || echo "${nic}: 0 0 0 0 0 0 0 0 0 0")
    local prev_rx=$(echo "$prev_net_line" | awk -F' ' '{print $2}')
    local prev_tx=$(echo "$prev_net_line" | awk -F' ' '{print $10}')
    local curr_rx=$(echo "$curr_net_line" | awk -F' ' '{print $2}')
    local curr_tx=$(echo "$curr_net_line" | awk -F' ' '{print $10}')
    local rx_diff=$((curr_rx - prev_rx))
    local tx_diff=$((curr_tx - prev_tx))
    if command -v bc >/dev/null 2>&1; then
        net_rx_kb_s=$(echo "scale=2; ($rx_diff / 1000) / $interval" | bc -l 2>/dev/null || echo "0.00")
        net_tx_kb_s=$(echo "scale=2; ($tx_diff / 1000) / $interval" | bc -l 2>/dev/null || echo "0.00")
    else
        net_rx_kb_s="0.00"
        net_tx_kb_s="0.00"
    fi
    cp ./tmp/netdev.tmp "$prev_net"
}

collect_gpu_Linux() {
    local gpu_line=$(nvidia-smi --query-gpu=utilization.gpu,utilization.memory,memory.used,temperature.gpu,power.draw,clocks.sm,clocks.mem --format=csv,noheader,nounits 2>/dev/null | head -n 1)
    IFS=',' read -r gpu_usage gpu_mem_usage gpu_mem_used gpu_temp gpu_power gpu_sm_clock gpu_mem_clock <<< "$gpu_line"
    gpu_usage=$(echo "${gpu_usage:-0}" | xargs)
    gpu_mem_usage=$(echo "${gpu_mem_usage:-0}" | xargs)
    gpu_mem_used=$(echo "${gpu_mem_used:-0}" | xargs)
    gpu_temp=$(echo "${gpu_temp:-0}" | xargs)
    gpu_power=$(echo "${gpu_power:-0.00}" | xargs)
    gpu_sm_clock=$(echo "${gpu_sm_clock:-0}" | xargs)
    gpu_mem_clock=$(echo "${gpu_mem_clock:-0}" | xargs)

    # PCIe throughput (host<->device batch transfer) is only exposed via dmon
    # (-s t: rx/tx in MB/s); one-shot sample, columns 2 and 3.
    local gpu_pcie_line=$(nvidia-smi dmon -c 1 -s t 2>/dev/null | awk '$1 == 0 {print $2,$3; exit}')
    read gpu_pcie_rx gpu_pcie_tx <<< "$gpu_pcie_line"
    gpu_pcie_rx=$(echo "${gpu_pcie_rx:-0}" | xargs)
    gpu_pcie_tx=$(echo "${gpu_pcie_tx:-0}" | xargs)

    # Device memory held by the profiled binary (batch buffers).
    gpu_proc_mem=$(nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>/dev/null | awk -F', *' -v pid=$binary_pid '$1 == pid {print $2; exit}')
    gpu_proc_mem=$(echo "${gpu_proc_mem:-0}" | xargs)
}

# ---------------------------------------------------------------------------
# macOS (Darwin) collectors
# ---------------------------------------------------------------------------

collect_cpu_Darwin() {
    # Total from top (second sample of two is current-interval); per-core is
    # unavailable without sudo powermetrics, cores log 0.
    local cpu_line=$(top -l 2 -n 0 -s 1 2>/dev/null | grep "CPU usage" | tail -1)
    local cpu_user=$(echo "$cpu_line" | awk '{print $3}' | tr -d '%')
    local cpu_sys=$(echo "$cpu_line" | awk '{print $5}' | tr -d '%')
    total_usage=$(echo "${cpu_user:-0} ${cpu_sys:-0}" | awk '{printf "%.2f", $1 + $2}')
    cpu_usages=()
    for (( core=0; core<num_cores; core++ )); do
        cpu_usages+=("0.00")
    done
}

collect_memory_Darwin() {
    vm_stat > ./tmp/vmstat.tmp
    local pg_free=$(awk '/^Pages free/ {gsub("\\.",""); print $3}' ./tmp/vmstat.tmp)
    local pg_spec=$(awk '/^Pages speculative/ {gsub("\\.",""); print $3}' ./tmp/vmstat.tmp)
    local pg_inactive=$(awk '/^Pages inactive/ {gsub("\\.",""); print $3}' ./tmp/vmstat.tmp)
    local pg_purgeable=$(awk '/^Pages purgeable/ {gsub("\\.",""); print $3}' ./tmp/vmstat.tmp)
    local pg_file=$(awk '/^File-backed pages/ {gsub("\\.",""); print $3}' ./tmp/vmstat.tmp)
    local to_kb=$((page_size / 1024))
    mem_free=$(( (${pg_free:-0} + ${pg_spec:-0}) * to_kb ))
    mem_available=$(( (${pg_free:-0} + ${pg_spec:-0} + ${pg_inactive:-0} + ${pg_purgeable:-0}) * to_kb ))
    cached=$(( ${pg_file:-0} * to_kb ))
    buffers=0
    mapped=0
    dirty=0

    # Swap from sysctl vm.swapusage (values in M).
    local swap_line=$(sysctl -n vm.swapusage)
    swap_total=$(echo "$swap_line" | awk '{print $3}' | tr -d 'M' | awk '{printf "%d", $1 * 1024}')
    swap_free=$(echo "$swap_line" | awk '{print $9}' | tr -d 'M' | awk '{printf "%d", $1 * 1024}')
}

collect_process_Darwin() {
    # Process IO is not exposed per-process without dtrace.
    process_read_kb_s=0
    process_write_kb_s=0

    # Faults from top cumulative counters (pageins ~ major faults).
    local top_line=$(top -l 1 -pid $binary_pid -stats pid,faults,pageins 2>/dev/null | tail -1)
    local faults=$(echo "$top_line" | awk '{gsub("[^0-9]","",$2); print $2}')
    local pageins=$(echo "$top_line" | awk '{gsub("[^0-9]","",$3); print $3}')
    faults=${faults:-0}
    pageins=${pageins:-0}
    if [ $first_sample -eq 1 ]; then
        min_flt_s=0
        maj_flt_s=0
    else
        maj_flt_s=$(( (pageins - prev_pageins) / interval ))
        min_flt_s=$(( (faults - prev_faults) / interval - maj_flt_s ))
        [ $min_flt_s -lt 0 ] && min_flt_s=0
        [ $maj_flt_s -lt 0 ] && maj_flt_s=0
    fi
    prev_faults=$faults
    prev_pageins=$pageins

    # Residency: physical footprint (top mem) as anon proxy, rss - footprint
    # as file-backed proxy (unified accounting blurs the split on macOS).
    local rss_kb=$(ps -o rss= -p $binary_pid 2>/dev/null | tr -d ' ')
    rss_kb=${rss_kb:-0}
    local foot_raw=$(top -l 1 -pid $binary_pid -stats mem 2>/dev/null | tail -1 | tr -d ' ')
    case "$foot_raw" in
        *G*) rss_anon=$(echo "$foot_raw" | tr -d 'G+-' | awk '{printf "%d", $1 * 1048576}') ;;
        *M*) rss_anon=$(echo "$foot_raw" | tr -d 'M+-' | awk '{printf "%d", $1 * 1024}') ;;
        *K*) rss_anon=$(echo "$foot_raw" | tr -d 'K+-' | awk '{printf "%d", $1}') ;;
        *)   rss_anon=0 ;;
    esac
    rss_file=$((rss_kb - rss_anon))
    [ $rss_file -lt 0 ] && rss_file=0
    vm_swap=0

    # Pressure: memorystatus level (1 normal, 2 warning, 4 critical) in the
    # psi_mem_some10 column; a level, not a stall percentage.
    psi_mem_some=$(sysctl -n kern.memorystatus_vm_pressure_level 2>/dev/null)
    psi_mem_some=${psi_mem_some:-0}
    psi_mem_full=0
    psi_io_some=0
    psi_io_full=0
}

collect_disk_Darwin() {
    # macOS iostat reports combined MB/s; logged as read, write logs 0.
    local latest_iostat=$(tail -1 ./tmp/iostat.tmp)
    if [ $debug -eq 1 ]; then echo "$latest_iostat"; fi
    disk_read_kb_s=$(echo "$latest_iostat" | awk '{printf "%d", $3 * 1024}')
    disk_read_kb_s=${disk_read_kb_s:-0}
    disk_write_kb_s=0
}

collect_net_Darwin() {
    local net_line=$(netstat -ib -I $nic 2>/dev/null | awk 'NR==2')
    local curr_rx=$(echo "$net_line" | awk '{print $7}')
    local curr_tx=$(echo "$net_line" | awk '{print $10}')
    curr_rx=${curr_rx:-0}
    curr_tx=${curr_tx:-0}
    if [ $first_sample -eq 1 ]; then
        net_rx_kb_s="0.00"
        net_tx_kb_s="0.00"
    else
        net_rx_kb_s=$(echo "$curr_rx $prev_net_rx $interval" | awk '{printf "%.2f", ($1 - $2) / 1000 / $3}')
        net_tx_kb_s=$(echo "$curr_tx $prev_net_tx $interval" | awk '{printf "%.2f", ($1 - $2) / 1000 / $3}')
    fi
    prev_net_rx=$curr_rx
    prev_net_tx=$curr_tx
}

collect_gpu_Darwin() {
    # powermetrics (sudo); residency as usage, frequency as sm clock.
    sudo powermetrics -n 1 -i 200 --samplers gpu_power > ./tmp/gpu.tmp 2>/dev/null
    gpu_usage=$(awk -F': ' '/GPU HW active residency/ {gsub("%","",$2); printf "%.0f", $2; exit}' ./tmp/gpu.tmp)
    gpu_sm_clock=$(awk -F': ' '/GPU HW active frequency/ {gsub(" MHz","",$2); printf "%.0f", $2; exit}' ./tmp/gpu.tmp)
    gpu_power=$(awk -F': ' '/GPU Power/ {gsub(" mW","",$2); printf "%.2f", $2 / 1000; exit}' ./tmp/gpu.tmp)
    gpu_usage=${gpu_usage:-0}
    gpu_sm_clock=${gpu_sm_clock:-0}
    gpu_power=${gpu_power:-0.00}
    gpu_mem_usage=0
    gpu_mem_used=0
    gpu_temp=0
    gpu_mem_clock=0
    gpu_pcie_rx=0
    gpu_pcie_tx=0
    gpu_proc_mem=0
}

# ---------------------------------------------------------------------------
# Main loop (shared)
# ---------------------------------------------------------------------------

while true; do
    current_time=$(date +%s)
    time_s=$((current_time - start_time))

    if ! kill -0 $binary_pid 2>/dev/null; then
        if [ $debug -eq 1 ]; then echo "Binary has exited, stopping profiling."; fi
        kill $iostat_pid 2>/dev/null
        wait $iostat_pid 2>/dev/null
        exit 0
    fi

    collect_cpu_$os
    collect_memory_$os
    collect_process_$os
    collect_disk_$os
    collect_net_$os
    if [ $gpu -eq 1 ]; then collect_gpu_$os; fi

    csv_line="$time_s,$total_usage"
    for usage in "${cpu_usages[@]}"; do
        csv_line="${csv_line},${usage}"
    done
    csv_line="${csv_line},${mem_total},${mem_free},${mem_available},${cached},${buffers},${mapped},${dirty},${swap_total},${swap_free}"
    csv_line="${csv_line},${process_read_kb_s},${process_write_kb_s},${min_flt_s},${maj_flt_s},${disk_read_kb_s},${disk_write_kb_s}"
    csv_line="${csv_line},${net_rx_kb_s},${net_tx_kb_s}"
    csv_line="${csv_line},${rss_anon},${rss_file},${vm_swap},${psi_mem_some},${psi_mem_full},${psi_io_some},${psi_io_full}"
    if [ $gpu -eq 1 ]; then
        csv_line="${csv_line},${gpu_usage},${gpu_mem_usage},${gpu_mem_used},${gpu_temp},${gpu_power}"
        csv_line="${csv_line},${gpu_sm_clock},${gpu_mem_clock},${gpu_pcie_rx},${gpu_pcie_tx},${gpu_proc_mem}"
    fi
    csv_line="${csv_line},0"

    printf '%s\n' "$csv_line" >> "$output_csv"

    if [ $debug -eq 1 ]; then
        echo "Logged data at ${time_s}s: CPU=${total_usage}%, Disk R/W=${disk_read_kb_s}/${disk_write_kb_s} kB/s"
    fi

    first_sample=0
    sleep $interval
done
