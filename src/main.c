/* File Association Manager: main window.
 *
 * Left: categories and the applications currently in use. Right: the application to assign
 * (card on top), a searchable / filterable list of formats with tick boxes, and details.
 * Tick formats (or a whole category with Ctrl+A), choose an application, press Assign. */
#include "common.h"
#include "resource.h"
#include "registry.h"
#include "apps.h"
#include "backup.h"
#include "dialog.h"
#include "icons.h"

enum {
    IDC_SIDE = 100, IDC_APPICON, IDC_APPNAME, IDC_APPPATH, IDC_CHOOSE, IDC_ASSIGN, IDC_RULE, IDC_FILTER,
    IDC_SHOW, IDC_CHECKALL, IDC_CHECKNONE, IDC_LIST, IDC_DET1, IDC_DET2, IDC_RESET, IDC_UNDO, IDC_STATUS
};
enum {
    IDM_ADD = 200, IDM_EXPORT, IDM_EXPORT_XML, IDM_RESTORE, IDM_BACKUPS, IDM_EXIT,
    IDM_CHOOSE, IDM_ASSIGN, IDM_UNDO, IDM_CHECKALL, IDM_CHECKNONE, IDM_CHECKSUPPORTED, IDM_CHECKSEL,
    IDM_UNCHECKSEL, IDM_RESET, IDM_REFRESH, IDM_FIND, IDM_SETTINGS, IDM_SELFTEST, IDM_REPORT, IDM_NOTIFY,
    IDM_ABOUT, IDM_COPYPROG, IDM_REGEDIT, IDM_TYPESETTINGS,
    IDM_RECENT0 = 400
};
#define MAX_RECENT   6
#define WM_APP_LOAD  (WM_APP + 1)
#define WM_APP_APPS  (WM_APP + 2)
#define TIMER_FILTER 1
#define SETTINGS_KEY L"Software\\" APP_ID

enum { SK_ALL, SK_CAT, SK_APP, SK_NONE };                      /* sidebar entry kinds */
enum { SM_ALL, SM_NOTAPP, SM_APP, SM_MINE, SM_NONE };           /* "Show" filter */

typedef struct { int kind, cat; wchar_t key[MAX_PATH]; } SideItem;

static HWND g_main, g_side, g_appicon, g_appname, g_apppath, g_choose, g_assign, g_rule, g_filter, g_show;
static HWND g_checkall, g_checknone, g_list, g_det1, g_det2, g_reset, g_undo, g_status;
static HMENU g_menu;
static void show_env(void);
static UcEnv g_env;
static FmtList g_fl;
static AppList g_apps;
static BOOL g_appsLoaded, g_loading;
static wchar_t g_appExe[MAX_PATH];
static int *g_view, g_nview;
static SideItem *g_si;
static int g_nsi;
static int g_sideKind = SK_ALL, g_sideCat, g_sortcol = 2, g_sortdesc, g_showMode = SM_ALL;
static wchar_t g_sideKey[MAX_PATH];
static int g_showModes[5], g_nshow;

typedef struct {
    DWORD version; RECT rc; DWORD maximized;
    int sideKind, sideCat, sortcol, sortdesc, showMode;
    wchar_t lastApp[MAX_PATH];
    wchar_t recent[MAX_RECENT][MAX_PATH];
} Settings;
static Settings g_cfg;

static const struct { const wchar_t *name; int w; } COLS[] = {
    { L"Format", 86 }, { L"Description", 230 }, { L"Category", 120 }, { L"Default app", 210 }, { L"Status", 130 },
};
#define NCOLS ((int)(sizeof COLS / sizeof COLS[0]))

/* ----------------------------------------------------------------- settings */
static BOOL settings_load(void)
{
    Settings t; DWORD cb = sizeof t;
    memset(&g_cfg, 0, sizeof g_cfg);
    if (RegGetValueW(HKEY_CURRENT_USER, SETTINGS_KEY, L"Settings", RRF_RT_REG_BINARY, NULL, &t, &cb) == ERROR_SUCCESS
        && cb == sizeof t && t.version == 1 && MonitorFromRect(&t.rc, MONITOR_DEFAULTTONULL) != NULL) {
        g_cfg = t;
        return TRUE;
    }
    return FALSE;
}

static void settings_save(void)
{
    WINDOWPLACEMENT wp = { 0 }; HKEY k;
    wp.length = sizeof wp;
    GetWindowPlacement(g_main, &wp);
    g_cfg.version = 1;
    g_cfg.rc = wp.rcNormalPosition;
    g_cfg.maximized = wp.showCmd == SW_SHOWMAXIMIZED;
    g_cfg.sideKind = g_sideKind; g_cfg.sideCat = g_sideCat;
    g_cfg.sortcol = g_sortcol; g_cfg.sortdesc = g_sortdesc; g_cfg.showMode = g_showMode;
    str_copy(g_cfg.lastApp, MAX_PATH, g_appExe);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, SETTINGS_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, L"Settings", 0, REG_BINARY, (const BYTE *)&g_cfg, sizeof g_cfg);
        RegCloseKey(k);
    }
}

static void mru_push(const wchar_t *exe)
{
    int at = MAX_RECENT - 1;
    for (int i = 0; i < MAX_RECENT; i++) if (!_wcsicmp(g_cfg.recent[i], exe)) { at = i; break; }
    for (int i = at; i > 0; i--) wcscpy(g_cfg.recent[i], g_cfg.recent[i - 1]);
    str_copy(g_cfg.recent[0], MAX_PATH, exe);
}

/* ------------------------------------------------------------------ helpers */
static const wchar_t *plural(int n) { return n == 1 ? L"" : L"s"; }
static void set_status(const wchar_t *s) { SendMessageW(g_status, SB_SETTEXTW, 0, (LPARAM)s); UpdateWindow(g_status); }
static Fmt *view_fmt(int row) { return (row >= 0 && row < g_nview) ? &g_fl.v[g_view[row]] : NULL; }
static int  fmt_icon(Fmt *f) { if (f->icon < 0) f->icon = icon_for_ext(f->ext); return f->icon; }

static App *cur_app(void)
{
    if (!g_appExe[0]) return NULL;
    int i = apps_find(&g_apps, g_appExe);
    return i >= 0 ? &g_apps.v[i] : NULL;
}

static int count_marked(void)
{
    int n = 0;
    for (int i = 0; i < g_fl.n; i++) n += g_fl.v[i].marked;
    return n;
}

/* The formats an action applies to: everything ticked, or if nothing is, the selected rows. */
static int collect_targets(int **out, BOOL *fromMarks)
{
    int n = 0;
    *out = (int *)malloc((size_t)(g_fl.n ? g_fl.n : 1) * sizeof(int));
    if (!*out) return 0;
    for (int i = 0; i < g_fl.n; i++) if (g_fl.v[i].marked) (*out)[n++] = i;
    if (fromMarks) *fromMarks = n > 0;
    if (n) return n;
    for (int r = -1; (r = ListView_GetNextItem(g_list, r, LVNI_SELECTED)) >= 0;) (*out)[n++] = g_view[r];
    return n;
}

static int collect_selected(int **out)
{
    int n = 0;
    *out = (int *)malloc((size_t)(g_nview ? g_nview : 1) * sizeof(int));
    if (!*out) return 0;
    for (int r = -1; (r = ListView_GetNextItem(g_list, r, LVNI_SELECTED)) >= 0;) (*out)[n++] = g_view[r];
    return n;
}

static void ensure_apps(void)
{
    if (g_appsLoaded) return;
    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    set_status(L"Looking for installed applications" U_ELLIPSIS);
    apps_discover(&g_apps);
    g_appsLoaded = TRUE;
    SetCursor(old);
}

