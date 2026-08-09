# PsBufferPaint

A flicker-free drawing surface for FreeBASIC Win32 applications: you paint into an offscreen
bitmap and it is copied to the screen in one blit, so nothing the user sees is ever half-drawn.

It is not a control. It is a `TYPE` you create as a local variable inside a `WM_PAINT` handler,
draw through, and let go of. Owner-drawn controls paint through one of these, and so does any
window that draws its own client area.

Beyond the double buffering it gives you a small set of drawing primitives — fills, borders,
rounded rectangles, outlines, ellipses, rules, text — that take a `RECT ptr` and the colours you
set on the object. **Geometry is rendered with GDI+ and text with GDI**, which is why a rounded
corner comes out smooth and a one-pixel divider stays hard-edged. The split is permanent, and the
object pays its cost internally: you never have to think about which API you are on.

---

## Requirements

**Files to copy into your project:**

| File | Purpose |
|---|---|
| `PsBufferPaint.bi` | Declarations — the `PsBufferPaint` type and the free functions |
| `PsBufferPaint.inc` | Implementation |

**AfxNova is required.** `CBufferPaint.bi` includes `AfxNova\CGdiPlus.inc` itself, so you do not
have to. Sources include AfxNova relative to the workspace root
(`#include once "AfxNova\CWindow.inc"`), so builds need the workspace root on the include path:

```bash
fbc64.exe -i "C:\dev" main.bas
```

`PaintLine` also needs an AfxNova `CWindow` behind the window handle you passed to
`BeginDoubleBuffer`, because that is where it gets the DPI scale from. Every other method works
on any `HWND`.

**Include order.** `PsBufferPaint.bi` pulls in its own GDI+ dependency, so the only ordering rule
is that `PsBufferPaint.inc` comes before anything that draws through it:

```freebasic
#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova

#include once "PsBufferPaint.inc"
#include once "frmMain.inc"        ' your own forms, which paint through it
```

**GDI+ must be running before the first repaint and must outlive the last one.** All geometry is
rendered through GDI+, so bracket your message loop:

```freebasic
dim as ULONG_PTR gdipToken = AfxGdipInit()
' ... create windows, run the message loop ...
AfxGdipShutdown( gdipToken )
```

`AfxGdipShutdown` must come **after** every window is destroyed. Each repaint builds a
`PsBufferPaint` and tears it down again, and the teardown deletes GDI+ objects — shutting GDI+ down
while a window can still paint means those deletions run against a dead library. Skip
`AfxGdipInit` altogether and nothing geometric draws at all, while text still appears, which makes
the symptom look like a colour bug rather than a missing initialisation.

**Never name an identifier `ok`.** This is the one thing adopting this file can break in code that
previously compiled, and the error will not point at the cause. GDI+ defines a `Status` enum whose
first member is `Ok = 0`, and that enum lives in namespace `AfxNova`. Hosts customarily say
`using AfxNova`, which pulls the name into scope — so an existing variable, parameter or function
of yours called `ok` becomes a duplicate definition the moment `PsBufferPaint.bi` is included. It
cannot be fixed from inside this file, because it is the host's own `using AfxNova` that exposes
the name. Rename yours; `bOK` is the convention here.

---

## Quick start

A complete `WM_PAINT` handler. The buffer is a stack local: constructed on entry, destroyed on
exit, with `BeginDoubleBuffer` / `EndDoubleBuffer` bracketing everything you draw.

```freebasic
case WM_PAINT
    dim b as PsBufferPaint
    b.BeginDoubleBuffer( hwnd )

    ' Clear the whole surface first.
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    ' A filled rounded rect. nCurvature is the corner ellipse DIAMETER.
    dim as RECT r = type( 24, 20, 174, 64 )
    b.SetBackColor( theme.BackColorSelect )
    b.PaintRoundRect( @r, 20 )

    ' Text on top of it, using a font the host owns.
    b.SetFont( ghFont(GUIFONT_9) )
    b.SetForeColor( theme.ForeColorSelect )
    b.PaintText( "Hello", @r, DT_CENTER )

    ' A one-pixel divider. PaintLine uses the PEN colour, not the back colour.
    b.SetPenColor( theme.Divider )
    b.PaintLine( 1, 24, 80, 224, 80 )

    b.EndDoubleBuffer()
    return 0
```

