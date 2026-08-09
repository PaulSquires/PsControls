'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsOptionButton.bi  --  owner-drawn option button / radio button (ring + dot + caption)
''

#pragma once

' ONE THING PsBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0 in namespace
' AfxNova, and every host in this family says "using AfxNova" -- so ANY identifier named "ok"
' becomes a duplicate definition. The family convention is bOK.
#include once "PsBufferPaint.bi"
' The tooltip backend switch. The control ships on the SYSTEM (comctl32) backend exactly
' as it always has; a host opts an instance into PsTooltip with PsOptionButton_SetTooltipMode.
#include once "PsTipHost.bi"

' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the control.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a periodic cursor
' check acts as a safety net. Timer IDs are per-window, so every instance can share this id.
' Distinct from PsCheckBox's &hCBA0, PsButton's &hCB90 and PsToggle's &hCB70 purely as a
' debugging courtesy -- nothing requires it, since the ids never share a window.
#define IDT_PSOPTIONBUTTON_HOTTRACK   &hCBB0
#define PSOPTIONBUTTON_HOTTRACK_MS     100

' Stamped into the first field of every PSOPTIONBUTTON and checked before any other field is
' read. NEW TO THIS FAMILY, and it exists because this is the first control that holds handles
' to OTHER controls: everywhere else a bad HWND costs you a null pointer, here it could cost a
' dereference of someone else's window memory. See the GROUP MODEL header below -- the registry
' is what actually removes that hazard, and this is the belt to its braces.
#define PSOPTIONBUTTON_SIGNATURE   &h4F505442    ' 'OPTB'

' Default geometry. DPI scaling is applied once at Create -- but note the two exceptions below,
' which are NOT an oversight. Every setter afterwards takes raw pixels and the caller scales
' (the family rule).
#define PSOPTIONBUTTON_DEFAULT_PADLEFT     4
#define PSOPTIONBUTTON_DEFAULT_PADTOP      2
#define PSOPTIONBUTTON_DEFAULT_PADRIGHT    4
#define PSOPTIONBUTTON_DEFAULT_PADBOTTOM   2
' Gap between the box cell and the caption. Spent ONLY when there is a caption to separate the
' box from -- a caption-less option button charges padding + box + padding and nothing else.
#define PSOPTIONBUTTON_DEFAULT_BOXGAP      8
' The box CELL. Declared, never measured. The circle is inscribed in it (see LayoutOptionButton),
' so a non-square cell still yields a round button rather than a stadium.
#define PSOPTIONBUTTON_DEFAULT_BOXWIDTH   16
#define PSOPTIONBUTTON_DEFAULT_BOXHEIGHT  16

' THESE TWO ARE DELIBERATELY *NOT* DPI-SCALED AT CREATE, and the reason is the same for both:
' they are handed to PsBufferPaint.PaintRoundOutline, WHICH SCALES THE PEN ITSELF
' (PsBufferPaint.inc -- "DPI-scale the pen, exactly as PaintLine already does"). Scaling here as
' well would scale them TWICE and give a ring two pixels thick at 175% where one was asked for.
' This is the same double-scaling trap tiko hit with PsListBox_SetRowHeight. They DO end up
' DPI-aware; the painter is simply the thing that does it.
#define PSOPTIONBUTTON_DEFAULT_RINGTHICK   1
#define PSOPTIONBUTTON_DEFAULT_FOCUSTHICK  1

' The dot. 0 means DERIVE it as a percentage of the circle diameter, which is the default
' because the alternative silently decouples: a host that scales the box cell up to 24px and
' leaves a declared dot at 6px gets a button that looks broken and nothing warns it. A nonzero
' value overrides and IS DPI-scaled at Create, since unlike the pens above it is a distance and
' nothing downstream scales it. (PsVScrollBar_SetWheelStep's "0 = ask the system" precedent.)
#define PSOPTIONBUTTON_DEFAULT_DOTSIZE     0
#define PSOPTIONBUTTON_DOTRATIO_PCT       40

#define PSOPTIONBUTTON_DEFAULT_FOCUSGAP    2
#define PSOPTIONBUTTON_DEFAULT_FOCUSCURV   4     ' ellipse DIAMETER, not a radius

' Every option button starts here. A form with ONE logical group therefore needs no grouping
' calls at all; see the GROUP MODEL header below for why the group id is nonetheless a Create
' parameter rather than only a setter.
#define PSOPTIONBUTTON_DEFAULT_GROUPID     0


' Which side of the caption the box sits on.
'
' THE BOX CELL IS PINNED TO THE PADDING on whichever side it is given, and the caption takes the
' whole SPAN left over -- exactly the way PsCheckBox pins its box, PsButton its icon cells and
' PsComboBox its chevron. That single model gives both looks, decided by nothing but how the
' host sizes the control:
'
'   sized to PsOptionButton_GetIdealSize   (o) Highest quality           packed
'   sized to a full row width             Highest quality          (o)  settings row
'
' So a column of BOXALIGN_RIGHT option buttons all sized to the same width line their circles up
' for free, which is the reason a "pack it next to the caption" model was rejected.
enum
    OPT_BOXALIGN_LEFT = 0
    OPT_BOXALIGN_RIGHT
end enum

' Where the caption sits WITHIN ITS SPAN -- which is the leftover after the box cell and the
' padding, not the control. With BOXALIGN_LEFT and OPT_TEXTALIGN_RIGHT on an over-wide control
' the caption goes to the right-hand end of the span, not to the right-hand end of the window.
' (PsCheckBox's and PsButton's rcText carry the identical caveat and it is the natural thing to
' get wrong.)
enum
    OPT_TEXTALIGN_LEFT = 0
    OPT_TEXTALIGN_CENTER
    OPT_TEXTALIGN_RIGHT
end enum

' Which of the four colour sets the painter uses. Produced by PsOptionButton_ResolveMood, which
' is a PURE FUNCTION precisely so its truth table can be asserted rather than only inspected
' from inside a paint.
'
' isChecked is deliberately NOT a mood. It selects between two RING colours (and turns the dot
' on) within whichever mood resolved, so it survives all four for free instead of doubling the
' matrix.
enum
    OPT_MOOD_IDLE = 0
    OPT_MOOD_HOT
    OPT_MOOD_PRESSED
    OPT_MOOD_DISABLED
end enum

' Which rect the two offscreen probes look at. See PsOptionButton_CountRenderedTones and
' PsOptionButton_HashRenderedPart for why they exist.
enum
    OPT_PART_CONTROL = 0
    OPT_PART_BOX
    OPT_PART_TEXT
