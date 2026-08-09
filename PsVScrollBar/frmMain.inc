'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"

' ----------------------------------------------------------------------------------------
' Harness model: each instance scrolls a "ruler" of numbered units painted into a
' viewport directly left of the bar. One unit = HARNESS_UNIT_H pixels; page = however
' many units fit in the viewport. This mirrors how hosts drive the control (line
' units in tiko's editor), without needing an editor.
' ----------------------------------------------------------------------------------------
#define HARNESS_UNIT_H   12

dim shared as HWND HWND_VSCROLL(1)
dim shared as RECT gViewport(1)              ' content rects, derived in frmMain_Layout
dim shared as integer gTotalUnits(1) = { 400, 30 }   ' B is small: exercises auto-hide

' Self-test support: a paint callback that captures the thumb rect the control
' actually computed, so geometry is asserted from the REAL WM_PAINT path.
dim shared as RECT gCapturedThumb
dim shared as boolean gCaptureValid = false
dim shared as integer gFailCount = 0

' Self-test support for the wheel section: captures whether the scroll callback fired and
' what position it reported, so "wheel scrolling notifies the owner" is asserted rather
' than assumed.
dim shared as boolean gWheelCbFired = false
dim shared as integer gWheelCbPos = -1


' ========================================================================================
' Position the scrollbars beside their viewports and push the derived range into them.
' The bars' geometry is entirely derived here: viewport height -> page size; the
' position is preserved (SetRange clamps it if the new range no longer allows it).
' ========================================================================================
sub frmMain_Layout( byval hwnd as HWND )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit sub

    dim as RECT rc : GetClientRect( hwnd, @rc )
    dim as integer nMargin   = pWindow->ScaleX(20)
    dim as integer labelH    = pWindow->ScaleY(20)
    dim as integer barW      = pWindow->ScaleX(CVSCROLL_DEFAULT_WIDTH)
    dim as integer yTop      = pWindow->ScaleY(16) + labelH
    dim as integer yBottom   = rc.bottom - pWindow->ScaleY(16)
    if yBottom < yTop then yBottom = yTop
    ' Two sections side by side, a viewport + bar each
    dim as integer sectionW  = (rc.right - rc.left - 3*nMargin) \ 2

    for i as integer = 0 to 1
        dim as integer x1 = rc.left + nMargin + i * (sectionW + nMargin)
        dim as integer x2 = x1 + sectionW - barW
        if x2 < x1 then x2 = x1
        SetRect( @gViewport(i), x1, yTop, x2, yBottom )

        SetWindowPos( HWND_VSCROLL(i), 0, _
            x2, yTop, barW, yBottom - yTop, SWP_NOZORDER )

        dim as integer nPage = (yBottom - yTop) \ HARNESS_UNIT_H
        PsVScrollBar_SetRange( HWND_VSCROLL(i), gTotalUnits(i), nPage, _
                              PsVScrollBar_GetPos( HWND_VSCROLL(i) ) )

        ' Auto-hide is the OWNER's decision, made from the range (never from the
        ' bar's own geometry -- see the deadlock note in PsVScrollBar.bi).
        dim as boolean needed = PsVScrollBar_NeedsThumb( HWND_VSCROLL(i) )
        if needed then
            ShowWindow( HWND_VSCROLL(i), SW_SHOWNA )
        else
            ShowWindow( HWND_VSCROLL(i), SW_HIDE )
        end if
    next

    InvalidateRect( hwnd, NULL, FALSE )
