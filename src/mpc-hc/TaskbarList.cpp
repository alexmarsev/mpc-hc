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
#include "TaskbarList.h"

#include "MainFrm.h"
#include "mplayerc.h"

#define LONG_LOAD_OR_CLOSE_THRESHOLD_MS 1000
#define PROGRESS_STEPS_COUNT 1024

enum {
    B_PREV = 0,
    B_STOP,
    B_PLAYPAUSE,
    B_NEXT,
    B_FULLSCREEN,
};

CTaskbarList::CTaskbarList(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
{
    ENSURE(m_pMainFrame);

    GetEventd().Connect(m_eventc, {
        MpcEvent::MEDIA_DURATION_CHANGED,
        MpcEvent::MEDIA_POSITION_CHANGED,
    }, std::bind(&CTaskbarList::EventCallback, this, std::placeholders::_1));

    m_taskbarService.CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER);

    for (auto& button : m_buttons) {
        button = { THB_BITMAP | THB_TOOLTIP | THB_FLAGS };
        button.dwFlags = THBF_DISABLED;
    }

    m_buttons[B_PREV].iId = IDTB_BUTTON3;
    m_buttons[B_STOP].iId = IDTB_BUTTON1;
    m_buttons[B_PLAYPAUSE].iId = IDTB_BUTTON2;
    m_buttons[B_NEXT].iId = IDTB_BUTTON4;
    m_buttons[B_FULLSCREEN].iId = IDTB_BUTTON5;

    m_buttons[B_PREV].iBitmap = 0;
    m_buttons[B_STOP].iBitmap = 1;
    m_buttons[B_PLAYPAUSE].iBitmap = 3;
    m_buttons[B_NEXT].iBitmap = 4;
    m_buttons[B_FULLSCREEN].iBitmap = 5;
}

CTaskbarList::~CTaskbarList()
{
    m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::TASKBAR_LONG_LOAD_OR_CLOSE);
}

void CTaskbarList::SetPlayState(MPC_PLAYSTATE state)
{
    if (state != m_playState) {
        m_playState = state;
        Update();
    }
}

void CTaskbarList::Update()
{
    const auto& s = AfxGetAppSettings();
    const MLS mls = m_pMainFrame->GetLoadState();

    {
        const auto timerId = CMainFrame::TimerOneTimeSubscriber::TASKBAR_LONG_LOAD_OR_CLOSE;

        if (mls != MLS::LOADING && mls != MLS::CLOSING) {
            m_bInLoadOrClose = m_bInLongLoadOrClose = false;
            m_pMainFrame->m_timerOneTime.Unsubscribe(timerId);
        } else if (!m_bInLoadOrClose) {
            m_bInLoadOrClose = true;
            auto cb = [this]() {
                m_bInLongLoadOrClose = true;
                Update();
            };
            m_pMainFrame->m_timerOneTime.Subscribe(timerId, std::move(cb), LONG_LOAD_OR_CLOSE_THRESHOLD_MS);
        }

    }

    if (m_taskbarService) {
        if (s.fUseWin7TaskBar && mls == MLS::LOADED) {
            auto ps = m_pMainFrame->GetPlaybackState();
            switch (m_playState) {
                case PS_PLAY:
                    if (ps->HasDuration()) {
                        VERIFY(SUCCEEDED(m_taskbarService->SetProgressState(m_pMainFrame->m_hWnd, TBPF_NORMAL)));
                    } else {
                        VERIFY(SUCCEEDED(m_taskbarService->SetProgressState(m_pMainFrame->m_hWnd, TBPF_NOPROGRESS)));
                    }
                    SetOverlayIconFromResource(MAKEINTRESOURCE(IDR_TB_PLAY));
                    break;
                case PS_PAUSE:
                    if (ps->HasDuration()) {
                        VERIFY(SUCCEEDED(m_taskbarService->SetProgressState(m_pMainFrame->m_hWnd, TBPF_PAUSED)));
                    } else {
                        VERIFY(SUCCEEDED(m_taskbarService->SetProgressState(m_pMainFrame->m_hWnd, TBPF_NOPROGRESS)));
                    }
                    SetOverlayIconFromResource(MAKEINTRESOURCE(IDR_TB_PAUSE));
                    break;
                case PS_STOP:
                    VERIFY(SUCCEEDED(m_taskbarService->SetProgressState(m_pMainFrame->m_hWnd, TBPF_NOPROGRESS)));
                    SetOverlayIconFromResource(MAKEINTRESOURCE(IDR_TB_STOP));
                    break;
                default:
                    ASSERT(FALSE);
            }
        } else if (m_bInLongLoadOrClose) {
            VERIFY(SUCCEEDED(m_taskbarService->SetProgressState(m_pMainFrame->m_hWnd, TBPF_INDETERMINATE)));
            SetOverlayIconFromResource(nullptr);
        } else {
            VERIFY(SUCCEEDED(m_taskbarService->SetProgressState(m_pMainFrame->m_hWnd, TBPF_NOPROGRESS)));
            SetOverlayIconFromResource(nullptr);
        }
    }

}

