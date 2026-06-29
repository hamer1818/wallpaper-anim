#pragma once
#include <windows.h>
#include <string>

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

        // Settings
        const char* performance;
        const char* maxFps;
        const char* powerSaving;
        const char* pauseFullscreen;
        const char* pauseBattery;

        // About / Update
        const char* about;
        const char* checkUpdate;
        const char* checkingUpdate;
        const char* updateAvailableTitle;
        const char* updateAvailableMsg; // format: "New version (%s) available!"
        const char* updateDownloadHint;
        const char* downloadBtn;
        const char* appUpToDateTitle;
        const char* appUpToDateMsg; // format: "You are using the latest version (%s)."

        // Error
        const char* errorTitle;
        const char* okBtn;
        const char* cancelBtn;
    };

    inline const Strings& Get() {
        static const Strings tr = {
            "WallpaperAnim Ayarlar",
            "Kapat",

            "WallpaperAnim'e Hos Geldiniz!",
            "Merhaba! WallpaperAnim'i kurdugunuz icin tesekkurler.\n\n"
            "Ilk animasyonlu arka planinizi ayarlamak icin:\n"
            "1. 'Yeni Ekle' sekmesine gidin.\n"
            "2. Bir video secin veya YouTube linki yapistirin.\n",
            "Anladim, Baslayalim!",

            "[=] Kutuphane",
            "[+] Yeni Ekle",
            "[*] Ayarlar",

            "Duvar Kagitlariniz:",
            "Henuz gecmisinizde duvar kagidi yok.",
            "Kaldir",

            "Yerel Dosya",
            "Bilgisayardan Sec:",
            "Gozat...",
            "YouTube Video",
            "URL:",
            "Getir",
            "Cozunurlukler getiriliyor... Lutfen bekleyin.",
            "Baslik:",
            "Kalite",
            "Indir ve Oynat",

            "--- PERFORMANS ---",
            "Maks. FPS:",
            "--- GUC TASARRUFU ---",
            "Tam ekranda duraklat (Oyun modu)",
            "Pil kullanirken duraklat",

            "--- HAKKINDA ---",
            "Guncellemeyi Kontrol Et",
            "Guncellemeler kontrol ediliyor...",
            "Guncelleme Mevcut!",
            "Yeni bir surum (%s) mevcut!",
            "Indirmek icin asagidaki butona tiklayabilirsiniz.",
            "Indir",
            "Uygulama Guncel",
            "Harika! En guncel surumu (%s) kullaniyorsunuz.",

            "Hata Olustu",
            "Tamam",
            "Kapat",
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

            "[=] Library",
            "[+] Add New",
            "[*] Settings",

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

            "--- PERFORMANCE ---",
            "Max FPS:",
            "--- POWER SAVING ---",
            "Pause on Fullscreen (Gaming)",
            "Pause on Battery Power",

            "--- ABOUT ---",
            "Check for Updates",
            "Checking for updates...",
            "Update Available!",
            "A new version (%s) is available!",
            "Click the button below to download.",
            "Download",
            "App is Up to Date",
            "Great! You are using the latest version (%s).",

            "Error",
            "OK",
            "Close",
        };

        static const bool isTR = IsSystemTurkish();
        return isTR ? tr : en;
    }

} // namespace Localization