Pair it with this, or the system paints the background first and you see the flicker the buffer
exists to remove:

```freebasic
case WM_ERASEBKGND
    return true
```

That is the whole minimum. Everything below is refinement.

---

## Concepts

### The object is a stack local, and its lifetime is one repaint

You do not keep a `PsBufferPaint` around between paints. Declare it inside the handler and let it
go out of scope at the end. The GDI+ `Graphics`, pen and brush it builds live only as long as the
object does, so a long-lived instance would hold GDI+ objects open indefinitely.

The destructor is a safety net, not the normal exit: if you return between `BeginDoubleBuffer` and
`EndDoubleBuffer`, it releases the GDI+ objects, deletes the bitmap and DC it owns, and calls
`EndPaint` if the buffer was the one that called `BeginPaint`. Nothing leaks — but nothing is
blitted either, so an early return means a repaint that drew nothing.

### The Begin / End bracket

`BeginDoubleBuffer` records the target, works out the buffer rectangle and creates the offscreen
surface. Everything you draw goes there. `EndDoubleBuffer` tears the GDI+ objects down (which
flushes anything still batched), blits the surface to the target in one `BitBlt`, restores the DC
and frees what it owns.

There are three ways to begin:

| Call | When |
|---|---|
| `BeginDoubleBuffer( hwnd )` | Inside `WM_PAINT`. Calls `BeginPaint` for you, takes the client rect as the buffer rect, and `EndDoubleBuffer` calls `EndPaint`. This is the one you want almost always. |
| `BeginDoubleBuffer( hwnd, hdc, rcItem )` | You already have a DC and a rectangle — a `WM_DRAWITEM`, or a DC of your own. No `BeginPaint`/`EndPaint` is involved. The buffer creates and owns its own bitmap. |
| `BeginDoubleBuffer( hwnd, hdc, rcItem, cachedMemDC )` | As above, but drawing into a memory DC **you** created, with a bitmap already selected into it and big enough for `rcItem`. `EndDoubleBuffer` blits from it but does **not** delete it. This exists so that painting a long list does not create and destroy a DC and bitmap once per row. |

The surface the first two overloads create is a 32-bit top-down DIB section rather than a
compatible bitmap, because GDI+ wants a known pixel format for antialiasing and alpha. The cached
overload skips that step entirely and draws onto whatever surface you handed it — so a cached DC
built from a compatible bitmap gets the display's format, not a guaranteed 32bpp one.

`EndDoubleBuffer` copies from the buffer's own origin to `rcItem`'s top-left, and applies no
coordinate translation. Pass a rectangle whose origin is `(0,0)` — which is what
`BeginDoubleBuffer( hwnd )` does for you — and draw in ordinary client coordinates.

### Geometry is GDI+, text is GDI

Every shape — fills, borders, rounded rectangles, outlines, ellipses, rules — goes through GDI+.
Every string goes through GDI's `DrawText`. This is deliberate and permanent, not a migration in
progress: GDI+ measures and lays text out differently, so moving text across would mean converting
every `GetTextExtentPoint32W` / `GetTextMetricsW` measurement in a host's layout code in lockstep,
and would shift icon-font glyphs.

**What that costs you: nothing, as long as you draw through the object.** GDI+ batches its drawing
and GDI does not, so mixing them on one HDC without a flush loses shapes intermittently. Every
text method flushes first, internally. The flush itself (`EnsureGdiReady`) is **private** — you
cannot call it and you do not need to.

The one place this reaches you is `getMemDC()`, the escape hatch to the raw surface. Asking for the
HDC means you are about to do something with GDI — read pixels back, blit, draw directly — so
`getMemDC()` flushes before it hands the DC over. **Call it each time you need the DC rather than
caching the handle**: a handle you obtained before a run of `Paint*` calls points at a surface
those calls may not have reached yet, and you would read a half-painted picture. The flush is
skipped when nothing is pending, so asking again is cheap.

