#!/usr/bin/env bash
# WallpaperAnim Linux diagnostic collector.
#
# Run this on the machine where the app misbehaves, then send back the single report
# file it writes. Everything here is read-only except that file and a scratch dir: it
# never touches the config, the wallpaper, the package database or the running session.
#
#   ./diagnose.sh                collect everything, including a real launch test
#   ./diagnose.sh --no-launch    collect everything except starting the app
#   ./diagnose.sh --binary PATH  diagnose a build-tree binary instead of the installed one
#   ./diagnose.sh --redact       replace the user name and $HOME in the finished report
#   ./diagnose.sh --out FILE     write the report somewhere specific
#
# Deliberately no `set -e`: a missing optional tool must degrade one section, never
# abort the report. The report is the product; a half-collected one is worthless.

set -uo pipefail

SCRIPT_VERSION="1"
# Stamped by the packaging step (linux/PKGBUILD, via linux/package.sh) with the exact
# library versions the shipped binary was compiled against. A plain checkout leaves the
# placeholder untouched and the comparison below is skipped - so this can never go stale.
BUILD_BASELINE="@WPA_BUILD_BASELINE@"
BUILD_HOST_INFO="@WPA_BUILD_HOST_INFO@"
DO_LAUNCH=1
REDACT=0
BIN=""
OUT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --no-launch) DO_LAUNCH=0 ;;
    --redact) REDACT=1 ;;
    --binary)
        BIN="${2:-}"
        shift
        ;;
    --out)
        OUT="${2:-}"
        shift
        ;;
    -h | --help)
        sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "Unknown option: $1 (try --help)" >&2
        exit 2
        ;;
    esac
    shift
done

TMP="$(mktemp -d "${TMPDIR:-/tmp}/wpa-diag.XXXXXX")"
BODY="${TMP}/body.txt"
: >"${BODY}"
trap 'rm -rf "${TMP}"' EXIT

if [[ -z "${OUT}" ]]; then
    OUT="$(pwd)/wallpaperanim-diagnostic-$(hostname 2>/dev/null || echo host)-$(date +%Y%m%d-%H%M%S).txt"
    # Fall back to the temp dir when the working directory is not writable (a report
    # that cannot be written is the one failure mode this script must not have).
    if ! : >"${OUT}" 2>/dev/null; then
        OUT="${TMPDIR:-/tmp}/wallpaperanim-diagnostic-$(date +%Y%m%d-%H%M%S).txt"
    fi
fi

FINDINGS=()

have() { command -v "$1" >/dev/null 2>&1; }
say() { printf '%s\n' "$*" >>"${BODY}"; }
finding() { FINDINGS+=("$1"); }

section() {
    say ""
    say "==============================================================================="
    say "$*"
    say "==============================================================================="
}

# Runs a shell snippet and records both the command and everything it printed, so the
# report shows what was asked as well as what came back.
runsh() {
    local label="$1"
    shift
    say ""
    say "--- ${label}"
    say "\$ $*"
    local rc=0
    bash -c "$*" >>"${BODY}" 2>&1 || rc=$?
    [[ ${rc} -ne 0 ]] && say "[exit ${rc}]"
    return 0
}

note() {
    say ""
    say "[note] $*"
}

# ---------------------------------------------------------------------------------
# 1. Report meta
# ---------------------------------------------------------------------------------
section "1. REPORT META"
say ""
say "script version : ${SCRIPT_VERSION}"
say "collected at   : $(date -Is 2>/dev/null || date)"
say "collected by   : ${USER:-?} (uid $(id -u 2>/dev/null || echo ?))"
say "hostname       : $(hostname 2>/dev/null || echo ?)"
say "cwd            : $(pwd)"
say "launch test    : $([[ ${DO_LAUNCH} -eq 1 ]] && echo yes || echo "no (--no-launch)")"

