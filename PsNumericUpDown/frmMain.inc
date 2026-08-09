'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' One settings row: the label the form draws, and the spinner that sits at its right.
type DEMO_ROW
    wszLabel as DWSTRING
end type
dim shared gRows(0 to SPIN_COUNT - 1) as DEMO_ROW

dim shared as long gFailCount
dim shared as long gTestCount
dim shared as long gChangeCount      ' counts value-changed hits during the silent tests


' ========================================================================================
' Row geometry.
' ========================================================================================
#define ROW_HEIGHT      48
#define ROW_TOP         14
#define ROW_MARGIN      22

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
' The form draws the labels; each spinner paints its own cell. There is no container window
' between them.
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to SPIN_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )
        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right -= margin
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )
    next

    ' A caption over the plain textbox at the bottom, so the tab-order point is obvious.
    dim as RECT rcRow = RowRect( hwnd, SPIN_COUNT )
    dim as RECT rcText = rcRow
    rcText.left  += margin
    rcText.right -= margin
    b.SetColors( theme.ForeColor, theme.BackColor )
    b.SetFont( ghFont(GUIFONT_10) )
    b.PaintText( "Plain PsTextBox   (Tab should reach it, and come back)", @rcText, DT_LEFT )

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Place every spinner at the right end of its row, at its own IDEAL SIZE -- which is what a
' host is expected to do, and what makes a too-narrow control impossible by construction.
' ========================================================================================
sub frmMain_PositionControls( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to SPIN_COUNT - 1
        if IsWindow(ghSpin(i)) = 0 then continue for
        dim as RECT rcRow = RowRect( hwnd, i )

        dim as long iw, ih
        PsNumericUpDown_GetIdealSize( ghSpin(i), iw, ih )

        dim as long x = rc.right - margin - iw
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - ih) \ 2)
        SetWindowPos( ghSpin(i), 0, x, y, iw, ih, SWP_NOZORDER )
    next

    if IsWindow(ghPlainBox) then
        dim as RECT rcRow = RowRect( hwnd, SPIN_COUNT )
        dim as long w = 160
        dim as long h = 30
        if pWindow then
            w = pWindow->ScaleX(160)
            h = pWindow->ScaleY(30)
        end if
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - h) \ 2)
        SetWindowPos( ghPlainBox, 0, rc.right - margin - w, y, w, h, SWP_NOZORDER )
    end if
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
        frmMain_PositionControls( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
    function = 0
end function


function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean
    return true
end function


function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow(hwnd)
    function = 0
end function


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
' Name this instance, so the console output says which spinner is talking.
' ========================================================================================
private function SpinName( byval hCtl as HWND ) as string
    for i as long = 0 to SPIN_COUNT - 1
        if hCtl = ghSpin(i) then return str(i) & " (" & gRows(i).wszLabel & ")"
    next
    return "selftest"
end function


' ========================================================================================
' PsNumericUpDown_ValueChangedCallback -- the USER changed a value.
'
' Immediately for a button, an arrow key, PgUp/PgDn or the wheel; on COMMIT (ENTER or focus
' leaving the field) for typing. Never for the programmatic setters -- which is what the
' silent-setter assertions in the self-test check.
' ========================================================================================
sub PsNumericUpDown_ValueChangedCallback( byval hCtl as HWND, byval nValue as double )
    gChangeCount += 1
    dim as string s
    s = "valuechanged  spinner=" & SpinName(hCtl) & "  value=" & str(nValue)
    print s
end sub


' ========================================================================================
' PsNumericUpDown_MessageCallback -- observe, never act.
'
' Note the two SOURCES arriving at one callback: WM_RBUTTONUP comes from the container (the
' mouse over a button), while WM_SETFOCUS/WM_KILLFOCUS are relayed up from the RichEdit
' inside the embedded PsTextBox. A host never has to know which is which.
' ========================================================================================
function PsNumericUpDown_MessageCallback( byval m as PSNUMERICUPDOWN_MESSAGEINFO ptr ) as boolean
    select case m->uMsg
    case WM_RBUTTONUP
        dim as string s = "rclick        spinner=" & SpinName(m->hCtrl)
        print s
    case WM_SETFOCUS, WM_KILLFOCUS
        dim as string s = "focus         spinner=" & SpinName(m->hCtrl) & _
                          "  focused=" & str(m->uMsg = WM_SETFOCUS)
        print s
    end select
    ' Return false to allow the control's default handling to proceed.
    return false
end function


' ========================================================================================
' PsNumericUpDown_FlatPaintCallback -- row 3's own renderer, REPLACING the built-in painter.
'
' Square corners, square button cells, and a thicker glyph, which makes two things visible at
' once: that a callback owns the whole look, and that it still does NOT own the number -- the
' RichEdit paints that over rcValue afterwards no matter what this draws there.
'
' It also honours p->bAtMin / p->bAtMax, which is how a callback gets the at-a-limit greying
' without having to know anything about the range.
' ========================================================================================
sub PsNumericUpDown_FlatPaintCallback( byval p as PSNUMERICUPDOWN_PAINTINFO ptr )

    ' The value cell's background, square this time.
    p->b->SetBackColor( theme.BackColorHot )
    p->b->PaintRect( @p->rcFrame )

    for iPart as long = NUD_PART_MINUS to NUD_PART_PLUS step (NUD_PART_PLUS - NUD_PART_MINUS)
        dim as RECT rcCell
        dim as boolean bAtLimit
        if iPart = NUD_PART_MINUS then
            rcCell = p->rcMinus : bAtLimit = p->bAtMin
        else
            rcCell = p->rcPlus  : bAtLimit = p->bAtMax
        end if

        dim as COLORREF clrBack = theme.BackColorHot
        dim as COLORREF clrGlyph = theme.ForeColor
        if (p->isEnabled = false) orelse bAtLimit then
            clrGlyph = theme.ForeColorDisabled
        elseif p->pressedPart = iPart then
            clrBack = theme.BackColorSelect : clrGlyph = BGR(255,255,255)
        elseif p->hotPart = iPart then
            clrBack = theme.FocusAccent : clrGlyph = BGR(255,255,255)
        end if

        p->b->SetBackColor( clrBack )
        p->b->PaintRect( @rcCell )
        p->b->SetBackColor( clrGlyph )
        if iPart = NUD_PART_MINUS then
            dim as RECT rcBar = p->rcMinusBar
            p->b->PaintRect( @rcBar )
        else
            dim as RECT rcBarH = p->rcPlusBarH
            dim as RECT rcBarV = p->rcPlusBarV
            p->b->PaintRect( @rcBarH )
            p->b->PaintRect( @rcBarV )
        end if
    next

    ' A square frame instead of a rounded one, focus-coloured when focused.
    '
    ' PaintRoundOutline WITH CURVATURE 0, and NOT PaintBorderRect. PaintBorderRect delegates
    ' to PaintRectFactory, which fills the rect with the current BACK colour unconditionally
    ' -- the fill is not gated on anything, only the stroke is. Used here, over pixels this
    ' callback has just drawn, it flooded the entire control with whatever colour the last
    ' glyph happened to use and erased the cells, the dividers and both glyphs, leaving a
    ' solid block with only the RichEdit's value showing through (the child paints itself
    ' afterwards, and WS_CLIPCHILDREN keeps its rect out of our DC).
    '
    ' PaintRoundOutline exists for exactly this -- its own header says "stroke a rounded rect
    ' WITHOUT filling it ... a focus ring around an already-painted control being the case
    ' that needed it". Curvature 0 gives square corners.
    '
    ' This is the third time this defect has been written in this control family (PsComboBox
    ' and PsToggle first), every time by copying a sibling's paint callback without
    ' re-checking it. SelfTest_MinusCellTones now asserts against it.
    dim as RECT rcFrame = p->rcFrame
    p->b->SetPenColor( iif( p->isFocused, theme.FocusAccent, theme.Divider ) )
    p->b->PaintRoundOutline( @rcFrame, 0, 1 )

end sub


' ========================================================================================
' Geometry and arithmetic self-test (run with PSNUMERICUPDOWN_SELFTEST=1).
'
' Every number is asserted, never eyeballed -- CLAUDE.md's rule, and Learnings.md records
' several "verifications" that were invalid because they were done by eye. Expectations are
' computed from what the control ACTUALLY reports for its own defaults (which are DPI-scaled
' at Create), so this cannot produce a false failure on a high-DPI display.
'
' WHAT THIS CANNOT COVER, and is therefore left to the interactive pass: hover, the pressed
' look, auto-repeat, Tab navigation through the two container levels, select-on-focus versus
' caret-on-button-click, typing and its commit, and the right-click menu. Learnings.md is
' explicit that SendMessage-simulated clicks cannot reproduce capture, so no attempt is made
' to fake one here -- the wheel is driven by a real message because a hover-wheel genuinely
' arrives that way.
'
' CENTRING USED TO BE ON THAT LIST, and it was the item most likely to be quietly broken --
' which it was. The assertions below ask the RICHEDIT what its paragraph format is, rather
' than asking this control what it believes it set, and that is the only reason the earlier
' silently-refused EM_SETPARAFORMAT was ever caught.
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
' Count the DISTINCT colours the demo's CUSTOM PAINTER produces inside the minus button's
' interior, by driving it into an offscreen buffer exactly as a WM_PAINT would.
'
' WHAT THIS CATCHES, and why it is worth a permanent assertion rather than a throwaway probe:
' every primitive that fills-and-strokes (PaintBorderRect, PaintRoundBorderRect) goes through
' PaintRectFactory, which fills with the CURRENT BACK COLOUR unconditionally -- the fill is
' not gated on anything, only the stroke is. So one of those used as a FRAME, over pixels you
' have already drawn, floods the whole rect with whatever colour was last set and erases the
' lot. The control still paints, still reports no error, and every geometry assertion in this
' file still passes; the row just renders as a solid block.
'
' That exact defect has now been found in THREE sibling repos (PsComboBox and PsToggle before
' this one), every time in a paint callback copied from a sibling without being re-checked --
' the copy-a-sibling trap CLAUDE.md warns about. It is invisible to every other kind of test,
' so it gets one of its own here.
'
' The cell interior is scanned inset by 2px so the frame's own stroke is excluded: a flooded
' cell must read as exactly ONE colour, and a correct one has at least the cell fill plus the
' glyph bar.
' ========================================================================================
private function SelfTest_MinusCellTones( byval h as HWND ) as long
    dim as PSNUMERICUPDOWN ptr pNud = GetNumericUpDownPointer(h)
    if pNud = 0 then return 0

    dim as RECT rcClient
    GetClientRect( h, @rcClient )
    if (rcClient.right <= rcClient.left) orelse (rcClient.bottom <= rcClient.top) then return 0

    dim as HDC hdc = GetDC(h)
    if hdc = 0 then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( h, hdc, rcClient )
    b.SetBackColor( pNud->colors.BackColor )
    b.PaintClientRect()

    ' EVERY rect must be populated. A zeroed rect makes PaintRectFactory bail on a negative
    ' extent, and then a broken callback and a correct one measure identically -- which is
    ' how PsComboBox's first attempt at this probe managed to prove nothing at all.
    dim p as PSNUMERICUPDOWN_PAINTINFO
    p.hCtrl       = h
    p.b           = @b
    p.rcClient    = rcClient
    p.rcFrame     = pNud->rcFrame
    p.rcMinus     = pNud->rcMinus
    p.rcDiv1      = pNud->rcDiv1
    p.rcValue     = pNud->rcValue
    p.rcDiv2      = pNud->rcDiv2
    p.rcPlus      = pNud->rcPlus
    p.rcMinusBar  = pNud->rcMinusBar
    p.rcPlusBarH  = pNud->rcPlusBarH
    p.rcPlusBarV  = pNud->rcPlusBarV
    p.hotPart     = NUD_PART_NONE
    p.pressedPart = NUD_PART_NONE
    p.isEnabled   = true
    p.isFocused   = false
    ' The screenshot's state: sitting AT the maximum, so the plus glyph is the disabled
    ' colour -- which is precisely the colour the flood bug painted the whole control with.
    p.bAtMin      = false
    p.bAtMax      = true
    p.nValue      = 3.0

    PsNumericUpDown_FlatPaintCallback( @p )

    dim as HDC memDC = b.getMemDC()
    dim as long nDistinct = 0
    if memDC then
        dim as COLORREF seen(0 to 63)
        for y as long = p.rcMinus.top + 2 to p.rcMinus.bottom - 3
            for x as long = p.rcMinus.left + 2 to p.rcMinus.right - 3
                dim as COLORREF c = GetPixel( memDC, x, y )
                if c = CLR_INVALID then continue for
                dim as boolean bFound = false
                for i as long = 0 to nDistinct - 1
                    if seen(i) = c then
                        bFound = true
                        exit for
                    end if
                next
                if (bFound = false) andalso (nDistinct <= ubound(seen)) then
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


private function RectW( byref rc as RECT ) as long
    return rc.right - rc.left
end function
private function RectH( byref rc as RECT ) as long
    return rc.bottom - rc.top
end function

private function SelfTest_CenterOf( byref rc as RECT ) as POINT
    dim as POINT pt
    pt.x = rc.left + (RectW(rc) \ 2)
    pt.y = rc.top + (RectH(rc) \ 2)
    return pt
end function


sub PsNumericUpDown_SelfTest()

    dim as string s
    print "---- PsNumericUpDown self-test ----"

    dim as HWND h = PsNumericUpDown_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPIN_TEST )
    PsNumericUpDown_SetFont( h, ghFont(GUIFONT_10) )
    PsNumericUpDown_SetValueChangedCallback( h, @PsNumericUpDown_ValueChangedCallback )

    dim as long bw = PsNumericUpDown_GetButtonWidth( h )
    dim as long bt = PsNumericUpDown_GetBorderThickness( h )
    dim as long dt = PsNumericUpDown_GetDividerThickness( h )
    dim as long padL, padR, padV
    PsNumericUpDown_GetValuePadding( h, padL, padR, padV )
    dim as long glyphL, glyphT
    PsNumericUpDown_GetGlyphSize( h, glyphL, glyphT )

    s = "defaults: buttonW=" & str(bw) & " border=" & str(bt) & " divider=" & str(dt) & _
        " pad=" & str(padL) & "/" & str(padR) & "/" & str(padV) & _
        " glyph=" & str(glyphL) & "x" & str(glyphT)
    print s

    ' ---- layout at a comfortable size ---------------------------------------------------
    dim as long cw, ch
    SelfTest_SizeTo( h, 200, 34, cw, ch )

    dim as RECT rcFrame, rcMinus, rcValue, rcPlus
    dim as boolean bGot = PsNumericUpDown_GetFrameRect( h, rcFrame )
    PsNumericUpDown_GetMinusRect( h, rcMinus )
    PsNumericUpDown_GetValueRect( h, rcValue )
    PsNumericUpDown_GetPlusRect( h, rcPlus )

    SelfTest_Check( "rect queries succeed once the control has geometry", bGot, "" )
    SelfTest_Check( "rcFrame is the whole client", _
        (rcFrame.left = 0) andalso (rcFrame.top = 0) andalso (RectW(rcFrame) = cw) andalso (RectH(rcFrame) = ch), _
        "frame=" & str(RectW(rcFrame)) & "x" & str(RectH(rcFrame)) & " client=" & str(cw) & "x" & str(ch) )

    SelfTest_Check( "the minus cell starts at the client edge and is buttonWidth wide", _
        (rcMinus.left = 0) andalso (RectW(rcMinus) = bw), _
        "left=" & str(rcMinus.left) & " w=" & str(RectW(rcMinus)) )
    SelfTest_Check( "the plus cell ends at the client edge and is buttonWidth wide", _
        (rcPlus.right = cw) andalso (RectW(rcPlus) = bw), _
        "right=" & str(rcPlus.right) & " w=" & str(RectW(rcPlus)) )

    ' The cells are NOT deflated by the border: the outline is stroked OVER their outer
    ' edges, which is what lets the button fills reach the rounded corners.
    SelfTest_Check( "the button cells span the full client height (the border strokes over them)", _
        (rcMinus.top = 0) andalso (rcMinus.bottom = ch) andalso (rcPlus.top = 0) andalso (rcPlus.bottom = ch), _
        "minus=" & str(rcMinus.top) & ".." & str(rcMinus.bottom) & " ch=" & str(ch) )

    ' The value cell IS deflated vertically -- the child window covers those pixels. The inset
    ' is the border (so the child cannot cover the frame's rows) PLUS nVertPadding (DPI-scaled
    ' clearance, so the borderless RichEdit's full-height line does not clip against the frame).
    ' At this comfortable height the padding fits, so the fallback branch is not exercised.
    dim as long vpL, vpR, vpV
    PsNumericUpDown_GetValuePadding( h, vpL, vpR, vpV )
    SelfTest_Check( "the value cell is inset vertically by border + vertical padding, clearing the child of the frame", _
        (rcValue.top = bt + vpV) andalso (rcValue.bottom = ch - bt - vpV), _
        "value=" & str(rcValue.top) & ".." & str(rcValue.bottom) & " border=" & str(bt) & " vpad=" & str(vpV) )

    SelfTest_Check( "the value cell sits exactly between the two dividers", _
        (rcValue.left = rcMinus.right + dt) andalso (rcValue.right = rcPlus.left - dt), _
        "value=" & str(rcValue.left) & ".." & str(rcValue.right) )

    SelfTest_Check( "cells and dividers tile the client width exactly, no gap and no overlap", _
        (bw + dt + RectW(rcValue) + dt + bw) = cw, _
        "sum=" & str(bw + dt + RectW(rcValue) + dt + bw) & " client=" & str(cw) )

    ' ---- the glyphs ---------------------------------------------------------------------
    dim as PSNUMERICUPDOWN ptr pTest = GetNumericUpDownPointer( h )
    dim as RECT rcMinusBar = pTest->rcMinusBar
    dim as RECT rcPlusBarH = pTest->rcPlusBarH
    dim as RECT rcPlusBarV = pTest->rcPlusBarV

    SelfTest_Check( "the minus bar is glyphLength x glyphThickness", _
        (RectW(rcMinusBar) = glyphL) andalso (RectH(rcMinusBar) = glyphT), _
        "bar=" & str(RectW(rcMinusBar)) & "x" & str(RectH(rcMinusBar)) )
    SelfTest_Check( "the plus vertical bar is glyphThickness x glyphLength", _
        (RectW(rcPlusBarV) = glyphT) andalso (RectH(rcPlusBarV) = glyphL), _
        "bar=" & str(RectW(rcPlusBarV)) & "x" & str(RectH(rcPlusBarV)) )

    dim as POINT ptMinusCell = SelfTest_CenterOf( rcMinus )
    dim as POINT ptMinusBar  = SelfTest_CenterOf( rcMinusBar )
    SelfTest_Check( "the minus bar is centred in its cell (both axes, to within rounding)", _
        (abs(ptMinusBar.x - ptMinusCell.x) <= 1) andalso (abs(ptMinusBar.y - ptMinusCell.y) <= 1), _
        "bar=" & str(ptMinusBar.x) & "," & str(ptMinusBar.y) & " cell=" & str(ptMinusCell.x) & "," & str(ptMinusCell.y) )

    ' Both plus bars are centred on the SAME cell centre rather than one derived from the
    ' other, so the cross is symmetric whatever the parity of the cell and the bar.
    dim as POINT ptBarH = SelfTest_CenterOf( rcPlusBarH )
    dim as POINT ptBarV = SelfTest_CenterOf( rcPlusBarV )
    SelfTest_Check( "the two plus bars share one centre (the cross is symmetric)", _
        (abs(ptBarH.x - ptBarV.x) <= 1) andalso (abs(ptBarH.y - ptBarV.y) <= 1), _
        "H=" & str(ptBarH.x) & "," & str(ptBarH.y) & " V=" & str(ptBarV.x) & "," & str(ptBarV.y) )

    ' ---- the child sits exactly on the value cell ---------------------------------------
    dim as HWND hBox = PsNumericUpDown_GetTextBoxHandle( h )
    SelfTest_Check( "the embedded PsTextBox exists and is our child", _
        (hBox <> 0) andalso (GetParent(hBox) = h), "" )
    SelfTest_Check( "the embedded PsTextBox is borderless (this control owns all the chrome)", _
        PsTextBox_GetBorderWidth( hBox ) = 0, "width=" & str(PsTextBox_GetBorderWidth(hBox)) )
    SelfTest_Check( "the embedded PsTextBox is in numeric mode", _
        PsTextBox_GetNumericMode( hBox ) = true, "" )

    ' ---- the number is centred, and stays centred -----------------------------------------
    ' This used to be the one thing on the "only a human can confirm it" list, because the
    ' control sent EM_SETPARAFORMAT itself and had no way to check the result. It now goes
    ' through PsTextBox_SetTextAlign, and the RichEdit can simply be ASKED -- which is the
    ' only witness worth asking, since the earlier version was silently doing nothing.
    SelfTest_Check( "the value field is set to centre alignment", _
        PsTextBox_GetTextAlign( hBox ) = TXT_ALIGN_CENTER, _
        "align=" & str(PsTextBox_GetTextAlign(hBox)) )

    dim pfNum as PARAFORMAT
    pfNum.cbSize = sizeof(PARAFORMAT)
    RichEdit_GetParaFormat( PsTextBox_GetRichEditHandle(hBox), @pfNum )
    SelfTest_Check( "the RICHEDIT really is centred (asked, not assumed)", _
        pfNum.wAlignment = PFA_CENTER, _
        "wAlignment=" & str(pfNum.wAlignment) & " PFA_CENTER=" & str(PFA_CENTER) )

    ' Every value write replaces the whole buffer, so this is the assertion that the centring
    ' is not quietly lost the first time the user presses "+".
    PsNumericUpDown_SetValue( h, 1234.0 )
    pfNum.cbSize = sizeof(PARAFORMAT)
    RichEdit_GetParaFormat( PsTextBox_GetRichEditHandle(hBox), @pfNum )
    SelfTest_Check( "the centring SURVIVES a value change", _
        pfNum.wAlignment = PFA_CENTER, "wAlignment=" & str(pfNum.wAlignment) )

    dim as RECT rcChild
    GetWindowRect( hBox, @rcChild )
    MapWindowPoints( HWND_DESKTOP, h, cast(POINT ptr, @rcChild), 2 )
    SelfTest_Check( "the child is positioned exactly on rcValue", _
        EqualRect( @rcChild, @rcValue ) <> 0, _
        "child=" & str(rcChild.left) & "," & str(rcChild.top) & "," & str(rcChild.right) & "," & str(rcChild.bottom) & _
        " value=" & str(rcValue.left) & "," & str(rcValue.top) & "," & str(rcValue.right) & "," & str(rcValue.bottom) )

    ' ---- hit testing agrees with the layout ---------------------------------------------
    SelfTest_Check( "HitTest finds the minus cell at its centre", _
        PsNumericUpDown_HitTest( h, SelfTest_CenterOf(rcMinus) ) = NUD_PART_MINUS, "" )
    SelfTest_Check( "HitTest finds the plus cell at its centre", _
        PsNumericUpDown_HitTest( h, SelfTest_CenterOf(rcPlus) ) = NUD_PART_PLUS, "" )
    SelfTest_Check( "HitTest finds the value cell at its centre", _
        PsNumericUpDown_HitTest( h, SelfTest_CenterOf(rcValue) ) = NUD_PART_VALUE, "" )
    dim as POINT ptOutside
    ptOutside.x = cw + 20 : ptOutside.y = 5
    SelfTest_Check( "HitTest returns NONE outside the client", _
        PsNumericUpDown_HitTest( h, ptOutside ) = NUD_PART_NONE, "" )

    ' ---- the thickness scaling asymmetry -------------------------------------------------
    ' Border and divider are RULES and are never DPI-scaled; the glyph bars are the strokes of
    ' an ICON and are. Pinned here because the asymmetry looks like an oversight to anyone
    ' reading the Create defaults, and "harmonising" it in either direction is a regression:
    ' scale the rules and hairlines thicken with the display, unscale the glyph and the minus
    ' sign becomes a thread at high DPI (which is what a screenshot caught it doing).
    '
    ' On a 96-DPI display every ScaleY(n) = n, so this passes trivially there and only
    ' discriminates on a scaled one. That is worth stating rather than hiding: the assertion
    ' is a guard against an edit, not a proof about this machine.
    dim as CWindow ptr pTestWin = AfxCWindowPtr( h )
    dim as long nScaledOne = iif( pTestWin, pTestWin->ScaleY(CNUD_DEFAULT_GLYPHTHICK), 1 )
    if nScaledOne < 1 then nScaledOne = 1
    PsNumericUpDown_GetGlyphSize( h, glyphL, glyphT )
    SelfTest_Check( "the glyph thickness IS DPI-scaled (it is an icon stroke)", _
        glyphT = nScaledOne, "glyphThick=" & str(glyphT) & " expected=" & str(nScaledOne) )
    SelfTest_Check( "the border and divider are NOT DPI-scaled (they are rules)", _
        (PsNumericUpDown_GetBorderThickness(h) = CNUD_DEFAULT_BORDERTHICK) andalso _
        (PsNumericUpDown_GetDividerThickness(h) = CNUD_DEFAULT_DIVIDERTHICK), _
        "border=" & str(PsNumericUpDown_GetBorderThickness(h)) & _
        " divider=" & str(PsNumericUpDown_GetDividerThickness(h)) )

    ' ---- the custom painter actually draws something ------------------------------------
    ' Drives the demo's own paint callback offscreen and counts colours in the minus cell.
    ' ONE colour means the cell was flooded -- see SelfTest_MinusCellTones for the whole
    ' story. This is the only assertion in the file that looks at pixels, and it exists
    ' because the defect it catches is invisible to every assertion that looks at numbers.
    dim as long nTones = SelfTest_MinusCellTones( h )
    SelfTest_Check( "the custom painter's frame does not flood the button cells", _
        nTones > 1, "distinct colours in the minus cell = " & str(nTones) & _
                    "  (1 = flooded; a drawn cell has a fill AND a glyph)" )

    ' ---- overflow: honest and clipped, never squeezed ------------------------------------
    SelfTest_SizeTo( h, (bw * 2) + (dt * 2) - 10, 34, cw, ch )
    PsNumericUpDown_GetMinusRect( h, rcMinus )
    PsNumericUpDown_GetValueRect( h, rcValue )
    PsNumericUpDown_GetPlusRect( h, rcPlus )
    SelfTest_Check( "too narrow: the buttons keep their width (honest, not squeezed)", _
        (RectW(rcMinus) = bw) andalso (RectW(rcPlus) = bw), _
        "minus=" & str(RectW(rcMinus)) & " plus=" & str(RectW(rcPlus)) )
    SelfTest_Check( "too narrow: the value cell collapses to zero and never inverts", _
        RectW(rcValue) = 0, "valueW=" & str(RectW(rcValue)) )

    ' ---- the ideal size ------------------------------------------------------------------
    SelfTest_SizeTo( h, 200, 34, cw, ch )
    ' A range whose MINIMUM is the wider string ("-1000.00" against "50.00"), so the
    ' widest-end assertion below is actually load-bearing rather than accidentally true.
    PsNumericUpDown_SetRange( h, -1000.0, 50.0 )
    PsNumericUpDown_SetDecimalPlaces( h, 2 )

    ' Measured independently of the control, with the same font: the assertion is that the
    ' control's arithmetic matches an oracle, not that it matches itself.
    dim as HDC hdcM = GetDC( h )
    dim as HFONT hOldM = cast( HFONT, SelectObject( hdcM, ghFont(GUIFONT_10) ) )
    dim as DWSTRING wszLo = format( -1000.0, "0.00" )
    dim as DWSTRING wszHi = format( 50.0, "0.00" )
    dim as SIZE szLo, szHi
    GetTextExtentPoint32W( hdcM, wszLo.vptr, len(wszLo), @szLo )
    GetTextExtentPoint32W( hdcM, wszHi.vptr, len(wszHi), @szHi )
    dim as TEXTMETRICW tmM
    GetTextMetricsW( hdcM, @tmM )
    SelectObject( hdcM, hOldM )
    ReleaseDC( h, hdcM )
    dim as long widest = szLo.cx
    if szHi.cx > widest then widest = szHi.cx

    dim as long iw, ih
    PsNumericUpDown_GetIdealSize( h, iw, ih )
    SelfTest_Check( "the ideal width = both buttons + both dividers + padding + the widest value", _
        iw = (bw * 2) + (dt * 2) + padL + padR + widest, _
        "ideal=" & str(iw) & " expected=" & str((bw * 2) + (dt * 2) + padL + padR + widest) )
    SelfTest_Check( "the ideal height = one line of text + vertical padding + both borders", _
        ih = tmM.tmHeight + (padV * 2) + (bt * 2), _
        "ideal=" & str(ih) & " expected=" & str(tmM.tmHeight + (padV * 2) + (bt * 2)) )
    SelfTest_Check( "the widest value is measured at BOTH ends of the range, not just the max", _
        (widest = szLo.cx) andalso (szLo.cx > szHi.cx), _
        "lo=" & str(szLo.cx) & " hi=" & str(szHi.cx) & " (-1000.00 is wider than 50.00)" )

    ' Valid BEFORE the control has ever been sized -- a host calls it precisely to decide how
    ' big to make the thing.
    dim as HWND hFresh = PsNumericUpDown_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPIN_TEST )
    PsNumericUpDown_SetFont( hFresh, ghFont(GUIFONT_10) )
    dim as long fw, fh
    PsNumericUpDown_GetIdealSize( hFresh, fw, fh )
    SelfTest_Check( "GetIdealSize is valid on a control that has never been sized", _
        (fw > 0) andalso (fh > 0), "ideal=" & str(fw) & "x" & str(fh) )
    DestroyWindow( hFresh )

    ' ---- value arithmetic -----------------------------------------------------------------
    PsNumericUpDown_SetRange( h, 0.0, 10.0 )
    PsNumericUpDown_SetDecimalPlaces( h, 2 )
    PsNumericUpDown_SetIncrement( h, 0.1 )

    PsNumericUpDown_SetValue( h, 99.0 )
    SelfTest_Check( "SetValue clamps above the maximum", _
        PsNumericUpDown_GetValue(h) = 10.0, "value=" & str(PsNumericUpDown_GetValue(h)) )
    PsNumericUpDown_SetValue( h, -99.0 )
    SelfTest_Check( "SetValue clamps below the minimum", _
        PsNumericUpDown_GetValue(h) = 0.0, "value=" & str(PsNumericUpDown_GetValue(h)) )

    ' THE DECIMAL-GRID TEST. Adding 0.1 ten times to a binary double lands on
    ' 0.9999999999999999 unless every step is snapped back onto the grid -- and a value a
    ' hair below the grid formats correctly while comparing wrong, so the drift stays
    ' invisible until a range check disagrees with the number on screen.
    PsNumericUpDown_SetValue( h, 0.0 )
    for i as long = 1 to 10
        PsNumericUpDown_StepUp( h )
    next
    SelfTest_Check( "0.1 stepped ten times lands EXACTLY on 1.00 (no floating-point drift)", _
        PsNumericUpDown_GetValue(h) = 1.0, _
        "value=" & str(PsNumericUpDown_GetValue(h)) & " diff=" & str(PsNumericUpDown_GetValue(h) - 1.0) )

    PsNumericUpDown_SetValue( h, 10.0 )
    PsNumericUpDown_StepUp( h )
    SelfTest_Check( "StepUp at the maximum does not exceed it", _
        PsNumericUpDown_GetValue(h) = 10.0, "value=" & str(PsNumericUpDown_GetValue(h)) )
    PsNumericUpDown_SetValue( h, 0.0 )
    PsNumericUpDown_StepDown( h )
    SelfTest_Check( "StepDown at the minimum does not go below it", _
        PsNumericUpDown_GetValue(h) = 0.0, "value=" & str(PsNumericUpDown_GetValue(h)) )

    ' ---- the setters are silent ------------------------------------------------------------
    ' Everything above went through SetValue / StepUp / StepDown / SetRange / SetDecimalPlaces
    ' and not one of them may have notified.
    SelfTest_Check( "every programmatic setter so far has been SILENT", _
        gChangeCount = 0, "callbacks=" & str(gChangeCount) )

    ' ---- range validation ------------------------------------------------------------------
    PsNumericUpDown_SetRange( h, 80.0, 20.0 )
    dim as double nLo, nHi
    PsNumericUpDown_GetRange( h, nLo, nHi )
    SelfTest_Check( "SetRange swaps an inverted pair rather than accepting it", _
        (nLo = 20.0) andalso (nHi = 80.0), "range=" & str(nLo) & ".." & str(nHi) )
    SelfTest_Check( "SetRange re-clamps the current value into the new range", _
        PsNumericUpDown_GetValue(h) = 20.0, "value=" & str(PsNumericUpDown_GetValue(h)) )

    PsNumericUpDown_SetRange( h, 0.0, 100.0 )
    PsNumericUpDown_SetDecimalPlaces( h, 2 )
    PsNumericUpDown_SetValue( h, 16.25 )
    PsNumericUpDown_SetDecimalPlaces( h, 0 )
    SelfTest_Check( "SetDecimalPlaces re-snaps the current value onto the new grid", _
        PsNumericUpDown_GetValue(h) = 16.0, "value=" & str(PsNumericUpDown_GetValue(h)) )
    SelfTest_Check( "SetDecimalPlaces reaches the embedded PsTextBox too", _
        PsTextBox_GetDecimalPlaces( hBox ) = 0, "places=" & str(PsTextBox_GetDecimalPlaces(hBox)) )

    ' ---- the wheel ---------------------------------------------------------------------
    ' Driven by a real WM_MOUSEWHEEL, because that is genuinely how a hover-wheel arrives
    ' (Windows' "scroll inactive windows" delivers it to the window under the cursor). No
    ' attempt is made to fake a CLICK -- Learnings.md is explicit that a SendMessage-simulated
    ' click cannot reproduce mouse capture, so that stays in the interactive pass.
    PsNumericUpDown_SetIncrement( h, 1.0 )
    PsNumericUpDown_SetValue( h, 50.0 )
    gChangeCount = 0

    dim as WPARAM wpHalf = cast( WPARAM, cast(ulongint, 60) shl 16 )
    SendMessage( h, WM_MOUSEWHEEL, wpHalf, 0 )
    SelfTest_Check( "half a wheel notch does not move the value (sub-notch accumulation)", _
        (PsNumericUpDown_GetValue(h) = 50.0) andalso (gChangeCount = 0), _
        "value=" & str(PsNumericUpDown_GetValue(h)) & " callbacks=" & str(gChangeCount) )

    SendMessage( h, WM_MOUSEWHEEL, wpHalf, 0 )
    SelfTest_Check( "the second half completes a notch and steps by one increment", _
        PsNumericUpDown_GetValue(h) = 51.0, "value=" & str(PsNumericUpDown_GetValue(h)) )
    SelfTest_Check( "a wheel step NOTIFIES (it is user interaction, unlike StepUp)", _
        gChangeCount = 1, "callbacks=" & str(gChangeCount) )

    dim as WPARAM wpDown = cast( WPARAM, cast(ulongint, cast(ushort, -120)) shl 16 )
    SendMessage( h, WM_MOUSEWHEEL, wpDown, 0 )
    SelfTest_Check( "a negative wheel delta steps DOWN", _
        PsNumericUpDown_GetValue(h) = 50.0, "value=" & str(PsNumericUpDown_GetValue(h)) )

    ' ---- enable / disable ----------------------------------------------------------------
    PsNumericUpDown_SetEnabled( h, false )
    SelfTest_Check( "SetEnabled(false) disables the container WINDOW", _
        (IsWindowEnabled(h) = 0) andalso (PsNumericUpDown_GetEnabled(h) = false), _
        "IsWindowEnabled=" & str(IsWindowEnabled(h)) )
    SelfTest_Check( "SetEnabled(false) disables the PsTextBox too (no SetFocus can sneak in)", _
        IsWindowEnabled(hBox) = 0, "IsWindowEnabled=" & str(IsWindowEnabled(hBox)) )
    dim as double nHeld = PsNumericUpDown_GetValue(h)
    PsNumericUpDown_SetEnabled( h, true )
    SelfTest_Check( "SetEnabled(true) re-enables both windows and keeps the value", _
        (IsWindowEnabled(h) <> 0) andalso (IsWindowEnabled(hBox) <> 0) andalso _
        (PsNumericUpDown_GetValue(h) = nHeld), "value=" & str(PsNumericUpDown_GetValue(h)) )

    ' ---- read-only -------------------------------------------------------------------------
    PsNumericUpDown_SetReadOnly( h, true )
    SelfTest_Check( "SetReadOnly reaches the embedded PsTextBox", _
        (PsNumericUpDown_GetReadOnly(h) = true) andalso (PsTextBox_GetReadOnly(hBox) = true), "" )
    gChangeCount = 0
    SendMessage( h, WM_MOUSEWHEEL, cast( WPARAM, cast(ulongint, 120) shl 16 ), 0 )
    SelfTest_Check( "read-only stops TYPING only -- the wheel still steps the value", _
        gChangeCount = 1, "callbacks=" & str(gChangeCount) )
    PsNumericUpDown_SetReadOnly( h, false )

    ' ---- input validation --------------------------------------------------------------
    PsNumericUpDown_SetIncrement( h, -5.0 )
    SelfTest_Check( "SetIncrement takes the magnitude of a negative increment", _
        PsNumericUpDown_GetIncrement(h) = 5.0, "increment=" & str(PsNumericUpDown_GetIncrement(h)) )
    PsNumericUpDown_SetDecimalPlaces( h, -3 )
    SelfTest_Check( "SetDecimalPlaces floors a negative place count at 0", _
        PsNumericUpDown_GetDecimalPlaces(h) = 0, "places=" & str(PsNumericUpDown_GetDecimalPlaces(h)) )
    PsNumericUpDown_SetGlyphSize( h, 0, 0 )
    PsNumericUpDown_GetGlyphSize( h, glyphL, glyphT )
    SelfTest_Check( "SetGlyphSize floors a degenerate glyph at 1x1 rather than drawing nothing", _
        (glyphL = 1) andalso (glyphT = 1), "glyph=" & str(glyphL) & "x" & str(glyphT) )

    DestroyWindow( h )

    s = "---- PsNumericUpDown self-test: " & str(gTestCount) & " assertions, " & str(gFailCount) & " failed ----"
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
            100, 100, 660, 460 _
            )

    gRows(0).wszLabel = "Font size   (integers, 6..72)"
    gRows(1).wszLabel = "Line height   (2 decimals, step 0.25)"
    gRows(2).wszLabel = "Opacity %   (custom colours, PgUp/PgDn = 25)"
    gRows(3).wszLabel = "Retries   (1..3, starts AT the max -- custom painter)"
    gRows(4).wszLabel = "Zoom %   (disabled)"
    gRows(5).wszLabel = "Preset   (read-only: buttons yes, typing no)"

    for i as long = 0 to SPIN_COUNT - 1
        ghSpin(i) = PsNumericUpDown_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPIN_FIRST + i )
        PsNumericUpDown_SetFont( ghSpin(i), ghFont(GUIFONT_10) )
        PsNumericUpDown_SetMessageCallback( ghSpin(i), @PsNumericUpDown_MessageCallback )
        PsNumericUpDown_SetValueChangedCallback( ghSpin(i), @PsNumericUpDown_ValueChangedCallback )
    next

    ' Row 0 -- the screenshot's case: an integer font size.
    PsNumericUpDown_SetRange( ghSpin(0), 6.0, 72.0 )
    PsNumericUpDown_SetValue( ghSpin(0), 16.0 )

    ' Row 1 -- two decimals and a fractional step, which is the decimal-grid path.
    PsNumericUpDown_SetDecimalPlaces( ghSpin(1), 2 )
    PsNumericUpDown_SetRange( ghSpin(1), 0.5, 3.0 )
    PsNumericUpDown_SetIncrement( ghSpin(1), 0.25 )
    PsNumericUpDown_SetLargeIncrement( ghSpin(1), 1.0 )
    PsNumericUpDown_SetValue( ghSpin(1), 1.5 )

    ' Row 2 -- custom colours, and a large increment far bigger than the small one.
    dim as PSNUMERICUPDOWN_COLORS clrs
    PsNumericUpDown_GetColors( ghSpin(2), @clrs )
    clrs.ButtonBackColorHot     = BGR( 62,140, 90)
    clrs.ButtonBackColorPressed = BGR( 42,100, 64)
    clrs.FocusBorderColor       = BGR(120,200,150)
    clrs.GlyphColorHot          = BGR(255,255,255)
    PsNumericUpDown_SetColors( ghSpin(2), @clrs )
    PsNumericUpDown_SetRange( ghSpin(2), 0.0, 100.0 )
    PsNumericUpDown_SetIncrement( ghSpin(2), 5.0 )
    PsNumericUpDown_SetLargeIncrement( ghSpin(2), 25.0 )
    PsNumericUpDown_SetValue( ghSpin(2), 80.0 )

    ' Row 3 -- a range narrow enough to sit against a limit on screen, so the "+" button is
    ' drawn disabled from the moment the window opens. The custom painter draws that greying
    ' itself, from p->bAtMax.
    PsNumericUpDown_SetPaintCallback( ghSpin(3), @PsNumericUpDown_FlatPaintCallback )
    PsNumericUpDown_SetRange( ghSpin(3), 1.0, 3.0 )
    PsNumericUpDown_SetValue( ghSpin(3), 3.0 )

    ' Row 4 -- disabled outright: greyed, no hover, no typing, no stepping.
    PsNumericUpDown_SetRange( ghSpin(4), 25.0, 400.0 )
    PsNumericUpDown_SetValue( ghSpin(4), 100.0 )
    PsNumericUpDown_SetEnabled( ghSpin(4), false )

    ' Row 5 -- read-only: the buttons and the wheel still work, typing does not.
    PsNumericUpDown_SetRange( ghSpin(5), 1.0, 5.0 )
    PsNumericUpDown_SetValue( ghSpin(5), 2.0 )
    PsNumericUpDown_SetReadOnly( ghSpin(5), true )

    ' A plain PsTextBox after the spinners. Tab must reach it THROUGH the extra container
    ' level each spinner adds -- which is the whole reason PsNumericUpDown's container carries
    ' WS_EX_CONTROLPARENT.
    ghPlainBox = PsTextBox_Create( HWND_FRMMAIN, IDC_FRMMAIN_TEXTBOX )
    PsTextBox_SetFont( ghPlainBox, ghFont(GUIFONT_10) )
    PsTextBox_SetForeColor( ghPlainBox, theme.ForeColor )
    PsTextBox_SetBackColor( ghPlainBox, theme.BackColorHot )
    PsTextBox_SetBorderColor( ghPlainBox, theme.Divider )
    PsTextBox_SetFocusBorderColor( ghPlainBox, theme.FocusAccent )
    PsTextBox_SetOuterBackColor( ghPlainBox, theme.BackColor )
    PsTextBox_SetCueBannerText( ghPlainBox, "ordinary text" )
    PsTextBox_SetCueBannerColor( ghPlainBox, theme.ForeColorDisabled )

    dim as long iw, ih
    PsNumericUpDown_GetIdealSize( ghSpin(0), iw, ih )
    dim as string s = "row 0 ideal size: " & str(iw) & "x" & str(ih)
    print s

    frmMain_PositionControls( HWND_FRMMAIN )
    for i as long = 0 to SPIN_COUNT - 1
        ShowWindow( ghSpin(i), SW_SHOW )
    next
    ShowWindow( ghPlainBox, SW_SHOW )

    if environ("PSNUMERICUPDOWN_SELFTEST") = "1" then PsNumericUpDown_SelfTest()

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Process windows events
    dim uMsg as MSG

    ' Message loop.
    '
    ' TWO THINGS TO NOTICE HERE.
    '
    ' PsNumericUpDown_FilterMessage is MANDATORY, not decoration: the value field carries
    ' PsTextBox's right-click menu, which is a PsPopupMenu and therefore not modal -- keyboard
    ' navigation and click-outside dismissal both live in that filter.
    '
    ' There is deliberately NO IsDialogMessage call. PsTextBox handles VK_TAB itself, and
    ' leaving the dialog manager out is what proves it still works through the extra
    ' container level this control adds. A host that does run IsDialogMessage is fine too.
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        if PsNumericUpDown_FilterMessage( @uMsg ) then continue do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
            TranslateMessage @uMsg    ' Translates virtual-key messages into character messages.
            DispatchMessage @uMsg     ' Dispatches a message to a window procedure.
        end if  ' accelerators
    loop  ' message loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

end function
