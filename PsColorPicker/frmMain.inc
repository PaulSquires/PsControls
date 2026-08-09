'    PsColorPicker - reusable owner-drawn modal colour picker popup
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


dim shared as HWND ghBtnPick(0 to ROW_COUNT - 1)
dim shared as HWND ghBtnClose

' THE HOST'S MODEL, and it is the whole of what a host needs now: a colour and an alpha per row.
' There is no picker handle to keep, because a popup that has been answered no longer exists.
dim shared as COLORREF gRowColor(0 to ROW_COUNT - 1)
dim shared as ubyte    gRowAlpha(0 to ROW_COUNT - 1)
dim shared as RECT     gRowSwatch(0 to ROW_COUNT - 1)

dim shared as DWSTRING gStatusLine
dim shared as DWSTRING gPhaseLine
dim shared as long     gMoveCount

dim shared as DWSTRING gHeading( 0 to ROW_COUNT - 1 )
dim shared as DWSTRING gNote( 0 to ROW_COUNT - 1 )


private function frmMain_ClrToStr( byval clr as COLORREF, byval nAlpha as ubyte ) as DWSTRING
    dim as DWSTRING wszOut = str( clng(clr) and &hFF ) & "; " & _
                             str( (clng(clr) shr 8) and &hFF ) & "; " & _
                             str( (clng(clr) shr 16) and &hFF ) & _
                             "   " & PsColorPicker_FormatHex( clr ) & _
                             "   alpha " & str( clng(nAlpha) )
    return wszOut
end function


' ========================================================================================
' The colour callback.
'
' IT STILL MATTERS IN A MODAL PICKER, which is worth saying because it looks as though it might
' not: DoModal reports the final answer on its own, so a host that only wants the result needs no
' callback at all. This one is here for the OTHER use -- LIVE PREVIEW. The phases are what make
' that possible: coalesce the MOVEs while the user drags, commit on the END.
'
' IT IS ONLY REACHABLE THROUGH Create/RunModal, not through DoModal, because DoModal has no room
' in its signature to hand you a callback. Row 2 is the row that uses it.
' ========================================================================================
private sub frmMain_ColorChanged( _
            byval hCtrl  as HWND, _
            byval clr    as COLORREF, _
            byval nAlpha as ubyte, _
            byval nPhase as long _
            )

    dim as DWSTRING wszPhase
    select case nPhase
    case CLR_PHASE_BEGIN : wszPhase = "BEGIN" : gMoveCount = 0
    case CLR_PHASE_MOVE  : wszPhase = "MOVE"  : gMoveCount += 1
    case CLR_PHASE_END   : wszPhase = "END"
    end select

    gStatusLine = frmMain_ClrToStr( clr, nAlpha )
    gPhaseLine  = "live preview: " & wszPhase & "   (moves in this drag: " & str(gMoveCount) & ")"

    ' The host repaints ITSELF, not the picker. This is the shape a live preview takes: the
    ' callback updates a model and the window that renders from it invalidates.
    InvalidateRect( HWND_FRMMAIN, NULL, TRUE )
    UpdateWindow( HWND_FRMMAIN )
end sub


private sub frmMain_TabChanged( byval hCtrl as HWND, byval nTab as long )
    gPhaseLine = "tab -> " & PsColorPicker_TabName( nTab )
    InvalidateRect( HWND_FRMMAIN, NULL, TRUE )
end sub


' ========================================================================================
' A HOST PAINT CALLBACK THAT DECORATES RATHER THAN REPLACES.
'
' PsColorPicker_RenderInfo runs the built-in painter into the buffer we were handed, and then we
' add one thing on top. Every other control in this family makes a paint callback all-or-nothing,
' which is a fair trade when the built-in painter is a circle and a caption; here it would mean
' re-implementing three tabs, 224 cells, the preview, four entry fields and the OK button before
' you could change the one thing you cared about.
'
' NOTE WHICH PRIMITIVE THE FRAME USES. PaintRoundOutline STROKES; PaintBorderRect FILLS and then
' strokes, so used here it would erase the entire matrix underneath. That is the mistake this
' family has made four separate times, and frmMain_Show below ASSERTS it has not happened here
' rather than trusting the render to look right.
' ========================================================================================
private sub frmMain_PickerPaint( byval p as any ptr )
    if p = 0 then exit sub
    dim as PSCOLORPICKER_PAINTINFO ptr pi = cast( PSCOLORPICKER_PAINTINFO ptr, p )
    if pi->b = 0 then exit sub

    PsColorPicker_RenderInfo( p )

    dim as RECT rc = pi->rcBody
    InflateRect( @rc, 2, 2 )
    pi->b->SetPenColor( theme.FocusAccent )
    pi->b->PaintRoundOutline( @rc, 6, 1 )
