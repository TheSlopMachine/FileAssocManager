/* The list of file formats the program shows, their current defaults, and the batch
 * assign / reset operations. */
#ifndef REGISTRY_H
#define REGISTRY_H
#include "common.h"
#include "formats.h"
#include "userchoice.h"
#include "apps.h"

enum { FF_BLOCKED = 1, FF_PROTECTED = 2, FF_CUSTOM = 4, FF_CATALOG = 8 };

typedef struct {
    wchar_t         ext[40];       /* ".jpg" */
    const wchar_t  *desc;          /* catalogue description, or NULL */
    wchar_t         sdesc[96];     /* description read from the registry (lazy) */
    BOOL            sdescDone;
    int             cat;
    int             flags;
    BOOL            marked;        /* ticked for assignment */
    wchar_t         user[200];     /* stored user choice ("" = none) */
    wchar_t         sys[200];      /* system default ProgId from HKCR\.ext */
    const ProgInfo *pi;            /* who opens it now: user choice, else system default */
    int             icon;          /* shell icon index of the type, -1 until needed */
} Fmt;

typedef struct { Fmt *v; int n, cap; } FmtList;

void  fmts_load(FmtList *l, const UcEnv *e);
void  fmts_free(FmtList *l);
int   fmts_find(const FmtList *l, const wchar_t *ext);
int   fmts_add_custom(FmtList *l, const UcEnv *e, const wchar_t *ext);   /* index, or -1 */
void  fmt_refresh(Fmt *f, const UcEnv *e);
void  fmts_refresh_all(FmtList *l, const UcEnv *e);
const wchar_t *fmt_desc(Fmt *f);
const wchar_t *fmt_progid(const Fmt *f);                 /* effective ProgId, "" if none */

/* ---- batch operations ---------------------------------------------------------------- */
enum { AS_OK, AS_ALREADY, AS_PROTECTED, AS_REJECTED, AS_BLOCKED, AS_NOHANDLER, AS_FAILED, AS_SKIPPED };

typedef struct { int fmt; int outcome; LONG err; } AssignItem;
typedef struct {
    AssignItem *items;
    int n, cap;
    int count[AS_SKIPPED + 1];
    BOOL aborted;
} AssignResult;

typedef void (*ProgressFn)(int done, int total, const wchar_t *ext, void *ctx);

void  assign_run(FmtList *l, const UcEnv *e, const App *app, const int *idx, int n,
                 AssignResult *r, ProgressFn pf, void *ctx);
void  reset_run(FmtList *l, const UcEnv *e, const int *idx, int n, AssignResult *r, ProgressFn pf, void *ctx);
void  result_add(AssignResult *r, int fmt, int outcome, LONG err);
void  result_free(AssignResult *r);
const wchar_t *outcome_text(int outcome);

#endif
