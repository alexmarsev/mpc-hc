#pragma once

#include "../Registration/Branding.h"

#define MediaFormats_FormatsLambda(...) [&](const MediaFormats::Format& format, const CString& progid)
#define MediaFormats_ExtensionsLambda(...) [&](const MediaFormats::Format& format, const CString& progid, const CString& ext)
#define MediaFormats_ProtocolsLambda(...) [&](const MediaFormats::Protocol& protocol, const CString& progid)
#define MediaFormats_ProtocolSchemesLambda(...) [&](const MediaFormats::Protocol& protocol, const CString& progid, const CString& scheme)

namespace MediaFormats
{
    enum class Type {
        Audio,
        Video,
        PlaylistVideo,
        PlaylistAudio,
        Misc,
    };

    struct Format {
        Type type;
        int icon;
        int friendlyName;
        const TCHAR* progidSuffix;
        const TCHAR* exts;
    };

    struct Protocol {
        const TCHAR* progidSuffix;
        int icon;
        int friendlyName;
        const TCHAR* schemes;
    };

    void GetFormats(const Format** formats, size_t* count);
    void GetProtocols(const Protocol** protocols, size_t* count);

    template <typename F>
    void IterateFormats(F&& f)
    {
        const Format* formats;
        size_t count;

        GetFormats(&formats, &count);

        for (size_t i = 0; i < count; i++) {
            f(formats[i], Registration::GetProgidPrefix() + formats[i].progidSuffix);
        }
    }

    template <typename F>
    void IterateExtensions(F&& f)
    {
        const auto sep = _T(" ");

        auto f0 = MediaFormats_FormatsLambda(...) {
            const CString exts = format.exts;
            int pos = 0;
            for (CString token = exts.Tokenize(sep, pos); !token.IsEmpty(); token = exts.Tokenize(sep, pos)) {
                ASSERT(token[0] == _T('.'));
                f(format, progid, token);
            }
        };

        IterateFormats(f0);
    }

    template <typename F>
    void IterateProtocols(F&& f)
    {
        const Protocol* protocols;
        size_t count;

        GetProtocols(&protocols, &count);

        for (size_t i = 0; i < count; i++) {
            f(protocols[i], Registration::GetProgidPrefix() + protocols[i].progidSuffix);
        }
    }

    template <typename F>
    void IterateProtocolSchemes(F&& f)
    {
        const auto sep = _T(" ");

        auto f0 = MediaFormats_ProtocolsLambda(...) {
            const CString schemes = protocol.schemes;
            int pos = 0;
            for (CString token = schemes.Tokenize(sep, pos); !token.IsEmpty(); token = schemes.Tokenize(sep, pos)) {
                f(protocol, progid, token);
            }
        };

        IterateProtocols(f0);
    }
}
