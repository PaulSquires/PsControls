'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' One settings row: the label the form draws, and the toggle that sits at its right.
type DEMO_ROW
    wszLabel as DWSTRING
    wszNote  as DWSTRING          ' small print under the label, "" for none
end type
dim shared gRows(0 to TOGGLE_COUNT - 1) as DEMO_ROW

dim shared as long gFailCount
dim shared as long gTestCount
dim shared as long gSilentCbCount    ' counts change-callback hits during the silent test


' ========================================================================================
' Row geometry. The toggle cell is deliberately much WIDER than a switch needs (240px) so
' the three justification rows show their difference on screen, not just in an assertion.
' ========================================================================================
#define ROW_HEIGHT      52
#define ROW_TOP         14
#define ROW_MARGIN      22
#define CELL_WIDTH     240

private function RowRect( byval hwnd as HWND, byval idx as long ) as RECT
    dim as RECT rc, rcRow
    GetClientRect( hwnd, @rc )
    dim as long h = ROW_HEIGHT
    dim as long t = ROW_TOP
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then
        h = pWindow->ScaleY(ROW_HEIGHT)
        t = pWindow->ScaleY(ROW_TOP)
    end if
    SetRect( @rcRow, rc.left, t + (idx * h), rc.right, t + (idx * h) + h )
    return rcRow
end function


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmMain
'
' The form draws the labels and the hairlines; each toggle paints its own cell. There is no
' container window between them -- the same arrangement tiko's Output panel uses for its
' PsSelectBar + PsIconPanel pair.
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to TOGGLE_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )

        ' label
        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right -= margin
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )

        ' hairline under the row
        dim as RECT rcLine
        SetRect( @rcLine, rcRow.left + margin, rcRow.bottom - 1, rcRow.right - margin, rcRow.bottom )
        b.SetBackColor( theme.Divider )
        b.PaintRect( @rcLine )
    next

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Place every toggle at the right end of its row. Each is given its IDEAL SIZE -- which
' includes the focus-ring band, so a focused switch is never clipped -- except the three
' justification rows, which get the full 240px cell so the placement rule is visible.
' ========================================================================================
sub frmMain_PositionToggles( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    dim as long cellW  = CELL_WIDTH
    if pWindow then
        margin = pWindow->ScaleX(ROW_MARGIN)
        cellW  = pWindow->ScaleX(CELL_WIDTH)
    end if

    for i as long = 0 to TOGGLE_COUNT - 1
        if IsWindow(ghToggle(i)) = 0 then continue for
        dim as RECT rcRow = RowRect( hwnd, i )

        dim as long iw, ih
        PsToggle_GetIdealSize( ghToggle(i), iw, ih )

        ' Rows 5..7 demonstrate justification, so they get the whole wide cell. The rest are
        ' sized exactly to the switch and right-aligned against the margin.
        dim as long w = iw
        dim as long x = rc.right - margin - iw
        if i >= 5 then
            w = cellW
            x = rc.right - margin - cellW
        end if

        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - ih) \ 2)
        SetWindowPos( ghToggle(i), 0, x, y, w, ih, SWP_NOZORDER )
    next
end sub


' ========================================================================================
' Process WM_SIZE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        frmMain_PositionToggles( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
    function = 0