void CTaskbarList::EnableStopButton(bool bEnable)
{
    EnableButton(B_STOP, bEnable);
}

void CTaskbarList::EnablePlayButton(bool bEnable)
{
    m_bPlayEnabled = bEnable;
    if (!m_bShowingPause) {
        EnableButton(B_PLAYPAUSE, bEnable);
    }
}

void CTaskbarList::EnablePauseButton(bool bEnable)
{
    if (m_bShowingPause && !bEnable) {
        m_bShowingPause = false;
        EnableButton(B_PLAYPAUSE, m_bPlayEnabled, true);
    } else if (!m_bShowingPause && bEnable) {
        m_bShowingPause = true;
        EnableButton(B_PLAYPAUSE, bEnable, true);
    }
}

void CTaskbarList::EnableBackButton(bool bEnable)
{
    EnableButton(B_PREV, bEnable);
}

void CTaskbarList::EnableForwardButton(bool bEnable)
{
    EnableButton(B_NEXT, bEnable);
}

void CTaskbarList::EnableFullscreenButton(bool bEnable)
{
    EnableButton(B_FULLSCREEN, bEnable);
}

ITaskbarList3* CTaskbarList::GetService()
{
    return m_taskbarService;
}

void CTaskbarList::OnCreate()
{
    m_bButtonsInitialized = false;
    UpdateButtons();
    Update();
}

bool CTaskbarList::EatWindowMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    bool ret = false;

    if ((message == WM_COMMAND) && (THBN_CLICKED == HIWORD(wParam))) {
        switch (LOWORD(wParam)) {
            case IDTB_BUTTON1:
                m_pMainFrame->SendMessage(WM_COMMAND, ID_PLAY_STOP);
                break;
            case IDTB_BUTTON2:
                m_pMainFrame->SendMessage(WM_COMMAND, ID_PLAY_PLAYPAUSE);
                break;
            case IDTB_BUTTON3:
                m_pMainFrame->SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPBACK);
                break;
            case IDTB_BUTTON4:
                m_pMainFrame->SendMessage(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
                break;
            case IDTB_BUTTON5:
                // when the button is clicked, taskbar has the focus
                m_pMainFrame->SetForegroundWindow();

                m_pMainFrame->SendMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
                break;
            default:
                ASSERT(FALSE);
        }
        ret = true;
    }

    return ret;
}

void CTaskbarList::EventCallback(MpcEvent ev)
{
    const auto& s = AfxGetAppSettings();
    auto ps = m_pMainFrame->GetPlaybackState();

    switch (ev) {
        case MpcEvent::MEDIA_DURATION_CHANGED:
            Update();
            break;
        case MpcEvent::MEDIA_POSITION_CHANGED:
            if (m_taskbarService && s.fUseWin7TaskBar && ps->HasDuration()) {
                const PlaybackState::Pos pos = ps->GetPos();
                VERIFY(SUCCEEDED(m_taskbarService->SetProgressValue(
                                     m_pMainFrame->m_hWnd,
                                     1 + pos.rtNow * (PROGRESS_STEPS_COUNT - 1) / pos.rtDur,
                                     PROGRESS_STEPS_COUNT)));
            }
            break;
        default:
            ASSERT(FALSE);
    }
}

