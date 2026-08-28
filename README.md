# ALUS

Collection of processing operators/routines defined by software - plural of ALU(Arithmetic Logic Unit)  
or  
ALU for Space/Surveillance etc.

A software project that targets to utilize Nvidia GPUs for processing earth observation data (faster).  
Kickstart of this project was funded
through [ESA's EOEP programme](http://www.esa.int/About_Us/Business_with_ESA/Business_Opportunities/Earth_Observation_Envelope_Programme)  
And further development was funded
through [ESA's GSTP programme](https://www.esa.int/Enabling_Support/Space_Engineering_Technology/Shaping_the_Future/About_the_General_Support_Technology_Programme_GSTP)
Developed by [CGI Estonia](https://www.cgi.com/ee/et).

Current fork is developed through contract CT-EX2026D1431914-101 of the European Commission

## [Quick performance overview](PERFORMANCE.md)
For further comprehensive evaluation see [Wiki](https://github.com/cgi-estonia-space/ALUs/wiki).

# Installation And Build

Use [ALUs-platform](https://github.com/kptr-juku/ALUs-platform) for prerequisites, dependencies, local builds, Docker
image builds, and container usage. See [Dependencies and Prerequisites](DEPENDENCIES.md) for an overview of its setup
layers. Verified releases are available from https://github.com/kptr-juku/ALUs/releases.

The development setup from ALUs-platform is required for source builds. Initialize the submodules and configure an
out-of-source release build:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF
cmake --build build -j8
```

To build and run the repository tests, use a separate build directory:

```bash
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=ON
cmake --build build-tests -j8
ctest --test-dir build-tests --output-on-failure
```

When compiler discovery needs to be overridden, specify the host compilers and `nvcc` path during the first configure:

```bash
CC=/usr/bin/gcc CXX=/usr/bin/g++ cmake -S . -B build-custom \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTS=OFF \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12/bin/nvcc
cmake --build build-custom -j8
```

Use a fresh build directory when changing compilers. Built executables are placed in `<build_dir>/alus_package`.

## Executing

Each algorithm is a separate executable. Currently available ones are (more info and usage in parenthesis):

* Sentinel 1 coherence estimation routine - ``alus-coh`` ([README](algs/coherence-estimation-routine/README.md))
* Sentinel 1 coherence estimation timeline generation - ``alus-coht`` ([README](algs/coherence-estimation-routine/README.md))
* Sentinel 1 calibration routine - ``alus-cal`` ([README](algs/calibration-routine/README.md))
* Sentinel 2 and other raster resample and tiling - ``alus-resa`` ([README](algs/resample/README.md))
* Gabor feature extraction - ``alus-gfe`` ([README](algs/feature-extraction-gabor/README.md))
* PALSAR level 0 focuser - ``alus-palsar-focus`` ([README](algs/palsar-focus/README.md))

Update **PATH** environment variable in order to execute everywhere:  
``export PATH=$PATH:/path/to/<alus_package>``

See ``--help`` for specific arguments/parameters how to invoke processing. For more information see detailed explanation
of Sentinel 1 processors' [processing arguments](docs/PROCESSING_ARGUMENTS.md).

# Minimum/Recommended requirements

For specific, check each processor's README.

Below are rough figures:
* NVIDIA GPU device compute capability 6.0 (Pascal) or higher
* 2(minimum)/4(recommended) GB of available device memory (some ALUs can manage with less)
* High speed (NVMe) SSD to benefit from the computation speedups
* 4 GB of extra RAM to enable better caching/input-output (GDAL raster IO)

# Contributing

[Contribution guidelines](CONTRIBUTING.md)

# License

[GNU GPLv3](LICENSE.txt)

# [Release notes](RELEASE.md)

[Binary downloads](https://github.com/kptr-juku/ALUs/releases/)

# Troubleshooting

For CUDA related errors - [CUDA troubleshooting](CUDA_TROUBLESHOOT.md).
