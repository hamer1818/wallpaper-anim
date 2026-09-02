# WallpaperAnim for Linux

A native Wayland port of WallpaperAnim: videos, GIFs and GLSL shaders as your desktop
wallpaper, with the same library, playlists and settings as the Windows build — the two
share the same `config.json` schema.

Rendering is **libmpv** (hardware decoded, frames stay on the GPU) and the UI is **Qt 6
Widgets**. Nothing about the Windows/DirectX code is reused; this is a separate program
in the same repository.

## Install

**Arch / CachyOS — one command.** This is the counterpart of the Windows
`WallpaperAnimSetup.exe`: pacman pulls in Qt 6, mpv and the rest by itself, and the app
lands in the application menu. Nothing to extract, no PATH to set.

```sh
curl -fsSL https://raw.githubusercontent.com/hamer1818/wallpaper-anim/master/linux/install.sh | bash
```

The same command **updates** an existing install: it fetches the newest release, verifies
the download against the published SHA-256, stops the running instance, installs over the
old version and starts it again. Before installing anything it compares the library
versions the package was compiled against with the ones on this machine and stops with an
explanation rather than leaving you with an app that installs cleanly and never starts —
`--force` overrides, `--tag v1.6.0` pins a version, `--dry-run` shows what it would do.

Every run is appended to `~/wallpaperanim-install.log`, and a failure prints that path
plus the command that collects a full diagnostic report — so a `curl | bash` that goes
wrong leaves two files to send back rather than a screenshot of a terminal.

