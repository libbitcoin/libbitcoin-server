import argparse
import io
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import MultipleLocator, FuncFormatter

# ---------------------------------------------------------------------------
# Libbitcoin brand palette (dark theme)
# ---------------------------------------------------------------------------
BG_FIGURE   = '#141414'
BG_AXES     = '#1e1e1e'
TEXT_COLOR  = '#d0cfc8'
GRID_COLOR  = '#2e2e2e'
SPINE_COLOR = '#3a3a3a'

LB_ORANGE   = '#e8880a'   # libbitcoin primary (matches the fist in the logo)
LB_GOLD     = '#f5c518'   # libbitcoin accent (matches the banner)
CORE_BLUE   = '#4a9abb'
CORE_LBLUE  = '#7dbddb'
LB_RUST     = '#c85637'   # logo fist shadow
LB_TAN      = '#b9a180'   # logo banner shadow
LB_GREEN    = '#7dbb7d'
LB_VIOLET   = '#bb7dbd'

# Line-color cycle applied to every panel.
SERIES_CYCLE = [LB_ORANGE, CORE_BLUE, LB_GOLD, CORE_LBLUE, LB_RUST, LB_TAN,
                LB_GREEN, LB_VIOLET]

SAVE_DPI = 100


def apply_theme():
    plt.rcParams.update({
        'text.color':         TEXT_COLOR,
        'axes.labelcolor':    TEXT_COLOR,
        'xtick.color':        TEXT_COLOR,
        'ytick.color':        TEXT_COLOR,
        'axes.edgecolor':     SPINE_COLOR,
        'axes.titlecolor':    TEXT_COLOR,
        'figure.facecolor':   BG_FIGURE,
        'axes.facecolor':     BG_AXES,
        'grid.color':         GRID_COLOR,
        'grid.linewidth':     0.6,
        'legend.facecolor':   '#242424',
        'legend.edgecolor':   SPINE_COLOR,
        'legend.labelcolor':  TEXT_COLOR,
        'font.family':        'monospace',
        'axes.prop_cycle':    plt.cycler(color=SERIES_CYCLE),
    })


def render_svg(svg_path, height_px):
    """Rasterize an SVG to PNG bytes: cairosvg (needs native cairo) with a
    pure-python svglib/reportlab fallback (Windows-friendly)."""
    try:
        import cairosvg
        return cairosvg.svg2png(url=str(svg_path), output_height=height_px)
    except Exception:
        pass
    try:
        from svglib.svglib import svg2rlg
        from reportlab.graphics import renderPM
        drawing = svg2rlg(str(svg_path))
        scale = height_px / drawing.height
        drawing.width *= scale
        drawing.height *= scale
        drawing.scale(scale, scale)
        return renderPM.drawToString(drawing, fmt='PNG', bg=0x141414)
    except Exception as e:
        print(f"Warning: SVG rasterization unavailable – logo watermark "
              f"skipped ({e}). Install cairosvg or svglib+reportlab.")
        return None


def add_logo_watermark(fig, svg_path):
    """Render the SVG logo and stamp it into the lower-right figure corner."""
    try:
        from PIL import Image
    except ImportError:
        print("Warning: Pillow not found – logo watermark skipped.")
        return

    if not Path(svg_path).exists():
        return

    fig_w_px = int(fig.get_figwidth() * SAVE_DPI)
    logo_h_px = int(fig.get_figheight() * SAVE_DPI * 0.035)

    png_bytes = render_svg(svg_path, logo_h_px)
    if png_bytes is None:
        return

    img = Image.open(io.BytesIO(png_bytes)).convert('RGBA')
    img_arr = np.array(img, dtype=np.uint8)

    # Dim the alpha channel for a subtle watermark look
    img_arr[..., 3] = (img_arr[..., 3] * 0.38).astype(np.uint8)

    logo_h, logo_w = img_arr.shape[:2]
    margin = int(fig_w_px * 0.006)
    fig.figimage(img_arr, xo=fig_w_px - logo_w - margin, yo=margin, zorder=5)


def extract_milestones(events_log, event, modulus):
    """Extract (seconds, height) milestones from a libbitcoin events log.
    Lines have the form: <event>..... <height> <seconds>."""
    import re
    pattern = rf'{re.escape(event)}\.*\s*(\d+)\s+(\d+)'
    results = {}
    with open(events_log, 'r') as file:
        for line in file:
            match = re.match(pattern, line.strip())
            if match:
                height = int(match.group(1))
                seconds = int(match.group(2))
                if height % modulus == 0:
                    results[height] = seconds
    return sorted((seconds, height) for height, seconds in results.items())


