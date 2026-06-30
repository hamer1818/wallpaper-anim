#include "settings_ui.h"
#include "config.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <commdlg.h>
#include <vector>
#include "utils/texture_loader.h"
#include "utils/thumbnail_generator.h"
#include "../version.h"
#include "../localization.h"
#include <ctime>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include "resource.h"

// Global pointer for WndProc
static SettingsUI* g_pSettingsUI = nullptr;

SettingsUI::SettingsUI() : m_hInstance(nullptr), m_hwnd(nullptr), m_mainHwnd(nullptr), m_isVisible(false), m_imguiInitialized(false) {}

SettingsUI::~SettingsUI() {
    Cleanup();
}

void SettingsUI::CheckForUpdate(bool showPopupIfUpToDate) {
    if (m_updateChecking) return;
    m_updateChecking = true;
    m_showUpdateUpToDate = showPopupIfUpToDate;
    
    Utils::UpdateChecker::CheckForUpdateAsync([this](const Utils::UpdateInfo& info) {
        m_updateChecking = false;
        m_updateInfo = info;
        m_showUpdatePopup = true;
    });
}

bool SettingsUI::Initialize(HINSTANCE hInstance, ID3D11Device* device, HWND mainHwnd) {
    m_hInstance = hInstance;
    m_device = device;
    m_mainHwnd = mainHwnd;
    m_device->GetImmediateContext(&m_context);
    g_pSettingsUI = this;

    // Register class
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON));
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, hInstance, hIcon, nullptr, nullptr, nullptr, L"WallpaperAnimSettingsUI", hIcon };
    RegisterClassExW(&wc);

    // Create window
    m_hwnd = CreateWindowW(L"WallpaperAnimSettingsUI", L"WallpaperAnim Settings", WS_OVERLAPPEDWINDOW, 100, 100, 600, 550, nullptr, nullptr, hInstance, nullptr);
    if (!m_hwnd) return false;

    if (!CreateDeviceAndSwapChain()) return false;

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

    ImGuiIO& io = ImGui::GetIO();
    // Disable INI file
    io.IniFilename = nullptr;
    
    // Load modern font (Segoe UI)
    char windowsFolder[MAX_PATH];
    GetWindowsDirectoryA(windowsFolder, MAX_PATH);
    std::string fontPath = std::string(windowsFolder) + "\\Fonts\\segoeui.ttf";
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f);

    // Load Logo
    Utils::TextureLoader::LoadTextureFromFile(L"assets\\WallpaperAnim-logo.png", m_device.Get(), m_logoSrv, m_logoWidth, m_logoHeight);

    // Apply Premium Dark Theme
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);

    m_imguiInitialized = true;

    Config::AppConfig& config = Config::ConfigManager::GetInstance().GetConfig();
    int64_t now = time(nullptr);
    if (now - config.lastUpdateCheck > 7 * 24 * 3600) {
        config.lastUpdateCheck = now;
        Config::ConfigManager::GetInstance().Save();
        CheckForUpdate(false);
    }

    return true;
}

void SettingsUI::Cleanup() {
    if (m_imguiInitialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
    }

    CleanupRenderTarget();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        UnregisterClassW(L"WallpaperAnimSettingsUI", m_hInstance);
        m_hwnd = nullptr;
    }
    g_pSettingsUI = nullptr;
}

void SettingsUI::Show() {
    if (m_hwnd && !m_isVisible) {
        ShowWindow(m_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(m_hwnd);
        m_isVisible = true;
    }
}

void SettingsUI::Hide() {
    if (m_hwnd && m_isVisible) {
        ShowWindow(m_hwnd, SW_HIDE);
        m_isVisible = false;
    }
}

bool SettingsUI::CreateDeviceAndSwapChain() {
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(m_device.As(&dxgiDevice))) return false;

    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) return false;

    ComPtr<IDXGIFactory> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    if (FAILED(factory->CreateSwapChain(m_device.Get(), &sd, &m_swapChain))) return false;

    CreateRenderTarget();
    return true;
}

void SettingsUI::CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> pBackBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_mainRenderTargetView);
}

void SettingsUI::CleanupRenderTarget() {
    m_mainRenderTargetView.Reset();
}