### Colours are state on the object, not arguments

No drawing method except `PaintChar` takes a colour. You set colours on the buffer and they stay
set until you change them:

| Colour | Consumed by |
|---|---|
| Back colour | Every **fill**: `PaintRect`, `PaintClientRect`, `PaintRoundRect`, the interiors of `PaintBorderRect` / `PaintRoundBorderRect`, `PaintEllipse`, `PaintPolygon`, `PaintIconButton` |
| Pen colour | Every **stroke**: the borders of `PaintBorderRect` / `PaintRoundBorderRect`, `PaintRoundOutline`, the rim of `PaintEllipse`, the outline of `PaintPolygon` — and **`PaintLine`, whose rules are strokes even though they are drawn as filled rectangles** |
| Fore colour | Text: `PaintText`, `PaintTextEx`, and the caption inside `PaintIconButton` |

`PaintGradientRect` is the one exception: its two stops are **arguments**, so it consumes neither
the back colour nor the pen colour.

The consequence worth internalising: hover and selection styling is entirely yours. The buffer
always paints with whatever colours are set right now, so decide the mood first — with
`isMouseOverRECT`, or your own tracked hover state — and set the colours **before** you paint.

### Coordinates and units

Everything is in **pixels**, in the coordinate space of the buffer rectangle — client coordinates
for the `BeginDoubleBuffer( hwnd )` case.

Rectangles are passed as `RECT ptr` and are never modified. They follow GDI's conventions, and
those conventions are not uniform across the primitives, because each one matches what its GDI
equivalent did:

| Primitive | Extent |
|---|---|
| `PaintRect` / `PaintClientRect` | `right` and `bottom` **exclusive**. A rect of `(10,5,40,20)` covers x 10..39, y 5..19. |
| Rounded rects, outlines, borders, ellipses | Stop **one pixel short** of `right` and `bottom`, exactly as GDI's `RoundRect` and `Ellipse` do. A rect of `(10,5,30,25)` gives an ellipse spanning x 10..28. |
| `PaintLine`, axis-aligned | Endpoint **exclusive**, like GDI's `LineTo`. A rule from x=10 to x=40 paints 10..39, at any thickness. |
| `PaintLine`, diagonal | Endpoint **inclusive**, which is GDI+'s convention. |

**Nothing is DPI-scaled except `PaintLine`'s thickness.** Every rectangle, every curvature, every
pen width you pass is used as given. `PaintLine`'s `nWidth` is the single exception: it is scaled
through the window's `CWindow::ScaleY` before use, and floored at 1.

### Curvature is a diameter, and a pen style is a pen style

`PaintRectFactory` — the workhorse behind the bordered and rounded rectangle methods — keeps GDI's
vocabulary for two of its arguments, deliberately, because that is the vocabulary its callers
already speak. Getting either wrong produces a picture that still renders and still looks
deliberate, which is why they are worth reading twice:

- **`nCurvature` is the corner ellipse's DIAMETER**, exactly as `RoundRect` took it — not a radius.
  It is halved internally. Pass a radius and every corner comes out half as round as you intended;
  pass a diameter to something that wanted a radius and it comes out twice as round. It is clamped
  to half the shape's width and height, so an over-large curvature degrades to a stadium rather
  than to a broken path. `nCurvature = 0` means square corners, and also switches antialiasing off
  — a square-cornered fill wants to land on exact pixel boundaries.
- **`iStyle` is a GDI pen style.** `PS_SOLID` strokes a border; `PS_NULL` means fill only. An
  `nPenWidth` of 0 or less means the same thing by a different route, and both fold into one
  "no stroke" decision.

A pill is a rounded rect whose curvature equals its own height: both ends then become exact
semicircles.

### Where a stroke lands

