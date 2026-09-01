#!/usr/bin/env bash
# Linux build helper - the counterpart of build.bat.
#
#   ./build.sh              configure + build into linux/build
#   ./build.sh install      build, then install into /usr/local (needs sudo)
#   ./build.sh user-install build, then install into ~/.local (no sudo)
#   ./build.sh clean        remove the build directory

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${script_dir}/build"
action="${1:-build}"

require() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing dependency: $1" >&2
        exit 1
    fi
}

case "${action}" in
clean)
    rm -rf "${build_dir}"
    echo "Removed ${build_dir}"
    exit 0
    ;;
build | install | user-install) ;;
*)
    echo "Usage: $0 [build|install|user-install|clean]" >&2
    exit 2
    ;;
esac

require cmake
require pkg-config

if ! pkg-config --exists mpv; then
    echo "Missing dependency: libmpv (Arch: sudo pacman -S mpv)" >&2
    exit 1
fi

prefix="/usr/local"
if [[ "${action}" == "user-install" ]]; then
    prefix="${HOME}/.local"
fi

generator=()
if command -v ninja >/dev/null 2>&1; then
    generator=(-G Ninja)
fi

cmake -B "${build_dir}" -S "${script_dir}" "${generator[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${prefix}"
cmake --build "${build_dir}" --parallel

echo
echo "Built: ${build_dir}/wallpaperanim"

case "${action}" in
install)
    sudo cmake --install "${build_dir}"
    ;;
user-install)
    cmake --install "${build_dir}"
    echo "Installed into ${prefix}. Make sure ${prefix}/bin is on your PATH."
    ;;
esac
