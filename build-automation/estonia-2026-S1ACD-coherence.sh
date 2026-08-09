#!/bin/bash

set -euo pipefail

function print_help {
    echo "Usage:"
    echo "$0 <eo data dir> <COPDEM 30m COG location> <orbit files dir> [optional - output folder]"
}

if [[ $# -lt 3 || $# -gt 4 ]]; then
    echo "Wrong count of input arguments"
    print_help
    exit 1
fi

test_dataset_dir=$1
dem_files_dir=$2
orbit_files_dir=$3

if [[ $# -eq 4 ]]; then
    output_dir=$4
else
    me=$0
    me=${me##*/}
    me=${me%.*}
    output_dir="/tmp/$me"
    echo "Created output folder for results - $output_dir"
fi

mkdir -p "$output_dir"

s1d_20260622="S1D_IW_SLC__1SDV_20260622T155540_20260622T155607_003351_005E29_9073.SAFE"
s1a_20260627="S1A_IW_SLC__1SDV_20260627T155620_20260627T155647_065157_0836A8_D8AA.SAFE"
s1c_20260628="S1C_IW_SLC__1SDV_20260628T155528_20260628T155555_008308_0106FE_42F0.SAFE"

for slc_product in "$s1d_20260622" "$s1a_20260627" "$s1c_20260628"; do
    if [[ ! -d "$test_dataset_dir/$slc_product" ]]; then
        echo "Missing unpacked SAFE product: $test_dataset_dir/$slc_product" >&2
        exit 1
    fi
done

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

s1d_20260622_orbit="$orbit_files_dir/S1D_OPER_AUX_POEORB_OPOD_20260712T071613_V20260621T225942_20260623T005942.EOF"
s1a_20260627_orbit="$orbit_files_dir/S1A_OPER_AUX_POEORB_OPOD_20260717T070655_V20260626T225942_20260628T005942.EOF"
s1c_20260628_orbit="$orbit_files_dir/S1C_OPER_AUX_RESORB_OPOD_20260628T191919_V20260628T153938_20260628T185708.EOF"

for orbit_file in "$s1d_20260622_orbit" "$s1a_20260627_orbit" "$s1c_20260628_orbit"; do
    if [[ ! -f "$orbit_file" ]]; then
        echo "Missing orbit file: $orbit_file" >&2
        exit 1
    fi
done

test_1_prod_path="$output_dir/S1D_IW_SLC__1SDV_20260622T155540_20260622T155607_003351_005E29_9073_Orb_Stack_coh_deb_mrg_IW1_IW2_IW3_tc.tif"
time alus-coh -r "$test_dataset_dir/$s1d_20260622" \
     -s "$test_dataset_dir/$s1a_20260627" \
     --orbit_ref "$s1d_20260622_orbit" \
     --orbit_sec "$s1a_20260627_orbit" \
     -o "$test_1_prod_path" -p VV \
     "${dem_args[@]}" --no_mask_cor --ll info

test_2_prod_path="$output_dir/S1A_IW_SLC__1SDV_20260627T155620_20260627T155647_065157_0836A8_D8AA_Orb_Stack_coh_deb_mrg_IW1_IW2_IW3_tc.tif"
time alus-coh -r "$test_dataset_dir/$s1a_20260627" \
     -s "$test_dataset_dir/$s1c_20260628" \
     --orbit_ref "$s1a_20260627_orbit" \
     --orbit_sec "$s1c_20260628_orbit" \
     -o "$test_2_prod_path" -p VV \
     "${dem_args[@]}" --no_mask_cor --ll info

if [[ -z "${NIGHTLY_GOLDEN_DIR:-}" ]]; then
    echo "no golden directory defined, no verification executed"
    exit 0
fi

set +e

echo "Validating $test_1_prod_path"
./alus_result_check.py -I "$test_1_prod_path" \
    -G "$NIGHTLY_GOLDEN_DIR"/S1D_IW_SLC__1SDV_20260622T155540_20260622T155607_003351_005E29_9073_Orb_Stack_coh_deb_mrg_IW1_IW2_IW3_tc.tif \
    -O SKIP_ALUs_VERSION
res1=$?

echo "Validating $test_2_prod_path"
./alus_result_check.py -I "$test_2_prod_path" \
    -G "$NIGHTLY_GOLDEN_DIR"/S1A_IW_SLC__1SDV_20260627T155620_20260627T155647_065157_0836A8_D8AA_Orb_Stack_coh_deb_mrg_IW1_IW2_IW3_tc.tif \
    -O SKIP_ALUs_VERSION
res2=$?

exit $((res1 | res2))