static UINT dpi_of(HWND h)
{
    typedef UINT (WINAPI *Fn)(HWND);
    static Fn fn; static BOOL init;
    if (!init) { init = TRUE; fn = (Fn)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"); }
    UINT d = fn ? fn(h) : 0;
    return d ? d : (UINT)g_dpi;
}

static void make_fonts(UINT dpi)
{
    typedef BOOL (WINAPI *Fn)(UINT, UINT, PVOID, UINT, UINT);
    static Fn fn; static BOOL init;
    NONCLIENTMETRICSW ncm;
    memset(&ncm, 0, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    if (!init) { init = TRUE; fn = (Fn)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi"); }
    if (!(fn && fn(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0, dpi)))
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    if (g_font) DeleteObject(g_font);
    if (g_fontBold) DeleteObject(g_fontBold);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW b = ncm.lfMessageFont;
    b.lfWeight = FW_BOLD;
    g_fontBold = CreateFontIndirectW(&b);
}

static void regedit_jump(const wchar_t *path)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit", 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(k, L"LastKey", 0, REG_SZ, (const BYTE *)path, (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
    ShellExecuteW(g_main, L"open", L"regedit.exe", NULL, NULL, SW_SHOWNORMAL);
}

/* ------------------------------------------------------------------ sidebar */
typedef struct { wchar_t key[MAX_PATH]; wchar_t name[160]; int count; const ProgInfo *pi; } AppCount;

static int cmp_appcount(const void *a, const void *b)
{
    const AppCount *x = (const AppCount *)a, *y = (const AppCount *)b;
    if (x->count != y->count) return y->count - x->count;
    return _wcsicmp(x->name, y->name);
}

static void side_cols(void)
{
    RECT rc;
    GetClientRect(g_side, &rc);
    int w = rc.right - GetSystemMetrics(SM_CXVSCROLL);
    ListView_SetColumnWidth(g_side, 1, S(48));
    ListView_SetColumnWidth(g_side, 0, w - S(48) > S(60) ? w - S(48) : S(60));
}

static void side_item(int gid, const wchar_t *name, int icon, int count, int kind, int cat, const wchar_t *key)
{
    wchar_t t[24];
    SideItem *s = &g_si[g_nsi];
    s->kind = kind; s->cat = cat;
    str_copy(s->key, MAX_PATH, key ? key : L"");
    LVITEMW it = { 0 };
    it.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_GROUPID;
    it.iItem = g_nsi; it.pszText = (wchar_t *)name; it.iImage = icon; it.lParam = g_nsi; it.iGroupId = gid;
    SendMessageW(g_side, LVM_INSERTITEMW, 0, (LPARAM)&it);
    wf(t, 24, L"%d", count);
    LVITEMW sub = { 0 };
    sub.iSubItem = 1; sub.pszText = t;
    SendMessageW(g_side, LVM_SETITEMTEXTW, (WPARAM)g_nsi, (LPARAM)&sub);
    g_nsi++;
}

static void build_side(void)
{
    int catc[CAT_COUNT] = { 0 }, none = 0, nac = 0;
    AppCount *ac = (AppCount *)calloc((size_t)g_fl.n + 1, sizeof *ac);
    free(g_si);
    g_si = (SideItem *)calloc((size_t)CAT_COUNT + 80, sizeof *g_si);
    g_nsi = 0;
    if (!ac || !g_si) { free(ac); return; }

    for (int i = 0; i < g_fl.n; i++) {
        Fmt *f = &g_fl.v[i];
        catc[f->cat]++;
        if (!f->pi || !fmt_progid(f)[0]) { none++; continue; }
        int k = 0;
        for (; k < nac; k++) if (!wcscmp(ac[k].key, f->pi->key)) break;
        if (k == nac) { str_copy(ac[k].key, MAX_PATH, f->pi->key); str_copy(ac[k].name, 160, f->pi->name); ac[k].pi = f->pi; nac++; }
        ac[k].count++;
    }
    qsort(ac, (size_t)nac, sizeof *ac, cmp_appcount);

    g_loading = TRUE;
    SendMessageW(g_side, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_side);
    SendMessageW(g_side, LVM_REMOVEALLGROUPS, 0, 0);
    LVGROUP g;
    memset(&g, 0, sizeof g);
    g.cbSize = sizeof g; g.mask = LVGF_HEADER | LVGF_GROUPID;
    g.pszHeader = L"File types"; g.iGroupId = 1;
    SendMessageW(g_side, LVM_INSERTGROUP, (WPARAM)-1, (LPARAM)&g);
    g.pszHeader = L"Currently opened by"; g.iGroupId = 2;
    SendMessageW(g_side, LVM_INSERTGROUP, (WPARAM)-1, (LPARAM)&g);

    side_item(1, L"All formats", icon_for_ext(L".zzz"), g_fl.n, SK_ALL, 0, NULL);
    for (int c = 0; c < CAT_COUNT; c++)
        if (catc[c]) side_item(1, cat_name(c), icon_for_ext(cat_sample_ext(c)), catc[c], SK_CAT, c, NULL);
    for (int k = 0; k < nac && k < 40; k++)
        side_item(2, ac[k].name, prog_icon(ac[k].pi), ac[k].count, SK_APP, 0, ac[k].key);
    if (none) side_item(2, L"No default app", icon_for_ext(L".zzz"), none, SK_NONE, 0, NULL);

    int sel = 0;                                                /* restore the selection */
    for (int i = 0; i < g_nsi; i++) {
        const SideItem *s = &g_si[i];
        if (s->kind == g_sideKind && (s->kind != SK_CAT || s->cat == g_sideCat) && (s->kind != SK_APP || !wcscmp(s->key, g_sideKey))) { sel = i; break; }
    }
    if (g_nsi && !(g_si[sel].kind == g_sideKind)) { g_sideKind = SK_ALL; sel = 0; }
    ListView_SetItemState(g_side, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    side_cols();
    SendMessageW(g_side, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_side, NULL, TRUE);
    g_loading = FALSE;
    free(ac);
}

/* -------------------------------------------------------------- list / view */
static const wchar_t *status_text(const Fmt *f, wchar_t *buf, size_t n)
{
    const wchar_t *s = f->user[0] ? L"Set by you" : (f->sys[0] ? L"Windows default" : L"Not associated");
    if ((f->flags & FF_PROTECTED) && g_env.ucpd) { wf(buf, n, L"%ls " U_MIDDOT L" protected", s); return buf; }
    return s;
}

static int cmp_view(const void *a, const void *b)
{
    const Fmt *x = &g_fl.v[*(const int *)a], *y = &g_fl.v[*(const int *)b];
    int r = 0;
    switch (g_sortcol) {
    case 0: r = _wcsicmp(x->ext, y->ext); break;
    case 1: r = _wcsicmp(fmt_desc((Fmt *)x), fmt_desc((Fmt *)y)); break;
    case 2: r = x->cat - y->cat; break;
    case 3: r = _wcsicmp(x->pi ? x->pi->name : L"\xFFFF", y->pi ? y->pi->name : L"\xFFFF"); break;
    case 4: r = (y->user[0] != 0) - (x->user[0] != 0); break;
    }
    if (g_sortdesc) r = -r;
    return r ? r : _wcsicmp(x->ext, y->ext);
}

static void update_sort_arrow(void)
{
    HWND hdr = ListView_GetHeader(g_list);
    for (int i = 0; i < NCOLS; i++) {
        HDITEMW hd = { 0 };
        hd.mask = HDI_FORMAT;
        if (!Header_GetItem(hdr, i, &hd)) continue;
        hd.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == g_sortcol) hd.fmt |= g_sortdesc ? HDF_SORTDOWN : HDF_SORTUP;
        Header_SetItem(hdr, i, &hd);
    }
}

static BOOL passes(Fmt *f, wchar_t (*tok)[40], int nt, const App *app)
{
    switch (g_sideKind) {
    case SK_CAT:  if (f->cat != g_sideCat) return FALSE; break;
    case SK_APP:  if (!f->pi || !fmt_progid(f)[0] || wcscmp(f->pi->key, g_sideKey)) return FALSE; break;
    case SK_NONE: if (fmt_progid(f)[0]) return FALSE; break;
    }
    switch (g_showMode) {
    case SM_NOTAPP: if (app && f->pi && !wcscmp(f->pi->key, app->key)) return FALSE; break;
    case SM_APP:    if (!app || !f->pi || wcscmp(f->pi->key, app->key)) return FALSE; break;
    case SM_MINE:   if (!f->user[0]) return FALSE; break;
    case SM_NONE:   if (fmt_progid(f)[0]) return FALSE; break;
    }
    for (int t = 0; t < nt; t++) {
        if (!(StrStrIW(f->ext, tok[t]) || StrStrIW(fmt_desc(f), tok[t]) || StrStrIW(cat_name(f->cat), tok[t]) ||
              (f->pi && StrStrIW(f->pi->name, tok[t]))))
            return FALSE;
    }
    return TRUE;
}

static void update_status_idle(void)
{
    wchar_t t[300];
    int marked = count_marked(), inview = 0, mine = 0;
    for (int i = 0; i < g_nview; i++) inview += g_fl.v[g_view[i]].marked;
    for (int i = 0; i < g_fl.n; i++) mine += g_fl.v[i].user[0] != 0;
    wf(t, 300, L"%d format%ls shown " U_MIDDOT L" %d marked", g_nview, plural(g_nview), marked);
    if (marked > inview) { wchar_t h[80]; wf(h, 80, L" (%d hidden by the current filter)", marked - inview); wcscat(t, h); }
    { wchar_t h[80]; wf(h, 80, L" " U_MIDDOT L" %d set by you", mine); wcscat(t, h); }
    set_status(t);
}

static void update_details(void);

static void update_buttons(void)
{
    int *tg = NULL, n = collect_targets(&tg, NULL), sel = ListView_GetSelectedCount(g_list), resettable = 0;
    wchar_t t[64];
    if (n) wf(t, 64, L"Assign %d format%ls", n, plural(n)); else str_copy(t, 64, L"Assign");
    SetWindowTextW(g_assign, t);
    EnableWindow(g_assign, n > 0);
    int *rs = NULL, rn = 0;
    if (sel) rn = collect_selected(&rs); else { free(rs); rs = NULL; }
    if (!sel) { rn = n; rs = tg; tg = NULL; }
    for (int i = 0; i < rn; i++) resettable += g_fl.v[rs[i]].user[0] != 0;
    EnableWindow(g_reset, resettable > 0);
    free(tg); free(rs);
    wchar_t p[MAX_PATH];
    EnableWindow(g_undo, backup_newest(p));
    EnableWindow(g_checkall, g_nview > 0);
}

static void rebuild_view(void)
{
    wchar_t q[100], tok[6][40], keep[40] = L"";
    int nt = 0;
    App *app = cur_app();
    int fr = ListView_GetNextItem(g_list, -1, LVNI_FOCUSED);
    if (fr >= 0 && fr < g_nview) str_copy(keep, 40, g_fl.v[g_view[fr]].ext);

    GetWindowTextW(g_filter, q, 100);
    for (wchar_t *p = q; *p && nt < 6;) {
        while (*p == L' ') p++;
        if (!*p) break;
        size_t n = wcscspn(p, L" ");
        if (n >= 40) n = 39;
        wcsncpy(tok[nt], p, n); tok[nt][n] = 0; nt++;
        p += n;
    }
    free(g_view);
    g_view = (int *)malloc((size_t)(g_fl.n ? g_fl.n : 1) * sizeof(int));
    g_nview = 0;
    for (int i = 0; i < g_fl.n && g_view; i++) if (passes(&g_fl.v[i], tok, nt, app)) g_view[g_nview++] = i;
    if (g_view) qsort(g_view, (size_t)g_nview, sizeof(int), cmp_view);

    g_loading = TRUE;
    SendMessageW(g_list, WM_SETREDRAW, FALSE, 0);
    ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemCountEx(g_list, g_nview, 0);
    if (keep[0]) for (int i = 0; i < g_nview; i++)
        if (!wcscmp(g_fl.v[g_view[i]].ext, keep)) {
            ListView_SetItemState(g_list, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_list, i, FALSE);
            break;
        }
    SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_list, NULL, TRUE);
    g_loading = FALSE;
    update_sort_arrow();
    update_status_idle();
    update_buttons();
    update_details();
}

static void rebuild_show(void)
{
    App *a = cur_app();
    wchar_t t[220];
    SendMessageW(g_show, CB_RESETCONTENT, 0, 0);
    g_nshow = 0;
#define ADDMODE(m, text) do { g_showModes[g_nshow++] = (m); SendMessageW(g_show, CB_ADDSTRING, 0, (LPARAM)(text)); } while (0)
    ADDMODE(SM_ALL, L"Show: all formats");
    if (a) {
        wf(t, 220, L"Show: not opened by %ls", a->name); ADDMODE(SM_NOTAPP, t);
        wf(t, 220, L"Show: opened by %ls", a->name);     ADDMODE(SM_APP, t);
    }
    ADDMODE(SM_MINE, L"Show: set by you");
    ADDMODE(SM_NONE, L"Show: no default app");
#undef ADDMODE
    int sel = 0;
    for (int i = 0; i < g_nshow; i++) if (g_showModes[i] == g_showMode) sel = i;
    g_showMode = g_showModes[sel];
    SendMessageW(g_show, CB_SETCURSEL, (WPARAM)sel, 0);
}

static void update_app_card(void)
{
    App *a = cur_app();
    SHFILEINFOW fi;
    HICON ic = NULL;
    memset(&fi, 0, sizeof fi);
    if (g_appExe[0] && file_exists(g_appExe)) {
        if (SHGetFileInfoW(g_appExe, 0, &fi, sizeof fi, SHGFI_ICON | SHGFI_LARGEICON)) ic = fi.hIcon;
    } else if (SHGetFileInfoW(L"app.exe", FILE_ATTRIBUTE_NORMAL, &fi, sizeof fi, SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)) ic = fi.hIcon;
    HICON old = (HICON)SendMessageW(g_appicon, STM_SETICON, (WPARAM)ic, 0);
    if (old) DestroyIcon(old);

    if (a) {
        SetWindowTextW(g_appname, a->name);
        SetWindowTextW(g_apppath, a->exe);
    } else if (g_appExe[0]) {
        wchar_t b[260];
        const wchar_t *s = wcsrchr(g_appExe, L'\\');
        str_copy(b, 260, s ? s + 1 : g_appExe);
        SetWindowTextW(g_appname, b);
        SetWindowTextW(g_apppath, g_appExe);
    } else {
        SetWindowTextW(g_appname, L"No application chosen yet");
        SetWindowTextW(g_apppath, L"Choose the application that should open your formats.");
    }
    SetWindowTextW(g_choose, g_appExe[0] ? L"Change" U_ELLIPSIS : L"Choose application" U_ELLIPSIS);
    rebuild_show();
}

static void set_app(const wchar_t *exe)
{
    str_copy(g_appExe, MAX_PATH, exe);
    if (exe[0]) mru_push(exe);
    update_app_card();
    rebuild_view();
}

/* ------------------------------------------------------------------- details */
static void update_details(void)
{
    wchar_t a[500] = L"", b[700] = L"", eff[260];
    int sel = ListView_GetSelectedCount(g_list);
    if (sel == 1) {
        Fmt *f = view_fmt(ListView_GetNextItem(g_list, -1, LVNI_SELECTED));
        if (f) {
            const wchar_t *pid = fmt_progid(f);
            wchar_t st[80];
            wf(a, 500, L"%ls  " U_EMDASH L"  %ls  " U_EMDASH L"  opens with %ls  (%ls)", f->ext, fmt_desc(f),
               f->pi ? f->pi->name : L"nothing", status_text(f, st, 80));
            if (pid[0]) {
                wf(b, 700, L"ProgId: %ls%ls%ls", pid, f->pi && f->pi->exe[0] ? L"   " U_MIDDOT L"   " : L"", f->pi ? f->pi->exe : L"");
                if (f->user[0] && uc_effective(f->ext, eff, 260) && _wcsicmp(eff, f->user) != 0) {
                    wchar_t w[400];
                    wf(w, 400, L"   " U_MIDDOT L"   Windows is actually using: %ls (it ignored this entry)", eff);
                    wcsncat(b, w, 699 - wcslen(b));
                }
            } else str_copy(b, 700, L"No application is associated with this type.");
        }
    } else if (sel > 1) {
        wf(a, 500, L"%d formats selected", sel);
        str_copy(b, 700, L"Press Space to tick or untick them all, or right-click for more.");
    } else {
        str_copy(a, 500, L"Tick the formats to change (or press Ctrl+A for everything shown), choose an application, then press Assign.");
    }
    SetWindowTextW(g_det1, a);
    SetWindowTextW(g_det2, b);
}

/* ----------------------------------------------------------------- marking */
static void toggle_row(int row)
{
    Fmt *f = view_fmt(row);
    if (!f) return;
    f->marked = !f->marked;
    ListView_RedrawItems(g_list, row, row);
    update_status_idle();
    update_buttons();
}

static void mark_selected(BOOL on)
{
    for (int r = -1; (r = ListView_GetNextItem(g_list, r, LVNI_SELECTED)) >= 0;) { Fmt *f = view_fmt(r); if (f) f->marked = on; }
    InvalidateRect(g_list, NULL, FALSE);
    update_status_idle();
    update_buttons();
}

static void mark_visible(BOOL on)
{
    for (int i = 0; i < g_nview; i++) g_fl.v[g_view[i]].marked = on;
    if (!on) for (int i = 0; i < g_fl.n; i++) g_fl.v[i].marked = FALSE;      /* "none" clears hidden ones too */
    InvalidateRect(g_list, NULL, FALSE);
    update_status_idle();
    update_buttons();
}

static void do_check_supported(void)
{
    App *a = cur_app();
    if (!a) { MessageBoxW(g_main, L"Choose an application first; then this marks every format that application says it can open.", APP_TITLE, MB_ICONINFORMATION); return; }
    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    wchar_t keys[1000];
    int n = 0;
    set_status(L"Checking which formats this application opens" U_ELLIPSIS);
    for (int i = 0; i < g_fl.n; i++) {
        Fmt *f = &g_fl.v[i];
        if (f->flags & FF_BLOCKED) continue;
        ext_handlers(&g_apps, f->ext, keys, 1000);
        if (ext_handled_by(keys, a)) { f->marked = TRUE; n++; }
    }
    SetCursor(old);
    InvalidateRect(g_list, NULL, FALSE);
    update_status_idle();
    update_buttons();
    if (n) {
        wchar_t t[300];
        wf(t, 300, L"Marked %d format%ls that %ls registers for itself. Narrow it down with the category list if needed.", n, plural(n), a->name);
        set_status(t);
    } else {
        wchar_t t[500];
        wf(t, 500, L"%ls doesn't register specific file types with Windows, so nothing could be marked automatically.\n\n"
                   L"You can still tick formats yourself (or a whole category with Ctrl+A) and assign them: "
                   L"the application is registered for each one as needed.", a->name);
        MessageBoxW(g_main, t, APP_TITLE, MB_ICONINFORMATION);
    }
}

/* ---------------------------------------------------------- load / refresh */
static void reload(void)
{
    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    wchar_t *keep = NULL;
    size_t kl = 0, kc = 0;
    set_status(L"Reading file associations" U_ELLIPSIS);
    for (int i = 0; i < g_fl.n; i++) if (g_fl.v[i].marked) {                    /* remember ticks across a reload */
        size_t l = wcslen(g_fl.v[i].ext) + 1;
        if (kl + l + 1 > kc) { kc = kc ? kc * 2 : 1024; keep = (wchar_t *)realloc(keep, kc * sizeof(wchar_t)); }
        if (!keep) break;
        wcscpy(keep + kl, g_fl.v[i].ext); kl += l; keep[kl] = 0;
    }
    fmts_free(&g_fl);
    fmts_load(&g_fl, &g_env);
    for (const wchar_t *p = keep; p && *p; p += wcslen(p) + 1) {
        int i = fmts_find(&g_fl, p);
        if (i >= 0) g_fl.v[i].marked = TRUE;
    }
    free(keep);
    build_side();
    rebuild_view();
    SetCursor(old);
}

/* --------------------------------------------------------------- operations */
static void progress_cb(int done, int total, const wchar_t *ext, void *ctx)
{
    static DWORD last;
    DWORD now = GetTickCount();
    if (done && done < total && now - last < 40) return;
    last = now;
    wchar_t t[200];
    wf(t, 200, L"%ls %d of %d   %ls", (const wchar_t *)ctx, done < total ? done + 1 : total, total, ext);
    set_status(t);
}

static BOOL ensure_can_write(void)
{
    if (!g_env.canWrite) {
        wchar_t t[900];
        wf(t, 900, L"%ls\n\nOpen Windows Settings " U_ARROW L" Default apps instead?", g_env.note);
        if (MessageBoxW(g_main, t, APP_TITLE, MB_ICONWARNING | MB_YESNO) == IDYES) open_default_apps_settings(cur_app());
        return FALSE;
    }
    if (g_env.preflight < 0) {
        HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
        set_status(L"Checking that Windows accepts changes on this PC" U_ELLIPSIS);
        uc_preflight(&g_env);
        SetCursor(old);
    }
    if (g_env.preflight == 0) {
        int r = MessageBoxW(g_main,
            L"Windows did not accept a harmless test change, so defaults set by this program may be ignored.\n\n"
            L"Each format is still verified after it is written, and the previous default is restored if Windows refuses it.\n\n"
            L"Try anyway?", APP_TITLE, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
        if (r != IDYES) return FALSE;
    }
    return TRUE;
}

static void after_change(void)
{
    build_side();
    rebuild_view();
}

static void run_assign(int *tg, int n)
{
    App *a = cur_app();
    if (!n) return;
    if (!a) {
        ensure_apps();
        wchar_t exe[MAX_PATH];
        if (!pick_app_dialog(g_main, &g_apps, &g_fl, (const wchar_t (*)[MAX_PATH])g_cfg.recent, MAX_RECENT, exe)) return;
        set_app(exe);
        a = cur_app();
        if (!a) return;
    }
    if (!ensure_can_write()) return;
    if (n > 60) {
        wchar_t t[300];
        wf(t, 300, L"Make %ls the default for %d formats?\n\nThe previous defaults are saved so you can undo this (Ctrl+Z).", a->name, n);
        if (MessageBoxW(g_main, t, APP_TITLE, MB_ICONQUESTION | MB_OKCANCEL) != IDOK) return;
    }
    wchar_t snap[MAX_PATH], label[260];
    wf(label, 260, L"Assigned %d type%ls to %ls", n, plural(n), a->name);
    backup_save(&g_fl, tg, n, label, snap);

    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    AssignResult r;
    assign_run(&g_fl, &g_env, a, tg, n, &r, progress_cb, (void *)L"Assigning");
    SetCursor(old);

    if (!r.count[AS_OK]) DeleteFileW(snap);            /* nothing changed, so there is nothing to undo */

    int good = r.count[AS_OK] + r.count[AS_ALREADY];
    for (int i = 0; i < r.n; i++)
        if (r.items[i].outcome == AS_OK || r.items[i].outcome == AS_ALREADY) g_fl.v[r.items[i].fmt].marked = FALSE;
    after_change();

    int bad = r.n - good;
    wchar_t msg[400];
    if (!bad) {
        wf(msg, 400, L"%ls now opens %d format%ls. Undo with Ctrl+Z.", a->name, good, plural(good));
        set_status(msg);
    } else {
        wchar_t sum[600];
        if (r.aborted) wf(sum, 600, L"Windows refused the changes, so the rest were not attempted. Your previous defaults are intact.\n\n%d of %d format%ls were changed.", good, n, plural(n));
        else wf(sum, 600, L"%ls now opens %d of %d format%ls. The others need attention:", a->name, good, n, plural(n));
        wf(msg, 400, L"%d changed, %d need attention", good, bad);
        set_status(msg);
        report_dialog(g_main, L"Some formats need attention", sum, &g_fl, &r, a);
    }
    result_free(&r);
}

static void do_assign(void)
{
    int *tg = NULL, n = collect_targets(&tg, NULL);
    if (!n) { set_status(L"Tick the formats you want to change first (Ctrl+A ticks everything shown)."); free(tg); return; }
    run_assign(tg, n);
    free(tg);
}

static void do_choose(void)
{
    ensure_apps();
    wchar_t exe[MAX_PATH];
    if (pick_app_dialog(g_main, &g_apps, &g_fl, (const wchar_t (*)[MAX_PATH])g_cfg.recent, MAX_RECENT, exe)) set_app(exe);
    else update_status_idle();
}

static void assign_selected_to(const wchar_t *exe)
{
    int *tg = NULL, n = collect_selected(&tg);
    if (n) { set_app(exe); run_assign(tg, n); }
    free(tg);
}

static void do_reset(void)
{
    int *tg = NULL, n, sel = ListView_GetSelectedCount(g_list);
    n = sel ? collect_selected(&tg) : collect_targets(&tg, NULL);
    int have = 0;
    for (int i = 0; i < n; i++) have += g_fl.v[tg[i]].user[0] != 0;
    if (!have) { free(tg); return; }
    wchar_t t[400], snap[MAX_PATH];
    wf(t, 400, L"Remove your default for %d format%ls and let Windows choose again?\n\nYou can undo this with Ctrl+Z.", have, plural(have));
    if (MessageBoxW(g_main, t, APP_TITLE, MB_ICONQUESTION | MB_OKCANCEL) != IDOK) { free(tg); return; }
    wf(t, 400, L"Reset %d type%ls to the Windows default", have, plural(have));
    backup_save(&g_fl, tg, n, t, snap);
    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    AssignResult r;
    reset_run(&g_fl, &g_env, tg, n, &r, progress_cb, (void *)L"Resetting");
    SetCursor(old);
    if (!r.count[AS_OK]) DeleteFileW(snap);
    after_change();
    if (r.count[AS_PROTECTED] || r.count[AS_FAILED]) {
        report_dialog(g_main, L"Some formats could not be reset", L"These formats could not be reset:", &g_fl, &r, NULL);
    } else {
        wf(t, 400, L"%d format%ls reset to the Windows default. Undo with Ctrl+Z.", r.count[AS_OK], plural(r.count[AS_OK]));
        set_status(t);
    }
    result_free(&r);
    free(tg);
}

static void do_undo(void)
{
    wchar_t path[MAX_PATH], label[300], t[600];
    int cnt = 0;
    if (!backup_newest(path)) { set_status(L"Nothing to undo."); return; }
    backup_describe(path, label, 300, &cnt);
    wf(t, 600, L"Undo the last change?\n\n%ls\n(%d format%ls will go back to how they were.)", label[0] ? label : L"Last change", cnt, plural(cnt));
    if (MessageBoxW(g_main, t, APP_TITLE, MB_ICONQUESTION | MB_OKCANCEL) != IDOK) return;
    if (!ensure_can_write()) return;
    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    AssignResult r;
    BOOL ok = backup_restore(&g_fl, &g_env, path, &r, progress_cb, (void *)L"Restoring");
    SetCursor(old);
    if (!ok) { MessageBoxW(g_main, L"The backup could not be read.", APP_TITLE, MB_ICONERROR); result_free(&r); return; }
    backup_mark_undone(path);
    after_change();
    if (r.count[AS_OK] == r.n) { wf(t, 600, L"Restored %d format%ls.", r.n, plural(r.n)); set_status(t); }
    else report_dialog(g_main, L"Some formats could not be restored", L"Restored what Windows allowed. These need attention:", &g_fl, &r, NULL);
    result_free(&r);
}

static void do_add_types(void)
{
    wchar_t text[2048];
    if (!add_formats_dialog(g_main, text, 2048)) return;
    int added = 0, bad = 0, blocked = 0, first = -1;
    for (wchar_t *p = text; *p;) {
        size_t n = wcscspn(p, L" ,;\t\r\n");
        if (n) {
            wchar_t tok[64];
            str_copy(tok, 64, p);
            tok[n < 63 ? n : 63] = 0;
            int i = fmts_add_custom(&g_fl, &g_env, tok);
            if (i == -1) bad++; else if (i == -2) blocked++;
            else { g_fl.v[i].marked = TRUE; added++; if (first < 0) first = i; }
        }
        p += n;
        p += wcsspn(p, L" ,;\t\r\n");
    }
    g_sideKind = SK_ALL;
    g_showMode = SM_ALL;
    SetWindowTextW(g_filter, L"");
    build_side();
    rebuild_view();
    wchar_t t[300];
    wf(t, 300, L"Added and marked %d file type%ls%ls%ls.", added, plural(added), bad ? L"; some entries weren't valid extensions" : L"",
       blocked ? L"; programs and shortcuts are never reassigned" : L"");
    set_status(t);
}

static void do_export(BOOL xml)
{
    wchar_t f[MAX_PATH];
    wcscpy(f, xml ? L"DefaultAssociations.xml" : L"my-defaults.fam");
    if (!browse_file(g_main, f, xml ? L"Default associations (XML)\0*.xml\0" : L"Association snapshot\0*.fam\0", TRUE, xml ? L"xml" : L"fam")) return;
    if (xml ? backup_export_xml(&g_fl, f) : backup_export(&g_fl, f)) set_status(xml ? L"Exported for DISM / Group Policy." : L"Exported your defaults.");
    else MessageBoxW(g_main, L"The file could not be written.", APP_TITLE, MB_ICONERROR);
}

static void do_restore(void)
{
    wchar_t f[MAX_PATH] = L"";
    if (!browse_file(g_main, f, L"Association snapshot\0*.fam\0All files\0*.*\0", FALSE, NULL)) return;
    if (MessageBoxW(g_main, L"Apply the defaults stored in this file?\n\nFormats listed in it will be set exactly as saved.", APP_TITLE, MB_ICONQUESTION | MB_OKCANCEL) != IDOK) return;
    if (!ensure_can_write()) return;
    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    AssignResult r;
    BOOL ok = backup_restore(&g_fl, &g_env, f, &r, progress_cb, (void *)L"Restoring");
    SetCursor(old);
    if (!ok) { MessageBoxW(g_main, L"That file isn't a valid snapshot.", APP_TITLE, MB_ICONERROR); result_free(&r); return; }
    after_change();
    wchar_t t[200];
    if (r.count[AS_OK] == r.n) { wf(t, 200, L"Applied %d format%ls from the file.", r.n, plural(r.n)); set_status(t); }
    else report_dialog(g_main, L"Some formats could not be applied", L"Applied what Windows allowed. These need attention:", &g_fl, &r, NULL);
    result_free(&r);
}

static void do_selftest(void)
{
    HCURSOR old = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    uc_probe(&g_env);
    BOOL ok = uc_preflight(&g_env);
    SetCursor(old);
    show_env();
    MessageBoxW(g_main, !g_env.canWrite ? g_env.note
                      : ok ? L"Windows accepted a test change on this PC, so defaults set by this program will be honoured."
                           : L"Windows did NOT accept a test change. Defaults set by this program may be ignored here; "
                             L"use Windows Settings for anything that doesn't stick.",
                APP_TITLE, ok ? MB_ICONINFORMATION : MB_ICONWARNING);
}

static void do_report(void)
{
    wchar_t r[2400], pf[120];
    uc_report(&g_env, r, 1800);
    wf(pf, 120, L"\r\nLive test with Windows:  %ls", g_env.preflight < 0 ? L"not run yet (Tools " U_ARROW L" Test that changes stick)"
                                                  : g_env.preflight ? L"accepted" : L"REFUSED");
    wcsncat(r, pf, 2399 - wcslen(r));
    text_dialog(g_main, L"Compatibility report",
                L"What this PC's Windows does with default-app choices, and whether this program can write them. Copy it when asking for help.", r);
}

static void do_about(void)
{
    MessageBoxW(g_main,
        APP_TITLE L" " APP_VERSION L"\n\n"
        L"Assign many file formats to the application you choose, in one go.\n\n"
        L"Every change is checked with Windows afterwards, and anything Windows refuses is put back. "
        L"Types Windows protects (such as .pdf) are handed over to Windows Settings.\n\n"
        L"The hashing code for Windows' default-app records is a C port of cssxn/UserChoiceLatestHash (MIT); "
        L"the classic hash follows PS-SFTA (MIT).",
        L"About", MB_ICONINFORMATION);
}

static void open_backups(void)
{
    wchar_t d[MAX_PATH];
    backup_folder(d, MAX_PATH);
    ShellExecuteW(g_main, L"open", d, NULL, NULL, SW_SHOWNORMAL);
}

/* -------------------------------------------------------------- context menu */
static void show_popup(POINT pt)
{
    int sel = ListView_GetSelectedCount(g_list);
    if (!sel) return;
    HMENU m = CreatePopupMenu(), sub = CreatePopupMenu();
    ensure_apps();
    int added = 0;
    for (int i = 0; i < MAX_RECENT; i++) {
        if (!g_cfg.recent[i][0]) continue;
        int ai = apps_find(&g_apps, g_cfg.recent[i]);
        if (ai < 0) continue;
        AppendMenuW(sub, MF_STRING, (UINT_PTR)(IDM_RECENT0 + i), g_apps.v[ai].name);
        added++;
    }
    if (added) AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sub, MF_STRING, IDM_CHOOSE, L"Choose another application" U_ELLIPSIS);
    wchar_t t[80];
    wf(t, 80, L"Open %d selected with", sel);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sub, sel == 1 ? L"Open with" : t);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_CHECKSEL, L"Tick selected");
    AppendMenuW(m, MF_STRING, IDM_UNCHECKSEL, L"Untick selected");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_RESET, L"Reset to Windows default");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (sel == 1 ? 0 : MF_GRAYED), IDM_COPYPROG, L"Copy ProgId");
    AppendMenuW(m, MF_STRING | (sel == 1 ? 0 : MF_GRAYED), IDM_REGEDIT, L"Show in Registry Editor");
    AppendMenuW(m, MF_STRING, IDM_TYPESETTINGS, L"Open Windows Settings for default apps");
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, NULL);
    DestroyMenu(m);
}

