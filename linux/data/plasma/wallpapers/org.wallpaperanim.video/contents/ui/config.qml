import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: true
        type: Kirigami.MessageType.Information
        text: i18n("This wallpaper is driven by the WallpaperAnim app. Open WallpaperAnim from the system tray to choose the video, the fit mode or auto-rotation.")
    }

    Item {
        Layout.fillHeight: true
    }
}
