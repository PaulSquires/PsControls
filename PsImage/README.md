# PsImage

A small support class for loading a raster image — `.ico`, `.png`, `.bmp`, `.jpg`, or an existing
`HBITMAP`/`HICON` — so an owner-drawn control can show a **real picture in an icon slot** instead
of (or as well as) a Segoe Fluent Icons glyph.

It is not a control. It is a `TYPE` you construct, point at a file once, and keep alongside a
control for as long as that control needs the image. It does exactly three things — **load**
(decode once, at natural size), **measure** (report natural width/height), and hand out the
decoded image — and it does **not** draw. The drawing is [`PsBufferPaint.PaintImage`](#drawing),
which owns the paint surface's GDI+ graphics object. Load here, draw there.

PsImage is the loader that pairs with `PsBufferPaint`, in the same way `PsBufferPaint` is the
surface every Ps\* control paints through.

Repository: <https://github.com/PaulSquires/PsImage>

---

## Requirements

**Files to copy into your project:**

| File | Purpose |
|---|---|
| `PsImage.bi` | Declarations — the `PsImage` type |
| `PsImage.inc` | Implementation |
| `PsBufferPaint.bi` / `.inc` | The surface whose `PaintImage` actually draws the loaded image |

**AfxNova is required.** `PsImage.bi` includes `AfxNova\CGdiPlus.inc` itself, so you do not have
to. Sources include AfxNova relative to the workspace root, so builds need it on the include path:

```bash
fbc64.exe -i "C:\dev" main.bas
```

**Include order.** `PsImage.bi` pulls in its own GDI+ dependency; the only rule is that
`PsImage.inc` comes before anything that uses it:

```freebasic
#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova

#include once "PsBufferPaint.inc"
#include once "PsImage.inc"
#include once "frmMain.inc"        ' your own forms
```

**GDI+ must be running before the first load and must outlive the last image.** Decoding builds
GDI+ objects, so bracket your message loop — the same bracket `PsBufferPaint` already asks for:

```freebasic
dim as ULONG_PTR gdipToken = AfxGdipInit()
' ... create windows, load images, run the message loop ...
AfxGdipShutdown( gdipToken )      ' AFTER every PsImage has been destroyed
```

**Never name an identifier `ok`.** GDI+ defines `Ok = 0` as a `Status` enum value in namespace
`AfxNova`, and hosts customarily say `using AfxNova`. An existing variable, parameter or function
called `ok` becomes a duplicate definition the moment you adopt these files. Use `bOK` instead.
(`PsBufferPaint` asks the same; it is repeated here because PsImage can be adopted on its own.)

**There is no pump obligation.** PsImage owns no window and no timer — there is nothing to add to
your `GetMessage` loop, and no `FilterMessage` to call.

---

## Usage

Load once (e.g. when you create the control), then hand the decoded image to the buffer inside
your paint routine:

```freebasic
' --- once ---
dim as PsImage img
if img.Load( "C:\icons\save.png" ) then
    ' loaded
end if

' --- in WM_PAINT, into an icon-cell RECT you computed ---
if img.IsValid() then
    b.PaintImage( img.Image(), @rcIcon )        ' PS_IMGFIT_ASPECT by default
end if
```

An image loaded through the icon-bearing controls (`PsButton`, `PsIconPanel`, `PsToolbar`) is
handled for you — see their own docs for `PsButton_SetImageLeft`, `PsIconPanel_SetImage`,
`PsToolbar_SetItemImage` and friends. You only touch PsImage directly when you paint your own
surface.

---

## API

| Member | Returns | Description |
|---|---|---|
| `Load( wszPath as DWSTRING )` | `long` | Decode a file. `.ico` / `.png` / `.bmp` / `.jpg` / `.jpeg` / `.gif` / `.tif`. Non-zero on success; 0 on failure (bad path, unsupported/corrupt file). A failed load clears any image previously held. |
| `LoadFromBitmap( hbm as HBITMAP )` | `long` | Wrap an existing `HBITMAP` (GDI+ takes a copy; you keep ownership of `hbm`). Non-zero on success. |
| `LoadFromIcon( hIcon as HICON )` | `long` | Wrap an existing `HICON` (GDI+ takes a copy; you still `DestroyIcon` it). Non-zero on success. |
| `IsValid()` | `long` | Non-zero once a load has succeeded and nothing has failed since. |
| `NaturalWidth()` | `long` | Decoded pixel width, or 0 when not valid. |
| `NaturalHeight()` | `long` | Decoded pixel height, or 0 when not valid. |
| `Image()` | `CGpImage ptr` | The decoded image, for `PsBufferPaint.PaintImage`. `NULL` when not valid. **The PsImage keeps ownership — do not delete it.** |

Notes:

- **Decode happens once, at natural size.** Scaling to a cell is done at draw time by
  `PaintImage`, with high-quality bicubic interpolation, so one PsImage can be drawn into cells
  of different sizes without reloading.
- **`.ico` uses a fast path** (`LoadImageW` → the `HICON` constructor) so the system picks the
  best-matching icon frame, then falls back to the generic file decoder if that fails.
- **Alpha is preserved.** A transparent `.png`/`.ico` composites over whatever the control has
  already painted into the cell, so fill the cell background first if you want one.

<a name="drawing"></a>
## Drawing — `PsBufferPaint.PaintImage`

```freebasic
b.PaintImage( pImage as CGpImage ptr, rc as RECT ptr, nFit as long = PS_IMGFIT_ASPECT )
```

| `nFit` | Placement |
|---|---|
| `PS_IMGFIT_ASPECT` (default) | Scale to fit inside `rc`, aspect preserved, then centre (letterbox). |
| `PS_IMGFIT_STRETCH` | Fill `rc` exactly, aspect ignored. |
| `PS_IMGFIT_CENTER` | Natural size, centred, clipped by `rc` if larger. No scaling. |

`PaintImage` lives on `PsBufferPaint` rather than on PsImage because it must draw through the
buffer's own GDI+ graphics — a control building a second graphics object on the same device
context would put two independently-batching GDI+ objects on one surface with nothing
coordinating them.

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