end enum


' Colors for the built-in painter. Copied on Set.
'
' FOUR MOODS, precedence  disabled > pressed > hot > idle.
'
' THE CONTROL READS COMPLETELY FLAT BY DEFAULT because BackColorHot, BackColorPressed and
' BackColorDisabled are all defaulted EQUAL to BackColor -- the same trick PsSelectBar uses for
' its hot cell and PsCheckBox for its row. Hovering therefore changes only the circle and the
' caption, which is what an option button in a settings pane should do. A host that wants a
' menu-style highlight across the whole row sets ONE field and gets it.
'
' THREE CIRCLE COLOUR SETS, not two. Unlike PsCheckBox -- whose box is a single composite GLYPH
' and therefore a single piece of ink in a single colour -- this control draws the circle as
' GEOMETRY, so the checked state can be an accent-FILLED disc with a CONTRASTING dot on top.
' That is the current Windows 11 / Fluent look and it is precisely what PsCheckBox's header says
' it cannot reach. The cost is a third quartet of fields:
'
'   RingColor*         the UNCHECKED circle: a muted outline, no fill
'   RingColorChecked*  the CHECKED circle: an accent FILL (this is the disc, not a stroke)
'   DotColor*          the dot drawn on top of that fill; defaults to white, NOT to ForeColor,
'                      because it sits on the accent rather than on the background
'
' THERE IS NO REFERENCE SCREENSHOT for this control. These defaults are INHERITED from
' PSCHECKBOX_COLORS and then accepted by eye, which is a weaker claim than "matched to a design"
' and is worth keeping distinct.
type PSOPTIONBUTTON_COLORS
    ' --- the control's own background, per mood. All four equal by default (see above). ---
    BackColor                as COLORREF = BGR( 33, 37, 43)
    BackColorHot             as COLORREF = BGR( 33, 37, 43)   ' = BackColor
    BackColorPressed         as COLORREF = BGR( 33, 37, 43)   ' = BackColor
    BackColorDisabled        as COLORREF = BGR( 33, 37, 43)   ' = BackColor
    ' --- the caption, per mood ---
    ForeColor                as COLORREF = BGR(200,205,214)
    ForeColorHot             as COLORREF = BGR(226,230,238)
    ForeColorPressed         as COLORREF = BGR(255,255,255)
    ForeColorDisabled        as COLORREF = BGR( 96,102,112)
    ' --- the circle, UNCHECKED: a muted OUTLINE, stroked and not filled ---
    RingColor                as COLORREF = BGR(138,144,153)
    RingColorHot             as COLORREF = BGR(176,182,191)
    RingColorPressed         as COLORREF = BGR(200,205,214)
    RingColorDisabled        as COLORREF = BGR( 84, 89, 97)
    ' --- the circle, CHECKED: an accent FILL ---
    RingColorChecked         as COLORREF = BGR( 86,156,214)
    RingColorCheckedHot      as COLORREF = BGR(122,180,230)
    RingColorCheckedPressed  as COLORREF = BGR(150,200,240)
    RingColorCheckedDisabled as COLORREF = BGR( 56, 70, 84)
    ' --- the dot, drawn ON the accent fill. Only ever visible when checked. ---
    DotColor                 as COLORREF = BGR(255,255,255)
    DotColorHot              as COLORREF = BGR(255,255,255)
    DotColorPressed          as COLORREF = BGR(255,255,255)
    DotColorDisabled         as COLORREF = BGR( 33, 37, 43)   ' = the default BackColor
    ' --- everything else ---
    FocusRingColor           as COLORREF = BGR( 86,156,214)
end type


' Everything the painter needs. Every rect is precomputed by LayoutOptionButton -- never
' re-derive one from another, and in particular never re-apply the padding to rcContent to get
' rcText, because the box cell, the box gap and the box ALIGNMENT can all change underneath you.
type PSOPTIONBUTTON_PAINTINFO
    hOptionButton as HWND              ' the control, so the callback can query it
    b             as PsBufferPaint ptr ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed by LayoutOptionButton ---
    rcClient      as RECT              ' the whole client area
    rcContent     as RECT              ' rcClient deflated by the focus-ring band
    rcBox         as RECT              ' the box CELL. Never empty -- the circle always exists
    ' THE INSCRIBED CIRCLE, which is what the built-in painter actually strokes and fills:
    ' the largest centred square inside rcBox. They are equal for the default square cell and
    ' differ the moment a host sets a non-square one. A callback that wants a round button
    ' should use rcCircle; one that wants to fill the whole declared cell wants rcBox.
    rcCircle      as RECT
    ' The dot, centred in rcCircle. ALWAYS COMPUTED, whether or not we are checked -- so a
    ' callback can cross-fade or draw an unchecked hover dot without re-deriving it. Only the
    ' built-in painter's decision to skip it when unchecked makes it invisible.
    rcDot         as RECT
    ' THE CAPTION SPAN, NOT THE INK. rcText is the whole box between the box cell and the far
    ' padding, so DT_END_ELLIPSIS has somewhere to ellipsize into; the caption is placed inside
    ' it by nTextAlign. Do NOT expect it to be a tight box around the glyphs -- that is the
    ' natural mistake here, and drawing a background behind "the text" using this rect fills the
    ' whole span. EMPTY when there is no caption, and DEGENERATE (left = right) when the client
    ' is too narrow to leave any span at all.
    rcText        as RECT
    rcVisual      as RECT              ' rcContent inflated by the focus-ring band
    ' --- State: the control decides these, you only render. ---
    isChecked     as boolean
    isHot         as boolean           ' the mouse is over the control
    isPressed     as boolean           ' a live left press AND the cursor is still inside
                                       '   (slide off and this goes false without ending the
                                       '   gesture)
    isEnabled     as boolean
    isFocused     as boolean           ' draw a focus ring when true
    ' --- What to draw ---
    wszText       as DWSTRING
    nRingThick    as long              ' RAW pixels -- PaintRoundOutline scales the pen itself
    nBoxAlign     as long              ' OPT_BOXALIGN_*
    nTextAlign    as long              ' OPT_TEXTALIGN_*; map to DT_LEFT / DT_CENTER / DT_RIGHT
end type

type PSOPTIONBUTTON_MESSAGEINFO
    hOptionButton as HWND
    uMsg          as UINT
    wParam        as WPARAM
    lParam        as LPARAM
end type


