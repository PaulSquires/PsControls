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
' Stack both tab bars at the top of the form (where an editor's file tabs live), full
' width. The control has no opinion about its own placement -- the host positions it
' like any window.
' ========================================================================================
sub frmMain_PositionTabBars( byval hwnd as HWND )
    dim as RECT rc: GetClientRect( hwnd, @rc )
    dim as long nHeight = 24
    dim as long nBtn = 28
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then
        nHeight = pWindow->ScaleY(36)
        nBtn = pWindow->ScaleY(28)
    end if

    if IsWindow(HWND_TABBAR) then
        SetWindowPos( HWND_TABBAR, 0, _
            rc.left, rc.top, rc.right - rc.left, nHeight, _
            SWP_NOZORDER )
    end if
    if IsWindow(HWND_TABBAR2) then
        SetWindowPos( HWND_TABBAR2, 0, _
            rc.left, rc.top + nHeight, rc.right - rc.left, nHeight, _
            SWP_NOZORDER )
    end if
    ' The nav stand-ins, below the bars.
    if IsWindow(HWND_BTNNAVLEFT) then
        SetWindowPos( HWND_BTNNAVLEFT, 0, _
            rc.left + 8, rc.top + (nHeight * 2) + 8, nBtn, nBtn, SWP_NOZORDER )
    end if
    if IsWindow(HWND_BTNNAVRIGHT) then
        SetWindowPos( HWND_BTNNAVRIGHT, 0, _
            rc.left + 8 + nBtn + 4, rc.top + (nHeight * 2) + 8, nBtn, nBtn, SWP_NOZORDER )
    end if
end sub


' ========================================================================================
' Refresh the nav buttons' enabled state from the bar itself, through the same message
' door the author's real nav control will use.
' ========================================================================================
sub frmMain_UpdateNavButtons()
    if IsWindow(HWND_TABBAR) = 0 then exit sub
    if IsWindow(HWND_BTNNAVLEFT) then
        EnableWindow( HWND_BTNNAVLEFT, _
            iif( SendMessage( HWND_TABBAR, TCM_CANNAVIGATE, cast(WPARAM, -1), 0 ), CTRUE, FALSE ) )
    end if
    if IsWindow(HWND_BTNNAVRIGHT) then
        EnableWindow( HWND_BTNNAVRIGHT, _
            iif( SendMessage( HWND_TABBAR, TCM_CANNAVIGATE, cast(WPARAM, 1), 0 ), CTRUE, FALSE ) )
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
    if state <> SIZE_MINIMIZED then frmMain_PositionTabBars( hwnd )
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

    KillTimer( hwnd, IDT_FRMMAIN_NAVPOLL )

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

        case WM_COMMAND
            ' The nav stand-ins drive the bar purely through the SendMessage door --
            ' exactly what the author's separate nav control will do.
            select case loword(wParam)
                case IDC_FRMMAIN_NAVLEFT
                    SendMessage( HWND_TABBAR, TCM_NAVIGATE, cast(WPARAM, -1), 0 )
                    frmMain_UpdateNavButtons()
                case IDC_FRMMAIN_NAVRIGHT
                    SendMessage( HWND_TABBAR, TCM_NAVIGATE, cast(WPARAM, 1), 0 )
                    frmMain_UpdateNavButtons()
            end select

        case WM_TIMER
            ' Cheap poll: tab closes, drags and window resizes all change whether the
            ' bar can navigate, so just re-ask it periodically.
            if wParam = IDT_FRMMAIN_NAVPOLL then frmMain_UpdateNavButtons()

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
' PsTabBar_PaintCallback
'
' The four rects arrive precomputed (p->rc deflated, p->rcText / rcIcon / rcClose) --
' no geometry is derived here, which is the whole point of the PAINTINFO contract.
' State priority: selected > hot > normal. The control draws the active tab's border
' and the bottom line itself, AFTER this callback; our fill of p->rc (which runs to the
' client bottom on the active tab) is what shows through the line's break.
' ========================================================================================
sub PsTabBar_PaintCallback( byval p as PsTabBar_PAINTINFO ptr )

    ' State priority: dragging > selected > hot > normal. The dragged tab gets its own
    ' fill so the eye can follow it while its neighbours change places around it.
    dim as COLORREF backclr, foreclr
    if p->isDragging then
        backclr = theme.BackColorSelect
        foreclr = theme.ForeColorSelect
    elseif p->isSelected then
        backclr = theme.BackColorHot
        foreclr = theme.ForeColorSelect
    elseif p->isHot then
        backclr = theme.BackColorHot
        foreclr = theme.ForeColorHot
    else
        backclr = theme.BackColor
        foreclr = theme.ForeColor
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )

    ' Fill the (already deflated) paint area
    p->b->PaintRect( @p->rc )

    ' Caption in its precomputed rect, with the font we handed to SetFont (the widths
    ' were measured with it). PaintText applies DT_VCENTER|DT_SINGLELINE itself, and
    ' rcText's symmetric extents keep the text still when selection changes.
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @p->rcText, DT_LEFT )

    ' The close "X" (Segoe Fluent Icons ChromeClose), only while the tab is hot;
    ' brightened while the cursor is over the X itself.
    if p->isHot orelse p->isHotClose then
        if p->isHotClose then p->b->SetForeColor( theme.ForeColorSelect )
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintText( wchr(&hE8BB), @p->rcClose, DT_CENTER )
    end if