# ---------------------------------------------------------------------------------
# 2. Distro, kernel, how current the system is
# ---------------------------------------------------------------------------------
section "2. SYSTEM"
runsh "os-release" "cat /etc/os-release 2>/dev/null | grep -vE '^(HOME_URL|SUPPORT|BUG_REPORT|PRIVACY|DOCUMENTATION)'"
runsh "kernel" "uname -a"
runsh "uptime" "uptime"
runsh "glibc" "ldd --version | head -1"
# An Arch-family box that has not been fully upgraded in months is the single most
# common reason a binary built elsewhere refuses to start.
runsh "last full system upgrades" "grep -a 'starting full system upgrade' /var/log/pacman.log 2>/dev/null | tail -5"
runsh "recent upgrades of the libraries we link" \
    "grep -aE ' upgraded (qt6-base|qt6-declarative|qt6-multimedia|mpv|layer-shell-qt|ffmpeg|glibc|gcc-libs|mesa) ' /var/log/pacman.log 2>/dev/null | tail -15"

LAST_UPGRADE="$(grep -a 'starting full system upgrade' /var/log/pacman.log 2>/dev/null | tail -1 | cut -c2-11)"
if [[ -n "${LAST_UPGRADE}" ]]; then
    last_epoch="$(date -d "${LAST_UPGRADE}" +%s 2>/dev/null || echo 0)"
    now_epoch="$(date +%s)"
    if [[ "${last_epoch}" -gt 0 ]]; then
        days=$(((now_epoch - last_epoch) / 86400))
        say ""
        say "[days since last full system upgrade] ${days}"
        [[ ${days} -gt 30 ]] && finding "Sistem ${days} gündür tam güncellenmemiş (son: ${LAST_UPGRADE}). Rolling dağıtımda paketi kuran makine geride kalmış olabilir."
    fi
fi

# ---------------------------------------------------------------------------------
# 3. Session and environment
# ---------------------------------------------------------------------------------
section "3. SESSION & ENVIRONMENT"
say ""
say "XDG_SESSION_TYPE   = ${XDG_SESSION_TYPE:-<unset>}"
say "XDG_CURRENT_DESKTOP= ${XDG_CURRENT_DESKTOP:-<unset>}"
say "DESKTOP_SESSION    = ${DESKTOP_SESSION:-<unset>}"
say "WAYLAND_DISPLAY    = ${WAYLAND_DISPLAY:-<unset>}"
say "DISPLAY            = ${DISPLAY:-<unset>}"
say "XDG_RUNTIME_DIR    = ${XDG_RUNTIME_DIR:-<unset>}"
say "QT_QPA_PLATFORM    = ${QT_QPA_PLATFORM:-<unset>}"
say "LANG               = ${LANG:-<unset>}"
runsh "graphics / Qt / toolkit environment" "env | grep -E '^(QT_|QML|KDE_|GDK_|GTK_|MESA|LIBGL|GALLIUM|__GL|__NV|VK_|LD_PRELOAD|LD_LIBRARY_PATH)' | sort"
runsh "session manager view" "loginctl show-session \$(loginctl show-user \${USER} -p Display --value 2>/dev/null) -p Type -p Class -p Active -p Remote 2>/dev/null"

if [[ -n "${QT_QPA_PLATFORM:-}" ]]; then
    finding "QT_QPA_PLATFORM ortamda '${QT_QPA_PLATFORM}' olarak zorlanmış - yanlış platform eklentisi uygulamayı pencere açmadan öldürebilir."
fi

# ---------------------------------------------------------------------------------
# 4. Graphics stack (the layer-shell backend needs a real GL 3.3 core context)
# ---------------------------------------------------------------------------------
section "4. GRAPHICS"
runsh "GPUs" "lspci -nn 2>/dev/null | grep -iE 'vga|3d|display'"
runsh "loaded graphics kernel modules" "lsmod 2>/dev/null | grep -iE '^(nvidia|amdgpu|i915|xe|radeon|nouveau)' | awk '{print \$1, \$2}'"
runsh "mesa / driver packages" "pacman -Q mesa vulkan-icd-loader nvidia-utils nvidia-dkms libva-mesa-driver 2>&1 | head -10"
runsh "GLX" "glxinfo -B 2>/dev/null | head -20"
runsh "EGL" "eglinfo -B 2>/dev/null | head -25"
runsh "Wayland globals (layer-shell support)" "wayland-info 2>/dev/null | grep -iE 'zwlr_layer_shell|wl_compositor|xdg_wm_base|wp_viewporter' | head"