static HMENU build_menu(void)
{
    HMENU bar = CreateMenu(), file = CreatePopupMenu(), edit = CreatePopupMenu(), view = CreatePopupMenu();
    HMENU tools = CreatePopupMenu(), help = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_ADD, L"&Add file types" U_ELLIPSIS L"\tCtrl+N");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, IDM_EXPORT, L"&Export my defaults" U_ELLIPSIS L"\tCtrl+E");
    AppendMenuW(file, MF_STRING, IDM_EXPORT_XML, L"Export for &DISM / Group Policy (XML)" U_ELLIPSIS);
    AppendMenuW(file, MF_STRING, IDM_RESTORE, L"&Restore defaults from a file" U_ELLIPSIS);
    AppendMenuW(file, MF_STRING, IDM_BACKUPS, L"Open &backup folder");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, IDM_EXIT, L"E&xit");
    AppendMenuW(edit, MF_STRING, IDM_CHOOSE, L"&Choose application" U_ELLIPSIS L"\tCtrl+O");
    AppendMenuW(edit, MF_STRING, IDM_ASSIGN, L"&Assign to marked formats\tCtrl+Enter");
    AppendMenuW(edit, MF_STRING, IDM_UNDO, L"&Undo last change\tCtrl+Z");
    AppendMenuW(edit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(edit, MF_STRING, IDM_CHECKALL, L"Tick all &shown\tCtrl+A");
    AppendMenuW(edit, MF_STRING, IDM_CHECKNONE, L"&Untick everything\tCtrl+Shift+A");
    AppendMenuW(edit, MF_STRING, IDM_CHECKSUPPORTED, L"Tick formats the chosen application &registers");
    AppendMenuW(edit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(edit, MF_STRING, IDM_RESET, L"Reset selected to Windows &default");
    AppendMenuW(view, MF_STRING, IDM_FIND, L"&Search formats\tCtrl+F");
    AppendMenuW(view, MF_STRING, IDM_REFRESH, L"&Refresh\tF5");
    AppendMenuW(tools, MF_STRING, IDM_SETTINGS, L"Open &Windows default apps settings");
    AppendMenuW(tools, MF_STRING, IDM_SELFTEST, L"&Test that changes stick");
    AppendMenuW(tools, MF_STRING, IDM_REPORT, L"&Compatibility report" U_ELLIPSIS);
    AppendMenuW(tools, MF_STRING, IDM_NOTIFY, L"Refresh &icons and associations");
    AppendMenuW(help, MF_STRING, IDM_ABOUT, L"&About");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"&File");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)edit, L"&Edit");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)view, L"&View");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)tools, L"&Tools");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, L"&Help");
    return bar;
}

