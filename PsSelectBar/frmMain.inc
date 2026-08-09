'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

﻿#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColorPanel )
    b.PaintClientRect()

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Stack the three bars down the top of the form, full width. The control has no opinion
' about its own placement -- the host positions it like any window. Full width is what
' makes the right-edge clipping visible when you drag the form narrow.
' ========================================================================================
sub frmMain_PositionSelectBars( byval hwnd as HWND )
    dim as RECT rc: GetClientRect( hwnd, @rc )
    dim as long nHeight = 44
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then nHeight = pWindow->ScaleY(44)

    dim as HWND hBars(0 to 2) = { HWND_SELECTBAR_OUTPUT, HWND_SELECTBAR_TRACKS, HWND_SELECTBAR_CUSTOM }
    dim as long y = rc.top
    for i as long = 0 to 2
        if IsWindow(hBars(i)) then
            ' The third bar is deliberately given a TALLER strip than the other two, so the
            ' block-centred vertical rule is visible as a difference on screen rather than
            ' just an assertion in a trace.
            dim as long h = iif( i = 2, nHeight + pWindow->ScaleY(24), nHeight )
            SetWindowPos( hBars(i), 0, rc.left, y, rc.right - rc.left, h, SWP_NOZORDER )
            y += h
        end if
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
    if state <> SIZE_MINIMIZED then frmMain_PositionSelectBars( hwnd )
    function = 0
end function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    '  Message cracker macro expects a true to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window.
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
' Name this instance, so the console output says which bar is talking.
' ========================================================================================
private function BarName( byval hCtl as HWND ) as string
    if hCtl = HWND_SELECTBAR_OUTPUT then return "output"
    if hCtl = HWND_SELECTBAR_TRACKS then return "tracks"
    return "custom"
end function


' ========================================================================================
' PsSelectBar_SelChangeCallback  -- the USER switched panels. Never fires for the
' programmatic PsSelectBar_SetCurSel below, which is exactly the point.
' ========================================================================================
sub PsSelectBar_SelChangeCallback( byval hCtl as HWND, byval idxOld as long, byval idxNew as long )
    dim as string s
    s = BarName(hCtl) & " selchange  from=" & str(idxOld) & " to=" & str(idxNew) & _
        "  id=" & str(PsSelectBar_GetPanelID(hCtl, idxNew))
    print s
end sub


' ========================================================================================
' PsSelectBar_TooltipCallback  -- consulted only for panels with no tooltip text of their
' own, which is how both tooltip paths get exercised.
' ========================================================================================
function PsSelectBar_TooltipCallback( byval hCtl as HWND, byval idx as long ) as DWSTRING
    return "on-demand tip for panel " & idx
end function


' ========================================================================================
' PsSelectBar_MessageCallback  -- right-click is reported, never acted on.
' ========================================================================================
function PsSelectBar_MessageCallback( byval m as PSSELECTBAR_MESSAGEINFO ptr ) as boolean
    if m->uMsg = WM_RBUTTONUP then
        dim as string s = BarName(m->hSelectBar) & " rclick     idx=" & str(m->idx)
        print s
    end if
    ' Return false to allow the control's default handling to proceed.
    return false
end function