' Draw the whole control, INSTEAD OF the built-in painter. Paint through p->b (the control's
' double buffer for this repaint) -- do not touch the screen DC.
'
' The control has already filled the client with the resolved mood's BackColor before calling
' you, so a callback that only wants to add something on top does not have to repaint the
' background.
'
' TWO CONTRACTS WORTH HONOURING:
'   - Draw the caption with the SAME font you handed to PsOptionButton_SetFont. The ideal width
'     was measured with it, and a different font means the width lies.
'   - DO NOT reach for PaintBorderRect to draw the ring or the focus ring. It FILLS
'     unconditionally, so used as an outline it erases everything beneath it -- a mistake this
'     family has now made FOUR separate times (PsToggle, PsComboBox, PsNumericUpDown, and
'     PsDatePicker, where it rendered an entire popup calendar as one flat blue rectangle).
'     PaintRoundOutline is the one that strokes without filling, and PaintEllipse ALWAYS fills
'     regardless of its pen argument. PsOptionButton_CountRenderedTones exists so you can ASSERT
'     you have not done it.
type OPT_PaintCallbackSub as sub( byval p as PSOPTIONBUTTON_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' Like PsToggle, PsComboBox, PsButton and PsCheckBox -- and unlike the mouse-only siblings --
' this control hands over the FOCUS and KEYBOARD messages as well as the mouse ones, because
' they carry real state. The arrow keys reach you here BEFORE they move the group selection, so
' a host can veto navigation.
'
' CAUTION: the result is IGNORED for three messages.
'   WM_LBUTTONUP        - the control holds mouse capture across a press and the up-message is
'                         what releases it: a callback that suppressed it would strand capture
'                         and route every subsequent click to this control (the PsListBox bug
'                         recorded in Learnings.md). Suppressing WM_LBUTTONDOWN suppresses the
'                         press, never the capture bookkeeping.
'   WM_SETFOCUS         - focus is a FACT the system reports, not an action to veto. The state
'   WM_KILLFOCUS          is already updated by the time you are called; there is nothing left
'                         to suppress. A host that does not want the control focusable must not
'                         make it a tabstop.
' For every other message TRUE suppresses the control's default handling.
type OPT_MessageCallbackFunc as function( byval m as PSOPTIONBUTTON_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text on demand (only when a tip is about to show). Consulted ONLY when the
' control has no tooltip text of its own. Return "" for no tooltip.
'
' THERE IS NO CAPTION FALLBACK. An option button whose caption is already on screen would
' otherwise pop a tip that just repeats what you are looking at, so a control with neither its
' own text nor a callback answer simply shows nothing. (PsStatusBar and PsTabBar do fall back to
' the caption; that is their call, made for controls whose text is often truncated.)
type OPT_TooltipCallbackFunc as function( byval hOptionButton as HWND ) as DWSTRING

' The checked state changed through USER interaction -- a completed click, Space, an arrow-key
' move within the group, or the programmatic PsOptionButton_Click (which is an ACTION, not a
' setter; see that function).
'
' BOTH EDGES FIRE, AND THE LOSER FIRES FIRST. When the user checks B while A was checked:
'
'       OnChanged( hA, false )      <-- first
'       OnChanged( hB, true  )      <-- second
'
' That order is guaranteed, and it is the reason it is worth having: at no point during the
' notification is the group observably double-checked, so a host that reads
' PsOptionButton_FindChecked() from inside either callback gets a coherent answer. A host that
' only cares about the winner tests isChecked and ignores the rest.
'
' Fired AFTER the control's state is updated, so PsOptionButton_GetChecked() = isChecked.
'
' The programmatic PsOptionButton_SetChecked / SetGroupID / ClearGroup do NOT fire this
' (Win32's BM_SETCHECK / BN_CLICKED precedent), which is also what makes it safe to call them
' from inside this handler.
type OPT_CheckChangedCallbackSub as sub( byval hOptionButton as HWND, byval isChecked as boolean )


type PSOPTIONBUTTON
    ' MUST BE FIRST and must be checked before any other field is read. See
    ' PSOPTIONBUTTON_SIGNATURE above.
    dwSignature       as DWORD = PSOPTIONBUTTON_SIGNATURE
    hWin              as HWND
    ' The tooltip, whichever backend it is on. Replaces the old hToolTip + HoverTime
    ' pair; PsOptionButton_GetTooltipHandle still answers the comctl32 handle, and 0 while
    ' this instance is on PsTooltip.
    tip         as PSTIPHOST
    ' Instance-lifetime buffer that TTN_GETDISPINFOW's lpszText is pointed at. It must NOT be
    ' the same field as wszTooltip below: that one is the host's authored text, and overwriting
    ' it with a callback's answer would silently promote a transient string into stored state.
    wszTipBuf         as DWSTRING
    idc_OptionButton  as long = 0
    id                as long = 0        ' host command id, reported by FindCheckedID
    itemData          as integer = 0     ' free-form host payload
    ' --- Group ---
    ' The group is (hParent, nGroupID). hParent is cached at Create rather than re-read with
    ' GetParent on every lookup: a group scan runs on every click and every arrow key, and the
    ' parent of a control does not change in this family. It is ALSO what makes the registry
    ' scan safe to run from WM_NCDESTROY, where GetParent has already stopped being useful.
    hParent           as HWND
    nGroupID          as long = PSOPTIONBUTTON_DEFAULT_GROUPID
    ' --- Content ---
    wszText           as DWSTRING        ' the caption. "" = no caption (and no gap spent)
    wszTooltip        as DWSTRING        ' "" = ask TooltipCallback, then show nothing
    ' --- State ---
    isChecked         as boolean = false
    isEnabled         as boolean = true
    isHot             as boolean = false  ' the mouse is over the client
    isFocused         as boolean = false
    ' Set when a WM_KEYDOWN we consume will be followed by a WM_CHAR, so that character can be
    ' swallowed too. Without it the character reaches DefWindowProc and the system BEEPS on
    ' every Enter/Space -- the keystroke still works, so the beep is the only symptom.
    bAteKeyDown   as boolean = false
    isPressed         as boolean = false  ' a live left press (see the capture note below)
    hotTimerOn        as boolean = false  ' is the hot-tracking safety-net timer running?
    ' --- Press state (mouse capture). The control TAKES capture on a press: the press/cancel
    '     gesture (press, slide off the control, release, nothing gets checked) consumes the
    '     guaranteed down->up pairing, which is the family's test for taking capture. It pays
    '     the full price: snapshot the press BEFORE releasing (ReleaseCapture sends
    '     WM_CAPTURECHANGED synchronously -- Learnings.md), release on the up-message before any
    '     callback runs, WM_CAPTURECHANGED cancels the press, WM_DESTROY releases, and no
    '     callback may suppress an up-message.
    '     There is no bPressedInside flag: "the cursor is still inside" is exactly isHot, and
    '     hover tracking already maintains that through the capture. ---
    colors            as PSOPTIONBUTTON_COLORS
    ' Caller-supplied font (caller owns it). NOT named hFont: FreeBASIC is case-insensitive, so
    ' a member called hFont would shadow the TYPE name HFONT inside every member procedure of
    ' this type, and "dim as HFONT x" there fails with a misleading "error 14: Expected
    ' identifier, found 'HFONT'". See C:\dev\Learnings.md.
    '
    ' ONE font, not two. PsCheckBox needs a second (icon) font because its box is a glyph; this
    ' control draws the circle as geometry, so there is no glyph font, no SegoeFluentIcons.ttf
    ' in this repo, and no missing-glyph failure mode.
    hTextFont         as HFONT
    ' --- Layout inputs. All pixels; DPI-scaled once at Create EXCEPT the two pens. ---
    nBoxAlign         as long = OPT_BOXALIGN_LEFT
    nTextAlign        as long = OPT_TEXTALIGN_LEFT
    bAutoSize         as boolean = false   ' opt-in: the control SetWindowPos's ITSELF
    nPadLeft          as long = PSOPTIONBUTTON_DEFAULT_PADLEFT
    nPadTop           as long = PSOPTIONBUTTON_DEFAULT_PADTOP
    nPadRight         as long = PSOPTIONBUTTON_DEFAULT_PADRIGHT
    nPadBottom        as long = PSOPTIONBUTTON_DEFAULT_PADBOTTOM
    nBoxGap           as long = PSOPTIONBUTTON_DEFAULT_BOXGAP
    ' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
    ' that never name the real problem -- hence nBoxWidth throughout.
    nBoxWidth         as long = PSOPTIONBUTTON_DEFAULT_BOXWIDTH
    nBoxHeight        as long = PSOPTIONBUTTON_DEFAULT_BOXHEIGHT
    nRingThick        as long = PSOPTIONBUTTON_DEFAULT_RINGTHICK   ' NOT scaled here; see above
    nDotSize          as long = PSOPTIONBUTTON_DEFAULT_DOTSIZE     ' 0 = derive from the circle
    nFocusGap         as long = PSOPTIONBUTTON_DEFAULT_FOCUSGAP
    nFocusCurvature   as long = PSOPTIONBUTTON_DEFAULT_FOCUSCURV   ' ellipse DIAMETER
    nFocusThick       as long = PSOPTIONBUTTON_DEFAULT_FOCUSTHICK  ' NOT scaled here; see above
    ' --- Layout outputs. Rects are DERIVED, never set from outside; LayoutOptionButton() owns
    '     them. Layout is lazy: mutators mark it dirty, the next paint (or any rect/size query)
    '     runs it, which coalesces a burst of mutations into ONE measuring pass. ---
    nTextWidth        as long = 0     ' derived: the measured caption width
    nTextHeight       as long = 0     ' derived: one height for the control, from tmHeight
    nIdealW           as long = 0     ' derived
    nIdealH           as long = 0     ' derived
    nDotDiameter      as long = 0     ' derived: nDotSize, or the ratio when that is 0
    rcContent         as RECT         ' derived
    rcBox             as RECT         ' derived -- the declared CELL
    rcCircle          as RECT         ' derived -- the inscribed circle
    rcDot             as RECT         ' derived -- always, checked or not
    rcText            as RECT         ' derived -- the SPAN, see PSOPTIONBUTTON_PAINTINFO
    rcVisual          as RECT         ' derived
    bLayoutDirty      as boolean = true
    PaintCallback        as OPT_PaintCallbackSub          ' optional; replaces the built-in painter
    MessageCallback      as OPT_MessageCallbackFunc       ' optional
    TooltipCallback      as OPT_TooltipCallbackFunc       ' optional
    CheckChangedCallback as OPT_CheckChangedCallbackSub   ' optional; user-driven changes only

    declare sub      LayoutOptionButton()
    declare function RingPad() as long
    declare function HasText() as boolean
    declare sub      CancelPress()
    declare sub      Refresh()
end type


' The distance the visual bounds extend beyond the content on every side. The focus ring is
' drawn INSIDE this band, and the band is reserved UNCONDITIONALLY -- whether or not the control
' currently has focus -- which is what stops the content jumping when focus arrives. It
' therefore contributes to the IDEAL SIZE (PsToggle's rule).
function PSOPTIONBUTTON.RingPad() as long
    return this.nFocusGap + this.nFocusThick
