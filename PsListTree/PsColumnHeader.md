# PsColumnHeader

An owner-drawn column header band for FreeBASIC Win32 applications: a horizontal strip of
column cells with draggable dividers between them, the last (or a nominated) column absorbing
whatever width the others leave over.

It is the header of a listview report view, separated out and made generic. The control owns
the column model — captions, widths, minimum widths, the fill designation — and the geometry
derived from it; you own every pixel, drawn through a per-column paint callback. It knows
nothing about lists: it is equally at home over a grid, a table you draw yourself, or anything
else that needs resizable column geometry.

What it gives you interactively: live divider drag with an ESC cancel that restores the
pre-drag width, a resize cursor over the gutters, divider double-click to auto-fit (you supply
the measurement), a click-on-the-caption notification for host-driven sorting, and per-column
tooltips.

The control never sorts, never takes focus, and never creates a child window.
[PsListTree](README.md) embeds one above its rows, but the control works perfectly well on its
own.

---

## Requirements

**Files to copy into your project:**

| File | Purpose |
|---|---|
| `PsColumnHeader.bi` | Declarations — types, callbacks, constants, function prototypes |
| `PsColumnHeader.inc` | Implementation |
| `PsBufferPaint.bi` | The flicker-free drawing surface the control paints through |
| `PsBufferPaint.inc` | Its implementation |
| `PsTipHost.bi` / `.inc` | The tooltip backend switch the control holds — not a control, no window of its own |
| `PsTooltip.bi` / `.inc` | The owner-drawn tooltip `PsTipHost` can drive instead of the system one |

The tooltip pair is required even if you never leave the default system tooltip:
`PsColumnHeader.bi` includes `PsTipHost.bi` and `PsColumnHeader.inc` includes `PsTipHost.inc`
(which pulls in `PsTooltip.inc`). They must be present on disk but you never name them in an
include line of your own.

**AfxNova is required.** The control is built on `CWindow`, and `PsBufferPaint` draws through
`AfxNova\CGdiPlus.inc`. Sources include AfxNova relative to the workspace root
(`#include once "AfxNova\CWindow.inc"`), so builds need the workspace root on the include
path:

```bash
fbc64.exe -i "C:\dev" main.bas
```

**Include order.** `PsColumnHeader.inc` pulls in its own `.bi`, which pulls in
`PsBufferPaint.bi`. The two implementation files go in this order:

```freebasic
#include once "PsBufferPaint.inc"
#include once "PsColumnHeader.inc"
```

with the AfxNova headers ahead of both:

```freebasic
#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"
using AfxNova
```

If you are using the header **inside a PsListTree**, it must come before `PsListTree.inc` —
`PsListTree.bi` names the `HDR_*` callback types without including this header itself. See the
include-order note in [README.md](README.md).

**GDI+ must be running before the first repaint and must outlive the last one.** All geometry
is rendered through GDI+, so bracket your message loop:

```freebasic
dim as ULONG_PTR gdipToken = AfxGdipInit()
' ... create windows, run the message loop ...
AfxGdipShutdown( gdipToken )
```

`AfxGdipShutdown` must come after every window is destroyed, because each repaint builds and
tears down a `PsBufferPaint`.

**Never name an identifier `ok`.** GDI+ defines `Ok = 0` as a `Status` enum value in namespace
`AfxNova`, and hosts customarily say `using AfxNova`. An existing variable, parameter or
function called `ok` becomes a duplicate definition the moment you adopt these files. Use
`bOK` instead.

**There is no message-pump obligation.** No filter to call, and nothing to add to your loop.
The control is not a tabstop and never takes focus — deliberately, since a header must not
steal the caret from whatever the host is editing — so it imposes no `IsDialogMessage`
requirement of its own either.

---

## Quick start

```freebasic
' Create it and place it. The control is created zero-sized and hidden.
dim as HWND hHdr = PsColumnHeader_Create( hWndParent, IDC_MYFORM_HEADER )
SetWindowPos( hHdr, 0, x, y, cx, headerHeight, SWP_NOZORDER or SWP_SHOWWINDOW )

' You MUST supply a painter -- without one only the flat background is drawn.
PsColumnHeader_SetPaintCallback( hHdr, @MyHeader_PaintColumn )

' Appearance. The font is borrowed -- you keep ownership and destroy it yourself.
PsColumnHeader_SetBackColor( hHdr, theme.BackColorScrollBar )
PsColumnHeader_SetFont( hHdr, ghFont(GUIFONTBOLD_10) )

' Columns. Widths are PIXELS. The last one is the fill column by default.
PsColumnHeader_AddColumn( hHdr, "Name", 160 )
PsColumnHeader_AddColumn( hHdr, "Size", 70 )
PsColumnHeader_AddColumn( hHdr, "Type" )

' Interaction hooks, all optional.
PsColumnHeader_SetWidthChangedCallback( hHdr, @MyHeader_WidthChanged )
PsColumnHeader_SetClickCallback( hHdr, @MyHeader_Click )
PsColumnHeader_SetAutoSizeCallback( hHdr, @MyHeader_AutoSize )
```

