#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build"
macro_dir="${build_dir}/macros/generated"
binary="${build_dir}/exampleB1"
base_data="${FFRE_AM242_BASE_DATA:-${repo_root}/data/G4NDL4.6_patched}"
proxy_data="${build_dir}/am242_ffproxy_data"
surrogate_ff="${FFRE_SURROGATE_FF_DATA:-}"
if [[ -z "${surrogate_ff}" && -n "${G4NEUTRONHPDATA:-}" ]]; then
  surrogate_ff="${G4NEUTRONHPDATA}/Fission/FF/92_238_Uranium.z"
fi
user_macro="${1:-}"
wrapper_macro="${macro_dir}/am242_ffproxy_wrapper.mac"

if [[ ! -x "${binary}" ]]; then
  echo "Missing executable: ${binary}" >&2
  exit 1
fi

if [[ -z "${user_macro}" ]]; then
  echo "Usage: $0 path/to/user.mac" >&2
  echo "Set FFRE_SURROGATE_FF_DATA or G4NEUTRONHPDATA for surrogate U-238 FF data." >&2
  exit 1
fi

if [[ ! -f "${user_macro}" ]]; then
  echo "Missing macro: ${user_macro}" >&2
  exit 1
fi

if [[ ! -d "${base_data}" ]]; then
  echo "Missing base HP data: ${base_data}" >&2
  exit 1
fi

if [[ -z "${surrogate_ff}" || ! -f "${surrogate_ff}" ]]; then
  echo "Missing surrogate FF data. Set FFRE_SURROGATE_FF_DATA to a Fission/FF data file, or set G4NEUTRONHPDATA." >&2
  exit 1
fi

mkdir -p "${macro_dir}"

rm -rf "${proxy_data}"
mkdir -p "${proxy_data}"

for entry in "${base_data}"/*; do
  name="$(basename "${entry}")"
  if [[ "${name}" == "Fission" ]]; then
    cp -a "${entry}" "${proxy_data}/"
  else
    ln -s "$(realpath "${entry}")" "${proxy_data}/${name}"
  fi
done

python3 - <<'PY' "${surrogate_ff}" "${proxy_data}/Fission/FF/95_242_Americium.z" "${proxy_data}/Fission/FF/95_242m1_Americium.z"
import pathlib
import sys
import zlib

source = pathlib.Path(sys.argv[1])
targets = [pathlib.Path(arg) for arg in sys.argv[2:]]

text = zlib.decompress(source.read_bytes()).decode("latin1")
lines = text.splitlines()
if lines[:2] == ["G4NDL", "ENDF/B-VII.1"]:
    text = "\n".join(lines[2:]) + "\n"

payload = zlib.compress(text.encode("latin1"))
for target in targets:
    if target.exists() or target.is_symlink():
        target.unlink()
    target.write_bytes(payload)
PY

cat > "${wrapper_macro}" <<EOF
/process/had/particle_hp/use_Wendt_fission_model false
/process/had/particle_hp/produce_fission_fragment true
/control/execute ${user_macro}
EOF

cat <<EOF
Using proxy HP data: ${proxy_data}
Using macro: ${user_macro}

This workaround injects surrogate Fission/FF data from U-238 for Am-242 and
Am-242m1. It is useful for code-path validation only, not for physics-quality
fragment yields.
EOF

G4NEUTRONHPDATA="${proxy_data}" "${binary}" "${wrapper_macro}"