/* ------------------------------------------------------------------- layout */
static void show_env(void)
{
    SendMessageW(g_status, SB_SETTEXTW, 1, (LPARAM)g_env.summary);
}

static void layout(void)
{
    RECT rc;
    GetClientRect(g_main, &rc);
    int W = rc.right, H = rc.bottom, m = S(10), g = S(6);

    int parts[2] = { W - S(420), -1 };
    SendMessageW(g_status, WM_SIZE, 0, 0);
    SendMessageW(g_status, SB_SETPARTS, 2, (LPARAM)parts);
    show_env();
    RECT sr;
    GetWindowRect(g_status, &sr);
    int sbh = sr.bottom - sr.top, top = m, bottom = H - sbh - m, lw = S(236);

    MoveWindow(g_side, m, top, lw, bottom - top, TRUE);
    side_cols();

    int rx = m + lw + m, rw = W - rx - m, y = top;
    int cardh = S(46);
    MoveWindow(g_appicon, rx + S(2), y + (cardh - S(32)) / 2, S(32), S(32), TRUE);
    int bw = S(190), cw = S(120), tx = rx + S(46);
    MoveWindow(g_appname, tx, y + S(2), rw - S(46) - bw - cw - 2 * g, S(20), TRUE);
    MoveWindow(g_apppath, tx, y + S(24), rw - S(46) - bw - cw - 2 * g, S(20), TRUE);
    MoveWindow(g_choose, rx + rw - bw - g - cw, y + S(7), cw, S(32), TRUE);
    MoveWindow(g_assign, rx + rw - bw, y + S(7), bw, S(32), TRUE);
    y += cardh + g;
    MoveWindow(g_rule, rx, y, rw, S(2), TRUE);
    y += S(2) + g + S(2);

    int bh = S(26), cb = S(104);
    int fw = S(220), sw = rw - fw - 2 * cb - 3 * g;
    if (sw > S(300)) sw = S(300);
    MoveWindow(g_filter, rx, y + S(1), fw, S(24), TRUE);
    MoveWindow(g_show, rx + fw + g, y, sw, S(240), TRUE);
    MoveWindow(g_checknone, rx + rw - cb, y, cb, bh, TRUE);
    MoveWindow(g_checkall, rx + rw - 2 * cb - g, y, cb, bh, TRUE);
    y += bh + g;

    int dh = S(40), bwid = S(132);
    int d1 = bottom - dh;
    MoveWindow(g_reset, rx + rw - 2 * bwid - g, d1 + S(7), bwid, S(26), TRUE);
    MoveWindow(g_undo, rx + rw - bwid, d1 + S(7), bwid, S(26), TRUE);
    int dw = rw - 2 * bwid - 2 * g;
    MoveWindow(g_det1, rx, d1 + S(2), dw, S(18), TRUE);
    MoveWindow(g_det2, rx, d1 + S(21), dw, S(18), TRUE);
    MoveWindow(g_list, rx, y, rw, d1 - g - y, TRUE);
}