end sub


' ========================================================================================
' OPENING THE POPUP -- the whole integration, three ways.
'
' Rows 0 and 1 are ONE CALL each. Row 2 goes the long way round (Create, configure, place, run,
' destroy) purely because it wants a paint callback, which DoModal has nowhere to take.
' ========================================================================================
private sub frmMain_PickRow( byval hwnd as HWND, byval nRow as long )

    if (nRow < 0) orelse (nRow >= ROW_COUNT) then exit sub

    dim as boolean bOK = false

    if nRow < 2 then
        ' ANCHORED ON THE BUTTON THAT OPENED IT, in SCREEN coordinates -- the dropdown shape.
        dim as RECT rcAnchor
        GetWindowRect( ghBtnPick(nRow), @rcAnchor )

        ' Row 0 hides the alpha row, which the popup shows by default. That is reachable only
        ' through the long form, so row 0 uses it for that one setter and row 1 -- which wants
        ' the default -- is the one that gets to be a single line.
        if nRow = 0 then
            dim as HWND hPick = PsColorPicker_Create( hwnd, 0 )
            if hPick = 0 then exit sub
            PsColorPicker_SetFont( hPick, ghFont(GUIFONT_10) )
            PsColorPicker_ShowAlpha( hPick, false )
            PsColorPicker_SetColor( hPick, gRowColor(nRow), gRowAlpha(nRow), false )
            PsColorPicker_SetTab( hPick, CLR_TAB_WEB )
            PsColorPicker_PositionForRect( hPick, rcAnchor )
            bOK = PsColorPicker_RunModal( hPick )
            if bOK then
                gRowColor(nRow) = PsColorPicker_GetColor( hPick )
                gRowAlpha(nRow) = PsColorPicker_GetAlpha( hPick )
            end if
            DestroyWindow( hPick )
        else
            ' THE ENTIRE INTEGRATION, for a host that wants the default popup. Note what a
            ' cancel does to gRowColor: nothing. The out-parameters are only written on OK, so
            ' the host's own variables can be passed straight in.
            bOK = PsColorPicker_DoModalForRect( hwnd, rcAnchor, gRowColor(nRow), gRowAlpha(nRow) )
        end if
    else
        ' CENTRED, and configured first -- the long form, for the paint and colour callbacks.
        dim as HWND hPick = PsColorPicker_Create( hwnd, 0 )
        if hPick = 0 then exit sub
        PsColorPicker_SetFont( hPick, ghFont(GUIFONT_10) )
        PsColorPicker_SetPaintCallback( hPick, @frmMain_PickerPaint )
        PsColorPicker_SetColorChangedCallback( hPick, @frmMain_ColorChanged )
        PsColorPicker_SetTabChangedCallback( hPick, @frmMain_TabChanged )
        PsColorPicker_SetColor( hPick, gRowColor(nRow), gRowAlpha(nRow), false )
        PsColorPicker_SetTab( hPick, CLR_TAB_CUSTOM )
        PsColorPicker_PositionCentered( hPick, hwnd )
        bOK = PsColorPicker_RunModal( hPick )
        if bOK then
            gRowColor(nRow) = PsColorPicker_GetColor( hPick )
            gRowAlpha(nRow) = PsColorPicker_GetAlpha( hPick )
        end if
        DestroyWindow( hPick )
    end if

    if bOK then
        gStatusLine = "row " & str(nRow) & " -> " & frmMain_ClrToStr( gRowColor(nRow), gRowAlpha(nRow) )
        gPhaseLine  = "DoModal returned TRUE"
    else
        gStatusLine = "row " & str(nRow) & " unchanged: " & _
                      frmMain_ClrToStr( gRowColor(nRow), gRowAlpha(nRow) )
        gPhaseLine  = "cancelled (Escape or a click outside) -- the host's variables were not touched"
    end if

    InvalidateRect( hwnd, NULL, TRUE )
    ' FOCUS MUST COME BACK. The popup took it and is now destroyed; without this the form has no
    ' focused child and the first Tab afterwards does nothing -- the same trap that made the
    ' embedded demo need a SetFocus at startup.
    SetFocus( ghBtnPick(nRow) )
