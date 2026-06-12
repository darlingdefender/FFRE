#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build"
macro_template="${repo_root}/compare_magnetic_modes.mac"
macro_dir="${build_dir}/macros/generated"
log_dir="${build_dir}/logs"
binary="${build_dir}/exampleB1"
beam_on="${1:-50000}"
macro="${macro_dir}/escape_speed_distribution_${beam_on}.mac"
csv_file="${build_dir}/escape_speed_distribution_table.csv"
md_file="${build_dir}/escape_speed_distribution_table.md"
tex_file="${build_dir}/escape_speed_distribution_table.tex"

if [[ ! -x "${binary}" ]]; then
  echo "Missing executable: ${binary}" >&2
  exit 1
fi

if [[ ! -f "${macro_template}" ]]; then
  echo "Missing macro template: ${macro_template}" >&2
  exit 1
fi

mkdir -p "${macro_dir}" "${log_dir}"

python3 - <<'PY' "${macro_template}" "${macro}" "${beam_on}"
import pathlib
import re
import sys

template = pathlib.Path(sys.argv[1]).read_text()
beam_on = int(sys.argv[3])
updated = re.sub(r"^/run/beamOn\s+\d+\s*$",
                 f"/run/beamOn {beam_on}",
                 template,
                 flags=re.M)
pathlib.Path(sys.argv[2]).write_text(updated)
PY

run_mode() {
  local mode="$1"
  local log_file="${log_dir}/escape_speed_${mode}_${beam_on}.log"
  echo "Running mode=${mode} beamOn=${beam_on}" >&2
  FFRE_FIELD_MODE="${mode}" \
  FFRE_PRINT_EVENT_SUMMARY=0 \
  "${binary}" "${macro}" > "${log_file}"
  echo "${log_file}"
}

off_log="$(run_mode off)"
uniform_log="$(run_mode uniform)"
collimation_log="$(run_mode collimation)"

python3 - <<'PY' "${off_log}" "${uniform_log}" "${collimation_log}" "${beam_on}" "${csv_file}" "${md_file}" "${tex_file}"
import csv
import pathlib
import re
import sys

mode_names = ["off", "uniform", "collimation"]
log_paths = {mode: pathlib.Path(path) for mode, path in zip(mode_names, sys.argv[1:4])}
beam_on = int(sys.argv[4])
csv_path = pathlib.Path(sys.argv[5])
md_path = pathlib.Path(sys.argv[6])
tex_path = pathlib.Path(sys.argv[7])

summary_pattern = re.compile(
    r"Outward fuel-coating escapes\s+:\s+(\d+).*?"
    r"Outward coating escape speed\s+:\s+([0-9.]+) km/s "
    r"\(beta = ([0-9.]+), RMS = ([0-9.]+) km/s\).*?"
    r"Escape speed distribution \(km/s\)\s+:\n"
    r"((?:\s+.+\n)+?)"
    r"\s*Shape3 hit events",
    flags=re.S,
)
bin_pattern = re.compile(r"^\s+(.+?)\s+:\s+(\d+)\s+\(([0-9.]+)%\)$")

parsed = {}
all_bins = []
for mode, path in log_paths.items():
    text = path.read_text()
    match = summary_pattern.search(text)
    if not match:
        raise RuntimeError(f"Missing escape-speed summary in {path}")

    escapes = int(match.group(1))
    mean_speed = float(match.group(2))
    beta = float(match.group(3))
    rms_speed = float(match.group(4))
    bins = []
    for line in match.group(5).splitlines():
        line = line.rstrip()
        if not line.strip():
            continue
        bin_match = bin_pattern.match(line)
        if not bin_match:
            continue
        label = bin_match.group(1).strip()
        count = int(bin_match.group(2))
        pct = float(bin_match.group(3))
        bins.append((label, count, pct))
    parsed[mode] = {
        "escapes": escapes,
        "mean_speed": mean_speed,
        "beta": beta,
        "rms_speed": rms_speed,
        "bins": bins,
    }
    if not all_bins:
        all_bins = [label for label, *_ in bins]

