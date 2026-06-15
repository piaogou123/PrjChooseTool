# PrjChooseTool

A small Qt desktop tool for switching which `Data_User` variant a project
uses, by updating the matching line in `Data_System/ProjectDefinition.dat`.

## What it does

1. Scans a working folder (e.g. `C:/i-Novatrol`) for sub-folders named
   `Data_User.<ProjectId>.<Variant>` (for example `Data_User.F306.WFiber`,
   `Data_User.F306.WFiber2`) and groups them by project id.
2. Reads `Data_System/ProjectDefinition.dat`, where each line has the form
   `<ProjectId>,Data_User.<ProjectId>.<Variant>`, and marks the variant each
   project currently uses.
3. You pick one project from the horizontal selector, choose the variant you
   want, and click **Apply selection**.
4. Only that project's line is rewritten, e.g.
   `F306,Data_User.F306.WFiber` becomes `F306,Data_User.F306.WFiber2`.
   Other lines, line endings and the trailing newline are preserved, and a
   `ProjectDefinition.dat.bak` backup is written first.

The last-used working folder is remembered between launches (via `QSettings`);
the field is empty on first run.

## Build

Open `PrjChooseTool.pro` in Qt Creator and Build & Run, or from a shell:

```
qmake
make        # or nmake / mingw32-make on Windows
```

The project builds with both Qt 5 and Qt 6.

## Files

- `main.cpp` — application entry point
- `mainwindow.h` / `mainwindow.cpp` — the chooser window and all logic
- `PrjChooseTool.pro` — qmake project file
