/* Snapshots of default-app choices: automatic before every change (Undo), manual export and
 * restore, and a DISM / Group Policy XML export.
 *
 * Why not .reg files: a choice is only valid together with its hash, and the hash depends on
 * the registry key's last-write time, so importing a .reg file produces choices Windows
 * discards. Snapshots store only the ProgIds and are re-applied with fresh, valid hashes. */
#ifndef BACKUP_H
#define BACKUP_H
#include "registry.h"

void  backup_folder(wchar_t *out, size_t n);
BOOL  backup_save(const FmtList *l, const int *idx, int n, const wchar_t *label, wchar_t *pathOut);
BOOL  backup_newest(wchar_t *path);                      /* newest snapshot not yet undone */
void  backup_mark_undone(const wchar_t *path);
BOOL  backup_restore(FmtList *l, const UcEnv *e, const wchar_t *path, AssignResult *r, ProgressFn pf, void *ctx);
/* Header line of a snapshot ("2026-10-22 14:03:11 Assigned to VLC") and how many types it holds. */
BOOL  backup_describe(const wchar_t *path, wchar_t *label, size_t cch, int *count);
BOOL  backup_export(const FmtList *l, const wchar_t *path);
BOOL  backup_export_xml(const FmtList *l, const wchar_t *path);

#endif
