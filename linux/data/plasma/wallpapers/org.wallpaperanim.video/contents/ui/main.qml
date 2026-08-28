/*
    WallpaperAnim - Plasma wallpaper plugin.

    KWin keeps every wlr-layer-shell background surface underneath plasmashell's
    desktop window, and that window paints an opaque background even when its
    wallpaper plugin draws nothing. So on Plasma the only way to appear *behind* the
    desktop icons is to be the wallpaper - which is what this plugin does.

    State (which file, how to fit it, paused or not) arrives through this plugin's own
    Plasma configuration: the WallpaperAnim app writes it with plasmashell's scripting
    D-Bus interface. The plugin deliberately does NOT read config.json itself - Qt 6
    refuses XMLHttpRequest on file:// URLs unless QML_XHR_ALLOW_FILE_READ is set, which
    plasmashell does not set. Because the values live in the containment config, the
    wallpaper keeps playing even when the app is not running.
*/

import QtQuick
import QtMultimedia
import org.kde.plasma.plasmoid

WallpaperItem {
    id: root

    property string mediaPath: ""
    property int fitMode: 0
    property bool appPaused: false

    // 0 = video, 1 = GIF, 2 = shader (rendered by the app's own surface, not here)
    readonly property int mediaKind: {
        const lower = mediaPath.toLowerCase();
        if (lower.endsWith(".gif")) {
            return 1;
        }
        if (lower.endsWith(".glsl") || lower.endsWith(".frag") || lower.endsWith(".fs")
            || lower.endsWith(".hlsl")) {
            return 2;
        }
        return 0;
    }

    readonly property url mediaUrl: mediaPath.length > 0 ? "file://" + encodeURI(mediaPath) : ""

    function syncFromConfiguration() {
        const path = root.configuration.MediaPath ? root.configuration.MediaPath : "";
        const fit = root.configuration.FitMode !== undefined ? root.configuration.FitMode : 0;
        const paused = root.configuration.Paused === true;

        if (path !== root.mediaPath) {
            root.mediaPath = path;
        }
        if (fit !== root.fitMode) {
            root.fitMode = fit;
        }
        if (paused !== root.appPaused) {
            root.appPaused = paused;
        }
    }

    Component.onCompleted: syncFromConfiguration()

    // KConfigPropertyMap does not always emit a per-key change signal, so the values
    // are re-read on a slow timer rather than relied on as live bindings.
    Timer {
        interval: 2000
        running: true
        repeat: true
        onTriggered: root.syncFromConfiguration()
    }

    // Letterboxed fit modes show this behind the media.
    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        visible: root.mediaKind === 0
        fillMode: {
            switch (root.fitMode) {
            case 1:
                return VideoOutput.PreserveAspectFit;
            case 2:
                return VideoOutput.Stretch;
            case 3:
                // VideoOutput cannot draw at native size; letterboxing is the closest fit.
                return VideoOutput.PreserveAspectFit;
            default:
                return VideoOutput.PreserveAspectCrop;
            }
        }
    }

    MediaPlayer {
        id: player
        source: root.mediaKind === 0 ? root.mediaUrl : ""
        videoOutput: videoOutput
        loops: MediaPlayer.Infinite
        audioOutput: AudioOutput {
            muted: true
            volume: 0
        }

        onSourceChanged: {
            if (root.mediaKind === 0 && root.mediaPath.length > 0 && !root.appPaused) {
                player.play();
            }
        }

        onErrorOccurred: function (error, errorString) {
            console.warn("WallpaperAnim: playback error:", errorString);
        }
    }

    AnimatedImage {
        id: animatedImage
        anchors.fill: parent
        visible: root.mediaKind === 1
        source: root.mediaKind === 1 ? root.mediaUrl : ""
        playing: visible && !root.appPaused
        cache: false
        fillMode: {
            switch (root.fitMode) {
            case 1:
                return Image.PreserveAspectFit;
            case 2:
                return Image.Stretch;
            case 3:
                return Image.Pad;
            default:
                return Image.PreserveAspectCrop;
            }
        }
    }

    // Shaders are rendered by the WallpaperAnim app's own GL surface, not here.
    Text {
        anchors.centerIn: parent
        visible: root.mediaKind === 2
        color: "#888888"
        text: "GLSL shaders need the layer-shell backend"
    }

    onAppPausedChanged: {
        if (root.mediaKind !== 0) {
            return;
        }
        if (root.appPaused) {
            player.pause();
        } else if (root.mediaPath.length > 0) {
            player.play();
        }
    }
}
