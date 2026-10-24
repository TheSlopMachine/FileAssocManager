#include "common.h"
#include "userchoice.h"
#include "uchash.h"
#include <sddl.h>
#include <aclapi.h>
#include <shobjidl.h>

#define FILEEXTS  L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts"
#define URLASSOC  L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations"
#define TOASTS    L"Software\\Microsoft\\Windows\\CurrentVersion\\ApplicationAssociationToasts"
#define OUR_PREFIX L"FileAssocManager."
#define EXPERIENCE_DEFAULT L"User Choice set via Windows User Experience {D18B6DD5-6124-4341-9318-804003BAFA0B}"

/* Salt strings of the UserChoiceLatest scheme, one per "machine class" (last character of the
 * SQM machine id, modulo 3). They are fixed strings inside shell32/windows.storage. */
static const wchar_t *LATEST_SALT[3] = {
    L"Copyright (C) Microsoft. All rights reserved {3822B7CA-C2F4-4889-B8CC-4CE39A8FB81C}",
    L"Copyright (C) Microsoft. All rights reserved {D185E0A1-E265-4724-AA21-3A17B038D72E}",
    L"Copyright (C) Microsoft. All rights reserved {97B6BCF4-C367-4577-95BE-73BD3053A5E0}",
};

/* ---------------------------------------------------------------- helpers */
static BOOL hash_w(BOOL latest, const wchar_t *s, wchar_t *out /* >= UC_HASH_CCH */)
{
    uint16_t buf[1024];
    size_t n = wcslen(s);
    char h[UC_HASH_CCH];
    out[0] = 0;
    if (n >= 1024) return FALSE;
    for (size_t i = 0; i < n; i++) buf[i] = (uint16_t)s[i];
    if (!(latest ? uc_hash_latest(buf, n, h) : uc_hash_legacy(buf, n, h))) return FALSE;
    for (int i = 0; h[i]; i++) out[i] = (wchar_t)h[i], out[i + 1] = 0;
    return TRUE;
}

static void hex_time(ULONGLONG ft, wchar_t *out)
{
    wf(out, 20, L"%08x%08x", (unsigned)(ft >> 32), (unsigned)(ft & 0xFFFFFFFFu));
}

static BOOL user_sid(wchar_t *out, size_t cch)
{
    HANDLE t; BYTE buf[512]; DWORD n; BOOL ok = FALSE;
    out[0] = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return FALSE;
    if (GetTokenInformation(t, TokenUser, buf, sizeof buf, &n)) {
        LPWSTR s = NULL;
        if (ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &s) && s) {
            str_copy(out, cch, s);
            LocalFree(s);
            ok = TRUE;
        }
    }
    CloseHandle(t);
    return ok;
}

/* "User Choice set via Windows User Experience {GUID}" as stored in this PC's shell32.dll. */
static void load_experience(wchar_t *out, size_t cch)
{
    static const wchar_t NEEDLE[] = L"User Choice set via Windows User Experience";
    wchar_t path[MAX_PATH];
    str_copy(out, cch, EXPERIENCE_DEFAULT);
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat(path, L"\\shell32.dll");
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    const DWORD cap = 6u * 1024 * 1024;
    wchar_t *buf = (wchar_t *)malloc(cap + 4);
    DWORD got = 0;
    if (buf && ReadFile(f, buf, cap, &got, NULL) && got > sizeof NEEDLE) {
        size_t nw = got / sizeof(wchar_t), nl = wcslen(NEEDLE);
        buf[nw] = 0;
        for (size_t i = 0; i + nl < nw; i++) {
            if (buf[i] != L'U' || wcsncmp(buf + i, NEEDLE, nl) != 0) continue;
            const wchar_t *end = wmemchr(buf + i, L'}', nw - i > 160 ? 160 : nw - i);
            if (end && (size_t)(end - (buf + i)) + 2 < cch && wcsstr(buf + i, L" {") && buf[i + nl] == L' ') {
                size_t len = (size_t)(end - (buf + i)) + 1;
                wcsncpy(out, buf + i, len);
                out[len] = 0;
            }
            break;
        }
    }
    free(buf);
    CloseHandle(f);
}

