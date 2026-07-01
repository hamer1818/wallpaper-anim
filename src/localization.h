#pragma once
#include <windows.h>
#include <string>
#include "config.h"

#pragma execution_character_set("utf-8")

namespace Localization {

    inline bool IsSystemTurkish() {
        LANGID langId = GetUserDefaultUILanguage();
        // Turkish language ID primary = 0x1F
        return (PRIMARYLANGID(langId) == LANG_TURKISH);
    }

    struct Strings {
        // Window / Header
        const char* settingsTitle;
        const char* closeBtn;

        // Welcome
        const char* welcomeTitle;
        const char* welcomeBody;
        const char* welcomeBtn;

        // Tabs
        const char* tabLibrary;
        const char* tabAddNew;
        const char* tabSettings;

        // Library
        const char* yourWallpapers;
        const char* noHistory;
        const char* remove;

        // Add New
        const char* localFile;
        const char* browsePC;
        const char* browseBtn;
        const char* youtubeVideo;
        const char* urlLabel;
        const char* fetchBtn;
        const char* fetchingMsg;
        const char* titleLabel;
        const char* qualityLabel;
        const char* downloadPlayBtn;
        const char* youtubePlaceholder;

        // Settings
        const char* performance;
        const char* maxFps;
        const char* powerSaving;
        const char* pauseFullscreen;
        const char* pauseFullscreenDesc;
        const char* pauseBattery;
        const char* pauseBatteryDesc;
        const char* runAtStartup;
        const char* runAtStartupDesc;
        const char* languageLabel;

        // About
        const char* aboutTitle;
        const char* checkUpdate;
        const char* checkingUpdate;
        const char* updateAvailable;
        const char* newVersionAvailable;
        const char* clickToDownload;
        const char* downloadBtn;
        const char* appUpToDate;
        const char* usingLatestVersion;
        
        const char* updateBtn;
        const char* downloadingUpdate;
        const char* applyingUpdate;
        const char* updateFailed;

        // Minimize Warning
        const char* minimizeWarningTitle;
        const char* minimizeWarningDesc;
        const char* doNotShowAgain;

        // Common
        const char* errorTitle;
        const char* okBtn;
        const char* close;

        // Download / progress status
        const char* downloadStarting;
        const char* fetchInfoFailed;
        const char* downloading;        // prefix; a "<pct>%" is appended
        const char* downloadComplete;
        const char* videoDownloadFailed;

        // Media playback errors
        const char* mediaLoadFailedTitle;
        const char* mediaLoadFailed;
    };

