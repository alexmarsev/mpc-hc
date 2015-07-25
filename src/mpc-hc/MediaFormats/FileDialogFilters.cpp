#include "stdafx.h"
#include "FileDialogFilters.h"

#include "Formats.h"

#include "../resource.h"

#include "DSUtil.h" // for ResStr()

namespace MediaFormats
{
    CString OpenAudioFilesFilter()
    {
        CString ret;
        ret += ResStr(IDS_AG_AUDIOFILES);
        ret += _T('|');
        auto f = MediaFormats_ExtensionsLambda(...) {
            if (format.type == Type::Audio) {
                ret += _T('*');
                ret += ext;
                ret += _T(';');
            }
        };
        IterateExtensions(f);
        ret += _T('|');
        ret += ResStr(IDS_AG_ALLFILES);
        ret += _T('|');
        return ret;
    }

    CString OpenKnownFilesFilter()
    {
        CString ret;
        ret += ResStr(IDS_AG_MEDIAFILES);
        ret += _T('|');
        auto f = MediaFormats_ExtensionsLambda(...) {
            ret += _T('*');
            ret += ext;
            ret += _T(';');
        };
        IterateExtensions(f);
        ret += _T('|');
        ret += ResStr(IDS_AG_ALLFILES);
        ret += _T('|');
        return ret;
    }
}