if have wayland-info && [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
    if ! wayland-info 2>/dev/null | grep -q zwlr_layer_shell; then
        finding "Bileşik yönetici zwlr_layer_shell_v1 sunmuyor - layer-shell arka planı bu oturumda hiçbir yüzey oluşturamaz."
    fi
fi

# ---------------------------------------------------------------------------------
# 5. The installed package and the binary itself
# ---------------------------------------------------------------------------------
section "5. WALLPAPERANIM INSTALLATION"
runsh "on PATH" "command -v -- wallpaperanim || echo '(wallpaperanim is not on PATH)'"
runsh "all copies on disk" "ls -l /usr/bin/wallpaperanim /usr/local/bin/wallpaperanim \"\${HOME}/.local/bin/wallpaperanim\" 2>&1"

if [[ -z "${BIN}" ]]; then
    BIN="$(command -v wallpaperanim 2>/dev/null || true)"
    for candidate in /usr/bin/wallpaperanim /usr/local/bin/wallpaperanim "${HOME}/.local/bin/wallpaperanim"; do
        [[ -n "${BIN}" ]] && break
        [[ -x "${candidate}" ]] && BIN="${candidate}"
    done
fi

say ""
say "[binary under test] ${BIN:-<none found>}"

if [[ -z "${BIN}" || ! -x "${BIN}" ]]; then
    finding "wallpaperanim çalıştırılabiliri bulunamadı - paket kurulu değil ya da PATH dışında."
else
    runsh "file" "ls -l '${BIN}'; file '${BIN}' 2>/dev/null; sha256sum '${BIN}'"
fi

runsh "pacman package info" "pacman -Qi wallpaperanim 2>&1 | head -25"
# -Qkk compares every installed file against the package database: a truncated or
# hand-edited install shows up here and nowhere else.
runsh "package file integrity (pacman -Qkk)" "pacman -Qkk wallpaperanim 2>&1 | tail -20"
runsh "package file list" "pacman -Qlq wallpaperanim 2>/dev/null | head -30"
runsh "declared dependencies, as installed" \
    "pacman -Q qt6-base qt6-declarative qt6-multimedia mpv layer-shell-qt ffmpeg plasma-workspace kwin glibc gcc-libs yt-dlp 2>&1"

if have pacman && pacman -Qkk wallpaperanim >/dev/null 2>&1; then
    :
elif have pacman && pacman -Q wallpaperanim >/dev/null 2>&1; then
    finding "pacman -Qkk kurulu dosyalarda bozulma/eksik bildiriyor (5. bölüme bak)."
fi

# ---------------------------------------------------------------------------------
# 6. Dynamic linking - the decisive section for "it does not start at all"
# ---------------------------------------------------------------------------------
section "6. DYNAMIC LINKING"

# The decisive comparison for "works on his machine, not on mine": Arch-family distros
# make no ABI promise across versions, and the package declares its dependencies without
# version bounds, so pacman happily installs onto a system that is too old to run it.
if [[ "${BUILD_BASELINE}" == *"@"* ]]; then
    note "This copy of the script carries no build baseline (run from a checkout, not from a package)."
else
    say ""
    say "--- built against vs. installed here"
    say "    ${BUILD_HOST_INFO}"
    {
        printf '%-20s %-30s %-30s %s\n' "PACKAGE" "BUILT AGAINST" "INSTALLED HERE" "VERDICT"
        for pair in ${BUILD_BASELINE}; do
            pkg="${pair%%=*}"
            want="${pair#*=}"
            got="$(pacman -Q "${pkg}" 2>/dev/null | awk '{print $2}')"
            verdict="ok"
            if [[ -z "${got}" ]]; then
                verdict="NOT INSTALLED"
            elif have vercmp; then
                case "$(vercmp "${got}" "${want}")" in
                -*) verdict="OLDER - can break the binary" ;;
                0) verdict="identical" ;;
                *) verdict="newer" ;;
                esac
            fi
            printf '%-20s %-30s %-30s %s\n' "${pkg}" "${want}" "${got:-<none>}" "${verdict}"
        done
    } >>"${BODY}"

    for pair in ${BUILD_BASELINE}; do
        pkg="${pair%%=*}"
        want="${pair#*=}"
        got="$(pacman -Q "${pkg}" 2>/dev/null | awk '{print $2}')"
        if [[ -z "${got}" ]]; then
            finding "${pkg} kurulu değil, ama paket ona karşı derlendi (${want})."
        elif have vercmp && [[ "$(vercmp "${got}" "${want}")" == -* ]]; then
            finding "${pkg} burada ${got}, paket ise ${want} ile derlendi. Eski sürüm ABI'yi karşılamayabilir - 'sudo pacman -Syu' ilk denenecek şey."
        fi
    done
