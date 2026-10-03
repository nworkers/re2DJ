#!/usr/bin/env bash
# Build, test and package one Linux release platform.
#
# Usage:
#   scripts/build_release_linux.sh <linux-x64|linux-x86> [version]
#
# The release workflow runs this inside a Debian 12 container (debian:bookworm
# or i386/debian:bookworm) so the executable runs on glibc 2.36 and later; it
# works the same on a developer machine with the build packages installed.
# Warnings are errors, as in CI.

set -euo pipefail

platform="${1:-}"
case "$platform" in
    linux-x64 | linux-x86) ;;
    *)
        echo "usage: $0 <linux-x64|linux-x86> [version]" >&2
        exit 2
        ;;
esac

repository="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repository"

preset="${platform}-release"
cmake --preset "$preset" -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset "$preset"
ctest --test-dir "build/${preset}" --output-on-failure
bash scripts/package_release.sh "$platform" "${2:-}"