std::wstring SettingsUI::OpenFileDialog() {
    wchar_t filename[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Media Files (*.mp4;*.avi;*.gif;*.hlsl)\0*.mp4;*.avi;*.gif;*.hlsl\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        return std::wstring(filename);
    }
    return L"";
}

// Custom ToggleButton implementation for ImGui
bool ToggleButton(const char* str_id, bool* v) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    float height = ImGui::GetFrameHeight();
    float width = height * 1.8f;
    float radius = height * 0.5f;

    ImGui::InvisibleButton(str_id, ImVec2(width, height));
    bool clicked = ImGui::IsItemClicked();
    if (clicked) *v = !*v;
    
    float t = *v ? 1.0f : 0.0f;

    ImGuiContext& g = *GImGui;
    float ANIM_SPEED = 0.08f;
    if (g.LastActiveId == g.CurrentWindow->GetID(str_id)) {
        float t_anim = ImGui::GetStateStorage()->GetFloat(ImGui::GetID(str_id), t);
        float t_target = *v ? 1.0f : 0.0f;
        if (t_anim != t_target) {
            t_anim += (t_target - t_anim) * ImGui::GetIO().DeltaTime * 12.0f;
            ImGui::GetStateStorage()->SetFloat(ImGui::GetID(str_id), t_anim);
        }
        t = t_anim;
    }

    ImU32 col_bg;
    if (ImGui::IsItemHovered())
        col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.35f, 0.35f, 0.35f, 1.0f), ImVec4(0.00f, 0.80f, 1.00f, 1.0f), t));
    else
        col_bg = ImGui::GetColorU32(ImLerp(ImVec4(0.25f, 0.25f, 0.25f, 1.0f), ImVec4(0.00f, 0.70f, 0.90f, 1.0f), t));

    draw_list->AddRectFilled(p, ImVec2(p.x + width, p.y + height), col_bg, height * 0.5f);
    draw_list->AddCircleFilled(ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius), radius - 2.5f, IM_COL32(255, 255, 255, 255));

    return clicked;
}

ID3D11ShaderResourceView* SettingsUI::GetThumbnailSRV(const std::wstring& thumbPath) {
    if (thumbPath.empty()) return nullptr;
    if (m_thumbnails.find(thumbPath) != m_thumbnails.end()) {
        return m_thumbnails[thumbPath].Get();
    }
    
    // Load it
    ComPtr<ID3D11ShaderResourceView> srv;
    int w, h;
    if (Utils::TextureLoader::LoadTextureFromFile(thumbPath, m_device.Get(), srv, w, h)) {
        m_thumbnails[thumbPath] = srv;
        return srv.Get();
    }
    
    return nullptr;
}

void SettingsUI::AddToHistory(const std::wstring& path, int type) {
    auto& config = Config::ConfigManager::GetInstance().GetConfig();
    
    // Check if already exists
    for (auto it = config.history.begin(); it != config.history.end(); ++it) {
        if (it->path == path) {
            config.history.erase(it);
            break;
        }
    }
    
    Config::WallpaperHistoryItem item;
    item.path = path;
    item.type = type;
    
    // Get simple filename for name
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) item.name = path.substr(slash + 1);
    else item.name = path;
    
    item.thumbPath = Utils::ThumbnailGenerator::GenerateThumbnail(path);
    
    // Push to front
    config.history.insert(config.history.begin(), item);
    Config::ConfigManager::GetInstance().Save();
}

void SettingsUI::PlayMedia(const std::wstring& path) {
    auto& config = Config::ConfigManager::GetInstance().GetConfig();
    config.lastVideoPath = path;
    Config::ConfigManager::GetInstance().Save();
    if (m_mainHwnd) {
        PostMessage(m_mainHwnd, WM_APP + 1, 0, 0); // WM_APP_CONFIG_CHANGED
    }
}