def apply_milestones(df, milestones):
    """Fill df['milestone'] stepwise: each milestone lands on the first
    sample at or after its event time (later milestones win a shared
    sample)."""
    df['milestone'] = ''
    index = 0
    for row in df.index:
        time_s = df.at[row, 'time_s']
        value = ''
        while index < len(milestones) and time_s >= milestones[index][0]:
            value = str(milestones[index][1])
            index += 1
        if value:
            df.at[row, 'milestone'] = value


parser = argparse.ArgumentParser(description="Plot a profile.csv produced by profile-posix.sh / profile-windows.ps1")
parser.add_argument('-i', '--input', default='profile.csv',
                    help='Input CSV file (default: profile.csv)')
parser.add_argument('-o', '--output', default='profile',
                    help='Output basename; writes <name>.svg and <name>.png (default: profile)')
parser.add_argument('-e', '--events', default=None,
                    help='Events log to derive height milestones from (optional)')
parser.add_argument('--event', default='block_organized',
                    help='Event name to match in the events log (default: block_organized)')
parser.add_argument('--modulus', type=int, default=50000,
                    help='Mark heights that are multiples of this (default: 50000)')
args = parser.parse_args()

apply_theme()

# Load the CSV file
df = pd.read_csv(args.input)

# Derive milestones from the events log when given; otherwise any values
# already present in the milestone column are used as-is.
if args.events:
    apply_milestones(df, extract_milestones(args.events, args.event, args.modulus))

# Identify CPU core columns dynamically
cpu_cols = [col for col in df.columns if col.startswith('cpu') and col != 'cpu_usage_total' and '_' in col]
num_cores = len(cpu_cols)

# Optional column groups: panels appear only when their columns are present,
# so CSVs from older sampler versions (or platforms that omit a metric) plot
# unchanged.
gpu_cols = ['gpu_usage_pct', 'gpu_mem_usage_pct', 'gpu_mem_used_MB', 'gpu_temp_C', 'gpu_power_draw_W']
has_gpu = all(col in df.columns for col in gpu_cols)

gpu_ext_cols = ['gpu_sm_clock_MHz', 'gpu_mem_clock_MHz', 'gpu_pcie_rx_MB_s', 'gpu_pcie_tx_MB_s', 'gpu_proc_mem_MB']
has_gpu_ext = all(col in df.columns for col in gpu_ext_cols)

res_cols = ['rss_anon_kB', 'rss_file_kB', 'vm_swap_kB']
has_residency = all(col in df.columns for col in res_cols)

psi_cols = ['psi_mem_some10', 'psi_mem_full10', 'psi_io_some10', 'psi_io_full10']
has_psi = all(col in df.columns for col in psi_cols)