Both GDI and GDI+ centre a pen on the path it follows, so half the pen hangs outside the shape.
The buffer insets the stroke path by that half and nudges it to the pixel centre, so a border you
ask for on a rectangle sits **on** that rectangle's edge rather than half a pixel outside it, at
any pen width. You do not need to deflate rectangles yourself before stroking them.

Curves are antialiased; square corners and axis-aligned rules are not. That last part is
deliberate and load-bearing: a one-pixel horizontal rule drawn as an antialiased line straddles
two pixel rows and comes out grey and soft, which reads as a theme bug. Axis-aligned rules are
therefore painted as filled rectangles, where the extent is stated rather than inferred.

### The font is borrowed, never owned

`SetFont` stores an `HFONT` you created. The buffer selects it while painting text and restores
the previous one afterwards; it never creates or destroys a font. Destroying yours is your job,
and it must outlive the paint.

### Return values carry no information

Every method returns `as long`, and every one of them returns 0 — including the paths that give up
early, such as an empty rectangle or a `PaintLine` on a window with no `CWindow` behind it. Do not
test the result to decide whether something drew.

---

## Behaviour and limits

Firm properties, not settings:

- **A fill covering the whole control erases everything already drawn under it.** This is the
  single most common mistake when writing a control's paint callback. `PaintBorderRect` and
  `PaintRoundBorderRect` **fill as well as stroke** — used as a frame around content you have
  already painted, they wipe it out, and the result still renders, so it reads as a colour bug
  rather than a missing shape. Use `PaintRoundOutline` when you want a stroke over existing pixels.
  The same applies to `PaintClientRect`: it is a clear, so it belongs at the top of a paint
  routine, not in the middle of one.
- **`PaintText` cannot draw wrapped text.** It forces `DT_VCENTER OR DT_SINGLELINE` on top of
  whatever flags you pass, and `DT_SINGLELINE` and `DT_WORDBREAK` cannot both hold. Use
  `PaintTextEx` for anything multi-line — it adds only `DT_NOPREFIX`, leaving `DT_WORDBREAK`,
  `DT_TOP` and `DT_CALCRECT` reachable.
- **Ampersands are never mnemonics in `PaintText` / `PaintTextEx`.** Both force `DT_NOPREFIX`, so
  a caption can never eat an `&`. `PaintChar` does **not** force it.
- **`PaintLine` silently does nothing when the window has no `CWindow` behind it.** It asks
  `AfxCWindowPtr` for the DPI scale and returns immediately if there is none. Nothing is drawn and
  nothing is reported.
- **`PaintLine` is the only method that DPI-scales anything**, and it scales through `ScaleY` for
  both axes.
- **Fore-colour alpha has no effect.** `SetForeColorA` stores the value, but the fore colour is
  only ever used for GDI text, and GDI text has no alpha. Alpha is honoured for the back and pen
  colours, which feed GDI+.
- **`SetFont( 0 )` does not clear the font.** A null handle is ignored, so once a font is set it
  can be replaced but not removed.
- **`PaintChar` takes its colour as an argument** and ignores the buffer's fore colour. It also
  fixes its own layout flags — `DT_SINGLELINE`, `DT_VCENTER`, `DT_LEFT`, `DT_NOCLIP` — so you
  cannot centre with it, and `DT_NOCLIP` means a glyph too big for its rect draws outside it
  rather than being cut off.
- **`PaintRoundOutline` with a pen width of 0 or less draws nothing at all**, rather than falling
  back to a fill.
- **`PaintEllipse` and `PaintPolygon` are always antialiased**, with no way to turn that off. The
  square-cornered methods are the opposite — they switch smoothing off, because antialiasing an
  axis-aligned edge only makes it grey and soft.
- **`PaintPolygon` does not carry the `-1` its neighbours do.** `PaintRoundRect`, `PaintEllipse`
  and `PaintGradientRect` each stop one pixel short of `right`/`bottom` because each mirrors a GDI
  original that measurably does. A polygon has no rect and no GDI original, so its vertices are
  used verbatim — a 4-vertex box covers exactly the pixels `PaintRect` covers on the same corners.