fi

if [[ -n "${BIN}" && -x "${BIN}" ]]; then
    runsh "direct dependencies (NEEDED)" "readelf -d '${BIN}' 2>/dev/null | grep NEEDED"
    runsh "full resolution (ldd)" "ldd '${BIN}' 2>&1"

    ldd "${BIN}" 2>&1 | grep -F "not found" >"${TMP}/ldd_missing.txt"
    if [[ -s "${TMP}/ldd_missing.txt" ]]; then
        while IFS= read -r line; do
            finding "Kütüphane çözülemiyor: $(echo "${line}" | awk '{print $1}') - bu tek başına uygulamanın hiç başlamamasına yeter."
        done <"${TMP}/ldd_missing.txt"
    fi

    # -r forces relocation processing, so an ABI drift (built against a newer Qt/mpv
    # than this machine has) surfaces here without having to run the program.
    runsh "unresolved symbols (ldd -r)" "ldd -r '${BIN}' 2>&1 | grep -E 'undefined symbol|not found' | head -25"
    ldd -r "${BIN}" 2>&1 | grep -c "undefined symbol" >"${TMP}/undef_count.txt" 2>/dev/null || echo 0 >"${TMP}/undef_count.txt"

    # Which package owns each library we actually loaded: this is what gets compared
    # against the build machine's versions.
    say ""
    say "--- owning package of every resolved library"
    ldd "${BIN}" 2>/dev/null | awk '{for (i=1;i<=NF;i++) if ($i ~ /^\//) {print $i; break}}' | sort -u |
        while IFS= read -r lib; do
            owner="$(pacman -Qoq "${lib}" 2>/dev/null | head -1)"
            if [[ -n "${owner}" ]]; then
                ver="$(pacman -Q "${owner}" 2>/dev/null | awk '{print $2}')"
                printf '%-45s %s %s\n' "$(basename "${lib}")" "${owner}" "${ver}"
            else
                printf '%-45s %s\n' "$(basename "${lib}")" "(no owning package)"
            fi
        done >>"${BODY}"

    # glibc and libstdc++ are versioned by symbol, so a binary built on a newer host
    # can be proven incompatible here, before any launch attempt.
    if have objdump; then
        req_glibc="$(objdump -T "${BIN}" 2>/dev/null | grep -o 'GLIBC_2\.[0-9.]*' | sort -uV | tail -1)"
        req_glibcxx="$(objdump -T "${BIN}" 2>/dev/null | grep -o 'GLIBCXX_3\.4[0-9.]*' | sort -uV | tail -1)"
        sys_glibc="GLIBC_$(ldd --version 2>/dev/null | head -1 | grep -o '[0-9]\+\.[0-9]\+' | head -1)"
        libstdcxx="$(ldd "${BIN}" 2>/dev/null | awk '/libstdc\+\+/ {print $3}')"
        sys_glibcxx="$(strings -a "${libstdcxx:-/usr/lib/libstdc++.so.6}" 2>/dev/null | grep -o 'GLIBCXX_3\.4[0-9.]*' | sort -uV | tail -1)"
        say ""
        say "--- symbol-version floor"
        say "binary requires ${req_glibc:-?} / ${req_glibcxx:-?}"
        say "system provides ${sys_glibc:-?} / ${sys_glibcxx:-?}"
        if [[ -n "${req_glibc}" && -n "${sys_glibc}" ]] &&
            [[ "$(printf '%s\n%s\n' "${req_glibc}" "${sys_glibc}" | sort -V | tail -1)" == "${req_glibc}" ]] &&
            [[ "${req_glibc}" != "${sys_glibc}" ]]; then
            finding "Binary ${req_glibc} istiyor, sistemde ${sys_glibc} var - daha yeni bir glibc üzerinde derlenmiş, bu makinede asla çalışmaz."
        fi
        if [[ -n "${req_glibcxx}" && -n "${sys_glibcxx}" ]] &&
            [[ "$(printf '%s\n%s\n' "${req_glibcxx}" "${sys_glibcxx}" | sort -V | tail -1)" == "${req_glibcxx}" ]] &&
            [[ "${req_glibcxx}" != "${sys_glibcxx}" ]]; then
            finding "Binary ${req_glibcxx} istiyor, sistemin libstdc++'ı ${sys_glibcxx} veriyor - gcc-libs geride."
        fi
    fi

    # The one soname that changed within living memory for this project.
    if ! ldd "${BIN}" 2>/dev/null | grep -q 'libmpv.so.2 => /'; then
        runsh "libmpv on this system" "ls -l /usr/lib/libmpv.so* 2>&1"
        finding "libmpv.so.2 çözülemedi. Sistemdeki mpv sürümü farklı bir soname taşıyor olabilir (6. bölümdeki listeye bak)."
    fi
