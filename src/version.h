#pragma once

#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 4
#define APP_VERSION_PATCH 1
#define APP_VERSION_STRING "1.4.1"

// Wide-string form for WinUI / wide APIs
#define APP_WIDEN2(x) L##x
#define APP_WIDEN(x) APP_WIDEN2(x)
#define APP_VERSION_STRING_W APP_WIDEN(APP_VERSION_STRING)
