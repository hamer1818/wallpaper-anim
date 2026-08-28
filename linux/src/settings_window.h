#pragma once

#include <QWidget>

class App;
class YoutubeDownloader;

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTabWidget;

// Qt Widgets replacement for the WinUI 3 MainWindow: library, add-new, playlists,
// settings and about, all writing to the same config.json the Windows build uses.
class SettingsWindow : public QWidget {
    Q_OBJECT

public:
    explicit SettingsWindow(App* app, QWidget* parent = nullptr);
    ~SettingsWindow() override;

    void RefreshLibrary();
    void RefreshPlaylists();
    void RefreshPlasmaStatus();

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void onApplySelected();
    void onRemoveSelected();
    void onBrowseFile();
    void onDownloadClicked();
    void onDownloadProgress(int percent, const QString& statusLine);
    void onDownloadFinished(const QString& filePath, const QString& title);
    void onDownloadFailed(const QString& message);
    void onCreatePlaylist();
    void onDeletePlaylist();
    void onPlaylistSelectionChanged();
    void onAddSelectedToPlaylist();
    void onRemoveSelectedFromPlaylist();
    void onActivePlaylistChanged(int index);
    void onPlasmaActivate();
    void onPlasmaRestore();

private:
    QWidget* buildLibraryTab();
    QWidget* buildAddTab();
    QWidget* buildPlaylistTab();
    QWidget* buildSettingsTab();
    QWidget* buildAboutTab();

    void loadSettingsIntoUi();
    void connectSettingSignals();
    void saveConfig();
    QString selectedLibraryPath() const;
    QString currentPlaylistName() const;

    App* m_app = nullptr;
    QTabWidget* m_tabs = nullptr;

    // Library
    QListWidget* m_library = nullptr;
    QLabel* m_libraryEmptyLabel = nullptr;

    // Add new
    QLineEdit* m_urlEdit = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QProgressBar* m_downloadProgress = nullptr;
    QLabel* m_downloadStatus = nullptr;
    YoutubeDownloader* m_downloader = nullptr;

    // Playlists
    QListWidget* m_playlists = nullptr;
    QListWidget* m_playlistItems = nullptr;
    QLineEdit* m_newPlaylistName = nullptr;
    QComboBox* m_activePlaylist = nullptr;
    QCheckBox* m_rotateEnabled = nullptr;
    QSpinBox* m_rotateInterval = nullptr;
    QCheckBox* m_rotateShuffle = nullptr;

    // Settings
    QComboBox* m_fitMode = nullptr;
    QSpinBox* m_maxFps = nullptr;
    QCheckBox* m_hardwareDecode = nullptr;
    QCheckBox* m_pauseFullscreen = nullptr;
    QCheckBox* m_pauseBattery = nullptr;
    QCheckBox* m_pauseHidden = nullptr;
    QCheckBox* m_autostart = nullptr;
    QComboBox* m_language = nullptr;
    QComboBox* m_backend = nullptr;
    QComboBox* m_layer = nullptr;
    QLabel* m_plasmaStatus = nullptr;
    QPushButton* m_plasmaActivate = nullptr;
    QPushButton* m_plasmaRestore = nullptr;

    bool m_loading = false;
};
