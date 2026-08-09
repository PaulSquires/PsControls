# PsPopupMenu

An owner-drawn floating menu for FreeBASIC Win32 applications: a bordered panel of rows, each
with an optional check glyph on the left, a caption, an optional right-aligned accelerator
label, and an optional submenu chevron on the right.

On its own it is a themeable replacement for `TrackPopupMenu`. Right-click, call
`PsPopupMenu_Show` at the cursor, and the menu appears — but unlike `TrackPopupMenu` it does not
run a modal loop, it draws every pixel itself, and every colour it paints is one you set.
Submenus nest to any depth, because a submenu is just another `PsPopupMenu` attached to a row.

One popup window is long-lived: create it once, fill it with items, then show and hide it as
often as you like. It never takes focus and never takes activation, so your window's title bar
stays painted active while a menu is up.

It is also the dropdown used by [PsMenuBar](README.md). Nothing in this document requires the
bar — with one exception, called out below: which filter function you call in your message pump.

---

## Requirements

**Files to copy into your project:**

| File | Purpose |
|---|---|
| `PsPopupMenu.bi` | Declarations — types, callbacks, constants, function prototypes |
| `PsPopupMenu.inc` | Implementation |
| `PsBufferPaint.bi` | The flicker-free drawing surface the control paints through |
| `PsBufferPaint.inc` | Its implementation |

Those four files are the whole of it. `PsMenuBar.bi` / `PsMenuBar.inc` are **not** needed unless
you also want the horizontal bar — the dependency runs one way, and `PsPopupMenu` knows nothing
about `PsMenuBar`.

**AfxNova is required.** The control is built on `CWindow`, and `PsBufferPaint` draws through
`AfxNova\CGdiPlus.inc`. Sources include AfxNova relative to the workspace root
(`#include once "AfxNova\CWindow.inc"`), so builds need the workspace root on the include path:

```bash
fbc64.exe -i "C:\dev" main.bas
```

**Include order.** `PsPopupMenu.inc` pulls in its own `.bi`, which pulls in `PsBufferPaint.bi`.
The two implementation files are included in this order, after the AfxNova headers and after
any colour/font globals you want the control's callbacks to see:

```freebasic
#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova

#include once "PsBufferPaint.inc"
#include once "PsPopupMenu.inc"
```

**GDI+ must be running before the first repaint and must outlive the last one.** The control
renders through GDI+, so bracket your message loop:

```freebasic
dim as ULONG_PTR gdipToken = AfxGdipInit()
' ... create windows, run the message loop ...
AfxGdipShutdown( gdipToken )
```

`AfxGdipShutdown` must come after every window is destroyed, because each repaint builds and
tears down a `PsBufferPaint`. If you also use COM, initialise it before GDI+ and uninitialise it
after — GDI+ leans on COM.

**Never name an identifier `ok`.** GDI+ defines `Ok = 0` as a `Status` enum value in namespace
`AfxNova`, and hosts customarily say `using AfxNova`. An existing variable, parameter or
function called `ok` becomes a duplicate definition the moment you adopt these files. Use `bOK`
instead.

### The message-pump filter is mandatory

Keyboard navigation and click-outside dismissal both live in `PsPopupMenu_FilterMessage`. The
popup never takes focus, so keystrokes keep going to your own window — the filter is the only
thing that sees them:

```freebasic
dim uMsg as MSG
do while GetMessage( @uMsg, null, 0, 0 )
    if PsPopupMenu_FilterMessage( HWND_CTXMENU, @uMsg ) then continue do
    TranslateMessage( @uMsg )
    DispatchMessage( @uMsg )
loop
```

Leave that line out and the menu still opens, still paints, and still executes rows you click.
What you lose is everything else: **arrow keys, Home/End, Enter and Escape do nothing, and the
menu never closes when you click outside it.** Stray typing also leaks through to whatever
control has focus behind the menu.

Call it once per popup **chain root** you can open — that is, once per menu you hand to
`PsPopupMenu_Show`. Submenus need no call of their own: the filter accepts any handle in a
chain and resolves the root itself. A call for a menu that is not currently open costs a few
comparisons and returns FALSE, so a host with three context menus can simply list all three.

**If you are using [PsMenuBar](README.md), call `PsMenuBar_FilterMessage` for the bar** — it
wraps this filter for the bar's own dropdowns and adds the bar-boundary keys. It does **not**
cover popups you show yourself. A host with both a menu bar and a standalone context menu calls
both:

```freebasic
do while GetMessage( @uMsg, null, 0, 0 )
    if PsMenuBar_FilterMessage( HWND_MENUBAR, @uMsg ) then continue do
    if PsPopupMenu_FilterMessage( HWND_CTXMENU, @uMsg ) then continue do
    TranslateMessage( @uMsg )
    DispatchMessage( @uMsg )
loop
```

---

## Quick start

Build the menu once, at startup:

