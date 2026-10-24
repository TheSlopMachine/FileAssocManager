#include "common.h"
#include "registry.h"
#include "icons.h"

/* ------------------------------------------------------------------ the list */
static Fmt *list_add(FmtList *l, const wchar_t *ext)
{
    if (l->n == l->cap) {
        int nc = l->cap ? l->cap * 2 : 1024;
        Fmt *n = (Fmt *)realloc(l->v, (size_t)nc * sizeof *n);
        if (!n) return NULL;
        l->v = n; l->cap = nc;
    }
    Fmt *f = &l->v[l->n++];
    memset(f, 0, sizeof *f);
    str_copy(f->ext, 40, ext);
    str_lower(f->ext);
    f->cat = CAT_OTHER;
    f->icon = -1;
    if (fmt_is_blocked(f->ext)) f->flags |= FF_BLOCKED;
    if (uc_maybe_protected(f->ext)) f->flags |= FF_PROTECTED;
    return f;
}

int fmts_find(const FmtList *l, const wchar_t *ext)
{
    for (int i = 0; i < l->n; i++) if (!_wcsicmp(l->v[i].ext, ext)) return i;
    return -1;
}

const wchar_t *fmt_progid(const Fmt *f) { return f->user[0] ? f->user : f->sys; }

void fmt_refresh(Fmt *f, const UcEnv *e)
{
    uc_get(e, f->ext, f->user, COUNTOF(f->user));
    f->sys[0] = 0;
    rd_str(HKEY_CLASSES_ROOT, f->ext, NULL, f->sys, COUNTOF(f->sys));
    f->pi = prog_info(fmt_progid(f));
    f->icon = -1;                                   /* the type's icon follows its handler */
}

void fmts_refresh_all(FmtList *l, const UcEnv *e)
{
    for (int i = 0; i < l->n; i++) fmt_refresh(&l->v[i], e);
}

const wchar_t *fmt_desc(Fmt *f)
{
    if (f->desc) return f->desc;
    if (!f->sdescDone) {
        f->sdescDone = TRUE;
        const wchar_t *pid = fmt_progid(f);
        if (pid[0]) rd_str(HKEY_CLASSES_ROOT, pid, NULL, f->sdesc, COUNTOF(f->sdesc));
        if (!f->sdesc[0]) {
            wchar_t up[40];
            str_copy(up, 40, f->ext + 1);
            for (wchar_t *p = up; *p; p++) *p = (wchar_t)towupper(*p);
            wf(f->sdesc, COUNTOF(f->sdesc), L"%ls file", up);
        }
    }
    return f->sdesc;
}

/* Extensions the registry knows that the catalogue doesn't (from HKLM and HKCU classes). */
static void scan_classes(FmtList *l, HKEY root, const wchar_t *path)
{
    HKEY k;
    if (RegOpenKeyExW(root, path, 0, KEY_ENUMERATE_SUB_KEYS | WOW, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0;; i++) {
        wchar_t name[100], norm[100], pt[40], tmp[16];
        DWORD n = 100;
        LONG r = RegEnumKeyExW(k, i, name, &n, NULL, NULL, NULL, NULL);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS || name[0] != L'.') continue;
        str_lower(name);
        ext_normalize(name, norm, 100);
        if (!norm[0] || wcscmp(norm, name) != 0 || fmt_is_blocked(name)) continue;
        if (fmts_find(l, name) >= 0) continue;

        pt[0] = 0;
        rd_str(HKEY_CLASSES_ROOT, name, L"PerceivedType", pt, 40);
        if (!_wcsicmp(pt, L"application") || !_wcsicmp(pt, L"system")) continue;
        wchar_t sub[140];
        BOOL relevant = pt[0] || rd_str(HKEY_CLASSES_ROOT, name, NULL, tmp, 16);
        if (!relevant) { wf(sub, 140, L"%ls\\OpenWithProgids", name); relevant = key_exists(HKEY_CLASSES_ROOT, sub); }
        if (!relevant) continue;

        Fmt *f = list_add(l, name);
        if (!f) break;
        int c = cat_from_perceived(pt);
        f->cat = c >= 0 ? c : CAT_OTHER;
    }
    RegCloseKey(k);
}