And the painter:

```freebasic
sub MyHeader_PaintColumn( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
    ' State priority for the cell: pressed > hot > normal
    dim as COLORREF backclr, foreclr
    if p->isPressed then
        backclr = theme.BackColorSelect : foreclr = theme.ForeColorSelect
    elseif p->isHot then
        backclr = theme.BackColorHot    : foreclr = theme.ForeColorHot
    else
        backclr = theme.BackColor       : foreclr = theme.ForeColor
    end if
    p->b->SetColors( foreclr, backclr )
    p->b->PaintRect( @p->rc )

    ' Caption, inset by the control's padding -- the space the layout accounts for.
    dim as RECT rcText = p->rc
    dim as integer pad = PsColumnHeader_GetPadding( p->hHeader )
    rcText.left += pad : rcText.right -= pad
    p->b->SetFont( ghFont(GUIFONTBOLD_10) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' Divider tick at the right edge, accented while its gutter is hovered or dragged.
    ' The fill column has no grabbable divider, so it gets none.
    if p->isFill = false then
        dim as RECT rcDiv = p->rc
        rcDiv.left = rcDiv.right - 1
        dim as COLORREF divclr = theme.ForeColorScrollBar
        if p->isResizeHot orelse p->isResizing then
            divclr = theme.FocusAccent
            rcDiv.left -= 1
        end if
        p->b->SetBackColor( divclr )
        p->b->PaintRect( @rcDiv )
    end if
end sub
```

Auto-fit needs a measurement only you can make, because you own the cell data:

```freebasic
function MyHeader_AutoSize( byval hHeader as HWND, byval idx as integer ) as integer
    ' Measure your widest cell in that column, add the padding on both sides.
    ' Return <= 0 for "no change".
    return bestW + PsColumnHeader_GetPadding( hHeader ) * 2
end function
```

**Inside a PsListTree**, do not create the header — the list already owns one. Reach it with
`PsListTree_GetHeader`, define columns with the `PsListTree_*Column*` wrappers, and register
callbacks with `PsListTree_SetHeader*` / `PsListTree_SetColumn*`. See [README.md](README.md).

---

## Concepts

### The handle is a real HWND

`PsColumnHeader_Create` returns an ordinary window handle, and every `PsColumnHeader_*` function
takes it. It is not an opaque type, so you can treat the control as the window it is —
`SetWindowPos` to place and size it, `ShowWindow` to show it, `GetDlgItem` to find it by the
`CtrlID` you passed at creation.

There are no child windows. Every message is handled by the control's own procedure.

### It is created zero-sized and hidden

`PsColumnHeader_Create` gives the control the styles `WS_CHILD`, `WS_CLIPSIBLINGS` and
`WS_CLIPCHILDREN`, and adds `CS_DBLCLKS` to its window class so divider double-clicks arrive.
`WS_VISIBLE` is deliberately absent, so a newly created control shows nothing until you size it
and show it.

The control has no intrinsic height. You decide how tall the band is when you place it, and
every column spans that full height.

### Widths are pixels

Column widths, minimum widths and the caption padding are all stored in **pixels** and used as
given. Drag deltas and auto-fit answers are pixels, and round-tripping those through unscaled
units would accumulate rounding drift, so the control does not.

The only DPI scaling that happens is on the control's own three defaults, and it happens where
they are used: the caption padding is scaled once at create, and the divider gutter and the
default minimum width are scaled each time they are consulted. If you set a padding of your own
afterwards, scale it yourself.

### Geometry is derived, never assigned

You set widths; the control computes rects. There is no way to write a rect.

```
  for each fixed column:  w = max( nWidth, effectiveMinWidth )
  fill column:            w = max( clientWidth - sum(fixed widths), its effectiveMinWidth )

  x starts at clientLeft - xOffset, and each column is
      rc = ( x, clientTop, x + w, clientBottom )
  laid left to right, contiguous, spanning the full client height.
```

`effectiveMinWidth` is the column's own `nMinWidth`, or — when that is 0 — the DPI-scaled
control default of 16 pixels. A column can never be laid out or dragged below it.

Layout is lazy. Every mutator marks it stale and asks for a repaint; the next paint, or any
query that needs a rect, recomputes it. Setting six widths in a row costs one layout pass, not
six. There is no begin-update / end-update pair to remember.