static void machine_id(wchar_t *out, size_t cch)
{
    wchar_t raw[80];
    out[0] = 0;
    if (!rd_str(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\SQMClient", L"MachineId", raw, 80) || !raw[0]) return;
    wchar_t *s = raw;
    size_t n = wcslen(s);
    if (s[0] == L'{') { s++; n--; }
    if (n && s[n - 1] == L'}') n--;
    if (n >= cch) return;
    wcsncpy(out, s, n);
    out[n] = 0;
}

static int hash_version(const wchar_t *sid)
{
    wchar_t p[300]; DWORD v = 0, cb = sizeof v; HKEY k;
    wf(p, 300, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\SystemProtectedUserData\\%ls\\AnyoneRead\\AppDefaults", sid);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, p, 0, KEY_QUERY_VALUE | WOW, &k) != ERROR_SUCCESS) return 0;
    LONG r = RegGetValueW(k, NULL, L"HashVersion", RRF_RT_REG_DWORD, NULL, &v, &cb);
    RegCloseKey(k);
    return r == ERROR_SUCCESS ? (int)v : 0;
}

static BOOL ucpd_present(void)
{
    HKEY k; DWORD start = 0, cb = sizeof start; BOOL on = FALSE;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\UCPD", 0, KEY_QUERY_VALUE | WOW, &k) != ERROR_SUCCESS)
        return FALSE;
    if (RegGetValueW(k, NULL, L"Start", RRF_RT_REG_DWORD, NULL, &start, &cb) == ERROR_SUCCESS) on = start != 4;
    RegCloseKey(k);
    return on;
}

BOOL uc_maybe_protected(const wchar_t *ext)
{
    static const wchar_t *L_[] = { L".pdf", L".htm", L".html", L".shtml", L".xhtml", L".xht", L".mht",
                                   L".mhtml", L".svg", L".xml", L".webp" };
    for (int i = 0; i < COUNTOF(L_); i++) if (str_ieq(ext, L_[i])) return TRUE;
    return FALSE;
}

/* ----------------------------------------------------- registry plumbing */
/* Give the current user full control of a key so it can be removed (Windows sometimes
 * leaves restrictive ACLs on choices it created itself). */
static BOOL reg_unlock(const wchar_t *path)
{
    HKEY k; HANDLE tok; BYTE buf[512]; DWORD n; PACL acl = NULL; BOOL ok = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, WRITE_DAC | READ_CONTROL | WOW, &k) != ERROR_SUCCESS) return FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenUser, buf, sizeof buf, &n)) {
            EXPLICIT_ACCESSW ea;
            memset(&ea, 0, sizeof ea);
            ea.grfAccessPermissions = KEY_ALL_ACCESS;
            ea.grfAccessMode = SET_ACCESS;
            ea.grfInheritance = NO_INHERITANCE;
            ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
            ea.Trustee.ptstrName = (LPWSTR)((TOKEN_USER *)buf)->User.Sid;
            if (SetEntriesInAclW(1, &ea, NULL, &acl) == ERROR_SUCCESS) {
                SECURITY_DESCRIPTOR sd;
                if (InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) &&
                    SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE))
                    ok = RegSetKeySecurity(k, DACL_SECURITY_INFORMATION, &sd) == ERROR_SUCCESS;
                LocalFree(acl);
            }
        }
        CloseHandle(tok);
    }
    RegCloseKey(k);
    return ok;
}

/* Delete HKCU\<path> and everything below it. A missing key is success. */
static LONG tree_delete(const wchar_t *path)
{
    LONG r = RegDeleteTreeW(HKEY_CURRENT_USER, path);
    if (r == ERROR_ACCESS_DENIED) {
        wchar_t sub[500];
        wf(sub, 500, L"%ls\\ProgId", path);
        reg_unlock(sub);
        reg_unlock(path);
        r = RegDeleteTreeW(HKEY_CURRENT_USER, path);
    }
    return (r == ERROR_FILE_NOT_FOUND || r == ERROR_PATH_NOT_FOUND) ? ERROR_SUCCESS : r;
}

