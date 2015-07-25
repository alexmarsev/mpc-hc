#pragma once

#include "UnloadObserver.h"

namespace LocalServer
{
    class ClassFactory final
        : public CUnknown
        , public IClassFactory
        , public UnloadObserver
    {
    public:

        ClassFactory();

        // CUnknown
        DECLARE_IUNKNOWN
        STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

        // IClassFactory
        STDMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv) override;
        STDMETHODIMP LockServer(BOOL lock) override;
    };

}
