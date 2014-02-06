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

#pragma once

#include "AppSettings.h"
#include "ColorButton.h"
#include "EventDispatcher.h"
#include "PPageBase.h"

#include <array>
#include <utility>

// CPPageTweaks dialog

class CPPageTweaks : public CPPageBase
{
    DECLARE_DYNAMIC(CPPageTweaks)

public:
    CPPageTweaks();
    virtual ~CPPageTweaks();

    // Dialog Data
    enum { IDD = IDD_PPAGETWEAKS };
    int m_nJumpDistS;
    int m_nJumpDistM;
    int m_nJumpDistL;
    BOOL m_fNotifySkype;

    BOOL m_fPreventMinimize;
    BOOL m_fUseWin7TaskBar;
    BOOL m_fUseSearchInFolder;
    BOOL m_fUseTimeTooltip;
    BOOL m_bHideWindowedMousePointer;
    CComboBox m_TimeTooltipPosition;
    CComboBox m_FontSize;
    CComboBox m_FontType;
    CComboBox m_FastSeekMethod;

    BOOL m_fFastSeek;
    BOOL m_fShowChapters;

    BOOL m_fLCDSupport;

    EventClient m_eventc;
    CAppSettings::Osd m_osdUndo;
    std::array<std::pair<CString, COLORREF*>, 6> m_colors;
    CComboBox m_osdColorBox;
    CColorButton m_osdColorButton;
    CSliderCtrl m_osdTransparencySlider;
    BOOL m_bOsdEnableAnimation;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();
    virtual void OnReset();

    DECLARE_MESSAGE_MAP()

public:
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMH, LRESULT* pResult);
    afx_msg void OnUpdateFastSeek(CCmdUI* pCmdUI);
    afx_msg void OnBnClickedButton1();
    afx_msg void OnUseTimeTooltipClicked();
    afx_msg void OnOsdColorSelection();
    afx_msg void OnSetOsdColor();
    afx_msg void OnReinitializeOsd();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};