end sub

' ========================================================================================
' PsTabBar_PaintCallback2  -- the second instance's own renderer.
' Deliberately a DIFFERENT callback with different colors: callbacks are per-instance, and
' this makes the two bars tell each other apart on screen.
' ========================================================================================
sub PsTabBar_PaintCallback2( byval p as PsTabBar_PAINTINFO ptr )

    dim as COLORREF backclr, foreclr
    if p->isDragging then
        backclr = theme.BackColorHot
        foreclr = theme.ForeColorSelect
    elseif p->isSelected then
        backclr = theme.FocusAccent
        foreclr = theme.ForeColorSelect
    elseif p->isHot then
        backclr = theme.FocusAccent
        foreclr = theme.ForeColorSelect
    else
        backclr = theme.BackColorSelect
        foreclr = theme.ForeColorSelect
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @p->rcText, DT_LEFT )

    if p->isHot orelse p->isHotClose then
        if p->isHotClose then p->b->SetForeColor( theme.FocusAccent )
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintText( wchr(&hE8BB), @p->rcClose, DT_CENTER )
    end if

end sub


' ========================================================================================
' PsTabBar_SelChangeCallback  -- the USER changed the active tab (clicks only; the
' demo's own SetCurSel calls at startup are programmatic and stay silent).
' ========================================================================================
sub PsTabBar_SelChangeCallback( byval hTabBar as HWND, byval idxOld as integer, byval idxNew as integer )
    dim as string which = iif( hTabBar = HWND_TABBAR, "bar1", "bar2" )
    print which & " selchange " & str(idxOld) & " -> " & str(idxNew)
end sub


' ========================================================================================
' PsTabBar_CloseCallback  -- the user clicked a tab's "X" (or middle-clicked the tab).
' NOTIFY-ONLY: the control deleted nothing. The host decides -- tiko would prompt
' "save changes?" here and veto by simply not deleting. This demo just deletes.
' ========================================================================================
sub PsTabBar_CloseCallback( byval hTabBar as HWND, byval idx as integer )
    dim as string which = iif( hTabBar = HWND_TABBAR, "bar1", "bar2" )
    dim as string sCap = PsTabBar_GetText( hTabBar, idx )
    print which & " close request idx=" & str(idx) & " (" & sCap & ") -> host deletes"
    PsTabBar_DeleteTab( hTabBar, idx )
end sub


