'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsProgressBar.bi  --  owner-drawn progress bar (track + fill, determinate or floating)
''

#pragma once

' ONE THING PsBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0, and it lands at
' GLOBAL scope (AfxGdiPlus.bi has no NAMESPACE statement of its own), so ANY identifier named
' "ok" becomes a duplicate definition -- dropping "using AfxNova" does not save you, and
' FreeBASIC is case-insensitive so ok/Ok/OK all collide. The family convention is bOK.
#include once "PsBufferPaint.bi"
' The tooltip backend switch. The control ships on the SYSTEM (comctl32) backend exactly as
' it always has; a host opts an instance into PsTooltip with PsProgressBar_SetTooltipMode.
#include once "PsTipHost.bi"

' The marquee animation timer. This is the ONLY timer this control ever runs, and it runs
' ONLY in indeterminate mode -- there is no hot-tracking poll here, because a progress bar
' has no hot state to strand (see the PUBLIC API header). A determinate bar costs nothing.
' Timer IDs are per-window, so every instance can share this id; it is distinct from the
' siblings' purely as a debugging courtesy.
#define IDT_CPROGRESSBAR_MARQUEE   &hCBB0

' Defaults. The SIZE values are DPI-scaled at Create; every setter afterwards takes raw
' pixels and the caller scales (the family rule). The two THICKNESS/CURVATURE values are NOT
' DPI-scaled, ever -- a hairline should stay a hairline (PsMenuBar's nSeparatorThickness).
#define PSPROGRESSBAR_DEFAULT_THICKNESS      0   ' 0 = fill the client's cross axis
#define PSPROGRESSBAR_DEFAULT_BARINSET       0
#define PSPROGRESSBAR_DEFAULT_BORDERTHICK    0   ' not DPI-scaled; 0 = no track border
#define PSPROGRESSBAR_DEFAULT_CURVATURE     -1   ' not DPI-scaled; -1 = stadium (see below)
#define PSPROGRESSBAR_DEFAULT_BLOCKLEN      10
#define PSPROGRESSBAR_DEFAULT_BLOCKGAP       3
#define PSPROGRESSBAR_DEFAULT_MARQUEE_MS    30
#define PSPROGRESSBAR_DEFAULT_MARQUEE_STEP   2
#define PSPROGRESSBAR_DEFAULT_CHUNK_PCT     25

' A CURVATURE OF -1 MEANS "STADIUM": the painter substitutes the track's cross-axis extent,
' which PsBufferPaint's BuildRoundPath then clamps to half, making both ends exact
' semicircles. It is a sentinel rather than a real measurement because the track thickness is
' not known until layout runs, and it is what makes the default look survive a resize.
#define PSPROGRESSBAR_CURVATURE_STADIUM     -1


' Orientation. FIXED AT CREATE and immutable afterwards -- PsSplitter's rule. Named for the
' axis the bar GROWS along, not for the shape of the control.
enum
    PRG_HORIZONTAL = 0
    PRG_VERTICAL
end enum

' Determinate (the position means something) or indeterminate (a chunk floats, and the
' position means nothing at all).
enum
    PRG_MODE_DETERMINATE = 0
    PRG_MODE_INDETERMINATE
end enum

' How the indeterminate chunk moves.
'   BOUNCE  ping-pongs off both ends. The default, and the one this control was asked for.
'   WRAP    slides one way, leaves by one end and re-enters at the other. This is what
'           comctl32's PBS_MARQUEE does, kept so "standard Win32 progress bar" is honest.
enum
    PRG_MARQUEE_BOUNCE = 0
    PRG_MARQUEE_WRAP
end enum

' How the bar is filled. ORTHOGONAL to segmentation: all four combinations are legal.
enum
    PRG_FILL_SOLID = 0
    PRG_FILL_GRADIENT
end enum

' comctl32's PBM_SETSTATE. Each swaps the bar's colour PAIR, so it works for gradient fills
' as well as solid ones.
enum
    PRG_STATE_NORMAL = 0
    PRG_STATE_PAUSED
    PRG_STATE_ERROR
end enum

' What, if anything, is written across the bar.
'   NONE      the comctl32 behaviour: a real Win32 progress bar draws no text.
'   PERCENT   "45%"  -- the position as a whole-number percentage of the range.
'   VALUE     "45"   -- the raw position.
'   CALLBACK  whatever PRG_TextCallbackFunc returns.
enum
    PRG_TEXT_NONE = 0
    PRG_TEXT_PERCENT
    PRG_TEXT_VALUE
    PRG_TEXT_CALLBACK
end enum

