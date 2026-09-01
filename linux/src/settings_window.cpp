#include "settings_window.h"

#include "app.h"
#include "autostart.h"
#include "config.h"
#include "localization.h"
#include "plasma_integration.h"
#include "thumbnail.h"
#include "youtube.h"
#include "version.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString tr8(const char* utf8) { return QString::fromUtf8(utf8); }

QLabel* makeHint(const QString& text)
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: palette(mid);"));
    return label;
}

} // namespace

SettingsWindow::SettingsWindow(App* app, QWidget* parent) : QWidget(parent), m_app(app)
{
    const auto& strings = Localization::Get();
    setWindowTitle(tr8(strings.settingsTitle));
    resize(940, 660);

    m_downloader = new YoutubeDownloader(this);
    connect(m_downloader, &YoutubeDownloader::progress, this, &SettingsWindow::onDownloadProgress);
    connect(m_downloader, &YoutubeDownloader::finished, this, &SettingsWindow::onDownloadFinished);
    connect(m_downloader, &YoutubeDownloader::failed, this, &SettingsWindow::onDownloadFailed);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildLibraryTab(), tr8(strings.tabLibrary));
    m_tabs->addTab(buildAddTab(), tr8(strings.tabAddNew));
    m_tabs->addTab(buildPlaylistTab(), tr8(strings.tabPlaylists));
    m_tabs->addTab(buildSettingsTab(), tr8(strings.tabSettings));
    m_tabs->addTab(buildAboutTab(), tr8(strings.tabAbout));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->addWidget(m_tabs);

    loadSettingsIntoUi();
    connectSettingSignals();
    RefreshLibrary();
    RefreshPlaylists();
    RefreshPlasmaStatus();

    if (m_app) {
        connect(m_app, &App::wallpaperChanged, this, [this](const QString&) { RefreshLibrary(); });
    }
}

SettingsWindow::~SettingsWindow() = default;

void SettingsWindow::saveConfig()
{
    Config::ConfigManager::GetInstance().Save();
}

// ---------------------------------------------------------------- Library tab

QWidget* SettingsWindow::buildLibraryTab()
{
    const auto& strings = Localization::Get();

    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    auto* header = new QLabel(tr8(strings.yourWallpapers));
    QFont headerFont = header->font();
    headerFont.setBold(true);
    headerFont.setPointSizeF(headerFont.pointSizeF() * 1.2);
    header->setFont(headerFont);
    layout->addWidget(header);

    m_libraryEmptyLabel = makeHint(tr8(strings.noHistory));
    layout->addWidget(m_libraryEmptyLabel);

    m_library = new QListWidget(page);
    m_library->setViewMode(QListView::IconMode);
    m_library->setIconSize(QSize(240, 135));
    m_library->setGridSize(QSize(260, 190));
    m_library->setResizeMode(QListView::Adjust);
    m_library->setMovement(QListView::Static);
    m_library->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_library->setWordWrap(true);
    m_library->setSpacing(6);
    connect(m_library, &QListWidget::itemDoubleClicked, this, &SettingsWindow::onApplySelected);
    layout->addWidget(m_library, 1);

    auto* buttons = new QHBoxLayout();
    auto* applyButton = new QPushButton(tr8(strings.applyBtn));
    connect(applyButton, &QPushButton::clicked, this, &SettingsWindow::onApplySelected);
    auto* removeButton = new QPushButton(tr8(strings.remove));
    connect(removeButton, &QPushButton::clicked, this, &SettingsWindow::onRemoveSelected);
    buttons->addWidget(applyButton);
    buttons->addWidget(removeButton);
    buttons->addStretch(1);
    layout->addLayout(buttons);

    return page;
}

void SettingsWindow::RefreshLibrary()
{
    if (!m_library) return;

    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    const QString current = QString::fromStdString(cfg.lastVideoPath);

    m_library->clear();
    for (const auto& entry : cfg.history) {
        const QString path = QString::fromStdString(entry.path);
        const QString name = entry.name.empty() ? QFileInfo(path).completeBaseName()
                                                : QString::fromStdString(entry.name);

        auto* item = new QListWidgetItem();
        item->setText(path == current ? QStringLiteral("● %1").arg(name) : name);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);

        const QString thumb = QString::fromStdString(entry.thumbPath);
        if (!thumb.isEmpty() && QFileInfo::exists(thumb)) {
            item->setIcon(QIcon(thumb));
        } else {
            item->setIcon(QIcon::fromTheme(entry.type == 2 ? QStringLiteral("applications-graphics")
                                                           : QStringLiteral("video-x-generic")));
        }
        if (!QFileInfo::exists(path)) {
            item->setForeground(Qt::red);
            item->setToolTip(QStringLiteral("%1\n(missing)").arg(path));
        }
        m_library->addItem(item);
    }

    m_libraryEmptyLabel->setVisible(cfg.history.empty());
}

