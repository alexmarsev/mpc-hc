/*
 * (C) 2014 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "EventDispatcher.h"
#include "SharedMutex.h"

#include <map>

struct IDSMChapterBag;

class PlaybackState
{
public:
    PlaybackState();
    PlaybackState(const PlaybackState&) = delete;
    PlaybackState& operator=(const PlaybackState&) = delete;

    struct Pos {
        REFERENCE_TIME rtDur = 0;
        REFERENCE_TIME rtNow = 0;
        CComPtr<IDSMChapterBag> pChapterBag;
    };

    Pos GetPos() const;
    void SetPos(const Pos& pos);
    bool HasDuration() const;
    bool HasChapterBag() const;

    // currently represents IAMMediaContent interface
    // (or, more precisely, sub-portion of it)
    enum class MediaFileMetadataId
    {
        TITLE,
        AUTHOR_NAME,
        COPYRIGHT,
        RATING,
        DESCRIPTION,
    };

    typedef std::map<MediaFileMetadataId, CString> MediaFileMetadata;

    MediaFileMetadata GetFileMetadata() const;
    void SetFileMetadata(const MediaFileMetadata& metadata);

protected:
    EventClient m_eventc;
    mutable SharedMutex m_mutex;
    Pos m_pos;
    MediaFileMetadata m_fileMetadata;
};
