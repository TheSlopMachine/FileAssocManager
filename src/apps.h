/* Applications: finding installed programs, describing what a ProgId launches, and choosing
 * (or creating) the ProgId that makes a program the handler of a file type. */
#ifndef APPS_H
#define APPS_H
#include "common.h"

#define PROGID_PREFIX L"FileAssocManager."     /* ProgIds this program creates */

/* ---- What a stored ProgId means to the user ----------------------------------------- */
typedef struct {
    wchar_t progId[200];
    wchar_t name[160];        /* friendly application name ("VLC media player") */
    wchar_t exe[MAX_PATH];    /* program the open verb runs; "" if none (packaged apps) */
    wchar_t key[MAX_PATH];    /* grouping identity: lower-case exe path, else lower-case name */
    int     icon;             /* shell icon index, -1 until needed */
    BOOL    ours;             /* created by this program */
} ProgInfo;

const ProgInfo *prog_info(const wchar_t *progId);          /* cached; NULL for an empty id */
int             prog_icon(const ProgInfo *p);
const wchar_t  *prog_exe_key(const wchar_t *progId);       /* lower-case exe path or "" (cheap, cached) */

/* ---- Installed applications ---------------------------------------------------------- */
typedef struct {
    wchar_t  name[160];
    wchar_t  exe[MAX_PATH];
    wchar_t  key[MAX_PATH];     /* lower-case exe path */
    wchar_t  regName[128];      /* name under RegisteredApplications, "" if unregistered */
    BOOL     regUser;           /* registered for the current user rather than the machine */
    wchar_t  cmd[1024];         /* open command with %1, "" = "exe" "%1" */
    wchar_t *decl;              /* "\n.ext\tProgId" entries for types the app declares (malloc) */
    int      icon;              /* shell icon index, -1 until needed */
    int      nameQ;             /* how trustworthy the name is (higher wins when merging) */
} App;

typedef struct { App *v; int n, cap; } AppList;

void  apps_discover(AppList *l);
void  apps_free(AppList *l);
BOOL  app_from_exe(const wchar_t *path, App *out);          /* .exe or .lnk chosen by the user */
App  *apps_add(AppList *l, const App *a);                   /* copy in, merging with a known one */
int   apps_find(const AppList *l, const wchar_t *exe);
int   app_icon(App *a);
void  app_free(App *a);

/* Which programs already handle an extension: "\nexe\nexe\n" (lower-case paths). */
void  ext_handlers(const AppList *l, const wchar_t *ext, wchar_t *keys, size_t cch);
BOOL  ext_handled_by(const wchar_t *keys, const App *a);

/* ProgId to store so `a` opens `ext`: the program's own registration if it has one, otherwise
 * one this program creates. ERROR_SUCCESS or FAM_E_NOHANDLER / a registry error. */
LONG  app_progid_for(const App *a, const wchar_t *ext, wchar_t *progId, size_t cch);

#endif
