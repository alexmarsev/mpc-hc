/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014 see Authors.txt
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
#include "PlayerSeekBar.h"
#include "MainFrm.h"

#define TOOLTIP_SHOW_DELAY 100
#define TOOLTIP_HIDE_TIMEOUT 3000
#define HALT_THUMB_DRAG_TIMEOUT 100
#define ADD_TO_BOTTOM_WITHOUT_CONTROLBAR 2

CDraggableSeekbar::CDraggableSeekbar(CMainFrame* pMainFrame) : m_pMainFrame(pMainFrame)
{
    ENSURE(m_pMainFrame);
}

CDraggableSeekbar::~CDraggableSeekbar()
{
    StopThumbDrag();
}

bool CDraggableSeekbar::DraggingThumb() const
{
    return m_bDraggingThumb;
}

void CDraggableSeekbar::HaltThumbDrag()
{
    if (DraggingThumb()) {
        if (!m_bDragThumbHaltedOnce || m_rtDragThumbHaltPos != m_rtDragThumbPos) {
            m_bDragThumbHaltedOnce = true;
            m_rtDragThumbHaltPos = m_rtDragThumbPos;
            m_dragThumbHaltPoint = m_dragThumbHaltPoint;
            DraggedThumb(m_dragThumbHaltPoint, m_rtDragThumbHaltPos);
        }
    } else {
        ASSERT(FALSE);
    }
}

void CDraggableSeekbar::StartThumbDrag()
{
    ASSERT(!DraggingThumb());
    m_bDragThumbHaltedOnce = false;
    m_bDraggingThumb = true;
}

void CDraggableSeekbar::DragThumb(const CPoint& point, REFERENCE_TIME rtPos)
{
    if (DraggingThumb()) {
        m_dragThumbPoint = point;
        m_rtDragThumbPos = rtPos;
        m_pMainFrame->m_timerOneTime.Subscribe(
            CMainFrame::TimerOneTimeSubscriber::SEEKBAR_HALT_THUMB_DRAG,
            std::bind(&CDraggableSeekbar::HaltThumbDrag, this), HALT_THUMB_DRAG_TIMEOUT);
    }  else {
        ASSERT(FALSE);
    }
}

void CDraggableSeekbar::StopThumbDrag(bool bHard/* = true*/)
{
    if (DraggingThumb()) {
        m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::SEEKBAR_HALT_THUMB_DRAG);
        if (!bHard) {
            if (!m_bDragThumbHaltedOnce ||
                    (!CMouse::PointEqualsImprecise(m_dragThumbPoint.x, m_dragThumbHaltPoint.x) &&
                     m_rtDragThumbPos != m_rtDragThumbHaltPos)) {
                DraggedThumb(m_dragThumbPoint, m_rtDragThumbPos);
            }
        }
        m_bDraggingThumb = false;
    }
}

IMPLEMENT_DYNAMIC(CPlayerSeekBar, CDialogBar)

CPlayerSeekBar::CPlayerSeekBar(CMainFrame* pMainFrame)
    : CDraggableSeekbar(pMainFrame)
    , m_pMainFrame(pMainFrame)
    , m_bEnabled(false)
    , m_cursor(AfxGetApp()->LoadStandardCursor(IDC_HAND))
    , m_tooltipState(TooltipState::HIDDEN)
    , m_bIgnoreLastTooltipPoint(true)
{
    ZeroMemory(&m_ti, sizeof(m_ti));
    m_ti.cbSize = sizeof(m_ti);

    GetEventd().Connect(m_eventc, {
        MpcEvent::SWITCHING_TO_FULLSCREEN,
        MpcEvent::SWITCHING_FROM_FULLSCREEN,
        MpcEvent::SWITCHING_TO_FULLSCREEN_D3D,
        MpcEvent::SWITCHING_FROM_FULLSCREEN_D3D,
        MpcEvent::PLAYBACK_ENTERING_STOP,
        MpcEvent::MEDIA_CHAPTERBAG_CHANGED,
        MpcEvent::MEDIA_DURATION_CHANGED,
        MpcEvent::MEDIA_POSITION_CHANGED,
    }, std::bind(&CPlayerSeekBar::EventCallback, this, std::placeholders::_1));
}

CPlayerSeekBar::~CPlayerSeekBar()
{
    m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::SEEKBAR_TOOLTIP);
}