QString SettingsWindow::selectedLibraryPath() const
{
    if (!m_library) return {};
    QListWidgetItem* item = m_library->currentItem();
    if (!item) return {};
    return item->data(Qt::UserRole).toString();
}

void SettingsWindow::onApplySelected()
{
    const QString path = selectedLibraryPath();
    if (path.isEmpty() || !m_app) return;

    const auto& strings = Localization::Get();
    QString error;
    if (!m_app->ApplyWallpaper(path, &error)) {
        QMessageBox::warning(this, tr8(strings.mediaLoadFailedTitle),
                             error.isEmpty() ? tr8(strings.mediaLoadFailed) : error);
        return;
    }
    RefreshLibrary();
}

void SettingsWindow::onRemoveSelected()
{
    const QString path = selectedLibraryPath();
    if (path.isEmpty()) return;

    const auto& strings = Localization::Get();
    if (QMessageBox::question(this, tr8(strings.removeConfirmTitle), tr8(strings.removeConfirmBody))
        != QMessageBox::Yes) {
        return;
    }

    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();
    const std::string utf8Path = path.toStdString();

    auto entry = std::find_if(cfg.history.begin(), cfg.history.end(),
                              [&](const Config::WallpaperHistoryItem& item) { return item.path == utf8Path; });
    if (entry == cfg.history.end()) return;

    // The thumbnail is ours, so it always goes. The media file only goes when the app
    // downloaded it: a user's own file is never deleted.
    if (!entry->thumbPath.empty()) QFile::remove(QString::fromStdString(entry->thumbPath));
    const QString downloadsDir = QString::fromStdString(Config::DownloadsDir());
    if (path.startsWith(downloadsDir + QLatin1Char('/'))) QFile::remove(path);

    cfg.history.erase(entry);

    for (auto& playlist : cfg.playlists) {
        playlist.paths.erase(std::remove(playlist.paths.begin(), playlist.paths.end(), utf8Path),
                             playlist.paths.end());
    }

    if (cfg.lastVideoPath == utf8Path) cfg.lastVideoPath.clear();
    manager.Save();

    RefreshLibrary();
    RefreshPlaylists();
}

// ---------------------------------------------------------------- Add new tab

QWidget* SettingsWindow::buildAddTab()
{
    const auto& strings = Localization::Get();

    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    auto* localGroup = new QGroupBox(tr8(strings.localFile));
    auto* localLayout = new QVBoxLayout(localGroup);
    localLayout->addWidget(makeHint(tr8(strings.localFileDesc)));
    auto* browseButton = new QPushButton(tr8(strings.browseBtn));
    connect(browseButton, &QPushButton::clicked, this, &SettingsWindow::onBrowseFile);
    auto* browseRow = new QHBoxLayout();
    browseRow->addWidget(browseButton);
    browseRow->addStretch(1);
    localLayout->addLayout(browseRow);
    layout->addWidget(localGroup);

    auto* youtubeGroup = new QGroupBox(tr8(strings.youtubeVideo));
    auto* youtubeLayout = new QVBoxLayout(youtubeGroup);
    youtubeLayout->addWidget(makeHint(tr8(strings.youtubeDesc)));

    auto* urlRow = new QHBoxLayout();
    urlRow->addWidget(new QLabel(tr8(strings.urlLabel)));
    m_urlEdit = new QLineEdit();
    m_urlEdit->setPlaceholderText(tr8(strings.youtubePlaceholder));
    urlRow->addWidget(m_urlEdit, 1);
    m_downloadButton = new QPushButton(tr8(strings.downloadPlayBtn));
    connect(m_downloadButton, &QPushButton::clicked, this, &SettingsWindow::onDownloadClicked);
    urlRow->addWidget(m_downloadButton);
    youtubeLayout->addLayout(urlRow);

    m_downloadProgress = new QProgressBar();
    m_downloadProgress->setRange(0, 100);
    m_downloadProgress->setValue(0);
    m_downloadProgress->setVisible(false);
    youtubeLayout->addWidget(m_downloadProgress);

    m_downloadStatus = new QLabel();
    m_downloadStatus->setWordWrap(true);
    youtubeLayout->addWidget(m_downloadStatus);

    if (!YoutubeDownloader::IsAvailable()) {
        m_downloadButton->setEnabled(false);
        m_downloadStatus->setText(tr8(strings.ytDlpMissing));
    }

    layout->addWidget(youtubeGroup);
    layout->addStretch(1);
    return page;
}

