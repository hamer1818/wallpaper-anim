#include "pch.h"
#include "App.xaml.h"
#include <windows.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <Microsoft.UI.Xaml.Window.h>

using namespace winrt;
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Dispatching;

#include <fstream>
#include <MddBootstrap.h>
#include <WindowsAppSDK-VersionInfo.h>

void Log(const char* msg) {
#ifdef _DEBUG
    std::ofstream ofs("debug.log", std::ios::app);
    ofs << msg << std::endl;
#else
    (void)msg;
#endif
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    Log("Entering wWinMain");
    init_apartment(apartment_type::single_threaded);
    Log("Apartment initialized");

    try {
        Log("Calling Application::Start");
        ::winrt::Microsoft::UI::Xaml::Application::Start(
            [](auto&&)
            {
                Log("Inside Application::Start lambda");
                ::winrt::make<::winrt::WallpaperAnimWinUI::implementation::App>();
                Log("App created");
            });
        Log("Application::Start returned");
    } catch (winrt::hresult_error const&) {
        Log("Exception caught");
    } catch (...) {
        Log("Unknown exception");
    }

    Log("Exiting wWinMain");
    return 0;
}
