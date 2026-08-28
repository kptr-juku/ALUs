# Dependencies And Prerequisites

Use [ALUs-platform](https://github.com/kptr-juku/ALUs-platform) for supported environments and dependency setup. Its
platform directories provide composable setup levels:

* `install_*_base.sh` scripts install the platform's NVIDIA/CUDA base where needed.
* `setup_runtime.sh` installs the libraries required to run ALUs.
* `setup_dev.sh` or `setup_devel.sh` adds compilers, CMake, headers, and development tools.
* Platform Dockerfiles compose the same scripts to create reproducible environments.

Building ALUs from source requires the development setup in addition to the applicable base and runtime setup. Some
platforms also provide optional extra-development scripts, but these are not required for a standard ALUs build.
