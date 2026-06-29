# WallpaperAnim

An open-source, highly efficient animated wallpaper engine for Windows. Supports playing MP4 videos, GIFs, and running raw HLSL shaders as your desktop background. Includes native YouTube downloading and a built-in visual library!

## Features
- **Video & GIF Support:** Play `.mp4` and `.gif` files seamlessly behind your desktop icons.
- **HLSL Shaders:** Run interactive Pixel Shaders directly on the GPU.
- **YouTube Integration:** Paste a YouTube URL, fetch resolutions, and download directly to use as your wallpaper.
- **Visual Library:** Automatically generates thumbnails for your media and keeps track of your history.
- **Smart Pause:** Automatically pauses playback when a fullscreen application (like a game) is running or when on battery power to save 100% CPU/GPU resources.
- **Modern UI:** Clean, tabbed ImGui-based interface.

## Installation
1. Go to the [Releases](https://github.com/hamer1818/wallpaper-anim/releases) page.
2. Download the latest `WallpaperAnim_vX.X.X.zip`.
3. Extract the contents to a folder.
4. Run `WallpaperAnim.exe`.
*(Note: yt-dlp.exe is included in the release zip. Do not delete it if you want to use the YouTube download feature).*

## Building from Source
This project uses pure Win32 API, DirectX 11, and Media Foundation. No heavy frameworks!

1. Install Visual Studio 2019/2022 with C++ Desktop Development workload.
2. Clone the repository.
3. Run `build.bat` from the Developer Command Prompt.
4. The executable will be placed in the `build/` directory.

## License
MIT License