void CPlayerSeekBar::EventCallback(MpcEvent ev)
{
    switch (ev) {
        case MpcEvent::SWITCHING_TO_FULLSCREEN:
        case MpcEvent::SWITCHING_FROM_FULLSCREEN:
        case MpcEvent::SWITCHING_TO_FULLSCREEN_D3D:
        case MpcEvent::SWITCHING_FROM_FULLSCREEN_D3D:
        case MpcEvent::PLAYBACK_ENTERING_STOP:
            CancelDrag();
            break;
        case MpcEvent::MEDIA_CHAPTERBAG_CHANGED:
            Invalidate();
            break;
        case MpcEvent::MEDIA_DURATION_CHANGED:
            HideTooltip();
            CancelDrag();
            m_bEnabled = m_pMainFrame->GetPlaybackState()->HasDuration();
            Invalidate();
            break;
        case MpcEvent::MEDIA_POSITION_CHANGED:
            if (!DraggingThumb()) {
                MoveThumbToPositition(m_pMainFrame->GetPlaybackState()->GetPos().rtNow);
            }
            break;
        default:
            ASSERT(FALSE);
    }
}

BOOL CPlayerSeekBar::Create(CWnd* pParentWnd)
{
    if (!__super::Create(pParentWnd,
                         IDD_PLAYERSEEKBAR, WS_CHILD | WS_VISIBLE | CBRS_ALIGN_BOTTOM, IDD_PLAYERSEEKBAR)) {
        return FALSE;
    }

    // Should never be RTLed
    ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);

    m_tooltip.Create(this, TTS_NOPREFIX | TTS_ALWAYSTIP);
    m_tooltip.SetMaxTipWidth(SHRT_MAX);

    m_ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
    m_ti.hwnd = m_hWnd;
    m_ti.uId = (UINT_PTR)m_hWnd;
    m_ti.hinst = AfxGetInstanceHandle();
    m_ti.lpszText = nullptr;

    m_tooltip.SendMessage(TTM_ADDTOOL, 0, (LPARAM)&m_ti);

    return TRUE;
}

BOOL CPlayerSeekBar::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!__super::PreCreateWindow(cs)) {
        return FALSE;
    }

    m_dwStyle &= ~CBRS_BORDER_TOP;
    m_dwStyle &= ~CBRS_BORDER_BOTTOM;
    m_dwStyle |= CBRS_SIZE_FIXED;

    return TRUE;
}

CSize CPlayerSeekBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
    CSize ret = __super::CalcFixedLayout(bStretch, bHorz);
    ret.cy = m_pMainFrame->m_dpi.ScaleY(20);
    if (!m_pMainFrame->m_controls.ControlChecked(CMainFrameControls::Toolbar::CONTROLS)) {
        ret.cy += ADD_TO_BOTTOM_WITHOUT_CONTROLBAR;
    }
    return ret;
}

void CPlayerSeekBar::MoveThumb(const CPoint& point)
{
    if (m_pMainFrame->GetPlaybackState()->HasDuration()) {
        REFERENCE_TIME rtPos = PositionFromClientPoint(point);

        if (AfxGetAppSettings().bFastSeek ^ (GetKeyState(VK_SHIFT) < 0)) {
            rtPos = m_pMainFrame->GetClosestKeyFrame(rtPos);
        }

        MoveThumbToPositition(rtPos);
    }
}

void CPlayerSeekBar::MoveThumbToPositition(REFERENCE_TIME rtPos)
{
    if (m_pMainFrame->GetPlaybackState()->HasDuration()) {
        PlaybackState::Pos pos = m_pMainFrame->GetPlaybackState()->GetPos();

        m_rtThumbPos = std::max(0ll, std::min(pos.rtDur, rtPos));

        if (IsVisible()) {
            const CRect newThumbRect(GetThumbRect());

            if (newThumbRect != m_lastThumbRect) {
                InvalidateRect(newThumbRect | m_lastThumbRect);
            }
        }
    }
}

void CPlayerSeekBar::SyncMediaToThumb()
{
    ASSERT(IsVisible());
    ASSERT(m_pMainFrame->GetPlaybackState()->HasDuration());
    m_pMainFrame->SeekTo(m_rtThumbPos);
}

