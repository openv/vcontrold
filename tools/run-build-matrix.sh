#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
matrix_root=${TMPDIR:-/tmp}/vcontrold-build-matrix-$$
trap 'rm -rf "$matrix_root"' EXIT HUP INT TERM

configure_and_build()
{
    name=$1
    shift
    build_dir=$matrix_root/$name
    cmake -S "$source_dir" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        "$@"
    cmake --build "$build_dir" --parallel
    if [ "$name" = full ]; then
        ctest --test-dir "$build_dir" --output-on-failure
    fi
}

configure_and_build minimal \
    -DMANPAGES=OFF -DVCLIENT=OFF -DVSIM=OFF -DBUILD_TESTING=OFF
configure_and_build client \
    -DMANPAGES=OFF -DVCLIENT=ON -DVSIM=OFF -DBUILD_TESTING=OFF
configure_and_build simulator \
    -DMANPAGES=OFF -DVCLIENT=OFF -DVSIM=ON -DBUILD_TESTING=OFF
configure_and_build full \
    -DMANPAGES=OFF -DVCLIENT=ON -DVSIM=ON -DBUILD_TESTING=ON

if command -v rst2man >/dev/null 2>&1 ||
        command -v rst2man.py >/dev/null 2>&1; then
    configure_and_build manpages \
        -DMANPAGES=ON -DVCLIENT=ON -DVSIM=OFF -DBUILD_TESTING=OFF
else
    printf '%s\n' 'SKIP: rst2man is unavailable; manpage profile not built.'
fi
