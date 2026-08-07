#!/usr/bin/env bash

# Treat unset variables and failed pipeline commands as errors. Do not use -e because every test executable must run.
set -u -o pipefail

# BASH_SOURCE identifies this script even when invoked through a relative path. ALUS_TEST_DIR defaults to the caller's
# current directory to preserve the existing CI invocation.
script_path=$(realpath -- "${BASH_SOURCE[0]}")
test_dir=${ALUS_TEST_DIR:-.}

if ! cd -- "$test_dir"; then
    echo "Unit test directory does not exist: $test_dir" >&2
    exit 1
fi

result=0
tests_found=0

for exe in ./*; do
    # -ef compares file identity, preventing an executable copy of this script from recursively running itself.
    if [[ ! -f "$exe" || ! -x "$exe" || "$exe" -ef "$script_path" ]]; then
        continue
    fi

    tests_found=1
    # ${exe#./} removes the display-only "./" prefix; "$@" forwards every GoogleTest argument unchanged.
    echo "Running ${exe#./}"
    "$exe" "$@"
    last_result=$?
    # Preserve a non-zero final status if any executable fails while still allowing the remaining tests to run.
    result=$((result | last_result))
done

if ((tests_found == 0)); then
    echo "No unit test executables found in $(pwd)" >&2
    exit 1
fi

exit "$result"
