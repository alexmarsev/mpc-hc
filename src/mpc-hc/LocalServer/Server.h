#pragma once

namespace LocalServer
{
    class Server final
    {
    public:

        Server() = default;
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        bool Register();
        void Unregister();

    private:

        bool m_registered = false;
        DWORD m_id = 0;
    };
}
