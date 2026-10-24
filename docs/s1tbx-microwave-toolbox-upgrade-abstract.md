# S1TBX / Microwave Toolbox Upgrade Review

## Objective

Review ALUs against current SNAP Microwave Toolbox for logical differences affecting Sentinel-1 IW output quality,
supported products and benchmark comparability. The findings support the JRC.D5 discussion of completed upgrades,
remaining implementation work and acceptance criteria for the restored fork.

This review reflects release 1.8.0. Component paths, upstream references and implementation commits are maintained
in the [sync tree](microwave-toolbox-sync-tree.md). Detailed ETAD formulas and measured comparisons are maintained
in [ETAD validation](etad.md). The [release notes](../RELEASE.md#release-180) summarize user-facing changes.

## Baseline and method

ALUs is a component-wise C++/CUDA port of S1TBX/SNAP logic. There is no single upstream revision representing every
operator. Reconstruct lineage from source comments, function behavior and local git history before assessing a change.
Microwave Toolbox is the active comparison source and retains the legacy S1TBX history across module/package renames.

Useful baseline evidence:

* The historical `docs/SNAP_COMPARISON.md` at ALUs `fb324b95` identifies SNAP Engine `8.0.8` and S1TBX `8.0.5`
  for ALUs `0.6`. That document was subsequently removed; it is historical evidence, not a current benchmark definition.
* Calibration/coherence routine documentation contains SNAP 8 GPT comparisons.
* Core ports originate in 2020–2023. The 2026 changes include reader and thermal-noise synchronization, burst-product
  handling, geometry fixes and ETAD integration alongside toolchain updates.

Review numerical algorithms, metadata interpretation, product support and processing defaults. Include performance
changes when they affect precision, tile boundaries or numerical results. Evaluate each finding against an actual
ALUs processing path and representative IW input before proposing an implementation.

## Release 1.8.0 assessment

The initial reader and thermal-noise synchronization is implemented and accepted for the tested inputs. Burst-product
processing and experimental ETAD phase integration are now implemented as well. Broader upstream parity remains
component-specific; the following table distinguishes completed behavior from remaining review.

| Area | Current outcome | Review consequence |
|---|---|---|
| Sentinel-1C/1D | Generic `SENTINEL-1` mission handling, S1A/C/D workflows and timeline mission filtering are present | Qualify representative mission/IPF combinations rather than treating C/D support as a missing feature |
| SAFE reader | Manifest guards, LUT parsing, antimeridian handling, TOPSAR-prefix geocoding and grid robustness are implemented | Follow up on concrete IW reader or metadata failures |
| Thermal noise | GRD fallback/interpolation and SLC subset/burst alignment are implemented; S1D output manually accepted | Preserve issue datasets and calibration scripts as regressions |
| Calibration | LUT parsing updated; numerical behavior reviewed and accepted | No additional formula change is currently required |
| Burst products | Pseudo split, burst-product verification and split/deburst geometry fixes are included | Review further boundary, timing or merge differences only with specific evidence |
| ETAD | Native reader, SLC association, prepared grids, GPU pair correction and pairwise coherence integration are implemented | Experimental phase-only behavior needs explicit benchmark acceptance criteria |
| Backgeocoding | ETAD pair-correction integration is implemented | Broader footprint, DEM-mask, burst-pairing and interpolation review remains partial |
| Coherence | ETAD correction is applied before window accumulation | Compare the actual computation stage; retain defaults, no-data and flat-earth settings in benchmark definitions |
| Terrain correction | Fine-grid and TOPSAR output-geometry fixes are implemented | Investigate demonstrated geolocation differences with DEM, output spacing and subset selection pinned |
| Orbit handling | Existing orbit application remains in use | Review lookup, interpolation, timing and metadata when a benchmark exposes a difference |

### Reader and mission evidence

The reader constructs mission names such as `SENTINEL-1D` from SAFE `familyName + number` metadata.
`InputProductValidator` and `Sentinel1Utils` validate the generic `SENTINEL-1` prefix. Current Microwave Toolbox also
uses generic validation, while its calibrator explicitly lists A/B/C/D. This supports treating mission qualification
as a dataset/metadata review rather than assuming an A/B-only restriction in ALUs.

The reader synchronization is targeted: optional manifest nodes, image-to-band lookup, LUT formats and generated
geocoding grids are covered. The sync tree retains exact upstream references. The fine-grid output-vector fix is
relevant to terrain correction because that path consumes product latitude/longitude grids. Subset-dependent
geolocation errors should still be traced through the complete split/deburst/terrain-correction chain.

### Validation already recorded

Thermal-noise verification includes dataset-independent tests and successful execution of the reported 2023 GRD
failure. Virumaa calibration remained pixel-identical to its golden; the changed S1D calibration output was manually
accepted. These are separate outcomes: processing completion proves the failure is addressed, while golden or manual
comparison assesses the resulting numerical behavior.

Burst-product work includes pseudo split support, verification and subsequent geometry fixes. AOI and timeline
workflows broaden repeatable input coverage. ETAD has reader/preparation tests and Saint-Etienne/Storm Daniel
comparison workflows. Their detailed numerical findings are recorded in the linked ETAD document.

### ETAD comparison decision

ALUs preserves native ETAD grids and performs interpolation, gradient and phase calculations in double precision.
SNAP's reader introduces Float32 conversion. Numerical comparisons must account for that precision difference;
bitwise agreement with an intermediate conversion is not the objective.

More importantly, ALUs rotates the secondary complex samples before coherence-window accumulation, whereas SNAP's
classical interferogram operator computes coherence from uncorrected samples. A constant phase rotation preserves
coherence magnitude, but phase variation within the window can alter it. SNAP ETAD coherence therefore cannot be
used as a direct numerical baseline for this experimental ALUs behavior.

Assess native correction grids, the pair-specific phase layer and the coherence effect separately. Pin the
coregistered samples when isolating the phase effect; different graph/coregistration outputs otherwise confound the
comparison. Supported invocation, formulas, limitations and measured differences are documented in [ETAD validation](etad.md).

## Remaining work and decisions

| Topic | Action | Evidence needed for closure |
|---|---|---|
| UTC MJD rounding | Port the narrow SNAP Engine `ddf5a79cfc` fix | Focused boundary regression test |
| Experimental ETAD coherence | Agree on the intended behavior and numerical tolerances | Stage-specific comparisons and documented acceptance of the coherence effect |
| Additional reader/TOPSAR differences | Investigate failures in representative IW products | Reproducer, owning operator, upstream behavior and output geometry/mask comparison |
| Orbit/coregistration/terrain correction | Review concrete discrepancies before expanding the port | Pinned orbit/DEM inputs, intermediate geometry and pixel-level evidence |
| Coherence defaults and no-data | Compare settings and masks alongside numerical output | Equal windows, flat-earth configuration and valid-pixel treatment |
| JRC.D5 benchmark matrix | Select products, reference revision and acceptance thresholds | Reproducible commands/graphs and agreed quality metrics |

The remaining parity reviews are evidence-driven. The current calibration and thermal-noise scope is complete;
the new ETAD path remains experimental. Implementation status alone does not establish benchmark acceptance.

## Benchmark definition

Pin the following before interpreting output differences:

| Parameter | Required record |
|---|---|
| Software | Exact ALUs, Microwave Toolbox and SNAP Engine revisions, build/toolchain and GPU |
| Products | SAFE names, mission, IPF version, IW SLC/GRD type, polarisation and burst/subswath selection |
| Geometry | AOI input, subset selection, output spacing, CRS, raster dimensions and geotransform |
| Elevation | Explicit SRTM3 or Copernicus DEM tiles and no-elevation masking |
| Orbits | Selected orbit files or fixed directory contents and interpolation settings |
| Coherence | Range/azimuth windows, flat-earth subtraction and polynomial parameters |
| ETAD | Matching products, correction mode, comparison stage and coregistered-input provenance |
| No-data | Output no-data values, masks and statistical exclusion rules |
| Diagnostics | ALUs `--wif` and equivalent SNAP intermediate products |
| Metrics | Geometry, valid percentage, mask differences, min/max/mean/stddev, absolute/relative pixel differences and visual checks |

Record processing success independently from golden-comparison success. For intentional numerical changes, retain
the output and report differing-pixel count, maximum difference and checksum for review. A changed result requires
an explanation tied to the processing stage and acceptance criteria, not merely a new golden.

## Review workflow and JRC.D5 questions

1. Identify the ALUs function and its practical S1TBX-era lineage.
2. Inspect relevant local upstream history and classify the behavioral change.
3. Reproduce the difference with minimal inputs or metadata; implement and verify at the owning component.
4. Run the representative processing chain and record numerical/geometry effects.
5. Present the decision with evidence, remaining risk and proposed acceptance or follow-up.

The final review needs agreement on the exact Microwave Toolbox revision, representative IW SLC/GRD products across
missions/IPF versions, and quality tolerances. ETAD additionally needs an explicit decision on the experimental
coherence behavior. These choices determine the benchmark matrix and which remaining differences justify further
implementation under the current contract or follow-up work.
