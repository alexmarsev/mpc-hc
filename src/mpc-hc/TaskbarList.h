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
#include "MpcApi.h"

#include <array>

class CMainFrame;

class CTaskbarList
{
public:
    explicit CTaskbarList(CMainFrame* pMainFrame);
    ~CTaskbarList();

    void SetPlayState(MPC_PLAYSTATE state);
    void Update();

    void EnableStopButton(bool bEnable);
    void EnablePlayButton(bool bEnable);
    void EnablePauseButton(bool bEnable);
    void EnableBackButton(bool bEnable);
    void EnableForwardButton(bool bEnable);
    void EnableFullscreenButton(bool bEnable);

    ITaskbarList3* GetService();
    void OnCreate();

    bool EatWindowMessage(UINT message, WPARAM wParam, LPARAM lParam);

protected:
    void EventCallback(MpcEvent ev);

    void EnableButton(size_t idx, bool bEnable, bool bForcedUpdate = false);
    void UpdateButtons();

    void SetOverlayIconFromResource(LPTSTR lpszName);

    CMainFrame* m_pMainFrame;
    CComPtr<ITaskbarList3> m_taskbarService;

    EventClient m_eventc;

    std::array<THUMBBUTTON, 5> m_buttons;
    bool m_bButtonsInitialized = false;
    bool m_bButtonsHidden = false;
    bool m_bPlayEnabled = false;
    bool m_bShowingPause = false;

    MPC_PLAYSTATE m_playState = PS_STOP;
    bool m_bInLoadOrClose = false;
    bool m_bInLongLoadOrClose = false;
};
