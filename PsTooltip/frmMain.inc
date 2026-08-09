'    PsTooltip - reusable owner-drawn tooltip control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

dim shared as HWND ghTarget(0 to TARGET_COUNT - 1)
dim shared as HWND ghTip(0 to TARGET_COUNT - 1)
dim shared as HWND ghStrip, ghStripTip
dim shared as HWND ghBtnOK, ghBtnCancel, ghBtnTipOK, ghBtnTipCancel
dim shared as HBRUSH ghBackBrush
dim shared as long gStripHot = -1
dim shared as long gCallbackShows = 0
dim shared as long gVetoCount = 0

dim shared as wstring * 32 gwszTargetLabel(0 to TARGET_COUNT - 1) = { _
    "Plain text", "Balloon stem", "Multiline + MaxWidth", "Title + glyph", _
    "Callback text", "Custom painter", "No fade", "Vetoed show" }

dim shared as wstring * 24 gwszStripCell(0 to STRIP_CELLS - 1) = { _
    "Open", "Save", "Build", "Run" }


' ========================================================================================
' Host callbacks
' ========================================================================================

' Target 4: text pulled ON DEMAND. It changes between shows on purpose -- that is the whole
' difference between a callback and PsTooltip_SetText, and if the number never moved you could
' not tell the callback was being consulted per show rather than once.
private function frmMain_TipCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                      byval idx as long ) as DWSTRING
    gCallbackShows += 1
    dim as DWSTRING wszOut = "Resolved on demand, show #" & str(gCallbackShows)
    return wszOut
end function


' The per-item strip: one tip, four cells, text chosen by the index the host last pushed.
private function frmMain_StripTipCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                           byval idx as long ) as DWSTRING
    if idx < 0 then return ""
    if idx > STRIP_CELLS - 1 then return ""
    dim as DWSTRING wszOut = gwszStripCell(idx) & "  (cell " & str(idx) & " of " & str(STRIP_CELLS) & ")"
    return wszOut
end function


' Target 7: a veto. Returning FALSE from the showing edge suppresses the tip entirely, and no
' hiding edge follows -- the two edges stay paired.
private function frmMain_TipShowCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                          byval idx as long, byval bShowing as boolean ) as boolean
    if bShowing = false then return true
    ' Counted per HOVER, not per tick: a refused show suppresses the rest of that hover, so this
    ' is consulted exactly once each time the cursor arrives and settles. Leave the row and come
    ' back to get the other answer.
    gVetoCount += 1
    if (gVetoCount mod 2) = 1 then return false
    return true
end function


' Target 5: a host painter replacing the built-in one entirely.
'
' It honours the three contracts in the TIP_PaintCallbackSub header: it paints the body against
' rcBody (NOT rcClient, which includes the stem band), it draws the message with p->nTextFlags
' and the font the tip was measured with, and it frames with PaintRoundOutline rather than
' PaintBorderRect -- which fills unconditionally and would erase everything under it.
private sub frmMain_TipPaint( byval p as PSTOOLTIP_PAINTINFO ptr )
    dim as PsBufferPaint ptr b = p->b

    b->SetBackColor( BGR(24, 60, 46) )
    b->PaintRoundRect( @p->rcBody, p->nCurvature )

    b->SetPenColor( BGR(96, 200, 150) )
    b->PaintRoundOutline( @p->rcBody, p->nCurvature, p->nBorderWidth )

    ' An accent rule down the left edge, inside the padding.
    dim as RECT rcBar = type( p->rcBody.left + 4, p->rcBody.top + 6, _
                              p->rcBody.left + 6, p->rcBody.bottom - 6 )
    b->SetBackColor( BGR(96, 200, 150) )
    b->PaintRect( @rcBar )

    if IsRectEmpty( @p->rcText ) = 0 then
        b->SetFont( ghFont(GUIFONT_9) )
        b->SetForeColor( BGR(210, 245, 230) )
        b->PaintTextEx( p->wszText, @p->rcText, p->nTextFlags )
    end if
