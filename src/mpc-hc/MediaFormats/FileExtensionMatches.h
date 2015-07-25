#pragma once

namespace MediaFormats
{
    bool IsVideoFilename(const CString& path);
    bool IsAudioFilename(const CString& path);
    bool IsPlayableFilename(const CString& path);
    bool IsPlaylistFilename(const CString& path);
}
