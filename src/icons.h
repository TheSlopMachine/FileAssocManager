/* Shell icons. Everything uses the system small-icon image list, so one handle serves every
 * list view and an icon is only extracted by Windows when it is first drawn. */
#ifndef ICONS_H
#define ICONS_H
#include "common.h"

HIMAGELIST sys_imagelist(void);
int icon_for_ext(const wchar_t *ext);      /* the icon Explorer shows for files of this type */
int icon_for_exe(const wchar_t *exe);      /* a program's own icon; generic application icon if unknown */
int icon_generic_app(void);

#endif
