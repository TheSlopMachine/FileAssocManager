#include "common.h"
#include "apps.h"
#include "icons.h"
#include <shobjidl.h>

/* ------------------------------------------------------------------ helpers */
static BOOL rd_v(HKEY root, REGSAM view, const wchar_t *sub, const wchar_t *val, wchar_t *out, DWORD cch)
{
    HKEY k; BOOL ok;
    out[0] = 0;
    if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return FALSE;
    ok = read_str(k, val, out, cch);
    RegCloseKey(k);
    return ok;
}

/* "@shell32.dll,-1234" style values -> display text. */
static void indirect(wchar_t *s, size_t cch)
{
    if (s[0] != L'@') return;
    wchar_t t[512];
    if (SUCCEEDED(SHLoadIndirectString(s, t, 512, NULL)) && t[0]) str_copy(s, cch, t);
    else s[0] = 0;
}

static void make_key(const wchar_t *path, wchar_t *key)
{
    str_copy(key, MAX_PATH, path);
    for (wchar_t *p = key; *p; p++) if (*p == L'/') *p = L'\\';
    str_lower(key);
}

/* One command-line token -> existing file (environment variables and PATH resolved). */
static BOOL resolve_exe(const wchar_t *tok, wchar_t *out, size_t cch)
{
    wchar_t x[1100];
    if (!tok[0]) return FALSE;
    ExpandEnvironmentStringsW(tok, x, 1100);
    if (!wcschr(x, L'\\') && !wcschr(x, L'/')) {
        wchar_t f[MAX_PATH];
        if (SearchPathW(NULL, x, L".exe", MAX_PATH, f, NULL)) { str_copy(out, cch, f); return TRUE; }
        return FALSE;
    }
    if (!file_exists(x)) return FALSE;
    str_copy(out, cch, x);
    return TRUE;
}

/* The program a command line starts: handles quotes and unquoted paths containing spaces. */
static BOOL exe_from_command(const wchar_t *cmd, wchar_t *out, size_t cch)
{
    wchar_t t[1100];
    str_copy(t, 1100, cmd);
    wchar_t *p = t;
    while (*p == L' ') p++;
    out[0] = 0;
    if (*p == L'"') {
        wchar_t *q = wcschr(++p, L'"');
        if (q) *q = 0;
        return resolve_exe(p, out, cch);
    }
    for (wchar_t *q = p;;) {
        while (*q && *q != L' ') q++;
        wchar_t save = *q;
        *q = 0;
        if (resolve_exe(p, out, cch)) return TRUE;
        *q = save;
        if (!save) break;
        q++;
    }
    return FALSE;
}

static BOOL file_description(const wchar_t *exe, wchar_t *out, size_t cch)
{
    DWORD h, sz = GetFileVersionInfoSizeW(exe, &h);
    BOOL ok = FALSE;
    out[0] = 0;
    if (!sz) return FALSE;
    void *buf = malloc(sz);
    if (!buf) return FALSE;
    if (GetFileVersionInfoW(exe, 0, sz, buf)) {
        struct { WORD lang, cp; } *tr = NULL;
        UINT n = 0;
        if (VerQueryValueW(buf, L"\\VarFileInfo\\Translation", (void **)&tr, &n) && tr && n >= 4) {
            static const wchar_t *K[2] = { L"FileDescription", L"ProductName" };
            for (int i = 0; i < 2 && !ok; i++) {
                wchar_t q[100]; wchar_t *v = NULL; UINT vn = 0;
                wf(q, 100, L"\\StringFileInfo\\%04x%04x\\%ls", tr[0].lang, tr[0].cp, K[i]);
                if (VerQueryValueW(buf, q, (void **)&v, &vn) && v && v[0]) {
                    str_copy(out, cch, v);
                    StrTrimW(out, L" \t");
                    ok = out[0] != 0;
                }
            }
        }
    }
    free(buf);
    return ok;
}

static void base_name(const wchar_t *path, wchar_t *out, size_t cch, BOOL noext)
{
    const wchar_t *b = wcsrchr(path, L'\\');
    str_copy(out, cch, b ? b + 1 : path);
    if (noext) { wchar_t *d = wcsrchr(out, L'.'); if (d) *d = 0; }
}

/* ------------------------------------------------------- growable string */
typedef struct { wchar_t *s; size_t n, cap; } Sb;