end sub


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColorPanel )
    b.PaintClientRect()

    for i as integer = 0 to 1
        dim as RECT rcView = gViewport(i)

        ' Section label + live position readout
        dim as RECT rcLabel = rcView
        rcLabel.bottom = rcView.top
        rcLabel.top    = rcView.top - iif( pWindow <> 0, pWindow->ScaleY(20), 20 )
        dim as string sLabel = "Instance " & chr(65 + i) & ":  total=" & _
            str(gTotalUnits(i)) & "   pos=" & str(PsVScrollBar_GetPos(HWND_VSCROLL(i)))
        if PsVScrollBar_NeedsThumb( HWND_VSCROLL(i) ) = false then
            sLabel &= "   (fits: bar hidden)"
        end if
        b.SetFont( ghFont(GUIFONT_10) )
        b.SetForeColor( theme.ForeColorLabel )
        b.SetBackColor( theme.BackColorPanel )
        b.PaintText( sLabel, @rcLabel, DT_LEFT )

        ' Viewport background
        b.SetBackColor( theme.BackColorBox )
        b.PaintRect( @rcView )

        ' Ruler: a tick per unit, numbered every 10 units, scrolled by the bar's pos
        dim as integer nPos = PsVScrollBar_GetPos( HWND_VSCROLL(i) )
        dim as integer u = nPos
        do
            dim as integer y = rcView.top + (u - nPos) * HARNESS_UNIT_H
            if (y >= rcView.bottom) orelse (u > gTotalUnits(i)) then exit do

            dim as RECT rcTick
            dim as integer tickW = iif( (u mod 10) = 0, 24, iif( (u mod 5) = 0, 14, 7 ) )
            SetRect( @rcTick, rcView.left, y, rcView.left + tickW, y + 1 )
            b.SetBackColor( theme.ForeColorRuler )
            b.PaintRect( @rcTick )

            if ((u mod 10) = 0) andalso (y + 16 < rcView.bottom) then
                dim as RECT rcNum
                SetRect( @rcNum, rcView.left + 30, y - 2, rcView.left + 80, y + 14 )
                b.SetFont( ghFont(GUIFONT_9) )
                b.SetForeColor( theme.ForeColor )
                b.SetBackColor( theme.BackColorBox )
                b.PaintText( str(u), @rcNum, DT_LEFT )
            end if
            u += 1
        loop
    next

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Scroll callback: the user scrolled (thumb drag or track paging). Repaint the
' matching viewport and trace the event.
' ========================================================================================
sub VScroll_ScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    dim as integer idx = iif( hScrollBar = HWND_VSCROLL(0), 0, 1 )
    InvalidateRect( HWND_FRMMAIN, NULL, FALSE )
    dim as string s = "scroll  instance " & chr(65 + idx) & "  pos=" & str(newPos)
    print s
end sub


' ========================================================================================
' Custom paint callback (instance B only): inset accent thumb on the theme track.
' Instance A intentionally has NO paint callback so the default paint path stays covered.
' ========================================================================================
sub VScroll_PaintCallback( byval p as PSVSCROLLBAR_PAINTINFO ptr )
    p->b->SetBackColor( theme.BackColorBox )
    p->b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        dim as RECT rcT = p->rcThumb
        rcT.left += 2
        rcT.right -= 2
        dim as COLORREF clr = theme.FocusAccent
        if p->isHot orelse p->isDragging then clr = theme.FocusAccentHot
        p->b->SetBackColor( clr )
        p->b->PaintRect( @rcT )
    end if
end sub


' ========================================================================================
' Geometry self-test (run with PSVSCROLL_SELFTEST=1). Asserts the thumb rect the
' control computes for known ranges, captured from the real WM_PAINT path.
' ========================================================================================
sub SelfTest_CapturePaint( byval p as PSVSCROLLBAR_PAINTINFO ptr )
    gCapturedThumb = p->rcThumb
    gCaptureValid = true
    p->b->SetBackColor( theme.BackColorBox )
    p->b->PaintClientRect()
end sub

' Scroll callback used only by the wheel section of the self-test, so that "wheel scrolling
' notifies the owner" is observed rather than assumed.
sub SelfTest_CaptureScroll( byval hScrollBar as HWND, byval newPos as integer )
    gWheelCbFired = true
    gWheelCbPos   = newPos
end sub

