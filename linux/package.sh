#!/usr/bin/env bash
# Produces one installable package file for Arch / CachyOS - the Linux counterpart of
# build_setup.ps1. Hand the resulting .pkg.tar.zst to anyone on an Arch-based distro:
#
#   sudo pacman -U wallpaperanim-<version>-x86_64.pkg.tar.zst
#
# pacman pulls in Qt 6, mpv and the rest by itself, and the app appears in the
# application menu the way the Windows installer creates its Start-menu entry. There is
# nothing to extract and no PATH to set up.
#
#   ./package.sh          build the package into linux/dist
#   ./package.sh install  build it, then install it on this machine
#   ./package.sh clean    remove linux/dist

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
pkg_dir="${script_dir}/packaging"
dist_dir="${script_dir}/dist"
action="${1:-build}"

case "${action}" in
clean)
    rm -rf "${dist_dir}"
    echo "Removed ${dist_dir}"
    exit 0
    ;;
build | install) ;;
*)
    echo "Usage: $0 [build|install|clean]" >&2
    exit 2
    ;;
esac

if ! command -v makepkg >/dev/null 2>&1; then
    echo "makepkg not found - this packages for Arch-based distros only." >&2
    echo "On anything else, build from source with ./build.sh instead." >&2
    exit 1
fi

mkdir -p "${dist_dir}"

# Keep makepkg's scratch trees out of the source checkout, and drop the finished package
# straight into dist/. --cleanbuild forces a fresh configure so a stale CMake cache from
# an earlier run can never end up inside a package meant for someone else's machine.
cd "${pkg_dir}"
PKGDEST="${dist_dir}" BUILDDIR="${dist_dir}/.build" makepkg --force --cleanbuild
rm -rf "${dist_dir}/.build"
# Checksums and the unversioned release copy belong to the previous build; release.sh
# regenerates both after calling this script, so stale ones can only mislead.
rm -f "${dist_dir}"/*.sha256 "${dist_dir}/wallpaperanim-x86_64.pkg.tar.zst"

# The version-<rel> shape excludes the unversioned copy release.sh leaves here for the
# stable "latest" download URL.
package_file="$(ls -t "${dist_dir}"/wallpaperanim-*-*-x86_64.pkg.tar.zst 2>/dev/null | head -n1 || true)"
if [[ -z "${package_file}" ]]; then
    echo "makepkg reported success but produced no package file." >&2
    exit 1
fi

# The stamped diagnostic collector also goes out on its own: someone whose install will
# not start still needs to run it, and pacman may have refused the install entirely.
if bsdtar -xOf "${package_file}" usr/bin/wallpaperanim-diagnose >"${dist_dir}/diagnose.sh" 2>/dev/null &&
    [[ -s "${dist_dir}/diagnose.sh" ]]; then
    chmod +x "${dist_dir}/diagnose.sh"
else
    rm -f "${dist_dir}/diagnose.sh"
    echo "Warning: could not extract the diagnostic collector from the package." >&2
fi

echo
echo "Package: ${package_file}"
echo "Install: sudo pacman -U '${package_file}'"
if [[ -f "${dist_dir}/diagnose.sh" ]]; then
    echo
    echo "If it does not start on their machine, have them run:"
    echo "  ./diagnose.sh            (${dist_dir}/diagnose.sh, or wallpaperanim-diagnose once installed)"
    echo "and send back the single report file it writes."
fi

if [[ "${action}" == "install" ]]; then
    sudo pacman -U --noconfirm "${package_file}"
fi
