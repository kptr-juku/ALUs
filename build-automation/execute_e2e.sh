#!/usr/bin/env bash

set -euo pipefail

function print_help {
  echo "Usage:"
  echo "$0 <build ID - leave empty \"\" arg for local checks> <resources file> [--no-golden]"
}

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "Wrong count of input arguments"
  print_help
  exit 1
fi

if [[ $# -eq 3 ]]; then
  if [[ $3 != "--no-golden" ]]; then
    echo "Unknown option: $3"
    print_help
    exit 1
  fi
  unset NIGHTLY_GOLDEN_DIR
fi

build_id=$1
nightly_resources=$2
test_datasets_dir=$(grep test_data_dir "$nightly_resources" | awk -F'[=]' '{print $2}')
srtm3_files_dir=$(grep srtm3_dir "$nightly_resources" | awk -F'[=]' '{print $2}')
copdem30_files_dir=$(grep copdem30_dir "$nightly_resources" | awk -F'[=]' '{print $2}')
orbit_files_dir=$(grep orbit_dir "$nightly_resources" | awk -F'[=]' '{print $2}')
products_output=$(grep results_dir "$nightly_resources" | awk -F'[=]' '{print $2}')

if [[ -z "${build_id}" ]]; then
  echo "Performing locally"
else
  tar -xzvf "${build_id}.tar.gz"
  # Alus binary location included in path.
  export PATH="$PATH:$PWD"
fi

mkdir -p "$products_output"
rm -rf "$products_output"/*

if [[ -z "${NIGHTLY_GOLDEN_DIR:-}" ]]; then
  echo "NIGHTLY_GOLDEN_DIR is not defined; all processing cases will run without golden verification"
else
  echo "Golden verification enabled from $NIGHTLY_GOLDEN_DIR"
fi

set +e

failure_count=0
failed_cases=()

function run_case {
  local name=$1
  shift

  echo
  echo "*****$name*****"
  "$@"
  local result=$?
  if ((result != 0)); then
    ((failure_count++))
    failed_cases+=("$name (exit $result)")
  fi
}

function run_jupyter_case {
  python3 -m venv .env || return $?
  source .env/bin/activate || return $?
  ./run_jupyter_tests.sh "$(pwd)/jupyter-notebook" "$test_datasets_dir" "$(pwd)" "$orbit_files_dir" \
    "$products_output"
  local result=$?
  deactivate
  return "$result"
}

run_case "Beirut disaster coherence scenes" \
  ./run_beirut_disaster_test.sh "$test_datasets_dir" "$srtm3_files_dir" "$orbit_files_dir" "$products_output"
run_case "BEL and GER flood coherence scenes" \
  ./run_flood_bel_ger_test.sh "$test_datasets_dir" "$srtm3_files_dir" "$copdem30_files_dir" "$orbit_files_dir" \
  "$products_output"
run_case "Virumaa calibration scene" \
  ./run_virumaa_calibration_chain_test.sh "$test_datasets_dir" "$srtm3_files_dir" "$products_output"
run_case "Maharashtra flood calibration scene" \
  ./run_maharashtra_calibration_test.sh "$test_datasets_dir" "$srtm3_files_dir" "$products_output"
run_case "Resampling tests" ./run_resample_test.sh "$test_datasets_dir" "$products_output"
run_case "Gabor feature extraction tests" \
  ./run_gabor_feature_extraction_test.sh "$test_datasets_dir" "$products_output"
run_case "Perito Moreno glacier SLC scene tests" \
  ./run_perito_moreno_slc_scenes.sh "$test_datasets_dir" "$copdem30_files_dir" "$products_output"
run_case "Maharashtra coherence SLC scenes with COPDEM 30m COG" \
  ./run_maharashtra_coherence_test.sh "$test_datasets_dir" "$copdem30_files_dir" "$orbit_files_dir" \
  "$products_output"
run_case "UKR GRD calibration with COPDEM 30m COG" \
  ./run_ukr_grd_test.sh "$test_datasets_dir" "$copdem30_files_dir" "$products_output"
run_case "Post-IPF-2.9 UKR GRD calibration with COPDEM 30m COG" \
  ./ukr-2023-grd-calibration.sh "$test_datasets_dir" "$copdem30_files_dir" "$products_output"
run_case "Estonia S1D calibration with COPDEM 30m COG" \
  ./estonia-2026-S1D-calibration.sh "$test_datasets_dir" "$copdem30_files_dir" "$products_output"
run_case "Estonia S1ACD coherence with COPDEM 30m COG" \
  ./estonia-2026-S1ACD-coherence.sh "$test_datasets_dir" "$copdem30_files_dir" "$orbit_files_dir" \
  "$products_output"
run_case "Jupyter notebook tests" run_jupyter_case

echo
if ((failure_count == 0)); then
  echo "All E2E scripts passed"
else
  echo "$failure_count E2E script(s) failed:"
  printf '  %s\n' "${failed_cases[@]}"
fi

exit "$failure_count"