' Which rect the two offscreen probes look at. See PsProgressBar_CountRenderedTones and
' PsProgressBar_HashRenderedPart for why they exist.
enum
    PRG_PART_CONTROL = 0
    PRG_PART_TRACK
    PRG_PART_BAR
    PRG_PART_TEXT
end enum


' Colors for the built-in painter. Copied on Set.
'
' THE BAR CARRIES A PAIR PER STATE, not a single colour: the second stop is what a gradient
' fill ramps to. PRG_FILL_SOLID uses the START colour only and ignores the End field, so one
' struct serves both fill styles and switching between them needs no colour changes.
'
' THERE ARE NO HOT COLOURS. This control has no hot state -- it cannot be interacted with, so
' hovering it means nothing. That is a deliberate departure from every focusable sibling, and
' it is why no hot-tracking timer exists here.
'
' THESE DEFAULTS ARE INHERITED, NOT MATCHED. No reference screenshot existed for this control,
' so the track and accent are PSTOGGLE_COLORS' values (which is what makes it drop into tiko's
' theme unchanged) and the paused/error pairs are authored. That is a weaker claim than the
' controls drawn from a reference, and worth keeping distinct from one.
type PSPROGRESSBAR_COLORS
    BackColor            as COLORREF = BGR( 33, 37, 43)   ' the control's own background
    TrackColor           as COLORREF = BGR( 51, 55, 63)
    TrackBorderColor     as COLORREF = BGR( 72, 77, 86)   ' only drawn when nBorderThick > 0
    ' --- the bar: start/end stop pair per state ---
    BarColor             as COLORREF = BGR( 86,156,214)
    BarColorEnd          as COLORREF = BGR( 52,110,170)
    BarColorPaused       as COLORREF = BGR(222,184, 70)
    BarColorPausedEnd    as COLORREF = BGR(178,140, 34)
    BarColorError        as COLORREF = BGR(224, 96, 96)
    BarColorErrorEnd     as COLORREF = BGR(176, 56, 56)
    BarColorDisabled     as COLORREF = BGR( 84, 89, 97)
    BarColorDisabledEnd  as COLORREF = BGR( 62, 67, 75)
    ' --- text. Two colours, split at the fill boundary (see PRG_TEXT_*). ---
    TextColor            as COLORREF = BGR(190,196,206)   ' over the EMPTY track
    TextColorOnBar       as COLORREF = BGR(255,255,255)   ' over the FILLED bar
    TextColorDisabled    as COLORREF = BGR( 90, 96,106)   ' both halves, when disabled
end type


' Everything the painter needs. Every rect is precomputed by LayoutProgressBar -- never
' re-derive one from another, and in particular never rebuild rcBar from rcTrack by repeating
' the inset-and-percentage arithmetic, because the mode, the reverse flag and the marquee tick
' all feed into it.
type PSPROGRESSBAR_PAINTINFO
    hProgressBar     as HWND                ' the control, so the callback can query it
    b                as PsBufferPaint ptr   ' the control's buffer for this repaint (no copy)
    ' --- Geometry, all precomputed ---
    rcClient         as RECT                ' the whole client area
    rcTrack          as RECT                ' the groove
    rcFill           as RECT                ' rcTrack deflated by nBarInset: where a 100% bar reaches
    rcBar            as RECT                ' the drawn portion. In indeterminate mode this is
                                            '   the floating chunk, and it is NOT anchored to
                                            '   either end of rcFill.
    ' --- Model ---
    nPos             as long
    nMin             as long
    nMax             as long
    nPercent         as long                ' 0..100, already clamped; 0 when the range is empty
    ' --- State: the control decides these, you only render. ---
    isIndeterminate  as boolean
    isEnabled        as boolean
    isReverse        as boolean             ' the bar grows from the right / from the top
    isSegmented      as boolean
    nOrientation     as long                ' PRG_HORIZONTAL / PRG_VERTICAL
    nState           as long                ' PRG_STATE_*
    nFillStyle       as long                ' PRG_FILL_*
    nCurvature       as long                ' already resolved: the stadium sentinel is gone
    nBlockLength     as long                ' DPI-scaled, as drawn
    nBlockGap        as long                ' DPI-scaled, as drawn
    ' --- Text, already resolved through the mode and any callback. "" = draw nothing. ---
    wszText          as DWSTRING
end type

type PSPROGRESSBAR_MESSAGEINFO
    hProgressBar as HWND
    uMsg         as UINT
    wParam       as WPARAM
    lParam       as LPARAM
end type


' Draw the whole control, INSTEAD OF the built-in painter. Paint through p->b (the control's
' double buffer for this repaint) -- do not touch the screen DC.
'
' The control has already filled the client with BackColor before calling you, so a callback
' that only wants to add something on top does not have to repaint the background.
'
' THE MISTAKE TO AVOID, made four separate times in this family: PsBufferPaint's
' PaintBorderRect and PaintRoundBorderRect FILL before they stroke. Used as a frame over
' something you have already drawn, they erase it. Stroke with PaintRoundOutline instead.
' PsProgressBar_CountRenderedTones exists so you can ASSERT you have not done it.
type PRG_PaintCallbackSub as sub( byval p as PSPROGRESSBAR_PAINTINFO ptr )

' Observe messages. Return TRUE if you handled it and want the control's default handling
' suppressed, FALSE to let it proceed.
'
' EVERY MESSAGE IS OFFERED AND EVERY ANSWER IS HONOURED, WITH EXACTLY ONE EXCEPTION:
' WM_DESTROY and WM_NCDESTROY are never offered at all. They free the instance data and the
' tooltip window, and a callback that suppressed either would leak both.
'
' Otherwise there are no exemptions. The siblings that take mouse capture have to ignore your
' answer for the up-messages, or a callback could strand the capture; this control takes no
' capture, has no press gesture and no focus, so everything it does offer you is genuinely
' suppressible. That also makes this the hook for anything interactive a host wants to add --
' click-to-cancel being the obvious one -- since the control itself never acts on a click.
type PRG_MessageCallbackFunc as function( byval m as PSPROGRESSBAR_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text on demand (only when a tip is about to show). Consulted ONLY when
' the control has no tooltip text of its own. Return "" for no tooltip.
type PRG_TooltipCallbackFunc as function( byval hProgressBar as HWND ) as DWSTRING

' Supply the text drawn across the bar. Consulted only in PRG_TEXT_CALLBACK mode, and called
' on every repaint -- so keep it cheap and do not allocate a window in it.
type PRG_TextCallbackFunc as function( byval hProgressBar as HWND, _
                                       byval nPos as long, _
                                       byval nMin as long, _
                                       byval nMax as long ) as DWSTRING


type PSPROGRESSBAR
    hWin            as HWND
    idc_ProgressBar as long = 0
    ' The tooltip, whichever backend it is on. Replaces the old hToolTip field;
    ' PsProgressBar_GetTooltipHandle still answers the comctl32 handle, and 0 while this
    ' instance is on PsTooltip.
    tip             as PSTIPHOST
    ' --- Model. Setters clamp, never wrap. ALL OF THEM ARE SILENT: a progress bar's position
    '     is only ever set programmatically, so by the family rule ("programmatic setters are
    '     silent; only user interaction notifies") there is no change callback at all -- not
    '     one that is unused, one that does not exist. ---
    nMin            as long = 0
    nMax            as long = 100
    nPos            as long = 0
    nStep           as long = 10
    ' --- State ---
    isEnabled       as boolean = true
    isReverse       as boolean = false
    isSegmented     as boolean = false
    nOrientation    as long = PRG_HORIZONTAL   ' immutable after Create
    nMode           as long = PRG_MODE_DETERMINATE
    nState          as long = PRG_STATE_NORMAL
    nFillStyle      as long = PRG_FILL_SOLID
    nGradientMode   as long = LinearGradientModeVertical
    nTextMode       as long = PRG_TEXT_NONE
    hFont           as HFONT                   ' BORROWED. The host owns it; we never delete it.
    ' --- Marquee. nTick is the animation clock and the ONLY thing WM_TIMER advances. ---
    nMarqueeStyle   as long = PRG_MARQUEE_BOUNCE
    nMarqueeMs      as long = PSPROGRESSBAR_DEFAULT_MARQUEE_MS
    nMarqueeStep    as long = PSPROGRESSBAR_DEFAULT_MARQUEE_STEP   ' DPI-scaled at Create
    nChunkPercent   as long = PSPROGRESSBAR_DEFAULT_CHUNK_PCT
    nTick           as long = 0
    marqueeTimerOn  as boolean = false
    ' Tracked from WM_SHOWWINDOW so a hidden bar stops animating. DEFAULTS TO false -- i.e.
    ' "assume shown" -- deliberately: IsWindowVisible walks the ancestor chain, so a control
    ' created on a not-yet-shown parent reports hidden, and gating the START of the timer on
    ' that would leave an indeterminate bar frozen forever (the trap PsScrollPanel hit).
    bHidden         as boolean = false
    ' --- Layout. Rects are DERIVED, never set from outside; LayoutProgressBar() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs it.
    '     It takes NO DC: nothing about this control's geometry is measured. The text is
    '     centred by DrawText at paint time and never sizes anything. ---
    nThickness      as long = PSPROGRESSBAR_DEFAULT_THICKNESS     ' DPI-scaled at Create
    nBarInset       as long = PSPROGRESSBAR_DEFAULT_BARINSET      ' DPI-scaled at Create
    nBlockLength    as long = PSPROGRESSBAR_DEFAULT_BLOCKLEN      ' DPI-scaled at Create
    nBlockGap       as long = PSPROGRESSBAR_DEFAULT_BLOCKGAP      ' DPI-scaled at Create
    nBorderThick    as long = PSPROGRESSBAR_DEFAULT_BORDERTHICK   ' NOT DPI-scaled
    nCurvature      as long = PSPROGRESSBAR_DEFAULT_CURVATURE     ' NOT DPI-scaled
    rcTrack         as RECT              ' derived
    rcFill          as RECT              ' derived
    rcBar           as RECT              ' derived; depends on pos OR on the marquee tick
    bLayoutDirty    as boolean = true
    colors          as PSPROGRESSBAR_COLORS
    wszTooltip      as DWSTRING          ' the host's authored tip text
    wszTipBuf       as DWSTRING          ' instance-lifetime buffer TTN_GETDISPINFOW points at
    PaintCallback   as PRG_PaintCallbackSub       ' optional; replaces the built-in painter
    MessageCallback as PRG_MessageCallbackFunc    ' optional
    TooltipCallback as PRG_TooltipCallbackFunc    ' optional
    TextCallback    as PRG_TextCallbackFunc       ' optional; PRG_TEXT_CALLBACK only

    declare sub      LayoutProgressBar()
    declare function ResolvedCurvature() as long
    declare function FillLength() as long
    declare sub      Refresh()
end type


' ========================================================================================
' PURE FUNCTIONS
'
' These take no window, touch no state and run no timer. That is the point: the motion law
' and the fill arithmetic are the two things most likely to be subtly wrong, and this shape
' lets the self-test drive them through a full period without a message pump. PsScrollPanel's
' ShouldShow established the idea; these are the same move.
' ========================================================================================

' Bar length for a determinate position. Clamps nPos into the range, and answers 0 for an
' empty or inverted range rather than dividing by zero.
declare function PsProgressBar_ComputeBarLength( byval nPos as long, _
                                                 byval nMin as long, _
                                                 byval nMax as long, _
                                                 byval nTrackLen as long ) as long

' Leading edge of the indeterminate chunk at tick nTick, as an offset from the start of the
' track. Always within [0, nTrackLen - nChunkLen], for both styles.
'
'   BOUNCE  ping-pongs: the travel is one full sweep out and one back, so the period is
'           2 * span rather than span, and the offset is a triangle wave.
'   WRAP    slides one way and re-enters, so the period is span and the offset is a sawtooth.
'
' Pure -- no window, no timer, no state. Feed it a tick and it answers where the chunk is.
declare function PsProgressBar_ComputeMarqueeOffset( byval nTrackLen as long, _
                                                     byval nChunkLen as long, _
                                                     byval nStep as long, _
                                                     byval nTick as long, _
                                                     byval nStyle as long ) as long

' How many whole blocks of nBlockLen, separated by nBlockGap, fit inside nLength.
'
' ONE FUNCTION, TWO USES, deliberately: pass the fill length to learn how many slots the
' track has, or pass the bar length to learn how many are lit. That they cannot disagree is
' the reason it is one function.
declare function PsProgressBar_ComputeBlockCount( byval nLength as long, _
                                                  byval nBlockLen as long, _
                                                  byval nBlockGap as long ) as long


' ========================================================================================
' The stadium sentinel resolved to a real number. -1 means "as round as this track can be",
' which is its cross-axis extent -- BuildRoundPath clamps that to half, making both ends exact
' semicircles. It has to be resolved at PAINT time rather than stored, because the track
' thickness follows the client and a stored value would go stale on the first resize.
' ========================================================================================
function PSPROGRESSBAR.ResolvedCurvature() as long
    if this.nCurvature >= 0 then return this.nCurvature
    dim as long cross
    if this.nOrientation = PRG_VERTICAL then
        cross = this.rcTrack.right - this.rcTrack.left
    else
        cross = this.rcTrack.bottom - this.rcTrack.top
    end if
    if cross < 0 then cross = 0
    return cross
end function

' The distance a 100% bar covers: rcFill's extent along the GROWTH axis. One accessor so the
' orientation test lives in exactly one place instead of being repeated at every use.
function PSPROGRESSBAR.FillLength() as long
    dim as long n
    if this.nOrientation = PRG_VERTICAL then
        n = this.rcFill.bottom - this.rcFill.top
    else
        n = this.rcFill.right - this.rcFill.left
    end if
    if n < 0 then n = 0
    return n
end function


' ========================================================================================
' Assign the three rects. This is the only place progress-bar geometry is produced; painting,
' invalidation and every public rect query consume it.
'
'   rcTrack   spans the client along the GROWTH axis, and is nThickness deep across it,
'             CENTRED on that axis. nThickness = 0 (the default) means "fill the client",
'             so a bar sized by its host simply fills what it was given.
'   rcFill    rcTrack deflated by nBarInset on all four sides: where a 100% bar reaches.
'   rcBar     the drawn portion.
'               determinate   -> anchored to rcFill's START edge, length from the position.
'               indeterminate -> a chunk of nChunkPercent of the fill, floating at the offset
'                                the marquee law gives for the current tick. NOT anchored to
'                                either end.
'
' REVERSE mirrors the growth direction on whichever axis is in use: a horizontal bar grows
' right-to-left, a vertical one top-down. A VERTICAL BAR GROWS BOTTOM-UP BY DEFAULT, which is
' what everyone expects of a vertical gauge and is therefore the un-reversed case.
'
' OVERFLOW is handled by computing HONESTLY and letting the clip happen (PsIconPanel's and
' PsTabBar's rule): a client thinner than nThickness gives a track that hangs over the edges
' rather than one squeezed into a different shape.
'
' NO DC IS TAKEN. Nothing here is measured -- the optional text is centred by DrawText at
' paint time and never sizes anything. LayoutToggle is the only other one in the family that
' can say that.
' ========================================================================================
sub PSPROGRESSBAR.LayoutProgressBar()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top

    ' No geometry yet (created 0x0, sized only once the host places us). Placement is
    ' impossible, so leave the rects alone and ask to be run again.
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    ' ---- rcTrack: full length along the growth axis, nThickness across it, centred --------
    if this.nOrientation = PRG_VERTICAL then
        dim as long tw = this.nThickness
        if (tw <= 0) orelse (tw > clientW) then tw = clientW
        dim as long x = rcClient.left + ((clientW - tw) \ 2)
        SetRect( @this.rcTrack, x, rcClient.top, x + tw, rcClient.bottom )
    else
        dim as long th = this.nThickness
        if (th <= 0) orelse (th > clientH) then th = clientH
        dim as long y = rcClient.top + ((clientH - th) \ 2)
        SetRect( @this.rcTrack, rcClient.left, y, rcClient.right, y + th )
    end if

    ' ---- rcFill: the track, inset ---------------------------------------------------------
    this.rcFill = this.rcTrack
    if this.nBarInset > 0 then
        InflateRect( @this.rcFill, -this.nBarInset, -this.nBarInset )
        ' A generous inset on a thin control can invert the rect. Collapse it to empty rather
        ' than letting a negative extent reach the painter.
        if this.rcFill.right  < this.rcFill.left then this.rcFill.right  = this.rcFill.left
        if this.rcFill.bottom < this.rcFill.top  then this.rcFill.bottom = this.rcFill.top
    end if

    ' ---- rcBar: how much of rcFill is drawn, and where -----------------------------------
    dim as long fillLen = this.FillLength()
    dim as long barStart = 0            ' offset from rcFill's START edge, in growth units
    dim as long barLen   = 0

    if this.nMode = PRG_MODE_INDETERMINATE then
        dim as long chunk = (fillLen * this.nChunkPercent) \ 100
        if chunk < 1 then chunk = 1
        if chunk > fillLen then chunk = fillLen
        barLen   = chunk
        barStart = PsProgressBar_ComputeMarqueeOffset( fillLen, chunk, this.nMarqueeStep, _
                                                       this.nTick, this.nMarqueeStyle )
    else
        barLen   = PsProgressBar_ComputeBarLength( this.nPos, this.nMin, this.nMax, fillLen )
        barStart = 0
    end if

    ' Project the 1-D (start, length) pair onto the right axis and direction. This is the only
    ' place the four orientation-x-reverse cases exist; everything downstream reads rcBar.
    this.rcBar = this.rcFill
    if this.nOrientation = PRG_VERTICAL then
        if this.isReverse then
            ' top-down
            this.rcBar.top    = this.rcFill.top + barStart
            this.rcBar.bottom = this.rcBar.top + barLen
        else
            ' bottom-up (the default for a vertical gauge)
            this.rcBar.bottom = this.rcFill.bottom - barStart
            this.rcBar.top    = this.rcBar.bottom - barLen
        end if
    else
        if this.isReverse then
            ' right-to-left
            this.rcBar.right = this.rcFill.right - barStart
            this.rcBar.left  = this.rcBar.right - barLen
        else
            this.rcBar.left  = this.rcFill.left + barStart
            this.rcBar.right = this.rcBar.left + barLen
        end if
    end if

    ' CLIP THE BAR TO THE FILL AREA. Required, not defensive: PRG_MARQUEE_WRAP deliberately
    ' returns offsets from -nChunkLen upward so the chunk genuinely slides in from one edge
    ' and out of the other, which means rcBar legitimately overhangs on both sides during a
    ' wrap cycle. Everything downstream -- the painter, the block tiling, the text clip, the
    ' probes -- reads rcBar and assumes it is inside rcFill, so the clip belongs here, once,
    ' rather than at each of them.
    if this.rcBar.left   < this.rcFill.left   then this.rcBar.left   = this.rcFill.left
    if this.rcBar.top    < this.rcFill.top    then this.rcBar.top    = this.rcFill.top
    if this.rcBar.right  > this.rcFill.right  then this.rcBar.right  = this.rcFill.right
    if this.rcBar.bottom > this.rcFill.bottom then this.rcBar.bottom = this.rcFill.bottom
    ' A fully-off-track chunk collapses to empty rather than inverting.
    if this.rcBar.right  < this.rcBar.left then this.rcBar.right  = this.rcBar.left
    if this.rcBar.bottom < this.rcBar.top  then this.rcBar.bottom = this.rcBar.top
end sub


' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy.
sub PSPROGRESSBAR.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase so a region vacated by a shrinking bar is cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A PROGRESS BAR: a track with a fill that grows along it, or -- in indeterminate mode -- a
' chunk that floats back and forth inside it because there is nothing to measure.
'
' IT IS DISPLAY-ONLY, AND THAT DECIDES MOST OF THE REST
'   No focus (no WS_TABSTOP, and dwExStyle is passed explicitly as 0). No mouse capture. No
'   CS_DBLCLKS. No keyboard. No hot state, and therefore -- alone among the recent siblings --
'   no hot-tracking poll timer. Mouse messages are offered to the message callback and
'   otherwise ignored, so a host that wants click-to-cancel implements it there.
'
' IT ADDS NO PUMP OBLIGATION
'   There is no PsProgressBar_FilterMessage. Nothing here needs to see the host's message
'   loop: the tooltip is TTF_SUBCLASS (it hooks the control itself) and the marquee runs on a
'   WM_TIMER. Drop the control into any host and it works. PsButton and PsCheckBox are the
'   other two siblings that can say this.
'
' ORIENTATION IS FIXED AT CREATE
'   PsSplitter's rule. A vertical bar fills BOTTOM-UP by default, which PsProgressBar_SetReverse
'   inverts (as it inverts a horizontal bar to right-to-left).
'
' TEXT, AND WHY THERE ARE TWO TEXT COLOURS
'   The optional caption is drawn ONCE, in the same place, but in TWO CLIPPED PASSES: the part
'   of it lying over the filled bar takes TextColorOnBar and the part over the empty track
'   takes TextColor. A single-colour label washes out as the fill crosses under it; this does
'   not. The font is the HOST'S -- SetFont borrows the handle and the control never creates or
'   destroys one.
'
' THE POSITION JUMPS, IT DOES NOT ANIMATE
'   SetPos moves the bar immediately. comctl32 v6 eases instead, and that easing is also the
'   thing hosts most often try to switch off. The consequence worth knowing: the timer here
'   exists ONLY for indeterminate mode, so a determinate bar burns no CPU at all.
'
' THE CONTROL HANDLE
'   Every PsProgressBar_* function takes the handle returned by PsProgressBar_Create().
'   It is a real HWND on purpose (not an opaque type): callers legitimately need to treat the
'   control as a window, e.g. SetWindowPos() to place and size it.
'
' LIFETIME
'   The control frees itself when its window is destroyed. It owns its tooltip window and
'   destroys that too. It does NOT own the font.
'
' THERE IS NO PsProgressBar_HitTest
'   The whole client is the same inert surface. Ask GetTrackRect / GetFillRect / GetBarRect
'   if you need the geometry.
' ========================================================================================

declare function PsProgressBar_Create( byval hWndParent as HWND, _
                                       byval CtrlID as long, _
                                       byval nOrientation as long = PRG_HORIZONTAL ) as HWND
declare sub      PsProgressBar_Refresh( byval hProgressBar as HWND )
declare function PsProgressBar_GetOrientation( byval hProgressBar as HWND ) as long

' --- Model. Every one of these is SILENT. ---
declare sub      PsProgressBar_GetRange( byval hProgressBar as HWND, byref nMin as long, byref nMax as long )
declare sub      PsProgressBar_SetRange( byval hProgressBar as HWND, byval nMin as long, byval nMax as long )
declare function PsProgressBar_GetPos( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetPos( byval hProgressBar as HWND, byval nPos as long )
declare function PsProgressBar_GetStep( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetStep( byval hProgressBar as HWND, byval nStep as long )
' Advance by nStep and answer the new position. comctl32's PBM_STEPIT, except that it CLAMPS
' at nMax where PBM_STEPIT wraps around to nMin -- a wrap makes a finished job look restarted.
declare function PsProgressBar_StepIt( byval hProgressBar as HWND ) as long
declare function PsProgressBar_DeltaPos( byval hProgressBar as HWND, byval nDelta as long ) as long
declare function PsProgressBar_GetPercent( byval hProgressBar as HWND ) as long

' --- Mode / state ---
declare function PsProgressBar_GetMode( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetMode( byval hProgressBar as HWND, byval nMode as long )
declare function PsProgressBar_GetState( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetState( byval hProgressBar as HWND, byval nState as long )
declare function PsProgressBar_GetEnabled( byval hProgressBar as HWND ) as boolean
declare sub      PsProgressBar_SetEnabled( byval hProgressBar as HWND, byval isEnabled as boolean )

' --- Marquee (indeterminate mode only; setting them in determinate mode is legal and inert) ---
declare function PsProgressBar_GetMarqueeStyle( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetMarqueeStyle( byval hProgressBar as HWND, byval nStyle as long )
declare sub      PsProgressBar_GetMarqueeSpeed( byval hProgressBar as HWND, byref nIntervalMs as long, byref nStepPx as long )
declare sub      PsProgressBar_SetMarqueeSpeed( byval hProgressBar as HWND, byval nIntervalMs as long, byval nStepPx as long )
declare function PsProgressBar_GetMarqueeChunkPercent( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetMarqueeChunkPercent( byval hProgressBar as HWND, byval nPercent as long )

' --- Fill style and segmentation. ORTHOGONAL: all four combinations are legal. ---
declare function PsProgressBar_GetFillStyle( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetFillStyle( byval hProgressBar as HWND, byval nFillStyle as long )
declare function PsProgressBar_GetGradientMode( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetGradientMode( byval hProgressBar as HWND, byval nMode as long )
declare function PsProgressBar_GetSegmented( byval hProgressBar as HWND ) as boolean
declare sub      PsProgressBar_SetSegmented( byval hProgressBar as HWND, byval isSegmented as boolean )
declare sub      PsProgressBar_GetBlockMetrics( byval hProgressBar as HWND, byref nBlockLen as long, byref nBlockGap as long )
declare sub      PsProgressBar_SetBlockMetrics( byval hProgressBar as HWND, byval nBlockLen as long, byval nBlockGap as long )
' How many blocks are CURRENTLY DRAWN, and where each one is. Not "how many slots the track
' has" -- in indeterminate mode the blocks tile the floating chunk, not the track.
declare function PsProgressBar_GetBlockCount( byval hProgressBar as HWND ) as long
declare function PsProgressBar_GetBlockRect( byval hProgressBar as HWND, byval idx as long, byref rc as RECT ) as boolean

' --- Geometry / chrome ---
declare function PsProgressBar_GetThickness( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetThickness( byval hProgressBar as HWND, byval nThickness as long )
declare function PsProgressBar_GetBarInset( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetBarInset( byval hProgressBar as HWND, byval nBarInset as long )
declare function PsProgressBar_GetBorderThickness( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetBorderThickness( byval hProgressBar as HWND, byval nThickness as long )
declare function PsProgressBar_GetCurvature( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetCurvature( byval hProgressBar as HWND, byval nCurvature as long )
declare function PsProgressBar_GetReverse( byval hProgressBar as HWND ) as boolean
declare sub      PsProgressBar_SetReverse( byval hProgressBar as HWND, byval isReverse as boolean )
declare function PsProgressBar_GetTrackRect( byval hProgressBar as HWND, byref rc as RECT ) as boolean
declare function PsProgressBar_GetFillRect( byval hProgressBar as HWND, byref rc as RECT ) as boolean
declare function PsProgressBar_GetBarRect( byval hProgressBar as HWND, byref rc as RECT ) as boolean

' --- Text ---
declare function PsProgressBar_GetTextMode( byval hProgressBar as HWND ) as long
declare sub      PsProgressBar_SetTextMode( byval hProgressBar as HWND, byval nTextMode as long )
declare function PsProgressBar_GetFont( byval hProgressBar as HWND ) as HFONT
declare sub      PsProgressBar_SetFont( byval hProgressBar as HWND, byval hFont as HFONT )
' Exactly what would be drawn right now, mode and callback already applied. Assertable
' without rendering anything.
declare function PsProgressBar_GetDisplayText( byval hProgressBar as HWND ) as DWSTRING

' --- Colors ---
declare sub      PsProgressBar_GetColors( byval hProgressBar as HWND, byval pColors as PSPROGRESSBAR_COLORS ptr )
declare sub      PsProgressBar_SetColors( byval hProgressBar as HWND, byval pColors as PSPROGRESSBAR_COLORS ptr )
' The state x enabled precedence, resolved. Public because it is a pure decision the host may
' need to mirror, and because that makes the precedence table assertable.
declare sub      PsProgressBar_ResolveBarColors( byval hProgressBar as HWND, byref clr1 as COLORREF, byref clr2 as COLORREF )

' --- Tooltips ---
declare function PsProgressBar_GetTooltipText( byval hProgressBar as HWND ) as DWSTRING
declare sub      PsProgressBar_SetTooltipText( byval hProgressBar as HWND, byval Text as DWSTRING )
declare function PsProgressBar_GetTooltipHandle( byval hProgressBar as HWND ) as HWND
declare function PsProgressBar_SetTooltipMode( byval hProgressBar as HWND, byval nMode as long ) as boolean
declare function PsProgressBar_GetTooltipMode( byval hProgressBar as HWND ) as long
' The PsTooltip window, or 0 on the system backend. The door to PsTooltip's own
' SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph -- deliberately not mirrored here.
declare function PsProgressBar_GetPsTooltipHandle( byval hProgressBar as HWND ) as HWND
'
' THIS CONTROL HAD NO DELAY SETTERS AT ALL. It is display-only and nobody had asked, so its
' tips ran at whatever comctl32 derived and there was no way to say otherwise. All three are
' honoured by BOTH backends; one never set keeps the backend's own system-derived value.
declare sub      PsProgressBar_SetHoverTime( byval hProgressBar as HWND, byval milliseconds as long )
declare sub      PsProgressBar_SetAutoPopTime( byval hProgressBar as HWND, byval milliseconds as long )
declare sub      PsProgressBar_SetReshowTime( byval hProgressBar as HWND, byval milliseconds as long )

' --- Callbacks ---
declare sub      PsProgressBar_SetPaintCallback( byval hProgressBar as HWND, byval usersub as PRG_PaintCallbackSub )
declare sub      PsProgressBar_SetMessageCallback( byval hProgressBar as HWND, byval userfunc as PRG_MessageCallbackFunc )
declare sub      PsProgressBar_SetTooltipCallback( byval hProgressBar as HWND, byval userfunc as PRG_TooltipCallbackFunc )
declare sub      PsProgressBar_SetTextCallback( byval hProgressBar as HWND, byval userfunc as PRG_TextCallbackFunc )

' --- Probes and self-test ---
'
'   CountRenderedTones - render the control OFFSCREEN with its current painter and return how
'                        many distinct colours land in nPart's rect. PUBLIC on purpose: a host
'                        that supplies its own PaintCallback needs a way to assert it has not
'                        flooded the control, and every occurrence of that bug in this family
'                        so far has been in a host callback rather than in a control.
'                        A flooded part scores 1.
'
'   HashRenderedPart   - the same offscreen render, returning an FNV-1a hash of nPart's pixels.
'                        For asserting that a state change actually REACHED the pixels. Read
'                        the caveat in PsCheckBox's notes before trusting one of these: an
'                        assertion of this shape can pass for a reason other than the one it
'                        is named for, if more than one variable moves the pixels at once.
'
'   RunSelfTest        - geometry, the pure functions, block tiling, colour precedence, text
'                        modes and render assertions, gated on the PSPROGRESSBAR_SELFTEST env
'                        var. Call it from the host after the control exists.
declare function PsProgressBar_CountRenderedTones( byval hProgressBar as HWND, byval nPart as long ) as long
declare function PsProgressBar_HashRenderedPart( byval hProgressBar as HWND, byval nPart as long ) as ulong
declare sub      PsProgressBar_RunSelfTest( byval hWndParent as HWND )
