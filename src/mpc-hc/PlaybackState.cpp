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

#include "stdafx.h"
#include "PlaybackState.h"

#include "DSMPropertyBag.h"
#include "mplayerc.h"

PlaybackState::PlaybackState()
{
    GetEventd().Connect(m_eventc, {
        MpcEvent::MEDIA_CHAPTERBAG_CHANGED,
        MpcEvent::MEDIA_DURATION_CHANGED,
        MpcEvent::MEDIA_POSITION_CHANGED,
        MpcEvent::MEDIA_FILE_METADATA_CHANGED,
    });
}

PlaybackState::Pos PlaybackState::GetPos() const
{
    auto lock = m_mutex.GetSharedLock();
    return m_pos;
}

void PlaybackState::SetPos(const PlaybackState::Pos& pos)
{
    bool bChapterBagChanged, bDurationChanged, bPositionChanged;
    {
        auto lock = m_mutex.GetExclusiveLock();
        bChapterBagChanged = (m_pos.pChapterBag != pos.pChapterBag);
        bDurationChanged = (m_pos.rtDur != pos.rtDur);
        bPositionChanged = (m_pos.rtNow != pos.rtNow);
        m_pos = pos;
    }
    if (bChapterBagChanged) {
        m_eventc.FireEvent(MpcEvent::MEDIA_CHAPTERBAG_CHANGED);
    }
    if (bDurationChanged) {
        m_eventc.FireEvent(MpcEvent::MEDIA_DURATION_CHANGED);
    }
    if (bPositionChanged) {
        m_eventc.FireEvent(MpcEvent::MEDIA_POSITION_CHANGED);
    }
}

bool PlaybackState::HasDuration() const
{
    auto lock = m_mutex.GetSharedLock();
    return m_pos.rtDur > 0;
}

bool PlaybackState::HasChapterBag() const
{
    auto lock = m_mutex.GetSharedLock();
    return !!m_pos.pChapterBag;
}

PlaybackState::MediaFileMetadata PlaybackState::GetFileMetadata() const
{
    auto lock = m_mutex.GetSharedLock();
    return m_fileMetadata;
}

void PlaybackState::SetFileMetadata(const PlaybackState::MediaFileMetadata& metadata)
{
    bool bChanged = false;
    {
        auto lock = m_mutex.GetExclusiveLock();
        if (metadata != m_fileMetadata) {
            bChanged = true;
            m_fileMetadata = metadata;
        }
    }
    if (bChanged) {
        m_eventc.FireEvent(MpcEvent::MEDIA_FILE_METADATA_CHANGED);
    }
}
