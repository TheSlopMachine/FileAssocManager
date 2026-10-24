#include "common.h"
#include "backup.h"

#define MAGIC L"# FileAssocManager associations v1"

void backup_folder(wchar_t *out, size_t n) { data_dir(L"Backups", out, n); }

/* Appends to a malloc'd wide buffer that grows as needed. */
static BOOL app_w(wchar_t **buf, size_t *len, size_t *cap, const wchar_t *s)
{
    size_t l = wcslen(s);
    if (*len + l + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 4096;
        while (nc < *len + l + 1) nc *= 2;
        wchar_t *p = (wchar_t *)realloc(*buf, nc * sizeof(wchar_t));
        if (!p) return FALSE;
        *buf = p; *cap = nc;
    }
    memcpy(*buf + *len, s, (l + 1) * sizeof(wchar_t));
    *len += l;
    return TRUE;
}

static BOOL write_list(const FmtList *l, const int *idx, int n, const wchar_t *label, const wchar_t *path)
{
    wchar_t *buf = NULL, line[600];
    size_t len = 0, cap = 0;
    SYSTEMTIME t;
    GetLocalTime(&t);
    app_w(&buf, &len, &cap, MAGIC L"\r\n");
    wf(line, 600, L"# %04d-%02d-%02d %02d:%02d:%02d %ls\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, label ? label : L"");
    app_w(&buf, &len, &cap, line);
    for (int k = 0; k < n; k++) {
        const Fmt *f = &l->v[idx ? idx[k] : k];
        wf(line, 600, L"%ls\t%ls\r\n", f->ext, f->user);
        if (!app_w(&buf, &len, &cap, line)) { free(buf); return FALSE; }
    }
    BOOL ok = buf && write_text_file(path, buf, TRUE);
    free(buf);
    return ok;
}

BOOL backup_save(const FmtList *l, const int *idx, int n, const wchar_t *label, wchar_t *pathOut)
{
    wchar_t dir[MAX_PATH];
    SYSTEMTIME t;
    backup_folder(dir, MAX_PATH);
    GetLocalTime(&t);
    for (int i = 0; i < 100; i++) {
        if (i) wf(pathOut, MAX_PATH, L"%ls\\%04d%02d%02d-%02d%02d%02d-%d.fam", dir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, i);
        else   wf(pathOut, MAX_PATH, L"%ls\\%04d%02d%02d-%02d%02d%02d.fam", dir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        if (!file_exists(pathOut)) break;
    }
    return write_list(l, idx, n, label, pathOut);
}

BOOL backup_newest(wchar_t *path)
{
    wchar_t dir[MAX_PATH], pat[MAX_PATH + 8], best[MAX_PATH];
    WIN32_FIND_DATAW fd;
    best[0] = 0;
    backup_folder(dir, MAX_PATH);
    wf(pat, MAX_PATH + 8, L"%ls\\*.fam", dir);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    do { if (wcscmp(fd.cFileName, best) > 0) str_copy(best, MAX_PATH, fd.cFileName); } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!best[0]) return FALSE;
    wf(path, MAX_PATH, L"%ls\\%ls", dir, best);
    return TRUE;
}

void backup_mark_undone(const wchar_t *path)
{
    wchar_t to[MAX_PATH + 16];
    wf(to, MAX_PATH + 16, L"%ls.undone", path);
    DeleteFileW(to);
    MoveFileW(path, to);
}

BOOL backup_restore(FmtList *l, const UcEnv *e, const wchar_t *path, AssignResult *r, ProgressFn pf, void *ctx)
{
    wchar_t *text = NULL;
    memset(r, 0, sizeof *r);
    if (!read_text_file(path, &text)) return FALSE;
    if (wcsncmp(text, MAGIC, wcslen(MAGIC)) != 0) { free(text); return FALSE; }

    int total = 0, done = 0;
    for (const wchar_t *p = text; *p; ) {                      /* count the entries for the progress bar */
        if (*p == L'.') total++;
        const wchar_t *nl = wcschr(p, L'\n');
        if (!nl) break;
        p = nl + 1;
    }
    for (wchar_t *line = text; line && *line; ) {
        wchar_t *nl = wcschr(line, L'\n'), *tab;
        if (nl) *nl = 0;
        size_t ll = wcslen(line);
        if (ll && line[ll - 1] == L'\r') line[ll - 1] = 0;
        if (line[0] == L'.' && (tab = wcschr(line, L'\t')) != NULL) {
            *tab = 0;
            wchar_t *prog = tab + 1;
            StrTrimW(prog, L" \t");
            int i = fmts_add_custom(l, e, line);
            if (pf) pf(done, total, line, ctx);
            done++;
            if (i < 0) { result_add(r, 0, AS_BLOCKED, FAM_E_BLOCKED); }
            else {
                Fmt *f = &l->v[i];
                LONG err = prog[0] ? uc_set(e, f->ext, prog) : uc_clear(f->ext);
                wchar_t eff[260];
                if (err == ERROR_ACCESS_DENIED) result_add(r, i, AS_PROTECTED, err);
                else if (err) result_add(r, i, AS_FAILED, err);
                else if (prog[0] && uc_effective(f->ext, eff, 260) && _wcsicmp(eff, prog) != 0) result_add(r, i, AS_REJECTED, FAM_E_REJECTED);
                else result_add(r, i, AS_OK, 0);
                fmt_refresh(f, e);
            }
        }
        line = nl ? nl + 1 : NULL;
    }
    free(text);
    if (pf) pf(total, total, L"", ctx);
    if (r->count[AS_OK]) uc_notify();
    return r->n > 0;
}

BOOL backup_describe(const wchar_t *path, wchar_t *label, size_t cch, int *count)
{
    wchar_t *text = NULL;
    label[0] = 0;
    *count = 0;
    if (!read_text_file(path, &text)) return FALSE;
    if (wcsncmp(text, MAGIC, wcslen(MAGIC)) != 0) { free(text); return FALSE; }
    const wchar_t *p = wcschr(text, L'\n');
    if (p && p[1] == L'#') {
        str_copy(label, cch, p + 3);
        wchar_t *e = wcspbrk(label, L"\r\n");
        if (e) *e = 0;
    }
    for (p = text; p; p = wcschr(p, L'\n')) { if (*p == L'\n') p++; if (*p == L'.') (*count)++; if (!*p) break; }
    free(text);
    return TRUE;
}

BOOL backup_export(const FmtList *l, const wchar_t *path)
{
    int *idx = (int *)malloc((size_t)(l->n ? l->n : 1) * sizeof(int));
    int n = 0;
    if (!idx) return FALSE;
    for (int i = 0; i < l->n; i++) if (l->v[i].user[0]) idx[n++] = i;
    BOOL ok = write_list(l, idx, n, L"all choices", path);
    free(idx);
    return ok;
}

static void xml_esc(const wchar_t *s, wchar_t *out, size_t cch)
{
    size_t o = 0;
    for (; *s && o + 7 < cch; s++) {
        const wchar_t *r = *s == L'&' ? L"&amp;" : *s == L'<' ? L"&lt;" : *s == L'>' ? L"&gt;" : *s == L'"' ? L"&quot;" : NULL;
        if (r) { wcscpy(out + o, r); o += wcslen(r); } else out[o++] = *s;
    }
    out[o] = 0;
}

BOOL backup_export_xml(const FmtList *l, const wchar_t *path)
{
    wchar_t *buf = NULL, line[1200], a[400], b[400], c[400];
    size_t len = 0, cap = 0;
    app_w(&buf, &len, &cap, L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<DefaultAssociations>\r\n");
    for (int i = 0; i < l->n; i++) {
        const Fmt *f = &l->v[i];
        if (!f->user[0]) continue;
        xml_esc(f->ext, a, 400);
        xml_esc(f->user, b, 400);
        xml_esc(f->pi ? f->pi->name : L"", c, 400);
        wf(line, 1200, L"  <Association Identifier=\"%ls\" ProgId=\"%ls\" ApplicationName=\"%ls\" />\r\n", a, b, c);
        if (!app_w(&buf, &len, &cap, line)) { free(buf); return FALSE; }
    }
    app_w(&buf, &len, &cap, L"</DefaultAssociations>\r\n");
    BOOL ok = buf && write_text_file(path, buf, FALSE);
    free(buf);
    return ok;
}
