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

package_file="$(ls -t "${dist_dir}"/*.pkg.tar.zst 2>/dev/null | head -n1 || true)"
if [[ -z "${package_file}" ]]; then
    echo "makepkg reported success but produced no package file." >&2
    exit 1
fi

echo
echo "Package: ${package_file}"
echo "Install: sudo pacman -U '${package_file}'"

if [[ "${action}" == "install" ]]; then
    sudo pacman -U --noconfirm "${package_file}"
fi