void CTaskbarList::EnableButton(size_t idx, bool bEnable, bool bForcedUpdate/* = false*/)
{
    const auto& s = AfxGetAppSettings();

    bool bUpdate = !m_bButtonsInitialized || bForcedUpdate ||
                   (m_bButtonsInitialized && s.fUseWin7TaskBar == m_bButtonsHidden);

    const bool bDisabled = !!(m_buttons[idx].dwFlags & THBF_DISABLED);

    if (bDisabled && bEnable) {
        bUpdate = true;
        m_buttons[idx].dwFlags &= ~THBF_DISABLED;
    } else if (!bDisabled && !bEnable) {
        bUpdate = true;
        m_buttons[idx].dwFlags |= THBF_DISABLED;
    }

    if (bUpdate) {
        UpdateButtons();
    }
}

void CTaskbarList::UpdateButtons()
{
    const auto& s = AfxGetAppSettings();

    if (m_taskbarService) {
        if (s.fUseWin7TaskBar) {
            _tcscpy_s(m_buttons[B_PREV].szTip, ResStr(IDS_AG_PREVIOUS));
            _tcscpy_s(m_buttons[B_STOP].szTip, ResStr(IDS_AG_STOP));
            _tcscpy_s(m_buttons[B_PLAYPAUSE].szTip, ResStr(IDS_AG_PLAYPAUSE));
            _tcscpy_s(m_buttons[B_NEXT].szTip, ResStr(IDS_AG_NEXT));
            _tcscpy_s(m_buttons[B_FULLSCREEN].szTip, ResStr(IDS_AG_FULLSCREEN));

            m_buttons[B_PLAYPAUSE].iBitmap = m_bShowingPause ? 2 : 3;

            if (!m_bButtonsInitialized) {
                CMPCPngImage image;
                if (image.Load(MAKEINTRESOURCE(IDF_WIN7_TOOLBAR))) {
                    BITMAP bitmap;
                    image.GetBitmap(&bitmap);

                    int num = bitmap.bmWidth / bitmap.bmHeight;
                    HIMAGELIST hImageList = ImageList_Create(bitmap.bmHeight, bitmap.bmHeight, ILC_COLOR32, num, 0);

                    ImageList_Add(hImageList, (HBITMAP)image, 0);
                    VERIFY(SUCCEEDED(m_taskbarService->ThumbBarSetImageList(m_pMainFrame->m_hWnd, hImageList)));
                    ImageList_Destroy(hImageList);

                    image.CleanUp();
                } else {
                    ASSERT(FALSE);
                }

                m_bButtonsInitialized = SUCCEEDED(m_taskbarService->ThumbBarAddButtons(
                                                      m_pMainFrame->m_hWnd, (UINT)m_buttons.size(), m_buttons.data()));
                ASSERT(m_bButtonsInitialized);
            } else {
                VERIFY(SUCCEEDED(m_taskbarService->ThumbBarUpdateButtons(
                                     m_pMainFrame->m_hWnd, (UINT)m_buttons.size(), m_buttons.data())));
            }

            m_bButtonsHidden = false;
        } else if (m_bButtonsInitialized && !m_bButtonsHidden) {
            auto buttons = m_buttons; // copy
            for (auto& button : buttons) {
                button.dwFlags |= THBF_HIDDEN;
            }

            VERIFY(SUCCEEDED(m_taskbarService->ThumbBarUpdateButtons(
                                 m_pMainFrame->m_hWnd, (UINT)buttons.size(), buttons.data())));

            m_bButtonsHidden = true;
        }
    }
}

void CTaskbarList::SetOverlayIconFromResource(LPTSTR lpszName)
{
    if (lpszName) {
        HICON hIcon = (HICON)LoadImage(AfxGetInstanceHandle(), lpszName, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        VERIFY(SUCCEEDED(m_taskbarService->SetOverlayIcon(m_pMainFrame->m_hWnd, hIcon, L"")));
        DestroyIcon(hIcon);
    } else {
        VERIFY(SUCCEEDED(m_taskbarService->SetOverlayIcon(m_pMainFrame->m_hWnd, nullptr, L"")));
    }
}
