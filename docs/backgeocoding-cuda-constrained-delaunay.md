# Backgeocoding CUDA Constrained Delaunay

## Decision

ALUs backgeocoding uses a CUDA DEM-grid constrained Delaunay triangulation instead of copying control points to the CPU for SNAP's global point-set Delaunay triangulation. This is an intentional performance-oriented algorithm divergence, not a claim of pixel parity with SNAP.

The implementation remains burst based. A TOPS burst is the largest natural work unit because positioning, valid-line timing, and deramp/demod metadata are burst specific. ALUs processes each burst at full range width and no longer divides it into Java-style range tiles or schedules nine CPU workers.

## Algorithm Difference

| Behavior | SNAP and previous ALUs path | CUDA constrained path |
|---|---|---|
| Input topology | Unordered set of valid master azimuth/range points | Rectangular DEM sampling grid |
| Triangulation | Global, unconstrained point-set Delaunay | Cell-constrained Delaunay on the DEM grid |
| Connectivity | May connect non-neighboring grid points | Never connects points from different grid cells |
| Invalid points | Removed before triangulation; the global triangulation can bridge the resulting gap | Leave a hole in the constrained grid |
| Four valid cell corners | Determined by the global triangulation | Two triangles split on the local Delaunay diagonal using an in-circle test |
| Three valid cell corners | May be connected through the global triangulation | One triangle inside that cell |
| Fewer than three valid corners | No local triangle | No triangle |
| Non-convex or degenerate cell | Handled as part of the global point set | Rejected to avoid folded or zero-area triangles |
| Data movement | Master coordinates move device-to-host; triangles move host-to-device | Coordinates and triangles remain on the GPU |
| Output capacity | Compact global triangle list | Two fixed slots per DEM-grid cell; unused slots carry the invalid marker |

The CUDA result is a constrained Delaunay triangulation because DEM-grid cell edges are constraints. It is not equivalent to SNAP's unconstrained global Delaunay triangulation. In particular, choosing a Delaunay diagonal independently in each cell does not make unconstrained grid edges globally Delaunay.

## Control-Point Validity

A point participates only when all of the following hold:

- DEM elevation or the configured EGM fallback is valid.
- Master positioning succeeds.
- Secondary positioning succeeds.
- Master and secondary azimuth/range coordinates are finite.

Coordinate arrays are initialized with `INVALID_INDEX` before positioning. This removes the previous uninitialized failure paths that could feed arbitrary values to the CPU triangulator and cause nondeterministic failures or long-running triangulation.

## Numerical Expectations

The existing `56 x 56` triangular-interpolation fixture contains 3,136 valid control points. The previous global triangulation emits 6,248 triangles; the constrained grid emits 6,050 triangles. On its 10,000 output samples, maximum observed interpolation differences from the global fixture are:

| Quantity | Maximum absolute difference |
|---|---:|
| Secondary azimuth | `2.8e-5` |
| Secondary range | `3.1e-5` |
| Latitude | `1.6e-6` |
| Longitude | `1.4e-5` |

The unit regression allows `5e-5` for this fixture. Downstream coherence can amplify small complex-sample changes, so final rasters must be evaluated using both pixel differences and aggregate statistics.

## Downstream Validation

The following results compare the full-width-burst CUDA path against the pre-change global CPU path. Coherence, deburst, merge, and terrain-correction code were not changed.

| Scenario | Exact pixel differences | No-data mask differences | MAE | Maximum difference | Valid coverage | Mean change | Standard-deviation change |
|---|---:|---:|---:|---:|---:|---:|---:|
| Maharashtra S1A to S1A, IW3 | 2,328,223 of 109,303,650 (`2.13%`) | 205 (`0.000188%`) | `3.41e-5` | `0.75792598` | `43.07%` to `43.07%` | `-0.0230%` | `-0.0085%` |
| Estonia S1D to S1A, IW1-IW3 | 21,866,466 of 622,957,125 (`3.51%`) | 274 (`0.000044%`) | `2.70e-6` | `0.55475450` | `63.48%` to `63.48%` | `-0.00084%` | `+0.00064%` |
| Estonia S1A to S1C, IW1-IW3 | 21,867,881 of 621,799,618 (`3.52%`) | 198 (`0.000032%`) | `2.81e-6` | `0.64933228` | `63.69%` to `63.69%` | `-0.00087%` | `+0.00045%` |

These products are not golden-file compatible. Their geotransforms, dimensions, extrema, valid coverage, means, and standard deviations remain stable, and no-data mask changes are below `0.0002%`. The isolated maxima are nevertheless large enough that the pixel-level deltas require visual/manual review before replacing any golden products.

The final finite-coordinate and missing-DEM guards were rerun on all three products. Their rasters matched the measured CUDA products exactly under `alus_result_check`, and the generated products passed manual visual review.

## Performance Evidence

Measurements were collected on an NVIDIA GeForce RTX 5060 Laptop GPU with the same input products and local data cache.

| Scenario | Previous path | CUDA, old range tiles | CUDA, full-width bursts |
|---|---:|---:|---:|
| Maharashtra coregistration | `14.13 s` | `7.23 s` | `17.37-18.08 s` |
| Estonia S1D to S1A end to end | `87.96 s` | Not retained | `88.73-95.53 s` |
| Estonia S1A to S1C end to end | `74.87 s` | Not retained | `77.40-77.72 s` |

The old-range-tile measurement isolates the benefit of removing CPU Delaunay. The final full-width-burst path is slower because large synchronous allocations and host/device transfers are no longer hidden by CPU worker overlap. In the Maharashtra Nsight Systems traces, whole-process host-to-device traffic falls from `11.788 GB` to `7.793 GB` and device-to-host traffic falls from `4.778 GB` to `4.279 GB`, but large per-burst copies and frees dominate host API time. Optimizing those existing CUDA and transfer paths is intentionally deferred.

## COPDEM Scope

The ALUs COPDEM design remains authoritative. Variable-width 30 m COG tiles continue to be normalized with the documented row-neighbor resampling strategy; SNAP DEM behavior was not copied over it. The related changes are limited to deterministic bounds safety:

- The row-neighbor resampler clamps the second source index at the final column.
- A requested COPDEM sample is initialized to NaN before tile lookup, so an absent tile cannot return uninitialized stack data.
- A missing or out-of-grid tile in a bilinear neighborhood forces a no-data result instead of falling back to another corner.
- Grid tile-count bounds use exclusive upper limits.

DEM loading remains a separate one-time, I/O-bound cost and is not part of the CUDA triangulation optimization.

## Remaining Risks

- Invalid-grid holes are preserved instead of bridged, so coastlines and failed-position boundaries can differ from SNAP.
- The in-circle and orientation predicates use double precision, not adaptive exact arithmetic.
- Existing triangular interpolation assigns work per triangle and can write shared triangle-edge pixels concurrently. Adjacent planes agree at their shared endpoints, but final-bit determinism should remain in the regression matrix.
- Full-width bursts increase peak allocation size and are currently slower than the CUDA triangulator with range tiling. The tested Maharashtra and Estonia products fit the 8 GB GPU, but larger products require memory validation.
- Golden products must not be replaced based only on aggregate statistics. The generated Maharashtra and Estonia products passed manual visual review, but adopting them as new goldens remains a separate decision.