```freebasic
' Create it. hwnd is your top-level window: it owns the popup.
HWND_CTXMENU = PsPopupMenu_Create( hwnd )
PsPopupMenu_SetColors( HWND_CTXMENU, @gPopupColors )
PsPopupMenu_SetFonts( HWND_CTXMENU, ghFont(GUIFONT_10) )
PsPopupMenu_SetSelectCallback( HWND_CTXMENU, @MyApp_ContextSelect )

PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_INSERT, "Insert Row" )
PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_DELETE, "Delete Row", "Del" )
PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_RENAME, "Rename...", "F2" )
PsPopupMenu_AddSeparator( HWND_CTXMENU )
PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_ADVANCED, "Advanced" )

' A submenu is just another popup attached to a row. Ownership transfers.
dim as HWND hAdv = PsPopupMenu_Create( hwnd )
PsPopupMenu_SetColors( hAdv, @gPopupColors )
PsPopupMenu_SetFonts( hAdv, ghFont(GUIFONT_10) )
PsPopupMenu_AddItem( hAdv, IDM_CTX_ADV_ONE, "Advanced Option One" )
PsPopupMenu_AddItem( hAdv, IDM_CTX_ADV_TWO, "Advanced Option Two" )
PsPopupMenu_SetItemSubMenu( HWND_CTXMENU, IDM_CTX_ADVANCED, hAdv )
```

Show it from your right-click handler:

```freebasic
case WM_RBUTTONDOWN
    dim as POINT pt = ( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
    ClientToScreen( hwnd, @pt )         ' Show takes SCREEN coordinates
    PsPopupMenu_Show( HWND_CTXMENU, pt.x, pt.y )
    return 0
```

Handle the chosen command:

```freebasic
sub MyApp_ContextSelect( byval hPopup as HWND, byval id as long )
    ' The whole chain is already closed by the time this runs, so you can open a
    ' dialog here without fighting a live menu.
    select case id
        case IDM_CTX_INSERT : ' ...
        case IDM_CTX_DELETE : ' ...
    end select
end sub
```

And close it instantly when the app loses the foreground, rather than waiting for the
control's own poll:

```freebasic
case WM_ACTIVATEAPP
    if wParam = FALSE then PsPopupMenu_CloseChain( HWND_CTXMENU )
```

Plus the mandatory filter line in your pump, above. That is the whole minimum.

---

## Concepts

### The handle is a real HWND

`PsPopupMenu_Create` returns an ordinary window handle, and every `PsPopupMenu_*` function takes
it. It is not an opaque type, so `IsWindowVisible` legitimately tells you whether the menu is
up (which is exactly what `PsPopupMenu_IsOpen` does).

The window is `WS_POPUP` with `WS_EX_TOOLWINDOW or WS_EX_NOACTIVATE`, created hidden and
zero-sized. It carries `CS_DBLCLKS` and `CS_DROPSHADOW` — the shadow honours the system
drop-shadow setting and silently degrades to no shadow when that is off.

### Create once, show many times

There is no create-per-open. One popup window lives for as long as your form does; `Show` and
`Hide` are cheap. Sizing happens inside `Show`: the window is sized to its measured content
every time, so a menu whose items changed since the last open comes up at the new size.

### It never activates

`WM_MOUSEACTIVATE` answers `MA_NOACTIVATE`, and the window carries `WS_EX_NOACTIVATE`. Your
window keeps focus and keeps its active title bar the whole time a menu is up. That is what
makes the pump filter necessary — keystrokes never arrive at the popup, so something in your
pump has to hand them over.

### Ids address the tree; indices address the geometry

Everything that takes `byval id as long` searches **this** menu first, then recurses depth-first
into attached submenus, first match wins. So a host can flip a checkmark three levels down by
calling the setter on the root. Ids should therefore be unique across the whole tree.

Geometry is the other way around: `PsPopupMenu_GetItemRect` and `PsPopupMenu_HitTest` work in
**row indices**, because rects are per-row and have nothing to do with commands.

**Id 0 is reserved for separators and never matches**, so a row added with id 0 cannot be
reached by any of the by-id functions.

### Rows are not all the same height

A command row is `nItemHeight` tall (default 24, DPI-scaled at create). A **separator row is
`nSeparatorHeight`** (default 9) — a rule with a little air around it, not a full-height row.

Layout therefore walks a running `y` down the list instead of multiplying an index by a stride,
and everything downstream reads the resulting rects: hit-testing, painting, invalidation and
submenu anchoring. Keyboard navigation skips separators by *flag*, not by geometry. Nothing you
can do from outside re-introduces a uniform stride — if you want the old uniform look, set the
separator height equal to the item height.

### The separator rule is its own thing

The rule drawn inside a separator row has:

- its own colour, `SeparatorColor` in `PSPOPUPMENU_COLORS` — a divider and greyed-out text are
  different things, and you probably already own a divider colour you want menus to match;
