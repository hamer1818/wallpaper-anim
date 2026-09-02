#!/usr/bin/env bash
# WallpaperAnim installer/updater for Arch-based distros (CachyOS, EndeavourOS, Arch).
# Downloads the newest package from GitHub Releases and installs it over whatever is
# already there - the Linux counterpart of the Windows setup .exe.
#
#   curl -fsSL https://raw.githubusercontent.com/hamer1818/wallpaper-anim/master/linux/install.sh | bash
#
# Options (after `| bash -s --`, or when running the file directly):
#   --tag v1.6.0     install a specific release instead of the latest
#   --file PATH      install a local .pkg.tar.zst instead of downloading
#   --force          install even when this system is older than the package needs
#   --no-restart     do not relaunch the app afterwards
#   --dry-run        do everything except the actual pacman call
#
# It never installs silently over a system that cannot run the result: the package
# carries the library versions it was compiled against, and those are checked here,
# before pacman touches anything.

set -uo pipefail

REPO="hamer1818/wallpaper-anim"
ASSET_PATTERN='x86_64\.pkg\.tar\.zst'
STABLE_ASSET="wallpaperanim-x86_64.pkg.tar.zst"

TAG=""
LOCAL_FILE=""
FORCE=0
RESTART=1
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
    --tag)
        TAG="${2:-}"
        shift
        ;;
    --file)
        LOCAL_FILE="${2:-}"
        shift
        ;;
    --repo)
        REPO="${2:-}"
        shift
        ;;
    --force) FORCE=1 ;;
    --no-restart) RESTART=0 ;;
    --dry-run) DRY_RUN=1 ;;
    -h | --help)
        sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "Unknown option: $1 (try --help)" >&2
        exit 2
        ;;
    esac
    shift
done

RED=$'\033[31m'
YEL=$'\033[33m'
GRN=$'\033[32m'
DIM=$'\033[2m'
OFF=$'\033[0m'
[[ -t 1 ]] || { RED=""; YEL=""; GRN=""; DIM=""; OFF=""; }

