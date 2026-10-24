#include "icons.h"

HIMAGELIST sys_imagelist(void)
{
    static HIMAGELIST il;
    if (!il) {
        SHFILEINFOW fi;
        memset(&fi, 0, sizeof fi);
        il = (HIMAGELIST)SHGetFileInfoW(L"C:\\", FILE_ATTRIBUTE_DIRECTORY, &fi, sizeof fi,
                                        SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    }
    return il;
}

static int index_of(const wchar_t *name, DWORD attr, UINT flags)
{
    SHFILEINFOW fi;
    memset(&fi, 0, sizeof fi);
    if (!SHGetFileInfoW(name, attr, &fi, sizeof fi, SHGFI_SYSICONINDEX | SHGFI_SMALLICON | flags)) return -1;
    return fi.iIcon;
}

int icon_for_ext(const wchar_t *ext)
{
    wchar_t n[64];
    wf(n, 64, L"file%ls", ext);
    int i = index_of(n, FILE_ATTRIBUTE_NORMAL, SHGFI_USEFILEATTRIBUTES);
    return i < 0 ? 0 : i;
}

int icon_generic_app(void)
{
    static int g = -2;
    if (g == -2) { g = index_of(L"app.exe", FILE_ATTRIBUTE_NORMAL, SHGFI_USEFILEATTRIBUTES); if (g < 0) g = 0; }
    return g;
}

int icon_for_exe(const wchar_t *exe)
{
    if (exe && exe[0] && file_exists(exe)) {
        int i = index_of(exe, 0, 0);
        if (i >= 0) return i;
    }
    return icon_generic_app();
}
