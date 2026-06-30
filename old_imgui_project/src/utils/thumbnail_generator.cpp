#include "thumbnail_generator.h"
#include <vector>
#include <windows.h>
#include <shlobj.h>
#include <functional>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "stb_image.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace Utils {

    std::wstring ThumbnailGenerator::GetThumbnailsDirectory() {
        PWSTR path = NULL;
        std::wstring dir;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
            dir = path;
            dir += L"\\WallpaperAnim";
            CreateDirectoryW(dir.c_str(), NULL);
            dir += L"\\Thumbnails";
            CreateDirectoryW(dir.c_str(), NULL);
            CoTaskMemFree(path);
        }
        return dir;
    }

    // Extract a frame from video at given time (in 100-nanosecond units) using Media Foundation
    static bool ExtractFrameFromVideo(const std::wstring& videoPath, LONGLONG seekTime,
                                       std::vector<BYTE>& outPixels, UINT32& outWidth, UINT32& outHeight) {
        
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) return false;

        ComPtr<IMFAttributes> attributes;
        hr = MFCreateAttributes(&attributes, 2);
        if (FAILED(hr)) { MFShutdown(); return false; }
        
        attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

        ComPtr<IMFSourceReader> reader;
        hr = MFCreateSourceReaderFromURL(videoPath.c_str(), attributes.Get(), &reader);
        if (FAILED(hr)) { MFShutdown(); return false; }

        // Request RGB32 output
        ComPtr<IMFMediaType> mediaType;
        hr = MFCreateMediaType(&mediaType);
        if (FAILED(hr)) { MFShutdown(); return false; }

        mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

        hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType.Get());
        if (FAILED(hr)) { MFShutdown(); return false; }

        // Get actual dimensions
        ComPtr<IMFMediaType> outputType;
        hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &outputType);
        if (FAILED(hr)) { MFShutdown(); return false; }

        hr = MFGetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, &outWidth, &outHeight);
        if (FAILED(hr)) { MFShutdown(); return false; }

        // Seek to requested time
        PROPVARIANT var;
        PropVariantInit(&var);
        var.vt = VT_I8;
        var.hVal.QuadPart = seekTime;
        reader->SetCurrentPosition(GUID_NULL, var);
        PropVariantClear(&var);

        // Read the next available frame after seek
        DWORD streamIndex, flags;
        LONGLONG timestamp;
        ComPtr<IMFSample> sample;

        hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                 0, &streamIndex, &flags, &timestamp, &sample);
        if (FAILED(hr) || !sample || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            MFShutdown();
            return false;
        }

        ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr)) { MFShutdown(); return false; }

        BYTE* srcData = nullptr;
        DWORD srcLength = 0;
        hr = buffer->Lock(&srcData, nullptr, &srcLength);
        if (FAILED(hr)) { MFShutdown(); return false; }

        // MF RGB32 = BGRA. Convert to RGBA for stb_image_write.
        // Also, MF gives bottom-up rows, so we need to flip vertically.
        outPixels.resize(outWidth * outHeight * 4);
        UINT srcStride = outWidth * 4;
        for (UINT y = 0; y < outHeight; y++) {
            // MF can give top-down or bottom-up depending on codec.
            // RGB32 from MF is typically top-down. Copy row by row and swap B/R.
            const BYTE* srcRow = srcData + y * srcStride;
            BYTE* dstRow = &outPixels[y * outWidth * 4];
            for (UINT x = 0; x < outWidth; x++) {
                dstRow[x * 4 + 0] = srcRow[x * 4 + 2]; // R <- B
                dstRow[x * 4 + 1] = srcRow[x * 4 + 1]; // G
                dstRow[x * 4 + 2] = srcRow[x * 4 + 0]; // B <- R
                dstRow[x * 4 + 3] = 255;                // A
            }
        }

        buffer->Unlock();
        MFShutdown();
        return true;
    }

    // Check if the pixel data is mostly black (failed thumbnail)
    static bool IsFrameMostlyBlack(const std::vector<BYTE>& pixels, UINT32 width, UINT32 height) {
        if (pixels.empty()) return true;
        
        int totalPixels = width * height;
        int blackPixels = 0;
        int sampleStep = totalPixels > 1000 ? totalPixels / 1000 : 1; // Sample ~1000 pixels
        
        for (int i = 0; i < totalPixels; i += sampleStep) {
            int idx = i * 4;
            if (pixels[idx] < 10 && pixels[idx + 1] < 10 && pixels[idx + 2] < 10) {
                blackPixels++;
            }
        }
        
        int sampledCount = (totalPixels + sampleStep - 1) / sampleStep;
        return (blackPixels > sampledCount * 85 / 100); // >85% black
    }

    static bool SavePixelsAsJpg(const std::vector<BYTE>& pixels, UINT32 width, UINT32 height,
                                 const std::wstring& outPath) {
        // Convert wide path to narrow for stbi
        char path[512];
        WideCharToMultiByte(CP_UTF8, 0, outPath.c_str(), -1, path, (int)sizeof(path), NULL, NULL);
        
        return stbi_write_jpg(path, width, height, 4, pixels.data(), 90) != 0;
    }

    static std::wstring GenerateThumbFromVideo(const std::wstring& mediaPath, const std::wstring& thumbPath) {
        // Try extracting at different time offsets to avoid black frames
        // 100ns units: 2s = 20000000, 1s = 10000000, 0.5s = 5000000
        LONGLONG seekTimes[] = { 20000000LL, 10000000LL, 5000000LL, 0LL };
        
        for (int i = 0; i < 4; i++) {
            std::vector<BYTE> pixels;
            UINT32 w = 0, h = 0;
            
            if (ExtractFrameFromVideo(mediaPath, seekTimes[i], pixels, w, h)) {
                if (!IsFrameMostlyBlack(pixels, w, h)) {
                    if (SavePixelsAsJpg(pixels, w, h, thumbPath)) {
                        return thumbPath;
                    }
                }
                // If we got a frame but it was black, try next time offset
                // If it's the last try (0s), save it anyway
                if (i == 3 && !pixels.empty()) {
                    if (SavePixelsAsJpg(pixels, w, h, thumbPath)) {
                        return thumbPath;
                    }
                }
            }
        }
        
        return L"";
    }

    static std::wstring GenerateThumbFromGif(const std::wstring& mediaPath, const std::wstring& thumbPath) {
        // Use stb_image to extract the first frame
        char path[512];
        WideCharToMultiByte(CP_UTF8, 0, mediaPath.c_str(), -1, path, (int)sizeof(path), NULL, NULL);
        
        int w, h, channels;
        unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
        if (!data) return L"";
        
        char outPath[512];
        WideCharToMultiByte(CP_UTF8, 0, thumbPath.c_str(), -1, outPath, (int)sizeof(outPath), NULL, NULL);
        
        bool ok = stbi_write_jpg(outPath, w, h, 4, data, 90) != 0;
        stbi_image_free(data);
        
        return ok ? thumbPath : L"";
    }

    std::wstring ThumbnailGenerator::GenerateThumbnail(const std::wstring& mediaPath) {
        if (mediaPath.empty()) return L"";

        // If it's a shader, return a placeholder
        if (mediaPath.find(L".hlsl") != std::wstring::npos) {
            return L"assets\\shader_thumb.png";
        }

        std::wstring thumbDir = GetThumbnailsDirectory();
        if (thumbDir.empty()) return L"";

        // Generate a hash based on the file path to use as the thumbnail filename
        std::hash<std::wstring> hasher;
        size_t hash = hasher(mediaPath);
        std::wstring thumbPath = thumbDir + L"\\" + std::to_wstring(hash) + L".jpg";

        // Check if it already exists
        DWORD attrib = GetFileAttributesW(thumbPath.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return thumbPath;
        }

        // Determine file type by extension
        std::wstring ext;
        size_t dotPos = mediaPath.find_last_of(L".");
        if (dotPos != std::wstring::npos) {
            ext = mediaPath.substr(dotPos + 1);
            for (auto& c : ext) c = towlower(c);
        }

        if (ext == L"gif") {
            return GenerateThumbFromGif(mediaPath, thumbPath);
        } else {
            // MP4, AVI, etc — use Media Foundation
            return GenerateThumbFromVideo(mediaPath, thumbPath);
        }
    }

    std::wstring ThumbnailGenerator::RegenerateThumbnail(const std::wstring& mediaPath) {
        if (mediaPath.empty()) return L"";

        std::wstring thumbDir = GetThumbnailsDirectory();
        if (thumbDir.empty()) return L"";

        std::hash<std::wstring> hasher;
        size_t hash = hasher(mediaPath);
        std::wstring thumbPath = thumbDir + L"\\" + std::to_wstring(hash) + L".jpg";

        // Delete existing
        DeleteFileW(thumbPath.c_str());

        return GenerateThumbnail(mediaPath);
    }
}
