# Microwave Toolbox Sync Tree

## Purpose and baseline

Component mapping and implementation evidence for ALUs release 1.8.0. Priorities and benchmark decisions are in the
[upgrade review](s1tbx-microwave-toolbox-upgrade-abstract.md); ETAD numerical evidence is in [ETAD validation](etad.md).

ALUs is a native C++/CUDA port assembled from S1TBX and SNAP components, not a copy of one upstream revision.
Review lineage per component and method. Microwave Toolbox preserves S1TBX history: `9aca04cc5` renamed the modules
and `450907298` moved Java packages from `org.esa.s1tbx.*` to `eu.esa.sar.*`.

## Module map

| Legacy module | Current module | ALUs location |
|---|---|---|
| `s1tbx-commons` | `sar-commons` | `sentinel1/s1tbx-commons` |
| `s1tbx-io` | `sar-io` | `sentinel1/s1tbx-io` |
| `s1tbx-io-ephemeris` | `sar-io-ephemeris` | `sentinel1/io/orbits`, `algs/apply-orbit-file-op` |
| `s1tbx-op-calibration` | `sar-op-calibration` | `algs/sentinel1-calibrate`, `algs/thermal-noise-removal` |
| `s1tbx-op-insar` | `sar-op-insar` | `algs/coherence-cuda` |
| `s1tbx-op-sentinel1` | `sar-op-sentinel1` | `algs/backgeocoding`, `algs/topsar-*` |
| `s1tbx-op-sar-processing` | `sar-op-sar-processing` | `algs/range-doppler-terrain-correction` |
| SNAP Engine / jlinda | SNAP Engine / jlinda | `snap-engine`, `sentinel1/jlinda` |

## Component register

Paths below are relative to the ALUs root. Upstream class names refer to the current modules above; most retain
their legacy class names. Grouped entries identify the owning implementation rather than every support header.

| ALUs path | Upstream class or subsystem | Review focus |
|---|---|---|
| `sentinel1/s1tbx-commons/sentinel1_utils.*`, `subswath_info.*` | `Sentinel1Utils`, `SubSwathInfo` | Subswath/burst timing, valid samples, Doppler and geolocation metadata |
| `sentinel1/s1tbx-commons/*vector*` | Noise, azimuth-noise and calibration vectors | LUT representation and metadata selection |
| `sentinel1/s1tbx-commons/orbit_state_vectors.*` | `OrbitStateVectors` | State-vector timing and interpolation |
| `sentinel1/s1tbx-commons/s_a_r_geocoding.h`, `sar_geocoding.*`, `sar_utils.*` | `SARGeocoding`, `SARUtils` | Zero-Doppler time, slant range and CUDA equivalents |
| `sentinel1/s1tbx-io/sentinel1/sentinel1_product_reader*` | `Sentinel1ProductReader`, `Sentinel1ProductReaderPlugIn` | SAFE qualification, raster reads and complex I/Q |
| `sentinel1/s1tbx-io/sentinel1/sentinel1_level1_directory.*` | `Sentinel1Level1Directory` | Manifest, annotation, product dimensions and geocoding |
| `sentinel1/s1tbx-commons/io`, `sentinel1/s1tbx-io/geotiffxml` | Product-directory and image-reader helpers | Reader lifecycle, band metadata and GeoTIFF/XML |
| `sentinel1/s1tbx-io/sentinel1/sentinel1_etad_product.*`, `etad_computation.cuh` | ETAD reader and correction helpers | Native XML/grouped NetCDF grids and double-precision calculations |
| `sentinel1/etad` | TOPS ETAD preparation | SLC acquisition association, burst coverage and prepared correction layers |
| `sentinel1/io/orbits`, `algs/apply-orbit-file-op` | `SentinelPODOrbitFile`, `ApplyOrbitFileOp` | Orbit lookup, interpolation and metadata updates |
| `algs/sentinel1-calibrate` | `Sentinel1Calibrator`, `CalibrationOp` | LUT interpolation, calibration formulas and output representation |
| `algs/thermal-noise-removal` | `Sentinel1RemoveThermalNoiseOp` | GRD/SLC noise selection, interpolation and application |
| `algs/backgeocoding` | `BackGeocodingOp` | Burst pairing, bounds, DEM masks, deramp/demod, interpolation and ETAD pair correction |
| `algs/coherence-cuda` | `CoherenceOp`, jlinda helpers | Windows, flat-earth phase, complex accumulation and no-data |
| `algs/coherence-estimation-routine` | Split/orbit/coregistration/coherence/deburst/terrain-correction graph | Chain defaults, timeline selection and output metadata |
| `algs/topsar-split`, `algs/topsar-deburst-op`, `algs/topsar-merge` | `TOPSARSplitOp`, `TOPSARDeburstOp`, `TOPSARMergeOp` | Burst selection, stitching, raster geometry and subset metadata |
| `algs/range-doppler-terrain-correction` | `RangeDopplerGeocodingOp` | Target geometry, DEM, interpolation, boundaries and no-data |
| `snap-engine/snap-core`, `snap-engine/snap-engine-utilities` | SNAP product model, `ReaderUtils`, metadata helpers | Product/band/subset behavior, tie-point grids and validation |
| `snap-engine/snap-dem`, `snap-engine/triangular_interpolation*` | DEM/EGM96 and triangle interpolation | Elevation and backgeocoding support |

