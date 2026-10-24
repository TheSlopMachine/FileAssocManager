# File Association Manager

A small native Windows tool (pure C, Win32 API, no dependencies) for setting the default application
for many file formats at once, in Windows 10 and Windows 11.

## Features

- **Assign many formats at once.** Tick formats, choose an application, press *Assign*.
  Ctrl+A ticks everything currently shown.
- **Formats grouped by category:** images, camera RAW, video, audio, documents, spreadsheets,
  presentations, PDF and e-books, text and config, source code, web pages, archives, disk images,
  fonts, 3D and CAD, databases, email. About 440 built-in formats, plus any extension Windows knows.
- **Filter and search.** Type several words to narrow the list (they all must match). The *Show*
  menu filters by "not opened by X", "opened by X", "set by you", or "no default app".
- **Applications that register for types.** *Tick formats the chosen application registers* marks
  exactly what an app declares for itself.
- **Safe by design:**
  - every change is read back from Windows, and a change Windows doesn't honour is rolled back;
  - the batch stops after repeated refusals instead of failing hundreds of times;
  - before the first change, a harmless test runs on a throw-away type to check that Windows accepts
    changes on this PC;
  - programs and shortcuts (`.exe`, `.lnk`, `.bat`, …) are never reassigned;
  - types Windows protects (for example `.pdf`, `.htm`) are handed over to Windows Settings.
- **Undo.** Every change saves a snapshot first. *Undo last change* (Ctrl+Z) restores it.
- **Backups and sharing.** Export your defaults to a `.fam` file and restore them later, or export
  a `DefaultAssociations.xml` for DISM / Group Policy.
- **Applications from anywhere.** Programs are found from the registry, Start menu shortcuts,
  App Paths, and any `.exe` or shortcut you choose or drag onto the window.

| Shortcut | Action | Shortcut | Action |
|---|---|---|---|
| Ctrl+F | Search formats | Ctrl+Enter | Assign to ticked formats |
| Ctrl+A | Tick all shown | Ctrl+Shift+A | Untick everything |
| Ctrl+O | Choose application | Ctrl+Z | Undo last change |
| Ctrl+N | Add file types | Ctrl+E | Export my defaults |
| Space | Tick / untick selected | F5 | Refresh |

## Build

Requires MinGW-w64 `gcc`, `windres` and GNU `make` (`mingw32-make` on some installs).

```
make build            # -> build/FileAssocManager.exe
make build DEBUG=1    # unoptimised, with symbols
make test             # hash tests, run natively (needs a host C compiler)
make clean
make build CROSS=x86_64-w64-mingw32-   # cross-compile from Linux/macOS
```

## Project layout

```
Makefile
README.md
LICENSE-THIRD-PARTY.txt   licence of the copied hash tables (MIT)
src/
  main.c       main window, menus, list, actions, drag and drop
  dialog.c     application picker, result report, add file types, text viewer
  registry.c   format list, batch assign / reset, outcome tracking
  userchoice.c Windows default-app records: scheme detection, writing, verification, preflight
  uchash.c     the two hashes Windows checks (classic and UserChoiceLatest), portable C
  uchash_tables.inc  lookup tables for UserChoiceLatest (from cssxn/UserChoiceLatestHash)
  apps.c       finding installed applications; what a ProgId opens
  formats.c    built-in catalogue of formats and categories
  backup.c     snapshots, undo, export / restore, DISM XML
  icons.c      shell icon lookup
  util.c       shared helpers (registry, files, strings)
  *.h          module interfaces (common.h is included everywhere)
res/           icon, manifest (visual styles, per-monitor DPI), version info
tools/         make_icon.py (icon), ref_hash.py (independent hash reference), gen_vectors.py
tests/         uchash_test.c and vectors.inc (checked against ref_hash.py)
```

## How it works (and why it's not just a registry edit)

On Windows 10 and 11, a default app is stored as `HKCU\...\Explorer\FileExts\.ext\UserChoice`
with a `ProgId` **and a `Hash`**. Windows ignores a choice whose hash doesn't match, so simply
writing the `ProgId` does nothing. On Windows 11 (24H2 and later) the same record is kept under
`UserChoiceLatest` with a different hash. Windows also protects some types with a driver (UCPD).

This program:

1. detects which scheme the PC uses and checks its own hash calculation against a known answer
   and against Windows' own entries on the PC (the "canary");
2. writes the record in the order Windows expects;
3. asks the shell what the default now is, and rolls back if it isn't the new application;
4. refuses to write at all when it can't be sure the hashes are right, and says why, offering
   Windows Settings instead.

## Where data is stored

- Settings: `HKCU\Software\FileAssocManager`
- Snapshots (for Undo): `%APPDATA%\FileAssocManager\Backups\*.fam`
- Program associations it creates: `HKCU\Software\Classes\FileAssocManager.<app>.<hash>`
  (per user, no administrator rights needed)

## Verification status

Please read this before relying on the tool.

- **Hash code:** the UserChoiceLatest hash passes the known-answer test published with
  `cssxn/UserChoiceLatestHash` (I have not checked that value against a PC myself). All 111 test
  vectors (classic and UserChoiceLatest) match a separate Python transcription of both algorithms
  (`tools/ref_hash.py`), so the C port and the reference agree. The C code was also run under
  AddressSanitizer and UBSan by hand; `make test` itself does not enable them.
- **Interface and flows:** the GUI was exercised under Wine (Windows API emulation on Linux):
  filtering, ticking, the application picker, the assign flow, rollback of refused changes, and the
  report dialog. Layout was checked at 96 DPI only.
- **Not tested on real Windows 10 or 11.** The registry writes and the Windows acceptance checks
  could not be confirmed here. On a PC where Windows does not accept the test change, the program
  says so and asks before writing anything.
- **Wine limitation:** Wine does not expose per-user ProgIds (`HKCU\Software\Classes`) to the
  shell's association lookup, so the live test always refuses under Wine. On a real PC the
  standard per-user merge applies, but this has not been observed here.

**First run on your PC:** open *Tools → Test that changes stick*, then assign one harmless format
(for example `.log`) to Notepad and check that *Default apps* in Settings shows it.

## Where the Windows details come from

- `UserChoiceLatest` hash: C port of `cssxn/UserChoiceLatestHash` (MIT, 2026 cssxn).
- Classic `UserChoice` hash: follows `PS-SFTA` (MIT, Danysys and contributors).
- The layout of a choice, its timestamp rules and the protection driver behaviour are taken from
  these projects and from Microsoft's documentation. Windows updates can change them, which is
  why every write is verified and rolled back.

Editing default-app records is a supported but unofficial route. Use at your own risk; every
change is undoable with *Undo last change*, and Windows Settings is always available as a fallback.
