/*
 * (C) 2015 see Authors.txt
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
#include "PPageFormats.h"

#include "mplayerc.h"
#include "LegacyAssocDlg.h"
#include "PathUtils.h"
#include "SysVersion.h"
#include "Registration/Backend.h"
#include "Registration/Manager.h"
#include "Registration/UserOverride.h"

#include "shlobj.h"

namespace
{
    void OpenDefaultProgramsControlPanelPage()
    {
        //CComPtr<IOpenControlPanel> cp;
        //if (SUCCEEDED(cp.CoCreateInstance(CLSID_OpenControlPanel, nullptr, CLSCTX_ALL))) {
        //    cp->Open(L"Microsoft.DefaultPrograms", L"pageDefaultProgram", nullptr);
        //}
        CComPtr<IApplicationAssociationRegistrationUI> ui;
        if (SUCCEEDED(ui.CoCreateInstance(CLSID_ApplicationAssociationRegistrationUI, nullptr, CLSCTX_ALL))) {
            ui->LaunchAdvancedAssociationUI(_T("Media Player Classic"));
        }
    }
}

CPPageFormats::CPPageFormats()
    : CPPageBase(IDD, IDD)
    , m_timerOneSecond(this, TIMER_ONE_SECOND, 1000)
{
}

void CPPageFormats::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CHECK1, m_noFolderVerbs);
}

BOOL CPPageFormats::OnInitDialog()
{
    __super::OnInitDialog();

    UpdateData(FALSE);

    const auto& s = AfxGetAppSettings();

    m_noFolderVerbs.SetCheck(s.noFolderVerbs);

    RefreshAssociations();

    return TRUE;
}

BOOL CPPageFormats::OnSetActive()
{
    m_timerOneSecond.Subscribe(0, [this] { RefreshAssociations(); });
    return __super::OnSetActive();
}

BOOL CPPageFormats::OnKillActive()
{
    m_timerOneSecond.Unsubscribe(0);
    return __super::OnKillActive();
}

BOOL CPPageFormats::OnApply()
{
    UpdateData(TRUE);

    auto& s = AfxGetAppSettings();

    s.noFolderVerbs = !!m_noFolderVerbs.GetCheck();
    s.registrationUserOverride.Apply(!s.fKeepHistory, s.noFolderVerbs);

    return __super::OnApply();
}

void CPPageFormats::OnTimer(UINT_PTR nIDEvent)
{
    ASSERT(nIDEvent == TIMER_ONE_SECOND);
    m_timerOneSecond.NotifySubscribers();
}

void CPPageFormats::OnAssociationWindowButtonPressed()
{
    if (SysVersion::Is8OrLater() || (SysVersion::IsVistaOrLater() && m_registrationSystemLevel)) {
        OpenDefaultProgramsControlPanelPage();
    } else {
        CLegacyAssocDlg(this).DoModal();
        RefreshAssociations();
    }
}

void CPPageFormats::OnRegistrationState(CCmdUI* pCmdUI)
{
    bool registered;

    if (m_missingShellLibrary || m_registrationLegacy) {
        registered = false;
    } else if (m_registrationUserLevel) {
        registered = !IsDifferentSystemLevelPath();
    } else if (m_registrationSystemLevel) {
        registered = !IsDifferentUserLevelPath();
    } else {
        registered = false;
    }

    pCmdUI->Enable(registered);
}

void CPPageFormats::OnRegistrationButtonPressed()
{
    auto& s = AfxGetAppSettings();

    if (m_registrationLegacy) {
        if (IsUserAnAdmin()) {
            Registration::Unregister(Registration::Type::Legacy);
        } else {
            CMPlayerCApp::RunAsAdministrator(PathUtils::GetProgramPath(true), _T("/unregisterSystem"), true);
        }
        QueryRegistrationStatus();
    }

    if (!m_registrationSystemLevel) {
        if (!m_registrationUserLevel || IsDifferentUserLevelPath()) {
            Registration::Register(Registration::Type::User);
            QueryRegistrationStatus();
            s.registrationUserOverride.Apply(!s.fKeepHistory, s.noFolderVerbs);
        } else if (m_registrationUserLevel) {
            Registration::Unregister(Registration::Type::User);
            QueryRegistrationStatus();
        }
    }

    SetRegistrationButtonLabel();
    SetRegistrationStatusText();
}

void CPPageFormats::OnRegistrationButtonState(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!m_missingShellLibrary && (m_registrationLegacy || !m_registrationSystemLevel));
}

void CPPageFormats::SetRegistrationButtonLabel()
{
    bool update = m_registrationLegacy || (!m_registrationSystemLevel && IsDifferentUserLevelPath());
    bool unregister = !update && m_registrationUserLevel && !IsDifferentUserLevelPath();

    // TODO: move to .rc
    CString label = update ? _T("Update Application Registration") :
                    unregister ? _T("Unregister Application") : _T("Register Application");
    SetDlgItemText(IDC_BUTTON1, label);
}

void CPPageFormats::SetRegistrationStatusText()
{
    int videoTypesSelected = 0;
    int videoTypesTotal = 0;
    int audioTypesSelected = 0;
    int audioTypesTotal = 0;
    int otherTypesSelected = 0;
    int otherTypesTotal = 0;
    int protocolsSelected = 0;
    int protocolsTotal = 0;

    CComPtr<IApplicationAssociationRegistration> reg;
    if (SysVersion::IsVistaOrLater()) {
        reg.CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_ALL);
    }

    auto f1 = MediaFormats_ExtensionsLambda(...) {
        CString selected;
        if (reg) {
            CComHeapPtr<WCHAR> pointer;
            if (SUCCEEDED(reg->QueryCurrentDefault(ext, AT_FILEEXTENSION, AL_EFFECTIVE, &pointer))) {
                selected = pointer;
            }
        } else {
            selected = Registration::Backend::GetUserFileChoice(ext);
        }

        switch (format.type) {
            case MediaFormats::Type::Audio:
            case MediaFormats::Type::PlaylistAudio:
                audioTypesSelected += (progid == selected) ? 1 : 0;
                audioTypesTotal += 1;
                break;

            case MediaFormats::Type::Video:
            case MediaFormats::Type::PlaylistVideo:
                videoTypesSelected += (progid == selected) ? 1 : 0;
                videoTypesTotal += 1;
                break;

            case MediaFormats::Type::Misc:
                otherTypesSelected += (progid == selected) ? 1 : 0;
                otherTypesTotal += 1;
                break;
        }
    };

    auto f2 = MediaFormats_ProtocolSchemesLambda(...) {
        CString selected;
        if (reg) {
            CComHeapPtr<WCHAR> pointer;
            if (SUCCEEDED(reg->QueryCurrentDefault(scheme, AT_URLPROTOCOL, AL_EFFECTIVE, &pointer))) {
                selected = pointer;
            }
        }

        protocolsSelected += (progid == selected) ? 1 : 0;
        protocolsTotal += 1;
    };

    // TODO: move to .rc
    CString noRes = _T("%s file is missing, registration is not available.");
    CString notRegistered = _T("MPC-HC is not registered within the system.");
    CString legacy = _T("Application registration corresponds to an older version of MPC-HC and needs to be updated.");
    CString different = _T("Application registration points to a different instance of MPC-HC.");
    CString header = _T("MPC-HC is associated to open:");
    CString video = _T("  Video file types: %d out of %d");
    CString videoAll = _T("  Video file types: all");
    CString audio = _T("  Audio file types: %d out of %d");
    CString audioAll = _T("  Audio file types: all");
    CString other = _T("  Other file types: %d out of %d");
    CString otherAll = _T("  Other file types: all");
    CString protocols = _T("  URL protocols: %d out of %d");
    CString protocolsAll = _T("  URL protocols: all");

    CString text;

    if (m_missingShellLibrary) {
        text.Format(noRes, Registration::GetShellResFile());
    } else if (m_registrationLegacy) {
        text = legacy;
    } else if (IsDifferentUserLevelPath() || IsDifferentSystemLevelPath()) {
        text = different;
    } else if (m_registrationUserLevel || m_registrationSystemLevel) {
        MediaFormats::IterateExtensions(f1);
        if (SysVersion::IsVistaOrLater()) {
            MediaFormats::IterateProtocolSchemes(f2);
        }

        text = header;
        auto append = [&text](int selected, int total, const CString & format, const CString & all) {
            if (total > 0) {
                CString appendage;
                if (selected < total) {
                    appendage.Format(format, selected, total);
                } else {
                    appendage = all;
                }
                text += _T("\r\n") + appendage;
            }
        };
        append(videoTypesSelected, videoTypesTotal, video, videoAll);
        append(audioTypesSelected, audioTypesTotal, audio, audioAll);
        append(otherTypesSelected, otherTypesTotal, other, otherAll);
        append(protocolsSelected, protocolsTotal, protocols, protocolsAll);
    } else {
        text = notRegistered;
    }

    SetDlgItemText(IDC_STATIC2, text);
}

bool CPPageFormats::IsDifferentUserLevelPath()
{
    return m_registrationUserLevel && PathUtils::GetProgramPath(true).CompareNoCase(m_registrationUserLevelPath) != 0;
}

bool CPPageFormats::IsDifferentSystemLevelPath()
{
    return m_registrationSystemLevel && PathUtils::GetProgramPath(true).CompareNoCase(m_registrationSystemLevelPath) != 0;
}

void CPPageFormats::QueryRegistrationStatus()
{
    Registration::Type type;

    m_missingShellLibrary = !PathUtils::Exists(Registration::GetShellResFile());

    type = Registration::QueryCurrentSystemLevelRegistration(m_registrationSystemLevelPath);
    m_registrationSystemLevel = Registration::IsSystemLevelType(type);
    m_registrationLegacy = (type == Registration::Type::Legacy);

    type = Registration::QueryCurrentUserLevelRegistration(m_registrationUserLevelPath);
    m_registrationUserLevel = Registration::IsUserLevelType(type);
}

void CPPageFormats::RefreshAssociations()
{
    QueryRegistrationStatus();
    SetRegistrationButtonLabel();
    SetRegistrationStatusText();
}

BEGIN_MESSAGE_MAP(CPPageFormats, CPPageBase)
    ON_BN_CLICKED(IDC_BUTTON1, OnRegistrationButtonPressed)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnRegistrationButtonState)
    ON_BN_CLICKED(IDC_BUTTON2, OnAssociationWindowButtonPressed)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnRegistrationState)
    ON_UPDATE_COMMAND_UI(IDC_CHECK1, OnRegistrationState)
    ON_WM_TIMER()
END_MESSAGE_MAP()
