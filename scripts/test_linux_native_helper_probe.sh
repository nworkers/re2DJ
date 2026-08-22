#!/usr/bin/env bash

# Build and run the Linux x64 host / i386 native-helper integration probe.

set -euo pipefail

repository=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repository"

cmake --preset linux-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x64-debug
ctest --preset linux-x64-debug --output-on-failure

cmake --preset linux-x86-native-probe -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x86-native-probe

host_probe="$repository/build/linux-x64-debug/bin/re2dj_linux_native_ipc_host_probe"
helper_probe="$repository/build/linux-x86-native-probe/bin/re2dj_linux_native_ipc_helper_probe"
file "$host_probe" "$helper_probe"
"$host_probe" "$helper_probe"
