#!/usr/bin/env bash
# Package the already-built Linux runtime for a GitHub Release.
#
# Usage:
#   scripts/package_release.sh <linux-x64|linux-x86> [version]
#
# Reads build/<platform>-release/bin/re2dj and writes
# build/package/re2dj-v<version>-<platform>.tar.gz with a matching .sha256.
# The executable must stay portable: it may link only the C runtime's own
# libraries, and its highest GLIBC_ symbol version may not pass the release
# build image's glibc (RE2DJ_MAX_GLIBC, 2.36 for Debian 12 by default).

set -euo pipefail

platform="${1:-}"
case "$platform" in
    linux-x64) elf_machine="x86-64" ;;
    linux-x86) elf_machine="Intel (80386|i386)" ;;
    *)
        echo "usage: $0 <linux-x64|linux-x86> [version]" >&2
        exit 2
        ;;
esac

repository="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repository"

version="${2:-$(tr -d '[:space:]' < VERSION)}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "version must use major.minor.patch format, found '$version'" >&2
    exit 1
fi
max_glibc="${RE2DJ_MAX_GLIBC:-2.36}"

binary="build/${platform}-release/bin/re2dj"
name="re2dj-v${version}-${platform}"
package_directory="build/package"
staging="${package_directory}/staging/${name}"
archive="${package_directory}/${name}.tar.gz"

if [[ ! -f "$binary" ]]; then
    echo "Release binary does not exist: $binary" >&2
    exit 1
fi

# The right ELF width.
description="$(file -b "$binary")"
# Older file(1) says "Intel 80386", newer "Intel i386".
if [[ ! "$description" =~ $elf_machine ]]; then
    echo "$binary is not $elf_machine: $description" >&2
    exit 1
fi

# Only the C runtime's own libraries; SDL loads the rest with dlopen.
allowed='^(libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|ld-linux.*)$'
needed="$(readelf -d "$binary" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')"
while read -r library; do
    [[ -z "$library" ]] && continue
    if [[ ! "$library" =~ $allowed ]]; then
        echo "$binary links $library, which a release may not depend on" >&2
        exit 1
    fi
done <<< "$needed"

# No glibc symbol newer than the build image's.
highest="$(objdump -T "$binary" | grep -o 'GLIBC_[0-9][0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -n 1)"
if [[ -n "$highest" ]] && [[ "$(printf '%s\n%s\n' "$highest" "$max_glibc" | sort -V | tail -n 1)" != "$max_glibc" ]]; then
    echo "$binary needs GLIBC_$highest, newer than the allowed $max_glibc" >&2
    exit 1
fi

rm -rf "$staging"
mkdir -p "$staging"
strip -o "$staging/re2dj" "$binary"
chmod 755 "$staging/re2dj"
for document in README.md LICENSE VERSION RELEASE_NOTES.md THIRD_PARTY_NOTICES.md CREDITS.md; do
    if [[ -f "$document" ]]; then
        cp "$document" "$staging/$document"
    fi
done
if [[ -d config ]]; then
    cp -R config "$staging/config"
fi

rm -f "$archive" "$archive.sha256"
tar -C "${package_directory}/staging" --owner=0 --group=0 --numeric-owner -czf "$archive" "$name"
(cd "$package_directory" && printf '%s *%s\n' "$(sha256sum "${name}.tar.gz" | cut -d' ' -f1)" "${name}.tar.gz" \
    > "${name}.tar.gz.sha256")

rm -rf "$staging"
rmdir "${package_directory}/staging" 2>/dev/null || true

echo "Release package: $archive"
echo "Release checksum: $archive.sha256"
echo "Needs: ${needed//$'\n'/ } (GLIBC_${highest:-none})"
