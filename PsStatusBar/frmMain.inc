'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
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
' Stack both statusbars at the bottom of the form, full width. The control has no opinion
' about its own placement -- the host positions it like any window.
' ========================================================================================
sub frmMain_PositionStatusBar( byval hwnd as HWND )
    dim as RECT rc: GetClientRect( hwnd, @rc )
    dim as long nHeight = 24
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then nHeight = pWindow->ScaleY(24)

    if IsWindow(HWND_STATUSBAR) then
        SetWindowPos( HWND_STATUSBAR, 0, _
            rc.left, rc.bottom - nHeight, rc.right - rc.left, nHeight, _
            SWP_NOZORDER )
    end if
    if IsWindow(HWND_STATUSBAR2) then
        SetWindowPos( HWND_STATUSBAR2, 0, _
            rc.left, rc.bottom - (nHeight * 2), rc.right - rc.left, nHeight, _
            SWP_NOZORDER )
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
    if state <> SIZE_MINIMIZED then frmMain_PositionStatusBar( hwnd )
    function = 0
end function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    ' Enable drag and drop files
    DragAcceptFiles hwnd, CTRUE
    
    '  Message cracker macro expects a true to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window. This is just one of those Windows
    '  inconsistencies.
    return true
end Function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnClose( byval hwnd as HWND ) as LRESULT

    DestroyWindow(hwnd)
    
    function = 0
end Function


' ========================================================================================
' Process WM_DESTROY message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT

    ' Disable drag and drop files
    DragAcceptFiles hwnd, false
    
    PostQuitMessage(0)
    
    function = 0
end Function


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

end Function




' ========================================================================================
' PsStatusBar_PaintCallback
' ========================================================================================
sub PsStatusBar_PaintCallback( byval p as PSSTATUSBAR_PAINTINFO ptr )

    ' Panel state: hot > normal. (No selected state -- a statusbar has no selection.)
    dim as COLORREF backclr, foreclr
    if p->isHot then
        backclr = theme.BackColorHot
        foreclr = theme.ForeColorHot
    else
        backclr = theme.BackColor
        foreclr = theme.ForeColor
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )

    ' Paint the entire panel background
    p->b->PaintRect( @p->rc )

    ' Inset by the control's own padding, not a hardcoded guess: that padding is what the
    ' panel's width was measured with. And draw with the font we handed to SetFont, for
    ' the same reason.
    dim as RECT rcText = p->rc
    rcText.left += PsStatusBar_GetPadding( p->hStatusBar )
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

end sub

' ========================================================================================
' PsStatusBar_PaintCallback2  -- the second instance's own renderer.
' Deliberately a DIFFERENT callback with different colors: callbacks are per-instance, and
' this makes the two bars tell each other apart on screen.
' ========================================================================================
sub PsStatusBar_PaintCallback2( byval p as PSSTATUSBAR_PAINTINFO ptr )

    dim as COLORREF backclr, foreclr
    if p->isHot then
        backclr = theme.FocusAccent
        foreclr = theme.ForeColorSelect
    else
        backclr = theme.BackColorSelect
        foreclr = theme.ForeColorSelect
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    dim as RECT rcText = p->rc
    rcText.left += PsStatusBar_GetPadding( p->hStatusBar )
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

end sub


' ========================================================================================
' PsStatusBar_MessageCallback
' ========================================================================================
function PsStatusBar_MessageCallback( byval m as PSSTATUSBAR_MESSAGEINFO ptr ) as boolean

    ' Which instance is talking to us? Both bars share this callback, so name them.
    dim as string which = iif( m->hStatusBar = HWND_STATUSBAR, "bar1", "bar2" )

    select case m->uMsg

        case WM_MOUSEHOVER
            print which & " hover idx=" & str(m->idx)
            ' Tooltips are on-demand via the tooltip callback; nothing to push.
            return true

        case WM_LBUTTONUP
            ' No capture is taken, so an up only arrives while the cursor is still over the
            ' control: idx = -1 means "released over empty bar area".
            if m->idx <> -1 then
                print which & " click idx=" & str(m->idx) & " itemData=" & str(PsStatusBar_GetItemData(m->hStatusBar, m->idx))
            end if
            return false

    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function