void SettingsWindow::onBrowseFile()
{
    const auto& strings = Localization::Get();
    const QString path = QFileDialog::getOpenFileName(this, tr8(strings.fileDialogTitle), QDir::homePath(),
                                                      tr8(strings.fileDialogFilter));
    if (path.isEmpty() || !m_app) return;

    QString error;
    if (!m_app->ApplyWallpaper(path, &error)) {
        QMessageBox::warning(this, tr8(strings.mediaLoadFailedTitle),
                             error.isEmpty() ? tr8(strings.mediaLoadFailed) : error);
        return;
    }
    RefreshLibrary();
    m_tabs->setCurrentIndex(0);
}

void SettingsWindow::onDownloadClicked()
{
    const auto& strings = Localization::Get();
    const QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty()) return;

    if (!YoutubeDownloader::IsAvailable()) {
        m_downloadStatus->setText(tr8(strings.ytDlpMissing));
        return;
    }

    int maxHeight = 1080;
    for (QScreen* screen : QApplication::screens()) {
        maxHeight = qMax(maxHeight, static_cast<int>(screen->size().height() * screen->devicePixelRatio()));
    }

    m_downloadButton->setEnabled(false);
    m_downloadProgress->setVisible(true);
    m_downloadProgress->setValue(0);
    m_downloadStatus->setText(tr8(strings.downloadStarting));
    m_downloader->Start(url, maxHeight);
}

void SettingsWindow::onDownloadProgress(int percent, const QString& statusLine)
{
    Q_UNUSED(statusLine);
    const auto& strings = Localization::Get();
    m_downloadProgress->setValue(qBound(0, percent, 100));
    m_downloadStatus->setText(QStringLiteral("%1 %2%").arg(tr8(strings.downloading)).arg(percent));
}

void SettingsWindow::onDownloadFinished(const QString& filePath, const QString& title)
{
    const auto& strings = Localization::Get();
    m_downloadButton->setEnabled(true);
    m_downloadProgress->setValue(100);
    m_downloadStatus->setText(tr8(strings.downloadComplete));

    if (!m_app) return;

    QString error;
    if (!m_app->ApplyWallpaper(filePath, &error)) {
        QMessageBox::warning(this, tr8(strings.mediaLoadFailedTitle),
                             error.isEmpty() ? tr8(strings.mediaLoadFailed) : error);
        return;
    }

    // Mark it as an app-managed download so removing it also deletes the file.
    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();
    const std::string utf8Path = filePath.toStdString();
    for (auto& item : cfg.history) {
        if (item.path != utf8Path) continue;
        item.type = 3;
        if (!title.isEmpty()) item.name = title.toStdString();
        break;
    }
    manager.Save();

    m_urlEdit->clear();
    RefreshLibrary();
    m_tabs->setCurrentIndex(0);
}

void SettingsWindow::onDownloadFailed(const QString& message)
{
    const auto& strings = Localization::Get();
    m_downloadButton->setEnabled(true);
    m_downloadProgress->setVisible(false);
    m_downloadStatus->setText(QStringLiteral("%1: %2").arg(tr8(strings.videoDownloadFailed), message));
}

// ---------------------------------------------------------------- Playlists tab

