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
        MpcEvent::VR_WINDOW_DATA_CHANGED,
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

CString PlaybackState::GetPosString(const PlaybackState::Pos& pos)
{
    const REFERENCE_TIME rtSec = 10000000;
    const bool bWide = (std::max(pos.rtNow, pos.rtDur) / rtSec > 3600);
    const bool bRemaining = (AfxGetAppSettings().fRemainingTime && pos.rtDur > 0 && pos.rtNow < pos.rtDur);

    auto getTimestamp = [&](REFERENCE_TIME rt) {
        const int s = static_cast<int>(rt / rtSec % 60);
        const int m = static_cast<int>(rt / rtSec / 60 % 60);
        const int h = static_cast<int>(rt / rtSec / 3600);

        CString timestamp;

        if (bWide) {
            timestamp.Format(_T("%02d:%02d:%02d"), h, m, s);
        } else {
            timestamp.Format(_T("%02d:%02d"), m, s);
        }

        return timestamp;
    };

    CString ret(bRemaining ? _T("-") : _T(""));

    ret += getTimestamp(bRemaining ? (pos.rtDur - pos.rtNow) : pos.rtNow);

    if (pos.rtDur > 0) {
        ret += _T(" / ");
        ret += getTimestamp(pos.rtDur);
    }

    return ret;
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

PlaybackState::GraphState PlaybackState::GetGraphState() const
{
    CComPtr<IMediaControl> pMediaControl;
    {
        auto lock = m_mutex.GetSharedLock();
        pMediaControl = m_pMediaControl;
    }
    PlaybackState::GraphState st;
    if (m_pMediaControl) {
        st.bAsync = (VFW_S_STATE_INTERMEDIATE ==
                     m_pMediaControl->GetState(0, reinterpret_cast<OAFilterState*>(&st.state)));
    }
    return st;
}

void PlaybackState::SetGraphStateSource(IMediaControl* pMediaControl)
{
    auto lock = m_mutex.GetExclusiveLock();
    m_pMediaControl = pMediaControl;
}

PlaybackState::VrWindowData PlaybackState::GetVrWindowData() const
{
    auto lock = m_mutex.GetSharedLock();
    return m_vrWindowData;
}

void PlaybackState::SetVrWindowData(const PlaybackState::VrWindowData& vrWindowData)
{
    bool bWindowDataChanged;
    {
        auto lock = m_mutex.GetExclusiveLock();
        bWindowDataChanged =
            (vrWindowData.windowRect != m_vrWindowData.windowRect) ||
            (vrWindowData.videoRect != m_vrWindowData.videoRect);
        if (bWindowDataChanged) {
            m_vrWindowData = vrWindowData;
        }
    }
    if (bWindowDataChanged) {
        m_eventc.FireEvent(MpcEvent::VR_WINDOW_DATA_CHANGED);
    }
}
