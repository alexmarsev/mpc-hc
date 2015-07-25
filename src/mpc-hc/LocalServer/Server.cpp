#include "stdafx.h"
#include "Server.h"

#include "ClassFactory.h"
#include "ExecuteCommand.h"

namespace LocalServer
{
    bool Server::Register()
    {
        auto pFactory = new ClassFactory();

        pFactory->AddRef();

        m_registered = SUCCEEDED(CoRegisterClassObject(__uuidof(ExecuteCommand), pFactory,
                                                       CLSCTX_LOCAL_SERVER, REGCLS_SINGLEUSE, &m_id));
        pFactory->Release();

        return m_registered;
    }

    void Server::Unregister()
    {
        if (m_registered) {
            CoRevokeClassObject(m_id);
            m_registered = false;
        }
    }
}