' ========================================================================================
' PsTabBar_ReorderCallback  -- a drag moved a tab (one call per live move; the control's
' state is already updated). tiko would mirror this move into its document list.
' ========================================================================================
sub PsTabBar_ReorderCallback( byval hTabBar as HWND, byval idxFrom as integer, byval idxTo as integer )
    dim as string which = iif( hTabBar = HWND_TABBAR, "bar1", "bar2" )
    print which & " reorder " & str(idxFrom) & " -> " & str(idxTo)
end sub


' ========================================================================================
' PsTabBar_MessageCallback
' ========================================================================================
function PsTabBar_MessageCallback( byval m as PsTabBar_MESSAGEINFO ptr ) as boolean

    ' Which instance is talking to us? Both bars share this callback, so name them.
    dim as string which = iif( m->hTabBar = HWND_TABBAR, "bar1", "bar2" )

    select case m->uMsg

        case WM_MOUSEHOVER
            print which & " hover idx=" & str(m->idx)
            ' Tooltips are on-demand via the tooltip callback; nothing to push.
            return true

        case WM_LBUTTONUP
            ' The control holds capture across the press, so this up arrives even when
            ' the cursor was dragged outside: idx = -1 then. (Our return value is
            ' IGNORED for this message -- it is what releases the capture.)
            if m->idx <> -1 then
                print which & " click idx=" & str(m->idx) & " itemData=" & str(PsTabBar_GetItemData(m->hTabBar, m->idx))
            end if
            return false

    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function