end sub


' ========================================================================================
' Layout. Three rows, each a swatch, a value and a button.
' ========================================================================================
private sub frmMain_PositionRows( byval hwnd as HWND )

    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )

    dim as long nMargin = AfxScaleX( 24 )
    dim as long nTop    = AfxScaleY( 86 )
    dim as long nRowH   = AfxScaleY( 108 )
    dim as long nSwW    = AfxScaleX( 120 )
    dim as long nSwH    = AfxScaleY( 44 )
    dim as long nBtnW   = AfxScaleX( 130 )
    dim as long nBtnH   = AfxScaleY( 30 )

    for i as long = 0 to ROW_COUNT - 1
        dim as long y = nTop + (i * nRowH)
        SetRect( @gRowSwatch(i), nMargin, y, nMargin + nSwW, y + nSwH )
        SetWindowPos( ghBtnPick(i), 0, nMargin + nSwW + AfxScaleX(20), y + ((nSwH - nBtnH) \ 2), _
                      nBtnW, nBtnH, SWP_NOZORDER or SWP_NOACTIVATE )
    next

    SetWindowPos( ghBtnClose, 0, rcClient.right - nMargin - nBtnW, rcClient.bottom - AfxScaleY(46), _
                  nBtnW, nBtnH, SWP_NOZORDER or SWP_NOACTIVATE )
end sub


