/* Built-in catalogue of file formats, grouped into categories. */
#ifndef FORMATS_H
#define FORMATS_H
#include "common.h"

enum {
    CAT_IMAGES, CAT_RAW, CAT_VIDEO, CAT_AUDIO, CAT_DOCS, CAT_SHEETS, CAT_SLIDES, CAT_BOOKS,
    CAT_TEXT, CAT_CODE, CAT_WEB, CAT_ARCHIVES, CAT_DISKS, CAT_FONTS, CAT_3D, CAT_DATA, CAT_MAIL,
    CAT_OTHER,
    CAT_COUNT
};

typedef struct {
    const wchar_t *ext;      /* ".jpg" */
    const wchar_t *desc;     /* "JPEG image" */
    int            cat;
} FmtDef;

extern const FmtDef FMT_DEFS[];
extern const int    FMT_DEF_COUNT;

const wchar_t *cat_name(int cat);
const wchar_t *cat_sample_ext(int cat);              /* extension whose shell icon represents the category */
const FmtDef  *fmt_def_find(const wchar_t *ext);     /* NULL if not in the catalogue */
BOOL           fmt_is_blocked(const wchar_t *ext);   /* programs, shortcuts, ...: never reassigned */
int            cat_from_perceived(const wchar_t *perceived);   /* "image" -> CAT_IMAGES, else -1 */

#endif