While the control has no client area yet — created zero-sized, before your first
`SetWindowPos` — layout does nothing and stays stale, so the first real resize recomputes it.

### Exactly one column can be the fill column

The fill column absorbs whatever width the fixed columns leave over. It **shrinks as well as
grows** with the window, but never below its own minimum. That is what stops a dead strip
appearing to the right of the last column when the window widens.

`PsColumnHeader_SetFillColumn` takes three kinds of value:

| Value | Meaning |
|---|---|
| `CCOLHDR_FILL_LAST` | Whichever column is currently last (**the default**) |
| `CCOLHDR_FILL_NONE` | No fill column; columns end where their widths end |
| a column index | That specific column, tracked across inserts and deletes |

`PsColumnHeader_GetFillColumn` returns the **resolved** index, or -1 for none.

A stored index looks after itself: inserting at or before it shifts it up, deleting before it
shifts it down, and deleting the fill column itself falls back to `CCOLHDR_FILL_LAST`.
`PsColumnHeader_Clear` resets the designation to `CCOLHDR_FILL_LAST` as well.

**The fill column's own right-hand divider is not grabbable.** Its width is derived, so there
is nothing meaningful to drag it to. Your painter is told which column that is
(`isFill`) so it can omit the divider tick.

### On overflow, the rects run honestly past the edge

When the columns are collectively wider than the client, the layout does not squeeze them: the
later rects simply extend past the right edge. Clipping is the paint pass's job, and since the
cursor cannot reach past the edge, hit-testing stays correct for free.

There is no horizontal scrolling. `PsColumnHeader_SetXOffset` shifts the entire column run left
by a pixel amount and every rect follows, which is the hook a horizontal scrollbar would drive;
nothing in the control moves it for you, and it is 0 unless you set it.

### Padding is a paint-time value

`PsColumnHeader_GetPadding` is a number your painter is expected to inset the caption by. The
control does not measure text and does not use padding in the layout — widths are stored, not
derived from captions. Read it in your paint callback (and in your auto-fit measurement) so
captions and any cells you draw underneath line up.

### The divider gutter

A boundary is grabbable within ± 3 pixels (DPI-scaled) of a column's right edge. Inside a
gutter, the gesture is a resize, not a column press — which is why
`PSCOLUMNHEADER_MESSAGEINFO.idx` is -1 there even though the cursor is over a column.

When two boundaries nearly coincide, because a column sits at its minimum width, the gutters
are tested right to left, so the later divider wins. That matches listview feel.

### Programmatic changes are silent

`PsColumnHeader_SetColumnWidth` stores a width, re-lays out and repaints, but fires **no**
`WidthChanged` callback. That notification is reserved for user interaction — a divider drag or
an applied auto-fit. It means you can call the setter from inside your own resize handler
without recursing.

### Interaction, and what capture buys

The control takes mouse capture on a left press, because both of its gestures consume the
guaranteed down → up pairing:

- **Divider drag.** The width is applied live: every mouse move that actually changes it stores
  the clamped value, re-lays out, repaints, and fires `WidthChanged` with `bLive = true`.
  Releasing fires one final notification with `bLive = false`. Pressing **ESC** mid-drag, or
  losing the capture to anything else, restores the pre-drag width and fires that final
  notification carrying the restored value — so a host that consumed the live updates converges
  back on its own.
- **Column body click.** The click fires only on a matched gesture: pressed on a column's body
  and released still on it. Press, slide off, release does nothing. Capture is what makes that
  reliable, by routing an outside release back here as `idx = -1`.

ESC is polled, not read from the keyboard queue, so a motionless cancel works: the same 100 ms
timer that acts as the hover safety net doubles as the ESC poll while a drag is live.

Hover tracking is **suspended for the whole drag**. With capture held the cursor legitimately
roams outside the client, and clearing the hot column there would strip the resize visuals.

### Double-clicks

`CS_DBLCLKS` is on. A double-click delivers down, up, **dblclk**, up — the dblclk substituting
for the second down — and the control runs the same press bookkeeping for both, or the trailing
up would release a capture never taken.

- **On a divider**, the dblclk asks your auto-size callback for a best-fit width and applies it,
  replacing any press or drag semantics for that press. The trailing up finds no press state
  and does nothing.
- **On a column body**, it falls through to an ordinary press. So a rapid double-click on a
  caption fires your click callback **twice** — one per matched press/release pair. If your
  click handler toggles a sort direction, that is exactly the behaviour you want; if it is
  destructive, guard it.

### The control never sorts

`ClickCallback` is the hook and nothing more. Reorder your own data, then repaint a sort glyph
from your paint callback and call `PsColumnHeader_Refresh`. The control has no notion of a sort
column.