' ========================================================================================
' PsSelectBar_BoxPaintCallback  -- the third bar's own renderer, REPLACING the built-in
' painter for every one of its panels. It draws a boxed/underlined variant the built-in
' painter deliberately does not, and distinguishes isPressed from isHot (which the built-in
' painter merges).
'
' Note it must fill p->rc itself: the control background-filled the client before the
' callbacks ran, so anything left unpainted simply shows the bar background.
' ========================================================================================
sub PsSelectBar_BoxPaintCallback( byval p as PSSELECTBAR_PAINTINFO ptr )

    dim as COLORREF backclr = theme.BackColor
    dim as COLORREF foreclr = theme.ForeColor

    if p->isEnabled = false then
        foreclr = theme.ForeColorDisabled
    elseif p->isPressed then
        backclr = theme.FocusAccent          ' pressed: distinct from hot, unlike the built-in
        foreclr = theme.ForeColorSelect
    elseif p->isHot then
        backclr = theme.BackColorHot
        foreclr = theme.ForeColorHot
    elseif p->isSelected then
        backclr = theme.BackColorSelect
        foreclr = theme.ForeColorSelect
    end if

    p->b->SetColors( foreclr, backclr )
    p->b->PaintRect( @p->rc )

    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @p->rcText, DT_CENTER )

    if p->isSelected then
        p->b->SetBackColor( theme.FocusAccent )
        p->b->PaintRect( @p->rcIndicator )
    end if

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
            100, 100, 900, 330 _
            )

    ' ------------------------------------------------------------------------------------
    ' Bar 1 -- tiko's Output panel: white indicator on the dark bar, five panels, and
    ' "SEARCH RESULTS" disabled to show the greyed state. Panel 1 is made current
    ' PROGRAMMATICALLY, which must NOT fire the change callback.
    ' ------------------------------------------------------------------------------------
    HWND_SELECTBAR_OUTPUT = PsSelectBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_SELECTBAR_OUTPUT )

    PsSelectBar_SetFont( HWND_SELECTBAR_OUTPUT, ghFont(GUIFONT_10) )
    PsSelectBar_SetMessageCallback( HWND_SELECTBAR_OUTPUT, @PsSelectBar_MessageCallback )
    PsSelectBar_SetTooltipCallback( HWND_SELECTBAR_OUTPUT, @PsSelectBar_TooltipCallback )
    PsSelectBar_SetSelChangeCallback( HWND_SELECTBAR_OUTPUT, @PsSelectBar_SelChangeCallback )

    dim as long idx
    idx = PsSelectBar_AddPanel( HWND_SELECTBAR_OUTPUT, "COMPILER RESULTS", IDP_COMPILER_RESULTS )
    PsSelectBar_SetTooltipText( HWND_SELECTBAR_OUTPUT, idx, "Errors and warnings from the last build" )
    ' Panel 0's own left padding is the bar's lead-in indent -- there is no separate
    ' property for it.
    PsSelectBar_SetPanelPadding( HWND_SELECTBAR_OUTPUT, idx, pWindow->ScaleX(20), -1 )

    PsSelectBar_AddPanel( HWND_SELECTBAR_OUTPUT, "COMPILER LOG FILE", IDP_COMPILER_LOG )

    idx = PsSelectBar_AddPanel( HWND_SELECTBAR_OUTPUT, "SEARCH RESULTS", IDP_SEARCH_RESULTS )
    PsSelectBar_SetEnabled( HWND_SELECTBAR_OUTPUT, idx, false )   ' nothing searched yet

    PsSelectBar_AddPanel( HWND_SELECTBAR_OUTPUT, "TODO", IDP_TODO )
    PsSelectBar_AddPanel( HWND_SELECTBAR_OUTPUT, "NOTES", IDP_NOTES )   ' no tip text: uses the callback

    ' Silent: no selchange line should appear in the console for this.
    PsSelectBar_SetCurSel( HWND_SELECTBAR_OUTPUT, 1 )

    ' ------------------------------------------------------------------------------------
    ' Bar 2 -- tracks: a blue indicator, thicker line, tighter padding, four panels. Its own
    ' colours and its own hover/press state; hovering one bar must never light up another.
    ' ------------------------------------------------------------------------------------
    HWND_SELECTBAR_TRACKS = PsSelectBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_SELECTBAR_TRACKS )

    PsSelectBar_SetFont( HWND_SELECTBAR_TRACKS, ghFont(GUIFONT_10) )
    PsSelectBar_SetMessageCallback( HWND_SELECTBAR_TRACKS, @PsSelectBar_MessageCallback )
    PsSelectBar_SetSelChangeCallback( HWND_SELECTBAR_TRACKS, @PsSelectBar_SelChangeCallback )

    dim as PSSELECTBAR_COLORS clrs
    PsSelectBar_GetColors( HWND_SELECTBAR_TRACKS, @clrs )
    clrs.BackColor       = BGR(0,0,0)
    clrs.BackColorHot    = BGR(0,0,0)      ' keep the uniform-background look
    clrs.BackColorSelect = BGR(0,0,0)
    clrs.ForeColorSelect = theme.FocusAccent
    clrs.IndicatorColor  = theme.FocusAccent
    PsSelectBar_SetColors( HWND_SELECTBAR_TRACKS, @clrs )
    PsSelectBar_SetPadding( HWND_SELECTBAR_TRACKS, pWindow->ScaleX(10), pWindow->ScaleX(10) )
    PsSelectBar_SetIndicator( HWND_SELECTBAR_TRACKS, pWindow->ScaleY(3), pWindow->ScaleY(6) )

    PsSelectBar_AddPanel( HWND_SELECTBAR_TRACKS, "ACTIVE TRADES", IDP_ACTIVE_TRADES )
    PsSelectBar_AddPanel( HWND_SELECTBAR_TRACKS, "CLOSED TRADES", IDP_CLOSED_TRADES )
    PsSelectBar_AddPanel( HWND_SELECTBAR_TRACKS, "TRANSACTIONS", IDP_TRANSACTIONS )
    PsSelectBar_AddPanel( HWND_SELECTBAR_TRACKS, "NOTES", IDP_TRADES_NOTES )
    ' No SetCurSel: the FIRST panel added is already current, which is the invariant.

    ' ------------------------------------------------------------------------------------
    ' Bar 3 -- replaces the built-in painter entirely, and sits on a taller strip so the
    ' block-centred vertical rule shows as a visible difference.
    ' ------------------------------------------------------------------------------------
    HWND_SELECTBAR_CUSTOM = PsSelectBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_SELECTBAR_CUSTOM )

    PsSelectBar_SetFont( HWND_SELECTBAR_CUSTOM, ghFont(GUIFONT_10) )
    PsSelectBar_SetPaintPanelCallback( HWND_SELECTBAR_CUSTOM, @PsSelectBar_BoxPaintCallback )
    PsSelectBar_SetMessageCallback( HWND_SELECTBAR_CUSTOM, @PsSelectBar_MessageCallback )
    PsSelectBar_SetSelChangeCallback( HWND_SELECTBAR_CUSTOM, @PsSelectBar_SelChangeCallback )
    PsSelectBar_SetIndicator( HWND_SELECTBAR_CUSTOM, pWindow->ScaleY(4), pWindow->ScaleY(3) )

    PsSelectBar_AddPanel( HWND_SELECTBAR_CUSTOM, "custom painter", 300 )
    PsSelectBar_AddPanel( HWND_SELECTBAR_CUSTOM, "pressed differs from hot", 301 )
    PsSelectBar_AddPanel( HWND_SELECTBAR_CUSTOM, "on a taller strip", 302 )

    dim as string s
    s = "ideal widths: output=" & str(PsSelectBar_GetIdealWidth(HWND_SELECTBAR_OUTPUT)) & _
        " tracks="              & str(PsSelectBar_GetIdealWidth(HWND_SELECTBAR_TRACKS)) & _
        " custom="              & str(PsSelectBar_GetIdealWidth(HWND_SELECTBAR_CUSTOM))
    print s

    frmMain_PositionSelectBars( HWND_FRMMAIN )
    ShowWindow( HWND_SELECTBAR_OUTPUT, SW_SHOW )
    ShowWindow( HWND_SELECTBAR_TRACKS, SW_SHOW )
    ShowWindow( HWND_SELECTBAR_CUSTOM, SW_SHOW )

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

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