end function

' "Is there a caption at all". A named member rather than an inline len() check because the
' layout, the painter and the ideal-size arithmetic must all agree about it, and three copies of
' `len(x) > 0` is three places to disagree.
function PSOPTIONBUTTON.HasText() as boolean
    return (len( this.wszText ) > 0)
end function

' Forget any live press WITHOUT touching the capture. Releasing capture is the WndProc's job,
' and only on the up-message or WM_DESTROY -- doing it here would let a callback strand or
' double-release it.
sub PSOPTIONBUTTON.CancelPress()
    this.isPressed = false
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is what
' makes layout lazy: a burst of setters costs one measuring pass, not N.
sub PSOPTIONBUTTON.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking control is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' Measure the caption and assign the rects. This is the only place option-button geometry is
' produced; painting, invalidation and every public size query consume it.
'
'   ringPad    = nFocusGap + nFocusThick        reserved ALWAYS, focused or not
'   textW      = the measured caption width, 0 when there is no caption
'   textH      = GetTextMetrics.tmHeight        ONE height for the control
'   textBlock  = hasText ? nBoxGap + textW : 0
'   contentH   = max( hasText ? textH : 0 , nBoxHeight )
'
'   nIdealW    = 2*ringPad + nPadLeft + nBoxWidth + textBlock + nPadRight
'   nIdealH    = 2*ringPad + nPadTop  + contentH + nPadBottom
'
'   rcContent  = rcClient deflated by ringPad on all four sides
'   BOXALIGN_LEFT   rcBox  = at rcContent.left + nPadLeft,  nBoxWidth x nBoxHeight, v-centred
'                   rcText = ( rcBox.right + nBoxGap , rcContent.right - nPadRight )
'   BOXALIGN_RIGHT  rcBox  = at rcContent.right - nPadRight - nBoxWidth, same, v-centred
'                   rcText = ( rcContent.left + nPadLeft , rcBox.left - nBoxGap )
'   rcCircle   = the largest CENTRED SQUARE inside rcBox
'   rcDot      = nDotDiameter, centred in rcCircle
'   rcVisual   = rcContent inflated by ringPad  ( = rcClient when the host sized us ideally )
'
' THE CIRCLE IS INSCRIBED IN THE CELL, NOT EQUAL TO IT. rcBox is the declared cell and is what
' the layout charges for; rcCircle is the round thing actually drawn in it. They coincide for
' the default 16x16 and diverge the instant a host sets a non-square cell -- at which point a
' painter that used rcBox would draw a STADIUM (PaintRoundOutline on a non-square rect is a
' rounded rectangle, not an ellipse) and the control would stop looking like a radio button.
' Deriving it here rather than in the painter is what stops the built-in painter and a host
' callback disagreeing about where the circle is.
'
' NOTE nIdealW IS THE SAME IN BOTH ALIGNMENTS. The mirror is exact -- the box cell, the gap and
' the two paddings are all still charged, they have merely swapped ends -- which is what lets a
' host flip the alignment on a laid-out form without re-measuring anything. Asserted, not
' assumed.
'
' THE GAP IS SPENT ONLY WHEN THERE IS A CAPTION TO SEPARATE FROM. A caption-less option button
' charges padLeft + box + padRight, so the circle sits properly centred in its own padding
' instead of being pushed off-centre by a gap with nothing on the other side of it.
'
' AN ABSENT CAPTION GIVES AN EMPTY rcText, not a zero-width one tucked against an edge. A paint
' callback can therefore test IsRectEmpty rather than having to re-derive presence from the
' string it was handed. rcBox and rcCircle are NEVER empty: the circle is what makes this an
' option button.
'
' CONTENT HEIGHT IGNORES THE FONT WHEN THERE IS NO CAPTION, which means a caption-less option
' button in a column of captioned ones has a SHORTER ideal height. That is the honest answer --
' nothing in it is as tall as a line of text -- and a host laying out a column takes the max
' itself rather than having a phantom text height baked in here.
'
' WHY THE MEASURING PASS RUNS BEFORE THE ZERO-CLIENT BAIL: nIdealW/nIdealH do not depend on the
' client area at all, and PsOptionButton_GetIdealSize is exactly what a host calls to decide how
' big to make the control in the FIRST place. Returning 0 until it had already been sized would
' be a chicken-and-egg trap.
'
' OVERFLOW: when the client is smaller than ideal the rects are computed HONESTLY rather than
' squeezed -- the circle keeps its declared size and stays pinned to its padding, and rcText is
' what collapses, clamped to EMPTY rather than going negative. So a too-narrow option button
' loses its caption to the ellipsis and keeps its circle, which is the useful failure.
sub PSOPTIONBUTTON.LayoutOptionButton()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    ' ---- the measuring pass --------------------------------------------------------------
    this.nTextWidth  = 0
    this.nTextHeight = 0

    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font -- never
    ' "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as TEXTMETRICW tm
    GetTextMetricsW( hDC, @tm )
    this.nTextHeight = tm.tmHeight

    ' Resolved ONCE for the whole pass rather than re-tested at each of the sites below: two
    ' separate len() tests would be correct today but invite a future edit that changes the
    ' caption mid-layout and produces a control measured one way and placed another.
    dim as boolean bText = this.HasText()

    if bText then
        dim as SIZE sz
        if GetTextExtentPoint32W( hDC, this.wszText.vptr, len( this.wszText ), @sz ) then
            this.nTextWidth = sz.cx
        end if
    end if

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' ---- the ideal size, which does not depend on the client area -------------------------
    dim as long nRingPad = this.RingPad()

    dim as long textBlock = 0
    if bText then textBlock = this.nBoxGap + this.nTextWidth

    dim as long contentH = this.nBoxHeight
    if bText then
        if this.nTextHeight > contentH then contentH = this.nTextHeight
    end if

    this.nIdealW = (2 * nRingPad) + this.nPadLeft + this.nBoxWidth + textBlock + this.nPadRight
    this.nIdealH = (2 * nRingPad) + this.nPadTop + contentH + this.nPadBottom

    ' ---- placement, which does ------------------------------------------------------------
    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is impossible,
    ' so leave the rects alone and ask to be run again -- but note this bail comes AFTER the
    ' ideal size is computed. See the header.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.rcContent = rcClient
    InflateRect( @this.rcContent, -nRingPad, -nRingPad )
    dim as long contH = this.rcContent.bottom - this.rcContent.top

    ' The box cell, pinned to its padding on whichever end it was given, always v-centred.
    dim as long boxY = this.rcContent.top + ((contH - this.nBoxHeight) \ 2)
    dim as long boxX
    if this.nBoxAlign = OPT_BOXALIGN_RIGHT then
        boxX = this.rcContent.right - this.nPadRight - this.nBoxWidth
    else
        boxX = this.rcContent.left + this.nPadLeft
    end if
    SetRect( @this.rcBox, boxX, boxY, boxX + this.nBoxWidth, boxY + this.nBoxHeight )

    ' The inscribed circle: the largest centred SQUARE in the cell. See the header.
    dim as long nSide = this.nBoxWidth
    if this.nBoxHeight < nSide then nSide = this.nBoxHeight
    if nSide < 0 then nSide = 0
    dim as long circX = this.rcBox.left + ((this.nBoxWidth  - nSide) \ 2)
    dim as long circY = this.rcBox.top  + ((this.nBoxHeight - nSide) \ 2)
    SetRect( @this.rcCircle, circX, circY, circX + nSide, circY + nSide )

    ' The dot. A declared nDotSize wins; 0 means derive it from the circle, which is what keeps
    ' the two in proportion when a host scales the cell up and forgets the dot exists.
    if this.nDotSize > 0 then
        this.nDotDiameter = this.nDotSize
    else
        this.nDotDiameter = (nSide * PSOPTIONBUTTON_DOTRATIO_PCT) \ 100
    end if
    ' A dot has to survive being rounded down to nothing at small sizes, and it must never grow
    ' past the disc it sits on.
    if this.nDotDiameter < 2 then this.nDotDiameter = 2
    if this.nDotDiameter > nSide then this.nDotDiameter = nSide
    dim as long dotX = this.rcCircle.left + ((nSide - this.nDotDiameter) \ 2)
    dim as long dotY = this.rcCircle.top  + ((nSide - this.nDotDiameter) \ 2)
    SetRect( @this.rcDot, dotX, dotY, dotX + this.nDotDiameter, dotY + this.nDotDiameter )

    if bText then
        dim as long tLeft, tRight
        if this.nBoxAlign = OPT_BOXALIGN_RIGHT then
            tLeft  = this.rcContent.left + this.nPadLeft
            tRight = this.rcBox.left - this.nBoxGap
        else
            tLeft  = this.rcBox.right + this.nBoxGap
            tRight = this.rcContent.right - this.nPadRight
        end if
        ' Degenerate rather than negative when the client is too narrow (see OVERFLOW above).
        if tRight < tLeft then tRight = tLeft
        dim as long tTop = this.rcContent.top + ((contH - this.nTextHeight) \ 2)
        SetRect( @this.rcText, tLeft, tTop, tRight, tTop + this.nTextHeight )
    else
        SetRectEmpty( @this.rcText )
    end if

    this.rcVisual = this.rcContent
    InflateRect( @this.rcVisual, nRingPad, nRingPad )
