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
        bDurationChanged = (m_pos.rtStart != pos.rtStart || m_pos.rtStop != pos.rtStop);
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
    return m_pos.rtStart < m_pos.rtStop;
}

bool PlaybackState::HasChapterBag() const
{
    auto lock = m_mutex.GetSharedLock();
    return !!m_pos.pChapterBag;
}
