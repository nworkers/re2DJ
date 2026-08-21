#!/usr/bin/env bash
# Configure and build re2DJ on a Linux x86-64 host.
#
# Usage:
#   scripts/build.sh [preset]

set -euo pipefail

preset="${1:-linux-x64-debug}"
repository="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repository"

cmake --preset "$preset"
cmake --build --preset "$preset"

echo
echo "Build output: build/${preset}/bin"
