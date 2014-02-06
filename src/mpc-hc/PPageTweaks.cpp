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
#include "mplayerc.h"
#include "PPageTweaks.h"
#include "MainFrm.h"
#include "SysVersion.h"


// CPPageTweaks dialog

IMPLEMENT_DYNAMIC(CPPageTweaks, CPPageBase)
CPPageTweaks::CPPageTweaks()
    : CPPageBase(CPPageTweaks::IDD, CPPageTweaks::IDD)
    , m_nJumpDistS(0)
    , m_nJumpDistM(0)
    , m_nJumpDistL(0)
    , m_fNotifySkype(TRUE)
    , m_fPreventMinimize(FALSE)
    , m_fUseWin7TaskBar(TRUE)
    , m_fUseSearchInFolder(FALSE)
    , m_fLCDSupport(FALSE)
    , m_fFastSeek(FALSE)
    , m_fShowChapters(TRUE)
    , m_fUseTimeTooltip(TRUE)
    , m_bHideWindowedMousePointer(TRUE)
    , m_bOsdEnableAnimation(TRUE)
{
    GetEventd().Connect(m_eventc, {
        MpcEvent::OSD_REINITIALIZE_PAINTER,
    });
}

CPPageTweaks::~CPPageTweaks()
{
}

void CPPageTweaks::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT1, m_nJumpDistS);
    DDX_Text(pDX, IDC_EDIT2, m_nJumpDistM);
    DDX_Text(pDX, IDC_EDIT3, m_nJumpDistL);
    DDX_Check(pDX, IDC_CHECK4, m_fNotifySkype);
    DDX_Check(pDX, IDC_CHECK6, m_fPreventMinimize);
    DDX_Check(pDX, IDC_CHECK_WIN7, m_fUseWin7TaskBar);
    DDX_Check(pDX, IDC_CHECK7, m_fUseSearchInFolder);
    DDX_Check(pDX, IDC_CHECK8, m_fUseTimeTooltip);
    DDX_Control(pDX, IDC_COMBO3, m_TimeTooltipPosition);
    DDX_Control(pDX, IDC_COMBO1, m_FontType);
    DDX_Control(pDX, IDC_COMBO2, m_FontSize);
    DDX_Control(pDX, IDC_COMBO4, m_FastSeekMethod);
    DDX_Control(pDX, IDC_COMBO5, m_osdColorBox);
    DDX_Control(pDX, IDC_BUTTON2, m_osdColorButton);
    DDX_Control(pDX, IDC_SLIDER1, m_osdTransparencySlider);
    DDX_Check(pDX, IDC_CHECK1, m_fFastSeek);
    DDX_Check(pDX, IDC_CHECK2, m_fShowChapters);
    DDX_Check(pDX, IDC_CHECK_LCD, m_fLCDSupport);
    DDX_Check(pDX, IDC_CHECK3, m_bHideWindowedMousePointer);
    DDX_Check(pDX, IDC_CHECK5, m_bOsdEnableAnimation);
}

int CALLBACK EnumFontProc(ENUMLOGFONT FAR* lf, NEWTEXTMETRIC FAR* tm, int FontType, LPARAM dwData)
{
    CAtlArray<CString>* fntl = (CAtlArray<CString>*)dwData;
    if (FontType == TRUETYPE_FONTTYPE) {
        fntl->Add(lf->elfFullName);
    }
    return 1; /* Continue the enumeration */
}