- its own weight, `nSeparatorThickness` (default 1), set with
  `PsPopupMenu_SetSeparatorThickness`, independent of the row height it sits in;
- a horizontal inset of `nItemHeight \ 2` on each side, derived from the **item** height so it
  tracks the menu's overall scale and does not shrink just because the separator row is short.

`SeparatorColor` is **the one field in `PSPOPUPMENU_COLORS` that carries a non-zero default**
(mid grey). Fill the struct's other fields and leave this one alone and you get a visible grey
rule, not the black that a zero-initialised `COLORREF` would give you. Set it explicitly if you
actually want black.

### Geometry is derived, never assigned

You never write a rect. You influence the layout through the setters and the content, and the
control computes everything:

```
  contentW = max( nMinWidth,
                  checkCol + widestCaption + accelBlock + chevronCol )
             where accelBlock = accelGap + widestAccel, and is 0 unless
             at least one row actually has accelerator text

  idealW   = contentW + 2*hPadding + 2*borderWidth
  idealH   = sum of row heights + 2*vPadding + 2*borderWidth
             where a row is itemHeight unless it is a SEPARATOR,
             which gets separatorHeight

  row rect = left  = borderWidth + hPadding
             right = left + contentW
             top   = running y, starting at borderWidth + vPadding
```

Each row is then partitioned left to right into three cells:

| Cell | Width | Holds |
|---|---|---|
| `rcCheck` | `nCheckColWidth` | The check glyph, centred |
| `rcText` | whatever is left | Caption `DT_LEFT`, accelerator `DT_RIGHT`, same rect |
| `rcChevron` | `nChevronColWidth` | The submenu chevron, centred |

Layout is lazy. A mutator marks the layout stale and asks for a repaint; the next paint, show,
or rect query recomputes it. A burst of `AddItem` calls rebuilding a whole menu costs one
measuring pass, not one per item — which is what makes the init-popup rebuild pattern cheap.

Note that the rows are inset by `nHPadding` on each side, so the strips of window either side of
the rows belong to the panel background. A hit test there returns -1.

### Pixels, and who scales them

The creation-time defaults are DPI-scaled for you. One is deliberately not:

| Setting | Default | DPI-scaled at create? |
|---|---:|---|
| Item height | 24 | Yes |
| Separator row height | 9 | Yes |
| Check column width | 30 | Yes |
| Chevron column width | 20 | Yes |
| Accelerator gap | 24 | Yes |
| Horizontal padding | 2 | Yes |
| Vertical padding | 8 | Yes |
| Border width | 1 | Yes (floored at 1) |
| Separator thickness | 1 | **No** |

Every setter afterwards takes **raw pixels** and expects you to scale — typically
`pWindow->ScaleX(...)` / `ScaleY(...)`. The separator thickness should not be scaled at all: a
hairline divider should stay a hairline at 150% rather than growing into a bar.

### One chain is open at a time

The open chain is root → submenu → submenu…, and only one such chain exists in the process at
any moment. This is the same rule real Windows menus follow. Showing a popup as a root silently
closes any other chain that was open first.

### Hover is selection