- **`PaintGradientRect` never strokes**, whatever the pen colour is. Frame it afterwards with
  `PaintRoundOutline`; `PaintBorderRect` would fill straight back over it.
- **The cached `BeginDoubleBuffer` overload does not create a bitmap.** You supply the DC with a
  bitmap already selected and large enough for the rectangle; the buffer saves and restores the
  DC's state around the paint but never deletes either.
- **`EndDoubleBuffer` applies no coordinate translation.** The blit runs from the buffer's own
  origin to `rcItem`'s top-left.
- **Empty and collapsed rectangles are handled, not rejected.** A collapsed window's client rect is
  clamped to 1×1 so the bitmap can still be created, and the drawing methods return early on a
  negative or empty extent — silently, since the return value is always 0.
- **No clipping region, no transforms, no gradients, no images.** The primitives below are the
  whole surface. Anything else goes through `getMemDC()` — which flushes for you.

---

## API reference

Every method returns `as long`, and the value is always 0. It is not a status code.

### Lifecycle

| Method | Description |
|---|---|
| `BeginDoubleBuffer( hwnd ) as long` | Starts a buffered paint inside `WM_PAINT`. Calls `BeginPaint`, takes the window's client rect as the buffer rect, and creates the offscreen surface. `EndDoubleBuffer` will call `EndPaint`. |
| `BeginDoubleBuffer( hwnd, hdc, rcItem ) as long` | Starts a buffered paint against a DC you already have, over `rcItem`. No `BeginPaint`/`EndPaint`. The buffer creates and owns its surface. |
| `BeginDoubleBuffer( hwnd, hdc, rcItem, cachedMemDC ) as long` | As above, but draws into **your** memory DC (bitmap already selected, at least `rcItem`-sized). The DC's state is saved and restored, and `EndDoubleBuffer` blits from it but does not delete it. For per-row painting, where creating a DC and bitmap each time would be pure churn. |
| `EndDoubleBuffer() as long` | Releases the GDI+ objects — which flushes any batched drawing — blits the surface to the target, restores the DC, frees whatever the buffer owns, and calls `EndPaint` if it called `BeginPaint`. |
| `SetupBitmap() as long` | Creates the memory DC and the 32bpp top-down DIB section. **Called for you** by the first two `BeginDoubleBuffer` overloads; it reads state only they set, so there is no reason to call it yourself. |
| *(destructor)* | Safety net for an early return between Begin and End: releases GDI+ objects, deletes an owned bitmap and DC, and calls `EndPaint` if one is outstanding. Nothing leaks, but nothing is blitted either. |

### Colours and state

None of these draw anything; they set state the drawing methods consume. Each stays in effect
until changed.

| Method | Description |
|---|---|
| `SetBackColor( backcolor ) as long` | The **fill** colour. Also resets the back alpha to 255 (opaque). |
| `SetForeColor( forecolor ) as long` | The **text** colour. Also resets the fore alpha to 255. |
| `SetPenColor( pencolor ) as long` | The **stroke** colour — borders, outlines, ellipse rims and `PaintLine` rules. Also resets the pen alpha to 255. |
| `SetColors( forecolor, backcolor ) as long` | Both of the first two at once, and resets both alphas to 255. |
| `SetBackColorA( backcolor, nAlpha ) as long` | Back colour with an alpha of 0–255. Fills blend against whatever is already on the surface. |
| `SetPenColorA( pencolor, nAlpha ) as long` | Pen colour with an alpha of 0–255. |
| `SetForeColorA( forecolor, nAlpha ) as long` | Fore colour with an alpha. **The alpha is stored but never used** — the fore colour only ever reaches GDI text, which has no alpha channel. |
| `SetFont( hFont ) as long` | Stores a font handle for the text methods to select. The buffer never owns it; you create and destroy it, and it must outlive the paint. **A null handle is ignored**, so a font cannot be cleared once set. |

### Geometry drawing

All of these render through GDI+. All take a `RECT ptr` in buffer coordinates and leave it
unmodified.