Or download the `.pkg.tar.zst` from [Releases](https://github.com/hamer1818/wallpaper-anim/releases)
and install it by hand:

```sh
sudo pacman -U wallpaperanim-1.6.0-1-x86_64.pkg.tar.zst
```

Build that file from a checkout with:

```sh
cd linux && ./package.sh            # -> linux/dist/wallpaperanim-<version>-x86_64.pkg.tar.zst
cd linux && ./package.sh install    # build it and install it here
cd linux && ./release.sh            # build it and publish it to the GitHub release
```

Optional extra, only for the YouTube tab: `sudo pacman -S yt-dlp`.

First run: launch **WallpaperAnim** from the menu, add a video (or paste a YouTube link)
and apply it — on KDE that switches the desktop over to the app's wallpaper plugin
automatically. Turn on *Settings → Start at login* to have it come back after a reboot.

Any other distro builds from source; see below.

## Requirements

Arch / CachyOS:

```sh
sudo pacman -S --needed qt6-base qt6-multimedia qt6-declarative mpv layer-shell-qt \
                        ffmpeg cmake ninja pkgconf
sudo pacman -S yt-dlp          # optional, only for the YouTube tab
```

`layer-shell-qt` is optional at build time — without it only the Plasma backend is
available. `plasma-workspace` is needed for the Plasma backend.

## Build

```sh
cd linux
./build.sh                 # -> linux/build/wallpaperanim
./build.sh user-install    # installs into ~/.local (no sudo)
./build.sh install         # installs into /usr/local (sudo)
```

Arch package, straight from a checkout (`package.sh` above wraps this and drops the
result in `linux/dist/` instead of installing it):

```sh
cd linux/packaging && makepkg -si
```

## How it draws behind the desktop icons

There are two backends, because one mechanism does not fit every compositor.

### Plasma backend (KDE — the default there)

On KDE, drawing behind the desktop icons from an outside process is not possible.
Measured on Plasma 6 / KWin, in a nested session:

- a `zwlr_layer_shell_v1` surface on the **background** layer is stacked *below*
  plasmashell's desktop window, which paints an opaque background — so the wallpaper
  is completely invisible;
- the same surface on the **bottom** layer is stacked *above* the desktop window — so it
  is visible, but it covers the icons and desktop widgets;
- pointing the desktop containment at a wallpaper plugin that paints nothing does not
  help: the desktop window stays opaque (with its opacity forced to 0.99, only ~1% of
  the layer surface below bleeds through, which is what an opaque black surface
  composited at 99% looks like).

So on Plasma the app *becomes* the wallpaper: `data/plasma/wallpapers/org.wallpaperanim.video`
is a small QML wallpaper plugin that plays whatever `config.json` points at. Because it
runs inside plasmashell, the icons and widgets stay on top, exactly like the Windows
`WorkerW` behaviour.

Applying any wallpaper from the app takes care of this automatically: the plugin is
installed into `~/.local/share/plasma/wallpapers` and every desktop containment is
pointed at it. **Settings → Display Method** has explicit "Set as Plasma wallpaper" /
"Restore Plasma wallpaper" buttons too; restoring puts `org.kde.image` back.

The plugin does **not** read `config.json`. Qt 6 refuses `XMLHttpRequest` on `file://`
URLs unless `QML_XHR_ALLOW_FILE_READ=1` is set, and plasmashell does not set it, so a
file-reading plugin would silently render nothing. Instead the app pushes state into the
plugin's own Plasma configuration over plasmashell's scripting D-Bus interface
(`writeConfig` + `reloadConfig`):

| Key | Meaning |
| --- | --- |
| `MediaPath` | absolute path of the current wallpaper |
| `FitMode` | 0 fill · 1 fit · 2 stretch · 3 center |
| `Paused` | set by the app's auto-pause policy |

Because those live in the containment config, the wallpaper keeps playing after a logout
or when the app is not running.

### Layer-shell backend (wlroots compositors)

On Hyprland, Sway, river and friends the app creates its own `zwlr_layer_shell_v1`
surface per output, on the **background** layer, and renders into it with libmpv's
OpenGL render API. This is the backend that also runs GLSL shaders.

`Settings → Display Method → Backend` forces either backend; **Automatic** picks Plasma
on KDE and layer-shell everywhere else.

## Supported media

| Type | Layer-shell backend | Plasma backend |
| --- | --- | --- |
| Video (`.mp4`, `.mkv`, `.webm`, `.mov`, `.avi`) | yes (libmpv) | yes (QtMultimedia) |
| GIF | yes | yes |
| GLSL shader (`.glsl`, `.frag`, `.fs`) | yes | no |
| HLSL shader (`.hlsl`) | no — Windows only | no |

Shaders follow the Shadertoy convention: define `mainImage(out vec4, in vec2)` and the
renderer supplies `iResolution`, `iTime`, `iTimeDelta`, `iFrame`, `iMouse` and `iDate`.
A complete fragment shader with its own `main()` also works. See
[`../assets/sample.glsl`](../assets/sample.glsl).

## Files

```
~/.config/WallpaperAnim/config.json     settings + library (same schema as Windows)
~/.local/share/WallpaperAnim/
├── thumbnails/                         generated by ffmpeg
└── downloads/                          yt-dlp output
~/.local/share/plasma/wallpapers/org.wallpaperanim.video/   (Plasma backend)
~/.config/autostart/wallpaperanim.desktop
```

Linux-only settings are stored under `linux*` keys, so a `config.json` can be carried
between Windows and Linux.

## Command line

```sh
wallpaperanim              # start (opens settings)
wallpaperanim --background # start silently, for autostart
wallpaperanim --toggle     # pause/resume the running instance
wallpaperanim --next       # switch to the next wallpaper
wallpaperanim --quit
```

Only one instance runs; a second launch forwards its command to the first over a local
socket and exits.

## Auto-pause

- **On battery** — read from `/sys/class/power_supply`.
- **An app blocking the screensaver** — Wayland offers no way to inspect other clients'
  windows, so the app asks the desktop instead: it checks
  `org.freedesktop.PowerManagement.Inhibit.HasInhibit`. Browsers and remote-desktop tools
  hold that inhibition for hours as well, which is why this one is **off by default**.
- **While hidden** (layer-shell backend only) — when the compositor stops handing out
  frames because the wallpaper is fully covered, decoding stops until it asks again.

## Troubleshooting

**It does not start at all — collect a diagnostic report.** `diagnose.sh` (installed as
`wallpaperanim-diagnose`, and shipped next to the package in `linux/dist/`) writes one
text file with everything needed to pinpoint a startup failure, and prints its findings
straight away:

```sh
wallpaperanim-diagnose          # or ./diagnose.sh from linux/dist
```

It is read-only apart from that file: no config, package or wallpaper is touched. It
records the distro and session, the dynamic-link state of the binary, an actual launch
attempt with the exit code and stderr, the config and media it would load, the Plasma
containment state, and the journal and core dumps. The most important part is automatic:
the packaged copy is stamped with the exact library versions the binary was compiled
against, and the script compares them against the ones installed on that machine with
`vercmp`. Arch-family distros make no ABI promise across versions and this package
declares its dependencies without version bounds, so a package built on an up-to-date
machine installs cleanly on a system that is months behind and then refuses to start —
that comparison names the offending package instead of leaving it to guesswork.

**Nothing shows up on KDE.** Check that the desktop containment really switched:
`qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "print(desktops()[0].wallpaperPlugin)"`
should print `org.wallpaperanim.video`. If it prints `org.kde.image`, press "Set as
Plasma wallpaper" in Settings → Display Method.

**The wallpaper covers my desktop icons.** You are on the layer-shell backend with the
*bottom* layer. Switch the layer to *Background*, or switch to the Plasma backend.

**Plasma shows a black desktop after activating.** plasmashell's QML engine caches a
component per URL for the life of the process, so a refreshed plugin package does not
take effect until it restarts. The app detects that it replaced the package and restarts
plasmashell itself; if you updated the files by hand, run
`systemctl --user restart plasma-plasmashell` once.

**The wallpaper looks like it is not playing.** Check that plasmashell really has the
file open — that is the wallpaper being decoded inside the shell:
`ls -l /proc/$(pgrep -f "plasmashell --no-respawn")/fd | grep WallpaperAnim`

**Video plays but stutters.** Turn off *Hardware decoding* in Settings → Playback, or
cap *Max FPS*. Check `mpv <file>` plays the same file smoothly first.

**YouTube tab is disabled.** Install `yt-dlp`.

## Known limitations

- X11 sessions are not supported; the app needs a Wayland compositor with
  `zwlr_layer_shell_v1`, or KDE Plasma.
- The Plasma backend has no GLSL shader support (Qt 6 cannot compile GLSL at runtime
  from QML).
- *Center* fit mode falls back to letterboxing on the Plasma backend.
- The Windows in-app updater is not part of the Linux build; use your package manager.