BOOL CPPageTweaks::OnInitDialog()
{
    __super::OnInitDialog();

    SetHandCursor(m_hWnd, IDC_COMBO1);

    CAppSettings& s = AfxGetAppSettings();

    m_nJumpDistS = s.nJumpDistS;
    m_nJumpDistM = s.nJumpDistM;
    m_nJumpDistL = s.nJumpDistL;
    m_fNotifySkype = s.bNotifySkype;

    m_fPreventMinimize = s.fPreventMinimize;

    m_fUseWin7TaskBar = s.fUseWin7TaskBar;
    if (!SysVersion::Is7OrLater()) {
        GetDlgItem(IDC_CHECK_WIN7)->EnableWindow(FALSE);
    }

    m_fUseSearchInFolder = s.fUseSearchInFolder;

    m_fUseTimeTooltip = s.fUseTimeTooltip;
    m_TimeTooltipPosition.AddString(ResStr(IDS_TIME_TOOLTIP_ABOVE));
    m_TimeTooltipPosition.AddString(ResStr(IDS_TIME_TOOLTIP_BELOW));
    m_TimeTooltipPosition.SetCurSel(s.nTimeTooltipPosition);
    m_TimeTooltipPosition.EnableWindow(m_fUseTimeTooltip);

    m_osdUndo = s.osd;

    m_fFastSeek = s.bFastSeek;
    m_FastSeekMethod.AddString(ResStr(IDS_FASTSEEK_LATEST));
    m_FastSeekMethod.AddString(ResStr(IDS_FASTSEEK_NEAREST));
    m_FastSeekMethod.SetCurSel(s.eFastSeekMethod);

    m_fShowChapters = s.fShowChapters;

    m_bHideWindowedMousePointer = s.bHideWindowedMousePointer;

    m_fLCDSupport = s.fLCDSupport;

    m_colors[0] = std::make_pair(CString("Background"), &s.osd.colorBackground);
    m_colors[1] = std::make_pair(CString("Border"), &s.osd.colorBorder);
    m_colors[2] = std::make_pair(CString("Text"), &s.osd.colorText);
    m_colors[3] = std::make_pair(CString("OSD seekbar channel"), &s.osd.colorChannel);
    m_colors[4] = std::make_pair(CString("OSD seekbar mark"), &s.osd.colorChapter);
    m_colors[5] = std::make_pair(CString("OSD seekbar thumb"), &s.osd.colorThumb);

    for (size_t i = 0; i < m_colors.size(); i++) {
        int idx = m_osdColorBox.AddString(m_colors[i].first);
        m_osdColorBox.SetItemData(idx, i);
    }
    m_osdColorBox.SetCurSel(0);
    OnOsdColorSelection();

    m_bOsdEnableAnimation = s.osd.bAnimation;

    m_osdTransparencySlider.SetRange(0, 255);
    m_osdTransparencySlider.SetPos(static_cast<int>(255 * (1.0f - s.osd.fAlpha) + 0.5f));

    m_FontType.Clear();
    m_FontSize.Clear();
    HDC dc = CreateDC(_T("DISPLAY"), nullptr, nullptr, nullptr);
    CAtlArray<CString> fntl;
    EnumFontFamilies(dc, nullptr, (FONTENUMPROC)EnumFontProc, (LPARAM)&fntl);
    DeleteDC(dc);
    for (size_t i = 0; i < fntl.GetCount(); ++i) {
        if (i > 0 && fntl[i - 1] == fntl[i]) {
            continue;
        }
        m_FontType.AddString(fntl[i]);
    }
    int iSel = m_FontType.FindStringExact(0, s.osd.fontName);
    if (iSel == CB_ERR) {
        iSel = 0;
    }
    m_FontType.SetCurSel(iSel);

    CString str;
    for (int i = 10; i < 26; ++i) {
        str.Format(_T("%d"), i);
        m_FontSize.AddString(str);
        if (s.osd.fontSize == i) {
            iSel = i;
        }
    }
    m_FontSize.SetCurSel(iSel - 10);

    EnableToolTips(TRUE);

    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageTweaks::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.nJumpDistS = m_nJumpDistS;
    s.nJumpDistM = m_nJumpDistM;
    s.nJumpDistL = m_nJumpDistL;
    s.bNotifySkype = !!m_fNotifySkype;

    s.fPreventMinimize = !!m_fPreventMinimize;
    s.fUseWin7TaskBar = !!m_fUseWin7TaskBar;
    s.fUseSearchInFolder = !!m_fUseSearchInFolder;
    s.fUseTimeTooltip = !!m_fUseTimeTooltip;
    s.nTimeTooltipPosition = m_TimeTooltipPosition.GetCurSel();

    s.bFastSeek = !!m_fFastSeek;
    s.eFastSeekMethod = static_cast<decltype(s.eFastSeekMethod)>(m_FastSeekMethod.GetCurSel());

    s.bHideWindowedMousePointer = !!m_bHideWindowedMousePointer;

    s.fShowChapters = !!m_fShowChapters;

    s.fLCDSupport = !!m_fLCDSupport;

    s.osd.bAnimation = !!m_bOsdEnableAnimation;

    // There is no main frame when the option dialog is displayed stand-alone
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_SKYPE);
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_SEEKBAR_CHAPTERS);
        pMainFrame->UpdateControlState(CMainFrame::UPDATE_WIN7_TASKBAR);
    }

    return __super::OnApply();
}

void CPPageTweaks::OnReset()
{
    AfxGetAppSettings().osd = m_osdUndo;
    m_eventc.FireEvent(MpcEvent::OSD_REINITIALIZE_PAINTER);
    __super::OnReset();
}