' ========================================================================================
' PsStatusBar_TooltipCallback  -- per-panel tooltip text, resolved on demand
' ========================================================================================
function PsStatusBar_TooltipCallback( byval hCtl as HWND, byval idx as integer ) as DWSTRING
    dim as DWSTRING wszText = PsStatusBar_GetText( hCtl, idx )
    if len(wszText) = 0 then return ""      ' "" suppresses the tip (the spring panel)
    return "Panel " & idx & " -> " & wszText
end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)
    
    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL) 
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)
    ghFont(ITALICFONT_10)  = pWindow->CreateFont(GUIFIXEDFONT, 10, FW_NORMAL, CTRUE)
    ghFont(INFOFONT_11)    = pWindow->CreateFont(GUIFIXEDFONT, 11, FW_NORMAL)
    ghFont(SYMBOLFONT_9)   = pWindow->CreateFont(SYMBOLFONT, 9, FW_NORMAL)
    ghFont(SYMBOLFONT_10)  = pWindow->CreateFont(SYMBOLFONT, 10, FW_NORMAL)
    ghFont(SYMBOLFONT_12)  = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 800, 400 _
            )

    HWND_STATUSBAR = PsStatusBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_STATUSBAR )

    PsStatusBar_SetPaintCallback( HWND_STATUSBAR, @PsStatusBar_PaintCallback )
    PsStatusBar_SetMessageCallback( HWND_STATUSBAR, @PsStatusBar_MessageCallback )
    PsStatusBar_SetBackColor( HWND_STATUSBAR, theme.BackColor )
    PsStatusBar_SetFont( HWND_STATUSBAR, ghFont(GUIFONT_10) )
    PsStatusBar_SetTooltipCallback( HWND_STATUSBAR, @PsStatusBar_TooltipCallback )

    ' A representative statusbar: a status message on the left, a trailing group of
    ' readouts on the right, and an empty spring panel between them doing the pushing.
    PsStatusBar_AddPanel( HWND_STATUSBAR, "Ready", 1001 )
    PsStatusBar_AddPanel( HWND_STATUSBAR, "", 1002 )            ' the spring
    PsStatusBar_AddPanel( HWND_STATUSBAR, "Ln 1, Col 1", 1003 )
    PsStatusBar_AddPanel( HWND_STATUSBAR, "CRLF", 1004 )
    PsStatusBar_AddPanel( HWND_STATUSBAR, "UTF-8", 1005 )

    ' Panel 1 absorbs all unused width, which right-aligns panels 2..4 behind it.
    PsStatusBar_SetExpandPanel( HWND_STATUSBAR, 1 )
    ' The Ln/Col readout changes length constantly; floor it so it stops shoving its
    ' neighbours around on every caret move.
    PsStatusBar_SetPanelMinWidth( HWND_STATUSBAR, 2, pWindow->ScaleX(110) )

    ' ------------------------------------------------------------------------------------
    ' A SECOND, independent instance. Nothing is shared: different panels, a different
    ' spring (panel 0, so the group is pushed right from the start), different padding, and
    ' its own hover state. Hovering one bar must never light up the other.
    ' ------------------------------------------------------------------------------------
    HWND_STATUSBAR2 = PsStatusBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_STATUSBAR2 )

    PsStatusBar_SetPaintCallback( HWND_STATUSBAR2, @PsStatusBar_PaintCallback2 )  ' its own
    PsStatusBar_SetMessageCallback( HWND_STATUSBAR2, @PsStatusBar_MessageCallback )
    PsStatusBar_SetBackColor( HWND_STATUSBAR2, theme.BackColorSelect )
    PsStatusBar_SetFont( HWND_STATUSBAR2, ghFont(GUIFONT_10) )
    PsStatusBar_SetTooltipCallback( HWND_STATUSBAR2, @PsStatusBar_TooltipCallback )
    PsStatusBar_SetPadding( HWND_STATUSBAR2, pWindow->ScaleX(20) )   ' vs bar1's default 8

    PsStatusBar_AddPanel( HWND_STATUSBAR2, "", 2001 )               ' the spring
    PsStatusBar_AddPanel( HWND_STATUSBAR2, "second instance", 2002 )
    PsStatusBar_AddPanel( HWND_STATUSBAR2, "INS", 2003 )
    PsStatusBar_SetExpandPanel( HWND_STATUSBAR2, 0 )

    frmMain_PositionStatusBar( HWND_FRMMAIN )
    ShowWindow( HWND_STATUSBAR, SW_SHOW )
    ShowWindow( HWND_STATUSBAR2, SW_SHOW )


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