QWidget* SettingsWindow::buildPlaylistTab()
{
    const auto& strings = Localization::Get();

    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    auto* rotationGroup = new QGroupBox(tr8(strings.playlistTitle));
    auto* rotationLayout = new QVBoxLayout(rotationGroup);
    rotationLayout->addWidget(makeHint(tr8(strings.playlistDesc)));

    m_rotateEnabled = new QCheckBox(tr8(strings.playlistTitle));
    rotationLayout->addWidget(m_rotateEnabled);

    auto* intervalRow = new QHBoxLayout();
    intervalRow->addWidget(new QLabel(tr8(strings.playlistIntervalLabel)));
    m_rotateInterval = new QSpinBox();
    m_rotateInterval->setRange(1, 1440);
    intervalRow->addWidget(m_rotateInterval);
    intervalRow->addStretch(1);
    rotationLayout->addLayout(intervalRow);

    m_rotateShuffle = new QCheckBox(tr8(strings.playlistShuffle));
    rotationLayout->addWidget(m_rotateShuffle);

    auto* activeRow = new QHBoxLayout();
    activeRow->addWidget(new QLabel(tr8(strings.activePlaylistLabel)));
    m_activePlaylist = new QComboBox();
    activeRow->addWidget(m_activePlaylist, 1);
    rotationLayout->addLayout(activeRow);
    rotationLayout->addWidget(makeHint(tr8(strings.playlistHint)));

    layout->addWidget(rotationGroup);

    auto* listsGroup = new QGroupBox(tr8(strings.playlistsHeader));
    auto* listsLayout = new QHBoxLayout(listsGroup);

    auto* leftColumn = new QVBoxLayout();
    m_playlists = new QListWidget();
    connect(m_playlists, &QListWidget::currentRowChanged, this,
            [this](int) { onPlaylistSelectionChanged(); });
    leftColumn->addWidget(m_playlists, 1);

    auto* createRow = new QHBoxLayout();
    m_newPlaylistName = new QLineEdit();
    m_newPlaylistName->setPlaceholderText(tr8(strings.newPlaylistPlaceholder));
    createRow->addWidget(m_newPlaylistName, 1);
    auto* createButton = new QPushButton(tr8(strings.createBtn));
    connect(createButton, &QPushButton::clicked, this, &SettingsWindow::onCreatePlaylist);
    createRow->addWidget(createButton);
    auto* deleteButton = new QPushButton(tr8(strings.deleteBtn));
    connect(deleteButton, &QPushButton::clicked, this, &SettingsWindow::onDeletePlaylist);
    createRow->addWidget(deleteButton);
    leftColumn->addLayout(createRow);
    listsLayout->addLayout(leftColumn, 1);

    auto* rightColumn = new QVBoxLayout();
    m_playlistItems = new QListWidget();
    m_playlistItems->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rightColumn->addWidget(m_playlistItems, 1);

    auto* itemButtons = new QHBoxLayout();
    auto* addButton = new QPushButton(tr8(strings.addSelectedBtn));
    connect(addButton, &QPushButton::clicked, this, &SettingsWindow::onAddSelectedToPlaylist);
    auto* removeButton = new QPushButton(tr8(strings.removeSelectedBtn));
    connect(removeButton, &QPushButton::clicked, this, &SettingsWindow::onRemoveSelectedFromPlaylist);
    itemButtons->addWidget(addButton);
    itemButtons->addWidget(removeButton);
    itemButtons->addStretch(1);
    rightColumn->addLayout(itemButtons);
    listsLayout->addLayout(rightColumn, 2);

    layout->addWidget(listsGroup, 1);
    return page;
}

QString SettingsWindow::currentPlaylistName() const
{
    if (!m_playlists) return {};
    QListWidgetItem* item = m_playlists->currentItem();
    return item ? item->text() : QString();
}

void SettingsWindow::RefreshPlaylists()
{
    if (!m_playlists || !m_activePlaylist) return;

    const auto& strings = Localization::Get();
    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    const QString previousSelection = currentPlaylistName();

    m_loading = true;

    m_playlists->clear();
    for (const auto& playlist : cfg.playlists) {
        m_playlists->addItem(QString::fromStdString(playlist.name));
    }

    m_activePlaylist->clear();
    m_activePlaylist->addItem(tr8(strings.allLibraryItem), QString());
    for (const auto& playlist : cfg.playlists) {
        const QString name = QString::fromStdString(playlist.name);
        m_activePlaylist->addItem(name, name);
    }
    const QString active = QString::fromStdString(cfg.activePlaylist);
    const int activeIndex = qMax(0, m_activePlaylist->findData(active));
    m_activePlaylist->setCurrentIndex(activeIndex);

    if (!previousSelection.isEmpty()) {
        const auto matches = m_playlists->findItems(previousSelection, Qt::MatchExactly);
        if (!matches.isEmpty()) m_playlists->setCurrentItem(matches.first());
    } else if (m_playlists->count() > 0) {
        m_playlists->setCurrentRow(0);
    }

    m_loading = false;
    onPlaylistSelectionChanged();
}