static ULONGLONG ft64(const FILETIME *f) { return ((ULONGLONG)f->dwHighDateTime << 32) | f->dwLowDateTime; }

static BOOL key_time(HKEY k, ULONGLONG *t)
{
    FILETIME f;
    if (RegQueryInfoKeyW(k, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &f) != ERROR_SUCCESS) return FALSE;
    *t = ft64(&f);
    return TRUE;
}

/* ------------------------------------------------------ hash input strings */
static BOOL latest_input(const UcEnv *e, const wchar_t *assoc, const wchar_t *prog, ULONGLONG ft,
                         wchar_t *out, size_t cch)
{
    wchar_t ts[20];
    size_t ml = wcslen(e->machine);
    if (!ml) return FALSE;
    int cls = (int)(e->machine[ml - 1] % 3);
    hex_time(ft - ft % 10000, ts);
    const wchar_t *salt = LATEST_SALT[cls];
    switch (cls) {
    case 0:  wf(out, cch, L"%ls%ls%ls%ls%ls%ls", salt, assoc, ts, e->machine, prog, e->sid); break;
    case 1:  wf(out, cch, L"%ls%ls%ls%ls%ls%ls", salt, ts, assoc, e->sid, e->machine, prog); break;
    default: wf(out, cch, L"%ls%ls%ls%ls%ls%ls", e->sid, ts, salt, assoc, e->machine, prog); break;
    }
    return TRUE;
}

static BOOL latest_hash_for(const UcEnv *e, const wchar_t *assoc, const wchar_t *prog, ULONGLONG ft, wchar_t *out)
{
    wchar_t in[1024];
    if (!latest_input(e, assoc, prog, ft, in, 1024)) return FALSE;
    return hash_w(TRUE, in, out);
}

static BOOL legacy_hash_for(const UcEnv *e, const wchar_t *assoc, const wchar_t *prog, ULONGLONG ft, wchar_t *out)
{
    wchar_t in[1024], ts[20];
    hex_time(ft - ft % 600000000ULL, ts);            /* the classic hash covers whole minutes */
    wf(in, 1024, L"%ls%ls%ls%ls%ls", assoc, e->sid, prog, ts, e->experience);
    return hash_w(FALSE, in, out);
}

/* ----------------------------------------------------------------- canaries */
/* Recompute hashes Windows (or another tool) wrote and see whether ours match. Entries this
 * program wrote don't count: they would only confirm our own mistakes. */
static int canary(const UcEnv *e, BOOL latest)
{
    static const wchar_t *ROOTS[2] = { FILEEXTS, URLASSOC };
    int tested = 0;
    for (int r = 0; r < 2; r++) {
        HKEY root;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, ROOTS[r], 0, KEY_ENUMERATE_SUB_KEYS | WOW, &root) != ERROR_SUCCESS) continue;
        for (DWORD i = 0; tested < 60; i++) {
            wchar_t name[260], p[700], hash[40], prog[300], want[UC_HASH_CCH + 4];
            DWORD cch = 260;
            if (RegEnumKeyExW(root, i, name, &cch, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            if (latest) {
                wf(p, 700, L"%ls\\%ls\\UserChoiceLatest", ROOTS[r], name);
                if (!rd_str(HKEY_CURRENT_USER, p, L"Hash", hash, 40)) continue;
                wchar_t pp[760];
                wf(pp, 760, L"%ls\\ProgId", p);
                if (!rd_str(HKEY_CURRENT_USER, pp, L"ProgId", prog, 300)) {
                    if (!rd_str(HKEY_CURRENT_USER, p, L"ProgId", prog, 300)) continue;
                    str_copy(pp, 760, p);
                }
                ULONGLONG t = key_write_time(HKEY_CURRENT_USER, pp);
                if (!t || !prog[0] || str_starts_i(prog, OUR_PREFIX)) continue;
                tested++;
                if (latest_hash_for(e, name, prog, t, want) && wcscmp(want, hash) == 0) { RegCloseKey(root); return 1; }
            } else if (r == 0) {
                wf(p, 700, L"%ls\\%ls\\UserChoice", ROOTS[r], name);
                if (!rd_str(HKEY_CURRENT_USER, p, L"Hash", hash, 40) || !rd_str(HKEY_CURRENT_USER, p, L"ProgId", prog, 300)) continue;
                ULONGLONG t = key_write_time(HKEY_CURRENT_USER, p);
                if (!t || !prog[0] || str_starts_i(prog, OUR_PREFIX)) continue;
                tested++;
                if (legacy_hash_for(e, name, prog, t, want) && wcscmp(want, hash) == 0) { RegCloseKey(root); return 1; }
            }
        }
        RegCloseKey(root);
    }
    return tested ? 0 : -1;
}

