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

timeline_output_dir="$output_dir"
mkdir -p "$timeline_output_dir"

# This alus-coht timeline run intentionally duplicates estonia-2026-S1ACD-coherence.sh.
# Both scripts should produce identical rasters; keeping both guards parity between alus-coh and alus-coht.

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

orbit_files=(
    "$orbit_files_dir/S1D_OPER_AUX_POEORB_OPOD_20260712T071613_V20260621T225942_20260623T005942.EOF"
    "$orbit_files_dir/S1A_OPER_AUX_POEORB_OPOD_20260717T070655_V20260626T225942_20260628T005942.EOF"
    "$orbit_files_dir/S1C_OPER_AUX_RESORB_OPOD_20260628T191919_V20260628T153938_20260628T185708.EOF"
)

for orbit_file in "${orbit_files[@]}"; do
    if [[ ! -f "$orbit_file" ]]; then
        echo "Missing orbit file: $orbit_file" >&2
        exit 1
    fi
done

# timeline_end is parsed at midnight, so use the next day to include 20260628 scene.
time alus-coht \
    -i "$test_dataset_dir" \
    -s 20260622 \
    -e 20260629 \
    --relative-orbit 160 \
    --orbit-direction ascending \
    -o "$timeline_output_dir" \
    -p VV \
    --orbit_dir "$orbit_files_dir" \
    "${dem_args[@]}" \
    --no_mask_cor \
    --ll info

if [[ -z "${NIGHTLY_GOLDEN_DIR:-}" ]]; then
    echo "no golden directory defined, no verification executed"
    exit 0
fi

set +e

test_1_prod_path="$output_dir/S1D_IW_SLC__1SDV_20260622T155540_20260622T155607_003351_005E29_9073_Orb_Stack_coh_deb_mrg_IW1_IW2_IW3_tc.tif"
test_2_prod_path="$output_dir/S1A_IW_SLC__1SDV_20260627T155620_20260627T155647_065157_0836A8_D8AA_Orb_Stack_coh_deb_mrg_IW1_IW2_IW3_tc.tif"

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
