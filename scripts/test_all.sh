#!/usr/bin/env bash
# Build re2DJ and run the full unit test suite on a Linux x86-64 host.
#
# Warnings are errors here: a warning that only CI rejects is a warning that
# reaches the default branch first.

set -euo pipefail

preset="${1:-linux-x64-debug}"
repository="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repository"

cmake --preset "$preset" -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset "$preset"
ctest --preset "$preset"
