#include "stdafx.h"
#include "LegacyAssocDlg.h"

#include "Registration/Backend.h"
#include "Registration/Manager.h"

#include "mplayerc.h"
#include "PathUtils.h"
#include "SysVersion.h"

namespace
{
    const bool EnableUserChoiceReset = true;
}

CLegacyAssocDlg::CLegacyAssocDlg(CWnd* pParent)
    : CResizableDialog(IDD, pParent)
{
}

CLegacyAssocDlg::Type CLegacyAssocDlg::GetType(MediaFormats::Type type)
{
    switch (type) {
        case MediaFormats::Type::Audio:
        case MediaFormats::Type::PlaylistAudio:
            return Type::Audio;

        case MediaFormats::Type::Video:
        case MediaFormats::Type::PlaylistVideo:
            return Type::Video;

        case MediaFormats::Type::Misc:
        default:
            return Type::Other;
    }
}

int CALLBACK CLegacyAssocDlg::SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    ASSERT(lParam1);
    ASSERT(lParam2);

    const auto& item1 = *reinterpret_cast<Item*>(lParam1);
    const auto& item2 = *reinterpret_cast<Item*>(lParam2);

    switch ((SortOrder)lParamSort) {
        case SortOrder::NameUp:
            return item1.name.Compare(item2.name);

        case SortOrder::NameDown:
            return -item1.name.Compare(item2.name);

        case SortOrder::DescDown:
            return item1.desc.Compare(item2.desc);

        case SortOrder::DescUp:
            return -item1.desc.Compare(item2.desc);
    }

    ASSERT(FALSE);
    return 0;
}

BOOL CLegacyAssocDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // Setup window auto-resize.
    AddAnchor(IDC_LIST1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);

    Registration::ShellResourceLoader shellResourceLoader;

    // Generate files extensions list.
    {
        const auto& s = AfxGetAppSettings();

        auto f = MediaFormats_ExtensionsLambda(...) {
            bool sel = (Registration::Backend::GetUserFileChoice(ext) == progid);
            m_items.emplace_back(Item{GetType(format.type), format.icon, progid, ext,
                                      shellResourceLoader.GetString(format.friendlyName, s.language), sel, false});
        };

        MediaFormats::IterateExtensions(f);
    }

    // Generate protocol schemes list.
    if (SysVersion::IsVistaOrLater()) {
        const auto& s = AfxGetAppSettings();

        auto f = MediaFormats_ProtocolSchemesLambda(...) {
            bool sel = (Registration::Backend::GetUserProtocolChoice(scheme) == progid);
            m_items.emplace_back(Item{Type::Video, protocol.icon, progid, scheme,
                                      shellResourceLoader.GetString(protocol.friendlyName, s.language), sel, true});
        };

        MediaFormats::IterateProtocolSchemes(f);
    }

    // Generate icons list.
    {
        int w = GetSystemMetrics(SM_CXSMICON);
        int h = GetSystemMetrics(SM_CYSMICON);

        m_icons.Create(w, h, ILC_COLOR32, m_items.size(), 0);

        for (const Item& item : m_items) {
            HICON hIcon = shellResourceLoader.GetIcon(item.icon, w, h);
            VERIFY(m_icons.Add(hIcon) >= 0);
            DestroyIcon(hIcon);
        }
    }

    // Configure and populate associations list.
    {
        m_disableUpdateBulkCheck = true;

        m_list.EnableGroupView(TRUE);
        m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);

        m_list.SetImageList(&m_icons, LVSIL_SMALL);

        {
            LVGROUP group = { sizeof(group) };
            group.mask = LVGF_HEADER | LVGF_STATE | LVGF_GROUPID;
            group.pszHeader = L"Extensions"; // TODO: move to mpc-hc.rc
            group.cchHeader = _tcsclen(group.pszHeader);
            group.state = LVGS_NORMAL;
            group.iGroupId = 0;
            m_list.InsertGroup(0, &group);
        }

        if (SysVersion::IsVistaOrLater()) {
            LVGROUP group = { sizeof(group) };
            group.mask = LVGF_HEADER | LVGF_STATE | LVGF_GROUPID;
            group.pszHeader = L"Protocols"; // TODO: move to mpc-hc.rc
            group.cchHeader = _tcsclen(group.pszHeader);
            group.state = LVGS_NORMAL;
            group.iGroupId = 1;
            m_list.InsertGroup(1, &group);
        }

        // TODO: move to mpc-hc.rc
        m_list.InsertColumn(0, L"Name");
        m_list.InsertColumn(1, L"Description");

        for (size_t i = 0, n = m_items.size(); i < n; i++) {
            const Item& data = m_items[i];

            LVITEM item = { sizeof(item) };
            item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_GROUPID;
            item.iItem = i;
            item.pszText = (LPTSTR)(LPCTSTR)data.name;
            item.iImage = i;
            item.lParam = (LPARAM)&data;
            item.iGroupId = data.protocol ? 1 : 0;

            m_list.InsertItem(&item);
            m_list.SetItemText(i, 1, data.desc);
            m_list.SetCheck(i, data.selected);
        }

        m_list.SetColumnWidth(0, LVSCW_AUTOSIZE);
        m_list.SetColumnWidth(1, LVSCW_AUTOSIZE);

        SetSort(SortOrder::NameUp);

        m_disableUpdateBulkCheck = false;
        UpdateBulkCheck();
    }

    GotoDlgCtrl(&m_list);
    return FALSE;
}

void CLegacyAssocDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_list);
    DDX_Control(pDX, IDC_CHECK1, m_selectAll);
    DDX_Control(pDX, IDC_CHECK2, m_selectVideo);
    DDX_Control(pDX, IDC_CHECK3, m_selectAudio);
}

void CLegacyAssocDlg::OnOK()
{
    UpdateData();

    // Write user selection.
    for (int i = 0, n = m_list.GetItemCount(); i < n; i++) {
        auto pItem = reinterpret_cast<const Item*>(m_list.GetItemData(i));
        if (m_list.GetCheck(i)) {
            if (pItem->protocol) {
                Registration::Backend::SetUserProtocolChoice(pItem->name, pItem->progid);
            } else {
                Registration::Backend::SetUserFileChoice(pItem->name, pItem->progid);
            }
        } else if (EnableUserChoiceReset) {
            if (pItem->protocol) {
                Registration::Backend::UnsetUserProtocolChoice(pItem->name);
            } else {
                Registration::Backend::UnsetUserFileChoice(pItem->name);
            }
        }
    }

    __super::OnOK();
}

void CLegacyAssocDlg::SetSort(SortOrder order)
{
    m_list.SortItems(SortFunc, (DWORD_PTR)order);

    CHeaderCtrl* pHeaderCtrl = m_list.GetHeaderCtrl();
    ASSERT(pHeaderCtrl);

    HDITEM header;

    VERIFY(pHeaderCtrl->GetItem(0, &header));
    ASSERT(header.mask & HDI_FORMAT);
    header.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
    header.fmt |= (order == SortOrder::NameUp) ? HDF_SORTUP : (order == SortOrder::NameDown) ? HDF_SORTDOWN : 0;
    VERIFY(pHeaderCtrl->SetItem(0, &header));

    VERIFY(pHeaderCtrl->GetItem(1, &header));
    ASSERT(header.mask & HDI_FORMAT);
    header.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
    header.fmt |= (order == SortOrder::DescUp) ? HDF_SORTUP : (order == SortOrder::DescDown) ? HDF_SORTDOWN : 0;
    VERIFY(pHeaderCtrl->SetItem(1, &header));

    m_sort = order;
}

void CLegacyAssocDlg::UpdateBulkCheck()
{
    if (m_disableUpdateBulkCheck) {
        return;
    }

    size_t videoTypes = 0;
    size_t audioTypes = 0;

    for (const Item& item : m_items) {
        if (item.type == Type::Video) {
            videoTypes++;
        } else if (item.type == Type::Audio) {
            audioTypes++;
        }
    }

    size_t selectedTypes = 0;
    size_t selectedVideoTypes = 0;
    size_t selectedAudioTypes = 0;

    for (int i = 0, n = m_list.GetItemCount(); i < n; i++) {
        if (m_list.GetCheck(i)) {
            auto pItem = reinterpret_cast<const Item*>(m_list.GetItemData(i));
            if (pItem->type == Type::Video) {
                selectedVideoTypes++;
            } else if (pItem->type == Type::Audio) {
                selectedAudioTypes++;
            }
            selectedTypes++;
        }
    }

    m_selectAll.SetCheck(selectedTypes == m_items.size());
    m_selectVideo.SetCheck(selectedVideoTypes == videoTypes);
    m_selectAudio.SetCheck(selectedAudioTypes == audioTypes);
}