def add_milestones(ax, df):
    """
    Fügt bei vorhandenen Milestones eine vertikale Linie + Text hinzu.
    Der Text zeigt jetzt z. B. (900k – 67 min)
    """

    def format_milestone(value: int) -> str:
        """Wandelt z. B. 550000 → '550k', 1234567 → '1.23M' usw."""
        if value >= 1_000_000:
            return f"{value / 1_000_000:.2f}".rstrip('0').rstrip('.') + "M"
        elif value >= 10_000:           # ab 10k ganzzahlig
            return f"{value // 1000}k"
        elif value >= 1_000:
            return f"{value / 1000:.1f}".rstrip('0').rstrip('.') + "k"
        else:
            return f"{value:,}"

    # Milestone-Spalte bereinigen
    milestones = df['milestone'].astype(str).str.strip()

    valid = (
        milestones.notna() &
        (milestones != '') &
        (milestones != 'nan') &
        (milestones != '<NA>')
    )

    for time, milestone_str in zip(df[valid]['time_s'], milestones[valid]):
        try:
            t_sec = float(time)               # Zeit in Sekunden
            t_min = int(t_sec // 60)          # Ganzzahlige Minuten
            ms_value = int(float(milestone_str))
        except (ValueError, TypeError):
            continue

        # Skip the all-zero placeholder emitted by the samplers.
        if ms_value == 0:
            continue

        # Vertikale Linie
        ax.axvline(x=t_sec, color=LB_GOLD, linestyle='--', linewidth=1.2, alpha=0.45)

        # Text mit Milestone + Minuten
        label_text = f"({format_milestone(ms_value)} – {t_min} min)"

        ax.text(
            t_sec,                              # x-Position direkt an der Linie
            ax.get_ylim()[1] * 0.94,            # etwas unterhalb des oberen Randes
            label_text,
            rotation=90,
            va='top',
            ha='right',
            color=TEXT_COLOR,
            fontsize=11,
            fontweight='bold',
            bbox=dict(boxstyle="round,pad=0.35", facecolor="#1a1a1a", alpha=0.85,
                      edgecolor=SPINE_COLOR, linewidth=1)
        )

# Function to format x-axis in minutes
def format_minutes(x, pos):
    return f'{int(x // 60)} min'

# Fixed panels plus one per optional group present.
num_rows = 6 + int(has_residency) + int(has_psi) + int(has_gpu) + int(has_gpu_ext)
fig, axs = plt.subplots(num_rows, 1, figsize=(36, 4 * num_rows), sharex=True)
row = 0

# 1. Memory usage over time
# Calculate used memory: total - free - buffers - cached (common Linux metric)
ax = axs[row]; row += 1
df['mem_used_kB'] = df['mem_total_kB'] - df['mem_free_kB'] - df['buffers_kB'] - df['cached_kB']
ax.plot(df['time_s'], df['mem_used_kB'] / 1024, label='Used Memory (MB)')
ax.plot(df['time_s'], df['mem_available_kB'] / 1024, label='Available Memory (MB)')
ax.plot(df['time_s'], df['swap_total_kB'] / 1024, label='Swap Total (MB)')
ax.plot(df['time_s'], df['swap_free_kB'] / 1024, label='Swap Free (MB)')
ax.plot(df['time_s'], df['buffers_kB'] / 1024, label='Buffers (MB)')
ax.plot(df['time_s'], df['mapped_kB'] / 1024, label='Mapped (MB)')
ax.plot(df['time_s'], df['cached_kB'] / 1024, label='Cached (MB)')
ax.set_title('Memory Usage')
ax.set_ylabel('Memory (MB)')
ax.legend()
add_milestones(ax, df)
ax.grid(True)

# 2. CPU core usage over time
ax = axs[row]; row += 1
for i in range(num_cores):
    ax.plot(df['time_s'], df[f'cpu{i}_usage'], label=f'CPU{i}')
ax.plot(df['time_s'], df['cpu_usage_total'], label='Total CPU', linestyle='--', color=TEXT_COLOR)
ax.set_title('CPU Usage')
ax.set_ylabel('Usage (%)')
ax.legend(ncol=4)
add_milestones(ax, df)
ax.grid(True)

# 3. Network traffic over time
ax = axs[row]; row += 1
ax.plot(df['time_s'], df['net_rx_kB_s'] / 1024, label='RX (kB/s)')
ax.plot(df['time_s'], df['net_tx_kB_s'] / 1024, label='TX (kB/s)')
ax.set_title('Network Traffic')
ax.set_ylabel('Traffic (MB/s)')
ax.legend()
add_milestones(ax, df)
ax.grid(True)

# 4. Process IO transfer
ax = axs[row]; row += 1
ax.plot(df['time_s'], df['process_io_write_kB_s'] / 1024, label='Write (pidstat)')
ax.plot(df['time_s'], df['disk_write_kB_s'] / 1024, label='Write (iostat)')
ax.plot(df['time_s'], df['process_io_read_kB_s'] / 1024, label='Read (pidstat)')
ax.plot(df['time_s'], df['disk_read_kB_s'] / 1024, label='Read (iostat)')
ax.set_title('Disk I/O')
ax.set_ylabel('Transfer (MB/s)')
ax.legend()
add_milestones(ax, df)
ax.grid(True)

# 5. Dirty pages
ax = axs[row]; row += 1
ax.plot(df['time_s'], df['dirty_kB'] / 1024, label='Dirty Pages')
ax.set_title('Dirty Pages')
ax.set_ylabel('Dirty Pages (MB)')
ax.legend()
add_milestones(ax, df)
ax.grid(True)

# 6. page faults
ax = axs[row]; row += 1
ax.plot(df['time_s'], df['min_flt_s'], label='Minor Faults/s')
ax.plot(df['time_s'], df['maj_flt_s'], label='Major Faults/s')
ax.set_title('Page Faults')
ax.set_ylabel('Count/s')
ax.legend()
add_milestones(ax, df)
ax.grid(True)

# 7. Process residency split (anon = staging/heads, file = mapped store).
# Eviction of the mapped read set appears as rss_file collapsing while the
# system cache (memory panel) stays high.
if has_residency:
    ax = axs[row]; row += 1
    ax.plot(df['time_s'], df['rss_anon_kB'] / 1048576, label='RSS Anon (GB)')
    ax.plot(df['time_s'], df['rss_file_kB'] / 1048576, label='RSS File (GB)')
    ax.plot(df['time_s'], df['vm_swap_kB'] / 1048576, label='Swapped (GB)')
    ax.set_title('Process Residency')
    ax.set_ylabel('Resident (GB)')
    ax.legend()
    add_milestones(ax, df)
    ax.grid(True)

# 8. Pressure stalls (Linux PSI avg10; on macOS psi_mem_some10 carries the
# memorystatus level 1/2/4 and the rest are zero; all zero on Windows).
if has_psi:
    ax = axs[row]; row += 1
    ax.plot(df['time_s'], df['psi_mem_some10'], label='Mem some (%)')
    ax.plot(df['time_s'], df['psi_mem_full10'], label='Mem full (%)')
    ax.plot(df['time_s'], df['psi_io_some10'], label='IO some (%)')
    ax.plot(df['time_s'], df['psi_io_full10'], label='IO full (%)')
    ax.set_title('Pressure Stalls (PSI avg10)')
    ax.set_ylabel('Stalled time (%)')
    ax.legend()
    add_milestones(ax, df)
    ax.grid(True)

# 9. GPU usage (only if the sampler was run with -g and GPU columns are present)
if has_gpu:
    ax_gpu = axs[row]; row += 1
    # Left axis: percentage-based metrics
    l1, = ax_gpu.plot(df['time_s'], df['gpu_usage_pct'], label='GPU Usage (%)', color=LB_ORANGE)
    l2, = ax_gpu.plot(df['time_s'], df['gpu_mem_usage_pct'], label='GPU Mem Usage (%)', color=LB_GOLD)
    ax_gpu.set_ylabel('Usage (%)')
    ax_gpu.set_ylim(bottom=0)

    # Right axis: physical quantities (different units, share one scale for compactness)
    ax_gpu2 = ax_gpu.twinx()
    l3, = ax_gpu2.plot(df['time_s'], df['gpu_temp_C'], label='GPU Temp (°C)', color=LB_RUST)
    l4, = ax_gpu2.plot(df['time_s'], df['gpu_power_draw_W'], label='GPU Power (W)', color=CORE_BLUE)
    l5, = ax_gpu2.plot(df['time_s'], df['gpu_mem_used_MB'] / 1024, label='GPU Mem Used (GB)', color=LB_VIOLET)
    ax_gpu2.set_ylabel('Temp (°C) / Power (W) / Mem (GB)')
    ax_gpu2.set_ylim(bottom=0)

    ax_gpu.set_title('GPU Usage')
    lines = [l1, l2, l3, l4, l5]
    ax_gpu.legend(lines, [line.get_label() for line in lines], ncol=5)
    add_milestones(ax_gpu, df)
    ax_gpu.grid(True)

# 10. GPU clocks and transfer (batched verification: high utilization with
# high PCIe traffic and drooping SM clock means the device is fed, not
# compute-bound; proc mem shows batch buffer sizing).
if has_gpu_ext:
    ax_ext = axs[row]; row += 1
    l1, = ax_ext.plot(df['time_s'], df['gpu_sm_clock_MHz'], label='SM Clock (MHz)', color=LB_ORANGE)
    l2, = ax_ext.plot(df['time_s'], df['gpu_mem_clock_MHz'], label='Mem Clock (MHz)', color=LB_GOLD)
    ax_ext.set_ylabel('Clock (MHz)')
    ax_ext.set_ylim(bottom=0)

    ax_ext2 = ax_ext.twinx()
    l3, = ax_ext2.plot(df['time_s'], df['gpu_pcie_rx_MB_s'], label='PCIe RX (MB/s)', color=CORE_BLUE)
    l4, = ax_ext2.plot(df['time_s'], df['gpu_pcie_tx_MB_s'], label='PCIe TX (MB/s)', color=CORE_LBLUE)
    l5, = ax_ext2.plot(df['time_s'], df['gpu_proc_mem_MB'], label='Proc Mem (MB)', color=LB_VIOLET)
    ax_ext2.set_ylabel('PCIe (MB/s) / Proc Mem (MB)')
    ax_ext2.set_ylim(bottom=0)

    ax_ext.set_title('GPU Clocks and Transfer')
    lines = [l1, l2, l3, l4, l5]
    ax_ext.legend(lines, [line.get_label() for line in lines], ncol=5)
    add_milestones(ax_ext, df)
    ax_ext.grid(True)

# Set x-axis for all
for ax in axs:
    ax.xaxis.set_major_locator(MultipleLocator(600))  # Every 10 minutes (600 seconds)
    ax.xaxis.set_major_formatter(FuncFormatter(format_minutes))
    ax.set_xlabel('Time (minutes)')

plt.tight_layout()

for ax in axs:
    ax.set_xlim(left=0)

add_logo_watermark(fig, Path(__file__).parent / 'libbitcoin-logo.svg')

plt.savefig(f'{args.output}.svg', format='svg', facecolor=BG_FIGURE)
plt.savefig(f'{args.output}.png', format='png', dpi=SAVE_DPI, facecolor=BG_FIGURE)
plt.close()
