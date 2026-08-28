#pragma once

#include <cstdlib>
#include <string>

#include "config.h"

// UI strings for the Linux build. Mirrors the Windows src/localization.h idea but
// uses C++20 designated initializers, so adding a key cannot silently shift the
// other strings out of position the way the positional Windows lists can.
namespace Localization {

    struct Strings {
        // Window / tray
        const char* appName;
        const char* settingsTitle;
        const char* tabLibrary;
        const char* tabAddNew;
        const char* tabPlaylists;
        const char* tabSettings;
        const char* tabAbout;

        // Library
        const char* yourWallpapers;
        const char* noHistory;
        const char* applyBtn;
        const char* remove;
        const char* removeConfirmTitle;
        const char* removeConfirmBody;

        // Add new
        const char* localFile;
        const char* localFileDesc;
        const char* browseBtn;
        const char* fileDialogTitle;
        const char* fileDialogFilter;
        const char* youtubeVideo;
        const char* youtubeDesc;
        const char* urlLabel;
        const char* youtubePlaceholder;
        const char* downloadPlayBtn;
        const char* downloadStarting;
        const char* downloading;
        const char* downloadComplete;
        const char* videoDownloadFailed;
        const char* ytDlpMissing;

        // Settings - playback
        const char* performance;
        const char* maxFps;
        const char* fpsAuto;
        const char* fitModeLabel;
        const char* fitFill;
        const char* fitFitLetterbox;
        const char* fitStretch;
        const char* fitCenter;
        const char* hwdecLabel;
        const char* hwdecDesc;

        // Settings - power
        const char* powerSaving;
        const char* pauseFullscreen;
        const char* pauseFullscreenDesc;
        const char* pauseBattery;
        const char* pauseBatteryDesc;
        const char* pauseHidden;
        const char* pauseHiddenDesc;

        // Settings - system
        const char* systemGroup;
        const char* runAtStartup;
        const char* runAtStartupDesc;
        const char* languageLabel;
        const char* langAuto;
        const char* langTurkish;
        const char* langEnglish;
        const char* restartHint;

        // Settings - Linux display backend
        const char* displayGroup;
        const char* backendLabel;
        const char* backendAuto;
        const char* backendPlasma;
        const char* backendLayerShell;
        const char* backendPlasmaDesc;
        const char* backendLayerDesc;
        const char* layerLabel;
        const char* layerBackground;
        const char* layerBottom;
        const char* layerHint;
        const char* plasmaActivateBtn;
        const char* plasmaRestoreBtn;
        const char* plasmaActive;
        const char* plasmaInactive;
        const char* plasmaInstallFailed;
        const char* plasmaNotRunning;

        // Playlists
        const char* playlistTitle;
        const char* playlistDesc;
        const char* playlistShuffle;
        const char* playlistIntervalLabel;
        const char* playlistsHeader;
        const char* activePlaylistLabel;
        const char* newPlaylistPlaceholder;
        const char* createBtn;
        const char* deleteBtn;
        const char* playlistHint;
        const char* allLibraryItem;
        const char* addSelectedBtn;
        const char* removeSelectedBtn;

        // Tray
        const char* trayPause;
        const char* trayResume;
        const char* trayNext;
        const char* traySettings;
        const char* trayQuit;
        const char* trayTooltip;

        // About
        const char* aboutTitle;
        const char* aboutBody;
        const char* versionLabel;
        const char* projectPage;

        // Common / errors
        const char* errorTitle;
        const char* okBtn;
        const char* closeBtn;
        const char* mediaLoadFailedTitle;
        const char* mediaLoadFailed;
        const char* hlslUnsupported;
        const char* minimizeWarningTitle;
        const char* minimizeWarningDesc;
        const char* doNotShowAgain;
        const char* alreadyRunning;
    };

