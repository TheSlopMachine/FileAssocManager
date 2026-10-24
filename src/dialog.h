/* Modal dialogs: application picker, result report, add-file-types, text viewer. */
#ifndef DIALOG_H
#define DIALOG_H
#include "common.h"
#include "registry.h"
#include "apps.h"

BOOL dialog_init(HINSTANCE inst);                              /* register the window class */

/* Choose the program that should open the marked formats. `recent` lists exe paths, newest
 * first. TRUE with the chosen program's exe in exeOut (which is also added to `apps`). */
BOOL pick_app_dialog(HWND parent, AppList *apps, const FmtList *fl,
                     const wchar_t (*recent)[MAX_PATH], int nrecent, wchar_t *exeOut);

/* Outcome of a batch: everything that didn't simply succeed, with what to do about it. */
void report_dialog(HWND parent, const wchar_t *title, const wchar_t *summary,
                   const FmtList *fl, const AssignResult *r, const App *app);

BOOL add_formats_dialog(HWND parent, wchar_t *text, int cch);  /* free text in, free text out */
void text_dialog(HWND parent, const wchar_t *title, const wchar_t *intro, const wchar_t *text);
void open_default_apps_settings(const App *app);               /* Settings > Default apps (for `app` if known) */

#endif
