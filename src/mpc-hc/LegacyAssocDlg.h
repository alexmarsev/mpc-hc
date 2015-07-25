#pragma once

#include "MediaFormats/Formats.h"
#include "resource.h"

#include "ResizableLib/ResizableDialog.h"

#include <afxcmn.h>

class CLegacyAssocDlg final
    : public CResizableDialog
{
public:

    enum { IDD = IDD_LEGACYASSOC_DLG };

    CLegacyAssocDlg(CWnd* pParent);
    CLegacyAssocDlg(const CLegacyAssocDlg&) = delete;
    CLegacyAssocDlg& operator=(const CLegacyAssocDlg&) = delete;
    ~CLegacyAssocDlg() = default;

private:

    enum class Type {
        Audio,
        Video,
        Other,
    };

    struct Item {
        Type type;
        int icon;
        CString progid;
        CString name;
        CString desc;
        bool selected;
        bool protocol;
    };

    enum class SortOrder {
        NameUp,
        NameDown,
        DescUp,
        DescDown,
    };

    Type GetType(MediaFormats::Type type);

    static int CALLBACK SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* pDX) override;

    void OnOK() override;

    void OnTimer(UINT_PTR nIDEvent);

    void SetSort(SortOrder order);

    void UpdateBulkCheck();

    void OnHeaderClick(NMHDR* pNMHDR, LRESULT* pResult);
    void OnTryCheck(NMHDR* pNMHDR, LRESULT* pResult);
    void OnCheck(NMHDR* pNMHDR, LRESULT* pResult);

    void OnCheckAll();
    void OnCheckType(bool check, Type type);
    void OnCheckVideo();
    void OnCheckAudio();

    DECLARE_MESSAGE_MAP()

    CButton m_selectAll;
    CButton m_selectVideo;
    CButton m_selectAudio;
    CListCtrl m_list;
    SortOrder m_sort;
    CImageList m_icons;
    std::vector<Item> m_items;

    bool m_disableUpdateBulkCheck = false;
};