void SettingsWindow::onPlaylistSelectionChanged()
{
    if (!m_playlistItems) return;
    m_playlistItems->clear();

    const QString name = currentPlaylistName();
    if (name.isEmpty()) return;

    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();
    const std::string utf8Name = name.toStdString();
    for (const auto& playlist : cfg.playlists) {
        if (playlist.name != utf8Name) continue;
        for (const auto& path : playlist.paths) {
            const QString qpath = QString::fromStdString(path);
            auto* item = new QListWidgetItem(QFileInfo(qpath).completeBaseName());
            item->setData(Qt::UserRole, qpath);
            item->setToolTip(qpath);
            m_playlistItems->addItem(item);
        }
        break;
    }
}

void SettingsWindow::onCreatePlaylist()
{
    const QString name = m_newPlaylistName->text().trimmed();
    if (name.isEmpty()) return;

    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();
    const std::string utf8Name = name.toStdString();

    const bool exists = std::any_of(cfg.playlists.begin(), cfg.playlists.end(),
                                    [&](const Config::Playlist& p) { return p.name == utf8Name; });
    if (exists) return;

    Config::Playlist playlist;
    playlist.name = utf8Name;
    cfg.playlists.push_back(std::move(playlist));
    manager.Save();

    m_newPlaylistName->clear();
    RefreshPlaylists();
}

void SettingsWindow::onDeletePlaylist()
{
    const QString name = currentPlaylistName();
    if (name.isEmpty()) return;

    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();
    const std::string utf8Name = name.toStdString();

    cfg.playlists.erase(std::remove_if(cfg.playlists.begin(), cfg.playlists.end(),
                                       [&](const Config::Playlist& p) { return p.name == utf8Name; }),
                        cfg.playlists.end());
    if (cfg.activePlaylist == utf8Name) cfg.activePlaylist.clear();
    manager.Save();

    RefreshPlaylists();
    if (m_app) m_app->RefreshRotationTimer();
}

void SettingsWindow::onAddSelectedToPlaylist()
{
    const QString playlistName = currentPlaylistName();
    if (playlistName.isEmpty() || !m_library) return;

    const QList<QListWidgetItem*> selected = m_library->selectedItems();
    if (selected.isEmpty()) {
        const auto& strings = Localization::Get();
        QMessageBox::information(this, tr8(strings.playlistsHeader), tr8(strings.playlistHint));
        return;
    }

    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();
    const std::string utf8Name = playlistName.toStdString();

    for (auto& playlist : cfg.playlists) {
        if (playlist.name != utf8Name) continue;
        for (const QListWidgetItem* item : selected) {
            const std::string path = item->data(Qt::UserRole).toString().toStdString();
            if (std::find(playlist.paths.begin(), playlist.paths.end(), path) == playlist.paths.end()) {
                playlist.paths.push_back(path);
            }
        }
        break;
    }
    manager.Save();
    onPlaylistSelectionChanged();
}

void SettingsWindow::onRemoveSelectedFromPlaylist()
{
    const QString playlistName = currentPlaylistName();
    if (playlistName.isEmpty() || !m_playlistItems) return;

    const QList<QListWidgetItem*> selected = m_playlistItems->selectedItems();
    if (selected.isEmpty()) return;

    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();
    const std::string utf8Name = playlistName.toStdString();

    for (auto& playlist : cfg.playlists) {
        if (playlist.name != utf8Name) continue;
        for (const QListWidgetItem* item : selected) {
            const std::string path = item->data(Qt::UserRole).toString().toStdString();
            playlist.paths.erase(std::remove(playlist.paths.begin(), playlist.paths.end(), path),
                                 playlist.paths.end());
        }
        break;
    }
    manager.Save();
    onPlaylistSelectionChanged();
}

void SettingsWindow::onActivePlaylistChanged(int index)
{
    if (m_loading || !m_activePlaylist) return;
    auto& manager = Config::ConfigManager::GetInstance();
    manager.GetConfig().activePlaylist = m_activePlaylist->itemData(index).toString().toStdString();
    manager.Save();
    if (m_app) m_app->RefreshRotationTimer();
}

// ---------------------------------------------------------------- Settings tab

