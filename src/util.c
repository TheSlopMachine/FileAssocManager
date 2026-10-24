#include "common.h"

HINSTANCE g_inst;
HFONT     g_font, g_fontBold;
int       g_dpi = 96;

void wf(wchar_t *buf, size_t n, const wchar_t *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, n, fmt, ap);
    va_end(ap);
    buf[n - 1] = 0;
}

void win_error_text(LONG code, wchar_t *out, size_t n)
{
    switch (code) {
    case FAM_E_CANT_WRITE: str_copy(out, n, L"Windows' default-app protection on this PC can't be satisfied safely."); return;
    case FAM_E_REJECTED:   str_copy(out, n, L"Windows did not accept the change."); return;
    case FAM_E_NOHANDLER:  str_copy(out, n, L"The application can't be registered for this file type."); return;
    case FAM_E_BLOCKED:    str_copy(out, n, L"Programs and shortcuts are never reassigned."); return;
    case FAM_E_TOOLONG:    str_copy(out, n, L"The identifier is too long."); return;
    }
    out[0] = 0;
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, (DWORD)code, 0, out, (DWORD)n, NULL);
    for (size_t i = wcslen(out); i > 0 && (out[i - 1] == L'\r' || out[i - 1] == L'\n' || out[i - 1] == L' '); i--) out[i - 1] = 0;
    if (!out[0]) wf(out, n, L"Error %ld", code);
}

void err_box(HWND owner, const wchar_t *what, LONG code)
{
    wchar_t sys[512], full[1200];
    win_error_text(code, sys, 512);
    wf(full, 1200, L"%ls\n\n%ls (error %ld)%ls", what, sys, code,
       code == ERROR_ACCESS_DENIED
         ? L"\n\nWindows protects a few file types (such as .pdf and web links); those can only be changed in Windows Settings."
         : L"");
    MessageBoxW(owner, full, APP_TITLE, MB_ICONERROR);
}

BOOL is_elevated(void)
{
    HANDLE t; TOKEN_ELEVATION e; DWORD n; BOOL r = FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) {
        if (GetTokenInformation(t, TokenElevation, &e, sizeof e, &n)) r = e.TokenIsElevated;
        CloseHandle(t);
    }
    return r;
}

/* TRUE if the string contains %NAME% (two or more name characters). */
BOOL has_envvar(const wchar_t *s)
{
    for (const wchar_t *p = wcschr(s, L'%'); p; p = wcschr(p + 1, L'%')) {
        const wchar_t *q = p + 1; int n = 0;
        while (iswalnum(*q) || *q == L'_') { q++; n++; }
        if (n >= 2 && *q == L'%') return TRUE;
    }
    return FALSE;
}

BOOL browse_file(HWND owner, wchar_t *path, const wchar_t *filter, BOOL save, const wchar_t *defext)
{
    OPENFILENAMEW o = { 0 };
    o.lStructSize = sizeof o;
    o.hwndOwner = owner; o.lpstrFile = path; o.nMaxFile = MAX_PATH;
    o.lpstrFilter = filter; o.lpstrDefExt = defext;
    o.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o);
}

BOOL clipboard_set(HWND owner, const wchar_t *text)
{
    size_t cb = (wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, cb);
    if (!g) return FALSE;
    void *p = GlobalLock(g);
    if (!p) { GlobalFree(g); return FALSE; }
    memcpy(p, text, cb);
    GlobalUnlock(g);
    if (!OpenClipboard(owner)) { GlobalFree(g); return FALSE; }
    EmptyClipboard();
    BOOL ok = SetClipboardData(CF_UNICODETEXT, g) != NULL;
    if (!ok) GlobalFree(g);
    CloseClipboard();
    return ok;
}

BOOL CALLBACK font_cb(HWND h, LPARAM font)
{
    SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE);
    return TRUE;
}

HWND ctl(HWND parent, DWORD ex, const wchar_t *cls, const wchar_t *text, DWORD style,
         int x, int y, int w, int h, int id)
{
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                           parent, (HMENU)(INT_PTR)id, g_inst, NULL);
}

/* ---------------------------------------------------------------- strings */
void str_lower(wchar_t *s)
{
    for (; *s; s++) if (*s >= L'A' && *s <= L'Z') *s = (wchar_t)(*s + 32);
}

BOOL str_ieq(const wchar_t *a, const wchar_t *b) { return _wcsicmp(a, b) == 0; }

BOOL str_starts_i(const wchar_t *s, const wchar_t *prefix) { return _wcsnicmp(s, prefix, wcslen(prefix)) == 0; }

void str_copy(wchar_t *dst, size_t n, const wchar_t *src)
{
    if (!n) return;
    wcsncpy(dst, src ? src : L"", n - 1);
    dst[n - 1] = 0;
}

/* Accepts "jpg", ".JPG", "*.jpg". Returns "" for anything that can't be an extension. */
void ext_normalize(const wchar_t *in, wchar_t *out, size_t n)
{
    wchar_t t[80];
    str_copy(t, 80, in);
    StrTrimW(t, L" \t\r\n*;,\"'");
    out[0] = 0;
    if (!t[0] || wcslen(t) > 32) return;
    for (const wchar_t *p = t + (t[0] == L'.' ? 1 : 0); *p; p++)
        if (*p <= L' ' || wcschr(L"\\/:*?\"<>|.%", *p)) return;
    if (t[0] == L'.' && !t[1]) return;
    wf(out, n, L"%ls%ls", t[0] == L'.' ? L"" : L".", t);
    str_lower(out);
}