/* ------------------------------------------------------------------- probe */
void uc_probe(UcEnv *e)
{
    DWORD major;
    memset(e, 0, sizeof *e);
    e->preflight = -1;
    win_version(&major, &e->build);
    e->win11 = e->build >= 22000;
    e->selfTest = uc_hash_selftest();
    user_sid(e->sid, 160);
    machine_id(e->machine, 64);
    e->machineClass = e->machine[0] ? (int)(e->machine[wcslen(e->machine) - 1] % 3) : -1;
    load_experience(e->experience, 160);
    e->scheme = e->sid[0] ? hash_version(e->sid) : 0;
    e->ucpd = ucpd_present();

    e->latestOk = e->selfTest && e->machineClass >= 0 && e->sid[0];
    if (e->latestOk) {
        e->canaryLatest = canary(e, TRUE);
        if (e->canaryLatest == 0) e->latestOk = FALSE;
    } else e->canaryLatest = -1;
    e->canaryLegacy = (e->sid[0]) ? canary(e, FALSE) : -1;

    e->canWrite = TRUE;
    if (!e->sid[0]) {
        e->canWrite = FALSE;
        str_copy(e->note, 320, L"Could not identify the current Windows account.");
    } else if (e->scheme > 1) {
        e->canWrite = FALSE;
        wf(e->note, 320, L"This Windows build uses default-app protection version %d, which this program doesn't know yet. "
                         L"Use Windows Settings " U_ARROW L" Apps " U_ARROW L" Default apps.", e->scheme);
    } else if (e->scheme == 1 && !e->latestOk) {
        e->canWrite = FALSE;
        if (!e->selfTest)
            str_copy(e->note, 320, L"The built-in self-test of the hash code failed, so nothing is written.");
        else if (e->machineClass < 0)
            str_copy(e->note, 320, L"This Windows build needs a machine id that isn't present, so defaults can't be set safely.");
        else
            str_copy(e->note, 320, L"Windows changed how it signs default-app choices (existing entries no longer match this "
                                   L"program's calculation). Nothing is written; use Windows Settings instead.");
    } else if (e->scheme == 0 && e->canaryLegacy == 0) {
        str_copy(e->note, 320, L"Caution: existing default-app entries don't match this program's calculation, "
                               L"so changes may be ignored by Windows. Each change is verified afterwards.");
    }

    const wchar_t *os = e->win11 ? L"Windows 11" : L"Windows 10";
    const wchar_t *how = e->scheme >= 1 ? L"UserChoiceLatest" : L"classic UserChoice";
    const wchar_t *ver = !e->canWrite ? L"read-only"
                       : (e->scheme >= 1 ? (e->canaryLatest == 1 ? L"verified" : L"unverified")
                                         : (e->canaryLegacy == 1 ? L"verified" : L"unverified"));
    wf(e->summary, 160, L"%ls (build %lu) " U_MIDDOT L" %ls, %ls", os, e->build, how, ver);
}

static const wchar_t *tri(int v, const wchar_t *a, const wchar_t *b, const wchar_t *c) { return v > 0 ? a : (v == 0 ? b : c); }