info() { printf '%s\n' "$*"; }
warn() { printf '%s%s%s\n' "${YEL}" "$*" "${OFF}" >&2; }
die() {
    printf '%s%s%s\n' "${RED}" "$*" "${OFF}" >&2
    exit 1
}
have() { command -v "$1" >/dev/null 2>&1; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/wpa-install.XXXXXX")" || die "Cannot create a temp directory."
trap 'rm -rf "${TMP}"' EXIT

# --- preconditions ----------------------------------------------------------------
have pacman || die "This installer is for Arch-based distros only.
Build from source instead: https://github.com/${REPO}/tree/master/linux#build"
[[ "$(uname -m)" == "x86_64" ]] || die "Only x86_64 packages are published (this is $(uname -m))."
have curl || die "curl is required. Install it with: sudo pacman -S curl"

# Everything except the pacman call runs unprivileged.
if [[ "$(id -u)" -eq 0 ]]; then
    SUDO=()
else
    have sudo || die "sudo is required (or run this as root)."
    SUDO=(sudo)
fi

# libarchive ships with pacman, so one of these always exists; the tar fallback is for
# the odd container image without bsdtar.
extract_from_pkg() { # <package file> <path inside>
    if have bsdtar; then
        bsdtar -xOf "$1" "$2" 2>/dev/null
    else
        tar -I zstd -xOf "$1" "$2" 2>/dev/null
    fi
}

INSTALLED_VERSION="$(pacman -Q wallpaperanim 2>/dev/null | awk '{print $2}')"

# --- find the package -------------------------------------------------------------
PKG=""
if [[ -n "${LOCAL_FILE}" ]]; then
    [[ -f "${LOCAL_FILE}" ]] || die "No such file: ${LOCAL_FILE}"
    PKG="${LOCAL_FILE}"
    info "Using local package: ${PKG}"
else
    api="https://api.github.com/repos/${REPO}/releases/latest"
    [[ -n "${TAG}" ]] && api="https://api.github.com/repos/${REPO}/releases/tags/${TAG}"

    info "Looking up the ${TAG:-latest} release of ${REPO} ..."
    release_json="${TMP}/release.json"
    url=""
    if curl -fsSL --max-time 30 "${api}" -o "${release_json}"; then
        TAG="${TAG:-$(grep -m1 '"tag_name"' "${release_json}" | cut -d'"' -f4)}"
        url="$(grep -oE '"browser_download_url":[[:space:]]*"[^"]*'"${ASSET_PATTERN}"'"' "${release_json}" |
            cut -d'"' -f4 | grep -v "${STABLE_ASSET}\$" | head -1)"
        # The unversioned copy is the fallback within the release itself.
        [[ -z "${url}" ]] && url="$(grep -oE '"browser_download_url":[[:space:]]*"[^"]*'"${ASSET_PATTERN}"'"' \
            "${release_json}" | cut -d'"' -f4 | head -1)"
    else
        # Unauthenticated API calls are rate limited to 60/hour per IP; the redirecting
        # download URL is not, so a throttled user can still install.
        warn "GitHub API unreachable or rate limited; falling back to the direct download URL."
        url="https://github.com/${REPO}/releases/latest/download/${STABLE_ASSET}"
    fi
    [[ -n "${url}" ]] || die "The ${TAG:-latest} release has no Linux package (expected an *-x86_64.pkg.tar.zst asset).
Releases: https://github.com/${REPO}/releases"

    PKG="${TMP}/$(basename "${url}")"
    info "Downloading ${DIM}${url}${OFF}"
    curl -fL --retry 3 --retry-delay 2 --max-time 300 --progress-bar "${url}" -o "${PKG}" ||
        die "Download failed: ${url}"

    # Verify against the checksum published next to the asset when there is one.
    if curl -fsSL --max-time 30 "${url}.sha256" -o "${TMP}/sum" 2>/dev/null; then
        want="$(awk '{print $1}' "${TMP}/sum")"
        got="$(sha256sum "${PKG}" | awk '{print $1}')"
        if [[ "${want}" != "${got}" ]]; then
            die "Checksum mismatch - refusing to install.
  expected ${want}
  got      ${got}"
        fi
        info "${GRN}Checksum verified.${OFF}"
    else
        warn "No published checksum for this asset; relying on HTTPS alone."
    fi
fi

NEW_VERSION="$(extract_from_pkg "${PKG}" .PKGINFO | sed -n 's/^pkgver = //p' | head -1)"
[[ -n "${NEW_VERSION}" ]] || die "${PKG} is not a readable pacman package."

info ""
info "Installed : ${INSTALLED_VERSION:-<none>}"
info "Package   : ${NEW_VERSION}"

# --- will this build actually run here? -------------------------------------------
# The package embeds the exact library versions it was compiled against (see
# linux/packaging/PKGBUILD). Arch promises no ABI stability across versions and the
# package's own depends carry no bounds, so a system that is behind installs it happily
# and then fails to start. Catch that here rather than in a bug report.
baseline="$(extract_from_pkg "${PKG}" usr/bin/wallpaperanim-diagnose |
    sed -n 's/^BUILD_BASELINE="\(.*\)"$/\1/p' | head -1)"
outdated=()
if [[ -n "${baseline}" && "${baseline}" != *"@"* ]] && have vercmp; then
    for pair in ${baseline}; do
        pkg_name="${pair%%=*}"
        want_ver="${pair#*=}"
        got_ver="$(pacman -Q "${pkg_name}" 2>/dev/null | awk '{print $2}')"
        [[ -z "${got_ver}" ]] && continue # pacman will pull it in as a dependency
        [[ "$(vercmp "${got_ver}" "${want_ver}")" == -* ]] &&
            outdated+=("  ${pkg_name}: you have ${got_ver}, the package was built against ${want_ver}")
    done
fi

if [[ ${#outdated[@]} -gt 0 ]]; then
    warn ""
    warn "This system is older than the machine that built the package:"
    for line in "${outdated[@]}"; do warn "${line}"; done
    warn ""
    if [[ ${FORCE} -eq 1 ]]; then
        warn "Continuing anyway (--force). If the app does not start, run: wallpaperanim-diagnose"
    else
        die "The app would very likely install fine and then refuse to start.
Update the system first, then run this installer again:

  sudo pacman -Syu

To install regardless, re-run with --force."
    fi
fi

# --- stop, install, restart -------------------------------------------------------
WAS_RUNNING=0
if pgrep -x wallpaperanim >/dev/null 2>&1; then
    WAS_RUNNING=1
    info "Stopping the running instance ..."
    wallpaperanim --quit >/dev/null 2>&1
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        pgrep -x wallpaperanim >/dev/null 2>&1 || break
        sleep 0.3
    done
    pgrep -x wallpaperanim >/dev/null 2>&1 && pkill -x wallpaperanim
fi

if [[ ${DRY_RUN} -eq 1 ]]; then
    info ""
    info "${YEL}--dry-run: stopping before${OFF} ${SUDO[*]:-} pacman -U --noconfirm ${PKG}"
    exit 0
fi

info ""
info "Installing ${NEW_VERSION} ..."
if ! "${SUDO[@]}" pacman -U --noconfirm "${PKG}"; then
    die "pacman refused the package.
If it complains about dependencies, run 'sudo pacman -Syu' and try again."
fi

if [[ ${RESTART} -eq 1 && ${WAS_RUNNING} -eq 1 ]] && have wallpaperanim; then
    info "Restarting WallpaperAnim ..."
    (setsid wallpaperanim --background >/dev/null 2>&1 &)
fi

info ""
info "${GRN}WallpaperAnim ${NEW_VERSION} installed.${OFF}"
if [[ ${WAS_RUNNING} -eq 0 ]]; then
    info "Launch it from the application menu, or run: wallpaperanim"
fi
info "${DIM}If anything misbehaves: wallpaperanim-diagnose  (writes one report file to send back)${OFF}"