QWidget* SettingsWindow::buildSettingsTab()
{
    const auto& strings = Localization::Get();

    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    // Playback
    auto* playbackGroup = new QGroupBox(tr8(strings.performance));
    auto* playbackForm = new QFormLayout(playbackGroup);

    m_fitMode = new QComboBox();
    m_fitMode->addItem(tr8(strings.fitFill), 0);
    m_fitMode->addItem(tr8(strings.fitFitLetterbox), 1);
    m_fitMode->addItem(tr8(strings.fitStretch), 2);
    m_fitMode->addItem(tr8(strings.fitCenter), 3);
    playbackForm->addRow(tr8(strings.fitModeLabel), m_fitMode);

    m_maxFps = new QSpinBox();
    m_maxFps->setRange(0, 240);
    m_maxFps->setSpecialValueText(tr8(strings.fpsAuto));
    playbackForm->addRow(tr8(strings.maxFps), m_maxFps);

    m_hardwareDecode = new QCheckBox(tr8(strings.hwdecLabel));
    playbackForm->addRow(QString(), m_hardwareDecode);
    playbackForm->addRow(QString(), makeHint(tr8(strings.hwdecDesc)));
    layout->addWidget(playbackGroup);

    // Display method
    auto* displayGroup = new QGroupBox(tr8(strings.displayGroup));
    auto* displayLayout = new QVBoxLayout(displayGroup);

    auto* backendForm = new QFormLayout();
    m_backend = new QComboBox();
    m_backend->addItem(tr8(strings.backendAuto), Config::BackendAuto);
    m_backend->addItem(tr8(strings.backendPlasma), Config::BackendPlasma);
    m_backend->addItem(tr8(strings.backendLayerShell), Config::BackendLayerShell);
    backendForm->addRow(tr8(strings.backendLabel), m_backend);

    m_layer = new QComboBox();
    m_layer->addItem(tr8(strings.layerBackground), Config::LayerBackground);
    m_layer->addItem(tr8(strings.layerBottom), Config::LayerBottom);
    backendForm->addRow(tr8(strings.layerLabel), m_layer);
    displayLayout->addLayout(backendForm);

    displayLayout->addWidget(makeHint(tr8(strings.backendPlasmaDesc)));
    displayLayout->addWidget(makeHint(tr8(strings.backendLayerDesc)));
    displayLayout->addWidget(makeHint(tr8(strings.layerHint)));

    m_plasmaStatus = new QLabel();
    m_plasmaStatus->setWordWrap(true);
    displayLayout->addWidget(m_plasmaStatus);

    auto* plasmaButtons = new QHBoxLayout();
    m_plasmaActivate = new QPushButton(tr8(strings.plasmaActivateBtn));
    connect(m_plasmaActivate, &QPushButton::clicked, this, &SettingsWindow::onPlasmaActivate);
    m_plasmaRestore = new QPushButton(tr8(strings.plasmaRestoreBtn));
    connect(m_plasmaRestore, &QPushButton::clicked, this, &SettingsWindow::onPlasmaRestore);
    plasmaButtons->addWidget(m_plasmaActivate);
    plasmaButtons->addWidget(m_plasmaRestore);
    plasmaButtons->addStretch(1);
    displayLayout->addLayout(plasmaButtons);
    layout->addWidget(displayGroup);

    // Power saving
    auto* powerGroup = new QGroupBox(tr8(strings.powerSaving));
    auto* powerLayout = new QVBoxLayout(powerGroup);
    m_pauseFullscreen = new QCheckBox(tr8(strings.pauseFullscreen));
    powerLayout->addWidget(m_pauseFullscreen);
    powerLayout->addWidget(makeHint(tr8(strings.pauseFullscreenDesc)));
    m_pauseBattery = new QCheckBox(tr8(strings.pauseBattery));
    powerLayout->addWidget(m_pauseBattery);
    powerLayout->addWidget(makeHint(tr8(strings.pauseBatteryDesc)));
    m_pauseHidden = new QCheckBox(tr8(strings.pauseHidden));
    powerLayout->addWidget(m_pauseHidden);
    powerLayout->addWidget(makeHint(tr8(strings.pauseHiddenDesc)));
    layout->addWidget(powerGroup);

    // System
    auto* systemGroup = new QGroupBox(tr8(strings.systemGroup));
    auto* systemLayout = new QVBoxLayout(systemGroup);
    m_autostart = new QCheckBox(tr8(strings.runAtStartup));
    systemLayout->addWidget(m_autostart);
    systemLayout->addWidget(makeHint(tr8(strings.runAtStartupDesc)));

    auto* languageRow = new QHBoxLayout();
    languageRow->addWidget(new QLabel(tr8(strings.languageLabel)));
    m_language = new QComboBox();
    m_language->addItem(tr8(strings.langAuto), QString());
    m_language->addItem(tr8(strings.langTurkish), QStringLiteral("tr"));
    m_language->addItem(tr8(strings.langEnglish), QStringLiteral("en"));
    languageRow->addWidget(m_language);
    languageRow->addStretch(1);
    systemLayout->addLayout(languageRow);
    systemLayout->addWidget(makeHint(tr8(strings.restartHint)));
    layout->addWidget(systemGroup);

    layout->addStretch(1);

    // The tab is taller than the window on small screens, so it scrolls.
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(page);
    return scrollArea;
}

