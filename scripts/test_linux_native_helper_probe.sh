#!/usr/bin/env bash

# Build and run the Linux x64 host / i386 native-helper integration probe.

set -euo pipefail

repository=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repository"

cmake --preset linux-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x64-debug
ctest --preset linux-x64-debug --output-on-failure

cmake --preset linux-x86-helper -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x86-helper

host_probe="$repository/build/linux-x64-debug/bin/re2dj_linux_native_ipc_host_probe"
helper_probe="$repository/build/linux-x86-helper/bin/re2dj_linux_native_ipc_helper"
rejection_helper="$repository/build/linux-x64-debug/bin/re2dj_linux_capability_rejection_helper"
file "$host_probe" "$helper_probe"
"$host_probe" "$helper_probe"
"$host_probe" "$helper_probe" --stop
"$host_probe" "$rejection_helper" --reject

cmake --preset linux-x86-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset linux-x86-debug
ctest --preset linux-x86-debug --output-on-failure

host_probe_x86="$repository/build/linux-x86-debug/bin/re2dj_linux_native_ipc_host_probe"
rejection_helper_x86="$repository/build/linux-x86-debug/bin/re2dj_linux_capability_rejection_helper"
file "$host_probe_x86" "$helper_probe"
"$host_probe_x86" "$helper_probe"
"$host_probe_x86" "$helper_probe" --stop
"$host_probe_x86" "$rejection_helper_x86" --reject
