'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"

' ----------------------------------------------------------------------------------------
' Harness model: each instance scrolls a "ruler" of numbered units painted into a
' viewport directly above the bar. One unit = HARNESS_UNIT_W pixels; page = however
' many units fit in the viewport. This mirrors how tiko will drive the control
' (Scintilla xOffset units), without needing an editor.
' ----------------------------------------------------------------------------------------
#define HARNESS_UNIT_W   12

dim shared as HWND HWND_HSCROLL(1)
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
' Position the scrollbars under their viewports and push the derived range into them.
' The bars' geometry is entirely derived here: viewport width -> page size; the
' position is preserved (SetRange clamps it if the new range no longer allows it).
' ========================================================================================
sub frmMain_Layout( byval hwnd as HWND )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit sub

    dim as RECT rc : GetClientRect( hwnd, @rc )
    dim as integer nMargin     = pWindow->ScaleX(20)
    dim as integer labelH      = pWindow->ScaleY(20)
    dim as integer viewportH   = pWindow->ScaleY(60)
    dim as integer barH        = pWindow->ScaleY(CHSCROLL_DEFAULT_HEIGHT)
    dim as integer sectionGap  = pWindow->ScaleY(30)
    dim as integer yTop        = pWindow->ScaleY(16)

    for i as integer = 0 to 1
        dim as integer x1 = rc.left + nMargin
        dim as integer x2 = rc.right - nMargin
        if x2 < x1 then x2 = x1
        dim as integer yViewport = yTop + labelH
        SetRect( @gViewport(i), x1, yViewport, x2, yViewport + viewportH )

        SetWindowPos( HWND_HSCROLL(i), 0, _
            x1, yViewport + viewportH, x2 - x1, barH, SWP_NOZORDER )

        dim as integer nPage = (x2 - x1) \ HARNESS_UNIT_W
        PsHScrollBar_SetRange( HWND_HSCROLL(i), gTotalUnits(i), nPage, _
                              PsHScrollBar_GetPos( HWND_HSCROLL(i) ) )

        ' Auto-hide is the OWNER's decision, made from the range (never from the
        ' bar's own geometry -- see the deadlock note in PsHScrollBar.bi).
        dim as boolean needed = PsHScrollBar_NeedsThumb( HWND_HSCROLL(i) )
        if needed then
            ShowWindow( HWND_HSCROLL(i), SW_SHOWNA )
        else
            ShowWindow( HWND_HSCROLL(i), SW_HIDE )
        end if

        yTop = yViewport + viewportH + barH + sectionGap
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
            str(gTotalUnits(i)) & "   pos=" & str(PsHScrollBar_GetPos(HWND_HSCROLL(i)))
        if PsHScrollBar_NeedsThumb( HWND_HSCROLL(i) ) = false then
            sLabel &= "   (content fits: bar auto-hidden)"
        end if
        b.SetFont( ghFont(GUIFONT_10) )
        b.SetForeColor( theme.ForeColorLabel )
        b.SetBackColor( theme.BackColorPanel )
        b.PaintText( sLabel, @rcLabel, DT_LEFT )

        ' Viewport background
        b.SetBackColor( theme.BackColorBox )
        b.PaintRect( @rcView )

        ' Ruler: a tick per unit, numbered every 10 units, scrolled by the bar's pos
        dim as integer nPos = PsHScrollBar_GetPos( HWND_HSCROLL(i) )
        dim as integer u = nPos
        do
            dim as integer x = rcView.left + (u - nPos) * HARNESS_UNIT_W
            if (x >= rcView.right) orelse (u > gTotalUnits(i)) then exit do

            dim as RECT rcTick
            dim as integer tickH = iif( (u mod 10) = 0, 24, iif( (u mod 5) = 0, 14, 7 ) )
            SetRect( @rcTick, x, rcView.bottom - tickH, x + 1, rcView.bottom )
            b.SetBackColor( theme.ForeColorRuler )
            b.PaintRect( @rcTick )

            if ((u mod 10) = 0) andalso (x + 30 < rcView.right) then
                dim as RECT rcNum
                SetRect( @rcNum, x + 3, rcView.top + 6, x + 40, rcView.top + 24 )
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
sub HScroll_ScrollCallback( byval hScrollBar as HWND, byval newPos as integer )
    dim as integer idx = iif( hScrollBar = HWND_HSCROLL(0), 0, 1 )
    InvalidateRect( HWND_FRMMAIN, NULL, FALSE )
    dim as string s = "scroll  instance " & chr(65 + idx) & "  pos=" & str(newPos)
    print s