static void sb_add(Sb *b, const wchar_t *t)
{
    size_t l = wcslen(t);
    if (b->n + l + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->n + l + 1) nc *= 2;
        wchar_t *p = (wchar_t *)realloc(b->s, nc * sizeof(wchar_t));
        if (!p) return;
        b->s = p; b->cap = nc;
    }
    memcpy(b->s + b->n, t, (l + 1) * sizeof(wchar_t));
    b->n += l;
}

/* ------------------------------------------------- ProgId -> program, cached */
static BOOL prog_command(const wchar_t *progId, wchar_t *cmd, size_t cch)
{
    wchar_t p[500], verb[120], first[120];
    cmd[0] = 0;
    wf(p, 500, L"%ls\\shell", progId);
    verb[0] = 0;
    rd_str(HKEY_CLASSES_ROOT, p, NULL, verb, 120);
    wchar_t *c = wcschr(verb, L',');
    if (c) *c = 0;
    const wchar_t *try_[2] = { verb, L"open" };
    for (int i = 0; i < 2; i++) {
        if (!try_[i][0]) continue;
        wf(p, 500, L"%ls\\shell\\%ls\\command", progId, try_[i]);
        if (rd_str(HKEY_CLASSES_ROOT, p, NULL, cmd, (DWORD)cch) && cmd[0]) return TRUE;
    }
    /* first verb that has a command */
    wf(p, 500, L"%ls\\shell", progId);
    HKEY k;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, p, 0, KEY_ENUMERATE_SUB_KEYS | WOW, &k) == ERROR_SUCCESS) {
        for (DWORD i = 0;; i++) {
            DWORD n = 120;
            if (RegEnumKeyExW(k, i, first, &n, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            wf(p, 500, L"%ls\\shell\\%ls\\command", progId, first);
            if (rd_str(HKEY_CLASSES_ROOT, p, NULL, cmd, (DWORD)cch) && cmd[0]) { RegCloseKey(k); return TRUE; }
        }
        RegCloseKey(k);
    }
    /* ProgIds registered by 32-bit installers live in the 32-bit view */
    wf(p, 500, L"SOFTWARE\\Classes\\%ls\\shell\\open\\command", progId);
    return rd_v(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, p, NULL, cmd, (DWORD)cch) && cmd[0];
}

static BOOL prog_exe(const wchar_t *progId, wchar_t *exe, size_t cch)
{
    wchar_t cmd[1100];
    exe[0] = 0;
    if (!prog_command(progId, cmd, 1100)) return FALSE;
    return exe_from_command(cmd, exe, cch);
}

typedef struct { wchar_t prog[200]; wchar_t key[MAX_PATH]; } PK;
static PK **g_pk; static int g_npk, g_cpk;

const wchar_t *prog_exe_key(const wchar_t *progId)
{
    if (!progId || !progId[0]) return L"";
    for (int i = 0; i < g_npk; i++) if (!_wcsicmp(g_pk[i]->prog, progId)) return g_pk[i]->key;
    PK *p = (PK *)calloc(1, sizeof *p);
    if (!p) return L"";
    wchar_t exe[MAX_PATH];
    str_copy(p->prog, 200, progId);
    if (prog_exe(progId, exe, MAX_PATH)) make_key(exe, p->key);
    if (g_npk == g_cpk) {
        PK **n = (PK **)realloc(g_pk, (size_t)(g_cpk ? g_cpk * 2 : 256) * sizeof *n);
        if (!n) { free(p); return L""; }
        g_pk = n; g_cpk = g_cpk ? g_cpk * 2 : 256;
    }
    g_pk[g_npk++] = p;
    return p->key;
}

static ProgInfo **g_pi; static int g_npi, g_cpi;

const ProgInfo *prog_info(const wchar_t *progId)
{
    if (!progId || !progId[0]) return NULL;
    for (int i = 0; i < g_npi; i++) if (!_wcsicmp(g_pi[i]->progId, progId)) return g_pi[i];
    ProgInfo *p = (ProgInfo *)calloc(1, sizeof *p);
    if (!p) return NULL;
    str_copy(p->progId, 200, progId);
    p->icon = -1;
    p->ours = str_starts_i(progId, PROGID_PREFIX);
    prog_exe(progId, p->exe, MAX_PATH);

    wchar_t buf[512]; DWORD cch = 512;
    if (SUCCEEDED(AssocQueryStringW(ASSOCF_NOTRUNCATE | ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_FRIENDLYAPPNAME,
                                    progId, NULL, buf, &cch)) && buf[0])
        str_copy(p->name, 160, buf);
    if (!p->name[0] && p->exe[0]) file_description(p->exe, p->name, 160);
    if (!p->name[0] && rd_str(HKEY_CLASSES_ROOT, progId, NULL, buf, 512) && buf[0]) str_copy(p->name, 160, buf);
    if (!p->name[0]) str_copy(p->name, 160, progId);
    if (p->exe[0]) make_key(p->exe, p->key);
    else { str_copy(p->key, MAX_PATH, p->name); str_lower(p->key); }

    if (g_npi == g_cpi) {
        ProgInfo **n = (ProgInfo **)realloc(g_pi, (size_t)(g_cpi ? g_cpi * 2 : 256) * sizeof *n);
        if (!n) { free(p); return NULL; }
        g_pi = n; g_cpi = g_cpi ? g_cpi * 2 : 256;
    }
    g_pi[g_npi++] = p;
    return p;
}

int prog_icon(const ProgInfo *cp)
{
    ProgInfo *p = (ProgInfo *)cp;
    if (!p) return icon_generic_app();
    if (p->icon < 0) p->icon = icon_for_exe(p->exe);
    return p->icon;
}

/* ------------------------------------------------------------ app list basics */
void app_free(App *a) { free(a->decl); a->decl = NULL; }

void apps_free(AppList *l)
{
    for (int i = 0; i < l->n; i++) app_free(&l->v[i]);
    free(l->v);
    memset(l, 0, sizeof *l);
}

int apps_find(const AppList *l, const wchar_t *exe)
{
    wchar_t key[MAX_PATH];
    make_key(exe, key);
    for (int i = 0; i < l->n; i++) if (!wcscmp(l->v[i].key, key)) return i;
    return -1;
}

static wchar_t *dup_w(const wchar_t *s)
{
    size_t n = (wcslen(s) + 1) * sizeof(wchar_t);
    wchar_t *p = (wchar_t *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

App *apps_add(AppList *l, const App *a)
{
    int i = apps_find(l, a->exe);
    if (i >= 0) {
        App *d = &l->v[i];
        if (a->name[0] && a->nameQ > d->nameQ) { str_copy(d->name, 160, a->name); d->nameQ = a->nameQ; }
        if (!d->regName[0] && a->regName[0]) { str_copy(d->regName, 128, a->regName); d->regUser = a->regUser; }
        if (!d->cmd[0] && a->cmd[0]) str_copy(d->cmd, 1024, a->cmd);
        if (a->decl && a->decl[0]) {
            Sb b = { 0 };
            if (d->decl) sb_add(&b, d->decl);
            sb_add(&b, a->decl);
            free(d->decl);
            d->decl = b.s;
        }
        return d;
    }
    if (l->n == l->cap) {
        int nc = l->cap ? l->cap * 2 : 128;
        App *n = (App *)realloc(l->v, (size_t)nc * sizeof *n);
        if (!n) return NULL;
        l->v = n; l->cap = nc;
    }
    App *d = &l->v[l->n++];
    *d = *a;
    d->decl = a->decl ? dup_w(a->decl) : NULL;
    make_key(d->exe, d->key);
    d->icon = -1;
    return d;
}

int app_icon(App *a)
{
    if (a->icon < 0) a->icon = icon_for_exe(a->exe);
    return a->icon;
}

/* ------------------------------------------------------------------ discovery */
static BOOL looks_junk(const wchar_t *name, const wchar_t *exe)
{
    static const wchar_t *W[] = { L"uninstall", L"unins0", L"setup", L"installer", L"update", L"helper", L"crash",
                                  L"readme", L"license", L"website", L"release notes", L"documentation",
                                  L"manual", L"changelog", L"repair", L"redist", L"elevate", L"activation" };
    wchar_t n[260], b[260];
    str_copy(n, 260, name);
    str_lower(n);
    base_name(exe, b, 260, TRUE);
    str_lower(b);
    for (int i = 0; i < COUNTOF(W); i++) if (wcsstr(n, W[i]) || wcsstr(b, W[i])) return TRUE;
    return FALSE;
}

static BOOL in_windows_dir(const wchar_t *exe)
{
    wchar_t w[MAX_PATH];
    UINT n = GetWindowsDirectoryW(w, MAX_PATH);
    if (!n || _wcsnicmp(exe, w, n) != 0) return FALSE;
    static const wchar_t *OK[] = { L"notepad.exe", L"mspaint.exe", L"wordpad.exe", L"wmplayer.exe" };
    wchar_t b[MAX_PATH];
    base_name(exe, b, MAX_PATH, FALSE);
    for (int i = 0; i < COUNTOF(OK); i++) if (!_wcsicmp(b, OK[i])) return FALSE;
    return TRUE;
}

static void add_exe(AppList *l, const wchar_t *exe, const wchar_t *name, int q, const wchar_t *cmd,
                    const wchar_t *regName, BOOL regUser, const wchar_t *decl)
{
    App a;
    memset(&a, 0, sizeof a);
    str_copy(a.exe, MAX_PATH, exe);
    if (name) str_copy(a.name, 160, name);
    a.nameQ = a.name[0] ? q : 0;
    if (cmd) str_copy(a.cmd, 1024, cmd);
    if (regName) { str_copy(a.regName, 128, regName); a.regUser = regUser; }
    a.decl = (wchar_t *)decl;                       /* apps_add copies it */
    apps_add(l, &a);
}

static void scan_registered(AppList *l, HKEY hive, REGSAM view, BOOL user)
{
    HKEY k;
    if (RegOpenKeyExW(hive, L"SOFTWARE\\RegisteredApplications", 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0;; i++) {
        wchar_t vn[256], caps[512];
        DWORD cv = 256, cc = sizeof caps - sizeof(wchar_t), type = 0;
        LONG r = RegEnumValueW(k, i, vn, &cv, NULL, &type, (BYTE *)caps, &cc);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
        caps[cc / sizeof(wchar_t)] = 0;

        wchar_t sub[700], name[260], exe[MAX_PATH], firstProg[200];
        Sb decl = { 0 };
        exe[0] = firstProg[0] = 0;
        wf(sub, 700, L"%ls\\FileAssociations", caps);
        HKEY fk;
        int tried = 0;
        if (RegOpenKeyExW(hive, sub, 0, KEY_QUERY_VALUE | view, &fk) == ERROR_SUCCESS) {
            for (DWORD j = 0;; j++) {
                wchar_t ext[80], prog[200];
                DWORD ce = 80, cp = sizeof prog - sizeof(wchar_t), t2 = 0;
                LONG r2 = RegEnumValueW(fk, j, ext, &ce, NULL, &t2, (BYTE *)prog, &cp);
                if (r2 == ERROR_NO_MORE_ITEMS) break;
                if (r2 != ERROR_SUCCESS || t2 != REG_SZ || ext[0] != L'.') continue;
                prog[cp / sizeof(wchar_t)] = 0;
                str_lower(ext);
                wchar_t line[320];
                wf(line, 320, L"\n%ls\t%ls", ext, prog);
                sb_add(&decl, line);
                if (!exe[0] && tried < 8) { tried++; prog_exe(prog, exe, MAX_PATH); }
            }
            RegCloseKey(fk);
        }
        if (!exe[0]) { free(decl.s); continue; }
        rd_v(hive, view, caps, L"ApplicationName", name, 260);
        indirect(name, 260);
        if (!name[0]) str_copy(name, 260, vn);
        add_exe(l, exe, name, 3, NULL, vn, user, decl.s);
        free(decl.s);
    }
    RegCloseKey(k);
}

static void scan_app_paths(AppList *l, HKEY hive, REGSAM view)
{
    HKEY k;
    if (RegOpenKeyExW(hive, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths", 0, KEY_ENUMERATE_SUB_KEYS | view, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0;; i++) {
        wchar_t name[260], sub[400], path[1100], exe[MAX_PATH];
        DWORD n = 260;
        if (RegEnumKeyExW(k, i, name, &n, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        if (n < 5 || _wcsicmp(name + n - 4, L".exe") != 0) continue;
        wf(sub, 400, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%ls", name);
        if (!rd_v(hive, view, sub, NULL, path, 1100) || !path[0]) continue;
        if (!exe_from_command(path, exe, MAX_PATH)) continue;
        if (in_windows_dir(exe) || looks_junk(L"", exe)) continue;
        add_exe(l, exe, NULL, 0, NULL, NULL, FALSE, NULL);
    }
    RegCloseKey(k);
}

static void scan_applications(AppList *l)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"Applications", 0, KEY_ENUMERATE_SUB_KEYS | WOW, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0;; i++) {
        wchar_t name[260], sub[500], cmd[1100], exe[MAX_PATH], fr[260];
        DWORD n = 260;
        if (RegEnumKeyExW(k, i, name, &n, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        if (n < 5 || _wcsicmp(name + n - 4, L".exe") != 0) continue;
        wf(sub, 500, L"Applications\\%ls\\shell\\open\\command", name);
        if (!rd_str(HKEY_CLASSES_ROOT, sub, NULL, cmd, 1100) || !exe_from_command(cmd, exe, MAX_PATH)) continue;
        if (in_windows_dir(exe)) continue;
        wf(sub, 500, L"Applications\\%ls", name);
        fr[0] = 0;
        rd_str(HKEY_CLASSES_ROOT, sub, L"FriendlyAppName", fr, 260);
        indirect(fr, 260);
        if (!fr[0]) { wf(sub, 500, L"Applications\\%ls\\shell\\open", name); rd_str(HKEY_CLASSES_ROOT, sub, L"FriendlyAppName", fr, 260); indirect(fr, 260); }
        if (looks_junk(fr, exe)) continue;

        Sb decl = { 0 };
        HKEY st;
        wf(sub, 500, L"Applications\\%ls\\SupportedTypes", name);
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, sub, 0, KEY_QUERY_VALUE | WOW, &st) == ERROR_SUCCESS) {
            for (DWORD j = 0;; j++) {
                wchar_t ext[80]; DWORD ce = 80;
                if (RegEnumValueW(st, j, ext, &ce, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
                if (ext[0] != L'.') continue;
                str_lower(ext);
                wchar_t line[120];
                wf(line, 120, L"\n%ls\t", ext);
                sb_add(&decl, line);
            }
            RegCloseKey(st);
        }
        add_exe(l, exe, fr, 2, wcsstr(cmd, L"%1") ? cmd : NULL, NULL, FALSE, decl.s);
        free(decl.s);
    }
    RegCloseKey(k);
}

static void scan_dir(AppList *l, const wchar_t *dir, IShellLinkW *sl, IPersistFile *pf, int depth)
{
    wchar_t pat[MAX_PATH + 4];
    WIN32_FIND_DATAW fd;
    wf(pat, MAX_PATH + 4, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t full[MAX_PATH];
        if (fd.cFileName[0] == L'.') continue;
        wf(full, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 4) scan_dir(l, full, sl, pf, depth + 1);
            continue;
        }
        size_t n = wcslen(fd.cFileName);
        if (n < 5 || _wcsicmp(fd.cFileName + n - 4, L".lnk") != 0) continue;
        if (FAILED(IPersistFile_Load(pf, full, STGM_READ))) continue;
        wchar_t target[MAX_PATH], exe[MAX_PATH], name[260];
        target[0] = 0;
        if (FAILED(IShellLinkW_GetPath(sl, target, MAX_PATH, NULL, 0)) || !target[0]) continue;
        if (!resolve_exe(target, exe, MAX_PATH)) continue;
        size_t en = wcslen(exe);
        if (en < 5 || _wcsicmp(exe + en - 4, L".exe") != 0) continue;
        str_copy(name, 260, fd.cFileName);
        name[n - 4] = 0;
        if (in_windows_dir(exe) || looks_junk(name, exe)) continue;
        add_exe(l, exe, name, 2, NULL, NULL, FALSE, NULL);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void scan_start_menu(AppList *l)
{
    IShellLinkW *sl = NULL; IPersistFile *pf = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)) || !sl) return;
    if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf)) && pf) {
        static const int CS[2] = { CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS };
        for (int i = 0; i < 2; i++) {
            wchar_t dir[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(NULL, CS[i], NULL, SHGFP_TYPE_CURRENT, dir))) scan_dir(l, dir, sl, pf, 0);
        }
        IPersistFile_Release(pf);
    }
    IShellLinkW_Release(sl);
}

static int cmp_app(const void *a, const void *b)
{
    return _wcsicmp(((const App *)a)->name, ((const App *)b)->name);
}

void apps_discover(AppList *l)
{
    memset(l, 0, sizeof *l);
    scan_registered(l, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, FALSE);
    scan_registered(l, HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, FALSE);
    scan_registered(l, HKEY_CURRENT_USER, KEY_WOW64_64KEY, TRUE);
    scan_app_paths(l, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY);
    scan_app_paths(l, HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY);
    scan_app_paths(l, HKEY_CURRENT_USER, KEY_WOW64_64KEY);
    scan_applications(l);
    scan_start_menu(l);

    wchar_t w[MAX_PATH], p[MAX_PATH];                 /* a few built-ins worth offering */
    GetWindowsDirectoryW(w, MAX_PATH);
    static const wchar_t *B[] = { L"\\notepad.exe", L"\\system32\\mspaint.exe" };
    for (int i = 0; i < 2; i++) {
        wf(p, MAX_PATH, L"%ls%ls", w, B[i]);
        if (file_exists(p)) add_exe(l, p, NULL, 0, NULL, NULL, FALSE, NULL);
    }

    int out = 0;
    for (int i = 0; i < l->n; i++) {                  /* names for entries that have none */
        App *a = &l->v[i];
        if (!a->name[0]) {
            file_description(a->exe, a->name, 160);
            if (!a->name[0]) base_name(a->exe, a->name, 160, TRUE);
            a->nameQ = 1;
        }
        if (a->name[0]) l->v[out++] = *a; else app_free(a);
    }
    l->n = out;
    qsort(l->v, (size_t)l->n, sizeof(App), cmp_app);
}

BOOL app_from_exe(const wchar_t *path, App *out)
{
    wchar_t exe[MAX_PATH], t[MAX_PATH];
    memset(out, 0, sizeof *out);
    out->icon = -1;
    str_copy(exe, MAX_PATH, path);
    size_t n = wcslen(exe);
    if (n > 4 && _wcsicmp(exe + n - 4, L".lnk") == 0) {                 /* follow a shortcut to its program */
        IShellLinkW *sl = NULL; IPersistFile *pf = NULL;
        BOOL ok = FALSE;
        if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)) && sl) {
            if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf)) && pf) {
                if (SUCCEEDED(IPersistFile_Load(pf, exe, STGM_READ))) {
                    IShellLinkW_Resolve(sl, NULL, SLR_NO_UI | SLR_NOSEARCH | SLR_NOUPDATE | SLR_NOTRACK);
                    t[0] = 0;
                    ok = SUCCEEDED(IShellLinkW_GetPath(sl, t, MAX_PATH, NULL, 0)) && t[0];
                }
                IPersistFile_Release(pf);
            }
            IShellLinkW_Release(sl);
        }
        if (!ok) return FALSE;
        str_copy(exe, MAX_PATH, t);
        n = wcslen(exe);
    }
    if (n < 5 || _wcsicmp(exe + n - 4, L".exe") != 0 || !file_exists(exe)) return FALSE;
    str_copy(out->exe, MAX_PATH, exe);
    make_key(exe, out->key);

    wchar_t b[260], sub[500], cmd[1100];
    base_name(exe, b, 260, FALSE);
    wf(sub, 500, L"Applications\\%ls", b);
    rd_str(HKEY_CLASSES_ROOT, sub, L"FriendlyAppName", out->name, 160);
    indirect(out->name, 160);
    wf(sub, 500, L"Applications\\%ls\\shell\\open\\command", b);
    if (rd_str(HKEY_CLASSES_ROOT, sub, NULL, cmd, 1100) && wcsstr(cmd, L"%1")) {
        wchar_t cexe[MAX_PATH], ck[MAX_PATH];
        if (exe_from_command(cmd, cexe, MAX_PATH)) { make_key(cexe, ck); if (!wcscmp(ck, out->key)) str_copy(out->cmd, 1024, cmd); }
    }
    if (!out->name[0]) file_description(exe, out->name, 160);
    if (!out->name[0]) base_name(exe, out->name, 160, TRUE);
    out->nameQ = 2;
    return TRUE;
}