static void setup_list_columns(void)
{
    while (Header_GetItemCount(ListView_GetHeader(g_list)) > 0) ListView_DeleteColumn(g_list, 0);
    for (int i = 0; i < NCOLS; i++) {
        LVCOLUMNW c = { 0 };
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = (wchar_t *)COLS[i].name; c.cx = S(COLS[i].w);
        SendMessageW(g_list, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }
}

static void on_create(HWND h, BOOL haveCfg)
{
    g_main = h;
    g_dpi = (int)dpi_of(h);
    make_fonts((UINT)g_dpi);
    g_menu = build_menu();
    SetMenu(h, g_menu);

    DWORD edit = WS_TABSTOP | ES_AUTOHSCROLL;
    g_side = ctl(h, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | LVS_SHAREIMAGELISTS, 0, 0, 10, 10, IDC_SIDE);
    ListView_SetExtendedListViewStyle(g_side, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    SetWindowTheme(g_side, L"Explorer", NULL);
    ListView_SetImageList(g_side, sys_imagelist(), LVSIL_SMALL);
    ListView_EnableGroupView(g_side, TRUE);
    { LVCOLUMNW c = { 0 }; c.mask = LVCF_WIDTH | LVCF_FMT; c.cx = S(150); c.fmt = LVCFMT_LEFT;
      SendMessageW(g_side, LVM_INSERTCOLUMNW, 0, (LPARAM)&c);
      c.cx = S(48); c.fmt = LVCFMT_RIGHT; SendMessageW(g_side, LVM_INSERTCOLUMNW, 1, (LPARAM)&c); }

    g_appicon = ctl(h, 0, L"STATIC", L"", SS_ICON | SS_CENTERIMAGE, 0, 0, 10, 10, IDC_APPICON);
    g_appname = ctl(h, 0, L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 10, 10, IDC_APPNAME);
    g_apppath = ctl(h, 0, L"STATIC", L"", SS_NOPREFIX | SS_PATHELLIPSIS, 0, 0, 10, 10, IDC_APPPATH);
    g_choose = ctl(h, 0, L"BUTTON", L"Choose application" U_ELLIPSIS, WS_TABSTOP, 0, 0, 10, 10, IDC_CHOOSE);
    g_assign = ctl(h, 0, L"BUTTON", L"Assign", WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 10, 10, IDC_ASSIGN);
    g_rule = ctl(h, 0, L"STATIC", L"", SS_ETCHEDHORZ, 0, 0, 10, 10, IDC_RULE);
    g_filter = ctl(h, WS_EX_CLIENTEDGE, L"EDIT", L"", edit, 0, 0, 10, 10, IDC_FILTER);
    SendMessageW(g_filter, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search formats (Ctrl+F)");
    g_show = ctl(h, 0, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 10, 10, IDC_SHOW);
    g_checkall = ctl(h, 0, L"BUTTON", L"Tick all shown", WS_TABSTOP, 0, 0, 10, 10, IDC_CHECKALL);
    g_checknone = ctl(h, 0, L"BUTTON", L"Untick all", WS_TABSTOP, 0, 0, 10, 10, IDC_CHECKNONE);
    g_list = ctl(h, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_OWNERDATA | LVS_SHAREIMAGELISTS, 0, 0, 10, 10, IDC_LIST);
    ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP | LVS_EX_SUBITEMIMAGES | LVS_EX_HEADERDRAGDROP);
    SetWindowTheme(g_list, L"Explorer", NULL);
    ListView_SetImageList(g_list, sys_imagelist(), LVSIL_SMALL);
    ListView_SetCallbackMask(g_list, LVIS_STATEIMAGEMASK);      /* tick boxes come from our data, not the control */
    setup_list_columns();
    g_det1 = ctl(h, 0, L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 10, 10, IDC_DET1);
    g_det2 = ctl(h, 0, L"STATIC", L"", SS_NOPREFIX | SS_PATHELLIPSIS, 0, 0, 10, 10, IDC_DET2);
    g_reset = ctl(h, 0, L"BUTTON", L"Reset to default", WS_TABSTOP, 0, 0, 10, 10, IDC_RESET);
    g_undo = ctl(h, 0, L"BUTTON", L"Undo last change", WS_TABSTOP, 0, 0, 10, 10, IDC_UNDO);
    g_status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, h, (HMENU)(INT_PTR)IDC_STATUS, g_inst, NULL);

    EnumChildWindows(h, font_cb, (LPARAM)g_font);
    SendMessageW(g_appname, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
    DragAcceptFiles(h, TRUE);

    if (haveCfg) {
        g_sideKind = g_cfg.sideKind; g_sideCat = g_cfg.sideCat;
        if (g_cfg.sortcol >= 0 && g_cfg.sortcol < NCOLS) { g_sortcol = g_cfg.sortcol; g_sortdesc = g_cfg.sortdesc != 0; }
        g_showMode = g_cfg.showMode;
        str_copy(g_appExe, MAX_PATH, g_cfg.lastApp);
    }
    if (g_sideKind == SK_APP || g_sideKind == SK_NONE || g_sideKind > SK_NONE) g_sideKind = SK_ALL;   /* apps vary between runs */
    uc_probe(&g_env);
    show_env();
    layout();
    update_app_card();
    set_status(L"Starting" U_ELLIPSIS);
    PostMessageW(h, WM_APP_LOAD, 0, 0);
}

/* ------------------------------------------------------------ notifications */
static LRESULT on_list_notify(NMHDR *nh)
{
    switch (nh->code) {
    case LVN_GETDISPINFOW: {
        NMLVDISPINFOW *di = (NMLVDISPINFOW *)nh;
        Fmt *f = view_fmt(di->item.iItem);
        if (!f) return 0;
        if (di->item.mask & LVIF_TEXT) {
            wchar_t st[96];
            switch (di->item.iSubItem) {
            case 0: str_copy(di->item.pszText, (size_t)di->item.cchTextMax, f->ext); break;
            case 1: str_copy(di->item.pszText, (size_t)di->item.cchTextMax, fmt_desc(f)); break;
            case 2: str_copy(di->item.pszText, (size_t)di->item.cchTextMax, cat_name(f->cat)); break;
            case 3: str_copy(di->item.pszText, (size_t)di->item.cchTextMax, f->pi ? f->pi->name : L"(none)"); break;
            case 4: str_copy(di->item.pszText, (size_t)di->item.cchTextMax, status_text(f, st, 96)); break;
            }
        }
        if (di->item.mask & LVIF_IMAGE) {
            if (di->item.iSubItem == 0) di->item.iImage = fmt_icon(f);
            else if (di->item.iSubItem == 3) di->item.iImage = f->pi ? prog_icon(f->pi) : icon_generic_app();
        }
        if ((di->item.mask & LVIF_STATE) && (di->item.stateMask & LVIS_STATEIMAGEMASK))
            di->item.state |= INDEXTOSTATEIMAGEMASK(f->marked ? 2 : 1);
        return 0; }
    case NM_CLICK:
    case NM_DBLCLK: {
        NMITEMACTIVATE *ia = (NMITEMACTIVATE *)nh;
        LVHITTESTINFO ht = { 0 };
        ht.pt = ia->ptAction;
        ListView_HitTest(g_list, &ht);
        if (ht.iItem >= 0 && (ht.flags & LVHT_ONITEMSTATEICON)) toggle_row(ht.iItem);
        else if (nh->code == NM_DBLCLK && ht.iItem >= 0) toggle_row(ht.iItem);
        return 0; }
    case LVN_KEYDOWN: {
        NMLVKEYDOWN *kd = (NMLVKEYDOWN *)nh;
        if (kd->wVKey == VK_SPACE || kd->wVKey == VK_RETURN) {
            int fi = ListView_GetNextItem(g_list, -1, LVNI_FOCUSED);
            Fmt *ff = view_fmt(fi);
            if (ff) mark_selected(!ff->marked);
        }
        return 0; }
    case LVN_ITEMCHANGED: {
        NMLISTVIEW *nm = (NMLISTVIEW *)nh;
        if (!g_loading && (nm->uChanged & LVIF_STATE) && ((nm->uNewState ^ nm->uOldState) & LVIS_SELECTED)) {
            update_details();
            update_buttons();
        }
        return 0; }
    case LVN_COLUMNCLICK: {
        int c = ((NMLISTVIEW *)nh)->iSubItem;
        if (c == g_sortcol) g_sortdesc = !g_sortdesc; else { g_sortcol = c; g_sortdesc = 0; }
        rebuild_view();
        return 0; }
    case NM_CUSTOMDRAW: {
        NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)nh;
        switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: return CDRF_NOTIFYSUBITEMDRAW;
        case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
            Fmt *f = view_fmt((int)cd->nmcd.dwItemSpec);
            if (!f) return CDRF_DODEFAULT;
            if (cd->iSubItem == 4 && f->user[0]) { cd->clrText = GetSysColor(COLOR_HOTLIGHT); return CDRF_NEWFONT; }
            if ((cd->iSubItem == 3 || cd->iSubItem == 4) && !fmt_progid(f)[0]) { cd->clrText = GetSysColor(COLOR_GRAYTEXT); return CDRF_NEWFONT; }
            return CDRF_DODEFAULT; }
        }
        return CDRF_DODEFAULT; }
    }
    return 0;
}