end function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean
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
        HANDLE_MSG (hwnd, WM_SIZE,          frmMain_OnSize)
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

        case WM_SYSCOMMAND
            if (wParam and &HFFF0) = SC_CLOSE then
                SendMessage( hwnd, WM_CLOSE, wParam, lParam )
                exit function
            end if

    end select

    ' for messages that we don't deal with
    function = DefWindowProc(HWnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' Name this instance, so the console output says which switch is talking.
' ========================================================================================
private function ToggleName( byval hCtl as HWND ) as string
    for i as long = 0 to TOGGLE_COUNT - 1
        if hCtl = ghToggle(i) then return str(i) & " (" & gRows(i).wszLabel & ")"
    next
    return "selftest"
end function


' ========================================================================================
' PsToggle_CheckChangedCallback -- the USER flipped a switch, by click or by Space/Enter.
' Never fires for the programmatic PsToggle_SetChecked, which is exactly the point.
' ========================================================================================
sub PsToggle_CheckChangedCallback( byval hCtl as HWND, byval isChecked as boolean )
    gSilentCbCount += 1
    dim as string s
    s = "checkchanged  toggle=" & ToggleName(hCtl) & "  checked=" & str(isChecked)
    print s
end sub


' ========================================================================================
' PsToggle_MessageCallback -- right-click and focus changes are reported, never acted on.
' ========================================================================================
function PsToggle_MessageCallback( byval m as PSTOGGLE_MESSAGEINFO ptr ) as boolean
    select case m->uMsg
    case WM_RBUTTONUP
        dim as string s = "rclick        toggle=" & ToggleName(m->hToggle)
        print s
    case WM_SETFOCUS, WM_KILLFOCUS
        dim as string s = "focus         toggle=" & ToggleName(m->hToggle) & _
                          "  focused=" & str(m->uMsg = WM_SETFOCUS)
        print s
    end select
    ' Return false to allow the control's default handling to proceed.
    return false
end function


' ========================================================================================
' PsToggle_SquarePaintCallback -- row 4's own renderer, REPLACING the built-in painter.
'
' It deliberately draws a SQUARE switch, which makes the point that a callback owns the whole
' look -- the pill and the knob are the built-in painter's choice, not the control's.
'
' (This comment used to add that a callback also "gives up the supersampling pass the built-in
' painter runs". That has not been true since PsBufferPaint took over rendering: the
' supersampled tile is gone, geometry goes through GDI+, and a callback gets exactly the same
' antialiased primitives the built-in painter does. Nothing is given up by setting one.)
'
' The control has already filled the client with BackColor, so only the switch itself needs
' painting here.
' ========================================================================================
sub PsToggle_SquarePaintCallback( byval p as PSTOGGLE_PAINTINFO ptr )

    dim as COLORREF clrTrack = theme.BackColorHot
    dim as COLORREF clrKnob  = theme.ForeColor

    if p->isEnabled = false then
        clrTrack = theme.BackColorHot
        clrKnob  = theme.ForeColorDisabled
    elseif p->isPressed then
        clrTrack = theme.FocusAccent          ' pressed differs from hot, unlike the built-in
        clrKnob  = BGR(255,255,255)
    elseif p->isChecked then
        clrTrack = theme.BackColorSelect
        clrKnob  = BGR(255,255,255)
    end if
    if p->isHot andalso (p->isPressed = false) then clrKnob = BGR(255,255,255)

    p->b->SetBackColor( clrTrack )
    p->b->PaintRect( @p->rcTrack )

    p->b->SetBackColor( clrKnob )
    p->b->PaintRect( @p->rcKnob )

    ' The focus ring is the built-in painter's job too, so a callback that wants one draws
    ' it. The band is already reserved in the layout (rcVisual), so there is room.
    '
    ' PaintRoundOutline, NOT PaintBorderRect. PaintBorderRect delegates to PaintRectFactory,
    ' which ALWAYS fills the rect with _backcolor before stroking it -- so used here it
    ' flooded rcVisual with the knob colour set two lines up and erased the whole switch,
    ' leaving a solid block whenever this row had focus. PaintRoundOutline exists for exactly
    ' this case: stroke over already-painted pixels without filling. Curvature 0 gives the
    ' square ring this square switch wants.
    if p->isFocused then
        p->b->SetPenColor( theme.FocusAccent )
        p->b->PaintRoundOutline( @p->rcVisual, 0, 1 )
    end if

end sub


' ========================================================================================
' Geometry self-test (run with PSTOGGLE_SELFTEST=1).
'
' Every number is asserted, never eyeballed -- CLAUDE.md's rule, and Learnings.md records
' several "verifications" that were invalid because they were done by eye. Expectations are
' computed from what the control ACTUALLY reports for its own defaults (which are DPI-scaled
' at Create), so this cannot produce a false failure on a high-DPI display.
'
' What this CANNOT cover, and is therefore left to the interactive pass: the appearance of
' the antialiased edges, hover, and the press/cancel slide-off.
'
' Tab navigation WAS on that list, and that is exactly why it shipped broken -- "left to the
' interactive pass" turned out to mean "never actually checked". It is asserted now.
' ========================================================================================
private sub SelfTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gTestCount += 1
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

' Size the test control and report the client size it actually got, so every expectation
' below is computed from reality rather than from what we asked for.
private sub SelfTest_SizeTo( byval h as HWND, byval w as long, byval hgt as long, byref cw as long, byref ch as long )
    SetWindowPos( h, 0, 0, 0, w, hgt, SWP_NOZORDER or SWP_NOMOVE )
    dim as RECT rc
    GetClientRect( h, @rc )
    cw = rc.right - rc.left
    ch = rc.bottom - rc.top
end sub

' ========================================================================================
' Count the DISTINCT colours the real renderer produces inside the knob's bounding box.
'
' This guards the assumption the whole rendering design rests on: that the knob's rim is
' actually being SMOOTHED. It used to guard a HALFTONE StretchBlt averaging a supersampled
' tile; it now guards GDI+ antialiasing the ellipse directly. The assertion is deliberately
' unchanged in shape, because the failure it catches is unchanged: if smoothing silently
' stopped happening -- a supersample that dropped pixels then, a SmoothingMode that failed
' to apply now -- the control would still draw perfectly plausible output, just with hard
' aliased edges, and not one other assertion in this file would notice.
'
' A circle's rim is the ideal probe: its diagonals guarantee partial pixel coverage, so a
' smoothed knob yields many tones between the knob colour and the track colour, and an
' unsmoothed one yields exactly two.
'
' It drives the SHIPPED code path (PsToggle_RenderDefault) into an offscreen buffer, so what
' is measured is what a WM_PAINT would have produced.
' ========================================================================================
private function SelfTest_KnobEdgeTones( byval h as HWND ) as long
    dim as PSTOGGLE ptr pTog = GetTogglePointer(h)
    if pTog = 0 then return 0

    dim as RECT rcClient, rcKnob
    GetClientRect( h, @rcClient )
    if PsToggle_GetKnobRect( h, rcKnob ) = false then return 0

    dim as HDC hdc = GetDC(h)
    if hdc = 0 then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( h, hdc, rcClient )
    b.SetBackColor( pTog->colors.BackColor )
    b.PaintClientRect()

    dim p as PSTOGGLE_PAINTINFO
    p.hToggle   = h
    p.b         = @b
    p.rcClient  = rcClient
    p.rcTrack   = pTog->rcTrack
    p.rcKnob    = pTog->rcKnob
    p.rcVisual  = pTog->rcVisual
    p.isChecked = pTog->isChecked
    p.isEnabled = true
    PsToggle_RenderDefault( pTog, @b, @p )

    ' Collect distinct colours over the knob, one pixel of margin each way so the rim is
    ' fully inside the scanned region.
    dim as HDC memDC = b.getMemDC()
    dim as long nDistinct = 0
    if memDC then
        dim as COLORREF seen(0 to 63)
        for y as long = rcKnob.top - 1 to rcKnob.bottom
            for x as long = rcKnob.left - 1 to rcKnob.right
                dim as COLORREF c = GetPixel( memDC, x, y )
                if c = CLR_INVALID then continue for
                dim as boolean bFound = false
                for i as long = 0 to nDistinct - 1
                    if seen(i) = c then
                        bFound = true
                        exit for
                    end if
                next
                if bFound = false then
                    if nDistinct > 63 then continue for      ' the cap is plenty to decide
                    seen(nDistinct) = c
                    nDistinct += 1
                end if
            next
        next
    end if

    b.EndDoubleBuffer()
    ReleaseDC( h, hdc )
    return nDistinct
end function


sub PsToggle_SelfTest()

    dim as HWND h = PsToggle_Create( HWND_FRMMAIN, IDC_FRMMAIN_TOGGLE_TEST )
    if h = 0 then
        print "FAIL  selftest could not create a control"
        gFailCount += 1
        exit sub
    end if
    dim pw as CWindow ptr = AfxCWindowPtr(h)

    dim as long tw, th, inset, fgap, fthick
    PsToggle_GetTrackSize( h, tw, th )
    inset  = PsToggle_GetKnobInset( h )
    PsToggle_GetFocusRing( h, fgap, fthick )
    dim as long ringPad = fgap + fthick
    dim as long knobDia = th - (2 * inset)

    dim as RECT rcT, rcK, rcKoff, rcKon, rcV
    dim as long iw, ih, cw, ch

    ' ---- Create-time defaults ----------------------------------------------------------
    SelfTest_Check( "track size is DPI-scaled", _
        (tw = pw->ScaleX(PSTOGGLE_DEFAULT_TRACKW)) andalso (th = pw->ScaleY(PSTOGGLE_DEFAULT_TRACKH)), _
        "tw=" & str(tw) & " th=" & str(th) )

    SelfTest_Check( "knob inset is DPI-scaled", _
        inset = pw->ScaleY(PSTOGGLE_DEFAULT_KNOBINSET), "inset=" & str(inset) )

    SelfTest_Check( "border thickness is NOT DPI-scaled", _
        PsToggle_GetBorderThickness(h) = PSTOGGLE_DEFAULT_BORDERTHICK, _
        "border=" & str(PsToggle_GetBorderThickness(h)) )

    SelfTest_Check( "focus thickness is NOT DPI-scaled", _
        fthick = PSTOGGLE_DEFAULT_FOCUSTHICK, "fthick=" & str(fthick) )

    SelfTest_Check( "default justify is CENTER", _
        PsToggle_GetJustify(h) = TOG_JUSTIFY_CENTER, "justify=" & str(PsToggle_GetJustify(h)) )

    ' ---- ideal size is usable BEFORE the control has ever been sized --------------------
    PsToggle_GetIdealSize( h, iw, ih )
    SelfTest_Check( "ideal size valid before sizing", _
        (iw = tw + (2 * ringPad)) andalso (ih = th + (2 * ringPad)), _
        "ideal=" & str(iw) & "x" & str(ih) )

    SelfTest_Check( "rect query returns FALSE before sizing", _
        PsToggle_GetTrackRect( h, rcT ) = false, "" )

    ' ---- vertical centring, odd client height ------------------------------------------
    SelfTest_SizeTo( h, 200, 61, cw, ch )
    PsToggle_GetTrackRect( h, rcT )
    SelfTest_Check( "track v-centred (odd client height)", _
        rcT.top = ((ch - th) \ 2), "top=" & str(rcT.top) & " expected=" & str((ch - th) \ 2) )
    SelfTest_Check( "track keeps its authored size", _
        ((rcT.right - rcT.left) = tw) andalso ((rcT.bottom - rcT.top) = th), _
        "track=" & str(rcT.right - rcT.left) & "x" & str(rcT.bottom - rcT.top) )

    ' ---- the antialiasing is REAL, not assumed ------------------------------------------
    ' Exactly two distinct colours means no smoothing reached the rim at all -- the GDI+
    ' SmoothingMode never applied, and the edges are as hard as the old GDI ones were.
    ' Real antialiasing produces dozens of tones around the knob's rim; 4 is a deliberately
    ' conservative floor.
    dim as long nTones = SelfTest_KnobEdgeTones( h )
    SelfTest_Check( "knob rim is antialiased (GDI+ smoothing reached the rim)", _
        nTones >= 4, "distinct tones over the knob=" & str(nTones) )

    ' ---- horizontal justification ------------------------------------------------------
    SelfTest_Check( "CENTER x", _
        rcT.left = ((cw - tw) \ 2), "left=" & str(rcT.left) & " expected=" & str((cw - tw) \ 2) )

    PsToggle_SetJustify( h, TOG_JUSTIFY_LEFT )
    PsToggle_GetTrackRect( h, rcT )
    SelfTest_Check( "LEFT x = ringPad", _
        rcT.left = ringPad, "left=" & str(rcT.left) & " ringPad=" & str(ringPad) )

    PsToggle_SetJustify( h, TOG_JUSTIFY_RIGHT )
    PsToggle_GetTrackRect( h, rcT )
    SelfTest_Check( "RIGHT x = clientW - ringPad", _
        rcT.right = (cw - ringPad), "right=" & str(rcT.right) & " expected=" & str(cw - ringPad) )

    ' ---- overflow degrades to LEFT (still RIGHT-justified) -----------------------------
    SelfTest_SizeTo( h, tw + (2 * ringPad) - 6, 60, cw, ch )
    PsToggle_GetTrackRect( h, rcT )
    SelfTest_Check( "overflow degrades to LEFT", _
        rcT.left = ringPad, "clientW=" & str(cw) & " left=" & str(rcT.left) )

    ' ---- vertical centring, even client height -----------------------------------------
    PsToggle_SetJustify( h, TOG_JUSTIFY_CENTER )
    SelfTest_SizeTo( h, 200, 60, cw, ch )
    PsToggle_GetTrackRect( h, rcT )
    SelfTest_Check( "track v-centred (even client height)", _
        rcT.top = ((ch - th) \ 2), "top=" & str(rcT.top) & " expected=" & str((ch - th) \ 2) )

    ' ---- the knob ----------------------------------------------------------------------
    PsToggle_GetKnobRect( h, rcKoff )
    SelfTest_Check( "knob diameter = trackH - 2*inset", _
        ((rcKoff.right - rcKoff.left) = knobDia) andalso ((rcKoff.bottom - rcKoff.top) = knobDia), _
        "knob=" & str(rcKoff.right - rcKoff.left) & "x" & str(rcKoff.bottom - rcKoff.top) & " expected=" & str(knobDia) )

    SelfTest_Check( "knob inset symmetric top and bottom", _
        ((rcKoff.top - rcT.top) = inset) andalso ((rcT.bottom - rcKoff.bottom) = inset), _
        "top gap=" & str(rcKoff.top - rcT.top) & " bottom gap=" & str(rcT.bottom - rcKoff.bottom) )

    SelfTest_Check( "knob OFF sits at the left end", _
        rcKoff.left = (rcT.left + inset), "left=" & str(rcKoff.left) & " expected=" & str(rcT.left + inset) )

    PsToggle_SetChecked( h, true )
    PsToggle_GetKnobRect( h, rcKon )
    SelfTest_Check( "knob ON sits at the right end", _
        rcKon.right = (rcT.right - inset), "right=" & str(rcKon.right) & " expected=" & str(rcT.right - inset) )

    SelfTest_Check( "OFF and ON knobs mirror about the track centre", _
        (rcKoff.left - rcT.left) = (rcT.right - rcKon.right), _
        "offGap=" & str(rcKoff.left - rcT.left) & " onGap=" & str(rcT.right - rcKon.right) )

    ' ---- SetChecked is SILENT ----------------------------------------------------------
    ' The callback increments gSilentCbCount, so "silent" is measured, not assumed.
    PsToggle_SetCheckChangedCallback( h, @PsToggle_CheckChangedCallback )
    dim as long nBefore = gSilentCbCount
    PsToggle_SetChecked( h, false )
    PsToggle_SetChecked( h, true )
    SelfTest_Check( "SetChecked fires no change callback", _
        gSilentCbCount = nBefore, "callbacks=" & str(gSilentCbCount - nBefore) )
    SelfTest_Check( "SetChecked still moved the knob", _
        PsToggle_GetChecked(h) = true, "" )

    ' ---- the visual bounds --------------------------------------------------------------
    PsToggle_GetTrackRect( h, rcT )
    PsToggle_GetVisualRect( h, rcV )
    SelfTest_Check( "visual = track inflated by ringPad on all four sides", _
        ((rcT.left - rcV.left) = ringPad) andalso ((rcV.right - rcT.right) = ringPad) andalso _
        ((rcT.top - rcV.top) = ringPad) andalso ((rcV.bottom - rcT.bottom) = ringPad), _
        "ringPad=" & str(ringPad) )

    PsToggle_GetIdealSize( h, iw, ih )
    SelfTest_Check( "ideal size = visual size", _
        (iw = (rcV.right - rcV.left)) andalso (ih = (rcV.bottom - rcV.top)), _
        "ideal=" & str(iw) & "x" & str(ih) & " visual=" & str(rcV.right - rcV.left) & "x" & str(rcV.bottom - rcV.top) )

    ' ---- mutators re-lay out ------------------------------------------------------------
    PsToggle_SetTrackSize( h, 60, 30 )
    PsToggle_GetTrackRect( h, rcT )
    SelfTest_Check( "SetTrackSize re-lays out", _
        ((rcT.right - rcT.left) = 60) andalso ((rcT.bottom - rcT.top) = 30), _
        "track=" & str(rcT.right - rcT.left) & "x" & str(rcT.bottom - rcT.top) )

    PsToggle_GetKnobRect( h, rcK )
    SelfTest_Check( "knob follows the new track height", _
        (rcK.bottom - rcK.top) = (30 - (2 * inset)), _
        "dia=" & str(rcK.bottom - rcK.top) & " expected=" & str(30 - (2 * inset)) )

    PsToggle_SetKnobInset( h, 5 )
    PsToggle_GetKnobRect( h, rcK )
    SelfTest_Check( "SetKnobInset changes the diameter", _
        (rcK.bottom - rcK.top) = 20, "dia=" & str(rcK.bottom - rcK.top) & " expected=20" )

    PsToggle_SetFocusRing( h, 6, 2 )
    PsToggle_GetIdealSize( h, iw, ih )
    SelfTest_Check( "focus ring participates in the ideal size", _
        (iw = (60 + 16)) andalso (ih = (30 + 16)), "ideal=" & str(iw) & "x" & str(ih) & " expected=76x46" )

    ' ---- enable / disable ---------------------------------------------------------------
    PsToggle_SetEnabled( h, false )
    SelfTest_Check( "SetEnabled(false) disables the WINDOW (input cannot sneak past)", _
        (IsWindowEnabled(h) = 0) andalso (PsToggle_GetEnabled(h) = false), _
        "IsWindowEnabled=" & str(IsWindowEnabled(h)) )
    SelfTest_Check( "a disabled toggle keeps its checked state", _
        PsToggle_GetChecked(h) = true, "" )
    PsToggle_SetEnabled( h, true )
    SelfTest_Check( "SetEnabled(true) re-enables the window", _
        (IsWindowEnabled(h) <> 0) andalso (PsToggle_GetEnabled(h) = true), "" )

    ' ---- input validation ----------------------------------------------------------------
    PsToggle_SetJustify( h, 99 )
    SelfTest_Check( "SetJustify ignores an out-of-range value", _
        PsToggle_GetJustify(h) = TOG_JUSTIFY_CENTER, "justify=" & str(PsToggle_GetJustify(h)) )

    ' ---- tab navigability ------------------------------------------------------------------
    ' Added 2026-07-23, because this SHIPPED BROKEN. CWindow.Create defaults dwExStyle to
    ' WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE and PsToggle_Create passed only dwStyle, so the
    ' control declared itself a container; the dialog manager descended into it, found no
    ' children, and skipped it. Tab never reached a toggle. None of the ~30 geometry assertions
    ' above could see it, because nothing here had ever asked what the dialog manager sees.
    dim as ulong exStyle = cast( ulong, GetWindowLongPtr( h, GWL_EXSTYLE ) )
    SelfTest_Check( "NOT a control parent (a leaf control is a tab ITEM)", _
        (exStyle and WS_EX_CONTROLPARENT) = 0, "exstyle=&h" & hex(exStyle) )

    dim as ulong wsStyle = cast( ulong, GetWindowLongPtr( h, GWL_STYLE ) )
    SelfTest_Check( "is a tabstop", (wsStyle and WS_TABSTOP) <> 0, "style=&h" & hex(wsStyle) )

    ' The end-to-end claim rather than the flag: ask the dialog manager to walk to us. The
    ' WS_VISIBLE bit is set directly rather than with ShowWindow -- the walk tests the style
    ' bit, and this way there is no paint, no flash, and no dependence on whether the host runs
    ' the self-test before or after it shows the form.
    SetWindowLongPtr( h, GWL_STYLE, cast( LONG_PTR, wsStyle or WS_VISIBLE ) )
    dim as HWND hFirstTab = GetNextDlgTabItem( HWND_FRMMAIN, 0, FALSE )
    dim as boolean bReachable = false
    dim as HWND hScan = hFirstTab
    for i as long = 0 to 63
        if hScan = 0 then exit for
        if hScan = h then
            bReachable = true
            exit for
        end if
        hScan = GetNextDlgTabItem( HWND_FRMMAIN, hScan, FALSE )
        if hScan = hFirstTab then exit for          ' wrapped all the way round
    next
    SetWindowLongPtr( h, GWL_STYLE, cast( LONG_PTR, wsStyle ) )
    SelfTest_Check( "the dialog manager can reach us by Tab", bReachable, _
        "first tab item id=" & str(GetDlgCtrlID(hFirstTab)) & " reached=" & str(bReachable) )

    DestroyWindow( h )

    dim as string s
    ' ------------------------------------------------------------------------------------
    ' The Enter/Space BEEP.
    '
    ' Claiming a key with DLGC_WANTALLKEYS makes IsDialogMessage TranslateMessage it, which
    ' manufactures a matching WM_CHAR. Nothing consuming that character means it reaches
    ' DefWindowProc, and the system beeps -- while the keystroke itself works perfectly. The
    ' beep is the ONLY symptom, so it cannot be heard by any assertion; what is asserted here
    ' is the mechanism that suppresses it, entirely through messages.
    '
    ' The control is DISABLED on purpose: the arm happens before the enabled check, so the
    ' mechanism is identical while nothing fires a callback or opens a dropdown -- and a
    ' disabled control that swallows the key owes the character too.
    scope
        dim as HWND hBeep = PsToggle_Create( HWND_FRMMAIN, 7803 )
        PsToggle_SetEnabled( hBeep, false )

        dim as MSG mBeep
        mBeep.hwnd = hBeep : mBeep.message = WM_CHAR : mBeep.wParam = 13

        SendMessageW( hBeep, WM_KEYDOWN, VK_RETURN, 0 )
        dim as LRESULT dc1 = SendMessageW( hBeep, WM_GETDLGCODE, 13, cast(LPARAM, @mBeep) )
        SelfTest_Check( "Enter arms the WM_CHAR swallow that stops the system beep", _
            (dc1 and DLGC_WANTALLKEYS) <> 0, "dlgcode=&h" & hex(dc1) )

        SendMessageW( hBeep, WM_CHAR, 13, 0 )
        dim as LRESULT dc2 = SendMessageW( hBeep, WM_GETDLGCODE, 13, cast(LPARAM, @mBeep) )
        ' The disarm is the proof the WM_CHAR was consumed. The return value cannot be:
        ' DefWindowProc also returns 0 for WM_CHAR, so it reads the same when it fell through.
        SelfTest_Check( "...and the WM_CHAR is then swallowed instead of reaching DefWindowProc", _
            (dc2 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc2) )

        ' A key we never claimed must leave ordinary characters alone, or a host accelerator
        ' on a printable key would stop working.
        SendMessageW( hBeep, WM_KEYDOWN, VK_TAB, 0 )
        mBeep.wParam = asc("a")
        dim as LRESULT dc3 = SendMessageW( hBeep, WM_GETDLGCODE, asc("a"), cast(LPARAM, @mBeep) )
        SelfTest_Check( "an unclaimed key leaves ordinary characters alone", _
            (dc3 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc3) )

        DestroyWindow( hBeep )
    end scope

    s = "---- PsToggle self-test: " & str(gTestCount) & " assertions, " & str(gFailCount) & " failed ----"
    print s

end sub


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 620, 520 _
            )

    gRows(0).wszLabel = "Word wrap"
    gRows(1).wszLabel = "Show line numbers"
    gRows(2).wszLabel = "Highlight current line   (custom colours)"
    gRows(3).wszLabel = "Auto-save   (disabled, but still ON)"
    gRows(4).wszLabel = "Custom painter   (square switch)"
    gRows(5).wszLabel = "Justify LEFT   (wide cell)"
    gRows(6).wszLabel = "Justify CENTER   (wide cell)"
    gRows(7).wszLabel = "Justify RIGHT   (wide cell)"

    for i as long = 0 to TOGGLE_COUNT - 1
        ghToggle(i) = PsToggle_Create( HWND_FRMMAIN, IDC_FRMMAIN_TOGGLE_FIRST + i )
        PsToggle_SetMessageCallback( ghToggle(i), @PsToggle_MessageCallback )
        PsToggle_SetCheckChangedCallback( ghToggle(i), @PsToggle_CheckChangedCallback )
    next

    ' Row 0 -- default colours, ON. Programmatic, so NO callback line should appear.
    PsToggle_SetChecked( ghToggle(0), true )

    ' Row 1 -- default colours, OFF. Nothing to do.

    ' Row 2 -- custom colours: a green ON pill, and an OFF pill with no border at all, which
    ' exercises the nBorderThick = 0 path in the renderer.
    dim as PSTOGGLE_COLORS clrs
    PsToggle_GetColors( ghToggle(2), @clrs )
    clrs.TrackColorOn            = BGR( 62,140, 90)
    clrs.TrackColorOnHot         = BGR( 78,168,110)
    clrs.TrackBorderColorOn      = BGR( 62,140, 90)
    clrs.TrackBorderColorOnHot   = BGR( 78,168,110)
    clrs.KnobColorOn             = BGR(255,255,255)
    clrs.FocusRingColor          = BGR(120,200,150)
    PsToggle_SetColors( ghToggle(2), @clrs )
    PsToggle_SetTrackSize( ghToggle(2), pWindow->ScaleX(52), pWindow->ScaleY(26) )
    PsToggle_SetKnobInset( ghToggle(2), pWindow->ScaleY(3) )
    PsToggle_SetChecked( ghToggle(2), true )

    ' Row 3 -- disabled AND checked, which is the case the per-state disabled colours exist
    ' for: it must still read as ON.
    PsToggle_SetChecked( ghToggle(3), true )
    PsToggle_SetEnabled( ghToggle(3), false )

    ' Row 4 -- the built-in painter replaced wholesale.
    PsToggle_SetPaintCallback( ghToggle(4), @PsToggle_SquarePaintCallback )

    ' Rows 5..7 -- the three justifications, in a cell far wider than the switch.
    PsToggle_SetJustify( ghToggle(5), TOG_JUSTIFY_LEFT )
    PsToggle_SetJustify( ghToggle(6), TOG_JUSTIFY_CENTER )
    PsToggle_SetJustify( ghToggle(7), TOG_JUSTIFY_RIGHT )
    PsToggle_SetChecked( ghToggle(7), true )

    dim as long iw, ih
    PsToggle_GetIdealSize( ghToggle(0), iw, ih )
    dim as string s = "default ideal size: " & str(iw) & "x" & str(ih)
    print s

    frmMain_PositionToggles( HWND_FRMMAIN )
    for i as long = 0 to TOGGLE_COUNT - 1
        ShowWindow( ghToggle(i), SW_SHOW )
    next

    if environ("PSTOGGLE_SELFTEST") = "1" then PsToggle_SelfTest()

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Give the first toggle the focus. A real dialog does this in WM_INITDIALOG; an ordinary
    ' window must do it itself, and without it IsDialogMessage ignores the FIRST Tab entirely --
    ' it only acts when the focused window is a DESCENDANT of the window it is passed, and at
    ' startup the focus is on the form. The symptom is "Tab does nothing until you click
    ' something", which is indistinguishable from the tabstops being broken.
    SetFocus( ghToggle(0) )

    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then

            ' IsDialogMessage is what makes Tab move between the toggles -- PsToggle is the
            ' first control in the family that needs it. A host without it still gets full
            ' mouse behaviour and, once a toggle has focus, Space and Enter.
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