' ========================================================================================
' The form's own painting.
' ========================================================================================
private function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )

    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim as long nMargin = AfxScaleX( 24 )

    dim as RECT rcTitle
    SetRect( @rcTitle, nMargin, AfxScaleY(16), rcClient.right - nMargin, AfxScaleY(42) )
    b.SetFont( ghFont(GUIFONTBOLD_12) )
    b.SetColors( theme.ForeColorSelect, theme.BackColor )
    b.PaintText( "PsColorPicker  --  a MODAL POPUP. DoModal blocks, returns TRUE on OK, " & _
                 "and adds nothing to your message pump.", @rcTitle, DT_LEFT )

    dim as RECT rcSub
    SetRect( @rcSub, nMargin, AfxScaleY(44), rcClient.right - nMargin, AfxScaleY(70) )
    b.SetFont( ghFont(GUIFONT_9) )
    b.SetColors( theme.ForeColor, theme.BackColor )
    b.PaintText( "OK commits. Escape or a click anywhere outside cancels, and a cancel leaves " & _
                 "the host's variables untouched.", @rcSub, DT_LEFT )

    for i as long = 0 to ROW_COUNT - 1
        ' The swatch. Two rects rather than one when alpha is below 255, so the alpha the picker
        ' returned is VISIBLE rather than merely reported in text: the right half is the colour
        ' blended toward the form's background by its own alpha.
        dim as RECT rcS = gRowSwatch(i)
        b.SetBackColor( gRowColor(i) )
        b.PaintRect( @rcS )
        b.SetPenColor( theme.Divider )
        b.PaintRoundOutline( @rcS, 0, 1 )

        dim as RECT rcH
        SetRect( @rcH, gRowSwatch(i).left, gRowSwatch(i).bottom + AfxScaleY(6), _
                       rcClient.right - nMargin, gRowSwatch(i).bottom + AfxScaleY(26) )
        b.SetFont( ghFont(GUIFONTBOLD_10) )
        b.SetColors( theme.ForeColorSelect, theme.BackColor )
        b.PaintText( gHeading(i) & "   --   " & frmMain_ClrToStr( gRowColor(i), gRowAlpha(i) ), _
                     @rcH, DT_LEFT )

        ' THE NOTE RECT IS DEEP ENOUGH FOR THE LONGEST NOTE, and it is measured rather than
        ' guessed: DT_CALCRECT is run first and the drawing rect grown to whatever it asks for.
        ' DrawText truncates without complaining, no assertion can see it, and it is only visible
        ' by looking at the render.
        dim as RECT rcN
        SetRect( @rcN, gRowSwatch(i).left, rcH.bottom + AfxScaleY(2), _
                       rcClient.right - nMargin, rcH.bottom + AfxScaleY(2) )
        b.SetFont( ghFont(GUIFONT_9) )
        b.SetColors( theme.ForeColor, theme.BackColor )

        dim as RECT rcCalc = rcN
        rcCalc.bottom = rcCalc.top + AfxScaleY( 400 )
        b.PaintTextEx( gNote(i), @rcCalc, DT_LEFT or DT_TOP or DT_WORDBREAK or DT_CALCRECT )
        rcN.bottom = rcCalc.bottom

        ' PaintTextEx, not PaintText: PaintText forces DT_SINGLELINE and so structurally cannot
        ' wrap.
        b.PaintTextEx( gNote(i), @rcN, DT_LEFT or DT_TOP or DT_WORDBREAK )
    next

    ' ---- the status band -------------------------------------------------------------------
    dim as RECT rcDiv
    SetRect( @rcDiv, 0, rcClient.bottom - AfxScaleY(90), rcClient.right, rcClient.bottom - AfxScaleY(90) + 1 )
    b.SetBackColor( theme.Divider )
    b.PaintRect( @rcDiv )

    dim as RECT rcS1
    SetRect( @rcS1, nMargin, rcClient.bottom - AfxScaleY(82), rcClient.right - nMargin, rcClient.bottom - AfxScaleY(58) )
    b.SetFont( ghFont(GUIFONTBOLD_10) )
    b.SetColors( theme.ForeColorSelect, theme.BackColor )
    b.PaintText( gStatusLine, @rcS1, DT_LEFT )

    dim as RECT rcS2
    SetRect( @rcS2, nMargin, rcClient.bottom - AfxScaleY(58), rcClient.right - nMargin, rcClient.bottom - AfxScaleY(36) )
    b.SetFont( ghFont(GUIFONT_9) )
    b.SetColors( theme.ForeColor, theme.BackColor )
    b.PaintText( gPhaseLine, @rcS2, DT_LEFT )

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' frmMain window procedure
' ========================================================================================
function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

    case WM_PAINT
        return frmMain_OnPaint( hwnd )

    case WM_ERASEBKGND
        return true

    case WM_SIZE
        frmMain_PositionRows( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
        return 0

    case WM_COMMAND
        dim as long nId = loword(wParam)
        if (nId >= IDC_FRMMAIN_BTN_FIRST) andalso (nId < IDC_FRMMAIN_BTN_FIRST + ROW_COUNT) then
            frmMain_PickRow( hwnd, nId - IDC_FRMMAIN_BTN_FIRST )
            return 0
        end if
        if nId = IDC_FRMMAIN_BTN_CLOSE then
            SendMessageW( hwnd, WM_CLOSE, 0, 0 )
            return 0
        end if

    case WM_CLOSE
        DestroyWindow( hwnd )
        return 0

    case WM_DESTROY
        PostQuitMessage(0)
        return 0

    case WM_SYSCOMMAND
        if (wParam and &hFFF0) = SC_CLOSE then
            SendMessageW( hwnd, WM_CLOSE, 0, 0 )
            return 0
        end if

    case WM_NCDESTROY
        dim as CWindow ptr pWindow = AfxCWindowPtr( hwnd )
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow( APPCLASSNAME )

    ghFont(GUIFONT_9)       = pWindow->CreateFont( GUIFONT,  9 )
    ghFont(GUIFONT_10)      = pWindow->CreateFont( GUIFONT, 10 )
    ghFont(GUIFONTBOLD_10)  = pWindow->CreateFont( GUIFONT, 10, FW_BOLD )
    ghFont(GUIFONTBOLD_12)  = pWindow->CreateFont( GUIFONT, 12, FW_BOLD )

    HWND_FRMMAIN = pWindow->Create( hWndParent, APPNAME, @frmMain_WndProc, _
                                    100, 60, 900, 620 )
    pWindow->SetClientSize( 880, 580 )
    pWindow->Center()

    dim as long nDark = 1
    DwmSetWindowAttribute( HWND_FRMMAIN, 20, @nDark, sizeof(long) )   ' DWMWA_USE_IMMERSIVE_DARK_MODE

    gRowColor(0) = BGR(0, 0, 128)     : gRowAlpha(0) = 255
    gRowColor(1) = BGR( 97,175,239)   : gRowAlpha(1) = 180
    gRowColor(2) = BGR(152,195,121)   : gRowAlpha(2) = 255

    gHeading(0) = "Anchored, no alpha"
    gNote(0)    = "Opens UNDER its own button, opaque, on the Web tab -- a scrolling list of 137 " & _
                  "NAMED colours, opened on Navy at row 107, which the popup scrolled into view for " & _
                  "itself. Built the long way (Create / PositionForRect / RunModal) only because it " & _
                  "wants ShowAlpha(false); the popup shows alpha by default."
    gHeading(1) = "Anchored, the default"
    gNote(1)    = "ONE LINE: PsColorPicker_DoModalForRect( hwnd, rcAnchor, gRowColor(1), gRowAlpha(1) ). " & _
                  "Cancel it with Escape or a click outside and the swatch does not move -- the two " & _
                  "out-parameters are written only on OK, which is why a host can pass its live " & _
                  "variables straight in."
    gHeading(2) = "Centred, host-painted"
    gNote(2)    = "Centred on the form, with a paint callback that calls PsColorPicker_RenderInfo and " & _
                  "then adds an accent frame -- decorating rather than replacing. It also wires the " & _
                  "colour callback, so the status band below follows the drag live: one BEGIN, many " & _
                  "MOVEs, one END."

    gStatusLine = frmMain_ClrToStr( gRowColor(0), gRowAlpha(0) )
    gPhaseLine  = "press a button to open the popup"

    for i as long = 0 to ROW_COUNT - 1
        ghBtnPick(i) = CreateWindowExW( 0, "BUTTON", "Pick colour...", _
                                        WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                                        0, 0, 0, 0, HWND_FRMMAIN, _
                                        cast(HMENU, cast(UINT_PTR, IDC_FRMMAIN_BTN_FIRST + i)), _
                                        GetModuleHandleW(NULL), NULL )
        SendMessageW( ghBtnPick(i), WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), 1 )
    next

    ghBtnClose = CreateWindowExW( 0, "BUTTON", "Close", _
                                  WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                                  0, 0, 0, 0, HWND_FRMMAIN, _
                                  cast(HMENU, cast(UINT_PTR, IDC_FRMMAIN_BTN_CLOSE)), _
                                  GetModuleHandleW(NULL), NULL )
    SendMessageW( ghBtnClose, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), 1 )

    frmMain_PositionRows( HWND_FRMMAIN )

    ' ---- self-test ----------------------------------------------------------------------------
    PsColorPicker_RunSelfTest( HWND_FRMMAIN )

    ' HOST-SIDE probe of the demo's OWN paint callback, gated on the same env var. This is the
    ' assertion PsColorPicker_CountRenderedTones was made public for: every occurrence of the
    ' flood bug in this family has been in a HOST callback, never in a control's own painter.
    '
    ' IT HAS TO BUILD A POPUP TO PROBE, and then destroy it -- there is no long-lived picker on
    ' this form any more. The popup is never shown; CountRenderedTones renders it offscreen
    ' through the real WM_PAINT path.
    if environ("PSCOLORPICKER_SELFTEST") = "1" then
        print "=== the demo's own paint callback ==="

        dim as HWND hProbe = PsColorPicker_Create( HWND_FRMMAIN, 0 )
        PsColorPicker_SetFont( hProbe, ghFont(GUIFONT_10) )
        PsColorPicker_SetPaintCallback( hProbe, @frmMain_PickerPaint )
        PsColorPicker_SetColor( hProbe, BGR(152,195,121) )
        PsColorPicker_SetTab( hProbe, CLR_TAB_CUSTOM )
        dim as long pcx, pcy
        PsColorPicker_GetIdealSize( hProbe, pcx, pcy )
        SetWindowPos( hProbe, 0, 0, 0, pcx, pcy, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )

        dim as long tBody = PsColorPicker_CountRenderedTones( hProbe, CLR_PART_MATRIX )
        dim as long tTabs = PsColorPicker_CountRenderedTones( hProbe, CLR_PART_TABS )
        dim as string sP
        ' The floors are the same measured ones the suite uses. Swap this callback's
        ' PaintRoundOutline for PaintBorderRect and the body collapses to a single tone.
        if (tBody >= 32) andalso (tTabs >= 4) then sP = "PASS  " else sP = "FAIL  "
        print sP & "the demo's paint callback does not flood the picker   [matrix=" & _
              str(tBody) & " tabs=" & str(tTabs) & "]"

        ' THE POPUP FITS ON THE SCREEN. It used to be that a picker had to fit inside the demo
        ' FORM, and the assertion checked one axis and missed the other. A popup has no form to
        ' fit inside -- what it has to fit inside is the monitor's work area, and if it does not,
        ' PositionForRect clamps it and the bottom of it is simply unreachable.
        dim as RECT rcWork
        SystemParametersInfoW( SPI_GETWORKAREA, 0, @rcWork, 0 )
        dim as long nWorkW = rcWork.right - rcWork.left
        dim as long nWorkH = rcWork.bottom - rcWork.top
        if (pcx <= nWorkW) andalso (pcy <= nWorkH) then sP = "PASS  " else sP = "FAIL  "
        print sP & "the popup fits inside the monitor work area   [popup=" & str(pcx) & "x" & str(pcy) & _
              " work=" & str(nWorkW) & "x" & str(nWorkH) & "]"

        ' ...AND THE SIZE DOES NOT DEPEND ON THE ALPHA ROW, which is the opposite of what this
        ' line first asserted. It demanded that hiding alpha make the popup shorter, and FAILED:
        ' the bottom block is as tall as its tallest column, and the preview column is taller
        ' than the field stack either way. So the row comes and goes inside space the preview had
        ' already claimed. A host can flip ShowAlpha without re-placing an anchored popup, which
        ' is worth an assertion in its own right.
        PsColorPicker_ShowAlpha( hProbe, false )
        dim as long qcx, qcy
        PsColorPicker_GetIdealSize( hProbe, qcx, qcy )
        if (qcy = pcy) andalso (qcx = pcx) then sP = "PASS  " else sP = "FAIL  "
        print sP & "hiding alpha leaves the popup's size alone (the preview column sets it)   [" & _
              str(pcx) & "x" & str(pcy) & " -> " & str(qcx) & "x" & str(qcy) & "]"

        DestroyWindow( hProbe )
    end if

    ShowWindow( HWND_FRMMAIN, SW_SHOW )
    UpdateWindow( HWND_FRMMAIN )

    ' WITHOUT THIS THE FIRST TAB DOES NOTHING. IsDialogMessage only acts when the focused window
    ' is a DESCENDANT of the window passed to it, and at startup focus is on the form itself -- a
    ' window is not its own child. A real dialog solves this in WM_INITDIALOG; an ordinary CWindow
    ' host has to do it here.
    SetFocus( ghBtnPick(0) )

    dim as MSG uMsg
    do while GetMessage( @uMsg, null, 0, 0 )
        if uMsg.message = WM_QUIT then exit do
        if (pWindow->AccelHandle = 0) orelse _
           (TranslateAccelerator( pWindow->hWindow, pWindow->AccelHandle, @uMsg ) = 0) then
            ' IsDialogMessage is what makes Tab work between the form's own buttons. THE PICKER
            ' ADDS NOTHING HERE -- there is no PsColorPicker_FilterMessage and there must never be
            ' one, unlike PsComboBox / PsDatePicker / PsTextBox / PsMenuBar. While a popup is up
            ' this loop is not even running: DoModal's own loop has taken over, which is precisely
            ' what removes the obligation rather than merely making it small.
            if IsDialogMessage( pWindow->hWindow, @uMsg ) = 0 then
                TranslateMessage @uMsg
                DispatchMessage @uMsg
            end if
        end if
    loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS - 1
        if ghFont(i) then DeleteObject( ghFont(i) )
    next

end function