void CLegacyAssocDlg::OnHeaderClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    ASSERT(pNMHDR);
    ASSERT(pResult);

    int col = reinterpret_cast<NMLISTVIEW*>(pNMHDR)->iItem;

    if (col == 0) {
        SetSort((m_sort == SortOrder::NameUp) ? SortOrder::NameDown : SortOrder::NameUp);
    } else {
        ASSERT(col == 1);
        SetSort((m_sort == SortOrder::DescUp) ? SortOrder::DescDown : SortOrder::DescUp);
    }

    *pResult = 0;
}

void CLegacyAssocDlg::OnTryCheck(NMHDR* pNMHDR, LRESULT* pResult)
{
    ASSERT(pNMHDR);
    ASSERT(pResult);

    auto pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

    ASSERT(pNMLV->lParam);

    *pResult = 0;

    if (!EnableUserChoiceReset) {
        // Disallow un-checking already associated formats.
        if ((pNMLV->uChanged & LVIF_STATE) &&
                (pNMLV->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(BST_UNCHECKED + 1)) {
            *pResult = reinterpret_cast<Item*>(pNMLV->lParam)->selected;
        }
    }
}

void CLegacyAssocDlg::OnCheck(NMHDR* pNMHDR, LRESULT* pResult)
{
    ASSERT(pNMHDR);
    ASSERT(pResult);

    auto pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

    ASSERT(pNMLV->lParam);

    *pResult = 0;

    if ((pNMLV->uChanged & LVIF_STATE) && (pNMLV->uNewState & LVIS_STATEIMAGEMASK)) {
        UpdateBulkCheck();
    }
}

void CLegacyAssocDlg::OnCheckAll()
{
    m_disableUpdateBulkCheck = true;

    if (m_selectAll.GetCheck()) {
        for (int i = 0, n = m_list.GetItemCount(); i < n; i++) {
            if (m_list.GetCheck(i)) {
                auto pItem = reinterpret_cast<const Item*>(m_list.GetItemData(i));
                if (!pItem->selected || EnableUserChoiceReset) {
                    m_list.SetCheck(i, FALSE);
                }
            }
        }
    } else {
        for (int i = 0, n = m_list.GetItemCount(); i < n; i++) {
            m_list.SetCheck(i, TRUE);
        }
    }

    m_disableUpdateBulkCheck = false;
    UpdateBulkCheck();
}

void CLegacyAssocDlg::OnCheckType(bool check, Type type)
{
    m_disableUpdateBulkCheck = true;

    if (!check) {
        for (int i = 0, n = m_list.GetItemCount(); i < n; i++) {
            if (m_list.GetCheck(i)) {
                auto pItem = reinterpret_cast<const Item*>(m_list.GetItemData(i));
                if (pItem->type == type) {
                    if (!pItem->selected || EnableUserChoiceReset) {
                        m_list.SetCheck(i, FALSE);
                    }
                }
            }
        }
    } else {
        for (int i = 0, n = m_list.GetItemCount(); i < n; i++) {
            auto pItem = reinterpret_cast<const Item*>(m_list.GetItemData(i));
            if (pItem->type == type) {
                m_list.SetCheck(i, TRUE);
            }
        }
    }

    m_disableUpdateBulkCheck = false;
    UpdateBulkCheck();
}

void CLegacyAssocDlg::OnCheckVideo()
{
    OnCheckType(!m_selectVideo.GetCheck(), Type::Video);
}

void CLegacyAssocDlg::OnCheckAudio()
{
    OnCheckType(!m_selectAudio.GetCheck(), Type::Audio);
}

BEGIN_MESSAGE_MAP(CLegacyAssocDlg, CResizableDialog)
    ON_BN_CLICKED(IDC_CHECK1, OnCheckAll)
    ON_BN_CLICKED(IDC_CHECK2, OnCheckVideo)
    ON_BN_CLICKED(IDC_CHECK3, OnCheckAudio)
    ON_NOTIFY(HDN_ITEMCLICK, 0, OnHeaderClick)
    ON_NOTIFY(HDN_ITEMDBLCLICK, 0, OnHeaderClick)
    ON_NOTIFY(LVN_ITEMCHANGING, IDC_LIST1, OnTryCheck)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnCheck)
END_MESSAGE_MAP()