else
    note "No binary to link-check."
fi

# ---------------------------------------------------------------------------------
# 7. Is it already running? (a second launch forwards to the first and exits silently)
# ---------------------------------------------------------------------------------
section "7. RUNNING STATE"
runsh "processes" "pgrep -a wallpaperanim || echo '(no wallpaperanim process)'"
runsh "single-instance socket" "ls -l \"\${XDG_RUNTIME_DIR:-/run/user/\$(id -u)}\"/wallpaperanim-* /tmp/wallpaperanim-* 2>&1 | head"
runsh "desktop shell / compositor processes" \
    "ps -eo pid,comm,args --no-headers 2>/dev/null | grep -iE 'plasmashell|kwin_wayland|kwin_x11|gnome-shell|Hyprland|sway|labwc|niri|wayfire' | grep -v grep | cut -c1-140"
runsh "autostart entry" "cat \"\${XDG_CONFIG_HOME:-\${HOME}/.config}/autostart/wallpaperanim.desktop\" 2>&1"
runsh "menu entry" "cat /usr/share/applications/wallpaperanim.desktop 2>&1 | head -12"

ALREADY_RUNNING=0
if pgrep -x wallpaperanim >/dev/null 2>&1; then
    ALREADY_RUNNING=1
    finding "Zaten çalışan bir wallpaperanim süreci var. İkinci çalıştırma komutu ona iletip sessizce çıkar - kullanıcıya 'hiç açılmıyor' gibi görünür."
fi

# ---------------------------------------------------------------------------------
# 8. Launch test - what actually happens when the app is started
# ---------------------------------------------------------------------------------
section "8. LAUNCH TEST"
note "Starting the app is the point of this section. On KDE it can restart plasmashell" \
    "once, if the installed wallpaper plugin differs from the one the binary ships."
VERSION_RC=""
BG_RC=""
if [[ ${DO_LAUNCH} -eq 0 ]]; then
    note "Skipped (--no-launch)."