long CPlayerSeekBar::ChannelPointFromPosition(REFERENCE_TIME rtPos) const
{
    long ret = 0;

    PlaybackState::Pos pos = m_pMainFrame->GetPlaybackState()->GetPos();

    rtPos = std::min(pos.rtDur, std::max(0ll, rtPos));

    const int w = GetChannelRect().Width();

    if (pos.rtDur > 0) {
        ret = (long)(w * rtPos / pos.rtDur);
    }

    if (ret >= w) {
        ret = w - 1;
    }

    return ret;
}

REFERENCE_TIME CPlayerSeekBar::PositionFromClientPoint(const CPoint& point) const
{
    REFERENCE_TIME rtRet = 0;

    if (m_pMainFrame->GetPlaybackState()->HasDuration()) {
        PlaybackState::Pos pos = m_pMainFrame->GetPlaybackState()->GetPos();

        const CRect channelRect(GetChannelRect());
        const long channelPointX = (point.x < channelRect.left) ? channelRect.left :
                                   (point.x > channelRect.right) ? channelRect.right : point.x;
        ASSERT(channelPointX >= channelRect.left && channelPointX <= channelRect.right);

        rtRet = (channelPointX - channelRect.left) * pos.rtDur / channelRect.Width();
    }

    return rtRet;
}

void CPlayerSeekBar::DraggedThumb(const CPoint& point, REFERENCE_TIME rtPos)
{
    UNREFERENCED_PARAMETER(point);
    ASSERT(m_rtThumbPos == rtPos);
    SyncMediaToThumb();
}

void CPlayerSeekBar::CancelDrag()
{
    if (DraggingThumb()) {
        StopThumbDrag();
        ReleaseCapture();
    }
}

void CPlayerSeekBar::CreateThumb(bool bEnabled, CDC& parentDC)
{
    auto& pThumb = bEnabled ? m_pEnabledThumb : m_pDisabledThumb;
    pThumb = std::make_unique<CDC>();

    if (pThumb->CreateCompatibleDC(&parentDC)) {
        COLORREF
        white  = GetSysColor(COLOR_WINDOW),
        shadow = GetSysColor(COLOR_3DSHADOW),
        light  = GetSysColor(COLOR_3DHILIGHT),
        bkg    = GetSysColor(COLOR_BTNFACE);

        CRect r(GetThumbRect());
        r.MoveToXY(0, 0);
        CRect ri(GetInnerThumbRect(bEnabled, r));

        CBitmap bmp;
        VERIFY(bmp.CreateCompatibleBitmap(&parentDC, r.Width(), r.Height()));
        VERIFY(pThumb->SelectObject(bmp));

        pThumb->Draw3dRect(&r, light, 0);
        r.DeflateRect(0, 0, 1, 1);
        pThumb->Draw3dRect(&r, light, shadow);
        r.DeflateRect(1, 1, 1, 1);

        if (bEnabled) {
            pThumb->ExcludeClipRect(ri);
            ri.InflateRect(0, 1, 0, 1);
            pThumb->FillSolidRect(ri, white);
            pThumb->SetPixel(ri.CenterPoint().x, ri.top, 0);
            pThumb->SetPixel(ri.CenterPoint().x, ri.bottom - 1, 0);
        }
        pThumb->ExcludeClipRect(ri);

        ri.InflateRect(1, 1, 1, 1);
        pThumb->Draw3dRect(&ri, shadow, bkg);
        pThumb->ExcludeClipRect(ri);

        CBrush b(bkg);
        pThumb->FillRect(&r, &b);
    } else {
        ASSERT(FALSE);
    }
}

CRect CPlayerSeekBar::GetChannelRect() const
{
    CRect r;
    GetClientRect(&r);
    r.top += 1;
    if (m_pMainFrame->m_controls.ControlChecked(CMainFrameControls::Toolbar::CONTROLS)) {
        r.bottom += ADD_TO_BOTTOM_WITHOUT_CONTROLBAR;
    }
    CSize s(m_pMainFrame->m_dpi.ScaleFloorX(8), m_pMainFrame->m_dpi.ScaleFloorY(7) + 1);
    r.DeflateRect(s.cx, s.cy, s.cx, s.cy);
    return r;
}

