cmake_minimum_required(VERSION 3.20)
project(wallpaperanim VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL DBus Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

# LayerShellQt drives the Wayland wlr-layer-shell backend. It is optional: without it
# the app still builds, but only the Plasma wallpaper-plugin backend is available.
find_package(LayerShellQt QUIET)
if(LayerShellQt_FOUND)
    message(STATUS "LayerShellQt found - Wayland layer-shell backend enabled")
else()
    message(WARNING "LayerShellQt not found - layer-shell backend disabled (Plasma backend still works)")
endif()

add_executable(wallpaperanim
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/wallpaper_window.cpp
    src/shader_renderer.cpp
    src/system_monitor.cpp
    src/plasma_integration.cpp
    src/thumbnail.cpp
    src/youtube.cpp
    src/autostart.cpp
    src/settings_window.cpp
)

target_include_directories(wallpaperanim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(wallpaperanim PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::DBus Qt6::Network
    PkgConfig::MPV
)

if(LayerShellQt_FOUND)
    target_link_libraries(wallpaperanim PRIVATE LayerShellQt::Interface)
    target_compile_definitions(wallpaperanim PRIVATE WPA_HAVE_LAYER_SHELL=1)
endif()

include(GNUInstallDirs)
target_compile_definitions(wallpaperanim PRIVATE
    WPA_INSTALL_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}"
)

install(TARGETS wallpaperanim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY data/plasma/wallpapers/ DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/wallpapers)
install(FILES data/applications/wallpaperanim.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ../assets/WallpaperAnim-logo.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME wallpaperanim.png)
install(FILES ../assets/sample.glsl DESTINATION ${CMAKE_INSTALL_DATADIR}/wallpaperanim)
