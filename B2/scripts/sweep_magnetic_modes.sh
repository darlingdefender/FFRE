#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build"
macro_dir="${build_dir}/macros/generated"
log_dir="${build_dir}/logs"
binary="${build_dir}/exampleB1"
macro="${macro_dir}/sweep_magnetic_modes.mac"
threads="${FFRE_SWEEP_THREADS:-4}"

if [[ ! -x "${binary}" ]]; then
  echo "Missing executable: ${binary}" >&2
  exit 1
fi

mkdir -p "${macro_dir}" "${log_dir}"

cat > "${macro}" <<EOF
/control/verbose 0
/run/verbose 0
/event/verbose 0
/tracking/verbose 0
/run/numberOfThreads ${threads}
/random/setSeeds 12345 67890

/process/had/particle_hp/skip_missing_isotopes true
/process/had/particle_hp/use_Wendt_fission_model false
/process/had/particle_hp/produce_fission_fragment true

/run/initialize

/gps/particle neutron
/gps/pos/type Point
/gps/pos/centre 0 0 0 m
/gps/ang/type iso
/gps/ene/type Mono
/gps/ene/mono 0.0253 eV

/run/printProgress 5000
EOF

for beam_on in $(seq 5000 5000 50000); do
  echo "/run/beamOn ${beam_on}" >> "${macro}"
done

run_mode() {
  local mode="$1"
  local log_file="${log_dir}/sweep_${mode}.log"
  echo "Running mode=${mode} with macro ${macro}" >&2
  FFRE_FIELD_MODE="${mode}" \
  FFRE_PRINT_EVENT_SUMMARY=0 \
  "${binary}" "${macro}" > "${log_file}"
  echo "${log_file}"
}

off_log="$(run_mode off)"
uniform_log="$(run_mode uniform)"
collimation_log="$(run_mode collimation)"

csv_file="${build_dir}/magnetic_mode_sweep.csv"
md_file="${build_dir}/magnetic_mode_sweep.md"

python3 - <<'PY' "${off_log}" "${uniform_log}" "${collimation_log}" "${csv_file}" "${md_file}"
import pathlib
import re
import sys

log_paths = {
    "off": pathlib.Path(sys.argv[1]),
    "uniform": pathlib.Path(sys.argv[2]),
    "collimation": pathlib.Path(sys.argv[3]),
}
csv_path = pathlib.Path(sys.argv[4])
md_path = pathlib.Path(sys.argv[5])

patterns = {
    "fission_events": re.compile(r"Fission events\s+:\s+(\d+) / (\d+)"),
    "shape3_events": re.compile(r"Shape3 hit events\s+:\s+(\d+) / (\d+)"),
    "shape4_events": re.compile(r"Shape4 hit events\s+:\s+(\d+) / (\d+)"),
    "shape34_events": re.compile(r"Shape3 or Shape4 hit events\s+:\s+(\d+) / (\d+)"),
    "shape3_rate": re.compile(r"Shape3 hit rate among fission evts : ([0-9.]+)%"),
    "shape4_rate": re.compile(r"Shape4 hit rate among fission evts : ([0-9.]+)%"),
}


def parse_log(path: pathlib.Path):
    text = path.read_text()
    blocks = re.findall(
        r"--------------------End of Global Run-----------------------(.*?)"
        r"============================================================",
        text,
        flags=re.S,
    )
    rows = {}
    for block in blocks:
        beam_match = re.search(r"The run consists of (\d+) GPS source", block)
        if not beam_match:
            continue
        beam_on = int(beam_match.group(1))
        row = {"beam_on": beam_on}
        for key, pattern in patterns.items():
            match = pattern.search(block)
            if not match:
                raise RuntimeError(f"Missing {key} in {path} for beamOn={beam_on}")
            row[key] = float(match.group(1)) if "rate" in key else int(match.group(1))
        rows[beam_on] = row
    return rows


parsed = {mode: parse_log(path) for mode, path in log_paths.items()}
beam_ons = sorted(parsed["off"])

headers = [
    "beam_on",
    "off_shape34",
    "uniform_shape34",
    "collimation_shape34",
    "uniform_vs_off_pct",
    "collimation_vs_off_pct",
    "off_shape3",
    "off_shape4",
    "uniform_shape3",
    "uniform_shape4",
    "collimation_shape3",
    "collimation_shape4",
]

csv_lines = [",".join(headers)]
md_lines = [
    "| beamOn | off S3/S4/Total | uniform S3/S4/Total | collimation S3/S4/Total | uniform vs off | collimation vs off |",
    "| ---: | ---: | ---: | ---: | ---: | ---: |",
]

for beam_on in beam_ons:
    off = parsed["off"][beam_on]
    uniform = parsed["uniform"][beam_on]
    collimation = parsed["collimation"][beam_on]
    off_total = off["shape34_events"]
    uniform_total = uniform["shape34_events"]
    collimation_total = collimation["shape34_events"]
    uniform_gain = 100.0 * (uniform_total - off_total) / off_total if off_total else 0.0
    collimation_gain = 100.0 * (collimation_total - off_total) / off_total if off_total else 0.0

    csv_lines.append(",".join([
        str(beam_on),
        str(off_total),
        str(uniform_total),
        str(collimation_total),
        f"{uniform_gain:.3f}",
        f"{collimation_gain:.3f}",
        str(off["shape3_events"]),
        str(off["shape4_events"]),
        str(uniform["shape3_events"]),
        str(uniform["shape4_events"]),
        str(collimation["shape3_events"]),
        str(collimation["shape4_events"]),
    ]))

    md_lines.append(
        f"| {beam_on} | "
        f"{off['shape3_events']}/{off['shape4_events']}/{off_total} | "
        f"{uniform['shape3_events']}/{uniform['shape4_events']}/{uniform_total} | "
        f"{collimation['shape3_events']}/{collimation['shape4_events']}/{collimation_total} | "
        f"{uniform_gain:+.1f}% | {collimation_gain:+.1f}% |"
    )

csv_path.write_text("\n".join(csv_lines) + "\n")
md_path.write_text("\n".join(md_lines) + "\n")

print("\n".join(md_lines))
print()
print(f"CSV: {csv_path}")
print(f"MD:  {md_path}")
PY