CRect CPlayerSeekBar::GetThumbRect() const
{
    const CRect channelRect(GetChannelRect());
    const long x = channelRect.left + ChannelPointFromPosition(m_rtThumbPos);
    CSize s;
    s.cy = m_pMainFrame->m_dpi.ScaleFloorY(5);
    s.cx = m_pMainFrame->m_dpi.TransposeScaledY(channelRect.Height()) / 2 + s.cy;
    CRect r(x + 1 - s.cx, channelRect.top - s.cy, x + s.cx, channelRect.bottom + s.cy);
    return r;
}

CRect CPlayerSeekBar::GetInnerThumbRect(bool bEnabled, const CRect& thumbRect) const
{
    CSize s(m_pMainFrame->m_dpi.ScaleFloorX(4) - 1, m_pMainFrame->m_dpi.ScaleFloorY(5));
    if (!bEnabled) {
        s.cy -= 1;
    }
    CRect r(thumbRect);
    r.DeflateRect(s.cx, s.cy, s.cx, s.cy);
    return r;
}

void CPlayerSeekBar::UpdateTooltip(const CPoint& point)
{
    CRect clientRect;
    GetClientRect(&clientRect);

    if (!m_pMainFrame->GetPlaybackState()->HasDuration() || !clientRect.PtInRect(point)) {
        HideTooltip();
        return;
    }

    switch (m_tooltipState) {
        case TooltipState::HIDDEN:
            // if mouse moved or the tooltip wasn't hidden by timeout
            if (point != m_tooltipPoint || m_bIgnoreLastTooltipPoint) {
                m_bIgnoreLastTooltipPoint = false;

                // start show tooltip countdown
                m_tooltipState = TooltipState::TRIGGERED;
                m_pMainFrame->m_timerOneTime.Subscribe(
                    CMainFrame::TimerOneTimeSubscriber::SEEKBAR_TOOLTIP,
                    std::bind(&CPlayerSeekBar::ShowTooltip, this), TOOLTIP_SHOW_DELAY);

                // and track mouse leave
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd };
                VERIFY(TrackMouseEvent(&tme));
            }
            break;
        case TooltipState::TRIGGERED:
            // do nothing until the tooltip is shown
            break;
        case TooltipState::VISIBLE:
            // update the tooltip if needed
            ASSERT(!m_bIgnoreLastTooltipPoint);
            if (point != m_tooltipPoint) {
                m_tooltipPoint = point;
                UpdateTooltipPosition();
                UpdateTooltipText();
                m_pMainFrame->m_timerOneTime.Subscribe(
                    CMainFrame::TimerOneTimeSubscriber::SEEKBAR_TOOLTIP,
                    std::bind(&CPlayerSeekBar::HideTooltip, this), TOOLTIP_HIDE_TIMEOUT);
            }
            break;
        default:
            ASSERT(FALSE);
    }
}

void CPlayerSeekBar::UpdateTooltipPosition()
{
    CSize bubbleSize(m_tooltip.GetBubbleSize(&m_ti));
    CRect windowRect;
    GetWindowRect(windowRect);
    CPoint point(m_tooltipPoint);

    if (AfxGetAppSettings().nTimeTooltipPosition == TIME_TOOLTIP_ABOVE_SEEKBAR) {
        point.x -= bubbleSize.cx / 2 - 2;
        point.y = GetChannelRect().TopLeft().y - (bubbleSize.cy + m_pMainFrame->m_dpi.ScaleY(13));
    } else {
        point.x += m_pMainFrame->m_dpi.ScaleX(10);
        point.y += m_pMainFrame->m_dpi.ScaleY(20);
    }
    point.x = max(0l, min(point.x, windowRect.Width() - bubbleSize.cx));
    ClientToScreen(&point);

    m_tooltip.SendMessage(TTM_TRACKPOSITION, 0, MAKELPARAM(point.x, point.y));
}