headers = [
    "speed_bin_km_per_s",
    "off_count", "off_pct",
    "uniform_count", "uniform_pct",
    "collimation_count", "collimation_pct",
]

with csv_path.open("w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(headers)
    for idx, label in enumerate(all_bins):
        row = [label]
        for mode in mode_names:
            _, count, pct = parsed[mode]["bins"][idx]
            row.extend([count, f"{pct:.3f}"])
        writer.writerow(row)

summary_lines = [
    "# Escape Fragment Speed Distribution",
    "",
    f"beamOn = {beam_on}. Statistic = first outward crossing from `FuelShell -> Shape1`.",
    "",
    "| Mode | Escapes | Mean speed (km/s) | beta | RMS (km/s) |",
    "| --- | ---: | ---: | ---: | ---: |",
]
for mode in mode_names:
    item = parsed[mode]
    summary_lines.append(
        f"| {mode} | {item['escapes']} | {item['mean_speed']:.3f} | "
        f"{item['beta']:.6f} | {item['rms_speed']:.3f} |"
    )

summary_lines.extend([
    "",
    "| Speed bin (km/s) | off n (%) | uniform n (%) | collimation n (%) |",
    "| --- | ---: | ---: | ---: |",
])
for idx, label in enumerate(all_bins):
    row = [label]
    for mode in mode_names:
        _, count, pct = parsed[mode]["bins"][idx]
        row.append(f"{count} ({pct:.2f})")
    summary_lines.append(f"| {row[0]} | {row[1]} | {row[2]} | {row[3]} |")
md_path.write_text("\n".join(summary_lines) + "\n")

tex_lines = [
    "% Requires \\usepackage{booktabs}",
    "\\begin{table}[t]",
    "\\centering",
    "\\small",
    "\\setlength{\\tabcolsep}{4.5pt}",
    "\\caption{Speed distribution of outward escaping fission fragments under three magnetic-field modes. Statistics are taken at the first FuelShell-to-Shape1 crossing.}",
    "\\label{tab:escape-speed-distribution}",
    "\\begin{tabular}{lccc}",
    "\\toprule",
    "Velocity bin & Off & Uniform & Collimation \\\\",
    "$(\\mathrm{km\\,s^{-1}})$ & $n$ (\\%) & $n$ (\\%) & $n$ (\\%) \\\\",
    "\\midrule",
]
for idx, label in enumerate(all_bins):
    row = [label]
    for mode in mode_names:
        _, count, pct = parsed[mode]["bins"][idx]
        row.append(f"{count} ({pct:.2f})")
    tex_lines.append(f"{row[0]} & {row[1]} & {row[2]} & {row[3]} \\\\")

tex_lines.extend([
    "\\addlinespace[2pt]",
    "\\multicolumn{4}{l}{\\textit{Summary statistics}} \\\\",
    "Escapes & "
    f"{parsed['off']['escapes']} & {parsed['uniform']['escapes']} & {parsed['collimation']['escapes']} \\\\",
    "Mean speed (km/s) & "
    f"{parsed['off']['mean_speed']:.1f} & {parsed['uniform']['mean_speed']:.1f} & {parsed['collimation']['mean_speed']:.1f} \\\\",
    "$\\beta$ & "
    f"{parsed['off']['beta']:.4f} & {parsed['uniform']['beta']:.4f} & {parsed['collimation']['beta']:.4f} \\\\",
    "$\\mathrm{RMS}$ (km/s) & "
    f"{parsed['off']['rms_speed']:.1f} & {parsed['uniform']['rms_speed']:.1f} & {parsed['collimation']['rms_speed']:.1f} \\\\",
    "\\bottomrule",
    "\\end{tabular}",
    "\\end{table}",
])
tex_path.write_text("\n".join(tex_lines) + "\n")

print("\n".join(summary_lines))
print()
print(f"CSV: {csv_path}")
print(f"MD:  {md_path}")
print(f"TEX: {tex_path}")
PY
