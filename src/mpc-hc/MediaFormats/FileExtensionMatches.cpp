#include "stdafx.h"
#include "FileExtensionMatches.h"

#include "Formats.h"

#include "DSUtil.h" // for CStringUtils
#include "PathUtils.h"

#include <map>

namespace MediaFormats
{
    namespace
    {
        using Storage = std::map<CString, Format, CStringUtils::IgnoreCaseLess>;

        Storage InitStorage()
        {
            Storage storage;
            IterateExtensions(MediaFormats_ExtensionsLambda(...) { storage.emplace(ext, format); });
            return storage;
        }

        const Storage& GetStorage()
        {
            static const Storage storage = InitStorage();
            return storage;
        }

        Storage::const_iterator SearchStorage(const CString& path)
        {
            return GetStorage().find(PathUtils::FileExt(path));
        }
    }

    bool IsVideoFilename(const CString& path)
    {
        auto it = SearchStorage(path);
        if (it == GetStorage().cend()) {
            return false;
        }
        Type type = it->second.type;

        return type == Type::Video;
    }

    bool IsAudioFilename(const CString& path)
    {
        auto it = SearchStorage(path);
        if (it == GetStorage().cend()) {
            return false;
        }
        Type type = it->second.type;

        return type == Type::Audio;
    }

    bool IsPlayableFilename(const CString& path)
    {
        auto it = SearchStorage(path);
        if (it == GetStorage().cend()) {
            return false;
        }
        Type type = it->second.type;

        return type != Type::PlaylistVideo && type != Type::PlaylistAudio;
    }

    bool IsPlaylistFilename(const CString& path)
    {
        auto it = SearchStorage(path);
        if (it == GetStorage().cend()) {
            return false;
        }
        Type type = it->second.type;

        return type == Type::PlaylistVideo || type == Type::PlaylistAudio;
    }
}
