#include "common.h"
#include "dialog.h"
#include "icons.h"

enum { D_LABEL = 300, D_SEARCH, D_LIST, D_BROWSE, D_SETTINGS, D_COPY, D_EDIT, D_SUMMARY };
enum { K_PICK = 1, K_REPORT, K_ADD, K_TEXT };

#define PICK_MAX_MARKED 500
#define KEYS_CCH        1000

typedef struct {
    int  kind;
    BOOL done, ok;
    /* picker */
    AppList *apps;
    const FmtList *fl;
    const wchar_t (*recent)[MAX_PATH];
    int  nrecent;
    wchar_t *exeOut;
    wchar_t (*keys)[KEYS_CCH];
    int  nkeys, nmarked, *counts, *ord, nord, sortMode;
    /* report */
    const wchar_t *title, *summary;
    const AssignResult *res;
    const App *app;
    /* text / add */
    const wchar_t *intro, *text;
    wchar_t *buf;
    int  bufCch;
} Dlg;

static void lv_col(HWND lv, int i, const wchar_t *text, int w, int fmt)
{
    LVCOLUMNW c = { 0 };
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    c.pszText = (wchar_t *)text; c.cx = S(w); c.fmt = fmt;
    SendMessageW(lv, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
}

static HWND make_list(HWND h, int x, int y, int w, int hgt)
{
    HWND lv = ctl(h, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS,
                  x, y, w, hgt, D_LIST);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    SetWindowTheme(lv, L"Explorer", NULL);
    ListView_SetImageList(lv, sys_imagelist(), LVSIL_SMALL);
    return lv;
}

static void set_sub(HWND lv, int row, int col, const wchar_t *t)
{
    LVITEMW it = { 0 };
    it.iSubItem = col; it.pszText = (wchar_t *)t;
    SendMessageW(lv, LVM_SETITEMTEXTW, (WPARAM)row, (LPARAM)&it);
}

static void url_encode(const wchar_t *s, wchar_t *out, size_t cch)
{
    size_t o = 0;
    for (; *s && o + 4 < cch; s++) {
        if (iswalnum(*s) && *s < 128) out[o++] = *s;
        else if (*s == L'-' || *s == L'_' || *s == L'.') out[o++] = *s;
        else if (*s < 128) o += (size_t)swprintf(out + o, cch - o, L"%%%02X", (unsigned)*s);
        else out[o++] = *s;
    }
    out[o] = 0;
}

void open_default_apps_settings(const App *a)
{
    wchar_t url[600], enc[400];
    if (a && a->regName[0]) {
        url_encode(a->regName, enc, 400);
        wf(url, 600, L"ms-settings:defaultapps?registeredApp%ls=%ls", a->regUser ? L"User" : L"Machine", enc);
    } else str_copy(url, 600, L"ms-settings:defaultapps");
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

/* ---------------------------------------------------------------- app picker */
static const int *cmp_counts;
static const AppList *cmp_apps;

static int cmp_by_count(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    if (cmp_counts[x] != cmp_counts[y]) return cmp_counts[y] - cmp_counts[x];
    return _wcsicmp(cmp_apps->v[x].name, cmp_apps->v[y].name);
}

static void pick_prepare(Dlg *d)
{
    d->keys = (wchar_t (*)[KEYS_CCH])calloc(PICK_MAX_MARKED, sizeof *d->keys);
    d->counts = (int *)calloc((size_t)(d->apps->n ? d->apps->n : 1), sizeof(int));
    d->ord = (int *)malloc((size_t)(d->apps->n ? d->apps->n : 1) * sizeof(int));
    for (int i = 0; i < d->fl->n; i++) {
        const Fmt *f = &d->fl->v[i];
        if (!f->marked) continue;
        d->nmarked++;
        if (d->nkeys < PICK_MAX_MARKED && d->keys) ext_handlers(d->apps, f->ext, d->keys[d->nkeys++], KEYS_CCH);
    }
    for (int a = 0; a < d->apps->n && d->counts; a++)
        for (int k = 0; k < d->nkeys; k++)
            if (ext_handled_by(d->keys[k], &d->apps->v[a])) d->counts[a]++;
}

static void pick_fill(HWND h, Dlg *d)
{
    HWND lv = GetDlgItem(h, D_LIST);
    wchar_t q[100], t[64];
    GetDlgItemTextW(h, D_SEARCH, q, 100);
    StrTrimW(q, L" \t");
    SendMessageW(lv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(lv);
    if (!d->ord || !d->counts) return;

    int n = 0;
    char *used = (char *)calloc((size_t)(d->apps->n ? d->apps->n : 1), 1);
    if (!used) return;
    if (d->sortMode == 0) {                                   /* recent first, then alphabetical */
        for (int r = 0; r < d->nrecent; r++) {
            int i = apps_find(d->apps, d->recent[r]);
            if (i >= 0 && !used[i]) { used[i] = 1; d->ord[n++] = i; }
        }
        for (int i = 0; i < d->apps->n; i++) if (!used[i]) d->ord[n++] = i;
    } else {
        for (int i = 0; i < d->apps->n; i++) d->ord[n++] = i;
        cmp_counts = d->counts; cmp_apps = d->apps;
        qsort(d->ord, (size_t)n, sizeof(int), cmp_by_count);
    }
    free(used);

    int row = 0;
    d->nord = 0;
    for (int k = 0; k < n; k++) {
        int a = d->ord[k];
        App *ap = &d->apps->v[a];
        if (q[0] && !StrStrIW(ap->name, q) && !StrStrIW(ap->exe, q)) continue;
        LVITEMW it = { 0 };
        it.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
        it.iItem = row; it.pszText = ap->name; it.iImage = app_icon(ap); it.lParam = a;
        SendMessageW(lv, LVM_INSERTITEMW, 0, (LPARAM)&it);
        int col = 1;
        if (d->nmarked) {
            if (d->counts[a] == 0) t[0] = 0;
            else wf(t, 64, d->counts[a] >= d->nkeys ? L"All %d" : L"%d of %d", d->counts[a], d->nkeys);
            set_sub(lv, row, col++, t);
        }
        set_sub(lv, row, col, ap->exe);
        row++;
        d->ord[d->nord++] = a;
    }
    SendMessageW(lv, WM_SETREDRAW, TRUE, 0);
    if (row) ListView_SetItemState(lv, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    EnableWindow(GetDlgItem(h, IDOK), row > 0);
}

static void pick_build(HWND h, Dlg *d)
{
    wchar_t t[200];
    pick_prepare(d);
    if (d->nmarked) wf(t, 200, L"Choose the application that should open the %d format%ls you marked.", d->nmarked, d->nmarked == 1 ? L"" : L"s");
    else str_copy(t, 200, L"Choose the application that should open the formats you select.");
    ctl(h, 0, L"STATIC", t, 0, 12, 12, 616, 20, D_LABEL);
    HWND se = ctl(h, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 12, 38, 616, 24, D_SEARCH);
    SendMessageW(se, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search applications");
    HWND lv = make_list(h, 12, 70, 616, 336);
    lv_col(lv, 0, L"Application", 230, LVCFMT_LEFT);
    int c = 1;
    if (d->nmarked) lv_col(lv, c++, L"Opens your marked formats", 150, LVCFMT_LEFT);
    lv_col(lv, c, L"Location", 232, LVCFMT_LEFT);
    ctl(h, 0, L"BUTTON", L"Browse" U_ELLIPSIS, WS_TABSTOP, 12, 420, 110, 28, D_BROWSE);
    ctl(h, 0, L"BUTTON", L"Choose", WS_TABSTOP | BS_DEFPUSHBUTTON, 424, 420, 100, 28, IDOK);
    ctl(h, 0, L"BUTTON", L"Cancel", WS_TABSTOP, 530, 420, 98, 28, IDCANCEL);
    pick_fill(h, d);
}

static BOOL pick_accept(HWND h, Dlg *d)
{
    HWND lv = GetDlgItem(h, D_LIST);
    int i = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
    if (i < 0) return FALSE;
    LVITEMW it = { 0 };
    it.mask = LVIF_PARAM; it.iItem = i;
    SendMessageW(lv, LVM_GETITEMW, 0, (LPARAM)&it);
    str_copy(d->exeOut, MAX_PATH, d->apps->v[it.lParam].exe);
    d->ok = d->done = TRUE;
    return TRUE;
}

static void pick_browse(HWND h, Dlg *d)
{
    wchar_t f[MAX_PATH] = L"";
    App a;
    if (!browse_file(h, f, L"Programs and shortcuts\0*.exe;*.lnk\0All files\0*.*\0", FALSE, NULL)) return;
    if (!app_from_exe(f, &a)) {
        MessageBoxW(h, L"That isn't a program this tool can use. Choose an .exe file (or a shortcut to one).", APP_TITLE, MB_ICONINFORMATION);
        return;
    }
    App *added = apps_add(d->apps, &a);
    app_free(&a);
    if (!added) return;
    str_copy(d->exeOut, MAX_PATH, added->exe);
    d->ok = d->done = TRUE;
}

/* -------------------------------------------------------------------- report */
static void outcome_detail(const AssignItem *it, const App *app, wchar_t *out, size_t cch)
{
    switch (it->outcome) {
    case AS_PROTECTED: str_copy(out, cch, L"Windows only lets you change this type in Settings."); break;
    case AS_REJECTED:  str_copy(out, cch, L"The new default was ignored; the previous one is in place."); break;
    case AS_NOHANDLER: wf(out, cch, L"%ls can't be registered for this type.", app ? app->name : L"The application"); break;
    case AS_FAILED:    win_error_text(it->err, out, cch); break;
    case AS_BLOCKED:   str_copy(out, cch, L"Programs and shortcuts are never reassigned."); break;
    case AS_SKIPPED:   str_copy(out, cch, L"Stopped after repeated refusals."); break;
    default:           out[0] = 0;
    }
}

static BOOL shown(int o) { return o != AS_OK && o != AS_ALREADY; }

static void report_build(HWND h, Dlg *d)
{
    ctl(h, 0, L"STATIC", d->summary, SS_LEFT, 12, 12, 616, 44, D_SUMMARY);
    HWND lv = make_list(h, 12, 62, 616, 300);
    lv_col(lv, 0, L"Format", 90, LVCFMT_LEFT);
    lv_col(lv, 1, L"Result", 200, LVCFMT_LEFT);
    lv_col(lv, 2, L"What to do", 310, LVCFMT_LEFT);
    int row = 0;
    BOOL settings = FALSE;
    for (int i = 0; i < d->res->n; i++) {
        const AssignItem *it = &d->res->items[i];
        if (!shown(it->outcome)) continue;
        wchar_t t[300];
        LVITEMW li = { 0 };
        li.mask = LVIF_TEXT | LVIF_IMAGE;
        li.iItem = row; li.pszText = (wchar_t *)d->fl->v[it->fmt].ext; li.iImage = icon_for_ext(d->fl->v[it->fmt].ext);
        SendMessageW(lv, LVM_INSERTITEMW, 0, (LPARAM)&li);
        set_sub(lv, row, 1, outcome_text(it->outcome));
        outcome_detail(it, d->app, t, 300);
        set_sub(lv, row, 2, t);
        row++;
        if (it->outcome == AS_PROTECTED || it->outcome == AS_REJECTED) settings = TRUE;
    }
    if (settings) ctl(h, 0, L"BUTTON", L"Open Windows Settings", WS_TABSTOP, 12, 374, 170, 28, D_SETTINGS);
    ctl(h, 0, L"BUTTON", L"Copy details", WS_TABSTOP, settings ? 190 : 12, 374, 110, 28, D_COPY);
    ctl(h, 0, L"BUTTON", L"Close", WS_TABSTOP | BS_DEFPUSHBUTTON, 530, 374, 98, 28, IDOK);
}

static void report_copy(HWND h, Dlg *d)
{
    size_t cap = 256 + (size_t)d->res->n * 400;
    wchar_t *buf = (wchar_t *)malloc(cap * sizeof(wchar_t)), t[300], line[500];
    if (!buf) return;
    buf[0] = 0;
    for (int i = 0; i < d->res->n; i++) {
        const AssignItem *it = &d->res->items[i];
        if (!shown(it->outcome)) continue;
        outcome_detail(it, d->app, t, 300);
        wf(line, 500, L"%ls\t%ls\t%ls\r\n", d->fl->v[it->fmt].ext, outcome_text(it->outcome), t);
        wcsncat(buf, line, cap - wcslen(buf) - 1);
    }
    clipboard_set(h, buf);
    free(buf);
}

/* ------------------------------------------------------------ add / text */
static void add_build(HWND h, Dlg *d)
{
    ctl(h, 0, L"STATIC", d->intro, 0, 12, 12, 456, 54, D_LABEL);
    ctl(h, WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL, 12, 70, 456, 100, D_EDIT);
    ctl(h, 0, L"BUTTON", L"Add and mark", WS_TABSTOP | BS_DEFPUSHBUTTON, 258, 184, 120, 28, IDOK);
    ctl(h, 0, L"BUTTON", L"Cancel", WS_TABSTOP, 384, 184, 84, 28, IDCANCEL);
}

static void text_build(HWND h, Dlg *d)
{
    ctl(h, 0, L"STATIC", d->intro, 0, 12, 12, 576, 36, D_LABEL);
    ctl(h, WS_EX_CLIENTEDGE, L"EDIT", d->text, WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 12, 52, 576, 300, D_EDIT);
    ctl(h, 0, L"BUTTON", L"Copy", WS_TABSTOP, 12, 366, 90, 28, D_COPY);
    ctl(h, 0, L"BUTTON", L"Close", WS_TABSTOP | BS_DEFPUSHBUTTON, 490, 366, 98, 28, IDOK);
}

/* --------------------------------------------------------------- window proc */
static LRESULT CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    Dlg *d = (Dlg *)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_CREATE:
        d = (Dlg *)((CREATESTRUCTW *)l)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        switch (d->kind) {
        case K_PICK:   pick_build(h, d); break;
        case K_REPORT: report_build(h, d); break;
        case K_ADD:    add_build(h, d); break;
        case K_TEXT:   text_build(h, d); break;
        }
        EnumChildWindows(h, font_cb, (LPARAM)g_font);
        return 0;
    case WM_SHOWWINDOW:
        if (w && d) {
            HWND f = d->kind == K_PICK ? GetDlgItem(h, D_SEARCH) : d->kind == K_ADD ? GetDlgItem(h, D_EDIT) : GetDlgItem(h, IDOK);
            if (f) SetFocus(f);
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w);
        if (!d) break;
        if (id == IDCANCEL) { d->done = TRUE; return 0; }
        if (d->kind == K_PICK) {
            if (id == IDOK) { pick_accept(h, d); return 0; }
            if (id == D_BROWSE) { pick_browse(h, d); return 0; }
            if (id == D_SEARCH && HIWORD(w) == EN_CHANGE) { pick_fill(h, d); return 0; }
        } else if (d->kind == K_REPORT) {
            if (id == IDOK) { d->done = TRUE; return 0; }
            if (id == D_SETTINGS) { open_default_apps_settings(d->app); return 0; }
            if (id == D_COPY) { report_copy(h, d); return 0; }
        } else if (d->kind == K_ADD) {
            if (id == IDOK) {
                GetDlgItemTextW(h, D_EDIT, d->buf, d->bufCch);
                d->ok = d->buf[0] != 0;
                d->done = TRUE;
                return 0;
            }
        } else if (d->kind == K_TEXT) {
            if (id == IDOK) { d->done = TRUE; return 0; }
            if (id == D_COPY) { clipboard_set(h, d->text); return 0; }
        }
        break; }
    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)l;
        if (!d || d->kind != K_PICK || nm->idFrom != D_LIST) break;
        if (nm->code == NM_DBLCLK) { pick_accept(h, d); return 0; }
        if (nm->code == LVN_ITEMCHANGED) EnableWindow(GetDlgItem(h, IDOK), ListView_GetNextItem(nm->hwndFrom, -1, LVNI_SELECTED) >= 0);
        if (nm->code == LVN_COLUMNCLICK && d->nmarked) {
            int col = ((NMLISTVIEW *)l)->iSubItem;
            int mode = col == 1 ? 1 : (col == 0 ? 0 : d->sortMode);
            if (mode != d->sortMode) { d->sortMode = mode; pick_fill(h, d); }
            return 0;
        }
        break; }
    case WM_CLOSE:
        if (d) d->done = TRUE;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

BOOL dialog_init(HINSTANCE inst)
{
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"FamDlg";
    return RegisterClassExW(&wc) != 0;
}

/* Centered over `parent`, parent disabled until it closes. cw/ch are 96-dpi client sizes. */
static void run_modal(HWND parent, Dlg *d, const wchar_t *title, int cw, int ch)
{
    DWORD st = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT r = { 0, 0, S(cw), S(ch) }, pr;
    AdjustWindowRectEx(&r, st, FALSE, WS_EX_DLGMODALFRAME);
    GetWindowRect(parent, &pr);
    int w = r.right - r.left, h = r.bottom - r.top;
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FamDlg", title, st | WS_VISIBLE,
                               pr.left + ((pr.right - pr.left) - w) / 2, pr.top + ((pr.bottom - pr.top) - h) / 2,
                               w, h, parent, NULL, g_inst, d);
    if (!dlg) return;
    EnableWindow(parent, FALSE);
    MSG msg = { 0 };
    int rc = 1;
    while (!d->done && (rc = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        /* Enter starts a new line in the multi-line box, so Ctrl+Enter confirms there. */
        if (d->kind == K_ADD && msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessageW(dlg, WM_COMMAND, IDOK, 0);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(dlg);
    SetForegroundWindow(parent);
    if (rc == 0) PostQuitMessage((int)msg.wParam);
}

BOOL pick_app_dialog(HWND parent, AppList *apps, const FmtList *fl, const wchar_t (*recent)[MAX_PATH],
                     int nrecent, wchar_t *exeOut)
{
    Dlg d;
    memset(&d, 0, sizeof d);
    d.kind = K_PICK; d.apps = apps; d.fl = fl; d.recent = recent; d.nrecent = nrecent; d.exeOut = exeOut;
    exeOut[0] = 0;
    run_modal(parent, &d, L"Choose application", 640, 462);
    free(d.keys); free(d.counts); free(d.ord);
    return d.ok && exeOut[0];
}

void report_dialog(HWND parent, const wchar_t *title, const wchar_t *summary, const FmtList *fl,
                   const AssignResult *r, const App *app)
{
    Dlg d;
    memset(&d, 0, sizeof d);
    d.kind = K_REPORT; d.title = title; d.summary = summary; d.fl = fl; d.res = r; d.app = app;
    run_modal(parent, &d, title, 640, 416);
}

BOOL add_formats_dialog(HWND parent, wchar_t *text, int cch)
{
    Dlg d;
    memset(&d, 0, sizeof d);
    d.kind = K_ADD; d.buf = text; d.bufCch = cch;
    d.intro = L"Type the file extensions you want to add to the list, separated by spaces, commas or new lines "
              L"(for example: .heic .webp .jxl). They are marked for you so you can assign them straight away. "
              L"Press Ctrl+Enter when you are done.";
    text[0] = 0;
    run_modal(parent, &d, L"Add file types", 480, 224);
    return d.ok;
}

void text_dialog(HWND parent, const wchar_t *title, const wchar_t *intro, const wchar_t *text)
{
    Dlg d;
    memset(&d, 0, sizeof d);
    d.kind = K_TEXT; d.intro = intro; d.text = text;
    run_modal(parent, &d, title, 600, 408);
}
