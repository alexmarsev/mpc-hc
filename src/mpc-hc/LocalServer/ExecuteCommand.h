#pragma once

#include "UnloadObserver.h"

namespace LocalServer
{
    class __declspec(uuid("4A862C12-6F76-4532-9C78-0A60A2A4CD5C")) ExecuteCommand final
        : public CUnknown
        , public IInitializeCommand
        , public IObjectWithSelection
        , public IExecuteCommand
    {
    public:

        ExecuteCommand(UnloadObserver* pUnloadObserver);
        ~ExecuteCommand();

        // CUnknown
        DECLARE_IUNKNOWN
        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        // IInitializeCommand
        STDMETHODIMP Initialize(PCWSTR commandName, IPropertyBag*) override;

        // IObjectWithSelection
        STDMETHODIMP SetSelection(IShellItemArray* pArray);
        STDMETHODIMP GetSelection(REFIID riid, void** ppv);

        // IExecuteCommand
        STDMETHODIMP SetKeyState(DWORD keystate)       { return S_OK; }
        STDMETHODIMP SetParameters(LPCWSTR parameters) { return S_OK; }
        STDMETHODIMP SetPosition(POINT point)          { return S_OK; }
        STDMETHODIMP SetShowWindow(int showFlags)      { return S_OK; }
        STDMETHODIMP SetNoShowUI(BOOL noShow)          { return S_OK; }
        STDMETHODIMP SetDirectory(LPCWSTR directory)   { return S_OK; }
        STDMETHODIMP Execute();

    private:

        UnloadObserver* m_pUnloadObserver;
        CComPtr<IShellItemArray> m_selection;
        bool m_enqueue = false;
    };
}