static LRESULT on_notify(NMHDR *nh)
{
    if (nh->idFrom == IDC_LIST) return on_list_notify(nh);
    if (nh->idFrom == IDC_SIDE && nh->code == LVN_ITEMCHANGED && !g_loading) {
        NMLISTVIEW *nm = (NMLISTVIEW *)nh;
        if ((nm->uChanged & LVIF_STATE) && (nm->uNewState & LVIS_SELECTED) && !(nm->uOldState & LVIS_SELECTED) &&
            nm->iItem >= 0 && nm->iItem < g_nsi) {
            const SideItem *s = &g_si[nm->iItem];
            g_sideKind = s->kind; g_sideCat = s->cat;
            str_copy(g_sideKey, MAX_PATH, s->key);
            rebuild_view();
        }
    }
    return 0;
}

static void on_command(WPARAM w)
{
    int id = LOWORD(w);
    if (id >= IDM_RECENT0 && id < IDM_RECENT0 + MAX_RECENT) { assign_selected_to(g_cfg.recent[id - IDM_RECENT0]); return; }
    switch (id) {
    case IDC_FILTER:
        if (HIWORD(w) == EN_CHANGE && !g_loading) SetTimer(g_main, TIMER_FILTER, 180, NULL);
        break;
    case IDC_SHOW:
        if (HIWORD(w) == CBN_SELCHANGE) {
            int i = (int)SendMessageW(g_show, CB_GETCURSEL, 0, 0);
            if (i >= 0 && i < g_nshow) { g_showMode = g_showModes[i]; rebuild_view(); }
        }
        break;
    case IDC_CHOOSE: case IDM_CHOOSE:       do_choose(); break;
    case IDC_ASSIGN: case IDM_ASSIGN:       do_assign(); break;
    case IDC_CHECKALL: case IDM_CHECKALL:   mark_visible(TRUE); break;
    case IDC_CHECKNONE: case IDM_CHECKNONE: mark_visible(FALSE); break;
    case IDC_RESET: case IDM_RESET:         do_reset(); break;
    case IDC_UNDO: case IDM_UNDO:           do_undo(); break;
    case IDM_CHECKSUPPORTED:                do_check_supported(); break;
    case IDM_CHECKSEL:                      mark_selected(TRUE); break;
    case IDM_UNCHECKSEL:                    mark_selected(FALSE); break;
    case IDM_ADD:                           do_add_types(); break;
    case IDM_EXPORT:                        do_export(FALSE); break;
    case IDM_EXPORT_XML:                    do_export(TRUE); break;
    case IDM_RESTORE:                       do_restore(); break;
    case IDM_BACKUPS:                       open_backups(); break;
    case IDM_EXIT:                          DestroyWindow(g_main); break;
    case IDM_FIND:                          SetFocus(g_filter); SendMessageW(g_filter, EM_SETSEL, 0, -1); break;
    case IDM_REFRESH:
        g_appsLoaded = FALSE;
        apps_free(&g_apps);
        uc_probe(&g_env);
        show_env();
        reload();
        PostMessageW(g_main, WM_APP_APPS, 0, 0);
        break;
    case IDM_SETTINGS: case IDM_TYPESETTINGS: open_default_apps_settings(cur_app()); break;
    case IDM_SELFTEST:                      do_selftest(); break;
    case IDM_REPORT:                        do_report(); break;
    case IDM_NOTIFY:                        uc_notify(); set_status(L"Asked Explorer to refresh icons and associations."); break;
    case IDM_ABOUT:                         do_about(); break;
    case IDM_COPYPROG: {
        Fmt *f = view_fmt(ListView_GetNextItem(g_list, -1, LVNI_SELECTED));
        if (f && fmt_progid(f)[0]) clipboard_set(g_main, fmt_progid(f));
        break; }
    case IDM_REGEDIT: {
        Fmt *f = view_fmt(ListView_GetNextItem(g_list, -1, LVNI_SELECTED));
        if (f) {
            wchar_t p[500];
            wf(p, 500, L"Computer\\HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%ls", f->ext);
            regedit_jump(p);
        }
        break; }
    }
}