| Method | Description |
|---|---|
| `PaintClientRect() as long` | Fills the **whole buffer rectangle** with the back colour. The clear at the top of a paint routine. |
| `PaintRect( rc ) as long` | Fills `rc` with the back colour. No stroke, no antialiasing, square corners. `right` and `bottom` are **exclusive**. |
| `PaintBorderRect( rc, nPenWidth = 1 ) as long` | A square-cornered rectangle **filled** with the back colour **and** stroked with the pen colour. Not an outline — see the caution below. |
| `PaintRoundRect( rc, nCurvature = 20 ) as long` | A rounded rectangle filled with the back colour, **no stroke**. `nCurvature` is the corner ellipse **diameter** in pixels. |
| `PaintRoundBorderRect( rc, nCurvature = 20, nPenWidth = 1 ) as long` | A rounded rectangle filled with the back colour **and** stroked with the pen colour. `nCurvature` is a **diameter**. |
| `PaintRoundOutline( rc, nCurvature = 20, nPenWidth = 1 ) as long` | Strokes a rounded rectangle in the pen colour and **leaves the interior untouched**. This is the one to use over already-painted pixels — a focus ring, a frame around content. Draws nothing when `nPenWidth <= 0`. |
| `PaintEllipse( rc, nPenWidth = 0 ) as long` | An ellipse inscribed in `rc`, filled with the back colour. With `nPenWidth > 0` the rim is also stroked with the pen colour. Always antialiased. Stops one pixel short of `right`/`bottom`, matching GDI. |
| `PaintGradientRect( rc, clr1, clr2, nMode = LinearGradientModeVertical, nCurvature = 0, rcBrush = 0 ) as long` | Fills `rc` — square or rounded — with a two-stop linear gradient. **Fill only, never strokes**; put a frame over it with `PaintRoundOutline`. The two colours are arguments, not object state, so this is the one geometry method that ignores the back colour. `nMode` is horizontal(0) / vertical(1) / forward-diagonal(2) / backward-diagonal(3). `nCurvature` is a **diameter**, and the extent follows `PaintRectFactory`'s `-1`, not `PaintRect`'s full extent. `rcBrush` measures the ramp across a **different** rect than the one being filled, so one block of a segmented bar can be filled with its slice of a gradient spanning the whole run; `0` means measure across `rc`. |
| `PaintPolygon( pts, nCount, nPenWidth = 0 ) as long` | Fills the polygon through `nCount` vertices with the back colour; with `nPenWidth > 0` the outline is also stroked with the pen colour. `pts` is a `POINT ptr` to at least `nCount` vertices — the figure is closed for you. **The vertices are used exactly as given**, with no `-1`: every other closed shape here mirrors a GDI original whose fill stops a row short of its own rect, but a polygon has no rect and no GDI original. Always antialiased — a polygon exists for its diagonals. The stroke is **centred** on the path rather than inset inside the fill the way `PaintRoundOutline`'s is, but takes the same half-pixel offset and the same DPI pen scaling, so a 1px polygon edge lands on the same pixels as a 1px `PaintRoundOutline` frame it joins. Fewer than 3 vertices, or a null `pts`, paints nothing. |
| `PaintLine( nWidth, nLeft, nTop, nRight, nBottom ) as long` | A rule in the **pen** colour. `nWidth` is a thickness in unscaled units and is the **only** DPI-scaled value in the class (via `ScaleY`, floored at 1). Axis-aligned rules are painted as filled rectangles — hard-edged, pen centred the way GDI centres it, endpoint **exclusive** at any thickness. A diagonal is drawn as an antialiased line with the endpoint **included**. Requires a `CWindow` behind the window handle; does nothing without one. |
| `PaintRectFactory( rc, iStyle, nPenWidth = 1, nCurvature = 0 ) as long` | The workhorse the four rectangle methods above are thin wrappers over. `iStyle` is a **GDI pen style** — `PS_SOLID` to stroke, `PS_NULL` to fill only. `nCurvature` is the corner ellipse **diameter** (0 = square corners, which also disables antialiasing); it is halved to a radius internally and clamped to half the shape. `nPenWidth <= 0` means no stroke, the same as `PS_NULL`. It always fills with the back colour. Call it directly only when you need a combination the wrappers do not offer. |
| `PaintIconButton( wszText, rc, nCurvature = 20 ) as long` | Convenience: a `PaintRoundRect` in the back colour with `PaintText( …, DT_CENTER )` over it in the fore colour, using the current font. Centred both ways, since `PaintText` forces `DT_VCENTER`. |