end sub


' ========================================================================================
' The per-item strip. Owner-drawn, subclassed for its mouse tracking.
'
' This is the integration a per-item control does: on every WM_MOUSEMOVE work out which cell is
' hot and push it with PsTooltip_SetToolIndex. The setter is a no-op when the index has not
' changed, which is what makes it safe to call from a handler that fires constantly.
' ========================================================================================
private function frmMain_StripCellFromX( byval hStrip as HWND, byval x as long ) as long
    dim as RECT rc
    GetClientRect( hStrip, @rc )
    dim as long cw = (rc.right - rc.left) \ STRIP_CELLS
    if cw < 1 then return -1
    dim as long idx = x \ cw
    if idx < 0 then return -1
    if idx > STRIP_CELLS - 1 then return -1
    return idx
end function


private function frmMain_StripSubclass( byval hwnd as HWND, byval uMsg as UINT, _
                                        byval wParam as WPARAM, byval lParam as LPARAM, _
                                        byval uIdSubclass as UINT_PTR, byval dwRefData as DWORD_PTR ) as LRESULT

    select case uMsg

    case WM_MOUSEMOVE
        dim as long x = cast(short, loword(lParam))
        dim as long idx = frmMain_StripCellFromX( hwnd, x )
        if idx <> gStripHot then
            gStripHot = idx
            PsTooltip_SetToolIndex( ghStripTip, idx )
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        return 0

    case WM_PAINT
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )
        dim as RECT rc
        GetClientRect( hwnd, @rc )
        b.SetBackColor( theme.BackColorHot )
        b.PaintClientRect()
        dim as long cw = (rc.right - rc.left) \ STRIP_CELLS
        for i as long = 0 to STRIP_CELLS - 1
            dim as RECT rcCell = type( i * cw, rc.top, (i + 1) * cw, rc.bottom )
            if i = gStripHot then
                b.SetBackColor( theme.BackColorSelect )
                b.PaintRect( @rcCell )
            end if
            b.SetFont( ghFont(GUIFONT_9) )
            b.SetForeColor( theme.ForeColor )
            b.PaintText( gwszStripCell(i), @rcCell, DT_CENTER )
            if i > 0 then
                b.SetPenColor( theme.Divider )
                b.PaintLine( 1, rcCell.left, rc.top + 4, rcCell.left, rc.bottom - 4 )
            end if
        next
        b.EndDoubleBuffer()
        return 0

    case WM_ERASEBKGND
        return true

    case WM_NCDESTROY
        RemoveWindowSubclass( hwnd, @frmMain_StripSubclass, uIdSubclass )

    end select

    return DefSubclassProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Build every target and its tip.