end sub


' ========================================================================================
' Which colour set does this state call for?  disabled > pressed > hot > idle.
'
' A PURE FUNCTION on purpose: it takes no control pointer and touches no global, so its whole
' truth table can be asserted directly instead of only being observable from inside a WM_PAINT.
' The painter and the self-test call the same one.
' ========================================================================================
function PsOptionButton_ResolveMood( _
            byval isEnabled as boolean, _
            byval isPressed as boolean, _
            byval isHot     as boolean _
            ) as long

    if isEnabled = false then return OPT_MOOD_DISABLED
    if isPressed then return OPT_MOOD_PRESSED
    if isHot then return OPT_MOOD_HOT
    return OPT_MOOD_IDLE
end function


' ========================================================================================
' Turn a mood plus the checked flag into the four colours the painter uses.
'
' Split out of the painter for the same reason as PsOptionButton_ResolveMood: "isChecked
' changes the RING colour and nothing else" has to hold in EVERY mood, and that is a claim worth
' asserting rather than eyeballing in eight screenshots. The checked test is applied INSIDE each
' mood arm rather than as a fifth mood, which is what keeps it one rule rather than four.
'
' clrDot is resolved unconditionally, including when unchecked. It costs nothing, and it means
' a paint callback that wants an unchecked hover dot does not have to reach into the colour
' struct itself and re-implement the mood precedence.
' ========================================================================================
sub PsOptionButton_ResolveColors( _
            byval pColors   as PSOPTIONBUTTON_COLORS ptr, _
            byval nMood     as long, _
            byval isChecked as boolean, _
            byref clrBack   as COLORREF, _
            byref clrFore   as COLORREF, _
            byref clrRing   as COLORREF, _
            byref clrDot    as COLORREF _
            )

    if pColors = 0 then exit sub

    with *pColors
        select case nMood
        case OPT_MOOD_DISABLED
            clrBack = .BackColorDisabled
            clrFore = .ForeColorDisabled
            clrDot  = .DotColorDisabled
            if isChecked then
                clrRing = .RingColorCheckedDisabled
            else
                clrRing = .RingColorDisabled
            end if
        case OPT_MOOD_PRESSED
            clrBack = .BackColorPressed
            clrFore = .ForeColorPressed
            clrDot  = .DotColorPressed
            if isChecked then
                clrRing = .RingColorCheckedPressed
            else
                clrRing = .RingColorPressed
            end if
        case OPT_MOOD_HOT
            clrBack = .BackColorHot
            clrFore = .ForeColorHot
            clrDot  = .DotColorHot
            if isChecked then
                clrRing = .RingColorCheckedHot
            else
                clrRing = .RingColorHot
            end if
        case else
            clrBack = .BackColor
            clrFore = .ForeColor
            clrDot  = .DotColor
            if isChecked then
                clrRing = .RingColorChecked
            else
                clrRing = .RingColor
            end if
        end select
    end with
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' AN OPTION BUTTON (radio button): a circle and an optional caption, on either side of each
' other, with a focus ring around the pair. Unchecked is a thin outlined ring; checked is an
' accent-filled disc with a contrasting dot on it. No chrome, no animation.
'
' MUTUAL EXCLUSION IS THIS CONTROL'S JOB, WHICH IS NEW IN THIS FAMILY
'   PsIconPanel's header says selection there is "a set, not a current item ... radio behaviour
'   is the host's", and PsPopupMenu says the same about its check glyph. Every prior sibling
'   pushed exclusivity onto the host. This one owns it.
'
' THE GROUP MODEL: DECLARED, NOT DISCOVERED
'   A group is the pair (parent HWND, group id). Every live instance registers itself in a
'   private module-level list at Create and removes itself at WM_NCDESTROY, and every group
'   operation is a scan of that list.
'
'   IT IS NOT A SIBLING WALK, AND IT CANNOT BE. Two facts about AfxNova make the obvious
'   "GetWindow(GW_HWNDNEXT) and compare class names" approach impossible rather than merely
'   inelegant:
'     - CWindow.Create auto-registers a PER-INSTANCE window class named "FBWindowClass:N",
'       where N is a process-global counter. Two PsOptionButtons therefore have DIFFERENT
'       class names, and the name is not stable across runs. There is no class constant to
'       compare against.
'     - AfxCWindowPtr is an unchecked GetWindowLongPtrW(hwnd, 0). On a window that is not a
'       CWindow it returns whatever that class put in slot 0, and dereferencing it is not a
'       null return -- it is someone else's memory.
'   The registry removes that entire hazard class: the control only ever touches HWNDs it
'   created itself. GetOptionButtonPointer resolves through the REGISTRY rather than through
'   AfxCWindowPtr for exactly this reason -- a departure from the family idiom, and a
'   deliberate one.
'
'   ARROW ORDER IS CREATION ORDER, not screen position. It is the registry's order. That is
'   stable and matches how a host writes a group top-to-bottom; a host that creates its buttons
'   out of visual order gets arrow navigation in the order it created them, which is worth
'   knowing before blaming the control.
'
' EXACTLY ONE MEMBER OF A GROUP CARRIES WS_TABSTOP
'   The checked one, or the first enabled one when none is checked. So the GROUP is a single
'   tab stop and Tab moves past the whole thing -- which is what BS_AUTORADIOBUTTON does, and
'   is the reason the arrow keys exist at all. The invariant is maintained at four sites:
'   Create, a check moving, SetEnabled on the holder, and WM_NCDESTROY of the holder. The
'   fourth is the one that is easy to miss: destroying the tabstop holder without handing it on
'   drops the entire group out of the tab order silently.
'
' A GROUP WITH NOTHING CHECKED IS LEGAL
'   It is the state a group starts in, and it is where a group returns after ClearGroup or
'   after the checked member is destroyed. THE USER CAN NEVER REACH IT: clicking the checked
'   member is a silent no-op, and there is no gesture that unchecks. Only the host can empty a
'   group.
'
' ENTER IS NOT CLAIMED, AND THAT DEPARTS FROM PsCheckBox / PsToggle / PsButton
'   All three of those take VK_RETURN. A Win32 radio button does not -- it lets Enter reach the
'   dialog's default button. Since this control commits to Win32 group semantics everywhere
'   else, taking Enter would contradict that in the one place a user would actually notice:
'   pressing Enter to accept a dialog after picking an option. Asserted NEGATIVELY, so a future
'   edit that "restores family consistency" turns the suite red instead of changing behaviour
'   quietly.
'
' NO PUMP OBLIGATION
'   There is no PsOptionButton_FilterMessage and none is needed. Like PsCheckBox and PsButton,
'   and unlike PsComboBox / PsDatePicker / PsTextBox / PsMenuBar, this control adds nothing to
'   the host's message loop. Tab NAVIGATION still needs IsDialogMessage in the host pump, as it
'   does for any tabstop, and a host must SetFocus its first control itself -- IsDialogMessage
'   ignores Tab until a descendant already has focus.
'
' DELIBERATELY ABSENT
'   Tri-state. Mnemonics (PaintText forces DT_NOPREFIX, so the renderer enforces it).
'   Multiline. HICON. WM_COMMAND to the parent. HitTest. CS_DBLCLKS (a rapid second click on
'   the checked member is a legitimate no-op, and a double-click never spans two windows
'   anyway). Groups spanning different parents. Z-order-based arrow ordering.
' ========================================================================================