' Put the wheel accumulator back to zero before a test block. SetWheelStep only resets it
' when the value actually CHANGES (deliberately -- see PsVScrollBar.inc), so setting the
' same step again would not clear it. Bounce through 0 to force the reset.
sub SelfTest_ResetWheel( byval h as HWND, byval nStep as integer )
    PsVScrollBar_SetWheelStep( h, 0 )
    PsVScrollBar_SetWheelStep( h, nStep )
end sub

sub SelfTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gFailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

' Force a synchronous repaint of the bar and return the thumb rect it computed.
function SelfTest_GrabThumb( byval hSB as HWND ) as boolean
    gCaptureValid = false
    ShowWindow( hSB, SW_SHOWNA )
    AfxRedrawWindow( hSB )
    return gCaptureValid
end function

sub frmMain_RunSelfTest()
    dim as HWND h = HWND_VSCROLL(0)
    print ""
    print "=== PsVScrollBar geometry self-test ==="

    PsVScrollBar_SetPaintCallback( h, @SelfTest_CapturePaint )

    dim as RECT rcBar : GetClientRect( h, @rcBar )
    dim as integer track = rcBar.bottom - rcBar.top
    dim as integer minThumb = CVSCROLL_MIN_THUMB
    dim pWindow as CWindow ptr = AfxCWindowPtr(h)
    if pWindow then minThumb = pWindow->ScaleY( CVSCROLL_MIN_THUMB )
    print "track height = " & str(track) & "   min thumb = " & str(minThumb)

    ' --- pos = 0: thumb must start hard against the top edge ---
    PsVScrollBar_SetRange( h, 1000, 40, 0 )
    if SelfTest_GrabThumb( h ) then
        dim as integer th = gCapturedThumb.bottom - gCapturedThumb.top
        SelfTest_Check( "pos=0 -> thumb.top = 0", (gCapturedThumb.top = 0), _
            "top=" & str(gCapturedThumb.top) & " height=" & str(th) )
        SelfTest_Check( "thumb height in [minThumb, track]", _
            (th >= minThumb) andalso (th <= track), "height=" & str(th) )
        SelfTest_Check( "thumb spans full bar width", _
            (gCapturedThumb.left = rcBar.left) andalso (gCapturedThumb.right = rcBar.right), _
            "left=" & str(gCapturedThumb.left) & " right=" & str(gCapturedThumb.right) )
    else
        SelfTest_Check( "paint captured (pos=0)", false, "WM_PAINT never ran" )
    end if

    ' --- pos = max: thumb must end hard against the bottom edge ---
    PsVScrollBar_SetRange( h, 1000, 40, 960 )
    if SelfTest_GrabThumb( h ) then
        SelfTest_Check( "pos=max -> thumb.bottom = track", (gCapturedThumb.bottom = track), _
            "bottom=" & str(gCapturedThumb.bottom) & " track=" & str(track) )
    else
        SelfTest_Check( "paint captured (pos=max)", false, "WM_PAINT never ran" )
    end if

    ' --- pos = max/2: thumb centered on the free span (within 1px of rounding) ---
    PsVScrollBar_SetRange( h, 1000, 40, 480 )
    if SelfTest_GrabThumb( h ) then
        dim as integer th = gCapturedThumb.bottom - gCapturedThumb.top
        dim as integer span = track - th
        dim as integer expected = span \ 2
        SelfTest_Check( "pos=max/2 -> thumb at mid-span (+/-1)", _
            abs(gCapturedThumb.top - expected) <= 1, _
            "top=" & str(gCapturedThumb.top) & " expected=" & str(expected) )
    else
        SelfTest_Check( "paint captured (pos=mid)", false, "WM_PAINT never ran" )
    end if

    ' --- content fits: no thumb at all, and NeedsThumb says so ---
    PsVScrollBar_SetRange( h, 50, 100, 0 )
    SelfTest_Check( "total<=page -> NeedsThumb = false", _
        (PsVScrollBar_NeedsThumb(h) = false), "total=50 page=100" )
    if SelfTest_GrabThumb( h ) then
        SelfTest_Check( "total<=page -> thumb rect empty", _
            (IsRectEmpty( @gCapturedThumb ) <> 0), _
            "top=" & str(gCapturedThumb.top) & " bottom=" & str(gCapturedThumb.bottom) )
    else
        SelfTest_Check( "paint captured (fits)", false, "WM_PAINT never ran" )
    end if

    ' --- clamping: SetRange and SetPos both clamp to [0, MaxPos] ---
    PsVScrollBar_SetRange( h, 200, 40, 5000 )
    SelfTest_Check( "SetRange clamps pos to MaxPos", (PsVScrollBar_GetPos(h) = 160), _
        "pos=" & str(PsVScrollBar_GetPos(h)) & " expected=160" )
    PsVScrollBar_SetPos( h, -10 )
    SelfTest_Check( "SetPos clamps below to 0", (PsVScrollBar_GetPos(h) = 0), _
        "pos=" & str(PsVScrollBar_GetPos(h)) )
    PsVScrollBar_SetPos( h, 400 )
    SelfTest_Check( "SetPos clamps above to MaxPos", (PsVScrollBar_GetPos(h) = 160), _
        "pos=" & str(PsVScrollBar_GetPos(h)) )

    ' ------------------------------------------------------------------------------------
    ' Wheel / trackpad. Driven through PsVScrollBar_HandleWheelDelta with synthetic deltas,
    ' which is the same code path a real WM_MOUSEWHEEL takes -- so this asserts the actual
    ' arithmetic without needing a trackpad. What a trackpad still has to prove is only
    ' whether the MESSAGE reaches us; the maths below is settled here.
    '
    ' The step is pinned to 3 throughout, so the machine's SPI_GETWHEELSCROLLLINES setting
    ' cannot skew the expected numbers. SetWheelStep also zeroes the accumulator, which is
    ' what makes each block below independent of the one before it.
    ' ------------------------------------------------------------------------------------
    print ""
    print "--- wheel / trackpad ---"
    dim as integer posBefore, moved

    ' 1. DIRECTION. Positive delta = wheel pushed AWAY = scroll UP = pos DECREASES. This is
    '    the opposite of PsHScrollBar, where a positive horizontal delta increases pos, so
    '    it is asserted here in its own right rather than assumed symmetric.
    SelfTest_ResetWheel( h, 3 )
    PsVScrollBar_SetRange( h, 1000, 40, 500 )
    posBefore = PsVScrollBar_GetPos(h)
    PsVScrollBar_HandleWheelDelta( h, 120 )
    SelfTest_Check( "+120 delta scrolls UP (pos decreases by step)", _
        (PsVScrollBar_GetPos(h) = posBefore - 3), _
        "pos " & str(posBefore) & " -> " & str(PsVScrollBar_GetPos(h)) & " expected " & str(posBefore - 3) )

    SelfTest_ResetWheel( h, 3 )
    posBefore = PsVScrollBar_GetPos(h)
    PsVScrollBar_HandleWheelDelta( h, -120 )
    SelfTest_Check( "-120 delta scrolls DOWN (pos increases by step)", _
        (PsVScrollBar_GetPos(h) = posBefore + 3), _
        "pos " & str(posBefore) & " -> " & str(PsVScrollBar_GetPos(h)) & " expected " & str(posBefore + 3) )

    ' 2. Sub-notch accumulation: a precision touchpad's small deltas must move the bar
    '    IMMEDIATELY, not sit dead and then jump. Asserted twice -- the first small delta
    '    moves on its own, and three of them total exactly one notch's distance.
    SelfTest_ResetWheel( h, 3 )
    PsVScrollBar_SetRange( h, 1000, 40, 500 )
    posBefore = PsVScrollBar_GetPos(h)
    PsVScrollBar_HandleWheelDelta( h, -40 )
    SelfTest_Check( "sub-notch delta (-40) moves on its own", _
        (PsVScrollBar_GetPos(h) = posBefore + 1), _
        "pos " & str(posBefore) & " -> " & str(PsVScrollBar_GetPos(h)) )
    PsVScrollBar_HandleWheelDelta( h, -40 )
    PsVScrollBar_HandleWheelDelta( h, -40 )
    SelfTest_Check( "3 x -40 = one notch's distance (3 units)", _
        (PsVScrollBar_GetPos(h) = posBefore + 3), _
        "pos " & str(posBefore) & " -> " & str(PsVScrollBar_GetPos(h)) )

    ' 3. Conservation: uneven deltas must neither leak nor double-count. Five deltas of -50
    '    sum to -250, so at step 3 the bar must move (250 * 3) \ 120 = 6 units down -- and
    '    it gets there via a 1,1,1,2,1 sequence, which is exactly the remainder working.
    SelfTest_ResetWheel( h, 3 )
    PsVScrollBar_SetRange( h, 1000, 40, 100 )
    posBefore = PsVScrollBar_GetPos(h)
    for i as integer = 1 to 5
        PsVScrollBar_HandleWheelDelta( h, -50 )
    next
    SelfTest_Check( "accumulator conserves delta (5 x -50 -> 6 units)", _
        (PsVScrollBar_GetPos(h) = posBefore + 6), _
        "pos " & str(posBefore) & " -> " & str(PsVScrollBar_GetPos(h)) & " expected " & str(posBefore + 6) )

    ' 4. Clamping at the ends, and the return value that says "nothing moved".
    SelfTest_ResetWheel( h, 3 )
    PsVScrollBar_SetRange( h, 1000, 40, 960 )         ' MaxPos
    moved = iif( PsVScrollBar_HandleWheelDelta( h, -120 ), 1, 0 )
    SelfTest_Check( "wheel past the bottom clamps and returns FALSE", _
        (PsVScrollBar_GetPos(h) = 960) andalso (moved = 0), _
        "pos=" & str(PsVScrollBar_GetPos(h)) & " returned=" & str(moved) )
    SelfTest_ResetWheel( h, 3 )
    PsVScrollBar_SetPos( h, 0 )
    moved = iif( PsVScrollBar_HandleWheelDelta( h, 120 ), 1, 0 )
    SelfTest_Check( "wheel past the top clamps and returns FALSE", _
        (PsVScrollBar_GetPos(h) = 0) andalso (moved = 0), _
        "pos=" & str(PsVScrollBar_GetPos(h)) & " returned=" & str(moved) )

    ' 5. The step override actually overrides.
    PsVScrollBar_SetWheelStep( h, 10 )
    PsVScrollBar_SetRange( h, 1000, 40, 100 )
    posBefore = PsVScrollBar_GetPos(h)
    PsVScrollBar_HandleWheelDelta( h, -120 )
    SelfTest_Check( "SetWheelStep(10) -> one notch moves 10 units", _
        (PsVScrollBar_GetPos(h) = posBefore + 10), _
        "pos " & str(posBefore) & " -> " & str(PsVScrollBar_GetPos(h)) )
    SelfTest_Check( "GetWheelStep round-trips", (PsVScrollBar_GetWheelStep(h) = 10), _
        "got " & str(PsVScrollBar_GetWheelStep(h)) )

    ' 6. Re-setting the SAME step must NOT discard the part-accumulated remainder. A host
    '    whose units need converting sets the step from its per-layout range push, which
    '    re-runs constantly -- including mid-swipe. A reset there would throw away every
    '    sub-notch remainder and restore the dead-then-jump behaviour.
    SelfTest_ResetWheel( h, 3 )
    PsVScrollBar_SetRange( h, 1000, 40, 100 )
    PsVScrollBar_HandleWheelDelta( h, -50 )           ' moves 1, leaves -10 in the accumulator
    posBefore = PsVScrollBar_GetPos(h)
    PsVScrollBar_SetWheelStep( h, 3 )                 ' same value: must change nothing
    PsVScrollBar_HandleWheelDelta( h, -30 )           ' -10 + -30 = -40 -> exactly 1 unit
    SelfTest_Check( "re-setting the same step keeps the remainder", _
        (PsVScrollBar_GetPos(h) = posBefore + 1), _
        "pos " & str(posBefore) & " -> " & str(PsVScrollBar_GetPos(h)) & " (0 = remainder was lost)" )

    ' 7. Wheel scrolling NOTIFIES -- it is user interaction, so it must fire the callback
    '    (unlike the programmatic SetPos, which stays silent).
    PsVScrollBar_SetScrollCallback( h, @SelfTest_CaptureScroll )
    SelfTest_ResetWheel( h, 3 )
    PsVScrollBar_SetRange( h, 1000, 40, 100 )
    gWheelCbFired = false
    gWheelCbPos   = -1
    PsVScrollBar_HandleWheelDelta( h, -120 )
    SelfTest_Check( "wheel fires the scroll callback", gWheelCbFired, _
        "fired=" & str(gWheelCbFired) )
    SelfTest_Check( "callback reports the same pos as GetPos", _
        (gWheelCbPos = PsVScrollBar_GetPos(h)), _
        "callback=" & str(gWheelCbPos) & " GetPos=" & str(PsVScrollBar_GetPos(h)) )
    gWheelCbFired = false
    PsVScrollBar_SetPos( h, 200 )
    SelfTest_Check( "programmatic SetPos stays silent", (gWheelCbFired = false), _
        "fired=" & str(gWheelCbFired) )

    PsVScrollBar_SetScrollCallback( h, @VScroll_ScrollCallback )
    PsVScrollBar_SetWheelStep( h, 0 )                 ' back to following the system

    PsVScrollBar_SetPaintCallback( h, 0 )
    print "=== self-test complete: " & str(gFailCount) & " failure(s) ==="
