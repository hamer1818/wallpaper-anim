#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "App.xaml.h"
#include "src/config.h"
#include "src/localization.h"
#include "src/utils/thumbnail_generator.h"
#include "src/utils/youtube_downloader.h"
#include "src/utils/update_checker.h"
#include "src/version.h"
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <microsoft.ui.xaml.window.h>
#include <filesystem>
#include <algorithm>

void LogApp(const std::string& msg);

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;

namespace winrt::WallpaperAnimWinUI::implementation
{
    MainWindow::MainWindow()
    {
        LogApp("MainWindow: Entering constructor");
        InitializeComponent();
        LogApp("MainWindow: InitializeComponent done");
        
        try {
            SystemBackdrop(winrt::Microsoft::UI::Xaml::Media::MicaBackdrop());
            LogApp("MainWindow: MicaBackdrop initialized");
        } catch (...) {
            LogApp("MainWindow: MicaBackdrop failed (handled)");
        }

        // Show the real app version from a single source of truth (version.h)
        TxtAppVersion().Text(L"v" APP_VERSION_STRING_W);

        LogApp("MainWindow: Loading Config");
        // Initialize Settings from Config
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        
        LogApp("MainWindow: Setting TglBattery");
        TglBattery().IsOn(config.pauseOnBattery);
        LogApp("MainWindow: Setting TglFullscreen");
        TglFullscreen().IsOn(config.pauseOnFullscreen);
        
        LogApp("MainWindow: Checking Startup");
        // Startup check
        wchar_t runPath[MAX_PATH];
        GetModuleFileName(NULL, runPath, MAX_PATH);
        
        HKEY hKey;
        bool isStartup = false;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t value[MAX_PATH];
            DWORD value_length = MAX_PATH;
            if (RegQueryValueEx(hKey, L"WallpaperAnim", NULL, NULL, (LPBYTE)&value, &value_length) == ERROR_SUCCESS) {
                isStartup = true;
            }
            RegCloseKey(hKey);
        }
        TglStartup().IsOn(isStartup);

        LogApp("MainWindow: Setting FPS selector");
        // Options: 0 = Maximum (monitor Hz), 1 = 60 FPS, 2 = 30 FPS.
        int fpsIdx = 0;
        if (config.maxFPS == 60) fpsIdx = 1;
        else if (config.maxFPS == 30) fpsIdx = 2;
        else fpsIdx = 0; // 0 or anything else = Maximum
        CmbFps().SelectedIndex(fpsIdx);

        // Fit mode: 0=Fill, 1=Fit, 2=Stretch, 3=Center (matches ComboBox item order).
        int fitIdx = config.fitMode;
        if (fitIdx < 0 || fitIdx > 3) fitIdx = 0;
        CmbFitMode().SelectedIndex(fitIdx);

        // Playlist / auto-rotation.
        TglPlaylist().IsOn(config.playlistEnabled);
        TglShuffle().IsOn(config.playlistShuffle);
        int intervalIdx = 2; // default 30 min
        switch (config.playlistIntervalMin) {
            case 5:  intervalIdx = 0; break;
            case 15: intervalIdx = 1; break;
            case 30: intervalIdx = 2; break;
            case 60: intervalIdx = 3; break;
        }
        CmbPlaylistInterval().SelectedIndex(intervalIdx);

        LogApp("MainWindow: Setting language selector");
        if (config.language == L"en") CmbLanguage().SelectedIndex(1);
        else CmbLanguage().SelectedIndex(0);

        this->AppWindow().Closing({ this, &MainWindow::OnAppWindowClosing });

        LogApp("MainWindow: Loading Localization");
        LoadLocalization();
        LogApp("MainWindow: Refreshing Library");
        RefreshLibrary();
        LogApp("MainWindow: Exiting constructor");