BEGIN_MESSAGE_MAP(CPPageTweaks, CPPageBase)
    ON_UPDATE_COMMAND_UI(IDC_COMBO4, OnUpdateFastSeek)
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
    ON_BN_CLICKED(IDC_CHECK8, OnUseTimeTooltipClicked)
    ON_BN_CLICKED(IDC_BUTTON2, OnSetOsdColor)
    ON_CBN_SELCHANGE(IDC_COMBO5, OnOsdColorSelection)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnReinitializeOsd)
    ON_CBN_SELCHANGE(IDC_COMBO2, OnReinitializeOsd)
    ON_NOTIFY_EX_RANGE(TTN_NEEDTEXT, 0, 0xFFFF, OnToolTipNotify)
    ON_WM_HSCROLL()
END_MESSAGE_MAP()


// CPPageTweaks message handlers

void CPPageTweaks::OnUpdateFastSeek(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsDlgButtonChecked(IDC_CHECK1));
}

void CPPageTweaks::OnBnClickedButton1()
{
    m_nJumpDistS = DEFAULT_JUMPDISTANCE_1;
    m_nJumpDistM = DEFAULT_JUMPDISTANCE_2;
    m_nJumpDistL = DEFAULT_JUMPDISTANCE_3;

    UpdateData(FALSE);
    SetModified();
}

void CPPageTweaks::OnOsdColorSelection()
{
    m_osdColorButton.SetColor(*m_colors[m_osdColorBox.GetItemData(m_osdColorBox.GetCurSel())].second);
}

void CPPageTweaks::OnSetOsdColor()
{
    const size_t colorIdx = m_osdColorBox.GetItemData(m_osdColorBox.GetCurSel());
    COLORREF* pColor = m_colors[colorIdx].second;
    CColorDialog dlg(*pColor);
    dlg.m_cc.Flags |= CC_FULLOPEN;
    if (dlg.DoModal() == IDOK) {
        *pColor = dlg.m_cc.rgbResult;
        m_osdColorButton.SetColor(dlg.m_cc.rgbResult);
        if (colorIdx < 3) {
            OnReinitializeOsd();
        }
    }
}

void CPPageTweaks::OnReinitializeOsd()
{
    auto& s = AfxGetAppSettings();
    s.osd.fontSize = m_FontSize.GetCurSel() + 10;
    m_FontType.GetLBText(m_FontType.GetCurSel(), s.osd.fontName);
    s.osd.fAlpha = (255 - m_osdTransparencySlider.GetPos()) / 255.0f;
    m_eventc.FireEvent(MpcEvent::OSD_REINITIALIZE_PAINTER);
    if (CMainFrame* pMainFrame = AfxGetMainFrame()) {
        pMainFrame->m_osd.DisplayMessage(_T("Test Message"));
    }
    SetModified();
}

void CPPageTweaks::OnUseTimeTooltipClicked()
{
    m_TimeTooltipPosition.EnableWindow(IsDlgButtonChecked(IDC_CHECK8));

    SetModified();
}

BOOL CPPageTweaks::OnToolTipNotify(UINT id, NMHDR* pNMH, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = reinterpret_cast<LPTOOLTIPTEXT>(pNMH);
    int cid = ::GetDlgCtrlID((HWND)pNMH->idFrom);
    if (cid == IDC_COMBO1) {
        CDC* pDC = m_FontType.GetDC();
        CFont* pFont = m_FontType.GetFont();
        CFont* pOldFont = pDC->SelectObject(pFont);
        TEXTMETRIC tm;
        pDC->GetTextMetrics(&tm);
        CRect rc;
        m_FontType.GetWindowRect(rc);
        rc.right -= GetSystemMetrics(SM_CXVSCROLL) * GetSystemMetrics(SM_CXEDGE);
        int i = m_FontType.GetCurSel();
        CString str;
        m_FontType.GetLBText(i, str);
        CSize sz;
        sz = pDC->GetTextExtent(str);
        pDC->SelectObject(pOldFont);
        m_FontType.ReleaseDC(pDC);
        sz.cx += tm.tmAveCharWidth;
        str = str.Left(_countof(pTTT->szText));
        if (sz.cx > rc.Width()) {
            _tcscpy_s(pTTT->szText, str);
            pTTT->hinst = nullptr;
        }

        return TRUE;
    }

    return FALSE;
}

void CPPageTweaks::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (*pScrollBar == m_osdTransparencySlider) {
        OnReinitializeOsd();
    }

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