### Tooltips

One tool covers the whole band — the columns are painted rects, not windows — and its text is
resolved on demand for whichever column is currently hot. With no tooltip callback, that
column's own caption is used; with one, whatever it returns, and `""` suppresses the tip. The
tip is popped whenever the hot column changes, so the next hover re-queries.

### Lifetime

The control frees itself and its tooltip when its window is destroyed, and releases any capture
it still holds. Fonts you pass in stay yours: the control stores the `HFONT` and never destroys
it.

---

## Behaviour and limits

Firm properties of the control, not settings:

- **Nothing is drawn without a paint callback** except the flat `BackColor` fill of the whole
  band.
- **The control has no intrinsic height**, and every column spans the full client height. There
  are no sub-rows and no column groups.
- **Widths are stored, never measured.** The control does not look at your captions, your font
  or your data, which is why auto-fit has to ask you.
- **Padding does not affect layout.** It is a number for your painter to honour; a caption
  wider than its cell is your problem to ellipsize.
- **A column can never go below its effective minimum** — its own `nMinWidth`, or the scaled
  16-pixel default when that is 0 — by drag, by layout, or by `SetColumnWidth`.
- **`GetColumnWidth` on the fill column reports its laid-out width**, not the stored one.
  Setting a width on the fill column stores it but has no visual effect until that column stops
  being the fill.
- **`SetColumnMinWidth` does not immediately raise a smaller stored width.** The clamp is
  applied when the column is laid out and when its width is read, so `GetColumnWidth` reflects
  it at once even though the stored value is unchanged.
- **The fill column's divider is not grabbable**, and its gutter is not hit-tested at all.
- **`idx` is -1 while the cursor is in a divider gutter**, even though a column is under it.
- **A body double-click fires the click callback twice.**
- **The message callback's result is ignored for `WM_LBUTTONUP`.** That message releases the
  mouse capture, and a callback able to swallow it could strand the capture and route every
  later click here. The result is also ignored for `WM_MOUSEMOVE` while a divider drag is
  live — the gesture is not suppressible in flight.
- **Suppressing `WM_LBUTTONDOWN` suppresses the press semantics, not the capture.** Capture is
  taken before your callback runs, precisely so its release stays guaranteed.
- **The right mouse button is reported, never acted on.** No capture is taken for it and there
  are no press semantics, so a right-up can arrive without a matching down. A context menu is
  your business.
- **No horizontal scrolling.** `SetXOffset` exists and works, but nothing drives it for you.
- **No sorting, no column reordering by drag, no built-in sort glyph, no hidden columns.** Draw
  and drive all of that yourself.
- **Inserting or deleting a column cancels any live press or drag** without firing callbacks,
  and clears the hover state — a gesture whose target just shifted is ambiguous, so it is
  abandoned rather than guessed at.