' ========================================================================================
' PsTabBar_TooltipCallback  -- per-tab tooltip text, resolved on demand
' ========================================================================================
function PsTabBar_TooltipCallback( byval hCtl as HWND, byval idx as integer ) as DWSTRING
    dim as DWSTRING wszText = PsTabBar_GetText( hCtl, idx )
    if len(wszText) = 0 then return ""      ' "" suppresses the tip
    return "Tab " & idx & " -> " & wszText
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

    HWND_TABBAR = PsTabBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_TABBAR )

    PsTabBar_SetPaintCallback( HWND_TABBAR, @PsTabBar_PaintCallback )
    PsTabBar_SetMessageCallback( HWND_TABBAR, @PsTabBar_MessageCallback )
    PsTabBar_SetTooltipCallback( HWND_TABBAR, @PsTabBar_TooltipCallback )
    PsTabBar_SetSelChangeCallback( HWND_TABBAR, @PsTabBar_SelChangeCallback )
    PsTabBar_SetCloseCallback( HWND_TABBAR, @PsTabBar_CloseCallback )
    PsTabBar_SetReorderCallback( HWND_TABBAR, @PsTabBar_ReorderCallback )
    PsTabBar_SetBackColor( HWND_TABBAR, theme.BackColor )
    PsTabBar_SetFont( HWND_TABBAR, ghFont(GUIFONT_10) )

    ' A representative editor tab bar: a handful of open files. itemData carries a
    ' host-side document id (a tab's index shifts as tabs are inserted/deleted; the
    ' itemData follows the tab).
    PsTabBar_AddTab( HWND_TABBAR, "main.bas", 1001 )
    PsTabBar_AddTab( HWND_TABBAR, "frmMain.inc", 1002 )
    PsTabBar_AddTab( HWND_TABBAR, "PsTabBar.bi", 1003 )
    PsTabBar_AddTab( HWND_TABBAR, "PsTabBar.inc", 1004 )
    PsTabBar_AddTab( HWND_TABBAR, "a.bi", 1005 )
    ' Enough tabs to overflow the 800px demo window out of the box, so the rightmost
    ' clips at the edge and the nav buttons below have something to do immediately.
    PsTabBar_AddTab( HWND_TABBAR, "PsVScrollBar.inc", 1006 )
    PsTabBar_AddTab( HWND_TABBAR, "PsBufferPaint.bi", 1007 )
    PsTabBar_AddTab( HWND_TABBAR, "REFACTOR_PLAN.md", 1008 )
    PsTabBar_AddTab( HWND_TABBAR, "README.md", 1009 )

    ' A tab never renders narrower than its text, but a very short filename makes an
    ' uncomfortably small click target; floor it.
    PsTabBar_SetTabMinWidth( HWND_TABBAR, 4, pWindow->ScaleX(110) )

    ' The chrome (bottom line + active tab border) is the CONTROL's, in one color.
    PsTabBar_SetBorderColor( HWND_TABBAR, theme.FocusAccent )
    PsTabBar_SetCurSel( HWND_TABBAR, 0 )

    ' ------------------------------------------------------------------------------------
    ' A SECOND, independent instance. Nothing is shared: different tabs, different
    ' padding, and its own hover state. Hovering one bar must never light up the other.
    ' ------------------------------------------------------------------------------------
    HWND_TABBAR2 = PsTabBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_TABBAR2 )

    PsTabBar_SetPaintCallback( HWND_TABBAR2, @PsTabBar_PaintCallback2 )  ' its own
    PsTabBar_SetMessageCallback( HWND_TABBAR2, @PsTabBar_MessageCallback )
    PsTabBar_SetTooltipCallback( HWND_TABBAR2, @PsTabBar_TooltipCallback )
    PsTabBar_SetSelChangeCallback( HWND_TABBAR2, @PsTabBar_SelChangeCallback )
    PsTabBar_SetCloseCallback( HWND_TABBAR2, @PsTabBar_CloseCallback )
    PsTabBar_SetReorderCallback( HWND_TABBAR2, @PsTabBar_ReorderCallback )
    PsTabBar_SetBackColor( HWND_TABBAR2, theme.BackColorSelect )
    PsTabBar_SetFont( HWND_TABBAR2, ghFont(GUIFONT_10) )
    PsTabBar_SetPadding( HWND_TABBAR2, pWindow->ScaleX(20) )   ' vs bar1's default 8

    PsTabBar_AddTab( HWND_TABBAR2, "second instance", 2001 )
    PsTabBar_AddTab( HWND_TABBAR2, "notes.txt", 2002 )
    PsTabBar_AddTab( HWND_TABBAR2, "todo.md", 2003 )

    ' Its own chrome color and its own selection: nothing is shared between instances.
    PsTabBar_SetBorderColor( HWND_TABBAR2, theme.ForeColorSelect )
    PsTabBar_SetCurSel( HWND_TABBAR2, 1 )

    ' ------------------------------------------------------------------------------------
    ' Temporary stand-in for the author's separate nav control: two stock buttons that
    ' drive bar1 ONLY through the SendMessage door (TCM_NAVIGATE / TCM_CANNAVIGATE),
    ' proving the cross-control contract before the real control exists.
    ' ------------------------------------------------------------------------------------
    HWND_BTNNAVLEFT = CreateWindowExW( 0, "BUTTON", "<", _
            WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, 0, 0, 0, 0, _
            HWND_FRMMAIN, cast(HMENU, IDC_FRMMAIN_NAVLEFT), NULL, NULL )
    HWND_BTNNAVRIGHT = CreateWindowExW( 0, "BUTTON", ">", _
            WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, 0, 0, 0, 0, _
            HWND_FRMMAIN, cast(HMENU, IDC_FRMMAIN_NAVRIGHT), NULL, NULL )
    SendMessage( HWND_BTNNAVLEFT,  WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), CTRUE )
    SendMessage( HWND_BTNNAVRIGHT, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), CTRUE )
    SetTimer( HWND_FRMMAIN, IDT_FRMMAIN_NAVPOLL, 250, NULL )

    frmMain_PositionTabBars( HWND_FRMMAIN )
    ShowWindow( HWND_TABBAR, SW_SHOW )
    ShowWindow( HWND_TABBAR2, SW_SHOW )
    frmMain_UpdateNavButtons()


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

