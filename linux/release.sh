#!/usr/bin/env bash
# Publishes the Linux package to the GitHub release for the version in src/version.h -
# the Linux counterpart of release.ps1, which publishes the Windows assets to the same
# release. Run it on an Arch-based machine after the version has been bumped.
#
#   ./release.sh              build and upload to v<version>
#   ./release.sh --create     also create the release if it does not exist yet
#   ./release.sh --dry-run    build and checksum, but upload nothing
#
# Two copies of the package go up: the versioned file people download by hand, and an
# unversioned "wallpaperanim-x86_64.pkg.tar.zst" so that
# https://github.com/<repo>/releases/latest/download/<that name> is a stable URL the
# installer can fall back to without calling the GitHub API.

set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
dist_dir="${script_dir}/dist"
stable_name="wallpaperanim-x86_64.pkg.tar.zst"

CREATE=0
DRY_RUN=0
while [[ $# -gt 0 ]]; do
    case "$1" in
    --create) CREATE=1 ;;
    --dry-run) DRY_RUN=1 ;;
    -h | --help)
        sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "Unknown option: $1 (try --help)" >&2
        exit 2
        ;;
    esac
    shift
done

die() {
    echo "$*" >&2
    exit 1
}

command -v gh >/dev/null 2>&1 || die "gh (GitHub CLI) is required: sudo pacman -S github-cli"
gh auth status >/dev/null 2>&1 || die "gh is not authenticated. Run: gh auth login"

# The version lives in src/version.h and nowhere else; PKGBUILD reads the same file.
version="$(sed -n 's/^#define APP_VERSION_STRING "\([^"]*\)".*/\1/p' "${repo_root}/src/version.h")"
[[ -n "${version}" ]] || die "Could not read APP_VERSION_STRING from ${repo_root}/src/version.h"
tag="v${version}"

echo "Version ${version} -> release ${tag}"
echo

"${script_dir}/package.sh" || die "Packaging failed."

pkg="$(ls -t "${dist_dir}"/wallpaperanim-"${version}"-*-x86_64.pkg.tar.zst 2>/dev/null | head -1)"
[[ -n "${pkg}" ]] || die "No package for ${version} in ${dist_dir} - did the version bump reach PKGBUILD's source?"

# Checksums are what the installer verifies the download against.
(cd "$(dirname "${pkg}")" && sha256sum "$(basename "${pkg}")" >"$(basename "${pkg}").sha256")
cp -f "${pkg}" "${dist_dir}/${stable_name}"
(cd "${dist_dir}" && sha256sum "${stable_name}" >"${stable_name}.sha256")

assets=(
    "${pkg}"
    "${pkg}.sha256"
    "${dist_dir}/${stable_name}"
    "${dist_dir}/${stable_name}.sha256"
)

echo
echo "Assets:"
for a in "${assets[@]}"; do printf '  %s (%s)\n' "$(basename "${a}")" "$(du -h "${a}" | cut -f1)"; done

if [[ ${DRY_RUN} -eq 1 ]]; then
    echo
    echo "--dry-run: nothing uploaded."
    exit 0
fi

if ! gh release view "${tag}" >/dev/null 2>&1; then
    if [[ ${CREATE} -eq 0 ]]; then
        die "
Release ${tag} does not exist yet.
The Windows side (release.ps1) normally creates it. To create it from here - which tags
the current commit, so be on the right branch - re-run with --create."
    fi
    echo
    echo "Creating release ${tag} ..."
    gh release create "${tag}" -t "Release ${tag}" \
        -n "WallpaperAnim ${tag}.

**Linux (Arch / CachyOS):**
\`\`\`sh
curl -fsSL https://raw.githubusercontent.com/hamer1818/wallpaper-anim/master/linux/install.sh | bash
\`\`\`
Installs over any previous version and restarts the app." ||
        die "Could not create the release."
fi

echo
echo "Uploading to ${tag} ..."
gh release upload "${tag}" "${assets[@]}" --clobber || die "Upload failed."

echo
echo "Done: https://github.com/hamer1818/wallpaper-anim/releases/tag/${tag}"
echo
echo "Recipients install or update with:"
echo "  curl -fsSL https://raw.githubusercontent.com/hamer1818/wallpaper-anim/master/linux/install.sh | bash"
