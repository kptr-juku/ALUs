#!/bin/bash

set -euo pipefail

function print_help {
    echo "Usage:"
    echo "$0 <test data folder> <COPDEM 30m COG location> [optional - output folder]"
}

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "Wrong count of input arguments"
    print_help
    exit 1
fi

test_dataset_dir=$1
dem_files_dir=$2

if [[ $# -eq 3 ]]; then
    output_dir=$3
else
    me=$0
    me=${me##*/}
    me=${me%.*}
    output_dir="/tmp/$me"
    echo "Created output folder for results - $output_dir"
fi

mkdir -p "$output_dir"

# Sentinel-1 IPF 003.61 regression product. It exercises the post-IPF-2.9 noise metadata format
# and the sparse GRD noise range-vector coverage handled by the Microwave Toolbox fallback.
test_product="S1A_IW_GRDH_1SDV_20230702T034804_20230702T034833_049239_05EBBE_15A6.SAFE"
test_product_path="$test_dataset_dir/$test_product"
if [[ ! -d "$test_product_path" ]]; then
    echo "Missing unpacked SAFE product: $test_product_path" >&2
    exit 1
fi

dem_files=(
    "$dem_files_dir/Copernicus_DSM_COG_10_N47_00_E033_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N47_00_E034_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N47_00_E035_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N47_00_E036_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N47_00_E037_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N48_00_E033_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N48_00_E034_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N48_00_E035_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N48_00_E036_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N48_00_E037_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N49_00_E034_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N49_00_E035_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N49_00_E036_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N49_00_E037_00_DEM.tif"
)

dem_args=()
for dem_file in "${dem_files[@]}"; do
    if [[ ! -f "$dem_file" ]]; then
        echo "Missing DEM file: $dem_file" >&2
        exit 1
    fi
    dem_args+=(--dem "$dem_file")
done

output_name="S1A_IW_GRDH_1SDV_20230702T034804_20230702T034833_049239_05EBBE_15A6_tnr_Cal_VV_tc.tif"
test_product_output="$output_dir/$output_name"
time alus-cal -i "$test_product_path" \
    -o "$test_product_output" \
    -p VV -t sigma \
    "${dem_args[@]}" --ll info

if [[ -z "${NIGHTLY_GOLDEN_DIR:-}" ]]; then
    echo "no golden directory defined, no verification executed"
    exit 0
fi

echo "Validating $test_product_output"
./alus_result_check.py -I "$test_product_output" \
    -G "$NIGHTLY_GOLDEN_DIR/$output_name" \
    -O SKIP_ALUs_VERSION

exit $?