' ---- Creation --------------------------------------------------------------------------

' Create an option button as a child of hWndParent.
'
' nGroupID is a CREATE PARAMETER and not only a setter because grouping is the one thing this
' control cannot be correct without: a forgotten SetGroupID does not fail, it silently merges
' two groups into one and the form still looks right until a user clicks. The default of 0 means
' a form with a single logical group needs no grouping calls at all.
'
' The control is created 0x0 and hidden. Size it with PsOptionButton_GetIdealSize (valid
' immediately, before the control has ever been given a size) and show it.
declare function PsOptionButton_Create( byval hWndParent as HWND, _
                                        byval CtrlID as long, _
                                        byval nGroupID as long = PSOPTIONBUTTON_DEFAULT_GROUPID _
                                        ) as HWND

' ---- Group -----------------------------------------------------------------------------

declare function PsOptionButton_GetGroupID( byval hOptionButton as HWND ) as long
' Move a button to another group. SILENT. Both the old and the new group have their tab-stop
' invariant re-established, and if the button was the checked member of its old group that group
' is left with nothing checked (the legal empty state) rather than having the check reassigned
' to an arbitrary survivor.
declare sub      PsOptionButton_SetGroupID( byval hOptionButton as HWND, byval nGroupID as long )
' How many live members the button's group has, including the button itself.
declare function PsOptionButton_GetGroupCount( byval hOptionButton as HWND ) as long
' The idx'th member of the button's group in CREATION order. 0 if idx is out of range.
declare function PsOptionButton_GetGroupMember( byval hOptionButton as HWND, byval idx as long ) as HWND
' The checked member of the button's group, or 0 when the group is empty-checked.
declare function PsOptionButton_GetGroupChecked( byval hOptionButton as HWND ) as HWND
' Same question asked WITHOUT holding a member handle -- for a host that wants to read a group
' back before it has stored any of the button handles. 0 / -1 when nothing is checked.
declare function PsOptionButton_FindChecked( byval hWndParent as HWND, byval nGroupID as long ) as HWND
declare function PsOptionButton_FindCheckedID( byval hWndParent as HWND, byval nGroupID as long ) as long
' Uncheck every member of the button's group. SILENT, like every other programmatic setter. The
' group is left in the legal nothing-checked state and keeps its tab stop.
declare sub      PsOptionButton_ClearGroup( byval hOptionButton as HWND )