void fmts_load(FmtList *l, const UcEnv *e)
{
    memset(l, 0, sizeof *l);
    for (int i = 0; i < FMT_DEF_COUNT; i++) {
        Fmt *f = list_add(l, FMT_DEFS[i].ext);
        if (!f) break;
        f->desc = FMT_DEFS[i].desc;
        f->cat = FMT_DEFS[i].cat;
        f->flags |= FF_CATALOG;
    }
    scan_classes(l, HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes");
    scan_classes(l, HKEY_CURRENT_USER, L"Software\\Classes");
    fmts_refresh_all(l, e);
}

void fmts_free(FmtList *l)
{
    free(l->v);
    memset(l, 0, sizeof *l);
}

int fmts_add_custom(FmtList *l, const UcEnv *e, const wchar_t *text)
{
    wchar_t ext[80];
    ext_normalize(text, ext, 80);
    if (!ext[0]) return -1;
    int i = fmts_find(l, ext);
    if (i >= 0) return i;
    if (fmt_is_blocked(ext)) return -2;
    Fmt *f = list_add(l, ext);
    if (!f) return -1;
    f->flags |= FF_CUSTOM;
    fmt_refresh(f, e);
    return l->n - 1;
}

/* ------------------------------------------------------------- batch results */
void result_add(AssignResult *r, int fmt, int outcome, LONG err)
{
    if (r->n == r->cap) {
        int nc = r->cap ? r->cap * 2 : 64;
        AssignItem *n = (AssignItem *)realloc(r->items, (size_t)nc * sizeof *n);
        if (!n) return;
        r->items = n; r->cap = nc;
    }
    r->items[r->n].fmt = fmt;
    r->items[r->n].outcome = outcome;
    r->items[r->n].err = err;
    r->n++;
    r->count[outcome]++;
}

void result_free(AssignResult *r) { free(r->items); memset(r, 0, sizeof *r); }

const wchar_t *outcome_text(int o)
{
    switch (o) {
    case AS_OK:        return L"Done";
    case AS_ALREADY:   return L"Already set";
    case AS_PROTECTED: return L"Protected by Windows";
    case AS_REJECTED:  return L"Windows did not accept it";
    case AS_BLOCKED:   return L"Never reassigned (program or shortcut)";
    case AS_NOHANDLER: return L"This application can't open this type";
    case AS_FAILED:    return L"Failed";
    default:           return L"Not attempted";
    }
}

/* -------------------------------------------------------------------- assign */
/* TRUE only if the shell's own answer is the program we just set. A failed query is NOT a pass. */
static BOOL verify_default(const wchar_t *ext, const wchar_t *prog)
{
    wchar_t eff[260];
    return uc_effective(ext, eff, COUNTOF(eff)) && !_wcsicmp(eff, prog);
}

void assign_run(FmtList *l, const UcEnv *e, const App *app, const int *idx, int n,
                AssignResult *r, ProgressFn pf, void *ctx)
{
    int streak = 0;
    memset(r, 0, sizeof *r);
    for (int k = 0; k < n; k++) {
        Fmt *f = &l->v[idx[k]];
        wchar_t prog[260], before[260];
        LONG err;
        if (pf) pf(k, n, f->ext, ctx);
        if (r->aborted) { result_add(r, idx[k], AS_SKIPPED, 0); continue; }
        if (f->flags & FF_BLOCKED) { result_add(r, idx[k], AS_BLOCKED, FAM_E_BLOCKED); continue; }

        err = app_progid_for(app, f->ext, prog, COUNTOF(prog));
        if (err != ERROR_SUCCESS) { result_add(r, idx[k], err == FAM_E_NOHANDLER ? AS_NOHANDLER : AS_FAILED, err); continue; }

        if (f->user[0] && !_wcsicmp(f->user, prog) && verify_default(f->ext, prog)) {
            result_add(r, idx[k], AS_ALREADY, 0);
            continue;
        }

        wcscpy(before, f->user);                         /* what to put back if Windows refuses */
        err = uc_set(e, f->ext, prog);
        if (err == ERROR_ACCESS_DENIED) { result_add(r, idx[k], AS_PROTECTED, err); continue; }
        if (err == ERROR_SUCCESS && !verify_default(f->ext, prog))
            err = uc_set(e, f->ext, prog) == ERROR_SUCCESS ? ERROR_SUCCESS : ERROR_GEN_FAILURE;   /* once more: timing */
        if (err == ERROR_SUCCESS && verify_default(f->ext, prog)) {
            streak = 0;
            fmt_refresh(f, e);
            result_add(r, idx[k], AS_OK, 0);
            continue;
        }

        /* Windows wrote the entry but didn't honour it (or the write failed): restore the previous choice. */
        if (before[0]) uc_set(e, f->ext, before); else uc_clear(f->ext);
        fmt_refresh(f, e);
        if (err == ERROR_SUCCESS || err == ERROR_GEN_FAILURE) {
            result_add(r, idx[k], AS_REJECTED, FAM_E_REJECTED);
            if (++streak >= 2 && r->count[AS_OK] == 0) r->aborted = TRUE;   /* systematic: stop early */
        } else {
            if (err == FAM_E_CANT_WRITE) r->aborted = TRUE;
            result_add(r, idx[k], AS_FAILED, err);
        }
    }
    if (pf) pf(n, n, L"", ctx);
    if (r->count[AS_OK]) uc_notify();
}

void reset_run(FmtList *l, const UcEnv *e, const int *idx, int n, AssignResult *r, ProgressFn pf, void *ctx)
{
    memset(r, 0, sizeof *r);
    for (int k = 0; k < n; k++) {
        Fmt *f = &l->v[idx[k]];
        if (pf) pf(k, n, f->ext, ctx);
        if (!f->user[0]) { result_add(r, idx[k], AS_ALREADY, 0); continue; }
        LONG err = uc_clear(f->ext);
        fmt_refresh(f, e);
        if (err == ERROR_ACCESS_DENIED) result_add(r, idx[k], AS_PROTECTED, err);
        else if (err) result_add(r, idx[k], AS_FAILED, err);
        else result_add(r, idx[k], AS_OK, 0);
    }
    if (pf) pf(n, n, L"", ctx);
    if (r->count[AS_OK]) uc_notify();
}
