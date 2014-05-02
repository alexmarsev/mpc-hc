/*
 * (C) 2013-2014 see Authors.txt
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

#include <map>
#include <vector>

#include "EventDispatcher.h"

// TODO: handle touch gestures

class CMainFrame;

class CMouse
{
public:
    CMouse(CMainFrame* pMainFrm, bool bD3DFS = false);
    virtual ~CMouse();

    static inline bool PointEqualsImprecise(long a, long b) {
        const unsigned uDelta = 1;
        return abs(a - b) <= uDelta;
    }
    static inline bool PointEqualsImprecise(const CPoint& a, const CPoint& b) {
        return PointEqualsImprecise(a.x, b.x) && PointEqualsImprecise(a.y, b.y);
    }

    static UINT GetMouseFlags();

    static bool CursorOnRootWindow(const CPoint& screenPoint, const CFrameWnd& frameWnd);
    static bool CursorOnWindow(const CPoint& screenPoint, const CWnd& wnd);

    bool Dragging();

    enum class Cursor { NONE, ARROW, HAND };

    CPoint GetVideoPoint(const CPoint& clientPoint) const;
    CPoint GetClientPoint(const CPoint& videoPoint) const;

private:
    CMouse(const CMouse&);
    const bool m_bD3DFS;
    CMainFrame* m_pMainFrame;
    bool m_bMouseHiderStarted;
    CPoint m_mouseHiderStartScreenPoint;
    DWORD m_dwMouseHiderStartTick;
    bool m_bTrackingMouseLeave;
    enum class Drag { NO_DRAG, BEGIN_DRAG, DRAGGED } m_drag;
    std::map<Cursor, HCURSOR> m_cursors;
    Cursor m_cursor;
    CPoint m_beginDragPoint;
    CPoint m_hideCursorPoint;
    bool m_bLeftDown;
    bool m_bLeftDoubleStarted;
    CPoint m_leftDoubleStartPoint;
    int m_leftDoubleStartTime;
    int m_popupMenuUninitTime;
    bool m_bDraggingOsd;

    std::pair<bool, CPoint> m_switchingToFullscreen;

    virtual CWnd& GetWnd() = 0;

    void ResetToBlankState();
    void StartMouseHider(const CPoint& screenPoint);
    void StopMouseHider();
    void MouseHiderCallback();
    void StartMouseLeaveTracker();
    void StopMouseLeaveTracker();

    bool IsOnFullscreenWindow() const;
    bool OnButton(UINT id, const CPoint& point, bool bOnFullscreen);
    bool OnButton(UINT id, const CPoint& point);
    bool SelectCursor(const CPoint& screenPoint, const CPoint& clientPoint, UINT nFlags);
    void SetCursor(UINT nFlags, const CPoint& screenPoint, const CPoint& clientPoint);
    void SetCursor(UINT nFlags, const CPoint& clientPoint);
    void SetCursor(const CPoint& screenPoint);
    bool TestDrag(const CPoint& screenPoint);

    void CancelOsdDrag();

    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

protected:
    void InternalOnLButtonDown(UINT nFlags, const CPoint& point);
    void InternalOnLButtonUp(UINT nFlags, const CPoint& point);
    void InternalOnMButtonDown(UINT nFlags, const CPoint& point);
    void InternalOnMButtonUp(UINT nFlags, const CPoint& point);
    void InternalOnMButtonDblClk(UINT nFlags, const CPoint& point);
    void InternalOnRButtonDown(UINT nFlags, const CPoint& point);
    void InternalOnRButtonUp(UINT nFlags, const CPoint& point);
    void InternalOnRButtonDblClk(UINT nFlags, const CPoint& point);
    bool InternalOnXButtonDown(UINT nFlags, UINT nButton, const CPoint& point);
    bool InternalOnXButtonUp(UINT nFlags, UINT nButton, const CPoint& point);
    bool InternalOnXButtonDblClk(UINT nFlags, UINT nButton, const CPoint& point);
    BOOL InternalOnMouseWheel(UINT nFlags, short zDelta, const CPoint& point);
    BOOL InternalOnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    void InternalOnMouseMove(UINT nFlags, const CPoint& point);
    void InternalOnMouseLeave();
    void InternalOnCaptureChanged(CWnd* pWnd);
    void InternalOnDestroy();
};

class CMouseWnd : public CWnd, public CMouse
{
public:
    CMouseWnd(CMainFrame* pMainFrm, bool bD3DFS = false);

private:
    DECLARE_DYNAMIC(CMouseWnd)
    DECLARE_MESSAGE_MAP()

    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnLButtonUp(UINT nFlags, CPoint point);
    void OnLButtonDblClk(UINT nFlags, CPoint point);

    void OnMButtonDown(UINT nFlags, CPoint point);
    void OnMButtonUp(UINT nFlags, CPoint point);
    void OnMButtonDblClk(UINT nFlags, CPoint point);

    void OnRButtonDown(UINT nFlags, CPoint point);
    void OnRButtonUp(UINT nFlags, CPoint point);
    void OnRButtonDblClk(UINT nFlags, CPoint point);

    void OnXButtonDown(UINT nFlags, UINT nButton, CPoint point);
    void OnXButtonUp(UINT nFlags, UINT nButton, CPoint point);
    void OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point);

    BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint point);

    BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

    void OnMouseMove(UINT nFlags, CPoint point);

    void OnMouseLeave();

    void OnCaptureChanged(CWnd* pWnd);

    void OnDestroy();

    virtual CWnd& GetWnd() override final {
        return *this;
    }
};

template <class T>
class CMouseWheelHook
{
    HHOOK m_hHook;

    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL) {
            const auto& msex = *reinterpret_cast<MOUSEHOOKSTRUCTEX*>(lParam);
            if (CWnd* pFocus = CWnd::FromHandlePermanent(msex.hwnd)) {
                if (CWnd* pFocusRoot = pFocus->GetAncestor(GA_ROOT)) {
                    for (CWnd* pRoot : T::GetRoots()) {
                        ASSERT(pRoot);
                        // only intercept messages to focused windows that have white-listed root windows
                        if (pRoot && *pRoot == *pFocusRoot) {
                            if (CWnd* pUnder = CWnd::WindowFromPoint(msex.pt)) {
                                if (CWnd* pUnderRoot = pUnder->GetAncestor(GA_ROOT)) {
                                    if (*pUnderRoot == *pFocusRoot &&
                                            GetCurrentThreadId() == GetWindowThreadProcessId(pUnder->m_hWnd, nullptr)) {
                                        // prepare MSG struct
                                        MSG msg = {
                                            pUnder->m_hWnd,
                                            wParam,
                                            CMouse::GetMouseFlags() | msex.mouseData,
                                            MAKELPARAM(msex.pt.x, msex.pt.y),
                                            GetMessageTime(),
                                            msex.pt
                                        };

                                        // walk through pre-translate
                                        if (CWnd::WalkPreTranslateTree(pUnderRoot->m_hWnd, &msg)) {
                                            // the message shouldn't be dispatched
                                            return TRUE;
                                        }

                                        // dispatch the message
                                        if (msg.hwnd) {
                                            DispatchMessage(&msg);
                                        }
                                    }
                                }
                            }
                            return TRUE;
                        }
                    }
                }
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

public:
    CMouseWheelHook() {
        m_hHook = SetWindowsHookEx(WH_MOUSE, MouseProc, nullptr, GetCurrentThreadId());
        ASSERT(m_hHook);
    }

    virtual ~CMouseWheelHook() {
        if (m_hHook) {
            VERIFY(UnhookWindowsHookEx(m_hHook));
        }
    }
};

struct CMainFrameMouseHook : CMouseWheelHook<CMainFrameMouseHook> {
    static std::vector<CWnd*> GetRoots();
};
