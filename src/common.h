/* Shared includes, constants and small helpers used by every module. */
#ifndef COMMON_H
#define COMMON_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#undef  _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#undef  WINVER
#define WINVER 0x0601
#define COBJMACROS

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>

#define APP_TITLE   L"File Association Manager"
#define APP_VERSION L"1.0.0"
#define APP_ID      L"FileAssocManager"          /* registry / folder name */

/* Always use the 64-bit registry view, even from a 32-bit build. */
#define WOW KEY_WOW64_64KEY

extern HINSTANCE g_inst;
extern HFONT     g_font, g_fontBold;
extern int       g_dpi;
#define S(x) MulDiv((x), g_dpi, 96)          /* scale 96-dpi pixels to the screen */

/* Application-defined error codes (bit 29 set, as Win32 recommends for non-system codes). */
#define FAM_E_CANT_WRITE  0x20000001L   /* this PC's default-app protection can't be satisfied */
#define FAM_E_REJECTED    0x20000002L   /* written, but Windows did not accept it */
#define FAM_E_NOHANDLER   0x20000003L   /* the application can't be registered for this type */
#define FAM_E_BLOCKED     0x20000004L   /* file type is never reassigned (programs, shortcuts, ...) */
#define FAM_E_TOOLONG     0x20000005L   /* identifier too long to hash */

/* Non-ASCII characters used in UI text; spelled as escapes so a hex digit following one in a
 * string can't be swallowed (concatenate: L"a " U_MIDDOT L" b"). */
#define U_ELLIPSIS L"\x2026"
#define U_MIDDOT   L"\x00B7"
#define U_ARROW    L"\x2192"
#define U_EMDASH   L"\x2014"

#define COUNTOF(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* printf into a wide buffer; always NUL-terminated. Use %ls for wide strings. */
void wf(wchar_t *buf, size_t n, const wchar_t *fmt, ...);
void err_box(HWND owner, const wchar_t *what, LONG code);
void win_error_text(LONG code, wchar_t *out, size_t n);       /* system or app-defined message */
BOOL is_elevated(void);
BOOL has_envvar(const wchar_t *s);
BOOL browse_file(HWND owner, wchar_t *path /* MAX_PATH */, const wchar_t *filter, BOOL save, const wchar_t *defext);
BOOL clipboard_set(HWND owner, const wchar_t *text);
BOOL CALLBACK font_cb(HWND h, LPARAM font);
/* CreateWindowEx for a child control; coordinates are 96-dpi and get scaled. */
HWND ctl(HWND parent, DWORD ex, const wchar_t *cls, const wchar_t *text, DWORD style,
         int x, int y, int w, int h, int id);

/* Strings. */
void  str_lower(wchar_t *s);                                  /* ASCII lower-case, in place */
BOOL  str_ieq(const wchar_t *a, const wchar_t *b);
BOOL  str_starts_i(const wchar_t *s, const wchar_t *prefix);
void  str_copy(wchar_t *dst, size_t n, const wchar_t *src);   /* truncating, always terminated */
void  ext_normalize(const wchar_t *in, wchar_t *out, size_t n); /* "JPG", "*.jpg" -> ".jpg" */

/* Registry: REG_SZ / REG_EXPAND_SZ value (environment variables expanded). sub may be NULL. */
BOOL  read_str(HKEY k, const wchar_t *val, wchar_t *out, DWORD cch);
BOOL  rd_str(HKEY root, const wchar_t *sub, const wchar_t *val, wchar_t *out, DWORD cch);
LONG  set_str(HKEY k, const wchar_t *name, const wchar_t *v);
BOOL  key_exists(HKEY root, const wchar_t *sub);
ULONGLONG key_write_time(HKEY root, const wchar_t *sub);      /* FILETIME ticks, 0 if missing */

/* Files. */
BOOL  file_exists(const wchar_t *path);
BOOL  read_text_file(const wchar_t *path, wchar_t **out);     /* UTF-8/UTF-16 -> malloc'd wide text */
BOOL  write_text_file(const wchar_t *path, const wchar_t *text, BOOL bom);  /* UTF-8 */
void  data_dir(const wchar_t *sub, wchar_t *out, size_t n);   /* %APPDATA%\FileAssocManager[\sub], created */

/* Windows version (from ntdll, so the manifest can't lie to us). */
void  win_version(DWORD *major, DWORD *build);

#endif