> **Caution.** `PaintClientRect`, `PaintRect`, `PaintRoundRect`, `PaintBorderRect` and
> `PaintRoundBorderRect` all **fill**. Any of them over a rectangle that covers your whole control
> will erase everything already drawn under it. When you want a frame over existing pixels, the
> method is `PaintRoundOutline`.

### Text drawing

All of these render with GDI `DrawText`, flush any pending GDI+ drawing first, paint with a
transparent background, and select and restore the current font.

| Method | Description |
|---|---|
| `PaintText( wszText, rc, wsStyle ) as long` | Draws in the fore colour. Adds `DT_NOPREFIX OR DT_VCENTER OR DT_SINGLELINE` to your flags, so it is always one vertically centred line and **structurally cannot wrap**. Use it for captions, labels and single-line cells — it cannot get those two flags wrong. |
| `PaintTextEx( wszText, rc, wsStyle ) as long` | Same, but the layout flags are **yours**: only `DT_NOPREFIX` is added. This is the way to `DT_WORDBREAK`, `DT_TOP` and `DT_CALCRECT`, and therefore the method for wrapped or multi-line text. Measure with the same flags you draw with — sizing a box with `DT_CALCRECT OR DT_WORDBREAK` and then drawing without `DT_WORDBREAK` gives one clipped line inside a tall box. |
| `PaintChar( wszChar, rc, forecolor ) as long` | Draws a single glyph — an icon-font character — in the colour you pass, **ignoring** the buffer's fore colour. The flags are fixed at `DT_SINGLELINE OR DT_VCENTER OR DT_LEFT OR DT_NOCLIP`: left-aligned, vertically centred, and allowed to overflow `rc` rather than be clipped. Unlike the two above it does not force `DT_NOPREFIX`. |

### Queries

| Method | Description |
|---|---|
| `rcClient() as RECT` | The buffer rectangle — the client rect for `BeginDoubleBuffer( hwnd )`, or the `rcItem` you passed otherwise. |
| `rcClientWidth() as long` | Its width. |
| `rcClientHeight() as long` | Its height. |
| `getMemDC() as HDC` | The offscreen DC, for raw GDI work — reading pixels back, blitting, drawing directly. **Flushes any pending GDI+ drawing first**, so what you read is what has been drawn. Ask for it again each time rather than caching the handle across further `Paint*` calls. |

### Free functions

Not members; they need no buffer.

| Function | Description |
|---|---|
| `isMouseOverRECT( hWin, rc ) as boolean` | TRUE when the cursor is inside `rc`, which is given in `hWin`'s **client** coordinates. The rect is taken by value, so yours is not modified. Purely positional: it does not consider which window is actually on top, so a rect covered by another window still tests TRUE. |
| `isMouseOverWindow( hChild ) as boolean` | TRUE when `hChild` is exactly the window under the cursor. Unlike the above this *is* a hit test against the real window stack, so a child window sitting on top makes it FALSE. |
| `PaintRect( hDC, rc, clr ) as long` | Fills `rc` with `clr` on **any** DC, via `ExtTextOut`/`ETO_OPAQUE`. Nothing to do with a buffer's state — use it when you have a bare DC and no `PsBufferPaint`. Distinct from the same-named method, which takes only a rect and uses the buffer's back colour. |

---

## Colors

There is no colour struct. The object carries three colours and three alphas as state, and every
drawing method reads whichever ones apply to it. Set them before you paint; they persist until
changed.