## Implemented synchronization

Reader, LUT and thermal-noise changes were included through [PR #7](https://github.com/kptr-juku/ALUs/pull/7).
Burst-product and ETAD work followed in release 1.8.0. The tables record implemented behavior, not full module parity.

### Reader and geocoding

| Upstream reference | Implemented behavior | ALUs evidence |
|---|---|---|
| `12db283e1` | Lower-case image names for image-to-band metadata lookup | `sentinel1_level1_directory.cc` |
| `d8cd6bb39`, `73e10e98b` | Wrapped/direct `xmlData`; guarded optional orbit metadata | `c8112e0b` |
| `d8353246c` | Normalize negative longitudes before interpolation across the antimeridian | `c8112e0b` |
| `a8dd51abe` | Guard invalid raster/grid dimensions before tie-point-grid creation | `c8112e0b`; dimension guard only |
| `f08897475` | Key IW band geocoding by TOPSAR prefix | `c8112e0b` |
| `4adf88da0` | Close the underlying image reader | `ImageIOFile::Close()` |
| SNAP Engine `057211babf`, `904e622ac0`, `117bd71c68` | Clamp generated grid dimensions and use double subsampling | `c8112e0b`, `972ed881` |
| ALUs-specific port fix | Return the fine-grid output vector by reference | `972ed881`, `ReaderUtils::CreateFineTiePointGrid()` |

The fine-grid fix matters because terrain correction consumes product latitude/longitude grids. The STEP report
[Geolocation inaccuracy](https://forum.step.esa.int/t/geolocation-inaccuracy/37504) is subset-size geolocation evidence,
not proof that this helper alone explains the upstream issue.

### Processing

| Component | Implemented behavior / current status | Evidence |
|---|---|---|
| Noise/calibration parsing | Accept `noiseLut` and `noiseRangeLut`, general ASCII whitespace, declared/aligned count validation | `a98cb7af`; upstream `860a618515`, `e048ff875` |
| Thermal noise | Swath-constrained GRD fallback, azimuth-time interpolation, SLC subset coordinates and absolute burst alignment, constant one-value LUTs | `81d5fb6e`; upstream `8be3a2b94`, `38418ee3d`, `7be4ed8a4` |
| Calibration | Numerical behavior reviewed and manually accepted for current IW inputs | Parser changes retained; no further formula change required |
| Burst products | Pseudo split and burst-product handling | `b9378a65`, verification recorded in `e11ed229` |
| TOPSAR geometry | Split/metadata and deburst raster-size/geolocation fixes | `d2c00788`, `531fc0cc` |
| ETAD | Native reader, acquisition/burst association, GPU pair correction and experimental pairwise coherence integration | `6d48c480`; [scope and validation](etad.md) |

ETAD rotates secondary complex samples before coherence accumulation. SNAP's classical coherence branch uses
uncorrected samples, so its ETAD coherence is not a direct numerical baseline. Detailed formulas and limitations
are maintained in the ETAD document rather than duplicated here.

## Verification evidence

* Thermal-noise work recorded 26 tests and four real calibration executions. The reported
  `S1A_IW_GRDH_1SDV_20230702T034804_20230702T034833_049239_05EBBE_15A6.SAFE` failure completes using the IW2
  fallback to range vector 2, covering the behavior behind ALUs issues
  [#37](https://github.com/cgi-estonia-space/ALUs/issues/37) and [#39](https://github.com/cgi-estonia-space/ALUs/issues/39).
* Virumaa calibration remained pixel-identical to its golden. The changed S1D result from
  `build-automation/estonia-2026-S1D-calibration.sh` was manually verified and accepted.
* ETAD metadata/preparation tests and Saint-Etienne/Storm Daniel comparison workflows accompany the implementation.
  [ETAD validation](etad.md) records native-precision differences, pair-correction comparisons and coherence behavior.
* AOI and timeline workflows cover geometry inputs and Sentinel-1A/1C/1D processing; see `build-automation`.

## Remaining review register

| Component | Status / next action |
|---|---|
| UTC MJD rounding | Pending narrow port of SNAP Engine `ddf5a79cfc`, with a boundary regression test |
| ETAD coherence | Experimental implementation; assess acceptance using the documented comparison stages |
| TOPSAR and reader | Current product scope implemented; investigate additional timing, valid-pixel, subset or merge differences against concrete inputs |
| Orbit application | Evidence-driven review of lookup, interpolation, timing and metadata behavior |
| Backgeocoding/coherence | ETAD integration implemented; broader bounds, masks, interpolation, defaults and no-data parity remain partial |
| Terrain correction | Fine-grid and TOPSAR geometry fixes implemented; investigate additional geolocation/boundary differences when demonstrated |

For each new review, record the ALUs function, upstream class/commit, behavioral difference, quality impact,
implementation decision and test evidence. Benchmark priorities and acceptance questions are maintained in the
[upgrade review](s1tbx-microwave-toolbox-upgrade-abstract.md).
