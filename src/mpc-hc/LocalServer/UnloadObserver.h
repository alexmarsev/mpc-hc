#pragma once

namespace LocalServer
{
    class UnloadObserver
    {
    public:

        void Increment() {
            m_count++;
        }

        void Decrement() {
            m_count--;
            if (m_count == 0) {
                AfxPostQuitMessage(0);
            }
        }

    private:

        int m_count = 0;
    };
}