| State | Set with | Read by |
|---|---|---|
| Back colour | `SetBackColor`, `SetColors`, `SetBackColorA` | Every fill: `PaintClientRect`, `PaintRect`, `PaintRoundRect`, `PaintEllipse`, `PaintPolygon`, the interiors of `PaintBorderRect` / `PaintRoundBorderRect` / `PaintRectFactory`, and `PaintIconButton`'s body. **Not** `PaintGradientRect`, whose two stops are arguments |
| Pen colour | `SetPenColor`, `SetPenColorA` | Every stroke: the borders from `PaintRectFactory` (so `PaintBorderRect` and `PaintRoundBorderRect`), `PaintRoundOutline`, `PaintEllipse`'s rim, `PaintPolygon`'s outline, and every `PaintLine` rule |
| Fore colour | `SetForeColor`, `SetColors`, `SetForeColorA` | Text: `PaintText`, `PaintTextEx`, and `PaintIconButton`'s caption. **Not** `PaintChar`, which takes its colour as an argument |
| Back alpha | `SetBackColorA` | The same fills, blended against what is already on the surface |
| Pen alpha | `SetPenColorA` | The same strokes |
| Fore alpha | `SetForeColorA` | **Nothing.** Text is GDI, which has no alpha |

Colours are ordinary `COLORREF` values — `BGR(r,g,b)` — converted to GDI+'s ARGB internally, with
the byte order corrected.

Alpha defaults to 255 on all three, and **the plain setters reset it**. `SetBackColorA( c, 128 )`
followed by `SetBackColor( c )` is opaque again. That is what lets every colour call that does not
mention alpha simply mean "opaque".

---

## Constants

`PsBufferPaint.bi` defines no constants, enums or macros of its own. What you pass to the drawing
methods are ordinary Win32 values: `COLORREF` colours, `DT_*` flags for the text methods, and a GDI
`PS_*` pen style for `PaintRectFactory`.

The defaults that do exist are argument defaults:

| Method | Argument | Default | Meaning |
|---|---|---:|---|
| `PaintRectFactory` | `nPenWidth` | 1 | One-pixel stroke; 0 or less means no stroke |
| `PaintRectFactory` | `nCurvature` | 0 | Square corners, antialiasing off |
| `PaintBorderRect` | `nPenWidth` | 1 | One-pixel border |
| `PaintRoundRect` | `nCurvature` | 20 | Corner ellipse **diameter** in pixels |
| `PaintRoundBorderRect` | `nCurvature` | 20 | Corner ellipse **diameter** |
| `PaintRoundBorderRect` | `nPenWidth` | 1 | One-pixel border |
| `PaintRoundOutline` | `nCurvature` | 20 | Corner ellipse **diameter** |
| `PaintRoundOutline` | `nPenWidth` | 1 | One-pixel stroke; 0 or less draws nothing |
| `PaintEllipse` | `nPenWidth` | 0 | **Fill only, no rim** |
| `PaintPolygon` | `nPenWidth` | 0 | **Fill only, no outline** |
| `PaintGradientRect` | `nMode` | `LinearGradientModeVertical` (1) | A vertical blend across a horizontal bar |
| `PaintGradientRect` | `nCurvature` | 0 | Square corners |
| `PaintGradientRect` | `rcBrush` | 0 | Measure the ramp across `rc` itself |
| `PaintIconButton` | `nCurvature` | 20 | Corner ellipse **diameter** |

Alpha defaults to 255 (opaque) for the back, pen and fore colours.

## Licence

[Mozilla Public License 2.0](LICENSE).

MPL-2.0 is file-level copyleft, chosen deliberately for a drop-in control:

- **You may use this in closed-source software**, commercial or otherwise.
  §3.2 permits static linking with no additional conditions.
- **If you modify these files, publish those files' changes.** The obligation is
  per-file — your own sources are unaffected however tightly they are combined
  with these.
- The Exhibit B "Incompatible With Secondary Licenses" notice is **not applied**,
  which keeps this GPL-compatible.