/* ------------------------------------------------------ who handles an extension */
static void keys_add(wchar_t *keys, size_t cch, const wchar_t *key)
{
    wchar_t needle[MAX_PATH + 3];
    if (!key[0]) return;
    wf(needle, MAX_PATH + 3, L"\n%ls\n", key);
    if (wcsstr(keys, needle)) return;
    size_t len = wcslen(keys), add = wcslen(key) + 1;
    if (len + add + 1 >= cch) return;
    wcscpy(keys + len, key);
    wcscat(keys, L"\n");
}

/* Calls fn(ProgId) for every ProgId registered as a handler of `ext`; stops when fn returns TRUE. */
typedef BOOL (*ProgFn)(const wchar_t *prog, void *ctx);

static BOOL each_progid(const wchar_t *ext, ProgFn fn, void *ctx)
{
    wchar_t p[400], v[260];
    static const struct { HKEY root; const wchar_t *fmt; } SRC[2] = {
        { HKEY_CLASSES_ROOT, L"%ls\\OpenWithProgids" },
        { HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%ls\\OpenWithProgids" },
    };
    for (int s = 0; s < 2; s++) {
        HKEY k;
        wf(p, 400, SRC[s].fmt, ext);
        if (RegOpenKeyExW(SRC[s].root, p, 0, KEY_QUERY_VALUE | WOW, &k) != ERROR_SUCCESS) continue;
        for (DWORD i = 0;; i++) {
            DWORD cv = 260;
            LONG r = RegEnumValueW(k, i, v, &cv, NULL, NULL, NULL, NULL);
            if (r == ERROR_NO_MORE_ITEMS) break;
            if (r != ERROR_SUCCESS || !v[0]) continue;
            if (fn(v, ctx)) { RegCloseKey(k); return TRUE; }
        }
        RegCloseKey(k);
    }
    if (rd_str(HKEY_CLASSES_ROOT, ext, NULL, v, 260) && v[0] && fn(v, ctx)) return TRUE;   /* system default */
    return FALSE;
}

static BOOL collect_cb(const wchar_t *prog, void *ctx)
{
    void **c = (void **)ctx;
    keys_add((wchar_t *)c[0], *(size_t *)c[1], prog_exe_key(prog));
    return FALSE;
}

void ext_handlers(const AppList *l, const wchar_t *ext, wchar_t *keys, size_t cch)
{
    size_t cap = cch;
    void *ctx[2] = { keys, &cap };
    wcscpy(keys, L"\n");
    each_progid(ext, collect_cb, ctx);

    wchar_t needle[100];                                          /* programs that declare the type */
    wf(needle, 100, L"\n%ls\t", ext);
    for (int i = 0; i < l->n; i++)
        if (l->v[i].decl && wcsstr(l->v[i].decl, needle)) keys_add(keys, cch, l->v[i].key);
}

BOOL ext_handled_by(const wchar_t *keys, const App *a)
{
    wchar_t needle[MAX_PATH + 3];
    wf(needle, MAX_PATH + 3, L"\n%ls\n", a->key);
    return wcsstr(keys, needle) != NULL;
}

/* ------------------------------------------------ choosing / creating the ProgId */
typedef struct { const App *a; wchar_t *out; size_t cch; } FindCtx;

static BOOL find_cb(const wchar_t *prog, void *ctx)
{
    FindCtx *c = (FindCtx *)ctx;
    if (str_starts_i(prog, L"AppX")) return FALSE;
    if (wcscmp(prog_exe_key(prog), c->a->key) != 0) return FALSE;
    str_copy(c->out, c->cch, prog);
    return TRUE;
}

/* ProgId declared by the program's own Capabilities for this extension ("" if none). */
static BOOL decl_progid(const App *a, const wchar_t *ext, wchar_t *prog, size_t cch, BOOL *declaredOnly)
{
    wchar_t needle[100];
    prog[0] = 0;
    *declaredOnly = FALSE;
    if (!a->decl) return FALSE;
    wf(needle, 100, L"\n%ls\t", ext);
    const wchar_t *p = wcsstr(a->decl, needle);
    if (!p) return FALSE;
    p += wcslen(needle);
    size_t n = wcscspn(p, L"\n");
    if (n == 0) { *declaredOnly = TRUE; return FALSE; }
    if (n >= cch) return FALSE;
    wcsncpy(prog, p, n);
    prog[n] = 0;
    return key_exists(HKEY_CLASSES_ROOT, prog);
}

static unsigned fnv(const wchar_t *s)
{
    unsigned h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned)*s; h *= 16777619u; }
    return h;
}