void uc_report(const UcEnv *e, wchar_t *out, size_t cch)
{
    wchar_t m[200];
    if (e->machineClass >= 0) wf(m, 200, L"present (class %d)", e->machineClass); else str_copy(m, 200, L"missing");
    wf(out, cch,
       L"Windows:  %ls, build %lu%ls\r\n"
       L"Key Windows reads:  %ls\r\n"
       L"Hash self-test (known answer from a real PC):  %ls\r\n"
       L"UserChoiceLatest entries written by Windows reproduced:  %ls\r\n"
       L"Classic UserChoice entries written by Windows reproduced:  %ls\r\n"
       L"Machine id:  %ls\r\n"
       L"Protection driver (UCPD):  %ls\r\n"
       L"Account:  %ls%ls\r\n\r\n"
       L"Result:  %ls%ls",
       e->win11 ? L"11" : L"10", e->build, is_elevated() ? L" (running as administrator)" : L"",
       e->scheme >= 1 ? L"UserChoiceLatest (HashVersion = 1)" : L"classic UserChoice",
       e->selfTest ? L"passed" : L"FAILED",
       tri(e->canaryLatest, L"yes", L"NO", L"nothing to compare yet"),
       tri(e->canaryLegacy, L"yes", L"NO", L"nothing to compare yet"),
       m,
       e->ucpd ? L"installed; .pdf (and on some builds web types) can only be changed in Settings" : L"not present",
       e->sid, is_elevated() ? L"\r\n(Run this program as your normal user: an elevated token of a different account changes that account's defaults.)" : L"",
       e->canWrite ? L"defaults can be set directly." : L"direct changes are switched off.",
       e->note[0] ? L"\r\n" : L"");
    if (e->note[0]) { wcsncat(out, e->note, cch - wcslen(out) - 1); }
}

/* ------------------------------------------------------------------ reading */
BOOL uc_get(const UcEnv *e, const wchar_t *ext, wchar_t *out, size_t cch)
{
    wchar_t p[500];
    BOOL latestFirst = e && e->scheme >= 1;
    out[0] = 0;
    for (int pass = 0; pass < 2; pass++) {
        if ((pass == 0) == latestFirst) {
            wf(p, 500, FILEEXTS L"\\%ls\\UserChoiceLatest\\ProgId", ext);
            if (rd_str(HKEY_CURRENT_USER, p, L"ProgId", out, (DWORD)cch) && out[0]) return TRUE;
            wf(p, 500, FILEEXTS L"\\%ls\\UserChoiceLatest", ext);
            if (rd_str(HKEY_CURRENT_USER, p, L"ProgId", out, (DWORD)cch) && out[0]) return TRUE;
        } else {
            wf(p, 500, FILEEXTS L"\\%ls\\UserChoice", ext);
            if (rd_str(HKEY_CURRENT_USER, p, L"ProgId", out, (DWORD)cch) && out[0]) return TRUE;
        }
    }
    out[0] = 0;
    return FALSE;
}

static IApplicationAssociationRegistration *s_aar;

BOOL uc_effective(const wchar_t *ext, wchar_t *out, size_t cch)
{
    LPWSTR s = NULL;
    out[0] = 0;
    if (!s_aar && FAILED(CoCreateInstance(&CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC_SERVER,
                                          &IID_IApplicationAssociationRegistration, (void **)&s_aar)))
        return FALSE;
    HRESULT hr = IApplicationAssociationRegistration_QueryCurrentDefault(s_aar, ext, AT_FILEEXTENSION, AL_EFFECTIVE, &s);
    if (FAILED(hr) || !s) return FALSE;
    str_copy(out, cch, s);
    CoTaskMemFree(s);
    return out[0] != 0;
}

/* ------------------------------------------------------------------ writing */
/* Keys are updated in place, never deleted and recreated: if Windows refuses part-way (its
 * protection driver does for a few types) the previous choice must still be there. */
