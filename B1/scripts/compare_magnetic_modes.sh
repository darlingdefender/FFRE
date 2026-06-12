#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build"
log_dir="${build_dir}/logs"
binary="${build_dir}/exampleB1"
macro="${1:-${repo_root}/compare_magnetic_modes.mac}"
uniform_log="${log_dir}/compare_uniform.log"
collimation_log="${log_dir}/compare_collimation.log"

if [[ ! -x "${binary}" ]]; then
  echo "Missing executable: ${binary}" >&2
  exit 1
fi

if [[ ! -f "${macro}" ]]; then
  echo "Missing macro: ${macro}" >&2
  exit 1
fi

mkdir -p "${log_dir}"

extract_summary() {
  local log_file="$1"
  rg "mode =|Fission events|Outward fuel-coating escapes|Shape3 hit events|Shape4 hit events|Shape3 or Shape4 hit events|Shape3 hit rate among fission evts|Shape4 hit rate among fission evts" "${log_file}"
}

echo "Running uniform field comparison with macro: ${macro}"
FFRE_FIELD_MODE=uniform "${binary}" "${macro}" > "${uniform_log}"

echo "Running collimation field comparison with macro: ${macro}"
FFRE_FIELD_MODE=collimation "${binary}" "${macro}" > "${collimation_log}"

echo
echo "=== Uniform Field ==="
extract_summary "${uniform_log}"

echo
echo "=== Collimation Field ==="
extract_summary "${collimation_log}"

echo
echo "Logs:"
echo "  ${uniform_log}"
echo "  ${collimation_log}"