        // Set Window Icon
        HWND hwnd = GetWindowHandle();
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101)); // IDI_APP_ICON
        if (hwnd && hIcon) {
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            LogApp("MainWindow: Window icon set via WM_SETICON");
        }
    }

    void MainWindow::OnAppWindowClosing(winrt::Microsoft::UI::Windowing::AppWindow const& sender, winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args)
    {
        args.Cancel(true);
        if (!Config::ConfigManager::GetInstance().GetConfig().hideMinimizeWarning) {
            ShowMinimizeWarningDialog();
        } else {
            sender.Hide();
        }
    }

    winrt::fire_and_forget MainWindow::ShowMinimizeWarningDialog()
    {
        auto lifetime = get_strong();
        auto& strings = Localization::Get();

        winrt::Microsoft::UI::Xaml::Controls::ContentDialog dialog;
        dialog.XamlRoot(this->Content().XamlRoot());
        dialog.Title(box_value(winrt::to_hstring(strings.minimizeWarningTitle)));
        dialog.PrimaryButtonText(winrt::to_hstring(strings.okBtn));

        winrt::Microsoft::UI::Xaml::Controls::StackPanel sp;
        sp.Spacing(12);

        winrt::Microsoft::UI::Xaml::Controls::TextBlock tb;
        tb.Text(winrt::to_hstring(strings.minimizeWarningDesc));
        tb.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
        sp.Children().Append(tb);

        winrt::Microsoft::UI::Xaml::Controls::CheckBox cb;
        cb.Content(box_value(winrt::to_hstring(strings.doNotShowAgain)));
        sp.Children().Append(cb);

        dialog.Content(sp);

        co_await dialog.ShowAsync();

        auto isChecked = cb.IsChecked();
        if (isChecked && isChecked.GetBoolean()) {
            auto& config = Config::ConfigManager::GetInstance().GetConfig();
            config.hideMinimizeWarning = true;
            Config::ConfigManager::GetInstance().Save();
        }

        this->AppWindow().Hide();
    }

    HWND MainWindow::GetWindowHandle()
    {
        auto windowNative = this->try_as<::IWindowNative>();
        HWND hwnd = nullptr;
        if (windowNative) {
            windowNative->get_WindowHandle(&hwnd);
        }
        return hwnd;
    }

    void MainWindow::LoadLocalization()
    {
        auto& strings = Localization::Get();

        // Title
        Title(winrt::to_hstring(strings.settingsTitle));

        // Sidebar tabs
        TxtTabLibrary().Text(winrt::to_hstring(strings.tabLibrary));
        TxtTabAdd().Text(winrt::to_hstring(strings.tabAddNew));
        TxtTabSettings().Text(winrt::to_hstring(strings.tabSettings));

        // Headers
        TxtLibraryHeader().Text(winrt::to_hstring(strings.tabLibrary));
        TxtAddHeader().Text(winrt::to_hstring(strings.tabAddNew));
        TxtSettingsHeader().Text(winrt::to_hstring(strings.tabSettings));

        // Local file UI
        TxtLocalTitle().Text(winrt::to_hstring(strings.localFile));
        TxtLocalDesc().Text(winrt::to_hstring(strings.browsePC));
        BtnBrowse().Content(box_value(winrt::to_hstring(strings.browseBtn)));

        // YouTube UI
        TxtYoutubeTitle().Text(winrt::to_hstring(strings.youtubeVideo));
        BtnDownload().Content(box_value(winrt::to_hstring(strings.downloadPlayBtn)));
        TxtYoutubeUrl().PlaceholderText(winrt::to_hstring(strings.youtubePlaceholder));

        // Settings toggles
        TglBattery().Header(box_value(winrt::to_hstring(strings.pauseBattery)));
        TxtTglBatteryDesc().Text(winrt::to_hstring(strings.pauseBatteryDesc));
        TglFullscreen().Header(box_value(winrt::to_hstring(strings.pauseFullscreen)));
        TxtTglFullscreenDesc().Text(winrt::to_hstring(strings.pauseFullscreenDesc));
        TglStartup().Header(box_value(winrt::to_hstring(strings.runAtStartup)));
        TxtTglStartupDesc().Text(winrt::to_hstring(strings.runAtStartupDesc));
        TxtFpsLabel().Text(winrt::to_hstring(strings.maxFps));
        TxtFitModeLabel().Text(winrt::to_hstring(strings.fitModeLabel));
        TglPlaylist().Header(box_value(winrt::to_hstring(strings.playlistTitle)));
        TxtPlaylistDesc().Text(winrt::to_hstring(strings.playlistDesc));
        TglShuffle().Header(box_value(winrt::to_hstring(strings.playlistShuffle)));
        TxtPlaylistIntervalLabel().Text(winrt::to_hstring(strings.playlistIntervalLabel));
        TxtPlaylistsHeader().Text(winrt::to_hstring(strings.playlistsHeader));
        TxtActivePlaylistLabel().Text(winrt::to_hstring(strings.activePlaylistLabel));
        TxtNewPlaylistName().PlaceholderText(winrt::to_hstring(strings.newPlaylistPlaceholder));
        BtnCreatePlaylist().Content(box_value(winrt::to_hstring(strings.createBtn)));
        BtnDeletePlaylist().Content(box_value(winrt::to_hstring(strings.deleteBtn)));
        TxtPlaylistHint().Text(winrt::to_hstring(strings.playlistHint));
        TxtLanguageLabel().Text(winrt::to_hstring(strings.languageLabel));

        // Update UI
        TxtUpdateHeader().Text(winrt::to_hstring(strings.checkUpdate));
        BtnUpdate().Content(box_value(winrt::to_hstring(strings.checkUpdate)));

        // Repopulate the playlist selector (its "Whole Library" entry is localized).
        RefreshPlaylistUI();
    }

    void MainWindow::RefreshLibrary()
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        LibraryGridView().Items().Clear();

        for (const auto& item : config.history) {
            auto rootGrid = winrt::Microsoft::UI::Xaml::Controls::Grid();
            rootGrid.Width(220);
            rootGrid.Margin({0,0,12,12});
            rootGrid.CornerRadius({8,8,8,8});
            
            // Background brush for the root grid
            auto color = winrt::Microsoft::UI::ColorHelper::FromArgb(255, 45, 45, 48); // #2D2D30
            auto bgBrush = winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(color);
            rootGrid.Background(bgBrush);

            auto rowDefs = rootGrid.RowDefinitions();
            auto r0 = winrt::Microsoft::UI::Xaml::Controls::RowDefinition();
            r0.Height({124, winrt::Microsoft::UI::Xaml::GridUnitType::Pixel});
            auto r1 = winrt::Microsoft::UI::Xaml::Controls::RowDefinition();
            r1.Height({1, winrt::Microsoft::UI::Xaml::GridUnitType::Auto});
            rowDefs.Append(r0);
            rowDefs.Append(r1);

            auto img = winrt::Microsoft::UI::Xaml::Controls::Image();
            img.Stretch(winrt::Microsoft::UI::Xaml::Media::Stretch::UniformToFill);
            img.Height(124);
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetRow(img, 0);

            if (!item.thumbPath.empty() && std::filesystem::exists(item.thumbPath)) {
                auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
                bitmap.UriSource(winrt::Windows::Foundation::Uri(L"file:///" + winrt::hstring(item.thumbPath)));
                img.Source(bitmap);
            }

            auto lowerGrid = winrt::Microsoft::UI::Xaml::Controls::Grid();
            lowerGrid.Padding({8,8,8,8});
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetRow(lowerGrid, 1);
            auto colDefs = lowerGrid.ColumnDefinitions();
            auto c0 = winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition();
            c0.Width({1, winrt::Microsoft::UI::Xaml::GridUnitType::Star});
            auto c1 = winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition();
            c1.Width({1, winrt::Microsoft::UI::Xaml::GridUnitType::Auto});
            colDefs.Append(c0);
            colDefs.Append(c1);

            auto tb = winrt::Microsoft::UI::Xaml::Controls::TextBlock();
            tb.Text(winrt::hstring(item.name));
            tb.TextTrimming(winrt::Microsoft::UI::Xaml::TextTrimming::CharacterEllipsis);
            tb.VerticalAlignment(winrt::Microsoft::UI::Xaml::VerticalAlignment::Center);
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetColumn(tb, 0);

            // Buttons live in a horizontal strip in the auto-width column.
            auto btnStrip = winrt::Microsoft::UI::Xaml::Controls::StackPanel();
            btnStrip.Orientation(winrt::Microsoft::UI::Xaml::Controls::Orientation::Horizontal);
            winrt::Microsoft::UI::Xaml::Controls::Grid::SetColumn(btnStrip, 1);

            // "Add to / remove from the active playlist" toggle — only shown when a
            // playlist is selected. A check mark means the item is already in it.
            bool inPlaylist = false;
            if (!config.activePlaylist.empty()) {
                for (const auto& pl : config.playlists) {
                    if (pl.name == config.activePlaylist) {
                        inPlaylist = std::find(pl.paths.begin(), pl.paths.end(), item.path) != pl.paths.end();
                        break;
                    }
                }
                auto plBtn = winrt::Microsoft::UI::Xaml::Controls::Button();
                plBtn.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(winrt::Microsoft::UI::Colors::Transparent()));
                plBtn.BorderThickness({0,0,0,0});
                plBtn.Padding({4,4,4,4});
                plBtn.Tag(box_value(winrt::hstring(item.path)));
                plBtn.Click({this, &MainWindow::BtnTogglePlaylistItem_Click});
                auto plSym = winrt::Microsoft::UI::Xaml::Controls::SymbolIcon();
                plSym.Symbol(inPlaylist ? winrt::Microsoft::UI::Xaml::Controls::Symbol::Accept
                                        : winrt::Microsoft::UI::Xaml::Controls::Symbol::Add);
                plBtn.Content(plSym);
                btnStrip.Children().Append(plBtn);
            }

            auto btn = winrt::Microsoft::UI::Xaml::Controls::Button();
            btn.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(winrt::Microsoft::UI::Colors::Transparent()));
            btn.BorderThickness({0,0,0,0});
            btn.Padding({4,4,4,4});
            btn.Tag(box_value(winrt::hstring(item.path)));
            btn.Click({this, &MainWindow::BtnDeleteWallpaper_Click});

            auto sym = winrt::Microsoft::UI::Xaml::Controls::SymbolIcon();
            sym.Symbol(winrt::Microsoft::UI::Xaml::Controls::Symbol::Delete);
            btn.Content(sym);
            btnStrip.Children().Append(btn);

            lowerGrid.Children().Append(tb);
            lowerGrid.Children().Append(btnStrip);

            rootGrid.Children().Append(img);
            rootGrid.Children().Append(lowerGrid);
            
            // Set the path to the root Grid tag so we can read it on click
            rootGrid.Tag(box_value(winrt::hstring(item.path)));

            LibraryGridView().Items().Append(rootGrid);
        }
    }

    void MainWindow::Tab_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto button = sender.as<Button>();
        auto tag = winrt::unbox_value<hstring>(button.Tag());

        GridLibrary().Visibility(tag == L"library" ? Visibility::Visible : Visibility::Collapsed);
        GridAdd().Visibility(tag == L"add" ? Visibility::Visible : Visibility::Collapsed);
        GridSettings().Visibility(tag == L"settings" ? Visibility::Visible : Visibility::Collapsed);
    }

    void MainWindow::LibraryGridView_ItemClick(IInspectable const&, ItemClickEventArgs const& e)
    {
        auto clickedGrid = e.ClickedItem().as<winrt::Microsoft::UI::Xaml::Controls::Grid>();
        auto path = winrt::unbox_value<hstring>(clickedGrid.Tag());

        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        config.lastVideoPath = path.c_str();
        Config::ConfigManager::GetInstance().Save();

        // Notify background window
        auto appImpl = App::GetInstance();
        PostMessage(appImpl->GetWallpaperHwnd(), WM_APP_CONFIG_CHANGED, 0, 0);
    }

    void MainWindow::BtnDeleteWallpaper_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto button = sender.as<Button>();
        auto path = winrt::unbox_value<hstring>(button.Tag());

        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        auto it = std::remove_if(config.history.begin(), config.history.end(), [&](const auto& item) {
            return item.path == path.c_str();
        });
        if (it != config.history.end()) {
            // Clean up files owned by the app before dropping the history entries.
            for (auto delIt = it; delIt != config.history.end(); ++delIt) {
                std::error_code ec;
                // Thumbnails are always generated by the app, so they are safe to remove.
                if (!delIt->thumbPath.empty()) {
                    std::filesystem::remove(delIt->thumbPath, ec);
                }
                // Only remove media the app downloaded itself (YouTube); never the user's own files.
                if (delIt->type == 3 && !delIt->path.empty()) {
                    std::filesystem::remove(delIt->path, ec);
                }
            }

            config.history.erase(it, config.history.end());
            Config::ConfigManager::GetInstance().Save();
            RefreshLibrary();
        }
    }

    winrt::fire_and_forget MainWindow::BtnBrowse_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
        auto initializeWithWindow = picker.as<::IInitializeWithWindow>();
        initializeWithWindow->Initialize(GetWindowHandle());

        picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::Thumbnail);
        picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::VideosLibrary);
        picker.FileTypeFilter().Append(L".mp4");
        picker.FileTypeFilter().Append(L".avi");
        picker.FileTypeFilter().Append(L".mkv");
        picker.FileTypeFilter().Append(L".gif");
        picker.FileTypeFilter().Append(L".hlsl");

        auto file = co_await picker.PickSingleFileAsync();
        if (file) {
            std::wstring filePath = file.Path().c_str();
            std::wstring fileName = file.Name().c_str();

            // Generate thumbnail using native generator helper
            std::wstring thumbPath = Utils::ThumbnailGenerator::GenerateThumbnail(filePath);

            // Save to config
            auto& config = Config::ConfigManager::GetInstance().GetConfig();
            config.lastVideoPath = filePath;

            // Add to history
            Config::WallpaperHistoryItem item;
            item.name = fileName;
            item.path = filePath;
            item.thumbPath = thumbPath;
            item.type = 0; // Default type Video
            
            // Remove old duplicate
            auto it = std::remove_if(config.history.begin(), config.history.end(), [&](const auto& hi) {
                return hi.path == filePath;
            });
            if (it != config.history.end()) {
                config.history.erase(it, config.history.end());
            }

            config.history.insert(config.history.begin(), item);
            Config::ConfigManager::GetInstance().Save();

            // Refresh UI
            RefreshLibrary();

            // Notify app
            auto appImpl = App::GetInstance();
            PostMessage(appImpl->GetWallpaperHwnd(), WM_APP_CONFIG_CHANGED, 0, 0);
        }
    }

    void MainWindow::BtnDownload_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_isDownloading) return;

        hstring url = TxtYoutubeUrl().Text();
        if (url.empty()) return;

        m_isDownloading = true;
        BtnDownload().IsEnabled(false);
        PrgDownload().Visibility(Visibility::Visible);
        TxtDownloadStatus().Visibility(Visibility::Visible);
        TxtDownloadStatus().Text(winrt::to_hstring(Localization::Get().downloadStarting));

        std::wstring downloadUrl = url.c_str();
        auto dispatcher = this->DispatcherQueue();

        // Convert wstring to string for Downloader API
        int len = WideCharToMultiByte(CP_UTF8, 0, downloadUrl.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string urlMbs(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, downloadUrl.c_str(), -1, &urlMbs[0], len, nullptr, nullptr);
        if (!urlMbs.empty() && urlMbs.back() == '\0') {
            urlMbs.pop_back();
        }

        std::thread([this, urlMbs, dispatcher]() {
            auto progress = std::make_shared<std::atomic<float>>(0.0f);

            // 1. Fetch qualities to choose best
            Utils::YouTubeDownloader::FetchResolutionsAsync(urlMbs, [this, urlMbs, progress, dispatcher](bool success, const std::vector<Utils::YouTubeResolution>& resolutions, const std::string& title, const std::string& error_msg) {
                if (!success || resolutions.empty()) {
                    dispatcher.TryEnqueue([this]() {
                        m_isDownloading = false;
                        BtnDownload().IsEnabled(true);
                        PrgDownload().Visibility(Visibility::Collapsed);
                        TxtDownloadStatus().Text(winrt::to_hstring(Localization::Get().fetchInfoFailed));
                    });
                    return;
                }

                // Resolution/codec is chosen inside DownloadAsync (H.264 within the
                // screen height) so the file plays on every Windows version.
                int len = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
                std::wstring wTitle(len, 0);
                MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, &wTitle[0], len);
                if (!wTitle.empty() && wTitle.back() == L'\0') {
                    wTitle.pop_back();
                }

                // Start progress checking loop
                std::thread([this, progress, dispatcher]() {
                    while (m_isDownloading) {
                        float pct = progress->load();
                        dispatcher.TryEnqueue([this, pct]() {
                            PrgDownload().Value(pct * 100.0f);
                            std::wstring pctText = std::to_wstring((int)(pct * 100.0f)) + L"%";
                            TxtDownloadStatus().Text(winrt::to_hstring(Localization::Get().downloading) + winrt::hstring(pctText));
                        });
                        Sleep(500);
                    }
                }).detach();

                // 2. Download the video (0 = auto-detect screen height as the cap)
                Utils::YouTubeDownloader::DownloadAsync(urlMbs, 0, progress.get(), [this, wTitle, progress, dispatcher](bool dSuccess, const std::wstring& outPath, const std::string& dErrorMsg) {
                    dispatcher.TryEnqueue([this, dSuccess, outPath, wTitle]() {
                        m_isDownloading = false;
                        BtnDownload().IsEnabled(true);
                        PrgDownload().Visibility(Visibility::Collapsed);

                        if (dSuccess) {
                            TxtDownloadStatus().Text(winrt::to_hstring(Localization::Get().downloadComplete));

                            // Generate thumbnail
                            std::wstring thumbPath = Utils::ThumbnailGenerator::GenerateThumbnail(outPath);

                            // Add to config
                            auto& config = Config::ConfigManager::GetInstance().GetConfig();
                            config.lastVideoPath = outPath;

                            Config::WallpaperHistoryItem item;
                            item.name = wTitle.empty() ? L"YouTube Video" : wTitle;
                            item.path = outPath;
                            item.thumbPath = thumbPath;
                            item.type = 3; // YouTube type

                            config.history.insert(config.history.begin(), item);
                            Config::ConfigManager::GetInstance().Save();

                            RefreshLibrary();

                            // Notify app
                            auto appImpl = App::GetInstance();
                            PostMessage(appImpl->GetWallpaperHwnd(), WM_APP_CONFIG_CHANGED, 0, 0);
                        } else {
                            TxtDownloadStatus().Text(winrt::to_hstring(Localization::Get().videoDownloadFailed));
                        }
                    });
                });
            });
        }).detach();
    }

    void MainWindow::TglBattery_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        config.pauseOnBattery = TglBattery().IsOn();
        Config::ConfigManager::GetInstance().Save();
    }

    void MainWindow::TglFullscreen_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        config.pauseOnFullscreen = TglFullscreen().IsOn();
        Config::ConfigManager::GetInstance().Save();
    }

    void MainWindow::TglStartup_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        wchar_t runPath[MAX_PATH];
        GetModuleFileName(nullptr, runPath, MAX_PATH);

        HKEY hKey;
        if (TglStartup().IsOn()) {
            if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                RegSetValueEx(hKey, L"WallpaperAnim", 0, REG_SZ, (BYTE*)runPath, (lstrlen(runPath) + 1) * sizeof(wchar_t));
                RegCloseKey(hKey);
            }
        } else {
            if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValue(hKey, L"WallpaperAnim");
                RegCloseKey(hKey);
            }
        }
    }

    void MainWindow::CmbLanguage_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        config.language = CmbLanguage().SelectedIndex() == 0 ? L"tr" : L"en";
        Config::ConfigManager::GetInstance().Save();
        LoadLocalization();
    }

    void MainWindow::CmbFps_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        int fps = 0; // Maximum (monitor Hz)
        switch (CmbFps().SelectedIndex()) {
            case 0: fps = 0;  break; // Maximum
            case 1: fps = 60; break;
            case 2: fps = 30; break;
        }
        config.maxFPS = fps;
        Config::ConfigManager::GetInstance().Save();
        // App::RenderLoop re-reads config.maxFPS on its next 1-second check, so the new
        // frame rate takes effect automatically within a second — no restart needed.
    }

    void MainWindow::CmbFitMode_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        int idx = CmbFitMode().SelectedIndex();
        if (idx < 0) return;
        config.fitMode = idx; // 0=Fill, 1=Fit, 2=Stretch, 3=Center
        Config::ConfigManager::GetInstance().Save();
        // DX11Renderer::SetFitScale reads config.fitMode live each frame, so the change
        // is visible on the next rendered frame — no reload/restart needed.
    }

    void MainWindow::TglPlaylist_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        config.playlistEnabled = TglPlaylist().IsOn();
        Config::ConfigManager::GetInstance().Save();
        // App::RenderLoop re-reads playlist settings on its next 1-second check.
    }

    void MainWindow::TglShuffle_Toggled(IInspectable const&, RoutedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        config.playlistShuffle = TglShuffle().IsOn();
        Config::ConfigManager::GetInstance().Save();
    }

    void MainWindow::CmbPlaylistInterval_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        int minutes = 30;
        switch (CmbPlaylistInterval().SelectedIndex()) {
            case 0: minutes = 5;  break;
            case 1: minutes = 15; break;
            case 2: minutes = 30; break;
            case 3: minutes = 60; break;
        }
        config.playlistIntervalMin = minutes;
        Config::ConfigManager::GetInstance().Save();
    }

    // Rebuilds the active-playlist ComboBox from config ("Whole Library" + each playlist)
    // and reselects the active one. Guarded so it doesn't trigger the selection handler.
    void MainWindow::RefreshPlaylistUI()
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        auto& strings = Localization::Get();

        m_suppressPlaylistEvents = true;
        CmbActivePlaylist().Items().Clear();
        {
            auto ci = winrt::Microsoft::UI::Xaml::Controls::ComboBoxItem();
            ci.Content(box_value(winrt::to_hstring(strings.allLibraryItem)));
            CmbActivePlaylist().Items().Append(ci);
        }
        int selectedIdx = 0;
        for (size_t i = 0; i < config.playlists.size(); ++i) {
            auto ci = winrt::Microsoft::UI::Xaml::Controls::ComboBoxItem();
            ci.Content(box_value(winrt::hstring(config.playlists[i].name)));
            CmbActivePlaylist().Items().Append(ci);
            if (config.playlists[i].name == config.activePlaylist) selectedIdx = (int)(i + 1);
        }
        CmbActivePlaylist().SelectedIndex(selectedIdx);
        m_suppressPlaylistEvents = false;
    }

    void MainWindow::CmbActivePlaylist_SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (m_suppressPlaylistEvents) return;
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        int idx = CmbActivePlaylist().SelectedIndex();
        if (idx <= 0) {
            config.activePlaylist = L"";
        } else if (idx - 1 < (int)config.playlists.size()) {
            config.activePlaylist = config.playlists[idx - 1].name;
        }
        Config::ConfigManager::GetInstance().Save();
        RefreshLibrary(); // library cards' +/✓ buttons reflect the newly-selected list
    }

    void MainWindow::BtnCreatePlaylist_Click(IInspectable const&, RoutedEventArgs const&)
    {
        std::wstring name = TxtNewPlaylistName().Text().c_str();
        // Trim surrounding whitespace.
        size_t b = name.find_first_not_of(L" \t\r\n");
        size_t e = name.find_last_not_of(L" \t\r\n");
        name = (b == std::wstring::npos) ? L"" : name.substr(b, e - b + 1);
        if (name.empty()) return;

        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        bool exists = false;
        for (const auto& pl : config.playlists) if (pl.name == name) { exists = true; break; }
        if (!exists) {
            Config::Playlist p;
            p.name = name;
            config.playlists.push_back(p);
        }
        config.activePlaylist = name; // select it so the user can start adding items
        Config::ConfigManager::GetInstance().Save();

        TxtNewPlaylistName().Text(L"");
        RefreshPlaylistUI();
        RefreshLibrary();
    }

    void MainWindow::BtnDeletePlaylist_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        if (config.activePlaylist.empty()) return; // "Whole Library" isn't deletable
        config.playlists.erase(
            std::remove_if(config.playlists.begin(), config.playlists.end(),
                [&](const Config::Playlist& p) { return p.name == config.activePlaylist; }),
            config.playlists.end());
        config.activePlaylist = L"";
        Config::ConfigManager::GetInstance().Save();
        RefreshPlaylistUI();
        RefreshLibrary();
    }

    void MainWindow::BtnTogglePlaylistItem_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto button = sender.as<Button>();
        std::wstring path = winrt::unbox_value<hstring>(button.Tag()).c_str();

        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        if (config.activePlaylist.empty()) return;
        for (auto& pl : config.playlists) {
            if (pl.name == config.activePlaylist) {
                auto it = std::find(pl.paths.begin(), pl.paths.end(), path);
                if (it != pl.paths.end()) pl.paths.erase(it); // toggle off
                else pl.paths.push_back(path);                 // toggle on
                break;
            }
        }
        Config::ConfigManager::GetInstance().Save();
        RefreshLibrary(); // flip this card's +/✓ icon
    }

    void MainWindow::BtnUpdate_Click(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_isUpdating) return;

        m_isUpdating = true;
        BtnUpdate().IsEnabled(false);
        PrgUpdate().Visibility(Visibility::Visible);
        TxtUpdateStatus().Visibility(Visibility::Visible);
        TxtUpdateStatus().Text(winrt::to_hstring(Localization::Get().checkingUpdate));

        auto dispatcher = this->DispatcherQueue();

        // 1. Check for updates async
        Utils::UpdateChecker::CheckForUpdateAsync([this, dispatcher](const Utils::UpdateInfo& info) {
            if (!info.hasUpdate) {
                dispatcher.TryEnqueue([this]() {
                    m_isUpdating = false;
                    BtnUpdate().IsEnabled(true);
                    PrgUpdate().Visibility(Visibility::Collapsed);
                    TxtUpdateStatus().Text(winrt::to_hstring(Localization::Get().appUpToDate));
                });
                return;
            }

            // 2. Start progress tracking and download update
            auto progress = std::make_shared<std::atomic<float>>(0.0f);

            // Progress loop
            std::thread([this, progress, dispatcher]() {
                while (m_isUpdating) {
                    float pct = progress->load();
                    dispatcher.TryEnqueue([this, pct]() {
                        PrgUpdate().Value(pct * 100.0f);
                        std::wstring pctText = std::to_wstring((int)(pct * 100.0f)) + L"%";
                        TxtUpdateStatus().Text(winrt::to_hstring(Localization::Get().downloading) + winrt::hstring(pctText));
                    });
                    Sleep(200);
                }
            }).detach();

            dispatcher.TryEnqueue([this]() {
                TxtUpdateStatus().Text(winrt::to_hstring(Localization::Get().downloadingUpdate));
            });

            Utils::UpdateChecker::DownloadAndApplyUpdateAsync(info.downloadUrl, progress.get(), [this, progress, dispatcher](bool success, const std::string& errorMsg) {
                dispatcher.TryEnqueue([this, success]() {
                    m_isUpdating = false;
                    BtnUpdate().IsEnabled(true);
                    PrgUpdate().Visibility(Visibility::Collapsed);
                    if (!success) {
                        TxtUpdateStatus().Text(winrt::to_hstring(Localization::Get().updateFailed));
                    }
                });
            }, info.sha256);
        });
    }
}