static LONG open_rw(const wchar_t *path, HKEY *k)
{
    const REGSAM sam = KEY_SET_VALUE | KEY_QUERY_VALUE | WOW;
    LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, NULL, 0, sam, NULL, k, NULL);
    if (r == ERROR_ACCESS_DENIED && reg_unlock(path))
        r = RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, NULL, 0, sam, NULL, k, NULL);
    return r;
}

static LONG write_latest(const UcEnv *e, const wchar_t *ext, const wchar_t *prog, BOOL *touched)
{
    wchar_t base[400], sub[420], hash[UC_HASH_CCH + 4], in[1024];
    HKEY k; ULONGLONG t; LONG r;
    wf(base, 400, FILEEXTS L"\\%ls\\UserChoiceLatest", ext);
    wf(sub, 420, L"%ls\\ProgId", base);
    if (!latest_input(e, ext, prog, 0, in, 1024) || wcslen(in) > UC_LATEST_MAX_CCH) return FAM_E_TOOLONG;

    r = open_rw(sub, &k);
    if (r) return r;
    r = set_str(k, L"ProgId", prog);                          /* the hash covers this key's timestamp */
    if (!r) *touched = TRUE;
    if (!r && !key_time(k, &t)) r = ERROR_READ_FAULT;
    RegCloseKey(k);
    if (r) return r;
    if (!latest_hash_for(e, ext, prog, t, hash)) return FAM_E_TOOLONG;
    r = open_rw(base, &k);
    if (r) return r;
    r = set_str(k, L"Hash", hash);
    if (!r) r = set_str(k, L"ProgId", prog);
    RegCloseKey(k);
    return r;
}

static LONG write_legacy(const UcEnv *e, const wchar_t *ext, const wchar_t *prog, BOOL *touched)
{
    wchar_t key[400], hash[UC_HASH_CCH + 4];
    HKEY k; LONG r;
    wf(key, 400, FILEEXTS L"\\%ls\\UserChoice", ext);
    r = open_rw(key, &k);
    if (r) return r;
    r = set_str(k, L"ProgId", prog);
    if (!r) *touched = TRUE;
    /* The hash covers the minute of the key's last write, and writing the hash is itself a
     * write: if the minute turned over in between, hash again with the new one. */
    for (int i = 0; !r; i++) {
        ULONGLONG t, t2;
        if (i >= 6) { r = FAM_E_REJECTED; break; }
        if (!key_time(k, &t)) { r = ERROR_READ_FAULT; break; }
        if (!legacy_hash_for(e, ext, prog, t, hash)) { r = FAM_E_TOOLONG; break; }
        r = set_str(k, L"Hash", hash);
        if (!r && key_time(k, &t2) && t2 - t2 % 600000000ULL == t - t % 600000000ULL) break;
    }
    RegCloseKey(k);
    return r;
}

static LONG set_inner(const UcEnv *e, const wchar_t *ext, const wchar_t *prog, BOOL *touched)
{
    LONG r;
    BOOL wrote = FALSE;
    if (e->latestOk) {
        r = write_latest(e, ext, prog, touched);
        if (r == ERROR_SUCCESS) wrote = TRUE;
        else if (e->scheme >= 1 || r == ERROR_ACCESS_DENIED) return r;
    }
    r = write_legacy(e, ext, prog, touched);
    if (r != ERROR_SUCCESS && wrote) r = ERROR_SUCCESS;                  /* Latest is authoritative here */
    return r;
}

LONG uc_set(const UcEnv *e, const wchar_t *ext, const wchar_t *prog)
{
    if (!e->canWrite) return FAM_E_CANT_WRITE;
    wchar_t prev[260];
    BOOL touched = FALSE;
    uc_get(e, ext, prev, 260);
    LONG r = set_inner(e, ext, prog, &touched);
    if (r == ERROR_SUCCESS) { uc_register_toast(ext, prog); return r; }
    if (touched) {                                      /* part-way failure: put the old choice back */
        BOOL t2 = FALSE;
        if (prev[0]) set_inner(e, ext, prev, &t2); else uc_clear(ext);
    }
    return r;
}