/* One ProgId per program: FileAssocManager.<name>.<hash of its path>, in the user's classes. */
static LONG ensure_custom(const App *a, wchar_t *prog, size_t cch)
{
    static wchar_t made[300];
    wchar_t slug[40], base[MAX_PATH], root[300], cmd[1100], icon[MAX_PATH + 8];
    int o = 0;
    base_name(a->exe, base, MAX_PATH, TRUE);
    for (const wchar_t *p = base; *p && o < 24; p++) if (iswalnum(*p) && *p < 128) slug[o++] = *p;
    slug[o] = 0;
    wf(prog, cch, PROGID_PREFIX L"%ls.%08x", slug[0] ? slug : L"app", fnv(a->key));
    if (!wcscmp(made, prog)) return ERROR_SUCCESS;

    wf(root, 300, L"Software\\Classes\\%ls", prog);
    wf(cmd, 1100, wcsstr(a->cmd, L"%1") ? L"%ls" : L"\"%ls\" \"%%1\"", wcsstr(a->cmd, L"%1") ? a->cmd : a->exe);
    wf(icon, MAX_PATH + 8, L"\"%ls\",0", a->exe);

    struct { const wchar_t *sub; const wchar_t *name; const wchar_t *val; } W[4];
    wchar_t k0[320], k1[340], k2[340];
    wf(k0, 320, L"%ls", root);
    wf(k1, 340, L"%ls\\shell\\open", root);
    wf(k2, 340, L"%ls\\shell\\open\\command", root);
    wchar_t k3[340];
    wf(k3, 340, L"%ls\\DefaultIcon", root);
    W[0].sub = k0; W[0].name = NULL;              W[0].val = a->name;
    W[1].sub = k1; W[1].name = L"FriendlyAppName"; W[1].val = a->name;
    W[2].sub = k2; W[2].name = NULL;              W[2].val = cmd;
    W[3].sub = k3; W[3].name = NULL;              W[3].val = icon;
    for (int i = 0; i < 4; i++) {
        HKEY k;
        LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, W[i].sub, 0, NULL, 0, KEY_SET_VALUE | WOW, NULL, &k, NULL);
        if (r != ERROR_SUCCESS) return r;
        r = set_str(k, W[i].name, W[i].val);
        RegCloseKey(k);
        if (r != ERROR_SUCCESS) return r;
    }
    str_copy(made, 300, prog);
    return ERROR_SUCCESS;
}