void CPlayerSeekBar::UpdateTooltipText()
{
    ASSERT(IsVisible());
    ASSERT(m_pMainFrame->GetPlaybackState()->HasDuration());
    const REFERENCE_TIME rtNow = PositionFromClientPoint(m_tooltipPoint);

    CString time;
    GUID timeFormat = m_pMainFrame->GetTimeFormat();
    if (timeFormat == TIME_FORMAT_MEDIA_TIME) {
        DVD_HMSF_TIMECODE tcNow = RT2HMS_r(rtNow);
        if (tcNow.bHours > 0) {
            time.Format(_T("%02u:%02u:%02u"), tcNow.bHours, tcNow.bMinutes, tcNow.bSeconds);
        } else {
            time.Format(_T("%02u:%02u"), tcNow.bMinutes, tcNow.bSeconds);
        }
    } else if (timeFormat == TIME_FORMAT_FRAME) {
        time.Format(_T("%I64d"), rtNow);
    } else {
        ASSERT(FALSE);
    }

    CComBSTR chapterName;
    if (m_pMainFrame->GetPlaybackState()->HasChapterBag()) {
        PlaybackState::Pos pos = m_pMainFrame->GetPlaybackState()->GetPos();
        REFERENCE_TIME rt = rtNow;
        pos.pChapterBag->ChapLookup(&rt, &chapterName);
    }

    if (chapterName.Length() == 0) {
        m_tooltipText = time;
    } else {
        m_tooltipText.Format(_T("%s - %s"), time, chapterName);
    }

    m_ti.lpszText = (LPTSTR)(LPCTSTR)m_tooltipText;
    m_tooltip.SetToolInfo(&m_ti);
}

void CPlayerSeekBar::ShowTooltip()
{
    if (m_tooltipState != TooltipState::VISIBLE) {
        CPoint point;
        VERIFY(GetCursorPos(&point));
        ScreenToClient(&point);

        m_tooltipPoint = point;
        UpdateTooltipText();
        m_tooltip.SendMessage(TTM_TRACKACTIVATE, TRUE, (LPARAM)&m_ti);
        UpdateTooltipPosition();

        m_tooltipState = TooltipState::VISIBLE;
        m_pMainFrame->m_timerOneTime.Subscribe(
            CMainFrame::TimerOneTimeSubscriber::SEEKBAR_TOOLTIP,
            std::bind(&CPlayerSeekBar::HideTooltip, this), TOOLTIP_HIDE_TIMEOUT);
    } else {
        ASSERT(FALSE);
    }
}

void CPlayerSeekBar::HideTooltip()
{
    if (m_tooltipState != TooltipState::HIDDEN) {
        m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::SEEKBAR_TOOLTIP);
        m_tooltip.SendMessage(TTM_TRACKACTIVATE, FALSE, (LPARAM)&m_ti);
        m_tooltipState = TooltipState::HIDDEN;
    }
}

BEGIN_MESSAGE_MAP(CPlayerSeekBar, CDialogBar)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONUP()
    ON_WM_XBUTTONDOWN()
    ON_WM_XBUTTONUP()
    ON_WM_XBUTTONDBLCLK()
    ON_WM_MOUSEMOVE()
    ON_WM_ERASEBKGND()
    ON_WM_SETCURSOR()
    ON_WM_TIMER()
    ON_WM_MOUSELEAVE()
    ON_WM_THEMECHANGED()
    ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

void CPlayerSeekBar::OnPaint()
{
    CPaintDC dc(this);

    COLORREF
    dark   = GetSysColor(COLOR_GRAYTEXT),
    white  = GetSysColor(COLOR_WINDOW),
    shadow = GetSysColor(COLOR_3DSHADOW),
    light  = GetSysColor(COLOR_3DHILIGHT),
    bkg    = GetSysColor(COLOR_BTNFACE);

    auto ps = m_pMainFrame->GetPlaybackState();

    // Thumb
    {
        auto& pThumb = m_bEnabled ? m_pEnabledThumb : m_pDisabledThumb;
        if (!pThumb) {
            CreateThumb(m_bEnabled, dc);
            ASSERT(pThumb);
        }
        CRect r(GetThumbRect());
        CRect ri(GetInnerThumbRect(m_bEnabled, r));

        CRgn rg, rgi;
        VERIFY(rg.CreateRectRgnIndirect(&r));
        VERIFY(rgi.CreateRectRgnIndirect(&ri));

        ExtSelectClipRgn(dc, rgi, RGN_DIFF);
        VERIFY(dc.BitBlt(r.TopLeft().x, r.TopLeft().y, r.Width(), r.Height(), pThumb.get(), 0, 0, SRCCOPY));
        ExtSelectClipRgn(dc, rg, RGN_XOR);

        m_lastThumbRect = r;
    }

    const CRect channelRect(GetChannelRect());

    // Chapters
    if (ps->HasDuration() && ps->HasChapterBag()) {
        PlaybackState::Pos pos = ps->GetPos();
        for (DWORD i = 0; i < pos.pChapterBag->ChapGetCount(); i++) {
            REFERENCE_TIME rtChap;
            if (SUCCEEDED(pos.pChapterBag->ChapGet(i, &rtChap, nullptr))) {
                long chanPos = channelRect.left + ChannelPointFromPosition(rtChap);
                CRect r(chanPos, channelRect.top, chanPos + 1, channelRect.bottom);
                if (r.right < channelRect.right) {
                    r.right++;
                }
                ASSERT(r.right <= channelRect.right);
                dc.FillSolidRect(&r, dark);
                dc.ExcludeClipRect(&r);
            } else {
                ASSERT(FALSE);
            }
        }
    }

    // Channel
    {
        dc.FillSolidRect(&channelRect, m_bEnabled ? white : bkg);
        CRect r(channelRect);
        r.InflateRect(1, 1);
        dc.Draw3dRect(&r, shadow, light);
        dc.ExcludeClipRect(&r);
    }

    // Background
    {
        CRect r;
        GetClientRect(&r);
        dc.FillSolidRect(&r, bkg);
    }
}

void CPlayerSeekBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_bEnabled && m_pMainFrame->GetPlaybackState()->HasDuration()) {
        SetCapture();
        StartThumbDrag();
        MoveThumb(point);
        DragThumb(point, m_rtThumbPos);
    } else if (!m_pMainFrame->m_fFullScreen) {
        MapWindowPoints(m_pMainFrame, &point, 1);
        m_pMainFrame->PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
    }
}

void CPlayerSeekBar::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    OnLButtonDown(nFlags, point);
}

void CPlayerSeekBar::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (DraggingThumb()) {
        StopThumbDrag(false);
        ReleaseCapture();
    }
}

void CPlayerSeekBar::OnXButtonDown(UINT nFlags, UINT nButton, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(point);
    // do medium jumps when clicking mouse navigation buttons on the seekbar
    // if not dragging the seekbar thumb
    if (!DraggingThumb()) {
        switch (nButton) {
            case XBUTTON1:
                SendMessage(WM_COMMAND, ID_PLAY_SEEKBACKWARDMED);
                break;
            case XBUTTON2:
                SendMessage(WM_COMMAND, ID_PLAY_SEEKFORWARDMED);
                break;
        }
    }
}

void CPlayerSeekBar::OnXButtonUp(UINT nFlags, UINT nButton, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(nButton);
    UNREFERENCED_PARAMETER(point);
    // do nothing
}

void CPlayerSeekBar::OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point)
{
    OnXButtonDown(nFlags, nButton, point);
}

void CPlayerSeekBar::OnMouseMove(UINT nFlags, CPoint point)
{
    if (DraggingThumb()) {
        if (nFlags & MK_LBUTTON) {
            MoveThumb(point);
            DragThumb(point, m_rtThumbPos);
        } else {
            StopThumbDrag();
        }
    }
    if (AfxGetAppSettings().fUseTimeTooltip) {
        UpdateTooltip(point);
    }
}

BOOL CPlayerSeekBar::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

BOOL CPlayerSeekBar::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    BOOL ret = TRUE;
    if (m_bEnabled && m_pMainFrame->GetPlaybackState()->HasDuration()) {
        ::SetCursor(m_cursor);
    } else {
        ret = __super::OnSetCursor(pWnd, nHitTest, message);
    }
    return ret;
}

void CPlayerSeekBar::OnMouseLeave()
{
    HideTooltip();
    m_bIgnoreLastTooltipPoint = true;
}

LRESULT CPlayerSeekBar::OnThemeChanged()
{
    m_pEnabledThumb = nullptr;
    m_pDisabledThumb = nullptr;
    return __super::OnThemeChanged();
}

void CPlayerSeekBar::OnCaptureChanged(CWnd* pWnd)
{
    StopThumbDrag();
    if (!pWnd) {
        // HACK: windowed (not renderless) video renderers may not produce WM_MOUSEMOVE message here
        m_pMainFrame->UpdateControlState(CMainFrame::UPDATE_CHILDVIEW_CURSOR_HACK);
    }
}