void SettingsWindow::loadSettingsIntoUi()
{
    m_loading = true;
    const auto& cfg = Config::ConfigManager::GetInstance().GetConfig();

    m_fitMode->setCurrentIndex(qMax(0, m_fitMode->findData(cfg.fitMode)));
    m_maxFps->setValue(cfg.maxFPS);
    m_hardwareDecode->setChecked(cfg.hardwareDecode);
    m_backend->setCurrentIndex(qMax(0, m_backend->findData(cfg.backend)));
    m_layer->setCurrentIndex(qMax(0, m_layer->findData(cfg.layer)));
    m_pauseFullscreen->setChecked(cfg.pauseOnFullscreen);
    m_pauseBattery->setChecked(cfg.pauseOnBattery);
    m_pauseHidden->setChecked(cfg.pauseWhenHidden);
    m_autostart->setChecked(Autostart::IsEnabled());
    m_language->setCurrentIndex(qMax(0, m_language->findData(QString::fromStdString(cfg.language))));

    m_rotateEnabled->setChecked(cfg.playlistEnabled);
    m_rotateInterval->setValue(qMax(1, cfg.playlistIntervalMin));
    m_rotateShuffle->setChecked(cfg.playlistShuffle);

    m_loading = false;
}

void SettingsWindow::connectSettingSignals()
{
    auto save = [this]() {
        if (m_loading) return;
        auto& manager = Config::ConfigManager::GetInstance();
        auto& cfg = manager.GetConfig();

        cfg.fitMode = m_fitMode->currentData().toInt();
        cfg.maxFPS = m_maxFps->value();
        cfg.hardwareDecode = m_hardwareDecode->isChecked();
        cfg.pauseOnFullscreen = m_pauseFullscreen->isChecked();
        cfg.pauseOnBattery = m_pauseBattery->isChecked();
        cfg.pauseWhenHidden = m_pauseHidden->isChecked();
        cfg.playlistEnabled = m_rotateEnabled->isChecked();
        cfg.playlistIntervalMin = m_rotateInterval->value();
        cfg.playlistShuffle = m_rotateShuffle->isChecked();
        cfg.language = m_language->currentData().toString().toStdString();
        manager.Save();

        if (m_app) {
            m_app->ApplyPlaybackSettings();
            m_app->RefreshRotationTimer();
        }
    };

    connect(m_fitMode, &QComboBox::currentIndexChanged, this, [save](int) { save(); });
    connect(m_maxFps, &QSpinBox::valueChanged, this, [save](int) { save(); });
    connect(m_hardwareDecode, &QCheckBox::toggled, this, [save](bool) { save(); });
    connect(m_pauseFullscreen, &QCheckBox::toggled, this, [save](bool) { save(); });
    connect(m_pauseBattery, &QCheckBox::toggled, this, [save](bool) { save(); });
    connect(m_pauseHidden, &QCheckBox::toggled, this, [save](bool) { save(); });
    connect(m_rotateEnabled, &QCheckBox::toggled, this, [save](bool) { save(); });
    connect(m_rotateInterval, &QSpinBox::valueChanged, this, [save](int) { save(); });
    connect(m_rotateShuffle, &QCheckBox::toggled, this, [save](bool) { save(); });
    connect(m_language, &QComboBox::currentIndexChanged, this, [save](int) { save(); });

    connect(m_activePlaylist, &QComboBox::currentIndexChanged, this,
            &SettingsWindow::onActivePlaylistChanged);

    connect(m_autostart, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_loading) return;
        QString error;
        if (!Autostart::SetEnabled(checked, &error)) {
            const auto& strings = Localization::Get();
            QMessageBox::warning(this, tr8(strings.errorTitle), error);
        }
    });

    // Backend and layer changes recreate the surfaces, so they are handled apart from
    // the plain "write the value and carry on" settings above.
    connect(m_backend, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_loading) return;
        auto& manager = Config::ConfigManager::GetInstance();
        manager.GetConfig().backend = m_backend->currentData().toInt();
        manager.Save();
        if (m_app) m_app->RebuildSurfaces();
        RefreshPlasmaStatus();
    });
    connect(m_layer, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_loading) return;
        auto& manager = Config::ConfigManager::GetInstance();
        manager.GetConfig().layer = m_layer->currentData().toInt();
        manager.Save();
        if (m_app) m_app->RebuildSurfaces();
    });
}