' ---- Content ---------------------------------------------------------------------------

declare function PsOptionButton_GetText( byval hOptionButton as HWND ) as DWSTRING
declare sub      PsOptionButton_SetText( byval hOptionButton as HWND, byval Text as DWSTRING )
declare function PsOptionButton_GetID( byval hOptionButton as HWND ) as long
declare sub      PsOptionButton_SetID( byval hOptionButton as HWND, byval id as long )
declare function PsOptionButton_GetItemData( byval hOptionButton as HWND ) as integer
declare sub      PsOptionButton_SetItemData( byval hOptionButton as HWND, byval itemData as integer )

' ---- State -----------------------------------------------------------------------------

declare function PsOptionButton_GetChecked( byval hOptionButton as HWND ) as boolean
' SILENT -- fires no callback (Win32's BM_SETCHECK precedent), which is what makes it safe to
' call from inside a CheckChangedCallback.
'
' Checking unchecks every peer in the group and moves the group's tab stop here. Unchecking
' (isChecked = false) is ALLOWED and leaves the group with nothing checked; that is a legal
' state the user cannot reach, and PsOptionButton_ClearGroup is the whole-group form of it.
declare sub      PsOptionButton_SetChecked( byval hOptionButton as HWND, byval isChecked as boolean )
declare function PsOptionButton_GetEnabled( byval hOptionButton as HWND ) as boolean
' Goes through EnableWindow, so the disable is enforced by the system rather than being a
' cosmetic flag. Disabling the member that holds the group's tab stop hands it to another
' enabled member.
declare sub      PsOptionButton_SetEnabled( byval hOptionButton as HWND, byval isEnabled as boolean )
declare function PsOptionButton_GetFocused( byval hOptionButton as HWND ) as boolean
declare sub      PsOptionButton_Refresh( byval hOptionButton as HWND )

' ---- Action ----------------------------------------------------------------------------

' Check this button AS IF THE USER HAD CLICKED IT: it FIRES the change callbacks, both edges,
' loser first. An ACTION, not a setter -- PsButton_Click's and PsCheckBox_Toggle's precedent --
' and with no mnemonics it is the only door a host accelerator has into this control.
'
' A no-op (and silent) when the button is already checked or is disabled.
declare sub      PsOptionButton_Click( byval hOptionButton as HWND )

