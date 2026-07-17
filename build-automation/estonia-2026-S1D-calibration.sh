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

test_product="S1D_IW_SLC__1SDV_20260622T155540_20260622T155607_003351_005E29_9073.SAFE"
test_product_path="$test_dataset_dir/$test_product"
if [[ ! -d "$test_product_path" ]]; then
    echo "Missing unpacked SAFE product: $test_product_path" >&2
    exit 1
fi

dem_files=(
    "$dem_files_dir/Copernicus_DSM_COG_10_N57_00_E024_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N57_00_E025_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N57_00_E026_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N57_00_E027_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N57_00_E028_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N58_00_E023_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N58_00_E024_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N58_00_E025_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N58_00_E026_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N58_00_E027_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N58_00_E028_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N59_00_E023_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N59_00_E024_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N59_00_E025_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N59_00_E026_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N59_00_E027_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N59_00_E028_00_DEM.tif"
)

dem_args=()
for dem_file in "${dem_files[@]}"; do
    if [[ ! -f "$dem_file" ]]; then
        echo "Missing DEM file: $dem_file" >&2
        exit 1
    fi
    dem_args+=(--dem "$dem_file")
done

test_1_prod_path="$output_dir/S1D_IW_SLC__1SDV_20260622T155540_20260622T155607_003351_005E29_9073_tnr_Cal_deb_mrg_IW1_IW2_IW3_tc.tif"
time alus-cal -i "$test_product_path" \
     -o "$test_1_prod_path" \
     -p VV -t sigma \
     "${dem_args[@]}" --ll info

if [[ -z "${NIGHTLY_GOLDEN_DIR:-}" ]]; then
    echo "no golden directory defined, no verification executed"
    exit 0
fi

echo "Validating $test_1_prod_path"
./alus_result_check.py -I "$test_1_prod_path" \
    -G "$NIGHTLY_GOLDEN_DIR"/S1D_IW_SLC__1SDV_20260622T155540_20260622T155607_003351_005E29_9073_tnr_Cal_deb_mrg_IW1_IW2_IW3_tc.tif \
    -O SKIP_ALUs_VERSION

exit $?