void SettingsWindow::RefreshPlasmaStatus()
{
    if (!m_plasmaStatus) return;
    const auto& strings = Localization::Get();

    const bool plasma = PlasmaIntegration::IsPlasmaSession() && PlasmaIntegration::IsPlasmaShellRunning();
    m_plasmaActivate->setEnabled(plasma);
    m_plasmaRestore->setEnabled(plasma);

    if (!plasma) {
        m_plasmaStatus->setText(tr8(strings.plasmaNotRunning));
        return;
    }
    m_plasmaStatus->setText(PlasmaIntegration::IsActive() ? tr8(strings.plasmaActive)
                                                          : tr8(strings.plasmaInactive));

    const bool layerShellBackend = m_app && m_app->EffectiveBackend() == Config::BackendLayerShell;
    m_layer->setEnabled(layerShellBackend);
}

void SettingsWindow::onPlasmaActivate()
{
    const auto& strings = Localization::Get();
    QString error;
    if (!PlasmaIntegration::Activate(&error)) {
        QMessageBox::warning(this, tr8(strings.errorTitle),
                             error.isEmpty() ? tr8(strings.plasmaInstallFailed) : error);
    } else if (m_app) {
        m_app->SetPlasmaOwnership(true);
    }
    if (m_app) m_app->PushPlasmaState();
    RefreshPlasmaStatus();
}

void SettingsWindow::onPlasmaRestore()
{
    const auto& strings = Localization::Get();
    QString error;
    // Give the desktop back before dropping ownership, so the watchdog cannot race in
    // and re-take it between the two.
    if (m_app) m_app->SetPlasmaOwnership(false);
    if (!PlasmaIntegration::Restore(&error) && !error.isEmpty()) {
        QMessageBox::warning(this, tr8(strings.errorTitle), error);
    }
    RefreshPlasmaStatus();
}

// ---------------------------------------------------------------- About tab

QWidget* SettingsWindow::buildAboutTab()
{
    const auto& strings = Localization::Get();

    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    auto* title = new QLabel(tr8(strings.aboutTitle));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    title->setFont(titleFont);
    layout->addWidget(title);

    layout->addWidget(new QLabel(tr8(strings.aboutBody)));
    layout->addWidget(new QLabel(QStringLiteral("%1: %2")
                                     .arg(tr8(strings.versionLabel), QStringLiteral(APP_VERSION_STRING))));

    auto* link = new QLabel(QStringLiteral("<a href=\"https://github.com/hamzaturhan/WallpaperAnim\">%1</a>")
                                .arg(tr8(strings.projectPage)));
    link->setOpenExternalLinks(true);
    layout->addWidget(link);

    layout->addWidget(makeHint(QStringLiteral("config: %1")
                                   .arg(QString::fromStdString(
                                       Config::ConfigManager::GetInstance().Path()))));
    layout->addStretch(1);
    return page;
}

void SettingsWindow::closeEvent(QCloseEvent* event)
{
    // Closing the window only hides it; the wallpaper keeps running from the tray.
    auto& manager = Config::ConfigManager::GetInstance();
    auto& cfg = manager.GetConfig();

    if (!cfg.hideMinimizeWarning) {
        const auto& strings = Localization::Get();
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr8(strings.minimizeWarningTitle));
        box.setText(tr8(strings.minimizeWarningDesc));
        auto* dontShow = box.addButton(tr8(strings.doNotShowAgain), QMessageBox::ActionRole);
        box.addButton(tr8(strings.okBtn), QMessageBox::AcceptRole);
        box.exec();
        if (box.clickedButton() == dontShow) {
            cfg.hideMinimizeWarning = true;
        }
    }

    if (cfg.isFirstRun) {
        cfg.isFirstRun = false;
    }
    manager.Save();

    hide();
    event->ignore();
}
