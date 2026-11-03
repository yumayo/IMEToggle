#!/bin/sh
set -eu

source_dir=${1:-/workspace}
build_root=${IMETOGGLE_BUILD_ROOT:-/build}
output_dir=${IMETOGGLE_OUTPUT_DIR:-/output}

# Run portable tests with the Linux compiler, without Wine or a Windows runtime.
cmake -S "$source_dir" -B "$build_root/native" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$build_root/native" --parallel
ctest --test-dir "$build_root/native" --output-on-failure

# Build a native Windows executable with statically linked C++ libraries.
cmake -S "$source_dir" -B "$build_root/windows-x64" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_TOOLCHAIN_FILE="$source_dir/cmake/mingw-x64.cmake"
cmake --build "$build_root/windows-x64" --parallel
cmake --install "$build_root/windows-x64" --prefix "$output_dir"
printf 'Windows executable: %s/IMEToggle.exe\n' "$output_dir"
