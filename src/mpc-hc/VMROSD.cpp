/*
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
#include "VMROSD.h"
#include "mplayerc.h"
#include "DSMPropertyBag.h"
#include "MainFrm.h"

#define SEEKBAR_HEIGHT       60
#define SLIDER_BAR_MARGIN    10
#define SLIDER_BAR_HEIGHT    10
#define SLIDER_CURSOR_HEIGHT 30
#define SLIDER_CURSOR_WIDTH  15
#define SLIDER_CHAP_WIDTH    4
#define SLIDER_CHAP_HEIGHT   10

CVMROSD::CVMROSD(CMainFrame* pMainFrame)
    : m_pMainFrame(pMainFrame)
    , m_pWnd(nullptr)
    , m_llSeekMin(0)
    , m_llSeekMax(0)
    , m_llSeekPos(0)
    , m_nMessagePos(OSD_NOMESSAGE)
    , m_bShowSeekBar(false)
    , m_bSeekBarVisible(false)
    , m_bCursorMoving(false)
    , m_pMFVMB(nullptr)
    , m_pVMB(nullptr)
    , m_pMVTO(nullptr)
    , m_iFontSize(0)
    , m_fontName(_T(""))
    , m_bShowMessage(true)
    , m_pCB(nullptr)
{
    m_colors[OSD_TRANSPARENT] = RGB(0,     0,   0);
    m_colors[OSD_BACKGROUND]  = RGB(32,   40,  48);
    m_colors[OSD_BORDER]      = RGB(48,   56,  62);
    m_colors[OSD_TEXT]        = RGB(224, 224, 224);
    m_colors[OSD_BAR]         = RGB(64,   72,  80);
    m_colors[OSD_CURSOR]      = RGB(192, 200, 208);
    m_colors[OSD_DEBUGCLR]    = RGB(128, 136, 144);

    m_penBorder.CreatePen(PS_SOLID, 1, m_colors[OSD_BORDER]);
    m_penCursor.CreatePen(PS_SOLID, 4, m_colors[OSD_CURSOR]);
    m_brushBack.CreateSolidBrush(m_colors[OSD_BACKGROUND]);
    m_brushBar.CreateSolidBrush(m_colors[OSD_BAR]);
    m_brushChapter.CreateSolidBrush(m_colors[OSD_CURSOR]);
    m_debugBrushBack.CreateSolidBrush(m_colors[OSD_DEBUGCLR]);
    m_debugPenBorder.CreatePen(PS_SOLID, 1, m_colors[OSD_BORDER]);

    ZeroMemory(&m_bitmapInfo, sizeof(m_bitmapInfo));
    ZeroMemory(&m_VMR9AlphaBitmap, sizeof(m_VMR9AlphaBitmap));
    ZeroMemory(&m_MFVideoAlphaBitmap, sizeof(m_MFVideoAlphaBitmap));
}

CVMROSD::~CVMROSD()
{
    Stop();
    m_memDC.DeleteDC();
}

void CVMROSD::SetSize(const CRect& wndRect, const CRect& videoRect)
{
    if (m_pWnd && (m_pVMB || m_pMFVMB)) {
        if (m_bSeekBarVisible) {
            m_bCursorMoving   = false;
            m_bSeekBarVisible = false;
            Invalidate();
        }

        // Vanilla VMR9/EVR renderers draw the OSD relative to the video frame
        const CAppSettings& s = AfxGetAppSettings();
        m_rectWnd = (s.iDSVideoRendererType != VIDRNDT_DS_VMR9WINDOWED
                     && s.iDSVideoRendererType != VIDRNDT_DS_EVR) ? wndRect : videoRect;
        m_rectWnd.MoveToXY(0, 0);

        m_rectSeekBar.left   = m_rectWnd.left;
        m_rectSeekBar.right  = m_rectWnd.right;
        m_rectSeekBar.top    = m_rectWnd.bottom  - SEEKBAR_HEIGHT;
        m_rectSeekBar.bottom = m_rectSeekBar.top + SEEKBAR_HEIGHT;

        UpdateBitmap();
    }
}

void CVMROSD::UpdateBitmap()
{
    CAutoLock lock(&m_csLock);
    CWindowDC dc(m_pWnd);

    m_memDC.DeleteDC();
    ZeroMemory(&m_bitmapInfo, sizeof(m_bitmapInfo));

    if (m_memDC.CreateCompatibleDC(&dc)) {
        BITMAPINFO bmi;
        HBITMAP    hbmpRender;

        ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = m_rectWnd.Width();
        bmi.bmiHeader.biHeight = - m_rectWnd.Height(); // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        hbmpRender = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);
        m_memDC.SelectObject(hbmpRender);

        if (::GetObject(hbmpRender, sizeof(BITMAP), &m_bitmapInfo) != 0) {
            // Configure the VMR's bitmap structure
            if (m_pVMB) {
                ZeroMemory(&m_VMR9AlphaBitmap, sizeof(m_VMR9AlphaBitmap));
                m_VMR9AlphaBitmap.dwFlags      = VMRBITMAP_HDC | VMRBITMAP_SRCCOLORKEY;
                m_VMR9AlphaBitmap.hdc          = m_memDC;
                m_VMR9AlphaBitmap.rSrc         = m_rectWnd;
                m_VMR9AlphaBitmap.rDest.left   = 0;
                m_VMR9AlphaBitmap.rDest.top    = 0;
                m_VMR9AlphaBitmap.rDest.right  = 1.0;
                m_VMR9AlphaBitmap.rDest.bottom = 1.0;
                m_VMR9AlphaBitmap.fAlpha       = 1.0;
                m_VMR9AlphaBitmap.clrSrcKey    = m_colors[OSD_TRANSPARENT];
            } else if (m_pMFVMB) {
                ZeroMemory(&m_MFVideoAlphaBitmap, sizeof(m_MFVideoAlphaBitmap));
                m_MFVideoAlphaBitmap.params.dwFlags        = MFVideoAlphaBitmap_SrcColorKey;
                m_MFVideoAlphaBitmap.params.clrSrcKey      = m_colors[OSD_TRANSPARENT];
                m_MFVideoAlphaBitmap.params.rcSrc          = m_rectWnd;
                m_MFVideoAlphaBitmap.params.nrcDest.right  = 1;
                m_MFVideoAlphaBitmap.params.nrcDest.bottom = 1;
                m_MFVideoAlphaBitmap.GetBitmapFromDC       = TRUE;
                m_MFVideoAlphaBitmap.bitmap.hdc            = m_memDC;
            }
            m_memDC.SetTextColor(m_colors[OSD_TEXT]);
            m_memDC.SetBkMode(TRANSPARENT);
        }

        if (m_mainFont.GetSafeHandle()) {
            m_memDC.SelectObject(m_mainFont);
        }

        DeleteObject(hbmpRender);
    }
}

void CVMROSD::Start(CWnd* pWnd, IVMRMixerBitmap9* pVMB, bool bShowSeekBar)
{
    m_pVMB   = pVMB;
    m_pMFVMB = nullptr;
    m_pMVTO  = nullptr;
    m_pWnd   = pWnd;
    m_bShowSeekBar = bShowSeekBar;
    UpdateBitmap();
}

void CVMROSD::Start(CWnd* pWnd, IMFVideoMixerBitmap* pMFVMB, bool bShowSeekBar)
{
    m_pMFVMB = pMFVMB;
    m_pVMB   = nullptr;
    m_pMVTO  = nullptr;
    m_pWnd   = pWnd;
    m_bShowSeekBar = bShowSeekBar;
    UpdateBitmap();
}

void CVMROSD::Start(CWnd* pWnd, IMadVRTextOsd* pMVTO)
{
    m_pMFVMB = nullptr;
    m_pVMB   = nullptr;
    m_pMVTO  = pMVTO;
    m_pWnd   = pWnd;
}

void CVMROSD::Stop()
{
    m_pVMB.Release();
    m_pMFVMB.Release();
    m_pMVTO.Release();
    if (m_pWnd) {
        m_pWnd->KillTimer((UINT_PTR)this);
        m_pWnd = nullptr;
    }
}

void CVMROSD::SetVideoWindow(CWnd* pWnd)
{
    CAutoLock lock(&m_csLock);

    if (m_pWnd) {
        m_pWnd->KillTimer((UINT_PTR)this);
    }
    m_pWnd = pWnd;
    m_pWnd->SetTimer((UINT_PTR)this, 1000, TimerFunc);
    UpdateBitmap();
}

void CVMROSD::DrawRect(const CRect* rect, CBrush* pBrush, CPen* pPen)
{
    if (pPen) {
        m_memDC.SelectObject(pPen);
    } else {
        m_memDC.SelectStockObject(NULL_PEN);
    }

    if (pBrush) {
        m_memDC.SelectObject(pBrush);
    } else {
        m_memDC.SelectStockObject(HOLLOW_BRUSH);
    }

    m_memDC.Rectangle(rect);
}

void CVMROSD::DrawSlider(CRect* rect, __int64 llMin, __int64 llMax, __int64 llPos)
{
    m_rectBar.left   = rect->left    + SLIDER_BAR_MARGIN;
    m_rectBar.right  = rect->right   - SLIDER_BAR_MARGIN;
    m_rectBar.top    = rect->top     + (rect->Height() - SLIDER_BAR_HEIGHT) / 2;
    m_rectBar.bottom = m_rectBar.top + SLIDER_BAR_HEIGHT;

    if (llMax == llMin) {
        m_rectCursor.left = m_rectBar.left;
    } else {
        m_rectCursor.left = m_rectBar.left + (long)(m_rectBar.Width() * llPos / (llMax - llMin));
    }
    m_rectCursor.left  -= SLIDER_CURSOR_WIDTH / 2;
    m_rectCursor.right  = m_rectCursor.left + SLIDER_CURSOR_WIDTH;
    m_rectCursor.top    = rect->top + (rect->Height() - SLIDER_CURSOR_HEIGHT) / 2;
    m_rectCursor.bottom = m_rectCursor.top + SLIDER_CURSOR_HEIGHT;

    DrawRect(rect, &m_brushBack, &m_penBorder);
    DrawRect(&m_rectBar, &m_brushBar);

    if (m_pCB && m_pCB->ChapGetCount() > 1 && llMax != llMin) {
        REFERENCE_TIME rt;
        for (DWORD i = 0; i < m_pCB->ChapGetCount(); ++i) {
            if (SUCCEEDED(m_pCB->ChapGet(i, &rt, nullptr))) {
                __int64 pos = m_rectBar.Width() * rt / (llMax - llMin);
                if (pos < 0) {
                    continue;
                }

                CRect r;
                r.left = m_rectBar.left + (LONG)pos - SLIDER_CHAP_WIDTH / 2;
                r.top = rect->top + (rect->Height() - SLIDER_CHAP_HEIGHT) / 2;
                r.right = r.left + SLIDER_CHAP_WIDTH;
                r.bottom = r.top + SLIDER_CHAP_HEIGHT;

                DrawRect(&r, &m_brushChapter);
            }
        }
    }

    DrawRect(&m_rectCursor, nullptr, &m_penCursor);
}

void CVMROSD::DrawMessage()
{
    if (!m_bitmapInfo.bmWidth || !m_bitmapInfo.bmHeight || !m_bitmapInfo.bmBitsPixel) {
        return;
    }
    if (m_nMessagePos != OSD_NOMESSAGE) {
        CRect rectRestrictedWnd(m_rectWnd);
        rectRestrictedWnd.DeflateRect(10, 10);
        CRect rectText;
        CRect rectOSD;

        DWORD uFormat = DT_SINGLELINE | DT_NOPREFIX;
        m_memDC.DrawText(m_strMessage, &rectText, uFormat | DT_CALCRECT);
        rectText.InflateRect(10, 5);

        rectOSD = rectText;
        switch (m_nMessagePos) {
            case OSD_TOPLEFT:
                rectOSD.MoveToXY(10, 10);
                break;
            case OSD_TOPRIGHT:
            default:
                rectOSD.MoveToXY(m_rectWnd.Width() - rectText.Width() - 10, 10);
                break;
        }
        rectOSD &= rectRestrictedWnd;

        DrawRect(&rectOSD, &m_brushBack, &m_penBorder);
        uFormat |= DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS;
        rectOSD.DeflateRect(10, 5);
        m_memDC.DrawText(m_strMessage, &rectOSD, uFormat);
    }
}

void CVMROSD::DrawDebug()
{
    if (!m_debugMessages.IsEmpty()) {
        CString msg, tmp;
        POSITION pos;
        pos = m_debugMessages.GetHeadPosition();
        msg.Format(_T("%s"), m_debugMessages.GetNext(pos));

        while (pos) {
            tmp = m_debugMessages.GetNext(pos);
            if (!tmp.IsEmpty()) {
                msg.AppendFormat(_T("\r\n%s"), tmp);
            }
        }

        CRect rectText(0, 0, 0, 0);
        CRect rectMessages;
        m_memDC.DrawText(msg, &rectText, DT_CALCRECT);
        rectText.InflateRect(20, 10);

        int l, r, t, b;
        l = (m_rectWnd.Width()  >> 1) - (rectText.Width()  >> 1) - 10;
        r = (m_rectWnd.Width()  >> 1) + (rectText.Width()  >> 1) + 10;
        t = (m_rectWnd.Height() >> 1) - (rectText.Height() >> 1) - 10;
        b = (m_rectWnd.Height() >> 1) + (rectText.Height() >> 1) + 10;
        rectMessages = CRect(l, t, r, b);
        DrawRect(&rectMessages, &m_debugBrushBack, &m_debugPenBorder);
        m_memDC.DrawText(msg, &rectMessages, DT_CENTER | DT_VCENTER);
    }
}

void CVMROSD::Invalidate()
{
    CAutoLock lock(&m_csLock);
    if (!m_bitmapInfo.bmWidth || !m_bitmapInfo.bmHeight || !m_bitmapInfo.bmBitsPixel) {
        return;
    }
    memsetd(m_bitmapInfo.bmBits, 0xff000000, m_bitmapInfo.bmWidth * m_bitmapInfo.bmHeight * (m_bitmapInfo.bmBitsPixel / 8));

    if (m_bSeekBarVisible) {
        DrawSlider(&m_rectSeekBar, m_llSeekMin, m_llSeekMax, m_llSeekPos);
    }
    DrawMessage();
    DrawDebug();

    if (m_pVMB) {
        m_VMR9AlphaBitmap.dwFlags &= ~VMRBITMAP_DISABLE;
        m_pVMB->SetAlphaBitmap(&m_VMR9AlphaBitmap);
    } else if (m_pMFVMB) {
        m_pMFVMB->SetAlphaBitmap(&m_MFVideoAlphaBitmap);
    }

    if (m_pMainFrame->GetMediaState() == State_Paused) {
        m_pMainFrame->RepaintVideo();
    }
}

void CVMROSD::UpdateSeekBarPos(CPoint point)
{
    m_llSeekPos = (point.x - m_rectBar.left) * (m_llSeekMax - m_llSeekMin) / (m_rectBar.Width() - SLIDER_CURSOR_WIDTH);
    m_llSeekPos = max(m_llSeekPos, m_llSeekMin);
    m_llSeekPos = min(m_llSeekPos, m_llSeekMax);

    if (AfxGetAppSettings().bFastSeek ^ (GetKeyState(VK_SHIFT) < 0)) {
        m_llSeekPos = m_pMainFrame->GetClosestKeyFrame(m_llSeekPos);
    }

    if (m_pWnd) {
        AfxGetApp()->GetMainWnd()->PostMessage(WM_HSCROLL, MAKEWPARAM((short)m_llSeekPos, SB_THUMBTRACK), (LPARAM)m_pWnd->m_hWnd);
    }
}

bool CVMROSD::OnMouseMove(UINT nFlags, CPoint point)
{
    bool bRet = false;

    if (m_pVMB || m_pMFVMB) {
        if (m_bCursorMoving) {
            bRet = true;
            UpdateSeekBarPos(point);
            Invalidate();
        } else if (m_bShowSeekBar && m_rectSeekBar.PtInRect(point)) {
            bRet = true;
            if (!m_bSeekBarVisible) {
                m_bSeekBarVisible = true;
                Invalidate();
            }
        } else if (m_bSeekBarVisible) {
            OnMouseLeave();
        }
    }

    return bRet;
}

void CVMROSD::OnMouseLeave()
{
    const bool bHideSeekbar = (m_pVMB || m_pMFVMB) && m_bSeekBarVisible;
    m_bCursorMoving = false;
    m_bSeekBarVisible = false;

    if (bHideSeekbar) {
        // Add new timer for removing any messages
        if (m_pWnd) {
            m_pWnd->KillTimer((UINT_PTR)this);
            m_pWnd->SetTimer((UINT_PTR)this, 1000, TimerFunc);
        }
        Invalidate();
    }
}

bool CVMROSD::OnLButtonDown(UINT nFlags, CPoint point)
{
    bool bRet = false;
    if (m_pVMB || m_pMFVMB) {
        if (m_rectCursor.PtInRect(point)) {
            m_bCursorMoving = true;
            bRet = true;
            if (m_pWnd) {
                ASSERT(dynamic_cast<CMouseWnd*>(m_pWnd));
                m_pWnd->SetCapture();
            }
        } else if (m_rectSeekBar.PtInRect(point)) {
            bRet = true;
            UpdateSeekBarPos(point);
            Invalidate();
        }
    }

    return bRet;
}

bool CVMROSD::OnLButtonUp(UINT nFlags, CPoint point)
{
    bool bRet = false;

    if (m_pVMB || m_pMFVMB) {
        m_bCursorMoving = false;

        bRet = (m_rectCursor.PtInRect(point) || m_rectSeekBar.PtInRect(point));
    }
    return bRet;
}

__int64 CVMROSD::GetPos() const
{
    return m_llSeekPos;
}

void CVMROSD::SetPos(__int64 pos)
{
    m_llSeekPos = pos;
    if (m_bSeekBarVisible) {
        Invalidate();
    }
}

void CVMROSD::SetRange(__int64 start, __int64 stop)
{
    m_llSeekMin = start;
    m_llSeekMax = stop;
}

void CVMROSD::GetRange(__int64& start, __int64& stop)
{
    start = m_llSeekMin;
    stop  = m_llSeekMax;
}

void CVMROSD::TimerFunc(HWND hWnd, UINT nMsg, UINT_PTR nIDEvent, DWORD dwTime)
{
    CVMROSD* pVMROSD = (CVMROSD*)nIDEvent;
    if (pVMROSD) {
        pVMROSD->ClearMessage();
    }
    KillTimer(hWnd, nIDEvent);
}

void CVMROSD::ClearMessage(bool hide)
{
    CAutoLock lock(&m_csLock);
    if (m_bSeekBarVisible) {
        return;
    }

    if (!hide) {
        m_nMessagePos = OSD_NOMESSAGE;
    }

    if (m_pVMB) {
        DWORD dwBackup = (m_VMR9AlphaBitmap.dwFlags | VMRBITMAP_DISABLE);
        m_VMR9AlphaBitmap.dwFlags = VMRBITMAP_DISABLE;
        m_pVMB->SetAlphaBitmap(&m_VMR9AlphaBitmap);
        m_VMR9AlphaBitmap.dwFlags = dwBackup;
    } else if (m_pMFVMB) {
        m_pMFVMB->ClearAlphaBitmap();
    } else if (m_pMVTO) {
        m_pMVTO->OsdClearMessage();
    }

    if (m_pMainFrame->GetMediaState() == State_Paused) {
        m_pMainFrame->RepaintVideo();
    }
}

void CVMROSD::DisplayMessage(OSD_MESSAGEPOS nPos, LPCTSTR strMsg, int nDuration, int iFontSize, CString fontName)
{
    if (!m_bShowMessage) {
        return;
    }

    if (m_pVMB || m_pMFVMB) {
        if (nPos != OSD_DEBUG) {
            m_nMessagePos = nPos;
            m_strMessage  = strMsg;
        } else {
            m_debugMessages.AddTail(strMsg);
            if (m_debugMessages.GetCount() > 20) {
                m_debugMessages.RemoveHead();
            }
            nDuration = -1;
        }

        int iOldFontSize = m_iFontSize;
        CString oldFontName = m_fontName;
        const CAppSettings& s = AfxGetAppSettings();

        if (iFontSize == 0) {
            m_iFontSize = s.osd.fontSize;
        } else {
            m_iFontSize = iFontSize;
        }
        if (m_iFontSize < 10 || m_iFontSize > 26) {
            m_iFontSize = 20;
        }
        if (fontName.IsEmpty()) {
            m_fontName = s.osd.fontName;
        } else {
            m_fontName = fontName;
        }

        if (iOldFontSize != m_iFontSize || oldFontName != m_fontName) {
            if (m_mainFont.GetSafeHandle()) {
                m_mainFont.DeleteObject();
            }

            m_mainFont.CreatePointFont(m_iFontSize * 10, m_fontName);
            m_memDC.SelectObject(m_mainFont);
        }

        if (m_pWnd) {
            m_pWnd->KillTimer((UINT_PTR)this);
            if (nDuration != -1) {
                m_pWnd->SetTimer((UINT_PTR)this, nDuration, TimerFunc);
            }
        }
        Invalidate();
    } else if (m_pMVTO) {
        m_pMVTO->OsdDisplayMessage(strMsg, nDuration);
    }
}

void CVMROSD::DebugMessage(LPCTSTR format, ...)
{
    CString msg;
    va_list argList;
    va_start(argList, format);
    msg.FormatV(format, argList);
    va_end(argList);

    DisplayMessage(OSD_DEBUG, msg);
}

void CVMROSD::HideMessage(bool hide)
{
    if (m_pVMB || m_pMFVMB) {
        if (hide) {
            ClearMessage(true);
        } else {
            Invalidate();
        }
    }
}

void CVMROSD::EnableShowMessage(bool enabled)
{
    m_bShowMessage = enabled;
}

void CVMROSD::EnableShowSeekBar(bool enabled)
{
    m_bShowSeekBar = enabled;
}

void CVMROSD::SetChapterBag(IDSMChapterBag* pCB)
{
    CAutoLock lock(&m_csLock);
    m_pCB = pCB;
    Invalidate();
}

void CVMROSD::RemoveChapters()
{
    SetChapterBag(nullptr);
}

#include <mutex>
#include <tree.hh>

#define OSD_SEEKBAR_HEIGHT 50
#define OSD_SEEKBAR_HIDE_DELAY_MS 125
#define FADE_IN_TIME_MS 125
#define FADE_OUT_TIME_MS 1000
#define PROGRESS_MESSAGE_BUFFER_MS 3000

const REFERENCE_TIME ms = 10000;

struct OsdGdiBitmap {
    HBITMAP hBitmap = nullptr;
    CSize bitmapSize = { 0, 0 };
    float fAlpha = 1.0f;

    OsdGdiBitmap() = default;
    OsdGdiBitmap(const OsdGdiBitmap&) = delete;
    OsdGdiBitmap& operator=(const OsdGdiBitmap&) = delete;

    OsdGdiBitmap(OsdGdiBitmap&& other) : hBitmap(other.hBitmap), bitmapSize(other.bitmapSize), fAlpha(other.fAlpha) {
        other.hBitmap = nullptr;
    }

    virtual ~OsdGdiBitmap() {
        DeleteBitmap(hBitmap);
    }
};

class OsdGdiPainter
{
public:
    CSize CalcMessageSize(const CString& msg) {
        ASSERT(m_bInitialized);
        CRect rect;
        rect.SetRectEmpty();

        if (m_bInitialized) {
            m_dc.DrawText(msg, &rect, DT_CALCRECT | DT_NOPREFIX);
        }

        return rect.Size();
    }

    bool PaintMessage(const CString& msg, const CSize& maxSize, OsdGdiBitmap& out, bool& bTrimmed) {
        ASSERT(m_bInitialized);
        bool ret = false;
        const CSize padding = m_dpiHelper->ScaleXY(10, 5);

        if (m_bInitialized && padding.cx * 2 < maxSize.cx && padding.cy * 2 < maxSize.cy) {
            m_dc.SelectObject(&m_penBorder);
            m_dc.SelectObject(&m_brushBack);

            CRect rect(CPoint(0, 0), CalcMessageSize(msg));

            rect.InflateRect(0, 0, padding.cx * 2, padding.cy * 2);
            if (rect.Width() > maxSize.cx || rect.Height() > maxSize.cy) {
                bTrimmed = true;
                VERIFY(rect.IntersectRect(rect, CRect(CPoint(0, 0), maxSize)));
            } else {
                bTrimmed = false;
            }

            out.hBitmap = CreateBitmap(rect.Width(), rect.Height());

            if (out.hBitmap) {
                out.bitmapSize = rect.Size();
                ret = true;

                if (HGDIOBJ hOld = m_dc.SelectObject(out.hBitmap)) {
                    VERIFY(m_dc.Rectangle(rect));

                    rect.DeflateRect(padding.cx, padding.cy, padding.cx, padding.cy);
                    const UINT format = DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX | (bTrimmed ? DT_END_ELLIPSIS : 0);
                    VERIFY(m_dc.DrawText(msg, rect, format));

                    SetAlphaByte(out.hBitmap, 0xff);

                    out.fAlpha = m_fAlpha;

                    VERIFY(m_dc.SelectObject(hOld));
                } else {
                    ASSERT(FALSE);
                }
            }
        }

        return ret;
    }

    bool PaintSeekbar(REFERENCE_TIME rtDur, IDSMChapterBag* pChapterBag, const CSize& maxSize,
                      OsdGdiBitmap& outBack, OsdGdiBitmap& outThumb,
                      std::function<CRect(REFERENCE_TIME)>& outFuncRectFromPos,
                      std::function<REFERENCE_TIME(CPoint)>& outFuncPosFromPoint) {
        ASSERT(m_bInitialized);

        bool ret = false;

        ASSERT(OSD_SEEKBAR_HEIGHT % 2 == 0);
        const CSize backSize(maxSize.cx, m_dpiHelper->ScaleY(OSD_SEEKBAR_HEIGHT));
        const CSize chanMargin = m_dpiHelper->ScaleXY(12, 19);
        const CSize thumbSize(m_dpiHelper->ScaleX(20), m_dpiHelper->ScaleY(12) * 2);
        const CSize thumbBorder(m_dpiHelper->ScaleX(6), thumbSize.cy / 2 - backSize.cy / 2 + chanMargin.cy);
        const int chapWidth = m_dpiHelper->ScaleX(1) * 2;

        if (m_bInitialized && chanMargin.cx * 3 < maxSize.cx && backSize.cy <= maxSize.cy) {
            // initialize thumb positioning functions
            outFuncRectFromPos = [ = ](REFERENCE_TIME rtPos) {
                rtPos = std::min(rtDur, std::max(0ll, rtPos));
                const float fPos = (float)rtPos / rtDur;
                const int x = chanMargin.cx + (int)((backSize.cx - chanMargin.cx * 2) * fPos) - thumbSize.cx / 2;
                const int y = (backSize.cy - thumbSize.cy) / 2;
                return CRect(CPoint(x, y), thumbSize);
            };
            outFuncPosFromPoint = [ = ](CPoint point) {
                float fPos = (float)(point.x - chanMargin.cx) / (backSize.cx - chanMargin.cx * 2);
                fPos = std::min(1.0f, std::max(0.0f, fPos));
                return (REFERENCE_TIME)(rtDur * fPos);
            };

            outBack.hBitmap = CreateBitmap(backSize.cx, backSize.cy);
            outThumb.hBitmap = CreateBitmap(thumbSize.cx, thumbSize.cy);

            if (outBack.hBitmap && outThumb.hBitmap) {
                outBack.bitmapSize = backSize;
                outThumb.bitmapSize = thumbSize;
                ret = true;

                // paint back
                if (HGDIOBJ hOld = m_dc.SelectObject(outBack.hBitmap)) {
                    // background
                    m_dc.SelectObject(&m_penBorder);
                    m_dc.SelectObject(&m_brushBack);
                    CRect rect(CPoint(0, 0), backSize);
                    VERIFY(m_dc.Rectangle(rect));

                    // channel
                    rect.DeflateRect(chanMargin);
                    m_dc.FillRect(rect, &m_brushChan);

                    // chapters
                    if (pChapterBag) {
                        CRect chapRect = rect;
                        for (DWORD i = 0; i < pChapterBag->ChapGetCount(); i++) {
                            REFERENCE_TIME rtChap;
                            if (SUCCEEDED(pChapterBag->ChapGet(i, &rtChap, nullptr))) {
                                CPoint chapPoint = outFuncRectFromPos(rtChap).CenterPoint();
                                chapRect.left = chapPoint.x - chapWidth / 2;
                                chapRect.right = chapRect.left + chapWidth;
                                m_dc.FillRect(chapRect, &m_brushChap);
                            } else {
                                ASSERT(FALSE);
                            }
                        }
                    }

                    SetAlphaByte(outBack.hBitmap, 0xff);

                    outBack.fAlpha = m_fAlpha;

                    VERIFY(m_dc.SelectObject(hOld));
                } else {
                    ASSERT(FALSE);
                }

                // paint thumb
                if (HGDIOBJ hOld = m_dc.SelectObject(outThumb.hBitmap)) {
                    // outer
                    m_dc.SelectObject(&m_penBorder);
                    m_dc.SelectObject(&m_brushThumb);
                    CRect rect(CPoint(0, 0), thumbSize);
                    VERIFY(m_dc.Rectangle(rect));

                    // inner
                    m_dc.SelectObject(&m_brushChan);
                    rect.DeflateRect(thumbBorder);
                    rect.InflateRect(m_penWidth, m_penWidth, m_penWidth, m_penWidth);
                    VERIFY(m_dc.Rectangle(rect));

                    SetAlphaByte(outThumb.hBitmap, 0xff);

                    // alpha magic
                    rect.DeflateRect(m_penWidth, m_penWidth, m_penWidth, m_penWidth);
                    m_dc.FillRect(rect, &m_brushChan);

                    outThumb.fAlpha = 1.0f;

                    VERIFY(m_dc.SelectObject(hOld));
                } else {
                    ASSERT(FALSE);
                }
            } else {
                DeleteBitmap(outBack.hBitmap);
                DeleteBitmap(outThumb.hBitmap);
            }
        }

        return ret;
    }

    bool Initialize(std::shared_ptr<const DPI> dpiHelper) {
        ENSURE(dpiHelper);

        const auto& s = AfxGetAppSettings();

        bool ret = true;

        // TODO: check for handle leak

        m_dpiHelper = dpiHelper;

        m_dc.DeleteDC();
        ret = m_dc.CreateCompatibleDC(&CWindowDC(AfxGetMainFrame())) && ret;

        m_penWidth = std::min(m_dpiHelper->ScaleX(1), m_dpiHelper->ScaleY(1));
        m_penBorder.DeleteObject();
        ret = m_penBorder.CreatePen(PS_INSIDEFRAME, m_penWidth, s.osd.colorBorder) && ret;

        m_brushBack.DeleteObject();
        ret = m_brushBack.CreateSolidBrush(s.osd.colorBackground) && ret;
        m_brushChan.DeleteObject();
        ret = m_brushChan.CreateSolidBrush(s.osd.colorChannel) && ret;
        m_brushThumb.DeleteObject();
        ret = m_brushThumb.CreateSolidBrush(s.osd.colorThumb) && ret;
        m_brushChap.DeleteObject();
        ret = m_brushChap.CreateSolidBrush(s.osd.colorChapter) && ret;

        m_font.DeleteObject();
        if (!m_font.CreatePointFont(s.osd.fontSize * 10, s.osd.fontName)) {
            // try to fall back
            ret = m_font.CreatePointFont(s.osd.fontSize * 10, _T("Arial")) && ret;
        }

        m_fAlpha = s.osd.fAlpha;
        ASSERT(m_fAlpha >= 0.0f && m_fAlpha <= 1.0f);

        m_dc.SelectObject(&m_font);
        m_dc.SetTextColor(s.osd.colorText);
        m_dc.SetBkMode(TRANSPARENT);

        m_bInitialized = ret;
        return ret;
    }

protected:
    HBITMAP CreateBitmap(long w, long h) {
        BITMAPINFO bmi = { sizeof(BITMAPINFOHEADER) };
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        return CreateDIBSection(m_dc, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);
    }

    void SetAlphaByte(HBITMAP hBitmap, BYTE alpha) {
        BITMAP bitmap = { 0 };
        if (GetObject(hBitmap, sizeof(bitmap), &bitmap)) {
            ASSERT(bitmap.bmBitsPixel == 32);
            for (int i = 0; i < bitmap.bmHeight; i++) {
                for (int j = 3; j < bitmap.bmWidthBytes; j += 4) {
                    static_cast<BYTE*>(bitmap.bmBits)[i * bitmap.bmWidthBytes + j] = alpha;
                }
            }
        }
    }

    std::shared_ptr<const DPI> m_dpiHelper;
    CDC m_dc;
    CPen m_penBorder;
    int m_penWidth = 0;
    CBrush m_brushBack, m_brushChan, m_brushThumb, m_brushChap;
    CFont m_font;
    bool m_bInitialized = false;
    EventClient m_eventc;
    float m_fAlpha = 1.0f;
};

bool TextureFromBitmap(IDirect3DDevice9* pDev, IDirect3DTexture9** ppTex, HBITMAP hBitmap)
{
    bool bRet = false;

    BITMAP bitmap = { 0 };
    if (pDev && ppTex && GetObject(hBitmap, sizeof(bitmap), &bitmap)) {
        CComPtr<IDirect3DSurface9> memSurf;
        if (SUCCEEDED(pDev->CreateOffscreenPlainSurface(bitmap.bmWidth, bitmap.bmHeight, D3DFMT_A8R8G8B8,
                      D3DPOOL_SYSTEMMEM, &memSurf, nullptr))) {
            D3DLOCKED_RECT rect;
            if (SUCCEEDED(memSurf->LockRect(&rect, nullptr, 0))) {
                ASSERT(bitmap.bmBitsPixel == 32);
                for (int i = 0; i < bitmap.bmHeight; i++) {
                    memcpy(static_cast<BYTE*>(rect.pBits) + i * rect.Pitch,
                           static_cast<BYTE*>(bitmap.bmBits) + i * bitmap.bmWidthBytes,
                           std::min<size_t>(rect.Pitch, bitmap.bmWidthBytes));
                }
                if (SUCCEEDED(memSurf->UnlockRect())) {
                    if (SUCCEEDED(pDev->CreateTexture(bitmap.bmWidth, bitmap.bmHeight, 1, D3DUSAGE_RENDERTARGET,
                                                      D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, ppTex, nullptr))) {
                        CComPtr<IDirect3DSurface9> surf;
                        if (SUCCEEDED((*ppTex)->GetSurfaceLevel(0, &surf))) {
                            bRet = SUCCEEDED(pDev->UpdateSurface(memSurf, nullptr, surf, nullptr));
                        }
                        if (!bRet) {
                            SAFE_RELEASE(*ppTex);
                        }
                    }
                }
            }
        }
    }

    return bRet;
}

template <typename IdType, IdType RootId>
class OsdCompositor
{
public:
    enum class Anchor
    {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    struct Location {
        Anchor anchor = Anchor::TopLeft;
        CRect rect = { 0, 0, 0, 0 };
    };

    struct GdiItem {
        std::shared_ptr<const OsdGdiBitmap> bitmap;
        IdType parent = RootId;
        Location location;

        float fAlpha = 1.0f;
        bool bHidden = false;

        virtual ~GdiItem() = default;
    };

    enum class ItemAnimationInfo
    {
        Playing,
        Paused,
        Discard,
        Parentless,
    };

    // this functor doesn't provide notifications for switching to another animator or static bitmap
    typedef std::function<bool(GdiItem& bitmap, REFERENCE_TIME rtFrame, ItemAnimationInfo info)> GdiItemAnimation;

    virtual ~OsdCompositor() = default;

    virtual void BeginQueue() = 0;
    virtual void EndQueue() = 0;

    virtual CRect GetLayoutRect() = 0;

    virtual void PlaceOsdItem(IdType id, GdiItem item) = 0;
    virtual void PlaceOsdItem(IdType id, GdiItem item, GdiItemAnimation animation) = 0;
    virtual void HideOsdItem(IdType id) = 0;
    virtual void DiscardOsdItem(IdType id) = 0;
};

template <typename IdType, IdType RootId>
class OsdRenderCallbackCompositor : public CUnknown, public OsdCompositor<IdType, RootId>, public IOsdRenderCallback
{
public:
    OsdRenderCallbackCompositor(std::shared_ptr<const PlaybackState> ps,
                                IInternalOsdService* pInternalOsdSevice) : OsdRenderCallbackCompositor() {
        ENSURE(ps && pInternalOsdSevice);
        m_internalOsdSevice = pInternalOsdSevice;
        m_ps = ps;
    }

    OsdRenderCallbackCompositor(std::shared_ptr<const PlaybackState> ps,
                                IMadVROsdServices* pMadvrOsdService,
                                IMadVRSettings* pMadvrSettings) : OsdRenderCallbackCompositor() {
        ENSURE(ps && pMadvrOsdService && pMadvrSettings);
        m_ps = ps;
        m_madvrOsdService = pMadvrOsdService;
        m_madvrSettings = pMadvrSettings;
    }

    DECLARE_IUNKNOWN

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override {
        CheckPointer(ppv, E_INVALIDARG);
        return QI(IOsdRenderCallback) __super::NonDelegatingQueryInterface(riid, ppv);
    }

    STDMETHODIMP SetDevice(IDirect3DDevice9* dev) override {
        std::lock_guard<std::mutex> lock(m_itemsMutex);
        m_gdiCache.clear();
        m_dev = dev;
        return S_OK;
    }

    STDMETHODIMP ClearBackground(LPCSTR name, REFERENCE_TIME frameStart,
                                 RECT* fullOutputRect, RECT* activeVideoRect) override {
        UNREFERENCED_PARAMETER(name);
        UNREFERENCED_PARAMETER(frameStart);
        UNREFERENCED_PARAMETER(fullOutputRect);
        UNREFERENCED_PARAMETER(activeVideoRect);
        // not used
        return S_OK;
    }

    STDMETHODIMP RenderOsd(LPCSTR name, REFERENCE_TIME frameStart,
                           RECT* fullOutputRect, RECT* activeVideoRect) override {
        UNREFERENCED_PARAMETER(name);
        UNREFERENCED_PARAMETER(fullOutputRect);
        UNREFERENCED_PARAMETER(activeVideoRect);

        std::lock_guard<std::mutex> lock(m_itemsMutex);

        if (!m_dev) {
            ASSERT(FALSE);
            return E_UNEXPECTED;
        }

        // animation callbacks
        {
            const PlaybackState::GraphState graphState = m_ps->GetGraphState();
            const auto info = (graphState.bAsync || graphState.state == State_Paused) ?
                              ItemAnimationInfo::Paused : ItemAnimationInfo::Playing;

            for (auto it = m_gdiItems.begin(); it != m_gdiItems.end();) {
                auto& gdiItem = it->second;
                if (gdiItem.animation && !gdiItem.animation(gdiItem.item, frameStart, info)) {
                    m_gdiItems.erase(it++);
                } else {
                    ASSERT(gdiItem.item.bitmap);
                    ++it;
                }
            }
        }

        // parentless clean-up
        {
            const auto info = ItemAnimationInfo::Parentless;

            for (auto it = m_gdiItems.begin(); it != m_gdiItems.end();) {
                auto& gdiItem = it->second;
                if (gdiItem.item.parent != RootId && m_gdiItems.find(gdiItem.item.parent) == m_gdiItems.end() &&
                        (!gdiItem.animation || !gdiItem.animation(gdiItem.item, frameStart, info))) {
                    m_gdiItems.erase(it++);
                } else {
                    ASSERT(gdiItem.item.bitmap);
                    ++it;
                }
            }
        }

        // cache clean-up
        {
            decltype(m_gdiCache) oldGdiCache;
            std::swap(m_gdiCache, oldGdiCache);
            for (auto& gdiItemPair : m_gdiItems) {
                const auto& gdiBitmap = gdiItemPair.second.item.bitmap;
                m_gdiCache[gdiBitmap] = oldGdiCache[gdiBitmap];
            }
        }

        // ram -> gpu
        for (auto& gdiCachePair : m_gdiCache) {
            ASSERT(gdiCachePair.first);
            if (!gdiCachePair.second) {
                VERIFY(TextureFromBitmap(m_dev, &gdiCachePair.second, gdiCachePair.first->hBitmap));
            }
        }

        // produce layer tree (doesn't handle parent loops)
        tree<LayerTreeItem> layerTree;
        {
            bool bEmpty = true;

            LayerTreeItem rootItem(RootId, nullptr);
            VERIFY(SUCCEEDED(m_dev->GetRenderTarget(0, &rootItem.surf)));

            std::queue<decltype(layerTree)::iterator> queue;
            queue.push(layerTree.set_head(rootItem));
            while (!queue.empty()) {
                auto it = queue.front();
                for (auto& gdiItemPair : m_gdiItems) {
                    if (gdiItemPair.second.item.parent == it->id && !gdiItemPair.second.item.bHidden) {
                        queue.push(layerTree.append_child(it, LayerTreeItem(gdiItemPair.first,
                                                          &gdiItemPair.second.item)));
                        bEmpty = false;
                    }
                }
                queue.pop();
            }

            // short circuit
            if (bEmpty) {
                return S_OK;
            }
        }

        // because this is a callback, we want to return the device in the same state as we receive it
        CComPtr<IDirect3DStateBlock9> oldState;
        VERIFY(SUCCEEDED(m_dev->CreateStateBlock(D3DSBT_ALL, &oldState)));

        // output levels
        BYTE black, white;
        GetOutputLevels(black, white);
        bool bDrawingOnRoot = false;

        // colors
        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1)));
        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE)));

        // alpha
        VERIFY(SUCCEEDED(m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)));
        VERIFY(SUCCEEDED(m_dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD)));
        VERIFY(SUCCEEDED(m_dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA)));
        VERIFY(SUCCEEDED(m_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA)));
        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE)));
        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE)));
        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CONSTANT)));

        // disable 1-7 stages
        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE)));
        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE)));

        // clear possible leftover shaders
        VERIFY(SUCCEEDED(m_dev->SetPixelShader(nullptr)));
        VERIFY(SUCCEEDED(m_dev->SetVertexShader(nullptr)));

        // operate in screen coordinates
        VERIFY(SUCCEEDED(m_dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1)));

        // TODO: adjust more states?

        auto drawTexture = [this](IDirect3DTexture9 * pTex, const CRect & rect) {
            if (pTex && SUCCEEDED(m_dev->SetTexture(0, pTex))) {
                struct {
                    float x, y, z, rhw, tu, tv;
                } vertices[] = {
                    { (float)rect.left - 0.5f, (float)rect.top - 0.5f, 0.5f, 1.0f, 0.0f, 0.0f },
                    { (float)rect.right - 0.5f, (float)rect.top - 0.5f, 0.5f, 1.0f, 1.0f, 0.0f },
                    { (float)rect.left - 0.5f, (float)rect.bottom - 0.5f, 0.5f, 1.0f, 0.0f, 1.0f },
                    { (float)rect.right - 0.5f, (float)rect.bottom - 0.5f, 0.5f, 1.0f, 1.0f, 1.0f },
                };
                VERIFY(SUCCEEDED(m_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0]))));
            } else {
                ASSERT(FALSE);
            }
        };

        // draw layers
        for (auto it = layerTree.begin_post(); it != layerTree.end_post() && it->id != RootId; ++it) {
            auto parent = layerTree.parent(it);

            if (!parent->surf) {
                ASSERT(parent->id != RootId);
                VERIFY(SUCCEEDED(m_dev->CreateTexture(parent->pItem->location.rect.Width(),
                                                      parent->pItem->location.rect.Height(),
                                                      1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                                      &parent->tex, nullptr)));
                VERIFY(SUCCEEDED(parent->tex->GetSurfaceLevel(0, &parent->surf)));
                CComPtr<IDirect3DSurface9> itemSurface;
                VERIFY(SUCCEEDED(m_gdiCache[parent->pItem->bitmap]->GetSurfaceLevel(0, &itemSurface)));
                VERIFY(SUCCEEDED(m_dev->StretchRect(itemSurface, nullptr, parent->surf, nullptr, D3DTEXF_NONE)));
            }

            if (parent->surf && SUCCEEDED(m_dev->SetRenderTarget(0, parent->surf))) {
                ASSERT(it->pItem->fAlpha >= 0.0f && it->pItem->fAlpha <= 1.0f);
                ASSERT(it->pItem->bitmap->fAlpha >= 0.0f && it->pItem->bitmap->fAlpha <= 1.0f);
                const BYTE alpha = (BYTE)(255 * it->pItem->fAlpha * it->pItem->bitmap->fAlpha + 0.5f);
                VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_CONSTANT, D3DCOLOR_ARGB(alpha, white, white, white))));

                if (!bDrawingOnRoot && parent->id == RootId) {
                    bDrawingOnRoot = true;
                    if (black != 0 || white != 255) {
                        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_LERP)));
                        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_COLORARG0, D3DTA_TEXTURE)));
                        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_CONSTANT)));
                        VERIFY(SUCCEEDED(m_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR)));
                        VERIFY(SUCCEEDED(m_dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(255, black, black, black))));
                    }
                }

                auto getParentRect = [&]() -> CRect {
                    if (parent->id == RootId) {
                        return fullOutputRect;
                    } else {
                        return CRect(CPoint(0, 0), parent->pItem->bitmap->bitmapSize);
                    }
                };
                CRect rect = it->pItem->location.rect;

                switch (it->pItem->location.anchor) {
                    case Anchor::TopLeft:
                        // do nothing
                        break;
                    case Anchor::TopRight:
                        rect.OffsetRect(getParentRect().right, 0);
                        break;
                    case Anchor::BottomLeft:
                        rect.OffsetRect(0, getParentRect().bottom);
                        break;
                    case Anchor::BottomRight:
                        rect.OffsetRect(getParentRect().BottomRight());
                        break;
                    default:
                        ASSERT(FALSE);
                }

                if (it->surf) {
                    drawTexture(it->tex, rect);
                } else {
                    drawTexture(m_gdiCache[it->pItem->bitmap], rect);
                }
            } else {
                ASSERT(FALSE);
            }
        }

        // restore device state
        VERIFY(SUCCEEDED(oldState->Apply()));

        return S_OK;
    }

    virtual void BeginQueue() override {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ASSERT(m_queueDepth >= 0);
        if (!m_queueDepth++) {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            m_gdiQueue = m_gdiItems;
        }
    }

    virtual void EndQueue() override {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ASSERT(m_queueDepth > 0);
        if (!--m_queueDepth) {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            std::swap(m_gdiItems, m_gdiQueue);
            for (auto& oldItemPair : m_gdiQueue) {
                if (oldItemPair.second.animation && m_gdiItems.find(oldItemPair.first) == m_gdiItems.end()) {
                    VERIFY(!oldItemPair.second.animation(oldItemPair.second.item, 0, ItemAnimationInfo::Discard));
                }
            }
            m_gdiQueue.clear();
        }
        if (m_ps->GetGraphState().state == State_Paused) {
            if (m_madvrOsdService) {
                m_madvrOsdService->OsdRedrawFrame();
            } else {
                // TODO: redraw
            }
        }
    }

    virtual CRect GetLayoutRect() override {
        CRect ret = m_ps->GetVrWindowData().windowRect;
        ret.MoveToXY(0, 0);
        return ret;
    }

    virtual void PlaceOsdItem(IdType id, GdiItem item) override {
        PlaceOsdItem(id, item, {});
    }

    virtual void PlaceOsdItem(IdType id, GdiItem item, GdiItemAnimation animation) override {
        if (!item.bitmap && !animation) {
            DiscardOsdItem(id);
            return;
        }
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_queueDepth > 0) {
            m_gdiQueue[id] = { item, animation };
        } else {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            m_gdiItems[id] = { item, animation };
        }
    }

    virtual void HideOsdItem(IdType id) override {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_queueDepth > 0) {
            auto it = m_gdiQueue.find(id);
            if (it != m_gdiQueue.end()) {
                it->second.item.bHidden = true;
            }
        } else {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            auto it = m_gdiItems.find(id);
            if (it != m_gdiItems.end()) {
                it->second.item.bHidden = true;
            }
        }
    }

    virtual void DiscardOsdItem(IdType id) override {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_queueDepth > 0) {
            m_gdiQueue.erase(id);
        } else {
            std::lock_guard<std::mutex> lock(m_itemsMutex);
            auto it = m_gdiItems.find(id);
            if (it != m_gdiItems.end()) {
                if (it->second.animation) {
                    VERIFY(!it->second.animation(it->second.item, 0, ItemAnimationInfo::Discard));
                }
                m_gdiItems.erase(it);
            }
        }
    }

protected:
    struct GdiAnimatedItem {
        GdiItem item;
        GdiItemAnimation animation;
    };

    struct LayerTreeItem {
        IdType id = RootId;
        GdiItem* pItem = nullptr;
        CComPtr<IDirect3DSurface9> surf;
        CComPtr<IDirect3DTexture9> tex;

        LayerTreeItem() = default;
        LayerTreeItem(IdType id, GdiItem* pItem) : id(id), pItem(pItem) {}
    };

    OsdRenderCallbackCompositor() : CUnknown(typeid(OsdRenderCallbackCompositor).name(), nullptr) {}

    void GetOutputLevels(BYTE& black, BYTE& white) const {
        black = 0;
        white = 255;
        if (m_internalOsdSevice) {
            VERIFY(SUCCEEDED(m_internalOsdSevice->GetOutputLevels(black, white)));
        } else if (m_madvrSettings) {
            int val;
            if (m_madvrSettings->SettingsGetInteger(L"black", &val)) {
                black = val;
            } else {
                ASSERT(FALSE);
            }
            if (m_madvrSettings->SettingsGetInteger(L"white", &val)) {
                white = val;
            } else {
                ASSERT(FALSE);
            }
        } else {
            ASSERT(FALSE);
        }
    }

    std::shared_ptr<const PlaybackState> m_ps;
    std::map<IdType, GdiAnimatedItem> m_gdiItems, m_gdiQueue;
    std::map<std::shared_ptr<const OsdGdiBitmap>, CComPtr<IDirect3DTexture9>> m_gdiCache;
    int m_queueDepth = 0;
    std::mutex m_queueMutex, m_itemsMutex;
    CComPtr<IDirect3DDevice9> m_dev;
    CComPtr<IInternalOsdService> m_internalOsdSevice;
    CComPtr<IMadVROsdServices> m_madvrOsdService;
    CComPtr<IMadVRSettings> m_madvrSettings;
};

template <typename IdType, IdType RootId>
class OsdAnimationCoordinator
{
public:
    typedef typename OsdCompositor<IdType, RootId>::GdiItem UsedGdiItem;
    typedef std::map<REFERENCE_TIME, UsedGdiItem> UsedGdiProgressItems;
    typedef typename OsdCompositor<IdType, RootId>::ItemAnimationInfo UsedItemAnimationInfo;
    typedef typename OsdCompositor<IdType, RootId>::GdiItemAnimation UsedGdiItemAnimation;

    enum class AnimationPhase
    {
        None,
        BeginFadeIn,
        FadeIn,
        Normal,
        BeginFadeOutHide,
        BeginFadeOutDiscard,
        FadeOutHide,
        FadeOutDiscard,
    };

    OsdAnimationCoordinator() : m_core(std::make_shared<AnimationCore>()) {}

    UsedGdiItemAnimation ProgressAnimation(IdType id, UsedGdiProgressItems&& progItems) {
        return UsedGdiItemAnimation(
                   AnimationFunctor(id, m_core, AnimationPhase::Normal, std::move(progItems)));
    }

    UsedGdiItemAnimation NormalAnimation(IdType id) {
        return ProgressAnimation(id, {});
    }

    UsedGdiItemAnimation FadeInProgressAnimation(IdType id, UsedGdiProgressItems&& progItems) {
        return UsedGdiItemAnimation(
                   AnimationFunctor(id, m_core, AnimationPhase::BeginFadeIn, std::move(progItems)));
    }

    UsedGdiItemAnimation FadeInNormalAnimation(IdType id) {
        return FadeInProgressAnimation(id, {});
    }

    UsedGdiItemAnimation FadeOutHideProgressAnimation(IdType id, UsedGdiProgressItems&& progItems) {
        return UsedGdiItemAnimation(
                   AnimationFunctor(id, m_core, AnimationPhase::BeginFadeOutHide, std::move(progItems)));
    }

    UsedGdiItemAnimation FadeOutHideNormalAnimation(IdType id) {
        return FadeOutHideProgressAnimation(id, {});
    }

    UsedGdiItemAnimation FadeOutDiscardProgressAnimation(IdType id, UsedGdiProgressItems&& progItems) {
        return UsedGdiItemAnimation(
                   AnimationFunctor(id, m_core, AnimationPhase::BeginFadeOutDiscard, std::move(progItems)));
    }

    UsedGdiItemAnimation FadeOutDiscardNormalAnimation(IdType id) {
        return FadeOutDiscardProgressAnimation(id, {});
    }

    AnimationPhase GetAnimationPhase(IdType id) const {
        std::lock_guard<std::mutex> lock(m_core->itemsMutex);
        return m_core->items[id].phase;
    }

protected:
    struct AnimationCore {
        struct Item {
            AnimationPhase phase = AnimationPhase::None;
            float fLastAlpha = 0.0f;
            REFERENCE_TIME rtLastFrame = 0;
        };
        std::map<IdType, Item> items;
        std::mutex itemsMutex;
    };

    class AnimationFunctor
    {
    public:
        AnimationFunctor(IdType id, std::shared_ptr<AnimationCore> core,
                         AnimationPhase phase, UsedGdiProgressItems&& progItems)
            : m_id(id), m_core(core), m_animation(phase, std::move(progItems)) {}

        bool operator()(UsedGdiItem& bitmap, REFERENCE_TIME rtFrame, UsedItemAnimationInfo info) {
            bool ret = true;

            if (!m_animation.progItems.empty()) {
                auto hit = m_animation.progItems.begin();
                for (auto it = hit; it != m_animation.progItems.end(); ++it) {
                    if (it->first > rtFrame) {
                        break;
                    } else {
                        hit = it;
                    }
                }
                bitmap = hit->second;
            }

            std::lock_guard<std::mutex> lock(m_core->itemsMutex);

            auto& coreItem = m_core->items[m_id];

            if (info == UsedItemAnimationInfo::Playing) {
                switch (m_animation.phase) {
                    case AnimationPhase::BeginFadeIn: {
                        if (coreItem.phase == AnimationPhase::Normal) {
                            m_animation.phase = AnimationPhase::Normal;
                            break;
                        } else if (coreItem.phase == AnimationPhase::None) {
                            coreItem.rtLastFrame = rtFrame;
                        }
                        m_animation.phase = AnimationPhase::FadeIn;
                    }
                    // no break
                    case AnimationPhase::FadeIn: {
                        bitmap.fAlpha = FadeIn(coreItem.fLastAlpha, coreItem.rtLastFrame, 1.0f, rtFrame);
                        ASSERT(bitmap.fAlpha >= 0.0f && bitmap.fAlpha <= 1.0f);
                        if (bitmap.fAlpha == 1.0f) {
                            m_animation.phase = AnimationPhase::Normal;
                        }
                        break;
                    }
                    case AnimationPhase::BeginFadeOutHide: {
                        if (coreItem.phase == AnimationPhase::None) {
                            bitmap.fAlpha = 0.0f;
                            bitmap.bHidden = true;
                            break;
                        }
                        m_animation.phase = AnimationPhase::FadeOutHide;
                    }
                    // no break
                    case AnimationPhase::FadeOutHide: {
                        bitmap.fAlpha = FadeOut(coreItem.fLastAlpha, coreItem.rtLastFrame, 1.0f, rtFrame);
                        ASSERT(bitmap.fAlpha >= 0.0f && bitmap.fAlpha <= 1.0f);
                        if (bitmap.fAlpha == 0.0f) {
                            bitmap.bHidden = true;
                            m_animation.phase = AnimationPhase::None;
                        }
                        break;
                    }
                    case AnimationPhase::BeginFadeOutDiscard: {
                        if (coreItem.phase == AnimationPhase::None) {
                            ret = false;
                            break;
                        }
                        m_animation.phase = AnimationPhase::FadeOutDiscard;
                    }
                    // no break
                    case AnimationPhase::FadeOutDiscard: {
                        bitmap.fAlpha = FadeOut(coreItem.fLastAlpha, coreItem.rtLastFrame, 1.0f, rtFrame);
                        ASSERT(bitmap.fAlpha >= 0.0f && bitmap.fAlpha <= 1.0f);
                        if (bitmap.fAlpha == 0.0f) {
                            ret = false;
                        }
                        break;
                    }
                }
            } else if (info == UsedItemAnimationInfo::Paused) {
                switch (m_animation.phase) {
                    case AnimationPhase::BeginFadeIn:
                        m_animation.phase = AnimationPhase::Normal;
                        break;
                    case AnimationPhase::BeginFadeOutHide:
                    case AnimationPhase::FadeOutHide:
                        bitmap.fAlpha = 0.0f;
                        bitmap.bHidden = true;
                        break;
                    case AnimationPhase::BeginFadeOutDiscard:
                    case AnimationPhase::FadeOutDiscard:
                        bitmap.fAlpha = 0.0f;
                        ret = false;
                        break;
                }
            } else {
                ret = false;
            }

            if (ret) {
                coreItem.phase = m_animation.phase;
                coreItem.fLastAlpha = bitmap.fAlpha;
                coreItem.rtLastFrame = rtFrame;
            } else {
                m_core->items.erase(m_id);
            }

            return ret;
        }

    protected:
        struct Animation {
            AnimationPhase phase;
            UsedGdiProgressItems progItems;

            Animation() = delete;
            Animation(AnimationPhase phase, UsedGdiProgressItems&& progItems)
                : phase(phase), progItems(std::move(progItems)) {}
        };

        inline float FadeIn(float fLastAlpha, REFERENCE_TIME rtLastFrame,
                            float fNormalAlpha, REFERENCE_TIME rtFrame) {
            const REFERENCE_TIME rtDuration = FADE_IN_TIME_MS * ms;
            float ret = fNormalAlpha;
            if (rtFrame >= rtLastFrame) {
                ret = std::min(ret, fLastAlpha + fNormalAlpha * (rtFrame - rtLastFrame) / rtDuration);
            }
            return ret;
        }

        inline float FadeOut(float fLastAlpha, REFERENCE_TIME rtLastFrame,
                             float fNormalAlpha, REFERENCE_TIME rtFrame) {
            const REFERENCE_TIME rtDuration = FADE_OUT_TIME_MS * ms;
            float ret = 0.0f;
            if (rtFrame >= rtLastFrame) {
                ret = std::max(ret, fLastAlpha - fNormalAlpha * (rtFrame - rtLastFrame) / rtDuration);
            }
            return ret;
        }

        const IdType m_id;
        const std::shared_ptr<AnimationCore> m_core;
        Animation m_animation;
    };

    const std::shared_ptr<AnimationCore> m_core;
};

template <typename IdType, IdType RootId>
class OsdGdiBackend
{
public:
    typedef OsdCompositor<IdType, RootId> UsedCompositor;
    typedef OsdAnimationCoordinator<IdType, RootId> UsedAnimator;

    OsdGdiBackend(CMainFrame* pMainFrame, IInternalOsdService* pInternalOsdSevice) {
        ENSURE(pMainFrame && pInternalOsdSevice);
        m_internalOsdSevice = pInternalOsdSevice;
        auto pCompositor = new OsdRenderCallbackCompositor<IdType, RootId>(
            pMainFrame->GetPlaybackState(), pInternalOsdSevice);
        pCompositor->AddRef();
        m_pCompositor = pCompositor;
        VERIFY(SUCCEEDED(m_internalOsdSevice->SetRenderCallback(m_renderCallbackName, pCompositor)));
    }

    OsdGdiBackend(CMainFrame* pMainFrame, IMadVROsdServices* pMadvrOsdService, IMadVRSettings* pMadvrSettings) {
        ENSURE(pMainFrame && pMadvrOsdService && pMadvrSettings);
        m_madvrOsdService = pMadvrOsdService;
        auto pCompositor = new OsdRenderCallbackCompositor<IdType, RootId>(
            pMainFrame->GetPlaybackState(), m_madvrOsdService, pMadvrSettings);
        pCompositor->AddRef();
        m_pCompositor = pCompositor;
        VERIFY(SUCCEEDED(m_madvrOsdService->OsdSetRenderCallback(m_renderCallbackName, pCompositor)));
    }

    ~OsdGdiBackend() {
        if (m_internalOsdSevice) {
            VERIFY(SUCCEEDED(m_internalOsdSevice->SetRenderCallback(m_renderCallbackName, nullptr)));
        } else if (m_madvrOsdService) {
            VERIFY(SUCCEEDED(m_madvrOsdService->OsdSetRenderCallback(m_renderCallbackName, nullptr)));
        }

        if (auto pUnknown = dynamic_cast<IUnknown*>(m_pCompositor)) {
            pUnknown->Release();
        } else {
            delete m_pCompositor;
        }
    }

    UsedCompositor& GetCompositor() {
        return *m_pCompositor;
    }

    UsedAnimator& GetAnimator() {
        return m_animator;
    }

    OsdGdiPainter& GetPainter() {
        return m_painter;
    }

protected:
    const CStringA m_renderCallbackName = "MPC-HC.OsdCallback";

    CComPtr<IInternalOsdService> m_internalOsdSevice;
    CComPtr<IMadVROsdServices> m_madvrOsdService;

    UsedCompositor* m_pCompositor;
    UsedAnimator m_animator;
    OsdGdiPainter m_painter;
};

template <typename IdType, IdType RootId, IdType BackId, IdType ThumbId>
class OsdGdiSeekbar : public CDraggableSeekbar, public OsdControlsProvider, public OsdSeekbarProvider
{
public:
    typedef OsdGdiBackend<IdType, RootId> UsedBackend;

    OsdGdiSeekbar(std::shared_ptr<UsedBackend> backend) : OsdGdiSeekbar() {
        ENSURE(backend);
        m_backend = backend;
    }

    OsdGdiSeekbar(std::shared_ptr<UsedBackend> backend, IMadVRSettings* pMadvrSettingsService) : OsdGdiSeekbar() {
        ENSURE(backend && pMadvrSettingsService);
        m_backend = backend;
        m_pMadvrSettingsService = pMadvrSettingsService;
    }

    ~OsdGdiSeekbar() {
        if (m_bInDelayedHide) {
            m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_SEEKBAR);
        }
        auto& compositor = m_backend->GetCompositor();
        compositor.BeginQueue();
        compositor.DiscardOsdItem(ThumbId);
        compositor.DiscardOsdItem(BackId);
        compositor.EndQueue();
    }

    virtual bool PointOnOsdControl(const CPoint& videoPoint) override {
        return SeekbarEnabled() && GetSeekbarRect().PtInRect(videoPoint);;
    }

    virtual bool ReceiveInputEvent(OsdInputEvent event, const CPoint& videoPoint) override {
        if (!SeekbarEnabled()) {
            ASSERT(m_cursor == CMouse::Cursor::NONE);
            ASSERT(!m_bVisible);
            return false;
        }

        bool ret = false;

        switch (event) {
            case OsdInputEvent::MOUSE_DOWN:
                if (PointOnOsdControl(videoPoint)) {
                    ret = true;
                    StartThumbDrag();
                    const REFERENCE_TIME rtPos = PosFromPoint(videoPoint);
                    m_pThumbDragPos->Set({ true, rtPos });
                    DragThumb(videoPoint, rtPos);
                }
                break;
            case OsdInputEvent::MOUSE_UP:
                if (DraggingThumb()) {
                    ret = true;
                    StopThumbDrag(false);
                    m_pThumbDragPos->Set({ false, 0 });
                    if (!PointOnOsdControl(videoPoint)) {
                        m_cursor = CMouse::Cursor::NONE;
                    }
                }
                break;
            case OsdInputEvent::MOUSE_MOVE:
                if (DraggingThumb()) {
                    ret = true;
                    const REFERENCE_TIME rtPos = PosFromPoint(videoPoint);
                    m_pThumbDragPos->Set({ true, rtPos });
                    DragThumb(videoPoint, rtPos);
                } else if (PointOnOsdControl(videoPoint)) {
                    ret = true;
                    m_cursor = CMouse::Cursor::HAND;
                    DisplaySeekbar();
                } else {
                    m_cursor = CMouse::Cursor::NONE;
                    HideSeekbarAfterDelay();
                }
                break;
            case OsdInputEvent::MOUSE_LEAVE:
                ret = true;
                StopThumbDrag();
                m_pThumbDragPos->Set({ false, 0 });
                m_cursor = CMouse::Cursor::NONE;
                HideSeekbarAfterDelay();
                break;
            default:
                ASSERT(FALSE);
        }

        return ret;
    }

    virtual CMouse::Cursor GetOsdMouseCursor() const override {
        return m_cursor;
    }

    bool SeekbarEnabled() const override {
        bool ret = m_pMainFrame->IsD3DFullScreenMode();
        if (!ret && m_pMadvrSettingsService && m_pMainFrame->m_fFullScreen) {
            BOOL bOptExcl = FALSE;
            VERIFY(m_pMadvrSettingsService->SettingsGetBoolean(L"enableExclusive", &bOptExcl));
            ret = !!bOptExcl;
        }
        return ret;
    }

    void Relayout() {
        auto& compositor = m_backend->GetCompositor();
        auto& animator = m_backend->GetAnimator();
        auto& painter = m_backend->GetPainter();

        const CRect layoutRect = compositor.GetLayoutRect();

        if (layoutRect.IsRectEmpty()) {
            return;
        }

        bool bEnabled = SeekbarEnabled();

        if (bEnabled && (!m_itemBack.bitmap || m_itemBack.bitmap->bitmapSize.cx != layoutRect.Width())) {
            PlaybackState::Pos pos = m_pMainFrame->GetPlaybackState()->GetPos();
            OsdGdiBitmap bitmapBack, bitmapThumb;
            if (painter.PaintSeekbar(pos.rtDur, pos.pChapterBag, layoutRect.Size(),
                                     bitmapBack, bitmapThumb, m_funcRectFromPos, m_funcPosFromPoint)) {
                m_itemBack.bitmap = std::make_shared<OsdGdiBitmap>(std::move(bitmapBack));
                m_itemThumb.bitmap = std::make_shared<OsdGdiBitmap>(std::move(bitmapThumb));
            } else {
                ASSERT(FALSE);
                bEnabled = false;
            }
        }

        compositor.BeginQueue();
        if (bEnabled) {
            m_itemBack.location.anchor = UsedBackend::UsedCompositor::Anchor::BottomLeft;
            m_itemBack.location.rect = { CPoint(0, -m_itemBack.bitmap->bitmapSize.cy), m_itemBack.bitmap->bitmapSize };
            m_itemThumb.parent = BackId;

            compositor.PlaceOsdItem(ThumbId, m_itemThumb, GetThumbAnimation());
            if (AnimationEnabled()) {
                auto anim = m_bVisible ?
                            animator.FadeInNormalAnimation(BackId) :
                            animator.FadeOutHideNormalAnimation(BackId);
                compositor.PlaceOsdItem(BackId, m_itemBack, std::move(anim));
            } else if (m_bVisible) {
                compositor.PlaceOsdItem(BackId, m_itemBack, animator.NormalAnimation(BackId));
            } else {
                compositor.DiscardOsdItem(ThumbId);
                compositor.HideOsdItem(BackId);
            }
        } else {
            compositor.DiscardOsdItem(ThumbId);
            compositor.DiscardOsdItem(BackId);
        }
        compositor.EndQueue();
    }

    void Invalidate() {
        m_itemBack.bitmap = nullptr;
        m_itemThumb.bitmap = nullptr;
        Relayout();
    }

protected:
    class ThumbDragPos
    {
    public:
        typedef std::pair<bool, REFERENCE_TIME> ValueType;

        ThumbDragPos() : m_value(std::make_pair(false, 0)) {}

        ValueType Get() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_value;
        }

        void Set(ValueType value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_value = value;
        }

    protected:
        mutable std::mutex m_mutex;
        ValueType m_value;
    };

    OsdGdiSeekbar() : CDraggableSeekbar(AfxGetMainFrame()), m_pThumbDragPos(std::make_shared<ThumbDragPos>()) {
        ENSURE(m_pMainFrame);

        GetEventd().Connect(m_eventc, {
            MpcEvent::SWITCHING_FROM_FULLSCREEN,
            MpcEvent::SWITCHING_FROM_FULLSCREEN_D3D,
            MpcEvent::MEDIA_CHAPTERBAG_CHANGED,
            MpcEvent::MEDIA_DURATION_CHANGED,
            MpcEvent::VR_WINDOW_DATA_CHANGED,
        }, std::bind(&OsdGdiSeekbar::EventCallback, this, std::placeholders::_1));
    }

    void EventCallback(MpcEvent ev) {
        switch (ev) {
            case MpcEvent::SWITCHING_FROM_FULLSCREEN:
            case MpcEvent::SWITCHING_FROM_FULLSCREEN_D3D:
                m_cursor = CMouse::Cursor::NONE;
                HideSeekbar();
                break;
            case MpcEvent::MEDIA_CHAPTERBAG_CHANGED:
            case MpcEvent::MEDIA_DURATION_CHANGED:
                m_itemBack.bitmap = nullptr;
                m_itemThumb.bitmap = nullptr;
            // no break
            case MpcEvent::VR_WINDOW_DATA_CHANGED:
                Relayout();
                break;
            default:
                ASSERT(FALSE);
        }
    }

    typename UsedBackend::UsedAnimator::UsedGdiItemAnimation GetThumbAnimation() const {
        const auto funcRectFromPos = m_funcRectFromPos;
        const auto pThumbDragPos = m_pThumbDragPos;
        auto anim = [funcRectFromPos, pThumbDragPos](UsedBackend::UsedCompositor::GdiItem & item, REFERENCE_TIME rtFrame, UsedBackend::UsedCompositor::ItemAnimationInfo info) {
            ThumbDragPos::ValueType thumbDragPos = pThumbDragPos->Get();
            item.location.rect = funcRectFromPos(thumbDragPos.first ? thumbDragPos.second : rtFrame);
            return (info == UsedBackend::UsedCompositor::ItemAnimationInfo::Playing) ||
                   (info == UsedBackend::UsedCompositor::ItemAnimationInfo::Paused);
        };
        return anim;
    }

    virtual void DraggedThumb(const CPoint& point, REFERENCE_TIME rtPos) override {
        m_pMainFrame->SeekTo(rtPos);
    }

    REFERENCE_TIME PosFromPoint(const CPoint& point) const {
        REFERENCE_TIME rtPos = m_funcPosFromPoint(point);
        if (AfxGetAppSettings().bFastSeek ^ (GetKeyState(VK_SHIFT) < 0)) {
            rtPos = m_pMainFrame->GetClosestKeyFrame(rtPos);
        }
        return rtPos;
    }

    void DisplaySeekbar() {
        if (m_bInDelayedHide) {
            m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_SEEKBAR);
            m_bInDelayedHide = false;
            ASSERT(m_bVisible);
            Relayout();
        } else if (!m_bVisible) {
            m_bVisible = true;
            Relayout();
        }
    }

    void HideSeekbarAfterDelay() {
        if (m_bVisible && !m_bInDelayedHide) {
            m_pMainFrame->m_timerOneTime.Subscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_SEEKBAR,
                                                   [this] { HideSeekbar(); }, OSD_SEEKBAR_HIDE_DELAY_MS);
            m_bInDelayedHide = true;
        }
    }

    void HideSeekbar() {
        m_bInDelayedHide = false;
        if (m_bVisible) {
            m_bVisible = false;
            Relayout();
        }
    }

    CRect GetSeekbarRect() const {
        CRect rect = m_backend->GetCompositor().GetLayoutRect();
        rect.top = rect.bottom - OSD_SEEKBAR_HEIGHT;
        return rect;
    }

    bool AnimationEnabled() const {
        return AfxGetAppSettings().osd.bAnimation;
    }

    CMainFrame* const m_pMainFrame = AfxGetMainFrame();
    std::shared_ptr<UsedBackend> m_backend;
    CComPtr<IMadVRSettings> m_pMadvrSettingsService;

    typename UsedBackend::UsedCompositor::GdiItem m_itemBack, m_itemThumb;
    std::function<CRect(REFERENCE_TIME)> m_funcRectFromPos;
    std::function<REFERENCE_TIME(CPoint)> m_funcPosFromPoint;
    std::shared_ptr<ThumbDragPos> m_pThumbDragPos;

    CMouse::Cursor m_cursor = CMouse::Cursor::NONE;
    bool m_bVisible = false;
    bool m_bInDelayedHide = false;

    EventClient m_eventc;
};

class OsdMainGdiProvider : public OsdMessageProvider, public OsdSeekbarProvider, public OsdControlsProvider
{
public:
    OsdMainGdiProvider(IInternalOsdService* pInternalOsdSevice) : OsdMainGdiProvider() {
        ENSURE(pInternalOsdSevice);

        m_backend = std::make_shared<UsedBackend>(m_pMainFrame, pInternalOsdSevice);

        VERIFY(m_backend->GetPainter().Initialize(m_pMainFrame->GetDpiHelper()));

        m_seekbar = std::make_unique<UsedSeekbar>(m_backend);
    }

    OsdMainGdiProvider(IMadVROsdServices* pMadvrOsdService,
                       IMadVRSeekbarControl* pMadvrSeekbarService,
                       IMadVRSettings* pMadvrSettingsService) : OsdMainGdiProvider() {
        ENSURE(pMadvrOsdService && pMadvrSeekbarService && pMadvrSettingsService);

        m_pMadvrOsdService = pMadvrOsdService;

        m_backend = std::make_shared<UsedBackend>(m_pMainFrame, pMadvrOsdService, pMadvrSettingsService);

        VERIFY(m_backend->GetPainter().Initialize(m_pMainFrame->GetDpiHelper()));

        m_seekbar = std::make_unique<UsedSeekbar>(m_backend, pMadvrSettingsService);

        m_pMadvrSeekbarService = pMadvrSeekbarService;
        VERIFY(SUCCEEDED(m_pMadvrSeekbarService->DisableSeekbar(TRUE)));

        m_pMadvrSettingsService = pMadvrSettingsService;
    }

    ~OsdMainGdiProvider() {
        m_pMainFrame->m_timerOneTime.Unsubscribe(TimerAtPos(OsdPos::TOPLEFT));
        m_pMainFrame->m_timerOneTime.Unsubscribe(TimerAtPos(OsdPos::TOPRIGHT));
        m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_MADVR_REDRAW);

        auto& compositor = m_backend->GetCompositor();
        compositor.BeginQueue();
        compositor.DiscardOsdItem(ItemId::TopLeftMessage);
        compositor.DiscardOsdItem(ItemId::TopRightMessage);
        m_seekbar = nullptr;
        compositor.EndQueue();

        if (m_pMadvrSeekbarService) {
            VERIFY(SUCCEEDED(m_pMadvrSeekbarService->DisableSeekbar(FALSE)));
        }
    }

    virtual void DisplayMessage(OsdPos pos, const CString& msg, int duration) override {
        if (!msg) {
            HideMessage(pos);
        } else {
            auto& item = MsgItemAtPos(pos);
            auto& sticky = StickyItemAtPos(pos);

            if (sticky && sticky->bFadingOut) {
                sticky = nullptr;
            }

            item = std::make_unique<MessageItem>(MsgIdAtPos(pos));
            item->msg = msg;

            Relayout();

            if (duration > 0) {
                m_pMainFrame->m_timerOneTime.Subscribe(
                    TimerAtPos(pos), std::bind(&OsdMainGdiProvider::HideMessage, this, pos), duration);
            } else {
                m_pMainFrame->m_timerOneTime.Unsubscribe(TimerAtPos(pos));
            }
        }
    }

    virtual void DisplayMessage(OsdPos pos, ProgressMessage msg, int duration) override {
        if (!msg) {
            HideMessage(pos);
        } else {
            auto& item = MsgItemAtPos(pos);
            auto& sticky = StickyItemAtPos(pos);

            if (sticky && sticky->bFadingOut) {
                sticky = nullptr;
            }

            item = std::make_unique<MessageItem>(MsgIdAtPos(pos));
            item->progMsg = msg;

            Relayout();

            if (duration > 0) {
                m_pMainFrame->m_timerOneTime.Subscribe(
                    TimerAtPos(pos), std::bind(&OsdMainGdiProvider::HideMessage, this, pos), duration);
            } else {
                m_pMainFrame->m_timerOneTime.Unsubscribe(TimerAtPos(pos));
            }
        }
    }

    virtual void HideMessage(OsdPos pos) override {
        m_pMainFrame->m_timerOneTime.Unsubscribe(TimerAtPos(pos));

        auto& item = MsgItemAtPos(pos);

        if (item) {
            auto& sticky = StickyItemAtPos(pos);

            if (sticky) {
                item = nullptr;
            } else {
                item->bFadingOut = true;
            }

            Relayout();
        }
    }

    virtual void DisplayStickyMessage(OsdPos pos, const CString& msg) override {
        if (!msg) {
            HideStickyMessage(pos);
        } else {
            m_pMainFrame->m_timerOneTime.Unsubscribe(TimerAtPos(pos));

            auto& sticky = StickyItemAtPos(pos);
            auto& item = MsgItemAtPos(pos);

            item = nullptr;
            sticky = std::make_unique<MessageItem>(MsgIdAtPos(pos));
            sticky->msg = msg;

            Relayout();
        }
    }

    virtual void DisplayStickyMessage(OsdPos pos, OsdMessageProvider::ProgressMessage msg) override {
        if (!msg) {
            HideStickyMessage(pos);
        } else {
            m_pMainFrame->m_timerOneTime.Unsubscribe(TimerAtPos(pos));

            auto& item = MsgItemAtPos(pos);
            auto& sticky = StickyItemAtPos(pos);

            item = nullptr;
            sticky = std::make_unique<MessageItem>(MsgIdAtPos(pos));
            sticky->progMsg = msg;

            Relayout();
        }
    }

    virtual void HideStickyMessage(OsdPos pos) override {
        auto& sticky = StickyItemAtPos(pos);

        if (sticky) {
            auto& item = MsgItemAtPos(pos);

            if (item) {
                sticky = nullptr;
            } else {
                sticky->bFadingOut = true;
            }

            Relayout();
        }
    }

    virtual bool SeekbarEnabled() const override {
        return m_seekbar && m_seekbar->SeekbarEnabled();
    }

    virtual bool PointOnOsdControl(const CPoint& videoPoint) override {
        return m_seekbar && m_seekbar->PointOnOsdControl(videoPoint);
    }

    virtual bool ReceiveInputEvent(OsdInputEvent event, const CPoint& videoPoint) override {
        return m_seekbar && m_seekbar->ReceiveInputEvent(event, videoPoint);
    }

    virtual CMouse::Cursor GetOsdMouseCursor() const override {
        CMouse::Cursor ret = CMouse::Cursor::NONE;
        if (m_seekbar) {
            ret = m_seekbar->GetOsdMouseCursor();
        }
        return ret;
    }

protected:
    enum class ItemId
    {
        Root,
        TopLeftMessage,
        TopRightMessage,
        SeekbarBack,
        SeekbarThumb,
    };

    typedef OsdGdiBackend<ItemId, ItemId::Root> UsedBackend;
    typedef OsdGdiSeekbar<ItemId, ItemId::Root, ItemId::SeekbarBack, ItemId::SeekbarThumb> UsedSeekbar;

    struct MessageGdiItem {
        UsedBackend::UsedCompositor::GdiItem compBitmap;
        bool bTrimmed = false;
        CSize fullSize;
        CSize lastMaxSize;
        CString msg;

        MessageGdiItem() = default;
        explicit MessageGdiItem(const CString& msg) : msg(msg) {}
    };

    struct MessageItem {
        const ItemId id;

        // static message
        CString msg;
        MessageGdiItem msgItem;

        // progress message
        ProgressMessage progMsg;
        std::map<REFERENCE_TIME, MessageGdiItem> progItems;

        bool bFadingOut = false;

        explicit MessageItem(ItemId id) : id(id) {}
    };

    OsdMainGdiProvider() {
        ENSURE(m_pMainFrame);

        GetEventd().Connect(m_eventc, {
            MpcEvent::MEDIA_CHAPTERBAG_CHANGED,
            MpcEvent::MEDIA_DURATION_CHANGED,
            MpcEvent::MEDIA_POSITION_CHANGED,
            MpcEvent::OSD_REINITIALIZE_PAINTER,
            MpcEvent::VR_WINDOW_DATA_CHANGED,
        }, std::bind(&OsdMainGdiProvider::EventCallback, this, std::placeholders::_1));
    }

    void EventCallback(MpcEvent ev) {
        switch (ev) {
            case MpcEvent::MEDIA_CHAPTERBAG_CHANGED:
            case MpcEvent::MEDIA_DURATION_CHANGED:
            case MpcEvent::MEDIA_POSITION_CHANGED: {
                auto& msgTopLeft = m_msgTopLeft ? m_msgTopLeft : m_stickyTopLeft;
                auto& msgTopRight = m_msgTopRight ? m_msgTopRight : m_stickyTopRight;
                if ((msgTopLeft && msgTopLeft->progMsg) || (msgTopRight && msgTopRight->progMsg)) {
                    Relayout();
                }
                break;
            }
            case MpcEvent::OSD_REINITIALIZE_PAINTER: {
                auto clearBitmaps = [this](MessageItem * pItem) {
                    if (pItem) {
                        pItem->msgItem.compBitmap.bitmap = nullptr;
                        pItem->progItems.clear();
                    }
                };
                clearBitmaps(m_msgTopLeft.get());
                clearBitmaps(m_msgTopRight.get());
                clearBitmaps(m_stickyTopLeft.get());
                clearBitmaps(m_stickyTopRight.get());
                VERIFY(m_backend->GetPainter().Initialize(m_pMainFrame->GetDpiHelper()));
                Relayout();
                if (m_seekbar) {
                    m_seekbar->Invalidate();
                }
                break;
            }
            case MpcEvent::VR_WINDOW_DATA_CHANGED:
                Relayout();
                break;
            default:
                ASSERT(FALSE);
        }
    }

    std::unique_ptr<MessageItem>& MsgItemAtPos(OsdPos pos) {
        return (pos == OsdPos::TOPLEFT) ? m_msgTopLeft : m_msgTopRight;
    }

    std::unique_ptr<MessageItem>& StickyItemAtPos(OsdPos pos) {
        return (pos == OsdPos::TOPLEFT) ? m_stickyTopLeft : m_stickyTopRight;
    }

    ItemId MsgIdAtPos(OsdPos pos) const {
        return (pos == OsdPos::TOPLEFT) ? ItemId::TopLeftMessage : ItemId::TopRightMessage;
    }

    CMainFrame::TimerOneTimeSubscriber TimerAtPos(OsdPos pos) const {
        return (pos == OsdPos::TOPLEFT) ?
               CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT :
               CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPRIGHT;
    }

    bool AnimationEnabled() const {
        return AfxGetAppSettings().osd.bAnimation;
    }

    void PaintMessage(const CSize& maxSize, MessageGdiItem& msgBitmap) {
        auto& painter = m_backend->GetPainter();

        OsdGdiBitmap bitmap;
        if (painter.PaintMessage(msgBitmap.msg, maxSize, bitmap, msgBitmap.bTrimmed)) {
            msgBitmap.compBitmap.bitmap = std::make_shared<OsdGdiBitmap>(std::move(bitmap));
        } else {
            msgBitmap.compBitmap.bitmap = nullptr;
        }
        msgBitmap.lastMaxSize = maxSize;
        msgBitmap.compBitmap.parent = ItemId::Root;
    }

    void Relayout() {
        ASSERT(m_pMainFrame->GetLoadState() == MLS::LOADED);
        ASSERT(!m_pMainFrame->PlayingAudioOnly());

        auto& compositor = m_backend->GetCompositor();
        auto& animator = m_backend->GetAnimator();
        auto& painter = m_backend->GetPainter();

        const CRect layoutRect = compositor.GetLayoutRect();
        const CSize margin = m_pMainFrame->GetDpiHelper()->ScaleXY(10, 10);

        auto cleanFadedOut = [&](std::unique_ptr<MessageItem>& item) {
            if (item && item->bFadingOut) {
                if (!AnimationEnabled()) {
                    compositor.DiscardOsdItem(item->id);
                    item = nullptr;
                } else if (animator.GetAnimationPhase(item->id) == UsedBackend::UsedAnimator::AnimationPhase::None) {
                    item = nullptr;
                }
            }
        };

        auto updateProgressItem = [&](MessageItem & item) {
            if (item.progMsg) {
                // produce progress messages
                ProgressMessageReturn progMsgs = item.progMsg(m_pMainFrame->GetPlaybackState()->GetPos(),
                                                 ms * PROGRESS_MESSAGE_BUFFER_MS);

                // add new progress items
                for (const auto& msgPair : progMsgs) {
                    auto it = item.progItems.find(msgPair.first);
                    if (it == item.progItems.end()) {
                        item.progItems.emplace(msgPair.first, MessageGdiItem(msgPair.second));
                    } else if (it->second.msg != msgPair.second) {
                        it->second = MessageGdiItem(msgPair.second);
                    }
                }

                // remove outdated progress items
                for (auto it = item.progItems.begin(); it != item.progItems.end();) {
                    if (progMsgs.find(it->first) == progMsgs.end()) {
                        item.progItems.erase(it++);
                    } else {
                        ++it;
                    }
                }
            }
        };

        auto calcMessageItemFullSize = [&](MessageItem & item) -> CSize {
            if (!item.progMsg) {
                if (item.msgItem.fullSize == CSize(0, 0)) {
                    item.msgItem.fullSize = painter.CalcMessageSize(item.msg);
                    ASSERT(item.msgItem.fullSize != CSize(0, 0));
                }
                return item.msgItem.fullSize;
            } else {
                CSize ret(0, 0);
                for (auto& progItemPair : item.progItems) {
                    if (progItemPair.second.fullSize == CSize(0, 0)) {
                        progItemPair.second.fullSize = painter.CalcMessageSize(item.msg);
                        ASSERT(progItemPair.second.fullSize != CSize(0, 0));
                    }
                    ret.cx = std::max(progItemPair.second.fullSize.cx, ret.cx);
                    ret.cy = std::max(progItemPair.second.fullSize.cy, ret.cy);
                }
                return ret;
            }
        };

        auto getMessageItemCurrentSize = [&](MessageItem & item) -> CSize {
            if (!item.progMsg) {
                return item.msgItem.compBitmap.location.rect.Size();
            } else {
                CSize ret(0, 0);
                for (auto& progItemPair : item.progItems) {
                    CSize size = progItemPair.second.compBitmap.location.rect.Size();
                    ret.cx = std::max(size.cx, ret.cx);
                    ret.cy = std::max(size.cy, ret.cy);
                }
                return ret;
            }
        };

        auto paintMessageBitmap = [&](OsdPos pos, const CSize & maxSize, MessageGdiItem & msgBitmap) {
            ASSERT(!msgBitmap.msg.IsEmpty());

            // paint if needed
            if (!msgBitmap.compBitmap.bitmap ||
                    (msgBitmap.bTrimmed && maxSize.cx > msgBitmap.lastMaxSize.cx) ||
                    (maxSize.cx < msgBitmap.compBitmap.bitmap->bitmapSize.cx)) {
                PaintMessage(maxSize, msgBitmap);
            }

            // update position on the grid
            if (msgBitmap.compBitmap.bitmap) {
                switch (pos) {
                    case OsdPos::TOPLEFT:
                        msgBitmap.compBitmap.location.anchor = UsedBackend::UsedCompositor::Anchor::TopLeft;
                        msgBitmap.compBitmap.location.rect = {
                            CPoint(margin),
                            msgBitmap.compBitmap.bitmap->bitmapSize
                        };
                        break;
                    case OsdPos::TOPRIGHT:
                        msgBitmap.compBitmap.location.anchor = UsedBackend::UsedCompositor::Anchor::TopRight;
                        msgBitmap.compBitmap.location.rect = {
                            CPoint(-msgBitmap.compBitmap.bitmap->bitmapSize.cx - margin.cx, margin.cy),
                            msgBitmap.compBitmap.bitmap->bitmapSize
                        };
                        break;
                    default:
                        ASSERT(FALSE);
                }
            }
        };

        auto placeMessageItem = [&](OsdPos pos, const CSize & maxSize, MessageItem & item) {
            if (!item.progMsg) {
                item.msgItem.msg = item.msg;
                paintMessageBitmap(pos, maxSize, item.msgItem);
            } else {
                // paint progress items
                for (auto& progItemPair : item.progItems) {
                    paintMessageBitmap(pos, maxSize, progItemPair.second);
                }

                // set static message to the first progress message
                ASSERT(!item.progItems.empty());
                item.msgItem = item.progItems.cbegin()->second;
                item.msg = item.msgItem.msg;
            }

            // send items to the compositor
            if (!item.progItems.empty()) {
                UsedBackend::UsedAnimator::UsedGdiProgressItems animProgItems;
                for (const auto& pair : item.progItems) {
                    animProgItems.emplace(pair.first, pair.second.compBitmap);
                }
                auto anim = item.bFadingOut ?
                            animator.FadeOutDiscardProgressAnimation(item.id, std::move(animProgItems)) :
                            animator.ProgressAnimation(item.id, std::move(animProgItems));
                compositor.PlaceOsdItem(item.id, item.msgItem.compBitmap, std::move(anim));
            } else {
                auto anim = item.bFadingOut ?
                            animator.FadeOutDiscardNormalAnimation(item.id) :
                            animator.NormalAnimation(item.id);
                compositor.PlaceOsdItem(item.id, item.msgItem.compBitmap, std::move(anim));
            }
        };

        CSize maxSize(layoutRect.Size());

        maxSize.cx -= margin.cx * 2;
        maxSize.cy -= margin.cx * 2;

        if (maxSize.cx <= 0 || maxSize.cy <= 0) {
            return;
        }

        compositor.BeginQueue();

        cleanFadedOut(m_msgTopLeft);
        cleanFadedOut(m_msgTopRight);
        cleanFadedOut(m_stickyTopLeft);
        cleanFadedOut(m_stickyTopRight);

        auto& msgTopLeft = m_msgTopLeft ? m_msgTopLeft : m_stickyTopLeft;
        auto& msgTopRight = m_msgTopRight ? m_msgTopRight : m_stickyTopRight;

        if (msgTopLeft && !msgTopRight) {
            // only left
            updateProgressItem(*msgTopLeft);
            placeMessageItem(OsdPos::TOPLEFT, maxSize, *msgTopLeft);
        } else if (!msgTopLeft && msgTopRight) {
            // only right
            updateProgressItem(*msgTopRight);
            placeMessageItem(OsdPos::TOPRIGHT, maxSize, *msgTopRight);
        } else if (msgTopLeft && msgTopRight) {
            // left and right
            updateProgressItem(*msgTopLeft);
            updateProgressItem(*msgTopRight);

            maxSize.cx -= margin.cx;
            const CSize fullTopRightSize = calcMessageItemFullSize(*msgTopRight);
            const CSize maxTopLeftSize((fullTopRightSize.cx < maxSize.cx / 2) ?
                                       maxSize.cx - fullTopRightSize.cx :
                                       maxSize.cx / 2,
                                       maxSize.cy);
            placeMessageItem(OsdPos::TOPLEFT, maxTopLeftSize, *msgTopLeft);

            const CSize maxTopRightSize(maxSize.cx - getMessageItemCurrentSize(*msgTopLeft).cx, maxSize.cy);
            placeMessageItem(OsdPos::TOPRIGHT, maxTopRightSize, *msgTopRight);
        }

        // TODO: this is basically a hack, because madvr is rather inconsistent with OsdRedrawFrame()
        if (m_pMadvrOsdService) {
            const PlaybackState::GraphState graphState = m_pMainFrame->GetPlaybackState()->GetGraphState();
            if (graphState.state == State_Paused) {
                m_pMainFrame->m_timerOneTime.Subscribe(CMainFrame::TimerOneTimeSubscriber::OSD_MADVR_REDRAW,
                                                       [this] { m_pMadvrOsdService->OsdRedrawFrame(); }, 100);
            } else {
                m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_MADVR_REDRAW);
            }
        }

        compositor.EndQueue();
    }

    CMainFrame* const m_pMainFrame = AfxGetMainFrame();

    std::shared_ptr<UsedBackend> m_backend;

    CComPtr<IMadVROsdServices> m_pMadvrOsdService;
    CComPtr<IMadVRSeekbarControl> m_pMadvrSeekbarService;
    CComPtr<IMadVRSettings> m_pMadvrSettingsService;

    std::unique_ptr<MessageItem> m_msgTopLeft, m_msgTopRight;
    std::unique_ptr<MessageItem> m_stickyTopLeft, m_stickyTopRight;
    std::unique_ptr<UsedSeekbar> m_seekbar;

    EventClient m_eventc;
};

class OsdMadvrTextProvider : public OsdMessageProvider, public OsdControlsProvider, public OsdSeekbarProvider
{
public:
    OsdMadvrTextProvider(IMadVRTextOsd* pMadvrTextService, IMadVRSettings* pMadvrSettingsService) {
        ENSURE(pMadvrTextService && pMadvrSettingsService);

        m_pMadvrTextService = pMadvrTextService;
        m_pMadvrSettingsService = pMadvrSettingsService;

        auto progressCallback = [this](MpcEvent ev) {
            switch (ev) {
                case MpcEvent::MEDIA_CHAPTERBAG_CHANGED:
                case MpcEvent::MEDIA_DURATION_CHANGED:
                case MpcEvent::MEDIA_POSITION_CHANGED:
                    if (m_msg && m_msg->progMsg) {
                        const CString newMsg = GetCurrentProgressMessage(m_msg->progMsg);
                        if (newMsg != m_msg->msg) {
                            m_msg->msg = newMsg;
                            VERIFY(SUCCEEDED(m_pMadvrTextService->OsdDisplayMessage(m_msg->msg, INT_MAX)));
                        }
                    } else if (m_stickyMsg && m_stickyMsg->progMsg) {
                        const CString newMsg = GetCurrentProgressMessage(m_stickyMsg->progMsg);
                        if (newMsg != m_stickyMsg->msg) {
                            m_stickyMsg->msg = newMsg;
                            if (!m_msg) {
                                VERIFY(SUCCEEDED(m_pMadvrTextService->OsdDisplayMessage(m_stickyMsg->msg, INT_MAX)));
                            }
                        }
                    }
                    break;
                default:
                    ASSERT(FALSE);
            }
        };

        GetEventd().Connect(m_eventc, {
            MpcEvent::MEDIA_CHAPTERBAG_CHANGED,
            MpcEvent::MEDIA_DURATION_CHANGED,
            MpcEvent::MEDIA_POSITION_CHANGED,
        }, std::move(progressCallback));
    }

    ~OsdMadvrTextProvider() {
        m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT);
        HideStickyMessage(OsdPos::TOPLEFT);
        HideMessage(OsdPos::TOPLEFT);
    }

    virtual void DisplayMessage(OsdPos pos, const CString& msg, int duration) override {
        if (msg.IsEmpty()) {
            HideMessage(pos);
        } else {
            m_msg = std::make_unique<TextItem>();
            m_msg->msg = msg;
            VERIFY(SUCCEEDED(m_pMadvrTextService->OsdDisplayMessage(m_msg->msg, INT_MAX)));
            if (duration > 0) {
                m_pMainFrame->m_timerOneTime.Subscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT,
                                                       [this] { HideMessage(OsdPos::TOPLEFT); }, duration);
            } else {
                m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT);
            }
        }
    }

    virtual void DisplayMessage(OsdPos pos, ProgressMessage msg, int duration) override {
        if (!msg) {
            HideMessage(pos);
        } else {
            m_msg = std::make_unique<TextItem>();
            m_msg->progMsg = msg;
            m_msg->msg = GetCurrentProgressMessage(m_msg->progMsg);
            VERIFY(SUCCEEDED(m_pMadvrTextService->OsdDisplayMessage(m_msg->msg, INT_MAX)));
            if (duration > 0) {
                m_pMainFrame->m_timerOneTime.Subscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT,
                                                       [this] { HideMessage(OsdPos::TOPLEFT); }, duration);
            } else {
                m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT);
            }
        }
    }

    virtual void HideMessage(OsdPos pos) override {
        UNREFERENCED_PARAMETER(pos);
        if (m_msg) {
            m_msg = nullptr;
            m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT);
            if (m_stickyMsg) {
                VERIFY(SUCCEEDED(m_pMadvrTextService->OsdDisplayMessage(m_stickyMsg->msg, INT_MAX)));
            } else {
                VERIFY(SUCCEEDED(m_pMadvrTextService->OsdClearMessage()));
            }
        }
    }

    virtual void DisplayStickyMessage(OsdPos pos, const CString& msg) override {
        if (msg.IsEmpty()) {
            HideStickyMessage(pos);
        } else {
            m_msg = nullptr;
            m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT);
            m_stickyMsg = std::make_unique<TextItem>();
            m_stickyMsg->msg = msg;
            VERIFY(SUCCEEDED(m_pMadvrTextService->OsdDisplayMessage(m_stickyMsg->msg, INT_MAX)));
        }
    }

    virtual void DisplayStickyMessage(OsdPos pos, OsdMessageProvider::ProgressMessage msg) override {
        if (!msg) {
            HideStickyMessage(pos);
        } else {
            m_msg = nullptr;
            m_pMainFrame->m_timerOneTime.Unsubscribe(CMainFrame::TimerOneTimeSubscriber::OSD_HIDE_TOPLEFT);
            m_stickyMsg = std::make_unique<TextItem>();
            m_stickyMsg->progMsg = msg;
            m_stickyMsg->msg = GetCurrentProgressMessage(m_stickyMsg->progMsg);
            VERIFY(SUCCEEDED(m_pMadvrTextService->OsdDisplayMessage(m_stickyMsg->msg, INT_MAX)));
        }
    }

    virtual void HideStickyMessage(OsdPos pos) override {
        UNREFERENCED_PARAMETER(pos);
        if (m_stickyMsg) {
            m_stickyMsg = nullptr;
            if (!m_msg) {
                VERIFY(SUCCEEDED(m_pMadvrTextService->OsdClearMessage()));
            }
        }
    }

    virtual bool PointOnOsdControl(const CPoint& videoPoint) override {
        bool ret = false;
        if (SeekbarEnabled()) {
            CRect rect;
            m_pMainFrame->m_pVideoWnd->GetClientRect(rect);
            rect.top = rect.bottom - 56; // TODO: query this through IMadVRInfo
            ret = !!rect.PtInRect(m_pMainFrame->m_pVideoWnd->GetClientPoint(videoPoint));
        }
        return ret;
    }

    virtual bool ReceiveInputEvent(OsdInputEvent event, const CPoint& videoPoint) override {
        if (!SeekbarEnabled()) {
            m_cursor = CMouse::Cursor::NONE;
            return false;
        }

        bool ret = false;

        CPoint mappedPoint(m_pMainFrame->m_pVideoWnd->GetClientPoint(videoPoint));
        MapWindowPoints(m_pMainFrame->m_pVideoWnd->m_hWnd, m_pMainFrame->m_hWnd, &mappedPoint, 1);
        const LPARAM lp = MAKELPARAM(mappedPoint.x, mappedPoint.y);

        switch (event) {
            case OsdInputEvent::MOUSE_DOWN:
                ret = (m_pMainFrame->SendMessage(WM_LBUTTONDOWN, MK_LBUTTON, lp) != 42);
                break;
            case OsdInputEvent::MOUSE_UP:
                ret = (m_pMainFrame->SendMessage(WM_LBUTTONUP, 0, lp) != 42);
                break;
            case OsdInputEvent::MOUSE_MOVE:
                m_pMainFrame->SendMessage(WM_MOUSEMOVE, 0, lp);
                m_cursor = PointOnOsdControl(videoPoint) ? CMouse::Cursor::ARROW : CMouse::Cursor::NONE;
                break;
            case OsdInputEvent::MOUSE_LEAVE:
                ret = true;
                m_cursor = CMouse::Cursor::NONE;
                break;
            default:
                ASSERT(FALSE);
        }

        return ret;
    }

    virtual CMouse::Cursor GetOsdMouseCursor() const override {
        return m_cursor;
    }

    bool SeekbarEnabled() const override {
        bool ret = false;
        if (m_pMadvrSettingsService && m_pMainFrame->m_fFullScreen) {
            BOOL bOptExcl = FALSE, bOptExclSeekbar = FALSE;
            VERIFY(m_pMadvrSettingsService->SettingsGetBoolean(L"enableExclusive", &bOptExcl));
            VERIFY(m_pMadvrSettingsService->SettingsGetBoolean(L"enableSeekbar", &bOptExclSeekbar));
            ret = (bOptExcl && bOptExclSeekbar);
        }
        return ret;
    }

protected:
    struct TextItem {
        // static
        CString msg;

        // progress
        ProgressMessage progMsg;
    };

    CString GetCurrentProgressMessage(const ProgressMessage& progMsg) const {
        return progMsg(m_pMainFrame->GetPlaybackState()->GetPos(), 0).cbegin()->second;
    }

    CMainFrame* const m_pMainFrame = AfxGetMainFrame();
    CComPtr<IMadVRTextOsd> m_pMadvrTextService;
    CComPtr<IMadVRSettings> m_pMadvrSettingsService;

    CMouse::Cursor m_cursor = CMouse::Cursor::NONE;

    std::unique_ptr<TextItem> m_msg, m_stickyMsg;

    EventClient m_eventc;
};

void OsdService::Start(IInternalOsdService* pInternalOsdService)
{
    ASSERT(!m_prov);
    if (pInternalOsdService) {
        m_prov = std::make_shared<OsdMainGdiProvider>(pInternalOsdService);
    } else {
        ASSERT(FALSE);
    }
}

void OsdService::Start(IMadVROsdServices* pMadvrOsdService, IMadVRSeekbarControl* pMadvrSeekbarService, IMadVRSettings* pMadvrSettingsService)
{
    ASSERT(!m_prov);
    if (pMadvrOsdService && pMadvrSeekbarService && pMadvrSettingsService) {
        m_prov = std::make_shared<OsdMainGdiProvider>(pMadvrOsdService, pMadvrSeekbarService, pMadvrSettingsService);
    } else {
        ASSERT(FALSE);
    }
}

void OsdService::Start(IMadVRTextOsd* pMadvrTextService, IMadVRSettings* pMadvrSettingsService)
{
    ASSERT(!m_prov);
    if (pMadvrTextService && pMadvrSettingsService) {
        m_prov = std::make_shared<OsdMadvrTextProvider>(pMadvrTextService, pMadvrSettingsService);
    } else {
        ASSERT(FALSE);
    }
}

void OsdService::Stop()
{
    m_prov = nullptr;
}

void OsdService::DisplayMessage(OsdPos pos, LPCTSTR msg, int duration)
{
    DisplayMessage(pos, CString(msg), duration);
}

void OsdService::DisplayMessage(OsdPos pos, const CString& msg, int duration)
{
    if (m_prov) {
        m_prov->DisplayMessage(pos, msg, duration);
    }
}

void OsdService::DisplayMessage(OsdPos pos, OsdMessageProvider::ProgressMessage msg, int duration)
{
    if (m_prov) {
        m_prov->DisplayMessage(pos, msg, duration);
    }
}

void OsdService::HideMessage(OsdPos pos)
{
    if (m_prov) {
        m_prov->HideMessage(pos);
    }
}

void OsdService::DisplayStickyMessage(OsdPos pos, LPCTSTR msg)
{
    DisplayStickyMessage(pos, CString(msg));
}

void OsdService::DisplayStickyMessage(OsdPos pos, const CString& msg)
{
    if (m_prov) {
        m_prov->DisplayStickyMessage(pos, msg);
    }
}

void OsdService::DisplayStickyMessage(OsdPos pos, OsdMessageProvider::ProgressMessage msg)
{
    if (m_prov) {
        m_prov->DisplayStickyMessage(pos, msg);
    }
}

void OsdService::HideStickyMessage(OsdPos pos)
{
    if (m_prov) {
        m_prov->HideStickyMessage(pos);
    }
}

bool OsdService::SeekbarEnabled() const
{
    bool ret = false;
    if (auto prov = std::dynamic_pointer_cast<OsdSeekbarProvider>(m_prov)) {
        ret = prov->SeekbarEnabled();
    }
    return ret;
}

bool OsdService::PointOnOsdControl(const CPoint& videoPoint)
{
    bool ret = false;
    if (auto prov = std::dynamic_pointer_cast<OsdControlsProvider>(m_prov)) {
        ret = prov->PointOnOsdControl(videoPoint);
    }
    return ret;
}

bool OsdService::ReceiveInputEvent(OsdInputEvent event, const CPoint& videoPoint)
{
    bool ret = false;
    if (auto prov = std::dynamic_pointer_cast<OsdControlsProvider>(m_prov)) {
        ret = prov->ReceiveInputEvent(event, videoPoint);
    }
    return ret;
}

CMouse::Cursor OsdService::GetOsdMouseCursor() const
{
    CMouse::Cursor ret = CMouse::Cursor::NONE;
    if (auto prov = std::dynamic_pointer_cast<OsdControlsProvider>(m_prov)) {
        ret = prov->GetOsdMouseCursor();
    }
    return ret;
}

OsdMessageProvider::ProgressMessageReturn OsdProgressMessage::Position::operator()(PlaybackState::Pos pos, REFERENCE_TIME rtSpan)
{
    const REFERENCE_TIME rtSec = 10000000;
    const REFERENCE_TIME rtRoundFirst = rtSec / 4;

    const bool bRemaining = (AfxGetAppSettings().fRemainingTime && pos.rtDur > 0 && pos.rtNow < pos.rtDur);

    OsdMessageProvider::ProgressMessageReturn ret;

    if (m_eFirst == FirstState::Unset) {
        if (bRemaining) {
            m_eFirst = FirstState::Remaining;
            m_rtFirst = pos.rtDur - (pos.rtDur - pos.rtNow + rtRoundFirst) / rtSec * rtSec;
        } else {
            m_eFirst = FirstState::Normal;
            m_rtFirst = (pos.rtNow + rtRoundFirst) / rtSec * rtSec;
        }
    }

    if (m_eFirst == FirstState::Remaining || m_eFirst == FirstState::Normal) {
        if (pos.rtNow >= m_rtFirst ||
                (bRemaining && m_eFirst != FirstState::Remaining) ||
                (!bRemaining && m_eFirst != FirstState::Normal)) {
            m_eFirst = FirstState::Passed;
        } else if (m_rtFirst - pos.rtNow < rtRoundFirst) {
            pos.rtNow = m_rtFirst;
        }

    }

    pos.rtNow = pos.rtNow / rtSec * rtSec;
    for (const REFERENCE_TIME rtLast = pos.rtNow + rtSpan; pos.rtNow <= rtLast; pos.rtNow += rtSec) {
        ret.emplace(pos.rtNow, PlaybackState::GetPosString(pos));
    }

    return ret;
}

OsdMessageProvider::ProgressMessageReturn OsdProgressMessage::PositionWithStaticText::operator()(PlaybackState::Pos pos, REFERENCE_TIME rtSpan)
{
    auto ret = __super::operator()(pos, rtSpan);
    for (auto& pair : ret) {
        pair.second += m_text;
    }
    return ret;
}

// TODO: test reset device with all renderers
// TODO: CMainFrame::BuildGraphVideoAudio()
// TOOD: d3dfs and seekbar
// TODO: ticking messages option

// to madshi
// 1. redraw on pause
// 2. additional frames in smooth motion
// 3. ISR pause bug
// 4. exclusive mode notification
// 5. hack in fullscreen toggle
// 6. ISR and smooth motion
// 7. query madvr seekbar dimensions through some interface
