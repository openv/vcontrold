#!/bin/sh
set -eu

project_dir=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$project_dir/build-quality"}

for program in clang-18 clang-tidy run-clang-tidy scan-build cppcheck valgrind; do
    if ! command -v "$program" >/dev/null 2>&1; then
        echo "Missing required program: $program" >&2
        exit 2
    fi
done

cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_C_COMPILER="$(command -v clang-18)" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMANPAGES=OFF \
    -DVSIM=ON \
    -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

cppcheck \
    --project="$build_dir/compile_commands.json" \
    --enable=warning,performance,portability \
    --check-level=exhaustive \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    --error-exitcode=2 \
    --quiet

run-clang-tidy \
    -p "$build_dir" \
    -config-file="$project_dir/.clang-tidy" \
    -header-filter="^$project_dir/src/" \
    "$project_dir/src"

scan_dir="$build_dir/scan-build"
scan-build --status-bugs cmake \
    -S "$project_dir" -B "$scan_dir" \
    -DCMAKE_C_COMPILER="$(command -v clang-18)" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMANPAGES=OFF \
    -DVSIM=ON \
    -DBUILD_TESTING=ON
scan-build --status-bugs cmake --build "$scan_dir" --clean-first --parallel

# Valgrind 3.22 cannot initialize its private descriptors with the unusually
# large soft descriptor limit used by some containers and CI runners.
# Linux is the supported environment for this quality script.
# shellcheck disable=SC3045
ulimit -n 1024

for test_binary in \
    test_common test_client test_parser test_framer test_arithmetic test_cycle_time
do
    valgrind \
        --quiet \
        --leak-check=full \
        --show-leak-kinds=definite \
        --errors-for-leak-kinds=definite \
        --error-exitcode=99 \
        "$build_dir/$test_binary"
done
