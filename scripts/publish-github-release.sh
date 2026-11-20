#!/usr/bin/env bash
set -euo pipefail

tag="${1:?Usage: publish-github-release.sh <tag> <THUMPER-macOS.zip>}"
archive="${2:?Usage: publish-github-release.sh <tag> <THUMPER-macOS.zip>}"
repository="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY must identify the target repository}"

if [[ ! "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    printf 'Refusing to publish unexpected release tag: %s\n' "$tag" >&2
    exit 1
fi

if [[ ! -s "$archive" ]]; then
    printf 'Missing or empty release archive: %s\n' "$archive" >&2
    exit 1
fi

if gh release view "$tag" --repo "$repository" >/dev/null 2>&1; then
    gh release upload "$tag" "$archive" --repo "$repository" --clobber
else
    gh release create "$tag" "$archive" --repo "$repository" --title "$tag" --generate-notes
fi

gh release view "$tag" --repo "$repository" --json assets --jq '.assets[].name' \
    | grep -Fxq "THUMPER-macOS.zip"

printf 'Verified THUMPER-macOS.zip on release %s\n' "$tag"