    inline const Strings& Get() {
        static const Strings tr = {
            "WallpaperAnim Ayarları",
            "Kapat",

            "WallpaperAnim'e Hoş Geldiniz!",
            "Merhaba! WallpaperAnim'i yüklediğiniz için teşekkürler.\n\n"
            "İlk hareketli duvar kağıdınızı ayarlamak için:\n"
            "1. 'Duvar Kağıdı Ekle' sekmesine gidin.\n"
            "2. Bir video seçin veya YouTube linki yapıştırın.\n",
            "Anladım, başlayalım!",

            "Kütüphane",
            "Duvar Kağıdı Ekle",
            "Ayarlar",

            "Duvar Kağıtlarınız:",
            "Henüz geçmişte duvar kağıdı yok.",
            "Kaldır",

            "Yerel Dosya",
            "Bilgisayardan Seç:",
            "Gözat...",
            "YouTube Videosu",
            "URL:",
            "Getir",
            "Çözünürlükler getiriliyor... Lütfen bekleyin.",
            "Başlık:",
            "Kalite",
            "İndir ve Oynat",
            "YouTube video bağlantısını yapıştırın (ör. https://www.youtube.com/watch?...)",

            "--- PERFORMANS ---",
            "Maks. FPS:",
            "--- GÜÇ TASARRUFU ---",
            "Tam Ekranda Oynatmayı Duraklat",
            "Başka bir uygulama tam ekran olduğunda duvar kağıdını duraklatır.",
            "Pil Gücünde Oynatmayı Duraklat",
            "Cihaz pildeyken CPU/GPU tasarrufu için duvar kağıdını duraklatır.",
            "Windows Başlangıcında Çalıştır",
            "Bilgisayar açıldığında uygulamayı otomatik başlatır.",
            "Dil",

            "--- HAKKINDA ---",
            "Uygulama Güncellemesi",
            "Güncellemeler kontrol ediliyor...",
            "Güncelleme Mevcut!",
            "Yeni bir sürüm (%s) mevcut!",
            "İndirmek için aşağıdaki butona tıklayabilirsiniz.",
            "İndir",
            "Uygulama Güncel",
            "Harika! En güncel sürümü (%s) kullanıyorsunuz.",

            "Güncellemeleri Kontrol Et",
            "Güncelleme indiriliyor...",
            "Güncelleme uygulanıyor, uygulama yeniden başlatılacak...",
            "Güncelleme sırasında hata oluştu.",

            "Uygulama Küçültüldü",
            "Uygulama arka planda çalışmaya devam edecek ve sistem tepsisinden erişilebilecek.",
            "Bir daha gösterme",

            "Hata Oluştu",
            "Tamam",
            "Kapat",

            "İndirme başlatılıyor...",
            "Hata: Video bilgileri alınamadı.",
            "İndiriliyor: ",
            "İndirme tamamlandı!",
            "Hata: Video indirilemedi.",

            "Oynatılamadı",
            "Bu video oynatılamadı. Codec'i (ör. VP9/AV1) bu Windows sürümünde desteklenmiyor olabilir. Lütfen farklı bir video deneyin.",
        };

        static const Strings en = {
            "WallpaperAnim Settings",
            "Close",

            "Welcome to WallpaperAnim!",
            "Hello! Thank you for installing WallpaperAnim.\n\n"
            "To set your first animated wallpaper:\n"
            "1. Go to the 'Add New' tab.\n"
            "2. Select a video or paste a YouTube link.\n",
            "Got it, let's go!",

            "Library",
            "Add New Wallpaper",
            "Settings",

            "Your Wallpapers:",
            "No wallpapers in your history yet.",
            "Remove",

            "Local File",
            "Browse PC:",
            "Browse...",
            "YouTube Video",
            "URL:",
            "Fetch",
            "Fetching resolutions... Please wait.",
            "Title:",
            "Quality",
            "Download & Play",
            "Paste YouTube video link (e.g. https://www.youtube.com/watch?...)",

            "--- PERFORMANCE ---",
            "Max FPS:",
            "--- POWER SAVING ---",
            "Pause on Fullscreen",
            "Pauses the wallpaper when another app goes fullscreen.",
            "Pause on Battery Power",
            "Pauses the wallpaper to save CPU/GPU when on battery.",
            "Run at Windows Startup",
            "Automatically starts the app when the computer boots.",
            "Language",

            "--- ABOUT ---",
            "Application Update",
            "Checking for updates...",
            "Update Available!",
            "A new version (%s) is available!",
            "Click the button below to download.",
            "Download",
            "App is Up to Date",
            "Great! You are using the latest version (%s).",

            "Check for Updates",
            "Downloading update...",
            "Applying update, app will restart...",
            "Update failed.",

            "App Minimized",
            "The app will continue running in the background and can be accessed from the system tray.",
            "Do not show again",

            "Error",
            "OK",
            "Close",

            "Starting download...",
            "Error: Could not get video info.",
            "Downloading: ",
            "Download complete!",
            "Error: Could not download video.",

            "Playback failed",
            "This video could not be played. Its codec (e.g. VP9/AV1) may be unsupported on this Windows version. Please try a different video.",
        };

        std::wstring lang = Config::ConfigManager::GetInstance().GetConfig().language;
        if (lang.empty()) {
            return IsSystemTurkish() ? tr : en;
        } else if (lang == L"tr") {
            return tr;
        } else {
            return en;
        }
    }

} // namespace Localization
