#include "stdafx.h"
#include "ClassFactory.h"

#include "ExecuteCommand.h"

namespace LocalServer
{
    ClassFactory::ClassFactory()
        : CUnknown("LocalServer::ClassFactory", nullptr)
    {
    }

    STDMETHODIMP ClassFactory::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        if (riid == __uuidof(IClassFactory)) {
            return GetInterface(static_cast<IClassFactory*>(this), ppv);
        }

        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv)
    {
        CheckPointer(ppv, E_POINTER);
        *ppv = nullptr;

        if (pOuter) {
            return CLASS_E_NOAGGREGATION;
        }

        auto pInstance = new(std::nothrow) ExecuteCommand(this);
        CheckPointer(pInstance, E_OUTOFMEMORY);

        pInstance->NonDelegatingAddRef();
        HRESULT hr = pInstance->NonDelegatingQueryInterface(riid, ppv);
        pInstance->NonDelegatingRelease();

        return hr;
    }

    STDMETHODIMP ClassFactory::LockServer(BOOL lock)
    {
        lock ? UnloadObserver::Increment() : UnloadObserver::Decrement();

        return S_OK;
    }
}