' ---- Layout ----------------------------------------------------------------------------

declare function PsOptionButton_GetBoxAlign( byval hOptionButton as HWND ) as long
declare sub      PsOptionButton_SetBoxAlign( byval hOptionButton as HWND, byval nBoxAlign as long )
declare function PsOptionButton_GetTextAlign( byval hOptionButton as HWND ) as long
declare sub      PsOptionButton_SetTextAlign( byval hOptionButton as HWND, byval nTextAlign as long )
declare sub      PsOptionButton_GetPadding( byval hOptionButton as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
declare sub      PsOptionButton_SetPadding( byval hOptionButton as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
declare function PsOptionButton_GetBoxGap( byval hOptionButton as HWND ) as long
declare sub      PsOptionButton_SetBoxGap( byval hOptionButton as HWND, byval nBoxGap as long )
declare sub      PsOptionButton_GetBoxSize( byval hOptionButton as HWND, byref nBoxWidth as long, byref nBoxHeight as long )
declare sub      PsOptionButton_SetBoxSize( byval hOptionButton as HWND, byval nBoxWidth as long, byval nBoxHeight as long )
' RAW pixels. PaintRoundOutline DPI-scales the pen it is handed, so do NOT pre-scale this.
declare function PsOptionButton_GetRingThickness( byval hOptionButton as HWND ) as long
declare sub      PsOptionButton_SetRingThickness( byval hOptionButton as HWND, byval nThickness as long )
' 0 = derive the dot from the circle diameter (the default, and what keeps the two in
' proportion when the box size changes). Nonzero = a declared diameter in pixels.
declare function PsOptionButton_GetDotSize( byval hOptionButton as HWND ) as long
declare sub      PsOptionButton_SetDotSize( byval hOptionButton as HWND, byval nDotSize as long )
declare sub      PsOptionButton_GetFocusRing( byval hOptionButton as HWND, byref nGap as long, byref nThickness as long )
declare sub      PsOptionButton_SetFocusRing( byval hOptionButton as HWND, byval nGap as long, byval nThickness as long )
declare function PsOptionButton_GetFocusCurvature( byval hOptionButton as HWND ) as long
declare sub      PsOptionButton_SetFocusCurvature( byval hOptionButton as HWND, byval nCurvature as long )
declare function PsOptionButton_GetAutoSize( byval hOptionButton as HWND ) as boolean
declare sub      PsOptionButton_SetAutoSize( byval hOptionButton as HWND, byval bAutoSize as boolean )
declare sub      PsOptionButton_GetIdealSize( byval hOptionButton as HWND, byref nWidth as long, byref nHeight as long )
declare function PsOptionButton_GetContentRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
declare function PsOptionButton_GetBoxRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
declare function PsOptionButton_GetCircleRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
declare function PsOptionButton_GetDotRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
declare function PsOptionButton_GetTextRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
declare function PsOptionButton_GetVisualRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean

' ---- Appearance ------------------------------------------------------------------------

declare sub      PsOptionButton_GetColors( byval hOptionButton as HWND, byval pColors as PSOPTIONBUTTON_COLORS ptr )
declare sub      PsOptionButton_SetColors( byval hOptionButton as HWND, byval pColors as PSOPTIONBUTTON_COLORS ptr )
declare function PsOptionButton_GetFont( byval hOptionButton as HWND ) as HFONT
declare sub      PsOptionButton_SetFont( byval hOptionButton as HWND, byval hTextFont as HFONT )

' ---- Tooltips --------------------------------------------------------------------------

declare function PsOptionButton_GetTooltipText( byval hOptionButton as HWND ) as DWSTRING
declare sub      PsOptionButton_SetTooltipText( byval hOptionButton as HWND, byval Text as DWSTRING )
declare function PsOptionButton_GetTooltipHandle( byval hOptionButton as HWND ) as HWND
declare sub      PsOptionButton_SetHoverTime( byval hOptionButton as HWND, byval milliseconds as long )
declare function PsOptionButton_SetTooltipMode( byval hOptionButton as HWND, byval nMode as long ) as boolean
declare function PsOptionButton_GetTooltipMode( byval hOptionButton as HWND ) as long
' The PsTooltip window, or 0 on the system backend. The door to PsTooltip's own
' SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph -- deliberately not mirrored
' here, since thirteen controls x twenty setters is 260 wrappers to keep in step.
declare function PsOptionButton_GetPsTooltipHandle( byval hOptionButton as HWND ) as HWND
' Honoured by BOTH backends. A delay never set keeps the backend's own derivation from
' the system double-click time.
declare sub      PsOptionButton_SetAutoPopTime( byval hOptionButton as HWND, byval milliseconds as long )
declare sub      PsOptionButton_SetReshowTime( byval hOptionButton as HWND, byval milliseconds as long )

' ---- Callbacks -------------------------------------------------------------------------

declare sub      PsOptionButton_SetPaintCallback( byval hOptionButton as HWND, byval usersub as OPT_PaintCallbackSub )
declare sub      PsOptionButton_SetMessageCallback( byval hOptionButton as HWND, byval userfunc as OPT_MessageCallbackFunc )
declare sub      PsOptionButton_SetTooltipCallback( byval hOptionButton as HWND, byval userfunc as OPT_TooltipCallbackFunc )
declare sub      PsOptionButton_SetCheckChangedCallback( byval hOptionButton as HWND, byval usersub as OPT_CheckChangedCallbackSub )

' ---- Plumbing --------------------------------------------------------------------------

' Render one part offscreen and report how many DISTINCT colours came out of it. PUBLIC ON
' PURPOSE (PsButton's and PsCheckBox's precedent): a host that supplies its own PaintCallback
' should be able to assert it is not flooding the control, because every occurrence of that bug
' in this family so far has been in a host callback rather than in a control's own painter.
' A result of 1 means the part is a solid block of one colour.
declare function PsOptionButton_CountRenderedTones( byval hOptionButton as HWND, byval nPart as long ) as long
' FNV-1a over the same offscreen render. Two different states must hash differently; the same
' state twice must hash identically. Use it to assert that a state change actually reached the
' pixels rather than only reaching a field.
declare function PsOptionButton_HashRenderedPart( byval hOptionButton as HWND, byval nPart as long ) as ulong
declare sub      PsOptionButton_RunSelfTest( byval hWndParent as HWND )