static LONG register_for_ext(const wchar_t *ext, const wchar_t *prog)
{
    wchar_t p[400];
    HKEY k;
    wf(p, 400, L"Software\\Classes\\%ls\\OpenWithProgids", ext);
    LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, p, 0, NULL, 0, KEY_SET_VALUE | WOW, NULL, &k, NULL);
    if (r != ERROR_SUCCESS) return r;
    r = RegSetValueExW(k, prog, 0, REG_NONE, NULL, 0);
    RegCloseKey(k);
    return r;
}

LONG app_progid_for(const App *a, const wchar_t *ext, wchar_t *prog, size_t cch)
{
    BOOL declOnly;
    if (!a->exe[0]) return FAM_E_NOHANDLER;
    if (decl_progid(a, ext, prog, cch, &declOnly)) return ERROR_SUCCESS;      /* its own Capabilities */

    FindCtx c = { a, prog, cch };
    if (each_progid(ext, find_cb, &c)) return ERROR_SUCCESS;                  /* its own ProgId for the type */

    if (declOnly) {                                                           /* "Open with" registration */
        wchar_t b[260], sub[400];
        base_name(a->exe, b, 260, FALSE);
        wf(sub, 400, L"Applications\\%ls\\shell\\open\\command", b);
        if (key_exists(HKEY_CLASSES_ROOT, sub)) {
            wf(prog, cch, L"Applications\\%ls", b);
            return ERROR_SUCCESS;
        }
    }
    LONG r = ensure_custom(a, prog, cch);                                     /* create our own */
    if (r == ERROR_SUCCESS) r = register_for_ext(ext, prog);
    return r;
}