' ========================================================================================
private sub frmMain_BuildTargets( byval hwnd as HWND )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit sub

    dim as PSTOOLTIP_COLORS clr        ' the shipped defaults, used as-is by most targets

    dim as long yTop = pWindow->ScaleY(56)
    dim as long xLeft = pWindow->ScaleX(24)
    dim as long cxT = pWindow->ScaleX(300)
    dim as long cyT = pWindow->ScaleY(38)
    dim as long dy  = pWindow->ScaleY(46)

    for i as long = 0 to TARGET_COUNT - 1
        ' SS_NOTIFY: without it a static is hit-test transparent and WindowFromPoint returns
        ' the parent. PsTooltip copes with that, but a real host sets this.
        ghTarget(i) = CreateWindowExW( 0, "STATIC", @gwszTargetLabel(i), _
                        WS_CHILD or WS_VISIBLE or SS_NOTIFY or SS_CENTERIMAGE, _
                        xLeft, yTop + (i * dy), cxT, cyT, _
                        hwnd, cast(HMENU, IDC_FRMMAIN_TARGET_FIRST + i), _
                        GetModuleHandleW(NULL), NULL )
        SendMessageW( ghTarget(i), WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), 1 )

        ghTip(i) = PsTooltip_Create( ghTarget(i) )
        PsTooltip_SetFonts( ghTip(i), ghFont(GUIFONT_9), ghFont(GUIFONTBOLD_10), ghFont(GUIFONT_14) )
        PsTooltip_SetColors( ghTip(i), @clr )
    next

    ' 0 -- the default everything.
    PsTooltip_SetText( ghTip(0), "A plain tooltip." )

    ' 1 -- the balloon. Same tip, one setter different.
    PsTooltip_SetText( ghTip(1), "The stem points back at the cursor." )
    PsTooltip_SetStyle( ghTip(1), TIP_STYLE_BALLOON )

    ' 2 -- the gap this control exists to close. Nothing in the family exposes
    '      TTM_SETMAXTIPWIDTH, so tiko hand-sends it through GetTooltipHandle in four places.
    PsTooltip_SetText( ghTip(2), "This message is long enough to need wrapping, which the " & _
                                 "comctl32 tooltips in this family cannot do without the host " & _
                                 "reaching through GetTooltipHandle to send TTM_SETMAXTIPWIDTH " & _
                                 "by hand." & chr(10) & chr(10) & "An embedded newline breaks a line too." )
    PsTooltip_SetMaxWidth( ghTip(2), pWindow->ScaleX(280) )

    ' 3 -- title band and glyph cell. Plain Unicode, so it renders without an icon font.
    PsTooltip_SetTitle( ghTip(3), "Heads up" )
    PsTooltip_SetGlyph( ghTip(3), wchr(&h26A0) )          ' warning sign
    PsTooltip_SetText(  ghTip(3), "A title line, an icon cell, and the message underneath." )
    PsTooltip_SetMaxWidth( ghTip(3), pWindow->ScaleX(260) )
    PsTooltip_SetStyle( ghTip(3), TIP_STYLE_BALLOON )

    ' 4 -- pulled on demand. No SetText at all, so the callback is the only source.
    PsTooltip_SetTooltipCallback( ghTip(4), @frmMain_TipCallback )

    ' 5 -- a host painter replacing the built-in one.
    PsTooltip_SetText( ghTip(5), "Painted by the host callback." )
    PsTooltip_SetPaintCallback( ghTip(5), @frmMain_TipPaint )

    ' 6 -- instant, for comparison against its fading neighbours.
    PsTooltip_SetText( ghTip(6), "No fade: SetFadeTime(0)." )
    PsTooltip_SetFadeTime( ghTip(6), 0 )

    ' 7 -- the veto. Every second show is refused by the ShowCallback.
    PsTooltip_SetText( ghTip(7), "You only see this every second time." )
    PsTooltip_SetShowCallback( ghTip(7), @frmMain_TipShowCallback )

    ' --- the per-item strip -----------------------------------------------------------------
    dim as long yStrip = yTop + (TARGET_COUNT * dy) + pWindow->ScaleY(10)
    ghStrip = CreateWindowExW( 0, "STATIC", "", _
                    WS_CHILD or WS_VISIBLE or SS_NOTIFY or SS_OWNERDRAW, _
                    xLeft, yStrip, cxT, pWindow->ScaleY(34), _
                    hwnd, cast(HMENU, IDC_FRMMAIN_STRIP), GetModuleHandleW(NULL), NULL )
    SetWindowSubclass( ghStrip, @frmMain_StripSubclass, IDC_FRMMAIN_STRIP, 0 )

    ghStripTip = PsTooltip_Create( ghStrip )
    PsTooltip_SetFonts( ghStripTip, ghFont(GUIFONT_9), ghFont(GUIFONTBOLD_10), ghFont(GUIFONT_14) )
    PsTooltip_SetColors( ghStripTip, @clr )
    PsTooltip_SetTooltipCallback( ghStripTip, @frmMain_StripTipCallback )
    PsTooltip_SetStyle( ghStripTip, TIP_STYLE_BALLOON )

    ' --- two plain Win32 buttons, which also get tips ---------------------------------------
    dim as long yBtn = yStrip + pWindow->ScaleY(48)
    ghBtnOK = CreateWindowExW( 0, "BUTTON", "OK", _
                    WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                    xLeft, yBtn, pWindow->ScaleX(90), pWindow->ScaleY(28), _
                    hwnd, cast(HMENU, IDC_FRMMAIN_BTN_OK), GetModuleHandleW(NULL), NULL )
    SendMessageW( ghBtnOK, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), 1 )

    ghBtnCancel = CreateWindowExW( 0, "BUTTON", "Cancel", _
                    WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                    xLeft + pWindow->ScaleX(100), yBtn, pWindow->ScaleX(90), pWindow->ScaleY(28), _
                    hwnd, cast(HMENU, IDC_FRMMAIN_BTN_CANCEL), GetModuleHandleW(NULL), NULL )
    SendMessageW( ghBtnCancel, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), 1 )

    ghBtnTipOK = PsTooltip_Create( ghBtnOK )
    PsTooltip_SetFonts( ghBtnTipOK, ghFont(GUIFONT_9), 0, 0 )
    PsTooltip_SetColors( ghBtnTipOK, @clr )
    PsTooltip_SetText( ghBtnTipOK, "Attached to a stock Win32 BUTTON." )

    ghBtnTipCancel = PsTooltip_Create( ghBtnCancel )
    PsTooltip_SetFonts( ghBtnTipCancel, ghFont(GUIFONT_9), 0, 0 )
    PsTooltip_SetColors( ghBtnTipCancel, @clr )
    PsTooltip_SetText( ghBtnTipCancel, "Closes the demo." )
