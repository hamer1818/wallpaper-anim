cmake_minimum_required(VERSION 3.18)
project(WallpaperAnim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/app.cpp
    src/desktop/desktop_integration.cpp
    # We will add renderers and other files later
)

# Add executable with WIN32 to prevent console window from showing
add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE third_party)

# Link Windows libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    user32
    d3d11
    dxgi
    mf
    mfplat
    mfreadwrite
    mfuuid
    shcore
    shell32
)

# Set Unicode explicitly
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