end sub


' ========================================================================================
' Custom paint callback (instance B only): inset accent thumb on the theme track.
' Instance A intentionally has NO paint callback so the default paint path stays covered.
' ========================================================================================
sub HScroll_PaintCallback( byval p as PSHSCROLLBAR_PAINTINFO ptr )
    p->b->SetBackColor( theme.BackColorBox )
    p->b->PaintClientRect()
    if IsRectEmpty( @p->rcThumb ) = 0 then
        dim as RECT rcT = p->rcThumb
        rcT.top += 2
        rcT.bottom -= 2
        dim as COLORREF clr = theme.FocusAccent
        if p->isHot orelse p->isDragging then clr = theme.FocusAccentHot
        p->b->SetBackColor( clr )
        p->b->PaintRect( @rcT )
    end if
end sub


' ========================================================================================
' Geometry self-test (run with PSHSCROLL_SELFTEST=1). Asserts the thumb rect the
' control computes for known ranges, captured from the real WM_PAINT path.
' ========================================================================================
sub SelfTest_CapturePaint( byval p as PSHSCROLLBAR_PAINTINFO ptr )
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
' when the value actually CHANGES (deliberately -- see PsHScrollBar.inc), so setting the
' same step again would not clear it. Bounce through 0 to force the reset.
sub SelfTest_ResetWheel( byval h as HWND, byval nStep as integer )
    PsHScrollBar_SetWheelStep( h, 0 )
    PsHScrollBar_SetWheelStep( h, nStep )
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
    dim as HWND h = HWND_HSCROLL(0)
    print ""
    print "=== PsHScrollBar geometry self-test ==="

    PsHScrollBar_SetPaintCallback( h, @SelfTest_CapturePaint )

    dim as RECT rcBar : GetClientRect( h, @rcBar )
    dim as integer track = rcBar.right - rcBar.left
    dim as integer minThumb = CHSCROLL_MIN_THUMB
    dim pWindow as CWindow ptr = AfxCWindowPtr(h)
    if pWindow then minThumb = pWindow->ScaleX( CHSCROLL_MIN_THUMB )
    print "track width = " & str(track) & "   min thumb = " & str(minThumb)

    ' --- pos = 0: thumb must start hard against the left edge ---
    PsHScrollBar_SetRange( h, 1000, 40, 0 )
    if SelfTest_GrabThumb( h ) then
        dim as integer tw = gCapturedThumb.right - gCapturedThumb.left
        SelfTest_Check( "pos=0 -> thumb.left = 0", (gCapturedThumb.left = 0), _
            "left=" & str(gCapturedThumb.left) & " width=" & str(tw) )
        SelfTest_Check( "thumb width in [minThumb, track]", _
            (tw >= minThumb) andalso (tw <= track), "width=" & str(tw) )
        SelfTest_Check( "thumb spans full bar height", _
            (gCapturedThumb.top = rcBar.top) andalso (gCapturedThumb.bottom = rcBar.bottom), _
            "top=" & str(gCapturedThumb.top) & " bottom=" & str(gCapturedThumb.bottom) )
    else
        SelfTest_Check( "paint captured (pos=0)", false, "WM_PAINT never ran" )
    end if

    ' --- pos = max: thumb must end hard against the right edge ---
    PsHScrollBar_SetRange( h, 1000, 40, 960 )
    if SelfTest_GrabThumb( h ) then
        SelfTest_Check( "pos=max -> thumb.right = track", (gCapturedThumb.right = track), _
            "right=" & str(gCapturedThumb.right) & " track=" & str(track) )
    else
        SelfTest_Check( "paint captured (pos=max)", false, "WM_PAINT never ran" )
    end if

    ' --- pos = max/2: thumb centered on the free span (within 1px of rounding) ---
    PsHScrollBar_SetRange( h, 1000, 40, 480 )
    if SelfTest_GrabThumb( h ) then
        dim as integer tw = gCapturedThumb.right - gCapturedThumb.left
        dim as integer span = track - tw
        dim as integer expected = span \ 2
        SelfTest_Check( "pos=max/2 -> thumb at mid-span (+/-1)", _
            abs(gCapturedThumb.left - expected) <= 1, _
            "left=" & str(gCapturedThumb.left) & " expected=" & str(expected) )
    else
        SelfTest_Check( "paint captured (pos=mid)", false, "WM_PAINT never ran" )
    end if

    ' --- content fits: no thumb at all, and NeedsThumb says so ---
    PsHScrollBar_SetRange( h, 50, 100, 0 )
    SelfTest_Check( "total<=page -> NeedsThumb = false", _
        (PsHScrollBar_NeedsThumb(h) = false), "total=50 page=100" )
    if SelfTest_GrabThumb( h ) then
        SelfTest_Check( "total<=page -> thumb rect empty", _
            (IsRectEmpty( @gCapturedThumb ) <> 0), _
            "left=" & str(gCapturedThumb.left) & " right=" & str(gCapturedThumb.right) )
    else
        SelfTest_Check( "paint captured (fits)", false, "WM_PAINT never ran" )
    end if

    ' --- clamping: SetRange and SetPos both clamp to [0, MaxPos] ---
    PsHScrollBar_SetRange( h, 200, 40, 5000 )
    SelfTest_Check( "SetRange clamps pos to MaxPos", (PsHScrollBar_GetPos(h) = 160), _
        "pos=" & str(PsHScrollBar_GetPos(h)) & " expected=160" )
    PsHScrollBar_SetPos( h, -10 )
    SelfTest_Check( "SetPos clamps below to 0", (PsHScrollBar_GetPos(h) = 0), _
        "pos=" & str(PsHScrollBar_GetPos(h)) )
    PsHScrollBar_SetPos( h, 400 )
    SelfTest_Check( "SetPos clamps above to MaxPos", (PsHScrollBar_GetPos(h) = 160), _
        "pos=" & str(PsHScrollBar_GetPos(h)) )

    ' ------------------------------------------------------------------------------------
    ' Wheel / trackpad. Driven through PsHScrollBar_HandleWheelDelta with synthetic deltas,
    ' which is the same code path a real WM_MOUSEHWHEEL takes -- so this asserts the actual
    ' arithmetic without needing a trackpad. What a trackpad still has to prove is only
    ' whether the MESSAGE reaches us; the maths below is settled here.
    '
    ' The step is pinned to 3 throughout, so the machine's SPI_GETWHEELSCROLLCHARS setting
    ' cannot skew the expected numbers. SetWheelStep also zeroes the accumulator, which is
    ' what makes each block below independent of the one before it.
    ' ------------------------------------------------------------------------------------
    print ""
    print "--- wheel / trackpad ---"
    dim as integer posBefore, moved

    ' 1. DIRECTION -- the trap this control inverts relative to PsVScrollBar.
    SelfTest_ResetWheel( h, 3 )
    PsHScrollBar_SetRange( h, 1000, 40, 500 )
    posBefore = PsHScrollBar_GetPos(h)
    PsHScrollBar_HandleWheelDelta( h, 120 )
    SelfTest_Check( "+120 delta scrolls RIGHT (pos increases by step)", _
        (PsHScrollBar_GetPos(h) = posBefore + 3), _
        "pos " & str(posBefore) & " -> " & str(PsHScrollBar_GetPos(h)) & " expected " & str(posBefore + 3) )

    SelfTest_ResetWheel( h, 3 )
    posBefore = PsHScrollBar_GetPos(h)
    PsHScrollBar_HandleWheelDelta( h, -120 )
    SelfTest_Check( "-120 delta scrolls LEFT (pos decreases by step)", _
        (PsHScrollBar_GetPos(h) = posBefore - 3), _
        "pos " & str(posBefore) & " -> " & str(PsHScrollBar_GetPos(h)) & " expected " & str(posBefore - 3) )

    ' 2. Sub-notch accumulation: a precision touchpad's small deltas must move the bar
    '    IMMEDIATELY, not sit dead and then jump. Asserted twice -- the first small delta
    '    moves on its own, and three of them total exactly one notch's distance.
    SelfTest_ResetWheel( h, 3 )
    PsHScrollBar_SetRange( h, 1000, 40, 500 )
    posBefore = PsHScrollBar_GetPos(h)
    PsHScrollBar_HandleWheelDelta( h, 40 )
    SelfTest_Check( "sub-notch delta (40) moves on its own", _
        (PsHScrollBar_GetPos(h) = posBefore + 1), _
        "pos " & str(posBefore) & " -> " & str(PsHScrollBar_GetPos(h)) )
    PsHScrollBar_HandleWheelDelta( h, 40 )
    PsHScrollBar_HandleWheelDelta( h, 40 )
    SelfTest_Check( "3 x 40 = one notch's distance (3 units)", _
        (PsHScrollBar_GetPos(h) = posBefore + 3), _
        "pos " & str(posBefore) & " -> " & str(PsHScrollBar_GetPos(h)) )

    ' 3. Conservation: uneven deltas must neither leak nor double-count. Five deltas of 50
    '    sum to 250, so at step 3 the bar must move (250 * 3) \ 120 = 6 units -- and it
    '    gets there via a 1,1,1,2,1 sequence, which is exactly the remainder working.
    SelfTest_ResetWheel( h, 3 )
    PsHScrollBar_SetRange( h, 1000, 40, 100 )
    posBefore = PsHScrollBar_GetPos(h)
    for i as integer = 1 to 5
        PsHScrollBar_HandleWheelDelta( h, 50 )
    next
    SelfTest_Check( "accumulator conserves delta (5 x 50 -> 6 units)", _
        (PsHScrollBar_GetPos(h) = posBefore + 6), _
        "pos " & str(posBefore) & " -> " & str(PsHScrollBar_GetPos(h)) & " expected " & str(posBefore + 6) )

    ' 4. Clamping at the ends, and the return value that says "nothing moved".
    SelfTest_ResetWheel( h, 3 )
    PsHScrollBar_SetRange( h, 1000, 40, 960 )         ' MaxPos
    moved = iif( PsHScrollBar_HandleWheelDelta( h, 120 ), 1, 0 )
    SelfTest_Check( "wheel past the right end clamps and returns FALSE", _
        (PsHScrollBar_GetPos(h) = 960) andalso (moved = 0), _
        "pos=" & str(PsHScrollBar_GetPos(h)) & " returned=" & str(moved) )
    SelfTest_ResetWheel( h, 3 )
    PsHScrollBar_SetPos( h, 0 )
    moved = iif( PsHScrollBar_HandleWheelDelta( h, -120 ), 1, 0 )
    SelfTest_Check( "wheel past the left end clamps and returns FALSE", _
        (PsHScrollBar_GetPos(h) = 0) andalso (moved = 0), _
        "pos=" & str(PsHScrollBar_GetPos(h)) & " returned=" & str(moved) )

    ' 5. The step override actually overrides.
    PsHScrollBar_SetWheelStep( h, 10 )
    PsHScrollBar_SetRange( h, 1000, 40, 100 )
    posBefore = PsHScrollBar_GetPos(h)
    PsHScrollBar_HandleWheelDelta( h, 120 )
    SelfTest_Check( "SetWheelStep(10) -> one notch moves 10 units", _
        (PsHScrollBar_GetPos(h) = posBefore + 10), _
        "pos " & str(posBefore) & " -> " & str(PsHScrollBar_GetPos(h)) )
    SelfTest_Check( "GetWheelStep round-trips", (PsHScrollBar_GetWheelStep(h) = 10), _
        "got " & str(PsHScrollBar_GetWheelStep(h)) )

    ' 6. Re-setting the SAME step must NOT discard the part-accumulated remainder. tiko
    '    depends on this: it converts "3 characters" into pixels from its per-layout range
    '    push, which re-runs constantly -- including mid-swipe. A reset there would throw
    '    away every sub-notch remainder and restore the dead-then-jump behaviour.
    SelfTest_ResetWheel( h, 3 )
    PsHScrollBar_SetRange( h, 1000, 40, 100 )
    PsHScrollBar_HandleWheelDelta( h, 50 )            ' moves 1, leaves 10 in the accumulator
    posBefore = PsHScrollBar_GetPos(h)
    PsHScrollBar_SetWheelStep( h, 3 )                 ' same value: must change nothing
    PsHScrollBar_HandleWheelDelta( h, 30 )            ' 10 + 30 = 40 -> exactly 1 unit
    SelfTest_Check( "re-setting the same step keeps the remainder", _
        (PsHScrollBar_GetPos(h) = posBefore + 1), _
        "pos " & str(posBefore) & " -> " & str(PsHScrollBar_GetPos(h)) & " (0 = remainder was lost)" )

    ' 7. Wheel scrolling NOTIFIES -- it is user interaction, so it must fire the callback
    '    (unlike the programmatic SetPos, which stays silent).
    PsHScrollBar_SetScrollCallback( h, @SelfTest_CaptureScroll )
    SelfTest_ResetWheel( h, 3 )
    PsHScrollBar_SetRange( h, 1000, 40, 100 )
    gWheelCbFired = false
    gWheelCbPos   = -1
    PsHScrollBar_HandleWheelDelta( h, 120 )
    SelfTest_Check( "wheel fires the scroll callback", gWheelCbFired, _
        "fired=" & str(gWheelCbFired) )
    SelfTest_Check( "callback reports the same pos as GetPos", _
        (gWheelCbPos = PsHScrollBar_GetPos(h)), _
        "callback=" & str(gWheelCbPos) & " GetPos=" & str(PsHScrollBar_GetPos(h)) )
    gWheelCbFired = false
    PsHScrollBar_SetPos( h, 200 )
    SelfTest_Check( "programmatic SetPos stays silent", (gWheelCbFired = false), _
        "fired=" & str(gWheelCbFired) )

    PsHScrollBar_SetScrollCallback( h, @HScroll_ScrollCallback )
    PsHScrollBar_SetWheelStep( h, 0 )                 ' back to following the system

    PsHScrollBar_SetPaintCallback( h, 0 )
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

        case WM_MOUSEHWHEEL
            ' Route a horizontal swipe made over the CONTENT into the bar that scrolls it.
            ' This is what PsHScrollBar_HandleWheelDelta is public FOR: without it the
            ' gesture would only work over the bar itself, a 12px strip, which reads as
            ' the feature being broken. A real host does exactly this.
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
                    PsHScrollBar_HandleWheelDelta( HWND_HSCROLL(i), cast(short, hiword(wParam)) )
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
                    PsHScrollBar_SetPos( HWND_HSCROLL(0), 0 ) : moved = true
                case VK_END
                    PsHScrollBar_SetPos( HWND_HSCROLL(0), 99999 ) : moved = true
                case VK_LEFT
                    PsHScrollBar_SetPos( HWND_HSCROLL(0), PsHScrollBar_GetPos(HWND_HSCROLL(0)) - 3 ) : moved = true
                case VK_RIGHT
                    PsHScrollBar_SetPos( HWND_HSCROLL(0), PsHScrollBar_GetPos(HWND_HSCROLL(0)) + 3 ) : moved = true
            end select
            if moved then
                InvalidateRect( hwnd, NULL, FALSE )
                dim as string s = "SetPos (silent)  instance A  pos=" & _
                    str(PsHScrollBar_GetPos(HWND_HSCROLL(0))) & "  <- no scroll callback fired"
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
            100, 100, 700, 320 _
            )

    ' ---- Instance A: large range, default paint, scroll callback traces ----
    HWND_HSCROLL(0) = PsHScrollBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_HSCROLL1 )
    PsHScrollBar_SetColors( HWND_HSCROLL(0), theme.BackColorScrollBar, _
                           theme.ForeColorScrollBar, theme.ForeColorScrollBarHot )
    PsHScrollBar_SetScrollCallback( HWND_HSCROLL(0), @HScroll_ScrollCallback )

    ' ---- Instance B: small range (auto-hides on a wide window) + custom paint ----
    HWND_HSCROLL(1) = PsHScrollBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_HSCROLL2 )
    PsHScrollBar_SetScrollCallback( HWND_HSCROLL(1), @HScroll_ScrollCallback )
    PsHScrollBar_SetPaintCallback( HWND_HSCROLL(1), @HScroll_PaintCallback )

    frmMain_Layout( HWND_FRMMAIN )

    print "PsHScrollBar test harness"
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
    print "  Keyboard     Left/Right = SetPos +/-3, Home/End = ends (all SILENT:"
    print "               no scroll-callback trace may appear for these)"
    print "  Auto-hide    widen the window until instance B's 30 units fit -> its bar"
    print "               disappears; narrow it -> the bar returns"
    print "  Resize       page size rederives from viewport width; thumb resizes,"
    print "               position is preserved (clamped at the right edge)"
    print "  Trackpad     two-finger swipe LEFT/RIGHT over a bar -> it scrolls, smoothly"
    print "               (no dead zone on a slow swipe); the same swipe over a VIEWPORT"
    print "               scrolls that bar too -- the host forwards it. A vertical swipe"
    print "               over a bar must do NOTHING. Try it with Settings -> Mouse ->"
    print "               'Scroll inactive windows' OFF: hovering stops working, the"
    print "               forwarded viewport path should still work"
    print "  2 instances  nothing you do in one may disturb the other"
    print ""

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    if environ("PSHSCROLL_SELFTEST") = "1" then
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
