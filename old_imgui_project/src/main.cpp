#include "app.h"
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // COM must be initialized before Media Foundation can work
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    App app;
    int result = 0;

    if (app.Initialize(hInstance, nCmdShow)) {
        result = app.Run();
    }

    CoUninitialize();
    return result;
}
