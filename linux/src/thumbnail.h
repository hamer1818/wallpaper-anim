#pragma once

#include <QString>

// Library thumbnails. The Windows build decodes a frame with Media Foundation; here
// ffmpeg (already a hard dependency of mpv) does the same job out of process.
namespace Thumbnail {

    bool IsFfmpegAvailable();

    // Returns the path of a generated JPEG, or an empty string when a thumbnail
    // cannot be produced (shaders, missing ffmpeg, unreadable media).
    QString Generate(const QString& mediaPath);

}
