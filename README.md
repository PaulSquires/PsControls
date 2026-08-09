# PsControls

Owner-drawn custom controls for **FreeBASIC** on **Win32**, built on the
[AfxNova](https://github.com/PaulSquires/AfxNova) framework.

Every control here draws itself, top to bottom. There is no `comctl32` control underneath and
nothing about the appearance depends on the visual style the user happens to be running, so a
window built from these looks the same on every machine and can be themed to whatever palette
your application uses — including a dark one — without fighting the system.

Each control lives in its own subdirectory, is documented by its own README, and ships with a
runnable demo.

---

## The controls

### Text, numbers and dates

| Control | What it is |
|---|---|
| [PsTextBox](PsTextBox/README.md) | A themeable frame — your border, corner radius and fill — wrapped around a real `RichEdit50W` that does the typing, selection, clipboard and undo. |
| [PsNumericUpDown](PsNumericUpDown/README.md) | A spinner: a rounded frame holding a `−` button, an editable numeric field and a `+` button. Ranged, with a settable number of decimals. |
| [PsDatePicker](PsDatePicker/README.md) | An editable date field with a calendar icon, plus a popup calendar the control owns. Typing and picking are two routes to the same `SYSTEMTIME`. |
| [PsCalendar](PsCalendar/README.md) | A month calendar that lives **inside** your window rather than dropping down. One month or a grid of up to twelve, with drill-down from days to months to years. |

### Buttons and switches

| Control | What it is |
|---|---|
| [PsButton](PsButton/README.md) | A momentary command button: rounded rectangle, optional glyph either side of an optional caption. Fires on release, never latches. |
| [PsCheckBox](PsCheckBox/README.md) | A checkbox drawing its box glyph from a font, with an optional caption on either side and a focus ring around the pair. |
| [PsOptionButton](PsOptionButton/README.md) | A radio button: outlined ring when unchecked, accent-filled disc with a contrasting dot when checked. |
| [PsToggle](PsToggle/README.md) | A toggle switch — a rounded pill with a knob that sits left for OFF and right for ON. For the settings row where a checkbox works but a switch reads better. |

### Lists and selection

| Control | What it is |
|---|---|
| [PsListTree](PsListTree/README.md) | A list **and** tree: owner-painted fixed-height rows, unlimited-depth nodes with expand/collapse twisties, in-place label editing, group headers, single/multiple/extended selection, and an optional multi-column report mode with a resizable header. |
| [PsComboBox](PsComboBox/README.md) | A dropdown selector — a button showing the current choice, dropping a list where exactly one item is checkmarked. Single selection, no text entry. |
| [PsSelectBar](PsSelectBar/README.md) | A flat row of text labels where one is current, marked by a coloured line under the word. Reads as view choices rather than as tabs. |
| [PsTabBar](PsTabBar/README.md) | A strip of tabs with room for an icon and a close X, clickable to select and draggable to reorder. **A tab bar, not a tab container** — it never owns the pages behind it. |

### Command surfaces

| Control | What it is |
|---|---|
| [PsMenuBar](PsMenuBar/README.md) | A horizontal strip of text items, each owning a dropdown. Hovering across the bar switches dropdowns without a second click. |
| [PsToolbar](PsToolbar/README.md) | A command strip of icon/caption buttons, separators, split buttons, dropdown buttons and cells holding a child window of your own. Items that no longer fit move into an overflow menu. |
| [PsIconPanel](PsIconPanel/README.md) | A run of glyph cells — latching toggles, momentary buttons and separator rules — laid out in the order you supply. Every cell width is declared, never measured. |
| [PsStatusBar](PsStatusBar/README.md) | A strip of panels along the bottom of a window, one of them able to stretch so the rest sit flush right. |

### Layout and scrolling

| Control | What it is |
|---|---|
| [PsSplitter](PsSplitter/README.md) | A draggable divider between two panes. It maintains exactly one number — its position — and **does not own, create, size or even know about** the panes either side. |
| [PsScrollPanel](PsScrollPanel/README.md) | A viewport onto a page of ordinary child controls taller than the space available, with a scrollbar in a reserved strip down the right edge. |
| [PsVScrollBar](PsVScrollBar/README.md) | A vertical scrollbar: flat track, flat thumb that brightens under the mouse and while dragged. For when you already own the scrolling. |
| [PsHScrollBar](PsHScrollBar/README.md) | Its horizontal twin, axis-transposed. |

### Popups and feedback

| Control | What it is |
|---|---|
| [PsMessageBox](PsMessageBox/README.md) | A modal message box — owner-drawn caption band, an optional icon beside a wrapped message, one to three buttons. For when `MessageBox()` does the job but looks like a different application. |
| [PsColorPicker](PsColorPicker/README.md) | A modal colour picker popup. `PsColorPicker_DoModal` puts a chromeless popup on screen and blocks until the user answers. |
| [PsTooltip](PsTooltip/README.md) | A `WS_POPUP` tip that appears near the cursor after a dwell, optionally wrapped under a bold title and icon glyph, and fades in and out. |
| [PsProgressBar](PsProgressBar/README.md) | A rounded track with a fill that grows along it — flat or gradient, continuous or blocked, independently. Switches to indeterminate when there is nothing to measure. |

## Support classes

Not controls. These are types the controls are built from, useful in your own painting code too.

| Class | What it is |
|---|---|
| [PsBufferPaint](PsBufferPaint/README.md) | The flicker-free drawing surface **everything** here paints through: you draw into an offscreen bitmap and it reaches the screen in one blit, so nothing is ever seen half-drawn. Declared as a local inside a `WM_PAINT` handler. |
| [PsImage](PsImage/README.md) | Loads a raster image — `.ico`, `.png`, `.bmp`, `.jpg`, or an existing `HBITMAP`/`HICON` — so an icon slot can hold a real picture instead of a glyph. |

Two further support types ship **inside** the controls that use them, rather than in folders of
their own: `PsTipHost` (the tooltip backend switch, in `PsTooltip/`) and `PsPopupMenu` (the
dropdown behind the menu, combo, toolbar and right-click menus). `PsColumnHeader`, the resizable
header band, ships inside `PsListTree/`.

---

## Requirements

- **AfxNova** — [github.com/PaulSquires/AfxNova](https://github.com/PaulSquires/AfxNova). Every
  control is a `CWindow`-derived type and `PsBufferPaint` draws through `AfxNova\CGdiPlus.inc`.
- **FreeBASIC** (64-bit `fbc64.exe` is what these are developed against).
- **Windows.** These call the Win32 API directly; there is no cross-platform layer.
- **Segoe Fluent Icons** for the demos and for any control drawing glyphs. It ships with
  Windows 11 but belongs to Microsoft, so it is **not redistributed here**. Copy it in once:

  ```bash
  copy C:\Windows\Fonts\SegoeIcons.ttf SegoeFluentIcons.ttf
  ```

  Note the rename — Windows stores the file as `SegoeIcons.ttf` while the family name is
  "Segoe Fluent Icons". On Windows 10 the font is not present at all.

Sources include AfxNova relative to the workspace root (`#include once "AfxNova\CWindow.inc"`),
so a build needs that root on the include path:

```bash
fbc64.exe -i "C:\dev" main.bas
```

## Using a control

Each control's README opens with a **Requirements** table naming every file to copy into your
project, and a **Quick start** showing the minimum that puts a working instance on screen. Start
there — the list is per-control, because a control brings its dependencies with it. `PsButton`,
for example, needs `PsBufferPaint`, `PsImage`, `PsTipHost` and `PsTooltip` alongside its own two
files.

## Running a demo

Every subdirectory has a `main.bas` demo exercising that control. From the control's directory:

```bash
fbc64.exe -i "C:\dev" main.bas
```

Some directories carry a `build.bat` that wraps this with the right compiler path.

## How the sources are arranged

**Each control directory is self-contained.** A control that needs `PsBufferPaint` carries its own
copy of `PsBufferPaint.bi`/`.inc` rather than reaching across to a shared one, so you can copy a
single directory into your project and build it with nothing else from this repository.

That means shared files exist as several content-identical copies — `PsBufferPaint` appears in all
26 directories, the tooltip pair in 14, `PsPopupMenu` in 7. **A fix to a shared file has to be
applied to every copy**, and the copies are expected to stay byte-identical.

## Licence

[Mozilla Public License 2.0](PsButton/LICENSE) — the same licence in every directory.

MPL-2.0 is file-level copyleft, chosen deliberately for a drop-in control:

- **You may use these in closed-source software**, commercial or otherwise. §3.2 permits static
  linking with no additional conditions.
- **If you modify these files, publish those files' changes.** The obligation is per-file — your
  own sources are unaffected however tightly they are combined with these.
- The Exhibit B "Incompatible With Secondary Licenses" notice is **not applied**, which keeps
  this GPL-compatible.