    inline bool IsSystemTurkish() {
        for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
            const char* v = std::getenv(var);
            if (v && *v) {
                const std::string value(v);
                return value.rfind("tr", 0) == 0;
            }
        }
        return false;
    }

    inline const Strings& Get() {
        static const Strings tr = {
            .appName = "WallpaperAnim",
            .settingsTitle = "WallpaperAnim Ayarları",
            .tabLibrary = "Kitaplık",
            .tabAddNew = "Yeni Ekle",
            .tabPlaylists = "Oynatma Listeleri",
            .tabSettings = "Ayarlar",
            .tabAbout = "Hakkında",

            .yourWallpapers = "Duvar Kağıtlarınız",
            .noHistory = "Henüz duvar kağıdı eklenmemiş. \"Yeni Ekle\" sekmesinden başlayın.",
            .applyBtn = "Uygula",
            .remove = "Kaldır",
            .removeConfirmTitle = "Kaldırılsın mı?",
            .removeConfirmBody = "Bu duvar kağıdı kitaplıktan kaldırılacak. Devam edilsin mi?",

            .localFile = "Yerel Dosya",
            .localFileDesc = "Bilgisayarınızdaki bir video, GIF veya GLSL gölgelendirici dosyasını seçin.",
            .browseBtn = "Gözat",
            .fileDialogTitle = "Duvar kağıdı seç",
            .fileDialogFilter = "Desteklenen dosyalar (*.mp4 *.mkv *.webm *.mov *.avi *.gif *.glsl *.frag);;Tüm dosyalar (*)",
            .youtubeVideo = "YouTube Videosu",
            .youtubeDesc = "Bağlantıyı yapıştırın; video indirilip kitaplığa eklenir (yt-dlp gerekir).",
            .urlLabel = "Bağlantı",
            .youtubePlaceholder = "https://www.youtube.com/watch?v=...",
            .downloadPlayBtn = "İndir ve Uygula",
            .downloadStarting = "İndirme başlatılıyor...",
            .downloading = "İndiriliyor",
            .downloadComplete = "İndirme tamamlandı",
            .videoDownloadFailed = "Video indirilemedi",
            .ytDlpMissing = "yt-dlp bulunamadı. Kurmak için: sudo pacman -S yt-dlp",

            .performance = "Oynatma",
            .maxFps = "Azami FPS",
            .fpsAuto = "Otomatik (kaynak hızı)",
            .fitModeLabel = "Ekrana sığdırma",
            .fitFill = "Doldur (kırp)",
            .fitFitLetterbox = "Sığdır (siyah bant)",
            .fitStretch = "Uzat (oranı boz)",
            .fitCenter = "Ortala (gerçek boyut)",
            .hwdecLabel = "Donanım hızlandırmalı çözme",
            .hwdecDesc = "GPU ile video çözme (NVDEC/VAAPI). Sorun yaşarsanız kapatın.",

            .powerSaving = "Güç Tasarrufu",
            .pauseFullscreen = "Ekran koruyucuyu engelleyen uygulamada duraklat",
            .pauseFullscreenDesc = "Wayland başka pencereleri incelemeye izin vermediği için bu, ekran koruyucu/uyku engellemesine bakar. Tarayıcılar ve uzak masaüstü araçları da bunu tuttuğu için varsayılan olarak kapalıdır; \"Görünmezken duraklat\" daha isabetlidir.",
            .pauseBattery = "Pilde duraklat",
            .pauseBatteryDesc = "Dizüstü bilgisayar prizden çıkınca oynatmayı durdurur.",
            .pauseHidden = "Görünmezken duraklat",
            .pauseHiddenDesc = "Duvar kağıdı tamamen pencerelerle kaplıyken çözme işini durdurur.",

            .systemGroup = "Sistem",
            .runAtStartup = "Açılışta başlat",
            .runAtStartupDesc = "Oturum açıldığında WallpaperAnim'i otomatik çalıştırır.",
            .languageLabel = "Dil",
            .langAuto = "Otomatik",
            .langTurkish = "Türkçe",
            .langEnglish = "English",
            .restartHint = "Değişiklik uygulamayı yeniden başlattığınızda etkinleşir.",

            .displayGroup = "Görüntüleme Yöntemi",
            .backendLabel = "Arka uç",
            .backendAuto = "Otomatik",
            .backendPlasma = "Plasma duvar kağıdı eklentisi",
            .backendLayerShell = "Wayland layer-shell yüzeyi",
            .backendPlasmaDesc = "KDE Plasma için önerilir: masaüstü simgelerinin ve bileşenlerinin arkasına çizer.",
            .backendLayerDesc = "Hyprland, Sway ve diğer wlroots tabanlı derleyiciler için.",
            .layerLabel = "Katman",
            .layerBackground = "Arka plan (her şeyin altında)",
            .layerBottom = "Alt katman (masaüstünün üstünde)",
            .layerHint = "Plasma'da arka plan katmanı masaüstünün altında kaldığı için görünmez; alt katman ise simgeleri kapatır.",
            .plasmaActivateBtn = "Plasma duvar kağıdı olarak ayarla",
            .plasmaRestoreBtn = "Plasma duvar kağıdını geri al",
            .plasmaActive = "Etkin - WallpaperAnim, Plasma duvar kağıdı olarak çalışıyor.",
            .plasmaInactive = "Etkin değil - Plasma hâlâ kendi duvar kağıdını gösteriyor.",
            .plasmaInstallFailed = "Plasma eklentisi kurulamadı.",
            .plasmaNotRunning = "plasmashell çalışmıyor; bu seçenek yalnızca KDE Plasma oturumunda kullanılabilir.",

            .playlistTitle = "Otomatik Değiştirme",
            .playlistDesc = "Duvar kağıtlarını belirli aralıklarla sırayla değiştirir.",
            .playlistShuffle = "Karışık sırayla",
            .playlistIntervalLabel = "Değiştirme aralığı (dakika)",
            .playlistsHeader = "Listeler",
            .activePlaylistLabel = "Etkin liste",
            .newPlaylistPlaceholder = "Yeni liste adı",
            .createBtn = "Oluştur",
            .deleteBtn = "Sil",
            .playlistHint = "Bir liste seçiliyken otomatik değiştirme yalnızca o listedeki duvar kağıtlarını kullanır.",
            .allLibraryItem = "Tüm kitaplık",
            .addSelectedBtn = "Listeye ekle",
            .removeSelectedBtn = "Listeden çıkar",

            .trayPause = "Duraklat",
            .trayResume = "Devam ettir",
            .trayNext = "Sonraki duvar kağıdı",
            .traySettings = "Ayarlar",
            .trayQuit = "Çıkış",
            .trayTooltip = "WallpaperAnim - hareketli duvar kağıdı",

            .aboutTitle = "WallpaperAnim hakkında",
            .aboutBody = "Windows 10/11 ve Linux için hafif hareketli duvar kağıdı motoru.",
            .versionLabel = "Sürüm",
            .projectPage = "Proje sayfası",

            .errorTitle = "Hata",
            .okBtn = "Tamam",
            .closeBtn = "Kapat",
            .mediaLoadFailedTitle = "Duvar kağıdı yüklenemedi",
            .mediaLoadFailed = "Dosya açılamadı veya biçimi desteklenmiyor.",
            .hlslUnsupported = "HLSL gölgelendiricileri yalnızca Windows sürümünde çalışır. Linux'ta .glsl/.frag dosyası kullanın.",
            .minimizeWarningTitle = "Arka planda çalışıyor",
            .minimizeWarningDesc = "WallpaperAnim sistem tepsisinde çalışmaya devam ediyor.",
            .doNotShowAgain = "Bir daha gösterme",
            .alreadyRunning = "WallpaperAnim zaten çalışıyor.",
        };

        static const Strings en = {
            .appName = "WallpaperAnim",
            .settingsTitle = "WallpaperAnim Settings",
            .tabLibrary = "Library",
            .tabAddNew = "Add New",
            .tabPlaylists = "Playlists",
            .tabSettings = "Settings",
            .tabAbout = "About",

            .yourWallpapers = "Your Wallpapers",
            .noHistory = "No wallpapers yet. Start from the \"Add New\" tab.",
            .applyBtn = "Apply",
            .remove = "Remove",
            .removeConfirmTitle = "Remove?",
            .removeConfirmBody = "This wallpaper will be removed from the library. Continue?",

            .localFile = "Local File",
            .localFileDesc = "Pick a video, GIF or GLSL shader file from your computer.",
            .browseBtn = "Browse",
            .fileDialogTitle = "Choose a wallpaper",
            .fileDialogFilter = "Supported files (*.mp4 *.mkv *.webm *.mov *.avi *.gif *.glsl *.frag);;All files (*)",
            .youtubeVideo = "YouTube Video",
            .youtubeDesc = "Paste a link; the video is downloaded and added to your library (needs yt-dlp).",
            .urlLabel = "Link",
            .youtubePlaceholder = "https://www.youtube.com/watch?v=...",
            .downloadPlayBtn = "Download and Apply",
            .downloadStarting = "Starting download...",
            .downloading = "Downloading",
            .downloadComplete = "Download complete",
            .videoDownloadFailed = "Video download failed",
            .ytDlpMissing = "yt-dlp was not found. Install it with: sudo pacman -S yt-dlp",

            .performance = "Playback",
            .maxFps = "Max FPS",
            .fpsAuto = "Automatic (source rate)",
            .fitModeLabel = "Fit to screen",
            .fitFill = "Fill (crop)",
            .fitFitLetterbox = "Fit (letterbox)",
            .fitStretch = "Stretch (distort)",
            .fitCenter = "Center (native size)",
            .hwdecLabel = "Hardware decoding",
            .hwdecDesc = "Decode video on the GPU (NVDEC/VAAPI). Turn off if you hit problems.",

            .powerSaving = "Power Saving",
            .pauseFullscreen = "Pause while an app blocks the screensaver",
            .pauseFullscreenDesc = "Wayland does not let one app inspect another's windows, so this keys off screensaver/sleep inhibition. Browsers and remote-desktop tools hold that too, which is why it is off by default; \"Pause while hidden\" is the accurate one.",
            .pauseBattery = "Pause on battery",
            .pauseBatteryDesc = "Stops playback when a laptop is unplugged.",
            .pauseHidden = "Pause while hidden",
            .pauseHiddenDesc = "Stops decoding while the wallpaper is fully covered by windows.",

            .systemGroup = "System",
            .runAtStartup = "Run at startup",
            .runAtStartupDesc = "Starts WallpaperAnim automatically when you log in.",
            .languageLabel = "Language",
            .langAuto = "Automatic",
            .langTurkish = "Türkçe",
            .langEnglish = "English",
            .restartHint = "Takes effect after you restart the app.",

            .displayGroup = "Display Method",
            .backendLabel = "Backend",
            .backendAuto = "Automatic",
            .backendPlasma = "Plasma wallpaper plugin",
            .backendLayerShell = "Wayland layer-shell surface",
            .backendPlasmaDesc = "Recommended on KDE Plasma: draws behind desktop icons and widgets.",
            .backendLayerDesc = "For Hyprland, Sway and other wlroots-based compositors.",
            .layerLabel = "Layer",
            .layerBackground = "Background (below everything)",
            .layerBottom = "Bottom (above the desktop)",
            .layerHint = "On Plasma the background layer stays under the desktop and is invisible, while the bottom layer covers the icons.",
            .plasmaActivateBtn = "Set as Plasma wallpaper",
            .plasmaRestoreBtn = "Restore Plasma wallpaper",
            .plasmaActive = "Active - WallpaperAnim is running as the Plasma wallpaper.",
            .plasmaInactive = "Inactive - Plasma still shows its own wallpaper.",
            .plasmaInstallFailed = "Could not install the Plasma plugin.",
            .plasmaNotRunning = "plasmashell is not running; this option needs a KDE Plasma session.",

            .playlistTitle = "Auto-rotation",
            .playlistDesc = "Cycles through your wallpapers on a timer.",
            .playlistShuffle = "Shuffle order",
            .playlistIntervalLabel = "Switch every (minutes)",
            .playlistsHeader = "Playlists",
            .activePlaylistLabel = "Active playlist",
            .newPlaylistPlaceholder = "New playlist name",
            .createBtn = "Create",
            .deleteBtn = "Delete",
            .playlistHint = "With a playlist selected, auto-rotation only cycles through that playlist.",
            .allLibraryItem = "Whole library",
            .addSelectedBtn = "Add to playlist",
            .removeSelectedBtn = "Remove from playlist",

            .trayPause = "Pause",
            .trayResume = "Resume",
            .trayNext = "Next wallpaper",
            .traySettings = "Settings",
            .trayQuit = "Quit",
            .trayTooltip = "WallpaperAnim - animated wallpaper",

            .aboutTitle = "About WallpaperAnim",
            .aboutBody = "Lightweight animated wallpaper engine for Windows 10/11 and Linux.",
            .versionLabel = "Version",
            .projectPage = "Project page",

            .errorTitle = "Error",
            .okBtn = "OK",
            .closeBtn = "Close",
            .mediaLoadFailedTitle = "Could not load wallpaper",
            .mediaLoadFailed = "The file could not be opened or its format is unsupported.",
            .hlslUnsupported = "HLSL shaders only run on the Windows build. Use a .glsl/.frag file on Linux.",
            .minimizeWarningTitle = "Still running",
            .minimizeWarningDesc = "WallpaperAnim keeps running in the system tray.",
            .doNotShowAgain = "Don't show again",
            .alreadyRunning = "WallpaperAnim is already running.",
        };

        const std::string& lang = Config::ConfigManager::GetInstance().GetConfig().language;
        if (lang == "tr") return tr;
        if (lang == "en") return en;
        return IsSystemTurkish() ? tr : en;
    }
}
