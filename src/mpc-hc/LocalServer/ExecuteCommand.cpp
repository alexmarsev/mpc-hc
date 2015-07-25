#include "stdafx.h"
#include "ExecuteCommand.h"

#include "../AppSettings.h"
#include "../mplayerc.h"

#include "PathUtils.h"
#include "WinapiFunc.h"

#include <deque>

namespace LocalServer
{
    namespace
    {
        CString Escape(const CString& s)
        {
            return _T('"') + s + _T('"');
        }

        template <typename A>
        bool SendCommandLine(HWND hWindow, A&& args)
        {
            DWORD dwProcessId = 0;
            if (GetWindowThreadProcessId(hWindow, &dwProcessId)) {
                VERIFY(AllowSetForegroundWindow(dwProcessId));
            }

            if (IsIconic(hWindow)) {
                ShowWindow(hWindow, SW_RESTORE);
            }

            int dataSize = 0;

            for (const auto& arg : args) {
                dataSize += (arg.GetLength() + 1) * sizeof(TCHAR);
            }

            dataSize += 4;

            std::vector<char> data(dataSize);

            *(int*)data.data() = dataSize; // MainFrame WM_COPYDATA handler expects this
            size_t offset = 4;

            for (const auto& arg : args) {
                int size = (arg.GetLength() + 1) * sizeof(TCHAR);
                memcpy(data.data() + offset, arg, size);
                offset += size;
            }

            ASSERT(offset == (size_t)dataSize);

            COPYDATASTRUCT copyData = {
                0x6ABE51, // 1337 magic cookie
                dataSize,
                data.data()
            };

            return !!SendMessage(hWindow, WM_COPYDATA, 0, (LPARAM)&copyData);
        }

        BOOL CALLBACK EnumProc(HWND hWindow, LPARAM lParam)
        {
            CString strClass;
            VERIFY(GetClassName(hWindow, strClass.GetBuffer(256), 256));
            strClass.ReleaseBuffer();

            if (strClass == MPC_WND_CLASS_NAME) {
                ASSERT(lParam);
                *(HWND*)lParam = hWindow;
                return FALSE;
            }

            return TRUE;
        }
    }

    ExecuteCommand::ExecuteCommand(UnloadObserver* pUnloadObserver)
        : CUnknown("LocalServer::ExecuteCommand", nullptr)
        , m_pUnloadObserver(pUnloadObserver)
    {
        ASSERT(pUnloadObserver);
        m_pUnloadObserver->Increment();
    }

    ExecuteCommand::~ExecuteCommand()
    {
        m_pUnloadObserver->Decrement();
    }

    STDMETHODIMP ExecuteCommand::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        if (riid == __uuidof(IInitializeCommand)) {
            return GetInterface(static_cast<IInitializeCommand*>(this), ppv);
        }

        if (riid == __uuidof(IObjectWithSelection)) {
            return GetInterface(static_cast<IObjectWithSelection*>(this), ppv);
        }

        if (riid == __uuidof(IExecuteCommand)) {
            return GetInterface(static_cast<IExecuteCommand*>(this), ppv);
        }

        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    STDMETHODIMP ExecuteCommand::Initialize(PCWSTR commandName, IPropertyBag*)
    {
        CheckPointer(commandName, E_POINTER);

        const wchar_t* dot = wcsrchr(commandName, L'.');
        m_enqueue = (wcscmp(dot ? dot + 1 : commandName, L"enqueue") == 0);

        return S_OK;
    }

    STDMETHODIMP ExecuteCommand::SetSelection(IShellItemArray* pArray)
    {
        m_selection = pArray;
        return S_OK;
    }

    STDMETHODIMP ExecuteCommand::GetSelection(REFIID riid, void** ppv)
    {
        if (m_selection) {
            return m_selection->QueryInterface(riid, ppv);
        }

        CheckPointer(ppv, E_POINTER);
        *ppv = nullptr;

        return E_NOINTERFACE;
    }

    STDMETHODIMP ExecuteCommand::Execute()
    {
        if (!m_selection) {
            return E_UNEXPECTED;
        }

        try {

            std::deque<CString> args;
            {
                CComPtr<IEnumShellItems> items;
                if (SUCCEEDED(m_selection->EnumItems(&items))) {
                    for (CComPtr<IShellItem> item; items->Next(1, &item, nullptr) == S_OK; item.Release()) {
                        CComHeapPtr<WCHAR> file;
                        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &file))) {
                            args.emplace_back(file);
                        }
                    }
                }
            }

            if (args.empty()) {
                return E_FAIL;
            }

            if (m_enqueue) {
                args.emplace_front(_T("/add"));
            }

            bool newInstance = true;
            ATL::CMutex mutexOneInstance;
            if (mutexOneInstance.Open(SYNCHRONIZE, FALSE, MPC_WND_CLASS_NAME)) {
                DWORD res = WaitForSingleObject(mutexOneInstance, 5000);
                if (res == WAIT_OBJECT_0 || res == WAIT_ABANDONED) {
                    newInstance = CAppSettings::GetAllowMultiInst();
                }
            }

            if (!newInstance) {
                HWND hWindow = FindWindow(MPC_WND_CLASS_NAME, nullptr);
                if (hWindow) {

                    if (SendCommandLine(hWindow, args)) {
                        return S_OK;
                    }
                }
                newInstance = true;
            }

            // TODO: test 32bit app activation

            mutexOneInstance.Close();

            CString cmd = Escape(PathUtils::GetProgramPath(true));
            if (m_enqueue) {
                cmd += _T(' ') + args.front();
                args.pop_front();
            }
            cmd += _T(' ') + Escape(args.front());
            args.pop_front();

            STARTUPINFO startup = { sizeof(startup) };
            PROCESS_INFORMATION out;

            static const WinapiFunc<decltype(GetThreadId)>
            fnGetThreadId = { "Kernel32.dll", "GetThreadId" }; // Not suppported in Windows XP

            if (!fnGetThreadId) {
                return E_UNEXPECTED;
            }

            if (CreateProcess(nullptr, cmd.GetBuffer(), nullptr, nullptr, FALSE, 0,
                              nullptr, nullptr, &startup, &out)) {

                if (!args.empty()) {
                    args.emplace_front(_T("/add"));

                    DWORD exitCode;
                    while (GetExitCodeProcess(out.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                        HWND hWindow = NULL;
                        EnumThreadWindows(fnGetThreadId(out.hThread), EnumProc, (LPARAM)&hWindow);
                        if (hWindow) {
                            SendCommandLine(hWindow, args);
                            break;
                        }
                        Sleep(1);
                    }
                }

                CloseHandle(out.hThread);
                CloseHandle(out.hProcess);
            }
        } catch (...) {
            return E_FAIL;
        }

        return S_OK;
    }
}