LONG uc_clear(const wchar_t *ext)
{
    wchar_t p[400];
    wf(p, 400, FILEEXTS L"\\%ls\\UserChoiceLatest", ext);
    LONG r1 = tree_delete(p);
    wf(p, 400, FILEEXTS L"\\%ls\\UserChoice", ext);
    LONG r2 = tree_delete(p);
    return r1 ? r1 : r2;
}

/* Windows remembers whether it already offered "set a default" for each handler; marking them
 * as shown stops it from popping the app-picker the first time a file is opened. */
void uc_register_toast(const wchar_t *ext, const wchar_t *progId)
{
    HKEY k;
    DWORD zero = 0;
    wchar_t name[400], p[400];
    if (RegCreateKeyExW(HKEY_CURRENT_USER, TOASTS, 0, NULL, 0, KEY_SET_VALUE | WOW, NULL, &k, NULL) != ERROR_SUCCESS) return;
    wf(name, 400, L"%ls_%ls", progId, ext);
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&zero, sizeof zero);

    HKEY op;                                                  /* other registered handlers of the type */
    wf(p, 400, L"Software\\Classes\\%ls\\OpenWithProgids", ext);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, p, 0, KEY_QUERY_VALUE | WOW, &op) == ERROR_SUCCESS) {
        for (DWORD i = 0;; i++) {
            wchar_t v[260]; DWORD cv = 260, type = 0, cd = 0;
            if (RegEnumValueW(op, i, v, &cv, NULL, &type, NULL, &cd) != ERROR_SUCCESS) break;
            if (cd == 0 && v[0]) {
                wf(name, 400, L"%ls_%ls", v, ext);
                RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&zero, sizeof zero);
            }
        }
        RegCloseKey(op);
    }
    RegCloseKey(k);
}

void uc_notify(void)
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

/* ---------------------------------------------------------------- preflight */
BOOL uc_preflight(UcEnv *e)
{
    static const wchar_t EXT[] = L".famprobe", PROG[] = L"FileAssocManager.Probe";
    wchar_t cls[200], cmd[MAX_PATH + 20], eff[260], toast[260];
    HKEY k;
    BOOL ok = FALSE;
    e->preflight = 0;
    if (!e->canWrite) return FALSE;

    wf(cls, 200, L"Software\\Classes\\%ls\\shell\\open\\command", PROG);
    wchar_t exe[MAX_PATH];
    GetWindowsDirectoryW(exe, MAX_PATH);
    wcscat(exe, L"\\notepad.exe");
    wf(cmd, MAX_PATH + 20, L"\"%ls\" \"%%1\"", exe);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, cls, 0, NULL, 0, KEY_SET_VALUE | WOW, NULL, &k, NULL) == ERROR_SUCCESS) {
        set_str(k, NULL, cmd);
        RegCloseKey(k);
        wf(cls, 200, L"Software\\Classes\\%ls\\OpenWithProgids", EXT);
        if (RegCreateKeyExW(HKEY_CURRENT_USER, cls, 0, NULL, 0, KEY_SET_VALUE | WOW, NULL, &k, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(k, PROG, 0, REG_NONE, NULL, 0);
            RegCloseKey(k);
            if (uc_set(e, EXT, PROG) == ERROR_SUCCESS)
                ok = uc_effective(EXT, eff, 260) && _wcsicmp(eff, PROG) == 0;
        }
    }

    uc_clear(EXT);                                              /* leave nothing behind */
    wf(cls, 200, L"Software\\Classes\\%ls", EXT);
    RegDeleteTreeW(HKEY_CURRENT_USER, cls);
    wf(cls, 200, L"Software\\Classes\\%ls", PROG);
    RegDeleteTreeW(HKEY_CURRENT_USER, cls);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, TOASTS, 0, KEY_SET_VALUE | WOW, &k) == ERROR_SUCCESS) {
        wf(toast, 260, L"%ls_%ls", PROG, EXT);
        RegDeleteValueW(k, toast);
        RegCloseKey(k);
    }
    e->preflight = ok ? 1 : 0;
    return ok;
}