static void on_dpi_changed(WPARAM w, LPARAM l)
{
    g_dpi = LOWORD(w);
    make_fonts((UINT)g_dpi);
    EnumChildWindows(g_main, font_cb, (LPARAM)g_font);
    SendMessageW(g_appname, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
    const RECT *r = (const RECT *)l;
    SetWindowPos(g_main, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    for (int i = 0; i < NCOLS; i++) ListView_SetColumnWidth(g_list, i, S(COLS[i].w));
    layout();
    update_app_card();
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        on_create(h, ((CREATESTRUCTW *)l)->lpCreateParams != NULL);
        return 0;
    case WM_APP_LOAD:
        reload();
        PostMessageW(h, WM_APP_APPS, 0, 0);
        return 0;
    case WM_APP_APPS:
        ensure_apps();
        update_app_card();
        update_status_idle();
        return 0;
    case WM_SIZE:
        if (g_status && w != SIZE_MINIMIZED) layout();
        return 0;
    case WM_DPICHANGED:
        on_dpi_changed(w, l);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = S(960); mm->ptMinTrackSize.y = S(560);
        return 0; }
    case WM_NOTIFY:
        return on_notify((NMHDR *)l);
    case WM_COMMAND:
        on_command(w);
        return 0;
    case WM_TIMER:
        if (w == TIMER_FILTER) { KillTimer(h, TIMER_FILTER); rebuild_view(); }
        return 0;
    case WM_CTLCOLORSTATIC:
        if ((HWND)l == g_apppath || (HWND)l == g_det2) {
            SetTextColor((HDC)w, GetSysColor(COLOR_GRAYTEXT));
            SetBkMode((HDC)w, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    case WM_DROPFILES: {
        wchar_t f[MAX_PATH];
        App a;
        if (DragQueryFileW((HDROP)w, 0, f, MAX_PATH) && app_from_exe(f, &a)) {
            ensure_apps();
            App *added = apps_add(&g_apps, &a);
            if (added) set_app(added->exe);
            app_free(&a);
        } else set_status(L"Drop a program (.exe) or a shortcut to one to choose it.");
        DragFinish((HDROP)w);
        return 0; }
    case WM_CONTEXTMENU:
        if ((HWND)w == g_list) {
            POINT pt = { (short)LOWORD(l), (short)HIWORD(l) };
            if (l == -1) {                                           /* keyboard: Shift+F10 / menu key */
                RECT rc; int r = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
                GetCursorPos(&pt);
                if (r >= 0 && ListView_GetItemRect(g_list, r, &rc, LVIR_LABEL)) { pt.x = rc.left; pt.y = rc.bottom; ClientToScreen(g_list, &pt); }
            } else {
                LVHITTESTINFO ht = { 0 };
                ht.pt = pt;
                ScreenToClient(g_list, &ht.pt);
                ListView_HitTest(g_list, &ht);
                if (ht.iItem >= 0 && !(ListView_GetItemState(g_list, ht.iItem, LVIS_SELECTED) & LVIS_SELECTED)) {
                    ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED);
                    ListView_SetItemState(g_list, ht.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                }
            }
            show_popup(pt);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: {
        settings_save();
        HICON old = (HICON)SendMessageW(g_appicon, STM_SETICON, 0, 0);
        if (old) DestroyIcon(old);
        PostQuitMessage(0);
        return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}

/* Keep Ctrl+A / Ctrl+Z for the search box while it has the focus. */
static BOOL edit_owns_key(const MSG *msg)
{
    if (msg->message != WM_KEYDOWN || !(GetKeyState(VK_CONTROL) & 0x8000)) return FALSE;
    if (msg->wParam != 'A' && msg->wParam != 'Z') return FALSE;
    wchar_t cls[16];
    HWND f = GetFocus();
    return f && GetClassNameW(f, cls, 16) && !_wcsicmp(cls, L"Edit");
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cmdline, int show)
{
    (void)hp; (void)cmdline;
    g_inst = hi;
    HDC dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX ic = { sizeof ic, ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&ic);
    BOOL haveCfg = settings_load();

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon   = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.lpszClassName = L"FamMain";
    RegisterClassExW(&wc);
    dialog_init(hi);

    static ACCEL acc[] = {
        { FVIRTKEY | FCONTROL, 'A', IDM_CHECKALL },        { FVIRTKEY | FCONTROL | FSHIFT, 'A', IDM_CHECKNONE },
        { FVIRTKEY | FCONTROL, 'O', IDM_CHOOSE },          { FVIRTKEY | FCONTROL, VK_RETURN, IDM_ASSIGN },
        { FVIRTKEY | FCONTROL, 'Z', IDM_UNDO },            { FVIRTKEY | FCONTROL, 'N', IDM_ADD },
        { FVIRTKEY | FCONTROL, 'E', IDM_EXPORT },          { FVIRTKEY | FCONTROL, 'F', IDM_FIND },
        { FVIRTKEY, VK_F5, IDM_REFRESH },
    };
    HACCEL haccel = CreateAcceleratorTableW(acc, (int)(sizeof acc / sizeof acc[0]));

    int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = S(1200), h = S(720);
    if (haveCfg) { x = g_cfg.rc.left; y = g_cfg.rc.top; w = g_cfg.rc.right - g_cfg.rc.left; h = g_cfg.rc.bottom - g_cfg.rc.top; }
    HWND win = CreateWindowExW(0, L"FamMain", APP_TITLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, w, h,
                               NULL, NULL, hi, haveCfg ? (LPVOID)&g_cfg : NULL);
    if (!win) return 1;
    ShowWindow(win, (haveCfg && g_cfg.maximized) ? SW_SHOWMAXIMIZED : show);
    UpdateWindow(win);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!edit_owns_key(&msg) && TranslateAcceleratorW(win, haccel, &msg)) continue;
        if (IsDialogMessageW(win, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return (int)msg.wParam;
}