end sub


' ========================================================================================
' frmMain window procedure
' ========================================================================================
private function frmMain_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT

    select case uMsg

    case WM_COMMAND
        dim as long id = loword(wParam)
        if id = IDC_FRMMAIN_BTN_CANCEL then
            PostMessageW( hwnd, WM_CLOSE, 0, 0 )
            return 0
        end if
        if id = IDC_FRMMAIN_BTN_OK then
            PostMessageW( hwnd, WM_CLOSE, 0, 0 )
            return 0
        end if

    case WM_CTLCOLORSTATIC
        dim as HDC hdcStatic = cast(HDC, wParam)
        SetTextColor( hdcStatic, theme.ForeColor )
        SetBkColor( hdcStatic, theme.BackColorHot )
        if ghBackBrush = 0 then ghBackBrush = CreateSolidBrush( theme.BackColorHot )
        return cast(LRESULT, ghBackBrush)

    case WM_ERASEBKGND
        return true

    case WM_PAINT
        dim as PsBufferPaint b
        b.BeginDoubleBuffer( hwnd )
        b.SetBackColor( theme.BackColor )
        b.PaintClientRect()

        dim as RECT rc
        GetClientRect( hwnd, @rc )
        dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
        dim as RECT rcTitle = type( pWindow->ScaleX(24), pWindow->ScaleY(14), _
                                    rc.right - pWindow->ScaleX(24), pWindow->ScaleY(44) )
        b.SetFont( ghFont(GUIFONT_14) )
        b.SetForeColor( theme.ForeColorHot )
        b.PaintText( "PsTooltip -- hover a row and wait", @rcTitle, DT_LEFT )

        ' Right-hand notes. DrawText clips silently, so the column is given real room --
        ' PsOptionButton's demo shipped with its notes truncated mid-word and nothing said so.
        dim as long xNote = pWindow->ScaleX(348)
        dim as long yNote = pWindow->ScaleY(56)
        dim as long cxNote = rc.right - xNote - pWindow->ScaleX(20)
        b.SetFont( ghFont(GUIFONT_9) )
        b.SetForeColor( theme.ForeColor )
        dim as RECT rcNote = type( xNote, yNote, xNote + cxNote, rc.bottom - pWindow->ScaleY(12) )
        b.PaintTextEx( "Every target below is a plain Win32 STATIC, and the two buttons are " & _
                       "stock BUTTONs -- nothing here is a sibling control. That is the claim " & _
                       "being tested: the tip attaches to any window." & chr(10) & chr(10) & _
                       "There is no FilterMessage and nothing was added to the message loop. " & _
                       "One shared 100 ms timer drives every tip on the form; it speeds up " & _
                       "while a fade is running and drops back afterwards." & chr(10) & chr(10) & _
                       "Press a mouse button while a tip is up: it hides and stays hidden " & _
                       "until the cursor leaves the target. Wait with a tip up and the " & _
                       "auto-pop dismisses it." & chr(10) & chr(10) & _
                       "The strip under the rows is ONE tip over four cells, switched with " & _
                       "PsTooltip_SetToolIndex -- the same call site that sends TTM_POP in the " & _
                       "six per-item controls this replaces.", _
                       @rcNote, DT_LEFT or DT_TOP or DT_WORDBREAK or DT_NOPREFIX )

        b.EndDoubleBuffer()
        return 0

    case WM_DESTROY
        ' The tips are ours and go with us -- exactly where the comctl32 tooltip was destroyed
        ' in the eleven controls this replaces.
        for i as long = 0 to TARGET_COUNT - 1
            if IsWindow( ghTip(i) ) then DestroyWindow( ghTip(i) )
        next
        if IsWindow( ghStripTip )     then DestroyWindow( ghStripTip )
        if IsWindow( ghBtnTipOK )     then DestroyWindow( ghBtnTipOK )
        if IsWindow( ghBtnTipCancel ) then DestroyWindow( ghBtnTipCancel )
        if ghBackBrush then DeleteObject( ghBackBrush ) : ghBackBrush = 0
        PostQuitMessage(0)
        return 0

    end select

    return DefWindowProcW( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow( APPCLASSNAME )

    ghFont(GUIFONT_9)      = pWindow->CreateFont( GUIFONT, 9,  FW_NORMAL )
    ghFont(GUIFONT_10)     = pWindow->CreateFont( GUIFONT, 10, FW_NORMAL )
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont( GUIFONT, 10, FW_BOLD )
    ghFont(GUIFONT_14)     = pWindow->CreateFont( GUIFONT, 14, FW_NORMAL )

    HWND_FRMMAIN = pWindow->Create( hWndParent, APPNAME, @frmMain_WndProc )
    pWindow->SetClientSize( 720, 600 )
    pWindow->Center()

    frmMain_BuildTargets( HWND_FRMMAIN )

    ' The self-test needs a CWindow-backed HWND and real tips to measure, so it runs here --
    ' after the targets exist and before the loop. It then destroys the window, so the process
    ' exits immediately and the exit code is the failure count.
    dim as boolean bSelfTest = (environ("PSTOOLTIP_SELFTEST") = "1")
    if bSelfTest then
        PsTooltip_RunSelfTest( HWND_FRMMAIN )
        DestroyWindow( HWND_FRMMAIN )
    else
        ShowWindow( HWND_FRMMAIN, SW_SHOW )
        UpdateWindow( HWND_FRMMAIN )
    end if

    ' NOTHING IS ADDED TO THIS LOOP. PsTooltip has no FilterMessage and no pump obligation --
    ' that is the property that makes it adoptable without touching a host's message loop.
    dim as MSG uMsg
    while GetMessage( @uMsg, null, 0, 0 )
        TranslateMessage( @uMsg )
        DispatchMessage( @uMsg )
    wend

    for i as long = 0 to MAXFONTS - 1
        if ghFont(i) then DeleteObject( ghFont(i) )
    next

    delete pWindow

    if bSelfTest then return gTipFailCount
    return uMsg.wParam
end function