void SettingsUI::RenderUI() {
    auto& configManager = Config::ConfigManager::GetInstance();
    auto& config = configManager.GetConfig();
    bool changed = false;

    // Process Async Callbacks on Main Thread
    if (m_ytFetchComplete) {
        m_ytFetching = false;
        m_ytFetchComplete = false;
        if (m_ytFetchSuccess) {
            m_ytResolutions = m_ytFetchResResult;
            m_ytTitle = m_ytFetchTitleResult;
            m_ytSelectedRes = 0;
        } else {
            m_lastErrorMsg = m_ytFetchErrResult;
            m_showError = true;
        }
    }

    if (m_ytDownloadComplete) {
        m_ytDownloading = false;
        m_ytDownloadComplete = false;
        if (m_ytDownloadSuccess) {
            AddToHistory(m_ytDownloadPathResult, 3);
            PlayMedia(m_ytDownloadPathResult);
        } else {
            m_lastErrorMsg = m_ytDownloadErrResult;
            m_showError = true;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    
    // Header
    const auto& L = Localization::Get();

    if (m_logoSrv) {
        ImGui::Image((ImTextureID)m_logoSrv.Get(), ImVec2(32, 32));
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    }
    ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.9f, 1.0f), "%s", L.settingsTitle);
    ImGui::Separator();
    ImGui::Spacing();

    if (config.isFirstRun) {
        ImGui::OpenPopup("##Welcome");
    }

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##Welcome", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", L.welcomeTitle);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", L.welcomeBody);
        ImGui::Separator();
        if (ImGui::Button(L.welcomeBtn, ImVec2(200, 0))) {
            config.isFirstRun = false;
            changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showError) {
        ImGui::OpenPopup("##Error");
        m_showError = false;
    }
    
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, -1), ImVec2(600, -1));
    if (ImGui::BeginPopupModal("##Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", L.errorTitle);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_lastErrorMsg.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button(L.okBtn, ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Process auto-update download callbacks on main thread
    if (m_updateDownloadComplete) {
        m_updateDownloadComplete = false;
        if (!m_updateDownloadSuccess) {
            m_lastErrorMsg = m_updateDownloadError.empty() ? L.updateFailedMsg : m_updateDownloadError;
            m_showError = true;
            m_updateDownloading = false;
        }
        // On success, the app will have already exited via ExitProcess
    }

    if (m_showUpdatePopup) {
        if (!m_updateInfo.errorMessage.empty()) {
            m_lastErrorMsg = m_updateInfo.errorMessage;
            m_showError = true;
        } else if (m_updateInfo.hasUpdate) {
            ImGui::OpenPopup("##UpdateAvailable");
        } else if (m_showUpdateUpToDate) {
            ImGui::OpenPopup("##UpToDate");
        }
        m_showUpdatePopup = false;
    }

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(350, -1), ImVec2(600, -1));
    if (ImGui::BeginPopupModal("##UpdateAvailable", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", L.updateAvailableTitle);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text(L.updateAvailableMsg, m_updateInfo.version.c_str());
        ImGui::Spacing();

        if (m_updateDownloading) {
            // Show progress bar while downloading
            ImGui::Text("%s", L.updatingMsg);
            ImGui::ProgressBar(m_updateProgress.load(), ImVec2(-1.0f, 0.0f));
            if (m_updateProgress.load() >= 0.99f) {
                ImGui::Text("%s", L.updateApplyingMsg);
            }
        } else {
            ImGui::Text("%s", L.updateDownloadHint);
            ImGui::Separator();
            ImGui::Spacing();

            // Auto-update button (downloads and applies)
            if (!m_updateInfo.downloadUrl.empty()) {
                if (ImGui::Button(L.updateBtn, ImVec2(140, 0))) {
                    m_updateDownloading = true;
                    m_updateProgress = 0.0f;
                    Utils::UpdateChecker::DownloadAndApplyUpdateAsync(
                        m_updateInfo.downloadUrl,
                        &m_updateProgress,
                        [this](bool success, const std::string& err) {
                            m_updateDownloadSuccess = success;
                            m_updateDownloadError = err;
                            m_updateDownloadComplete = true;
                        }
                    );
                }
                ImGui::SameLine();
            }

            // Fallback: open in browser
            if (ImGui::Button(L.downloadBtn, ImVec2(140, 0))) {
                ShellExecuteA(NULL, "open", m_updateInfo.releaseUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(L.cancelBtn, ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("##UpToDate", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", L.appUpToDateTitle);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text(L.appUpToDateMsg, APP_VERSION_STRING);
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button(L.okBtn, ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24.0f, 12.0f));
    if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        // TAB 1: Library
        if (ImGui::BeginTabItem(L.tabLibrary)) {
            ImGui::Spacing();
            ImGui::Text("%s", L.yourWallpapers);
            ImGui::Separator();
            ImGui::BeginChild("LibraryScroll", ImVec2(0, ImGui::GetContentRegionAvail().y - 45));
            
            float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            ImGuiStyle& style = ImGui::GetStyle();
            
            int id = 0;
            auto it = config.history.begin();
            while (it != config.history.end()) {
                ImGui::PushID(id++);
                
                ID3D11ShaderResourceView* srv = GetThumbnailSRV(it->thumbPath);
                ImTextureID tex_id = srv ? (ImTextureID)srv : 0;
                
                // Group thumbnail and delete button
                ImGui::BeginGroup();
                
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 sz = ImVec2(240, 135);
                ImVec2 p1 = ImVec2(p0.x + sz.x, p0.y + sz.y);
                
                // Add a hover background rect spanning thumbnail and button
                bool isHovered = ImGui::IsMouseHoveringRect(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y + 36));
                if (isHovered) {
                    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p0.x - 6, p0.y - 6), ImVec2(p1.x + 6, p1.y + 40), IM_COL32(50, 50, 50, 255), 8.0f);
                    ImGui::GetWindowDrawList()->AddRect(ImVec2(p0.x - 6, p0.y - 6), ImVec2(p1.x + 6, p1.y + 40), IM_COL32(0, 180, 230, 255), 8.0f, 0, 2.0f);
                }

                if (ImGui::ImageButton("##Thumb", tex_id, sz)) { // 16:9 aspect ratio
                    PlayMedia(it->path);
                }
                if (ImGui::IsItemHovered()) {
                    char nameUtf8[256];
                    WideCharToMultiByte(CP_UTF8, 0, it->name.c_str(), -1, nameUtf8, sizeof(nameUtf8), NULL, NULL);
                    ImGui::SetTooltip("%s\n(Click to Play)", nameUtf8);
                }
                
                if (ImGui::Button(L.remove, ImVec2(240, 28))) {
                    it = config.history.erase(it);
                    changed = true;
                    ImGui::EndGroup();
                    ImGui::PopID();
                    continue;
                }
                ImGui::EndGroup();
                
                float last_button_x2 = ImGui::GetItemRectMax().x;
                float next_button_x2 = last_button_x2 + style.ItemSpacing.x + 240.0f; // Expected next item width
                if (id < (int)config.history.size() && next_button_x2 < window_visible_x2)
                    ImGui::SameLine();
                
                ++it;
                ImGui::PopID();
            }
            
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // TAB 2: Add New / YouTube
        if (ImGui::BeginTabItem(L.tabAddNew)) {
            ImGui::Spacing();
            
            ImGui::Text("%s", L.localFile);
            if (ImGui::BeginChild("LocalPanel", ImVec2(0, 80), true)) {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", L.browsePC);
                ImGui::SameLine();
                if (ImGui::Button(L.browseBtn, ImVec2(100, 0))) {
                    std::wstring newPath = OpenFileDialog();
                    if (!newPath.empty()) {
                        int type = 0;
                        if (newPath.find(L".gif") != std::wstring::npos) type = 1;
                        if (newPath.find(L".hlsl") != std::wstring::npos) type = 2;
                        AddToHistory(newPath, type);
                        PlayMedia(newPath);
                    }
                }
            }
            ImGui::EndChild();
            ImGui::Spacing();

            ImGui::Text("%s", L.youtubeVideo);
            if (ImGui::BeginChild("YouTubePanel", ImVec2(0, 160), true)) {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", L.urlLabel);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100);
                ImGui::InputText("##YTUrl", m_ytUrl, sizeof(m_ytUrl));
                ImGui::SameLine();
                if (ImGui::Button(L.fetchBtn, ImVec2(90, 0))) {
                    m_ytFetching = true;
                    m_ytResolutions.clear();
                    Utils::YouTubeDownloader::FetchResolutionsAsync(m_ytUrl, [this](bool success, const std::vector<Utils::YouTubeResolution>& res, const std::string& title, const std::string& err) {
                        m_ytFetchSuccess = success;
                        m_ytFetchResResult = res;
                        m_ytFetchTitleResult = title;
                        m_ytFetchErrResult = err;
                        m_ytFetchComplete = true;
                    });
                }

                if (m_ytFetching) {
                    ImGui::Text("%s", L.fetchingMsg);
                } else if (!m_ytResolutions.empty()) {
                    ImGui::Text("%s %s", L.titleLabel, m_ytTitle.c_str());
                    
                    std::string preview_value = std::to_string(m_ytResolutions[m_ytSelectedRes].height) + "p";
                    if (ImGui::BeginCombo(L.qualityLabel, preview_value.c_str())) {
                        for (int i = 0; i < (int)m_ytResolutions.size(); i++) {
                            const bool is_selected = (m_ytSelectedRes == i);
                            std::string res_label = std::to_string(m_ytResolutions[i].height) + "p";
                            if (ImGui::Selectable(res_label.c_str(), is_selected))
                                m_ytSelectedRes = i;
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    if (!m_ytDownloading) {
                        if (ImGui::Button(L.downloadPlayBtn, ImVec2(150, 0))) {
                            m_ytDownloading = true;
                            m_ytProgress = 0.0f;
                            Utils::YouTubeDownloader::DownloadAsync(m_ytUrl, m_ytResolutions[m_ytSelectedRes].format_id, &m_ytProgress, [this](bool success, const std::wstring& path, const std::string& err) {
                                m_ytDownloadSuccess = success;
                                m_ytDownloadPathResult = path;
                                m_ytDownloadErrResult = err;
                                m_ytDownloadComplete = true;
                            });
                        }
                    } else {
                        ImGui::ProgressBar(m_ytProgress.load(), ImVec2(-1.0f, 0.0f));
                    }
                }
            }
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        // TAB 3: Settings
        if (ImGui::BeginTabItem(L.tabSettings)) {
            ImGui::Spacing();
            ImGui::Text("%s", L.performance);
            if (ImGui::BeginChild("PerfPanel", ImVec2(0, 80), true)) {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", L.maxFps);
                ImGui::SameLine(100);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::SliderInt("##MaxFPS", &config.maxFPS, 10, 144, "%d FPS")) changed = true;
            }
            ImGui::EndChild();
            ImGui::Spacing();

            ImGui::Text("%s", L.powerSaving);
            if (ImGui::BeginChild("PowerPanel", ImVec2(0, 110), true)) {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", L.pauseFullscreen);
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
                if (ToggleButton("##PauseFS", &config.pauseOnFullscreen)) changed = true;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", L.pauseBattery);
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
                if (ToggleButton("##PauseBatt", &config.pauseOnBattery)) changed = true;
            }
            ImGui::EndChild();
            ImGui::Spacing();

            ImGui::Separator();
            ImGui::Text("%s", L.about);
            ImGui::Text("WallpaperAnim v%s", APP_VERSION_STRING);
            ImGui::Spacing();
            
            if (m_updateChecking) {
                ImGui::Text("%s", L.checkingUpdate);
            } else {
                if (ImGui::Button(L.checkUpdate, ImVec2(220, 0))) {
                    CheckForUpdate(true);
                }
            }
            
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();
    
    // Bottom Align Close Button
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
    ImGui::Separator();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 50);
    if (ImGui::Button(L.closeBtn, ImVec2(100, 0))) {
        Hide();
    }

    ImGui::PopFont();
    ImGui::End();

    if (changed) {
        configManager.Save();
        if (m_mainHwnd) {
            PostMessage(m_mainHwnd, WM_APP + 1, 0, 0); // WM_APP_CONFIG_CHANGED
        }
    }
}

void SettingsUI::Render() {
    if (!m_isVisible || !m_imguiInitialized || !m_swapChain) return;

    // Start ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderUI();

    ImGui::Render();

    const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    m_context->OMSetRenderTargets(1, m_mainRenderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_mainRenderTargetView.Get(), clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    m_swapChain->Present(1, 0); // VSync enabled
}

LRESULT CALLBACK SettingsUI::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    if (msg == WM_SIZE) {
        if (g_pSettingsUI && g_pSettingsUI->m_device && wParam != SIZE_MINIMIZED) {
            g_pSettingsUI->CleanupRenderTarget();
            g_pSettingsUI->m_swapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            g_pSettingsUI->CreateRenderTarget();
        }
        return 0;
    }
    if (msg == WM_SYSCOMMAND) {
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
    }
    if (msg == WM_CLOSE) {
        // Just hide it instead of closing
        if (g_pSettingsUI) g_pSettingsUI->Hide();
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
