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

if ! command -v ogr2ogr >/dev/null 2>&1; then
    echo "Missing required GDAL tool: ogr2ogr" >&2
    exit 1
fi

s1a_20260412="S1A_IW_SLC__1SDV_20260412T180621_20260412T180648_064050_080F15_EEE5.SAFE"
s1c_20260418="S1C_IW_SLC__1SDV_20260418T180513_20260418T180540_007274_00EBE7_EBBB.SAFE"

for slc_product in "$s1a_20260412" "$s1c_20260418"; do
    if [[ ! -d "$test_dataset_dir/$slc_product" ]]; then
        echo "Missing unpacked SAFE product: $test_dataset_dir/$slc_product" >&2
        exit 1
    fi
done

dem_files=(
    "$dem_files_dir/Copernicus_DSM_COG_10_N51_00_W006_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N51_00_W005_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N51_00_W004_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N51_00_W003_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N52_00_W007_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N52_00_W006_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N52_00_W005_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N52_00_W004_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N52_00_W003_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N53_00_W007_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N53_00_W006_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N53_00_W005_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N53_00_W004_00_DEM.tif"
    "$dem_files_dir/Copernicus_DSM_COG_10_N53_00_W003_00_DEM.tif"
)

dem_args=()
for dem_file in "${dem_files[@]}"; do
    if [[ ! -f "$dem_file" ]]; then
        echo "Missing DEM file: $dem_file" >&2
        exit 1
    fi
    dem_args+=(--dem "$dem_file")
done

s1a_20260412_orbit="$orbit_files_dir/S1A_OPER_AUX_POEORB_OPOD_20260502T070443_V20260411T225942_20260413T005942.EOF"
s1c_20260418_orbit="$orbit_files_dir/S1C_OPER_AUX_POEORB_OPOD_20260508T070852_V20260417T225942_20260419T005942.EOF"

for orbit_file in "$s1a_20260412_orbit" "$s1c_20260418_orbit"; do
    if [[ ! -f "$orbit_file" ]]; then
        echo "Missing orbit file: $orbit_file" >&2
        exit 1
    fi
done

aoi_wkt='POLYGON ((-3.746414 52.216364, -3.665496 52.216364, -3.665496 52.271574, -3.746414 52.271574, -3.746414 52.216364))'
aoi_dir="$output_dir/aoi-various"
geojson_aoi="$aoi_dir/coh6_VV_103.geojson"
shp_aoi="$aoi_dir/coh6_VV_103.shp"

mkdir -p "$aoi_dir"
cat > "$geojson_aoi" <<'EOF'
{
  "type": "FeatureCollection",
  "name": "coh6_VV_103",
  "features": [
    {
      "type": "Feature",
      "properties": { "id": 1 },
      "geometry": {
        "type": "Polygon",
        "coordinates": [[
          [-3.746414, 52.216364],
          [-3.665496, 52.216364],
          [-3.665496, 52.271574],
          [-3.746414, 52.271574],
          [-3.746414, 52.216364]
        ]]
      }
    }
  ]
}
EOF

for shp_ext in shp shx dbf prj cpg; do
    rm -f "$aoi_dir/coh6_VV_103.$shp_ext"
done
ogr2ogr -overwrite -f "ESRI Shapefile" -nlt POLYGON -a_srs EPSG:4326 "$shp_aoi" "$geojson_aoi"

function run_coherence {
    local aoi=$1
    local output_path=$2

    time alus-coh -r "$test_dataset_dir/$s1a_20260412" \
         -s "$test_dataset_dir/$s1c_20260418" \
         --orbit_ref "$s1a_20260412_orbit" \
         --orbit_sec "$s1c_20260418_orbit" \
         -p VV \
         -a "$aoi" \
         -o "$output_path" \
         --ll info \
         "${dem_args[@]}"
}

test_1_prod_path="$output_dir/20260412T180621_20260418T180513_S1A_S1C_coh6_VV_103.tif"
test_2_prod_path="$output_dir/20260412T180621_20260418T180513_S1A_S1C_coh6_VV_103-shp.tif"
test_3_prod_path="$output_dir/20260412T180621_20260418T180513_S1A_S1C_coh6_VV_103-geojson.tif"

run_coherence "$aoi_wkt" "$test_1_prod_path"
run_coherence "$shp_aoi" "$test_2_prod_path"
run_coherence "$geojson_aoi" "$test_3_prod_path"

if [[ -z "${NIGHTLY_GOLDEN_DIR:-}" ]]; then
    echo "no golden directory defined, no verification executed"
    exit 0
fi

set +e

golden_prod_path="$NIGHTLY_GOLDEN_DIR"/20260412T180621_20260418T180513_S1A_S1C_coh6_VV_103.tif

echo "Validating $test_1_prod_path"
./alus_result_check.py -I "$test_1_prod_path" -G "$golden_prod_path" -O SKIP_ALUs_VERSION SKIP_AREA_SELECTION
res1=$?

echo "Validating $test_2_prod_path"
./alus_result_check.py -I "$test_2_prod_path" -G "$golden_prod_path" -O SKIP_ALUs_VERSION SKIP_AREA_SELECTION
res2=$?

echo "Validating $test_3_prod_path"
./alus_result_check.py -I "$test_3_prod_path" -G "$golden_prod_path" -O SKIP_ALUs_VERSION SKIP_AREA_SELECTION
res3=$?

exit $((res1 | res2 | res3))