elif [[ -z "${BIN}" || ! -x "${BIN}" ]]; then
    note "Skipped: no binary."
else
    # --version exercises the whole dynamic link and Qt's initialisation, then exits.
    # If this fails, nothing after it matters.
    say ""
    say "--- '${BIN} --version' (loads every library, initialises Qt, exits)"
    timeout 20 "${BIN}" --version >"${TMP}/version_out.txt" 2>&1
    VERSION_RC=$?
    say "[exit ${VERSION_RC}]"
    cat "${TMP}/version_out.txt" >>"${BODY}"

    if [[ ${VERSION_RC} -eq 124 ]]; then
        # --version prints and exits; it can only time out if startup itself blocks.
        finding "'--version' 20 saniyede dönmedi - süreç başlıyor ama takılıyor (Wayland/X bağlantısı, D-Bus ya da GPU sürücüsü)."
    elif [[ ${VERSION_RC} -ne 0 ]]; then
        first_err="$(grep -m1 -E 'error|Error|symbol|cannot|failed' "${TMP}/version_out.txt" || head -1 "${TMP}/version_out.txt")"
        finding "Uygulama --version ile bile başlamıyor (çıkış ${VERSION_RC}): ${first_err:-<çıktı yok>}"
        if grep -qE 'cannot open shared object|not found' "${TMP}/version_out.txt"; then
            finding "Dinamik yükleyici bir kütüphaneyi bulamıyor - eksik/uyumsuz bağımlılık, kod hatası değil."
        fi
        if grep -qE 'symbol lookup error|undefined symbol|version .GLIBC' "${TMP}/version_out.txt"; then
            finding "Sembol/ABI uyuşmazlığı: binary bu makinedekinden farklı bir kütüphane sürümüne karşı derlenmiş."
        fi
    fi

    # Full start. A private XDG_RUNTIME_DIR (with the Wayland and D-Bus sockets linked
    # in) is used when another instance holds the single-instance socket, so this tests
    # a real startup instead of a forward-and-exit.
    RUNTIME_NOTE="session runtime dir"
    RUN_ENV=()
    if [[ ${ALREADY_RUNNING} -eq 1 ]]; then
        rt="${TMP}/runtime"
        mkdir -p "${rt}" && chmod 700 "${rt}"
        if [[ -n "${WAYLAND_DISPLAY:-}" && "${WAYLAND_DISPLAY}" != /* && -S "${XDG_RUNTIME_DIR:-}/${WAYLAND_DISPLAY}" ]]; then
            ln -sf "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}" "${rt}/${WAYLAND_DISPLAY}"
        fi
        [[ -S "${XDG_RUNTIME_DIR:-}/bus" ]] && ln -sf "${XDG_RUNTIME_DIR}/bus" "${rt}/bus"
        RUN_ENV=(env "XDG_RUNTIME_DIR=${rt}")
        RUNTIME_NOTE="isolated runtime dir (another instance is running)"
    fi

    say ""
    say "--- '${BIN} --background' for 15s, ${RUNTIME_NOTE}"
    say "    exit 124 = still alive after 15s = the app starts fine."
    timeout 15 "${RUN_ENV[@]}" "${BIN}" --background >"${TMP}/bg_out.txt" 2>&1
    BG_RC=$?
    say "[exit ${BG_RC}]"
    head -100 "${TMP}/bg_out.txt" >>"${BODY}"

    case "${BG_RC}" in
    124)
        say ""
        say "[verdict] the process was still running when the timeout killed it."
        ;;
    0)
        if [[ ${ALREADY_RUNNING} -eq 1 ]]; then
            finding "Arka plan testi hemen 0 ile çıktı: muhtemelen çalışan instansa iletti."
        else
            finding "Uygulama hata vermeden ama hemen çıktı (exit 0) - olay döngüsü hiç dönmüyor."
        fi
        ;;
    *)
        finding "Arka plan başlatması ${BG_RC} ile çıktı - 8. bölümdeki çıktıya bak."
        ;;
    esac

    # Qt platform-plugin problems (the classic "could not load the Qt platform plugin
    # wayland") only explain themselves with this variable on, so ask for it just when
    # the plain run failed.
    if [[ ${VERSION_RC} -ne 0 || (${BG_RC} -ne 124 && ${BG_RC} -ne 0) ]]; then
        runsh "Qt plugin loading (QT_DEBUG_PLUGINS=1)" \
            "QT_DEBUG_PLUGINS=1 timeout 20 '${BIN}' --version 2>&1 | grep -iE 'plugin|platform|cannot|failed|error' | head -40"
        runsh "Qt platform plugins present on disk" "ls /usr/lib/qt6/plugins/platforms 2>&1"
    fi
fi

# ---------------------------------------------------------------------------------
# 9. Config and data - absence of config.json proves startup never finished
# ---------------------------------------------------------------------------------
section "9. CONFIG & DATA"
CFG="${XDG_CONFIG_HOME:-${HOME}/.config}/WallpaperAnim/config.json"
DATA="${XDG_DATA_HOME:-${HOME}/.local/share}/WallpaperAnim"
runsh "config dir" "ls -la \"\${XDG_CONFIG_HOME:-\${HOME}/.config}/WallpaperAnim\" 2>&1"
runsh "config.json" "cat '${CFG}' 2>&1"
runsh "data dir" "ls -la '${DATA}' 2>&1 | head -20"
runsh "downloads / thumbnails counts" "ls -1 '${DATA}/downloads' 2>/dev/null | wc -l; ls -1 '${DATA}/thumbnails' 2>/dev/null | wc -l"
runsh "free space on \$HOME" "df -h \"\${HOME}\" | tail -1"

if [[ ! -f "${CFG}" ]]; then
    finding "config.json hiç yazılmamış (${CFG}). Bu dosya App::Start()'ın sonunda yazılıyor, yani başlatma o noktaya hiç ulaşmamış."
else
    # The wallpaper the app would try to load on startup - a missing or undecodable
    # file is worth ruling out before anything else in the media path.
    MEDIA="$(grep -o '"lastVideoPath"[[:space:]]*:[[:space:]]*"[^"]*"' "${CFG}" 2>/dev/null | sed 's/.*:[[:space:]]*"//; s/"$//')"
    say ""
    say "[lastVideoPath] ${MEDIA:-<empty>}"
    if [[ -n "${MEDIA}" ]]; then
        if [[ -f "${MEDIA}" ]]; then
            runsh "media file" "ls -l '${MEDIA}'"
            runsh "media probe" "ffprobe -v error -show_entries format=format_name,duration,size -show_entries stream=codec_name,width,height -of default=noprint_wrappers=1 '${MEDIA}' 2>&1 | head -20"
        else
            finding "Yapılandırmadaki duvar kağıdı dosyası yok: ${MEDIA}"
        fi
    fi
fi

# ---------------------------------------------------------------------------------
# 10. Playback stack
# ---------------------------------------------------------------------------------
section "10. MPV / FFMPEG / YT-DLP"
runsh "mpv" "mpv --version 2>&1 | head -3"
runsh "libmpv files" "ls -l /usr/lib/libmpv.so* 2>&1"
runsh "ffmpeg" "ffmpeg -version 2>&1 | head -2"
runsh "yt-dlp" "yt-dlp --version 2>&1 | head -2"

# ---------------------------------------------------------------------------------
# 11. Plasma backend state
# ---------------------------------------------------------------------------------
section "11. PLASMA BACKEND"
runsh "plasmashell / kwin versions" "plasmashell --version 2>&1; kwin_wayland --version 2>&1"
runsh "wallpaper plugin, user copy" "ls -la \"\${HOME}/.local/share/plasma/wallpapers/org.wallpaperanim.video\" 2>&1"
runsh "wallpaper plugin, system copy" "ls -la /usr/share/plasma/wallpapers/org.wallpaperanim.video 2>&1"
runsh "plugin QML present" "find \"\${HOME}/.local/share/plasma/wallpapers/org.wallpaperanim.video\" /usr/share/plasma/wallpapers/org.wallpaperanim.video -name '*.qml' -o -name 'metadata.json' 2>/dev/null"
runsh "which wallpaper plugin each containment uses" \
    "grep -aE 'wallpaperplugin|^\[Containments\]\[[0-9]+\]$' \"\${HOME}/.config/plasma-org.kde.plasma.desktop-appletsrc\" 2>/dev/null | head -40"
runsh "plugin's own stored state" \
    "grep -a -A8 'wallpaperanim' \"\${HOME}/.config/plasma-org.kde.plasma.desktop-appletsrc\" 2>/dev/null | head -40"
runsh "current activity" "grep -a -A3 '\\[main\\]' \"\${HOME}/.config/kactivitymanagerdrc\" 2>/dev/null | head"
runsh "plasmashell on the bus" "busctl --user list 2>/dev/null | grep -i plasmashell | head -3"

# ---------------------------------------------------------------------------------
# 12. Logs and crashes
# ---------------------------------------------------------------------------------
section "12. LOGS & CRASHES"
runsh "user journal, this boot" "journalctl --user -b --no-pager 2>/dev/null | grep -iE 'wallpaperanim' | tail -60"
runsh "system journal" "journalctl -b --no-pager 2>/dev/null | grep -iE 'wallpaperanim' | tail -30"
runsh "plasmashell complaints" "journalctl --user -b --no-pager 2>/dev/null | grep -iE 'plasmashell.*(wallpaper|qml|error)' | tail -25"
runsh "core dumps" "coredumpctl list --no-pager 2>/dev/null | grep -i wallpaperanim | tail -5"
if have coredumpctl && coredumpctl list --no-pager 2>/dev/null | grep -qi wallpaperanim; then
    finding "Kayıtlı bir çökme (core dump) var - 12. bölümdeki backtrace'e bak."
    runsh "last core dump backtrace" \
        "coredumpctl info wallpaperanim 2>/dev/null | sed -n '1,60p'"
fi

# ---------------------------------------------------------------------------------
# Assemble: findings first, then everything they were derived from
# ---------------------------------------------------------------------------------
{
    echo "WallpaperAnim diagnostic report"
    echo "generated $(date -Is 2>/dev/null || date) by diagnose.sh v${SCRIPT_VERSION}"
    echo
    echo "==============================================================================="
    echo "AUTOMATIC FINDINGS"
    echo "==============================================================================="
    if [[ ${#FINDINGS[@]} -eq 0 ]]; then
        echo
        echo "  Hiçbir otomatik bulgu yok: binary bağlanıyor, başlıyor ve ayakta kalıyor."
        echo "  Sorun büyük ihtimalle görünürlük tarafında (tepsi ikonu, arka plan seçimi,"
        echo "  Plasma containment). 7., 8. ve 11. bölümlere bakın."
    else
        echo
        i=1
        for f in "${FINDINGS[@]}"; do
            echo "  ${i}. ${f}"
            i=$((i + 1))
        done
    fi
    cat "${BODY}"
} >"${OUT}"

if [[ ${REDACT} -eq 1 ]]; then
    # Paths still need to be readable, so only the identifying parts go.
    sed -i "s|${HOME}|/home/USER|g; s|\\b${USER}\\b|USER|g" "${OUT}" 2>/dev/null
fi

echo
echo "Report written to:"
echo "  ${OUT}"
echo
echo "Size: $(du -h "${OUT}" 2>/dev/null | cut -f1). Send this one file back."
if [[ ${#FINDINGS[@]} -gt 0 ]]; then
    echo
    echo "Findings already visible at the top of the report:"
    i=1
    for f in "${FINDINGS[@]}"; do
        echo "  ${i}. ${f}"
        i=$((i + 1))
    done
fi