/* --------------------------------------------------------------- registry */
/* Reads a REG_SZ or REG_EXPAND_SZ value into out (expanding %VARS% in the latter).
 * Uses RegQueryValueExW and checks the type itself: RegGetValueW with
 * RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ fails with ERROR_INVALID_PARAMETER (87) on
 * current Windows and Wine, which silently emptied every string this program read. */
BOOL read_str(HKEY k, const wchar_t *val, wchar_t *out, DWORD cch)
{
    DWORD type = 0, cb = cch * sizeof(wchar_t);
    out[0] = 0;
    if (!cch || !k) return FALSE;
    if (RegQueryValueExW(k, val, NULL, &type, (BYTE *)out, &cb) != ERROR_SUCCESS) { out[0] = 0; return FALSE; }
    if (type != REG_SZ && type != REG_EXPAND_SZ) { out[0] = 0; return FALSE; }
    out[cch - 1] = 0;
    if (type == REG_EXPAND_SZ) {
        wchar_t x[2048];
        if (ExpandEnvironmentStringsW(out, x, 2048)) str_copy(out, cch, x);
    }
    return TRUE;
}

BOOL rd_str(HKEY root, const wchar_t *sub, const wchar_t *val, wchar_t *out, DWORD cch)
{
    HKEY k = root; BOOL ok, close = FALSE;
    out[0] = 0;
    if (sub) {
        if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | WOW, &k) != ERROR_SUCCESS) return FALSE;
        close = TRUE;
    }
    ok = read_str(k, val, out, cch);
    if (close) RegCloseKey(k);
    return ok;
}

LONG set_str(HKEY k, const wchar_t *name, const wchar_t *v)
{
    return RegSetValueExW(k, name, 0, has_envvar(v) ? REG_EXPAND_SZ : REG_SZ,
                          (const BYTE *)v, (DWORD)((wcslen(v) + 1) * sizeof(wchar_t)));
}

BOOL key_exists(HKEY root, const wchar_t *sub)
{
    HKEY k;
    if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | WOW, &k) != ERROR_SUCCESS) return FALSE;
    RegCloseKey(k);
    return TRUE;
}

ULONGLONG key_write_time(HKEY root, const wchar_t *sub)
{
    HKEY k; FILETIME ft; ULONGLONG r = 0;
    if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | WOW, &k) != ERROR_SUCCESS) return 0;
    if (RegQueryInfoKeyW(k, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &ft) == ERROR_SUCCESS)
        r = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    RegCloseKey(k);
    return r;
}

/* ------------------------------------------------------------------ files */
BOOL file_exists(const wchar_t *path)
{
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL read_text_file(const wchar_t *path, wchar_t **out)
{
    *out = NULL;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(f, &sz) || sz.QuadPart > 32 * 1024 * 1024) { CloseHandle(f); return FALSE; }
    DWORD n = (DWORD)sz.QuadPart, got = 0;
    BYTE *b = (BYTE *)malloc(n + 4);
    if (!b) { CloseHandle(f); return FALSE; }
    BOOL ok = ReadFile(f, b, n, &got, NULL);
    CloseHandle(f);
    if (!ok) { free(b); return FALSE; }
    b[got] = b[got + 1] = b[got + 2] = b[got + 3] = 0;
    wchar_t *w;
    if (got >= 2 && b[0] == 0xFF && b[1] == 0xFE) {                  /* UTF-16 LE */
        w = (wchar_t *)malloc(got + 4);
        if (w) { memcpy(w, b + 2, got - 2); w[(got - 2) / 2] = 0; }
    } else {
        const char *u = (const char *)b;
        if (got >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) u += 3;
        int cch = MultiByteToWideChar(CP_UTF8, 0, u, -1, NULL, 0);
        w = (wchar_t *)malloc((size_t)(cch > 0 ? cch : 1) * sizeof(wchar_t));
        if (w) { if (cch > 0) MultiByteToWideChar(CP_UTF8, 0, u, -1, w, cch); else w[0] = 0; }
    }
    free(b);
    *out = w;
    return w != NULL;
}

BOOL write_text_file(const wchar_t *path, const wchar_t *text, BOOL bom)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return FALSE;
    char *u = (char *)malloc((size_t)n);
    if (!u) return FALSE;
    WideCharToMultiByte(CP_UTF8, 0, text, -1, u, n, NULL, NULL);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL ok = FALSE;
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w;
        static const BYTE BOM[3] = { 0xEF, 0xBB, 0xBF };
        ok = (!bom || WriteFile(f, BOM, 3, &w, NULL)) && WriteFile(f, u, (DWORD)(n - 1), &w, NULL);
        CloseHandle(f);
    }
    free(u);
    return ok;
}

void data_dir(const wchar_t *sub, wchar_t *out, size_t n)
{
    wchar_t base[MAX_PATH];
    if (!GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH)) GetTempPathW(MAX_PATH, base);
    wf(out, n, L"%ls\\%ls", base, APP_ID);
    CreateDirectoryW(out, NULL);
    if (sub && sub[0]) {
        wf(out, n, L"%ls\\%ls\\%ls", base, APP_ID, sub);
        CreateDirectoryW(out, NULL);
    }
}

/* ---------------------------------------------------------------- version */
void win_version(DWORD *major, DWORD *build)
{
    typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOW *);
    OSVERSIONINFOW v;
    memset(&v, 0, sizeof v);
    v.dwOSVersionInfoSize = sizeof v;
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn fn = nt ? (RtlGetVersionFn)(void *)GetProcAddress(nt, "RtlGetVersion") : NULL;
    if (fn && fn(&v) == 0) { *major = v.dwMajorVersion; *build = v.dwBuildNumber; }
    else { *major = 6; *build = 0; }
}
