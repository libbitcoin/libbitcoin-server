# Diagnostics

Cross-platform node profiling and plotting. The two samplers emit an
identical CSV schema, so the plotter is platform-agnostic. Columns without a
platform analogue log 0 (each sampler documents its divergences in its
header comment).

## Pipeline

1. Profile a run (both device parameters are mandatory and enumerated in the
   usage text):

        ./profile-posix.sh -b ./bn -a '-c ./bn.cfg' -i 5 -D nvme0n1 -n eth0
        ./profile-posix.sh -b ./bs -a '--config ./bs.cfg' -i 5 -D disk0 -n en0
        .\profile-windows.ps1 -Binary .\bn.exe -Args '-c .\bn.cfg' -Disk '0 C:' -Nic 'Ethernet'

   profile-posix.sh dispatches on uname (Linux and macOS). Add `-g` (`-Gpu`)
   for GPU tracking (nvidia-smi; powermetrics on macOS).

2. Plot (writes `<name>.svg` and `<name>.png`, libbitcoin dark theme). Pass
   the node's events log to mark height milestones:

        python3 plot-profile.py -i profile.csv -e events.log -o profile

   Milestone granularity and event are tunable (`--modulus`, `--event`).

## Requirements

- Samplers: standard platform tools only (Linux additionally needs `sysstat`
  for pidstat/iostat).
- Plotting: `pandas`, `matplotlib`, and for the logo watermark either
  `cairosvg` (needs native cairo, typical on Linux/macOS) or
  `svglib` + `reportlab` + `rlPyCairo` (pure wheels, works on Windows).
  The watermark is skipped gracefully when neither renderer is available.

## Notes

- Panels for residency, pressure (PSI), and extended GPU metrics appear only
  when their columns are present, so older CSVs still plot.
- The samplers reserve the `milestone` column (always 0); plot-profile.py
  fills it from the events log (`-e`) and ignores zero values. A CSV whose
  milestone column was populated by other means plots as-is without `-e`.
