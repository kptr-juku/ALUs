#!/bin/bash

set -e

if [ "$#" -ne 2 ]; then
    echo "$0 requires repo location and output filename."
    echo "Usage:"
    echo "$0 [ALUs repo] [package filename path]"
    exit 1
fi

repo=$1
package_filename=$2
build_dir="$repo/build"
rm -rf $build_dir
CUDAARCHS="60;70;75;80;86;89;90;100;120-real;120-virtual" cmake $repo -B$build_dir -DENABLE_TESTS=false
cmake --build $build_dir --target all -- -j 8
cd $build_dir/alus_package
cp ../../VERSION ../../README.md .
cp -r ../../jupyter-notebook .
tar -czvf $package_filename *
