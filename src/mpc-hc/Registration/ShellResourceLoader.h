#pragma once

namespace Registration
{
    class ShellResourceLoader final
    {
    public:

        ShellResourceLoader();
        ShellResourceLoader(const ShellResourceLoader&) = delete;
        ShellResourceLoader& operator=(const ShellResourceLoader&) = delete;
        ~ShellResourceLoader();

        CString GetString(int id, LANGID lang);
        HICON GetIcon(int id, int w, int h);

    private:

        const CString m_resFile;
        HMODULE m_resLibrary = NULL;
    };
}