There is no separate hot row: one highlight, moved by the mouse and the keyboard alike. Resting
the cursor on a row for 250 ms opens its submenu (or closes another row's submenu when therested row has none); clicking a submenu row opens it immediately, without the wait.

While a level has an open submenu its highlight is **pinned** to the parent row, so crossing the
gap into the submenu can never strip the parent's highlight.

The control remembers whether the current selection was set by the mouse or by the keyboard,
and only mouse-owned selections are cleared when the cursor leaves. A keyboard highlight
survives with the cursor anywhere on screen.

### Executing a command

A row executes on the **left button release over it**, and only if it is an enabled command row
— separators, disabled rows and submenu parents never execute. Press-drag-release works: press
on one window, drag onto a row, release, and that row fires, which is the classic menu gesture.

Dispatch order is fixed and worth knowing: the id is captured, then the **whole chain is closed**,
then the notify window is sent `CPM_NOTIFY_COMMAND`, then the root's select callback runs. So by
the time your handler is called there is no live popup to fight — you can open a dialog, or even
re-open a menu, immediately.

Enter on the selected row does exactly the same thing, except on a submenu parent, where Enter
opens the submenu instead.

### Keyboard, through the filter

While a chain is open, the filter consumes **every** `WM_KEYDOWN`, `WM_KEYUP` and `WM_CHAR`, so
stray typing cannot reach an editor behind the menu. The `WM_SYSKEY*` family is deliberately
left alone — Alt handling stays yours.

| Key | Effect on the innermost open level |
|---|---|
| Down | Next selectable row, **wrapping** past the end to the first |
| Up | Previous selectable row, wrapping; from nothing selected, the last |
| Home | First selectable row |
| End | Last selectable row |
| Right | Open the selected row's submenu with its first row selected; a row without one is a consumed no-op |
| Left | Close this submenu level, parent keeps the highlight; at the root, a consumed no-op |
| Enter | Execute the selected row, or open its submenu |
| Escape | Close this submenu level; at the root, close the whole chain |

"Selectable" means neither a separator nor disabled. Down/Up/Home/End all route through the same
wrapping walk, so a menu whose every row is disabled simply keeps no selection.

The two consumed no-ops — Right on a plain row, Left at the root — are the seams
[PsMenuBar](README.md) fills in: under a bar they become "step to the next / previous bar item".

### Dismissal

Four things close an open chain:

| Trigger | Notifies? | Reason code |
|---|---|---|
| A click outside the chain's windows | Yes, `CPM_NOTIFY_CLOSED` | 0 |
| Escape at the root level | Yes, `CPM_NOTIFY_CLOSED` | 1 |
| The foreground moves outside your window and the chain | Yes, `CPM_NOTIFY_CLOSED` | 0 |
| `PsPopupMenu_Hide` / `PsPopupMenu_CloseChain` | **No** — programmatic is silent | — |

**The outside click still reaches its target.** The filter returns FALSE for it on purpose, so
clicking a toolbar button while a menu is open both closes the menu and presses the button.

The foreground watch is a 100 ms poll, because a window that never activates cannot see focus
loss itself and `WM_ACTIVATEAPP` is sent to your window rather than pumped. If you want instant
dismissal on an app switch, call `PsPopupMenu_CloseChain` from your own `WM_ACTIVATEAPP(FALSE)`
handler, as in the quick start.

### Lifetime and ownership

The control frees itself when its window is destroyed.

Attaching a popup with `PsPopupMenu_SetItemSubMenu` **transfers ownership**: the parent destroys
the attached window when the parent itself is destroyed, so a single `DestroyWindow` on the root
unwinds an entire menu tree. `PsPopupMenu_DeleteItem` and `PsPopupMenu_Clear` also destroy the
submenu windows of the rows they remove — which is why a menu rebuilt from an init-popup callback
has to re-create and re-attach any submenus it had.

**Never attach the same popup to two parents.** Both would destroy it.

A root popup is created with `hOwner` as its owner window, so Windows disposes of it along with
that window; you can also `DestroyWindow` it yourself. Fonts you pass in stay yours — the
control borrows them and never deletes one.

---

## Behaviour and limits

Firm properties of the control, not settings:

- **The pump filter is not optional.** Without `PsPopupMenu_FilterMessage` in your message loop
  there is no keyboard navigation and no outside-click dismissal. See Requirements.
- **`PsMenuBar_FilterMessage` does not cover popups you show yourself.** It wraps this filter
  only for the bar's own dropdowns. Standalone menus need their own call.
- **`Show` is not modal and returns immediately.** There is no return value carrying the chosen
  command — the choice arrives later, through the select callback or `CPM_NOTIFY_COMMAND`. This
  is the one real difference from `TrackPopupMenu` at the call site.
- **One chain open at a time, process-wide.** Showing a popup as a root closes any other open
  chain, silently.
- **Rows have no scrolling.** A menu taller than the work area is clamped so its top edge stays
  on screen, which means the bottom rows fall off it. Keep menus to a screen's worth of rows,
  or split them into submenus.
- **Id 0 is reserved for separators** and is never found by the by-id functions.
- **Ids must be unique across the tree**, because the by-id search takes the first match found
  depth-first from the popup you called.
- **Accelerator text is display only.** `wszAccel` is drawn right-aligned in the row and nothing
  more; registering the actual accelerator is your job.
- **No mnemonics, no type-ahead.** Letter keys are consumed while a chain is open, not matched
  against captions.
- **No icons or bitmaps.** The left gutter holds a check glyph, drawn as text; there is no
  `HBITMAP` or `HICON` path.
- **The check glyph and chevron are text**, drawn with the glyph font if you set one and the
  text font otherwise. Change the characters with `PsPopupMenu_SetGlyphs`.
- **No radio-group items.** A checkmark is per-row state; mutual exclusion is yours to enforce.
- **Separator thickness clamps to a minimum of 1**, so there is no "invisible rule" setting. Set
  `SeparatorColor` equal to `BackColor` if you want a separator that only takes up space.
- **Item and separator heights clamp to a minimum of 1**; `nMinWidth` clamps to 0.
- **No mouse capture is taken anywhere.** Selection follows hover and execution is the release
  over a row, so nothing consumes the down→up pairing.
- **Disabled rows never light up.** There is no disabled *background* colour — a disabled row
  draws `ForeColorDisabled` over the normal background. Use a paint callback if you need more.
- **The border is painted after the rows**, so it is authoritative no matter what a paint
  callback drew. Its width is fixed at DPI-scaled 1 and there is no setter for it.
- **`WM_SYSKEY*` is never consumed.** Whatever Alt does in your application is unaffected by an
  open menu.

---

## API reference

### Creation

| Function | Description |
|---|---|
| `PsPopupMenu_Create( hOwner, CtrlID = 0 ) as HWND` | Creates the popup window, hidden and zero-sized, and returns its handle. `hOwner` is your top-level window: it owns the popup's z-order, hides it on minimize, and anchors the foreground watch. `CtrlID` is stored and set as the window's `GWLP_ID` for your own bookkeeping — the control never routes anything by it, and there is no getter. |

### Items

`AddItem` and `AddSeparator` append and return the new **row index**, or -1. Every function
below taking `id` searches this menu then recurses into attached submenus, first match wins. All
of them are silent — no callbacks, no notifications.

| Function | Description |
|---|---|
| `PsPopupMenu_AddItem( hPopup, id, Caption, Accel = "", bDisabled = false, bChecked = false ) as long` | Appends a command row. `Accel` is right-aligned display text such as `"Ctrl+O"` and registers nothing. **`id` 0 is reserved for separators** and makes the row unreachable by the by-id API. Returns the new index. |
| `PsPopupMenu_AddSeparator( hPopup ) as long` | Appends a separator row: id 0, never selectable, `nSeparatorHeight` tall rather than `nItemHeight`. Returns the new index. |
| `PsPopupMenu_GetItemCount( hPopup ) as long` | Rows in **this** popup, separators included. Does not count submenu rows. |
| `PsPopupMenu_SetItemText( hPopup, id, Caption, Accel = "" ) as boolean` | Retitles the row. Note the default: calling it without `Accel` **clears** any accelerator text the row had. Re-lays out, because captions drive the measured width. |
| `PsPopupMenu_SetItemEnabled( hPopup, id, bEnabled ) as boolean` | Enables or disables a row. A disabled row is skipped by keyboard navigation, refuses to execute, and refuses to open its submenu. Repaints that row only. |
| `PsPopupMenu_GetItemEnabled( hPopup, id ) as boolean` | The row's enabled state. FALSE when the id is not found, which is indistinguishable from a genuinely disabled row. |
| `PsPopupMenu_SetItemChecked( hPopup, id, bChecked ) as boolean` | Shows or hides the check glyph in the row's left gutter. Repaints that row only. |
| `PsPopupMenu_GetItemChecked( hPopup, id ) as boolean` | The row's checked state. FALSE when the id is not found. |
| `PsPopupMenu_DeleteItem( hPopup, id ) as boolean` | Removes the row and **destroys any submenu window attached to it**. Rows below shift up, so held indices go stale. Clears the selection if it pointed past the new end. |
| `PsPopupMenu_Clear( hPopup )` | Removes every row from **this** popup — not from the tree above it — and **destroys the submenu windows** of the rows it removes. Clears the selection and unlinks any open child. The rebuild half of the init-popup pattern. |

> Because `Clear` and `DeleteItem` destroy attached submenus, a menu you rebuild on every open
> must re-create and re-attach its submenus in the same pass. The alternative is to detach them
> first with `PsPopupMenu_SetItemSubMenu( hPopup, id, 0 )`, which returns ownership to you.

### Submenus

| Function | Description |
|---|---|
| `PsPopupMenu_SetItemSubMenu( hPopup, id, hSubMenu ) as boolean` | Attaches a popup window as the row's submenu. **Ownership transfers** — the parent destroys it. Attaching over an existing submenu **destroys the old one**. `hSubMenu = 0` detaches without destroying, returning ownership to you. FALSE when the id is not found. |
| `PsPopupMenu_GetItemSubMenu( hPopup, id ) as HWND` | The attached submenu window, or 0. |

Nesting depth is not limited by the control; it falls out of the data model. Chain walks are
guarded at 32 levels, which no real menu approaches.

### Show and dismiss

Coordinates are **screen** pixels. Both `Show` forms are non-blocking, always show the popup as
a chain **root**, and in order: fire the popup's init-popup callback, run any pending layout,
size the window to the measured content, clamp it to the work area of the nearest monitor, then
show it without activation. Showing a root closes any other open chain first.

| Function | Description |
|---|---|
| `PsPopupMenu_Show( hPopup, x, y ) as boolean` | Shows the menu with its top-left corner at a screen point — the context-menu door. Nothing is selected. FALSE when the content could not be measured. |
| `PsPopupMenu_ShowForRect( hPopup, rcAnchor, nAlign ) as boolean` | Shows the menu against a screen anchor rect. `PM_ALIGN_BELOW` aligns left edges and puts the top at `rcAnchor.bottom - 1`, so the popup's border merges with the anchor's edge; `PM_ALIGN_RIGHT` puts it at `rcAnchor.right / rcAnchor.top` and **flips to the anchor's left side** when it would leave the work area. Nothing is selected. |
| `PsPopupMenu_Hide( hPopup )` | Hides this level and any open descendants, innermost first, and unlinks it from its parent. **Silent.** Hiding a submenu level leaves the rest of the chain open. |
| `PsPopupMenu_CloseChain( hPopup )` | Closes the whole chain containing `hPopup`, from any level. **Silent.** This is the instant-dismissal hook for your `WM_ACTIVATEAPP(FALSE)` handler. |
| `PsPopupMenu_IsOpen( hPopup ) as boolean` | Is this popup currently visible? |

Clamping applies on both axes: the popup is pushed left when it would overflow the right edge
(or flipped, for `PM_ALIGN_RIGHT`), pushed up when it would overflow the bottom, and never
allowed past the work area's top or left. A menu taller than the work area therefore keeps its
top on screen and loses its bottom rows.

### Selection

All three are **silent** — only user interaction fires callbacks — and none of them is
hover-owned, so the hover-clear paths leave what they set alone.

| Function | Description |
|---|---|
| `PsPopupMenu_GetCurSel( hPopup ) as long` | The highlighted row index, or -1 for none. |
| `PsPopupMenu_SetCurSel( hPopup, idx ) as boolean` | Moves the highlight, repainting only the two affected rows. `-1` clears it. FALSE for any other out-of-range index. Note it does **not** refuse separators or disabled rows — it moves the highlight exactly where you say. |
| `PsPopupMenu_SelectFirst( hPopup ) as boolean` | Selects the first row that is neither a separator nor disabled — what opening a menu from the keyboard wants. FALSE when the menu has no selectable row. |

### Geometry and layout

Queries force a pending layout first, so results are always current — including before the first
paint. Setters take **raw pixels**; you do the DPI scaling.

| Function | Description |
|---|---|
| `PsPopupMenu_GetItemRect( hPopup, idx, byref rc ) as boolean` | The row's rect in client coordinates, **by index**. FALSE with an empty rect for an invalid index. |
| `PsPopupMenu_HitTest( hPopup, x, y ) as long` | Which row is this **client** point over? -1 for none — window padding, the border, or outside. The exact inverse of `GetItemRect`, and the same routine the mouse handlers use. |
| `PsPopupMenu_GetIdealSize( hPopup, byref nWidth, byref nHeight ) as boolean` | The measured window size, content plus padding plus border, using the formulas above. FALSE with both outputs 0 when the content could not be measured. You rarely need this — `Show` sizes the window itself. |
| `PsPopupMenu_SetMinWidth( hPopup, nMinWidth ) as boolean` | Floors the **content** width in pixels, so the window is at least `nMinWidth + 2*hPadding + 2*border` wide. Clamped to a minimum of 0, where 0 means auto. Use it to stop a menu whose contents change on every open from resizing under the cursor. |
| `PsPopupMenu_SetItemHeight( hPopup, nItemHeight )` | Height of a command row. Clamped to a minimum of 1. Also sets the separator rule's horizontal inset, which is half this value. |
| `PsPopupMenu_SetSeparatorHeight( hPopup, nSeparatorHeight )` | Height of a **separator row** — the air around the rule, not the rule. Clamped to a minimum of 1. Independent of the item height on purpose: a compact menu usually still wants the same thin divider. Set it equal to the item height for uniform rows. |
| `PsPopupMenu_SetSeparatorThickness( hPopup, nThickness )` | The rule's own weight in pixels. Clamped to a minimum of 1. **Do not DPI-scale this value** — a hairline should stay a hairline. |

### Appearance

| Function | Description |
|---|---|
| `PsPopupMenu_SetColors( hPopup, pColors as PSPOPUPMENU_COLORS ptr )` | Copies the whole struct in and repaints. Your struct stays yours. |
| `PsPopupMenu_SetFonts( hPopup, hTextFont, hGlyphFont = 0 )` | Sets the caption/accelerator font and the glyph font. **`hTextFont` is also the measuring font**, so this re-lays out. `hGlyphFont = 0` means "use the text font". Both are **borrowed** — keep them alive and delete them yourself. |
| `PsPopupMenu_SetGlyphs( hPopup, wszCheck, wszChevron )` | The characters drawn for a checked row and for a submenu parent. Defaults are `chr(&h2713)` (check mark) and `chr(&h203A)` (single right angle quote), which render in any text font. Repaints without re-laying out — the glyph columns are fixed width. |

To change one colour, read-modify-write is not available: there is no `GetColors`. Keep your
`PSPOPUPMENU_COLORS` struct, change the field, and call `SetColors` again.

### Plumbing and callback registration

| Function | Description |
|---|---|
| `PsPopupMenu_FilterMessage( hPopup, pMsg as MSG ptr ) as boolean` | **The message-pump hook, and it is mandatory.** Call it once per pumped message; skip `TranslateMessage`/`DispatchMessage` when it returns TRUE. Accepts any handle in the popup's chain and acts only while that chain is the open one, so a call for a closed menu is a cheap FALSE. Consumes every `WM_KEYDOWN`/`WM_KEYUP`/`WM_CHAR` while a chain is open, except the `WM_SYSKEY*` family. **Returns FALSE for the outside click that dismissed the menu, on purpose**, so that click still reaches its target. |
| `PsPopupMenu_SetNotifyWindow( hPopup, hNotifyWnd )` | Where `CPM_NOTIFY_COMMAND` / `CPM_NOTIFY_CLOSED` are sent. 0 (the default) means no messages are sent. The callbacks and these messages are two doors onto the same events — use whichever suits. |
| `PsPopupMenu_SetSelectCallback( hPopup, usersub )` | Installs the handler told when a command is executed. Only the **chain root's** callback is consulted, so a submenu's own select callback is never called. |
| `PsPopupMenu_SetInitPopupCallback( hPopup, usersub )` | Installs the just-in-time hook fired as **this** popup becomes visible. Each level has its own. |
| `PsPopupMenu_SetPaintItemCallback( hPopup, usersub )` | Installs a renderer that draws every row of this popup **instead of** the built-in painter. All rows or none. |

All callbacks are optional and independent.

---

## Colors

The colour surface is one flat struct, `PSPOPUPMENU_COLORS`, copied on `SetColors`.

| Field | Paints | Default |
|---|---|---|
| `BackColor` | The window panel — rows, padding and the strips beside the rows — and the normal row background | 0 (black) |
| `ForeColor` | Caption and accelerator text on a normal row | 0 |
| `BackColorHot` | The selected / hovered row's fill | 0 |
| `ForeColorHot` | The selected / hovered row's text | 0 |
| `ForeColorDisabled` | Disabled text, and the accelerator text of disabled rows | 0 |
| `BorderColor` | The window border, drawn as four filled strips after the rows | 0 |
| `SeparatorColor` | The separator rule | **`BGR(128,128,128)`** |

`SeparatorColor` is the only field with a non-zero default. Everything else is black until you
set it, so a popup you never call `PsPopupMenu_SetColors` on is unreadable — fill the struct.

### Which colour wins

The built-in painter picks one fill and one text colour per row, in this precedence:

```
disabled   >   hot (selected or hovered)   >   normal
```

A **disabled row never lights up**: it draws `ForeColorDisabled` over the *normal* `BackColor`,
never over `BackColorHot`. There is deliberately no disabled background colour — use a paint
callback if your design needs one.

A **separator ignores all of it**: `BackColor` fill, then the rule in `SeparatorColor`. It is
never hot, because it is never selectable.

### What the painter draws

| Part | Shape |
|---|---|
| Row background | A filled rect over the whole row |
| Check glyph | `wszCheckGlyph` centred in `rcCheck`, in the glyph font, only when the row is checked |
| Caption | `DT_LEFT` in `rcText`, in the text font |
| Accelerator | `DT_RIGHT` in the **same** `rcText`, only when the row has accelerator text |
| Chevron | `wszChevronGlyph` centred in `rcChevron`, in the glyph font, only when the row has a submenu |
| Separator | `BackColor` fill, then a horizontal line `nSeparatorThickness` thick in `SeparatorColor`, at the row's vertical middle, inset `nItemHeight \ 2` from each end |
| Border | Four filled strips in `BorderColor`, drawn **after** every row, so a paint callback cannot overwrite it |

---

## Callbacks

### Select

```freebasic
type PM_SelectCallbackSub as sub( byval hPopup as HWND, byval id as long )
```

An enabled command row was executed — a left-button release over it, or Enter. Fires on the
chain's **root** popup, so a submenu's own select callback never runs; register one callback on
the menu you show and it hears about the whole tree. `hPopup` is that root, not the level the
row lives on.

It fires **after the whole chain has closed**, which is what lets the handler open a dialog, or
re-open a menu, without fighting a live popup.

User interaction only. Nothing you do programmatically fires it.

### Init popup

```freebasic
type PM_InitPopupCallbackSub as sub( byval hPopup as HWND )
```

**This** popup is about to become visible. Each level fires its own, as it opens, before its
layout runs.

This is the just-in-time hook: flip enabled and checked states to match your application, retitle
items, or rebuild a whole dynamic section with `PsPopupMenu_Clear` plus `PsPopupMenu_AddItem`.
Mutations made here are coalesced into the single layout pass that immediately follows, so a
full rebuild costs one measuring pass.

Remember that `Clear` destroys the submenu windows of the rows it removes, so a rebuild must
re-create and re-attach any submenus.

It fires for programmatic `Show` calls too — it reports a window becoming visible, not a user
gesture.

### Paint item

```freebasic
type PM_PaintItemCallbackSub as sub( byval p as PSPOPUPMENU_PAINTINFO ptr )
```

Draws one row **instead of** the built-in painter. Setting the callback takes over **every** row
of that popup, separators included — it is all rows or none.

Paint through `p->b`, the control's double buffer for this repaint; do not touch the screen DC.
Draw with the same fonts you handed to `PsPopupMenu_SetFonts`, because the widths were measured
with them.

The control fills the whole window with `BackColor` before the row loop, and paints its border
**after** it, so chrome stays authoritative whatever you draw.

`PSPOPUPMENU_PAINTINFO` carries everything you need:

| Field | Meaning |
|---|---|
| `hPopup` | The popup, so the callback can query it |
| `itemID` | The row index within this popup |
| `id` | The row's command id (0 for separators) |
| `b` | The control's `PsBufferPaint` for this repaint (borrowed, not owned) |
| `rc` | The full row rect — **fill this** |
| `rcCheck` | Left gutter; centre the check glyph here |
| `rcText` | Caption `DT_LEFT`, accelerator `DT_RIGHT`, same rect |
| `rcChevron` | Right column; centre the submenu chevron here |
| `isSeparator` | Draw a rule, not a row |
| `isDisabled` | The row refuses input |
| `isChecked` | Draw the check glyph |
| `hasSubMenu` | Draw the chevron |
| `isHot` | This row is the current selection or hover |
| `wszCaption` | The caption text |
| `wszAccel` | The accelerator text, `""` when none |

The three cell rects partition `rc` left to right. Use them as handed to you rather than
re-deriving insets — they are exactly the space the measured width accounted for, and the column
widths can change underneath you.

> **A paint callback that fills a rectangle covering more than its own row will erase its
> neighbours.** Fill `p->rc`, not the client area.

### The notification messages

Registered with `PsPopupMenu_SetNotifyWindow`, and **sent** (not posted) to that window. They
carry the same two events as the callbacks, for a host that would rather handle them in a window
procedure.

| Message | wParam | lParam | When |
|---|---|---|---|
| `CPM_NOTIFY_COMMAND` | The command id | The chain's root popup `HWND` | An enabled command row was executed. The chain is **already closed**. Sent before the select callback. |
| `CPM_NOTIFY_CLOSED` | 1 when closed by Escape at the root, 0 for every other dismissal | The chain's root popup `HWND` | The chain was dismissed by **user** interaction without executing anything — an outside click, Escape, or foreground loss. |

Programmatic `Hide` and `CloseChain` send nothing. The Escape-versus-everything-else distinction
in `CPM_NOTIFY_CLOSED` exists because they mean different things to a host: Escape is "back out
one step, I am still driving with the keyboard", an outside click is "I am done here".

---

## Constants

Anchor alignment for `PsPopupMenu_ShowForRect`:

| Constant | Value | Meaning |
|---|---:|---|
| `PM_ALIGN_BELOW` | 1 | Left edges align, top = `rcAnchor.bottom - 1` so the borders merge |
| `PM_ALIGN_RIGHT` | 2 | Left = `rcAnchor.right`, top = `rcAnchor.top`; flips to the anchor's left side at the work-area edge |

Notification messages:

| Constant | Value |
|---|---|
| `CPM_NOTIFY_COMMAND` | `WM_USER + 110` |
| `CPM_NOTIFY_CLOSED` | `WM_USER + 111` |

Geometry defaults, all DPI-scaled at create except where noted:

| Constant | Value | Meaning |
|---|---:|---|
| `PSPOPUPMENU_DEFAULT_ITEMHEIGHT` | 24 | Command row height |
| `PSPOPUPMENU_DEFAULT_SEPHEIGHT` | 9 | Separator row height |
| `PSPOPUPMENU_DEFAULT_SEPTHICKNESS` | 1 | Separator rule weight, **never DPI-scaled** |
| `PSPOPUPMENU_DEFAULT_CHECKCOL` | 30 | Left gutter width for the check glyph |
| `PSPOPUPMENU_DEFAULT_CHEVRONCOL` | 20 | Right column width for the submenu chevron |
| `PSPOPUPMENU_DEFAULT_ACCELGAP` | 24 | Minimum gap between caption and accelerator text |
| `PSPOPUPMENU_DEFAULT_HPADDING` | 2 | Window padding left and right of the rows |
| `PSPOPUPMENU_DEFAULT_VPADDING` | 8 | Window padding above and below the rows |

Timing and internal timer ids:

| Constant | Value | Meaning |
|---|---:|---|
| `PSPOPUPMENU_HOVER_MS` | 250 | How long the cursor rests on a row before its submenu opens |
| `PSPOPUPMENU_POLL_MS` | 100 | Safety-net poll interval, run on the chain root only |
| `IDT_CPOPUPMENU_POLL` | `&hCB41` | That timer's id. Timer ids are per-window, so every instance shares it |

---

## Related controls

- **[PsMenuBar](README.md)** — the horizontal menu bar. It attaches `PsPopupMenu` windows as its
  dropdowns, registers itself as their notify window, and adds the bar-boundary keyboard moves
  (Left at a root level and Right on a row without a submenu step between bar items). If you use
  it, call `PsMenuBar_FilterMessage` for the bar's own menus — but remember it does not cover
  popups you show yourself.

`PsPopupMenu` and `PsBufferPaint` are all you need for context menus. The bar is strictly additive.
