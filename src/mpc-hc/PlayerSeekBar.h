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

#include "DSMPropertyBag.h"
#include "EventDispatcher.h"
#include <memory>

class CMainFrame;

class CDraggableSeekbar
{
public:
    CDraggableSeekbar(CMainFrame* pMainFrame);
    virtual ~CDraggableSeekbar();

    bool DraggingThumb() const;
    void HaltThumbDrag();

protected:
    virtual void DraggedThumb(const CPoint& point, REFERENCE_TIME rtPos) = 0;

    void StartThumbDrag();
    void DragThumb(const CPoint& point, REFERENCE_TIME rtPos);
    void StopThumbDrag(bool bHard = true);

private:
    REFERENCE_TIME m_rtDragThumbPos = 0;
    REFERENCE_TIME m_rtDragThumbHaltPos = 0;
    CPoint m_dragThumbPoint = { 0, 0 };
    CPoint m_dragThumbHaltPoint = { 0, 0 };
    bool m_bDraggingThumb = false;
    bool m_bDragThumbHaltedOnce = false;
    CMainFrame* m_pMainFrame;
};

class CPlayerSeekBar : public CDialogBar, public CDraggableSeekbar
{
    DECLARE_DYNAMIC(CPlayerSeekBar)

public:
    CPlayerSeekBar(CMainFrame* pMainFrame);
    virtual ~CPlayerSeekBar();
    virtual BOOL Create(CWnd* pParentWnd);

private:
    CMainFrame* m_pMainFrame;
    EventClient m_eventc;
    bool m_bEnabled;

    REFERENCE_TIME m_rtThumbPos;

    HCURSOR m_cursor;

    CToolTipCtrl m_tooltip;
    enum class TooltipState { HIDDEN, TRIGGERED, VISIBLE } m_tooltipState;
    TOOLINFO m_ti;
    CPoint m_tooltipPoint;
    bool m_bIgnoreLastTooltipPoint;
    CString m_tooltipText;

    std::unique_ptr<CDC> m_pEnabledThumb;
    std::unique_ptr<CDC> m_pDisabledThumb;
    CRect m_lastThumbRect;

    void EventCallback(MpcEvent ev);

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;

    void MoveThumb(const CPoint& point);
    void MoveThumbToPositition(REFERENCE_TIME rtPos);
    void SyncMediaToThumb();
    long ChannelPointFromPosition(REFERENCE_TIME rtPos) const;
    REFERENCE_TIME PositionFromClientPoint(const CPoint& point) const;

    virtual void DraggedThumb(const CPoint& point, REFERENCE_TIME rtPos) override;
    void CancelDrag();

    void CreateThumb(bool bEnabled, CDC& parentDC);
    CRect GetChannelRect() const;
    CRect GetThumbRect() const;
    CRect GetInnerThumbRect(bool bEnabled, const CRect& thumbRect) const;

    void UpdateTooltip(const CPoint& point);
    void UpdateTooltipPosition();
    void UpdateTooltipText();
    void ShowTooltip();
    void HideTooltip();

private:
    DECLARE_MESSAGE_MAP()

    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnXButtonDown(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnXButtonUp(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnXButtonDblClk(UINT nFlags, UINT nButton, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnMouseLeave();
    afx_msg LRESULT OnThemeChanged();
    afx_msg void OnCaptureChanged(CWnd* pWnd);
};
