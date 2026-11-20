#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

cmake --preset macos-universal-release
cmake --build --preset macos-universal-release --parallel

build_dir="$repo_root/build/macos-universal-release"
artefact_dir="$build_dir/THUMPER_artefacts/Release"
archive="$build_dir/THUMPER-macOS.zip"

executables=(
    "$artefact_dir/AU/THUMPER.component/Contents/MacOS/THUMPER"
    "$artefact_dir/VST3/THUMPER.vst3/Contents/MacOS/THUMPER"
    "$artefact_dir/Standalone/THUMPER.app/Contents/MacOS/THUMPER"
)

for executable in "${executables[@]}"; do
    if [[ ! -f "$executable" ]]; then
        printf 'Missing expected macOS executable: %s\n' "$executable" >&2
        exit 1
    fi

    lipo "$executable" -verify_arch arm64 x86_64
done

au_install_dir="$HOME/Library/Audio/Plug-Ins/Components/THUMPER.component"
if [[ -e "$au_install_dir" ]]; then
    printf 'Refusing to overwrite an existing Audio Unit: %s\n' "$au_install_dir" >&2
    exit 1
fi

mkdir -p "$(dirname "$au_install_dir")"
ditto "$artefact_dir/AU/THUMPER.component" "$au_install_dir"
trap 'rm -rf "$au_install_dir"' EXIT
plutil -p "$au_install_dir/Contents/Info.plist"
au_components="$(auval -a)"
grep -Ei 'THUMPER|Thmp' <<< "$au_components" || true
auval -v aufx Thmp Thmp

if [[ ! -s "$archive" ]]; then
    printf 'Missing or empty package: %s\n' "$archive" >&2
    exit 1
fi

archive_entries="$(unzip -Z1 "$archive")"
for bundle in THUMPER.component THUMPER.vst3 THUMPER.app; do
    if ! grep -Fq "$bundle/Contents/" <<< "$archive_entries"; then
        printf 'Package does not contain expected bundle: %s\n' "$bundle" >&2
        exit 1
    fi
done

printf 'Verified Universal package: %s\n' "$archive"