- **Inside a PsListTree, the `WidthChanged` slot is taken.** Use
  `PsListTree_SetColumnResizeCallback` instead; see [Related controls](#related-controls).

---

## API reference

Every function takes the handle from `PsColumnHeader_Create` as its first argument, written
`h` in the tables below.

### Creation

| Function | Description |
|---|---|
| `PsColumnHeader_Create( hWndParent, CtrlID ) as HWND` | Creates the control as a child of `hWndParent` and returns its window handle. `CtrlID` becomes the window's `GWLP_ID`, so `GetDlgItem` finds it. There are no child controls. Created zero-sized and hidden — place it with `SetWindowPos`. |

### Columns

`AddColumn` returns the new column's index; `InsertColumn` the index actually used. Both return
-1 on failure.

| Function | Description |
|---|---|
| `PsColumnHeader_AddColumn( h, Text, nWidth = 100, nMinWidth = 0, itemData = 0 ) as integer` | Appends a column. Widths are **pixels**. A `nWidth` of 0 or less leaves the column at its 100-pixel default; a `nMinWidth` of 0 or less leaves it at 0, meaning the scaled `CCOLHDR_DEFAULT_MINWIDTH` floor applies. |
| `PsColumnHeader_InsertColumn( h, idx, Text, nWidth = 100, nMinWidth = 0, itemData = 0 ) as integer` | Inserts at `idx`, shifting later columns up. `idx` is clamped to `[0, count]` and the index used is returned. A stored fill index shifts up with it; any live press or drag is cancelled and the hover state cleared. |
| `PsColumnHeader_DeleteColumn( h, idx ) as boolean` | Deletes the column. FALSE for an invalid index. A stored fill index shifts down, or falls back to `CCOLHDR_FILL_LAST` if it *was* the deleted column. Cancels any live gesture. |
| `PsColumnHeader_Clear( h )` | Removes every column and resets the fill designation to `CCOLHDR_FILL_LAST`. |
| `PsColumnHeader_GetCount( h ) as integer` | How many columns are defined. |
| `PsColumnHeader_Refresh( h )` | Marks the layout stale and repaints with a background erase. Rarely needed — every mutator does it for you. Useful after changing host state your painter reads, such as a sort column. |

### Column contents

| Function | Description |
|---|---|
| `PsColumnHeader_GetColumnText( h, idx ) as DWSTRING` | The column's caption. `""` for an invalid index. |
| `PsColumnHeader_SetColumnText( h, idx, Text ) as boolean` | Sets it and repaints **that cell only** — captions do not drive layout, since widths are stored. FALSE for an invalid index. |
| `PsColumnHeader_GetColumnItemData( h, idx ) as integer` | The column's user value. 0 for an invalid index. |
| `PsColumnHeader_SetColumnItemData( h, idx, itemData ) as boolean` | Sets it. FALSE for an invalid index. Does not repaint — the control never draws it. |

### Geometry and layout

| Function | Description |
|---|---|
| `PsColumnHeader_GetColumnWidth( h, idx ) as integer` | The column's width in pixels, already raised to its effective minimum. For the **fill** column this is the laid-out width, forcing a pending layout first. 0 for an invalid index. |
| `PsColumnHeader_SetColumnWidth( h, idx, nWidth ) as boolean` | Stores the width, clamped up to the column's effective minimum, then re-lays out and repaints. **Silent** — no `WidthChanged` callback. FALSE for an invalid index. Called on a column whose divider is being dragged at that moment, the new value becomes the drag's base: a later ESC restores to it, and the drag resumes from it re-anchored to the current cursor position. |
| `PsColumnHeader_GetColumnMinWidth( h, idx ) as integer` | The column's **own stored** minimum. 0 means "use the control default". |
| `PsColumnHeader_SetColumnMinWidth( h, idx, nMinWidth ) as boolean` | Sets it; negatives become 0. Re-lays out and repaints. Does not rewrite a smaller stored width — the clamp is applied at layout and read time. FALSE for an invalid index. |
| `PsColumnHeader_GetFillColumn( h ) as integer` | The **resolved** index of the fill column, or -1 for none. |
| `PsColumnHeader_SetFillColumn( h, idx ) as boolean` | Takes a column index, `CCOLHDR_FILL_LAST` or `CCOLHDR_FILL_NONE`. FALSE for an index that is neither valid nor one of those two constants. |
| `PsColumnHeader_GetColumnRect( h, idx, byref rc ) as boolean` | The column's rect in client coordinates. Forces any pending layout, so the result is always current — an embedding host reads its row-cell x-coordinates from here. Returns FALSE and empties `rc` for an invalid index. |
| `PsColumnHeader_HitTestDivider( h, x, y ) as integer` | Which column's right-edge divider gutter contains this client-coordinate point? -1 if none. Gutters are tested right to left, and the fill column's own divider is never reported. |
| `PsColumnHeader_GetPadding( h ) as integer` | The caption inset, in pixels. Read it from your paint callback. |
| `PsColumnHeader_SetPadding( h, nPadding )` | Sets it, in **raw pixels** — scale it yourself. Negatives become 0. Repaints, but never re-lays out: padding is paint-side only. |
| `PsColumnHeader_SetXOffset( h, xOffset )` | Shifts the whole column run left by this many pixels; every rect follows. Negatives become 0, and setting the value it already has does nothing. Defaults to 0 and nothing moves it for you. |

### Appearance

| Function | Description |
|---|---|
| `PsColumnHeader_GetBackColor( h ) as COLORREF` | The band's flat background colour. |
| `PsColumnHeader_SetBackColor( h, clr ) as COLORREF` | Sets it, repaints, and returns the previous value. |
| `PsColumnHeader_GetFont( h ) as HFONT` | The font stored for the band. |
| `PsColumnHeader_SetFont( h, hFont ) as boolean` | Sets it and repaints. **Borrowed, never owned** — keep it alive and destroy it yourself. It does not drive layout, and your painter is free to select a different font per column. |

### Tooltips

The control ships on the **system** tooltip (comctl32) and stays there unless you ask
otherwise. `PsColumnHeader_SetTooltipMode( h, PSTIP_MODE_PS )` moves that one instance onto
`PsTooltip` instead: owner-drawn and themeable, it word-wraps to a maximum width without a
hand-sent `TTM_SETMAXTIPWIDTH`, and it does not subclass the control it serves.

The default is deliberate rather than timid — `PsTooltip`'s colour defaults are dark and the
system tip is light, so a control that switched on its own would put a dark tip on a light form.

Switching backend does **not** change what a tip says. Text resolution is the same either way:
your `HDR_TooltipCallbackFunc` if one is set, otherwise the column's own caption, with `""`
suppressing the tip. Only the drawing and the driving differ.

| Function | Description |
|---|---|
| `PsColumnHeader_SetTooltipMode( h, nMode ) as boolean` | `PSTIP_MODE_SYSTEM` (default) or `PSTIP_MODE_PS`. Destroys the outgoing tip, builds the incoming one and re-applies the stored delays. A no-op when the mode is already the one asked for. Returns TRUE if the requested mode is live on return. |
| `PsColumnHeader_GetTooltipMode( h ) as long` | The current mode. `PSTIP_MODE_SYSTEM` for an invalid handle. |
| `PsColumnHeader_GetTooltipHandle( h ) as HWND` | The comctl32 tooltip window, for direct `TTM_*` messages — and **0 while the instance is on `PsTooltip`**, since a `TTM_*` sent to a `PsTooltip` window is silently ignored. |
| `PsColumnHeader_GetPsTooltipHandle( h ) as HWND` | The reverse: the `PsTooltip` window, and 0 on the system backend. This is the door to `PsTooltip_SetColors` / `SetFonts` / `SetStyle` / `SetMaxWidth` / `SetTitle` / `SetGlyph`, which are deliberately **not** mirrored on this control. |
| `PsColumnHeader_SetHoverTime( h, milliseconds )` | The initial dwell — how long the cursor must rest on a column before a tip appears. Default 250. **Double duty:** the same value is `TrackMouseEvent`'s `dwHoverTime`, so this also decides when the control considers a column hot. |
| `PsColumnHeader_SetAutoPopTime( h, milliseconds )` | How long a shown tip stays up before it hides itself. |
| `PsColumnHeader_SetReshowTime( h, milliseconds )` | The delay before a tip reappears when the cursor moves to another column while a tip is already up. |

All three delays are honoured by **both** backends, and are stored rather than pushed straight
at the live tooltip, so a delay set before a mode switch is still in force after it. A delay you
never set keeps the backend's own value, derived from the system double-click time. A delay set
to 0 is a real request for "no delay" and survives a switch as such.

To theme every tip in the process at once — the intended way to use `PSTIP_MODE_PS` — call
`PsTooltip_SetDefaultColors` / `SetDefaultFonts` / `SetDefaultStyle` / `SetDefaultMaxWidth` /
`SetDefaultDelays` **before any control is created**, and then just opt each control in. The
fonts are borrowed: you keep ownership and must outlive every tip that uses them. See the
*Tooltips* section of [README.md](README.md) for a worked example.

A header embedded in a `PsListTree` has its own independent tooltip and its own mode — setting
the list's does not set the header's.

### Callback registration

| Function | Description |
|---|---|
| `PsColumnHeader_SetPaintCallback( h, usersub )` | Installs the per-column painter and repaints. **Required** — without it only the background is drawn. |
| `PsColumnHeader_SetMessageCallback( h, userfunc )` | Installs an observer for the mouse messages. |
| `PsColumnHeader_SetTooltipCallback( h, userfunc )` | Installs the on-demand tooltip text supplier. Unset, the column's caption is used. |
| `PsColumnHeader_SetClickCallback( h, usersub )` | Installs the completed-click-on-a-column-body handler — the sorting hook. |
| `PsColumnHeader_SetWidthChangedCallback( h, usersub )` | Installs the user-resize handler. **Not for a header embedded in a PsListTree** — that slot belongs to the list; use `PsListTree_SetColumnResizeCallback`. |
| `PsColumnHeader_SetAutoSizeCallback( h, userfunc )` | Installs the divider-double-click best-fit measurer. |

Passing 0 to any callback setter clears it.

---

## Colors

**There is no colour struct.** The control paints one thing — the band's flat background — and
every column cell is yours, drawn from whatever palette your host already has.

| Setting | Paints |
|---|---|
| `PsColumnHeader_SetBackColor` | The whole client band, filled before any column callback runs. It therefore also covers any strip no column owns, which is what the far right of the band looks like when the fill designation is `CCOLHDR_FILL_NONE`. It is the only thing painted when no paint callback is set |

### Which colour wins

The control does not decide; it hands you the state flags and you resolve them. It **does**
guarantee what can be true at once, which is what makes a precedence order well defined:

```
isPressed   >   isHot   >   plain
```

with `isResizeHot` / `isResizing` layered on the divider itself, not on the cell body.

- **At most one column is `isHot`**, and at most one is `isPressed`. Both clear
  deterministically.
- `isPressed` implies a live left press on that column's **body**; it is never set for a
  divider drag.
- `isResizeHot` means the cursor is in that column's right-hand gutter — mutually exclusive
  with `isHot` on the same column, because a gutter hit takes the body hit away.
- `isResizing` means that column's divider is being dragged right now. While it is set, hover
  tracking is frozen, so `isHot` keeps whatever value it had when the drag started.
- `isFill` is a property of the column, not a mood. It is the flag to test when deciding
  whether to draw a divider tick.

### What draws what

| Part | Drawn by |
|---|---|
| Band background | The control, in `BackColor`, every repaint |
| Column cells, captions, dividers, sort glyphs | **Your paint callback**, into the same buffer |
| Resize cursor over a gutter | The control (`IDC_SIZEWE`) |

Only the columns intersecting the update rect are handed to your callback, so a hover change
repaints two cells rather than the whole band.

---

## Callbacks

All six typedefs carry the `HDR_` prefix.

### Paint

```freebasic
type HDR_PaintCallbackSub as sub( byval p as PSCOLUMNHEADER_PAINTINFO ptr )
```

Draws one column's header cell. Called for each column touched by the repaint, so keep it
cheap. Paint through `p->b` — the control's buffer for this repaint — using `p->rc` as the cell
rect, and never touch a screen DC. The control has already filled the band with `BackColor`.

Inset your caption by `PsColumnHeader_GetPadding( p->hHeader )`.

`PSCOLUMNHEADER_PAINTINFO`:

| Field | Meaning |
|---|---|
| `hHeader` | The control, so the callback can query it |
| `itemID` | The column index |
| `b` | The control's `PsBufferPaint` for this repaint (borrowed, not owned) |
| `rc` | This column's rect, in client coordinates |
| `isHot` | The mouse is over this column's **body** |
| `isPressed` | A live left press on this column's body |
| `isResizeHot` | The cursor is in this column's right-hand divider gutter |
| `isResizing` | This column's divider is being dragged right now |
| `isFill` | This is the effective fill column — draw no divider tick |
| `wszCaption` | The column's text |

> **A paint callback that fills a rectangle covering the whole band will erase everything under
> it.** Draw your cell, not a background — the control has already painted one.

### Message

```freebasic
type HDR_MessageCallbackFunc as function( byval m as PSCOLUMNHEADER_MESSAGEINFO ptr ) as boolean
```

Observes mouse messages. Return TRUE to suppress the control's own handling of that message,
FALSE to let it proceed.

`PSCOLUMNHEADER_MESSAGEINFO`:

| Field | Meaning |
|---|---|
| `hHeader` | The control |
| `uMsg` | The message |
| `wParam` / `lParam` | Its parameters, unmodified |
| `idx` | The column index under the mouse — **-1 if none, and -1 on a divider gutter** |

Which messages arrive, and what TRUE does:

| Message | `idx` | Effect of returning TRUE |
|---|---|---|
| `WM_MOUSEMOVE` | the hot column | Suppresses the control's handling. **Ignored while a divider drag is live** |
| `WM_MOUSEHOVER` | the hot column | Suppresses the control's handling |
| `WM_MOUSELEAVE` | always -1 | Suppresses the control's handling. The hot state has already been cleared by the time you are called |
| `WM_LBUTTONDOWN` | the pressed column | Suppresses the **press semantics** — no drag armed, no press state — but never the capture, which is taken first |
| `WM_LBUTTONDBLCLK` | the column under the cursor | As `WM_LBUTTONDOWN`, and it also suppresses the auto-fit |
| `WM_LBUTTONUP` | the released-over column | **Ignored.** See below |
| `WM_RBUTTONDOWN` | the column under the cursor | Suppresses the control's handling. No capture is taken and there are no press semantics either way |

**Your return value is ignored for `WM_LBUTTONUP`.** The control holds mouse capture across a
press, and the up-message is what releases it. A callback that suppressed it would strand the
capture and route every later click to this control.

It is ignored for `WM_MOUSEMOVE` **during a drag** for the same family of reason: the gesture
is already under way and cannot be halted mid-flight.

### Tooltip

```freebasic
type HDR_TooltipCallbackFunc as function( byval hHeader as HWND, byval idx as integer ) as DWSTRING
```

Supplies the tooltip text for a column, on demand — called only when a tip is about to show.
Return `""` for no tooltip. With no callback installed, the column's own caption is used.

### Click

```freebasic
type HDR_ClickCallbackSub as sub( byval hHeader as HWND, byval idx as integer )
```

A completed click on a column's **body** — pressed and released on the same column, and not on
a divider. This is the sorting hook: the control never sorts, so reorder your own data, repaint
a sort glyph from your paint callback, and call `PsColumnHeader_Refresh`.

Press, slide off, release fires nothing. A rapid double-click on a body fires it **twice**.

### Width changed

```freebasic
type HDR_WidthChangedCallbackSub as sub( byval hHeader as HWND, byval idx as integer, byval nWidth as integer, byval bLive as boolean )
```

A column's width changed through **user** interaction. `nWidth` is the new width in pixels,
already clamped to the column's minimum.

| When | `bLive` |
|---|---|
| Each mouse move during a live divider drag that actually changes the width | `true` |
| The drag commits on release | `false` |
| The drag is cancelled by ESC or by losing the capture — carrying the **restored** width | `false` |
| An auto-fit is applied | `false` |

So a drag always ends with exactly one `bLive = false` notification, whether it committed or
cancelled, and that final value is the truth. A host that consumed the live updates converges
back on a cancel for free.

`PsColumnHeader_SetColumnWidth` does **not** fire this — programmatic setters are silent, which
is what makes it safe to call from inside this handler.

**Embedded in a PsListTree, subscribe with `PsListTree_SetColumnResizeCallback` instead.**

### Auto size

```freebasic
type HDR_AutoSizeCallbackFunc as function( byval hHeader as HWND, byval idx as integer ) as integer
```

A divider was double-clicked. Return the best-fit width for that column, in pixels; return 0 or
less for "no change". The control cannot do this itself — you own the cell data and the fonts,
so only you can measure. Add the padding on both sides if you want the caption to breathe.

The returned width is clamped up to the column's effective minimum before being applied, and
applying it fires `WidthChanged` with `bLive = false`.

With no auto-size callback installed, a divider double-click does nothing.

---

## Constants

Defined in `PsColumnHeader.bi`:

| Constant | Value | Meaning |
|---|---:|---|
| `CCOLHDR_DEFAULT_PADDING` | 8 | Caption inset in pixels. DPI-scaled once at create; `SetPadding` takes raw pixels thereafter |
| `CCOLHDR_DIVIDER_GUTTER` | 3 | Half-width of the divider hit zone: a boundary is grabbable within ± this many pixels of a column's right edge. DPI-scaled at use |
| `CCOLHDR_DEFAULT_MINWIDTH` | 16 | The floor applied to any column whose own `nMinWidth` is 0. DPI-scaled at use |
| `CCOLHDR_FILL_LAST` | -2 | Fill designation: whichever column is currently last (the default) |
| `CCOLHDR_FILL_NONE` | -1 | Fill designation: no column absorbs leftover width |
| `IDT_CCOLHDR_HOTTRACK` | `&hCB30` | Timer id for the hover safety-net poll, which doubles as the ESC poll during a drag. Timer ids are per-window, so every instance shares it |
| `CCOLHDR_HOTTRACK_MS` | 100 | How often that poll runs |

The tooltip backend values passed to `PsColumnHeader_SetTooltipMode` come from `PsTipHost.bi`:

| Constant | Value | Meaning |
|---|---:|---|
| `PSTIP_MODE_SYSTEM` | 0 | The comctl32 tooltip. **The default** |
| `PSTIP_MODE_PS` | 1 | `PsTooltip` — owner-drawn, themeable, word-wrapping |

Defaults a new column starts with:

| Setting | Default |
|---|---:|
| Width | 100 pixels |
| Minimum width | 0, meaning the scaled `CCOLHDR_DEFAULT_MINWIDTH` floor |
| `itemData` | 0 |

And the control itself:

| Setting | Default |
|---|---:|
| Fill column | `CCOLHDR_FILL_LAST` |
| Hover time | 250 ms |
| X offset | 0 |

---

## Related controls

`PsColumnHeader` creates nothing and embeds nothing. It is, however, embedded by
**[PsListTree](README.md)**, which places one above its rows and uses it as the single store for
its column model — every `PsListTree_*Column*` function delegates here.

If your header is a PsListTree's, three rules apply:

| Do | Instead of |
|---|---|
| Define columns with `PsListTree_AddColumn` and friends | `PsColumnHeader_AddColumn` on the child |
| Subscribe to resizes with `PsListTree_SetColumnResizeCallback` | `PsColumnHeader_SetWidthChangedCallback` — that slot is the list's, and it needs it to repaint rows on every live drag |
| Size the band with `PsListTree_SetHeaderHeight` and show it with `PsListTree_ShowHeader` | `SetWindowPos` / `ShowWindow` on the child, which the list re-lays out anyway |

Everything else — padding, back colour, font, the paint, click, auto-size and tooltip
callbacks, `GetColumnRect`, `HitTestDivider` — is safe to call directly on the handle from
`PsListTree_GetHeader`, and several of those have `PsListTree_*` pass-through wrappers for
convenience.
