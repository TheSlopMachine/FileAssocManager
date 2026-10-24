/* Writing default-app choices the way Windows 10/11 requires.
 *
 * A default is `HKCU\...\Explorer\FileExts\<.ext>\UserChoice` (ProgId + Hash). Windows ignores
 * a choice whose Hash doesn't match, and on Windows 11 24H2+ it reads a second key,
 * `UserChoiceLatest`, instead. This module detects which applies, computes the hashes
 * (uchash.c), refuses to write when it can't be sure they're right, and verifies results
 * through the shell's own query API. */
#ifndef USERCHOICE_H
#define USERCHOICE_H
#include "common.h"

typedef struct {
    int      scheme;            /* 0 = classic UserChoice, 1 = UserChoiceLatest, >1 = newer than we know */
    BOOL     latestOk;          /* UserChoiceLatest can be computed (and, if checkable, was verified) */
    int      canaryLatest;      /* 1 = reproduced a hash Windows wrote, 0 = none matched, -1 = nothing to compare */
    int      canaryLegacy;      /* same, for the classic UserChoice key (informational) */
    BOOL     selfTest;          /* built-in known-answer test of the hash code passed */
    BOOL     canWrite;          /* safe to write defaults on this PC */
    int      preflight;         /* live test on a scratch type: 1 Windows accepted it, 0 refused, -1 not run yet */
    BOOL     ucpd;              /* UserChoice Protection Driver installed and not disabled */
    BOOL     win11;
    DWORD    build;
    int      machineClass;      /* -1 if the machine id is missing */
    wchar_t  sid[160];
    wchar_t  machine[64];
    wchar_t  experience[160];   /* salt string of the classic hash (read from shell32.dll) */
    wchar_t  note[320];         /* why canWrite is FALSE, or a caution; "" if none */
    wchar_t  summary[160];      /* short status-bar text */
} UcEnv;

void  uc_probe(UcEnv *e);
void  uc_report(const UcEnv *e, wchar_t *out, size_t cch);

/* Choice currently stored for the user (not the system default); FALSE if none. */
BOOL  uc_get(const UcEnv *e, const wchar_t *ext, wchar_t *progId, size_t cch);
/* What Windows itself resolves today (validates hashes, honours policy). */
BOOL  uc_effective(const wchar_t *ext, wchar_t *progId, size_t cch);

/* Set / clear the choice. ERROR_SUCCESS or a Win32 / FAM_E_* code. ERROR_ACCESS_DENIED
 * means Windows (usually its UCPD driver) protects this type. */
LONG  uc_set(const UcEnv *e, const wchar_t *ext, const wchar_t *progId);
LONG  uc_clear(const wchar_t *ext);

/* Writes a default for a throw-away file type, asks Windows whether it accepted it, and removes
 * everything again. Run once before the first real change. Sets e->preflight. */
BOOL  uc_preflight(UcEnv *e);

void  uc_register_toast(const wchar_t *ext, const wchar_t *progId);  /* suppress the "pick an app" prompt */
void  uc_notify(void);                                               /* tell Explorer associations changed */
BOOL  uc_maybe_protected(const wchar_t *ext);                        /* types UCPD protects on some builds */

#endif