end sub


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    '  Message cracker macro expects a true to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window. This is just one of those Windows
    '  inconsistencies.
    return true
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow(hwnd)
    function = 0
end function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage(0)
    function = 0
end function


' ========================================================================================
' frmMain Window procedure
' ========================================================================================
function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,        frmMain_OnCreate)
        HANDLE_MSG (hwnd, WM_PAINT,         frmMain_OnPaint)
        HANDLE_MSG (hwnd, WM_CLOSE,         frmMain_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,       frmMain_OnDestroy)

        case WM_NCDESTROY
            ' delete the allocated memory for window
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMMAIN = 0
            return 0

        case WM_ERASEBKGND
            return true      ' prevents painting the background

        case WM_MOUSEWHEEL
            ' Route a wheel or two-finger swipe made over the CONTENT into the bar that
            ' scrolls it. This is what PsVScrollBar_HandleWheelDelta is public FOR: without
            ' it the gesture would only work over the bar itself, a 12px strip, which reads
            ' as the feature being broken. A real host does exactly this.
            '
            ' Two traps in four lines: wheel messages carry SCREEN coordinates in lParam
            ' (unlike every other mouse message), and both those coordinates and the delta
            ' are SIGNED words that loword/hiword hand back unsigned.
            dim as POINT pt
            pt.x = cast(short, loword(lParam))
            pt.y = cast(short, hiword(lParam))
            ScreenToClient( hwnd, @pt )
            for i as long = 0 to 1
                if PtInRect( @gViewport(i), pt ) then
                    PsVScrollBar_HandleWheelDelta( HWND_VSCROLL(i), cast(short, hiword(wParam)) )
                    exit for
                end if
            next
            return 0

        case WM_SIZE
            frmMain_Layout( hwnd )
            return 0

        case WM_KEYDOWN
            ' Programmatic SetPos exercise on instance A. SetPos is SILENT by design
            ' (owner-driven positioning must not echo through the scroll callback),
            ' so the owner repaints its own content here.
            dim as boolean moved = false
            select case wParam
                case VK_HOME
                    PsVScrollBar_SetPos( HWND_VSCROLL(0), 0 ) : moved = true
                case VK_END
                    PsVScrollBar_SetPos( HWND_VSCROLL(0), 99999 ) : moved = true
                case VK_UP
                    PsVScrollBar_SetPos( HWND_VSCROLL(0), PsVScrollBar_GetPos(HWND_VSCROLL(0)) - 3 ) : moved = true
                case VK_DOWN
                    PsVScrollBar_SetPos( HWND_VSCROLL(0), PsVScrollBar_GetPos(HWND_VSCROLL(0)) + 3 ) : moved = true
            end select
            if moved then
                InvalidateRect( hwnd, NULL, FALSE )
                dim as string s = "SetPos (silent)  instance A  pos=" & _
                    str(PsVScrollBar_GetPos(HWND_VSCROLL(0))) & "  <- no scroll callback fired"
                print s
            end if

        case WM_SYSCOMMAND
            if (wParam and &HFFF0) = SC_CLOSE then
                SendMessage( hwnd, WM_CLOSE, wParam, lParam )
                exit function
            end if

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)  = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10) = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 620, 420 _
            )

    ' ---- Instance A: large range, default paint, scroll callback traces ----
    HWND_VSCROLL(0) = PsVScrollBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_VSCROLL1 )
    PsVScrollBar_SetColors( HWND_VSCROLL(0), theme.BackColorScrollBar, _
                           theme.ForeColorScrollBar, theme.ForeColorScrollBarHot )
    PsVScrollBar_SetScrollCallback( HWND_VSCROLL(0), @VScroll_ScrollCallback )

    ' ---- Instance B: small range (auto-hides on a tall window) + custom paint ----
    HWND_VSCROLL(1) = PsVScrollBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_VSCROLL2 )
    PsVScrollBar_SetScrollCallback( HWND_VSCROLL(1), @VScroll_ScrollCallback )
    PsVScrollBar_SetPaintCallback( HWND_VSCROLL(1), @VScroll_PaintCallback )

    frmMain_Layout( HWND_FRMMAIN )

    print "PsVScrollBar test harness"
    print "------------------------"
    print "Instance A: total=" & str(gTotalUnits(0)) & " units, default paint, scroll callback traced"
    print "Instance B: total=" & str(gTotalUnits(1)) & " units, custom paint (accent inset thumb)"
    print ""
    print "Manual checks (things a script cannot drive):"
    print "  Thumb drag   grab the thumb; it must track the cursor with no jump,"
    print "               including with the cursor dragged OUTSIDE the window"
    print "  Track click  click the track: pages toward the click, then auto-repeats"
    print "               while held; stops when the thumb reaches the cursor"
    print "  Hover        thumb brightens under the mouse; whip the mouse out fast"
    print "               -> highlight must clear every time"
    print "  Keyboard     Up/Down = SetPos +/-3, Home/End = ends (all SILENT:"
    print "               no scroll-callback trace may appear for these)"
    print "  Auto-hide    make the window tall until instance B's 30 units fit -> its"
    print "               bar disappears; shorten it -> the bar returns"
    print "  Resize       page size rederives from viewport height; thumb resizes,"
    print "               position is preserved (clamped at the bottom edge)"
    print "  Trackpad     two-finger swipe UP/DOWN over a bar -> it scrolls, smoothly"
    print "               (no dead zone on a slow swipe); the same swipe over a VIEWPORT"
    print "               scrolls that bar too -- the host forwards it. Swiping up must"
    print "               move the content up, i.e. pos DECREASES. Try it with Settings ->"
    print "               Mouse -> 'Scroll inactive windows' OFF: hovering stops working,"
    print "               the forwarded viewport path should still work"
    print "  2 instances  nothing you do in one may disturb the other"
    print ""

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    if environ("PSVSCROLL_SELFTEST") = "1" then
        frmMain_RunSelfTest()
        DestroyWindow( HWND_FRMMAIN )
    end if

    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
            if IsDialogMessage(pWindow->hWindow, @uMsg) = 0 then
                TranslateMessage @uMsg    ' Translates virtual-key messages into character messages.
                DispatchMessage @uMsg     ' Dispatches a message to a window procedure.
            end if
        end if  ' accelerators
    loop  ' message loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

end function
