'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"


' ========================================================================================
' Demo state
' ========================================================================================
dim shared as boolean gWordWrap = true          ' the File > Word Wrap checked toggle
dim shared as boolean gExplorerVisible = true   ' View toggles
dim shared as boolean gOutputVisible = false
dim shared as long    gMruBuildCount = 0        ' proves the MRU menu rebuilds per open
dim shared as DWSTRING gStatusText              ' last command, painted at the bottom
dim shared as RECT    gUpdateRect               ' host-drawn "Update available" label

' Self-test bookkeeping
dim shared as long    gFailCount = 0
dim shared as long    gTestLastCommand = 0
dim shared as boolean gTestMenuOpenInCallback = false


' ========================================================================================
' Menu callbacks
' ========================================================================================

' Every executed command lands here (the popup chain is already closed and the bar
' highlight already cleared). tiko's shim would just PostMessage WM_COMMAND.
sub Demo_MenuCommand( byval hMenuBar as HWND, byval id as long )
    gTestLastCommand = id
    gTestMenuOpenInCallback = PsMenuBar_IsMenuOpen( hMenuBar )

    dim as string s = "menu command id=" & str(id)
    print s

    select case id
        case IDM_FILE_WORDWRAP
            ' State round-trip: the toggle flips the checkmark the menu shows next open.
            gWordWrap = (gWordWrap = false)
            dim as HWND hFileMenu = PsMenuBar_GetDropDown( hMenuBar, 0 )
            PsPopupMenu_SetItemChecked( hFileMenu, IDM_FILE_WORDWRAP, gWordWrap )
        case IDM_VIEW_EXPLORER
            gExplorerVisible = (gExplorerVisible = false)
        case IDM_VIEW_OUTPUT
            gOutputVisible = (gOutputVisible = false)
        case IDM_FILE_EXIT
            PostMessage( HWND_FRMMAIN, WM_CLOSE, 0, 0 )
    end select

    gStatusText = "last command: " & str(id)
    InvalidateRect( HWND_FRMMAIN, NULL, TRUE )
end sub

' The bar-level just-in-time hook: flip states on the dropdown about to open.
sub Demo_MenuInitPopup( byval hMenuBar as HWND, byval idx as long, byval hPopup as HWND )
    select case idx
        case 2      ' View
            PsPopupMenu_SetItemChecked( hPopup, IDM_VIEW_EXPLORER, gExplorerVisible )
            PsPopupMenu_SetItemChecked( hPopup, IDM_VIEW_OUTPUT, gOutputVisible )
            ' Full Screen only makes sense while the Output panel is hidden -- an
            ' arbitrary rule, here to show enable-state flipping at open time.
            PsPopupMenu_SetItemEnabled( hPopup, IDM_VIEW_FULLSCREEN, (gOutputVisible = false) )
    end select
end sub

' The popup-level hook on the "Open Recent" submenu: a full dynamic rebuild per open
' (tiko's MRU pattern), including re-creating and re-attaching the depth-2 submenu
' that Clear destroyed.
sub Demo_MruInitPopup( byval hPopup as HWND )
    gMruBuildCount += 1
    PsPopupMenu_Clear( hPopup )
    for i as long = 1 to 3
        PsPopupMenu_AddItem( hPopup, IDM_MRU_BASE + i, _
            "recent_file_" & i & ".bas  (rebuild #" & gMruBuildCount & ")" )
    next
    PsPopupMenu_AddSeparator( hPopup )
    PsPopupMenu_AddItem( hPopup, IDM_MRU_MORE, "More Files" )
    dim as HWND hMore = PsPopupMenu_Create( HWND_FRMMAIN )
    PsPopupMenu_SetColors( hMore, @gPopupColors )
    PsPopupMenu_SetFonts( hMore, ghFont(GUIFONT_10) )
    PsPopupMenu_AddItem( hMore, IDM_MRU_OLD_A, "older_file_a.bas" )
    PsPopupMenu_AddItem( hMore, IDM_MRU_OLD_B, "older_file_b.bas" )
    PsPopupMenu_SetItemSubMenu( hPopup, IDM_MRU_MORE, hMore )
end sub

' The standalone context menu's own selection callback (no bar involved).
sub Demo_ContextSelect( byval hPopup as HWND, byval id as long )
    dim as string s = "context menu command id=" & str(id)
    print s
    gStatusText = "last context command: " & str(id)
    InvalidateRect( HWND_FRMMAIN, NULL, TRUE )
end sub

' Per-item paint override on the Help dropdown ONLY: proves the callback contract and
' that the other popups keep the built-in painter. Deliberately different styling
' (accent fill, left-aligned bold-ish look) so the eye can tell it apart.
sub Demo_HelpPaintItem( byval p as PSPOPUPMENU_PAINTINFO ptr )
    dim as COLORREF backclr, foreclr
    if p->isHot then
        backclr = theme.FocusAccent
        foreclr = theme.ForeColorSelect
    else
        backclr = theme.BackColorHot
        foreclr = theme.ForeColor
    end if
    p->b->SetColors( foreclr, backclr )
    p->b->PaintRect( @p->rc )
    p->b->SetFont( ghFont(GUIFONT_10) )
    p->b->PaintText( p->wszCaption, @p->rcText, DT_LEFT )
    if len( p->wszAccel ) then p->b->PaintText( p->wszAccel, @p->rcText, DT_RIGHT )
end sub


' ========================================================================================
' Build the menu system: the bar, one dropdown per bar item, one dynamic submenu
' (rebuilt per open) and one static submenu two levels deep.
' ========================================================================================
sub frmMain_BuildMenus( byval hwnd as HWND )
    HWND_MENUBAR = PsMenuBar_Create( hwnd, IDC_FRMMAIN_MENUBAR )
    PsMenuBar_SetColors( HWND_MENUBAR, @gBarColors )
    PsMenuBar_SetFont( HWND_MENUBAR, ghFont(GUIFONT_10) )
    PsMenuBar_SetCommandCallback( HWND_MENUBAR, @Demo_MenuCommand )
    PsMenuBar_SetInitPopupCallback( HWND_MENUBAR, @Demo_MenuInitPopup )

    PsMenuBar_AddItem( HWND_MENUBAR, "File", IDMB_FILE )
    PsMenuBar_AddItem( HWND_MENUBAR, "Edit", IDMB_EDIT )
    PsMenuBar_AddItem( HWND_MENUBAR, "View", IDMB_VIEW )
    PsMenuBar_AddItem( HWND_MENUBAR, "Help", IDMB_HELP )

    ' --- File, with an accel column, a checked toggle, a disabled row, and the
    '     dynamic "Open Recent" submenu ---
    dim as HWND hFileMenu = PsPopupMenu_Create( hwnd )
    PsPopupMenu_SetColors( hFileMenu, @gPopupColors )
    PsPopupMenu_SetFonts( hFileMenu, ghFont(GUIFONT_10) )
    PsPopupMenu_AddItem( hFileMenu, IDM_FILE_NEW, "New File", "Ctrl+N" )
    PsPopupMenu_AddItem( hFileMenu, IDM_FILE_OPEN, "Open File...", "Ctrl+O" )
    PsPopupMenu_AddSeparator( hFileMenu )
    PsPopupMenu_AddItem( hFileMenu, IDM_FILE_RECENT, "Open Recent" )
    PsPopupMenu_AddSeparator( hFileMenu )
    PsPopupMenu_AddItem( hFileMenu, IDM_FILE_WORDWRAP, "Word Wrap", "", false, gWordWrap )
    PsPopupMenu_AddItem( hFileMenu, IDM_FILE_DISABLED, "Disabled Item", "", true )
    PsPopupMenu_AddSeparator( hFileMenu )
    PsPopupMenu_AddItem( hFileMenu, IDM_FILE_EXIT, "Exit", "Alt+F4" )

    dim as HWND hMruMenu = PsPopupMenu_Create( hwnd )
    PsPopupMenu_SetColors( hMruMenu, @gPopupColors )
    PsPopupMenu_SetFonts( hMruMenu, ghFont(GUIFONT_10) )
    PsPopupMenu_SetMinWidth( hMruMenu, 262 )     ' tiko's MRU floor
    PsPopupMenu_SetInitPopupCallback( hMruMenu, @Demo_MruInitPopup )
    PsPopupMenu_SetItemSubMenu( hFileMenu, IDM_FILE_RECENT, hMruMenu )

    PsMenuBar_SetDropDown( HWND_MENUBAR, 0, hFileMenu )

    ' --- Edit: plain commands ---
    dim as HWND hEditMenu = PsPopupMenu_Create( hwnd )
    PsPopupMenu_SetColors( hEditMenu, @gPopupColors )
    PsPopupMenu_SetFonts( hEditMenu, ghFont(GUIFONT_10) )
    PsPopupMenu_AddItem( hEditMenu, IDM_EDIT_UNDO, "Undo", "Ctrl+Z" )
    PsPopupMenu_AddItem( hEditMenu, IDM_EDIT_REDO, "Redo", "Ctrl+Y", true )
    PsPopupMenu_AddSeparator( hEditMenu )
    PsPopupMenu_AddItem( hEditMenu, IDM_EDIT_CUT, "Cut", "Ctrl+X" )
    PsPopupMenu_AddItem( hEditMenu, IDM_EDIT_COPY, "Copy", "Ctrl+C" )
    PsPopupMenu_AddItem( hEditMenu, IDM_EDIT_PASTE, "Paste", "Ctrl+V" )
    PsMenuBar_SetDropDown( HWND_MENUBAR, 1, hEditMenu )

    ' --- View: checked toggles whose states are refreshed by the InitPopup hook ---
    dim as HWND hViewMenu = PsPopupMenu_Create( hwnd )
    PsPopupMenu_SetColors( hViewMenu, @gPopupColors )
    PsPopupMenu_SetFonts( hViewMenu, ghFont(GUIFONT_10) )
    PsPopupMenu_AddItem( hViewMenu, IDM_VIEW_EXPLORER, "Explorer Panel" )
    PsPopupMenu_AddItem( hViewMenu, IDM_VIEW_OUTPUT, "Output Panel" )
    PsPopupMenu_AddSeparator( hViewMenu )
    PsPopupMenu_AddItem( hViewMenu, IDM_VIEW_FULLSCREEN, "Full Screen", "F11" )
    PsMenuBar_SetDropDown( HWND_MENUBAR, 2, hViewMenu )

    ' --- Help: the per-item paint override demo ---
    dim as HWND hHelpMenu = PsPopupMenu_Create( hwnd )
    PsPopupMenu_SetColors( hHelpMenu, @gPopupColors )
    PsPopupMenu_SetFonts( hHelpMenu, ghFont(GUIFONT_10) )
    PsPopupMenu_SetPaintItemCallback( hHelpMenu, @Demo_HelpPaintItem )
    PsPopupMenu_AddItem( hHelpMenu, IDM_HELP_ABOUT, "About PsMenuBar...", "F1" )
    PsMenuBar_SetDropDown( HWND_MENUBAR, 3, hHelpMenu )

    ' --- The standalone context menu (right-click the client area): no bar involved,
    '     its own select callback, its own submenu -- the PsPopupMenu-alone door ---
    HWND_CTXMENU = PsPopupMenu_Create( hwnd )
    PsPopupMenu_SetColors( HWND_CTXMENU, @gPopupColors )
    PsPopupMenu_SetFonts( HWND_CTXMENU, ghFont(GUIFONT_10) )
    PsPopupMenu_SetSelectCallback( HWND_CTXMENU, @Demo_ContextSelect )
    PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_INSERT, "Insert Row" )
    PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_DELETE, "Delete Row", "Del" )
    PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_RENAME, "Rename...", "F2" )
    PsPopupMenu_AddSeparator( HWND_CTXMENU )
    PsPopupMenu_AddItem( HWND_CTXMENU, IDM_CTX_ADVANCED, "Advanced" )
    dim as HWND hAdv = PsPopupMenu_Create( hwnd )
    PsPopupMenu_SetColors( hAdv, @gPopupColors )
    PsPopupMenu_SetFonts( hAdv, ghFont(GUIFONT_10) )
    PsPopupMenu_AddItem( hAdv, IDM_CTX_ADV_ONE, "Advanced Option One" )
    PsPopupMenu_AddItem( hAdv, IDM_CTX_ADV_TWO, "Advanced Option Two" )
    PsPopupMenu_SetItemSubMenu( HWND_CTXMENU, IDM_CTX_ADVANCED, hAdv )
end sub


' ========================================================================================
' Layout: the bar hugs its items (GetIdealWidth); the host owns the strip to its right
' and draws an "Update available" label there -- tiko's pattern.
' ========================================================================================
sub frmMain_PositionControls( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim as long nBarHeight = 30
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then nBarHeight = pWindow->ScaleY(30)

    if IsWindow(HWND_MENUBAR) then
        ' Give the bar geometry first (item rects need a client height), then shrink
        ' it to the items' ideal width so the host owns the rest of the strip.
        SetWindowPos( HWND_MENUBAR, 0, rc.left, rc.top, rc.right - rc.left, nBarHeight, SWP_NOZORDER )
        dim as long nBarW = PsMenuBar_GetIdealWidth( HWND_MENUBAR )
        if nBarW > 0 then
            SetWindowPos( HWND_MENUBAR, 0, rc.left, rc.top, nBarW, nBarHeight, SWP_NOZORDER )
        end if
    end if

    ' Host-drawn label, right-aligned in the leftover top strip.
    dim as long nLabelW = 140
    if pWindow then nLabelW = pWindow->ScaleX(140)
    SetRect( @gUpdateRect, rc.right - nLabelW, rc.top, rc.right, rc.top + nBarHeight )
end sub


' ========================================================================================
' frmMain message handlers
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColorPanel )
    b.PaintClientRect()

    ' The top strip right of the bar is the HOST's: paint it bar-colored and put the
    ' fake "Update available" label there (clickable; see WM_LBUTTONDOWN).
    dim as RECT rcStrip = gUpdateRect
    rcStrip.left = 0
    b.SetBackColor( theme.BackColor )
    b.PaintRect( @rcStrip )
    b.SetColors( theme.FocusAccent, theme.BackColor )
    b.SetFont( ghFont(GUIFONT_10) )
    b.PaintText( "Update available", @gUpdateRect, DT_CENTER )

    ' Status + instructions.
    dim as RECT rcText
    GetClientRect( hwnd, @rcText )
    rcText.left += 12
    rcText.top += 60
    ' PaintText forces DT_VCENTER or DT_SINGLELINE, so the line is centred in whatever rect it
    ' is handed. Handing it the whole remaining client area and only advancing "top" moves that
    ' rect's CENTRE by half the step, so consecutive lines land 12px apart instead of 24 and
    ' overprint each other. Each line therefore gets its own one-line-tall rect.
    const as long LINE_H = 24
    rcText.bottom = rcText.top + LINE_H
    b.SetColors( theme.ForeColorPanel, theme.BackColorPanel )
    b.PaintText( "Click the menu bar; right-click here for a context menu.", @rcText, DT_LEFT )
    rcText.top += LINE_H
    rcText.bottom = rcText.top + LINE_H
    b.PaintText( "F2 highlights the bar for keyboard navigation (arrows / Enter / Esc).", @rcText, DT_LEFT )
    if len(gStatusText) then
        rcText.top += 40
        rcText.bottom = rcText.top + LINE_H
        b.PaintText( gStatusText, @rcText, DT_LEFT )
    end if

    b.EndDoubleBuffer()
    return 0
end function


function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        case WM_PAINT
            return frmMain_OnPaint( hwnd )

        case WM_SIZE
            if wParam <> SIZE_MINIMIZED then frmMain_PositionControls( hwnd )
            return 0

        case WM_LBUTTONDOWN
            dim as POINT pt = ( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
            if PtInRect( @gUpdateRect, pt ) then
                dim as string s = "update label clicked (host-side, not a bar item)"
                print s
            end if
            return 0

        case WM_RBUTTONDOWN
            ' The standalone door: show the context menu at the cursor.
            dim as POINT pt = ( cast(short, loword(lParam)), cast(short, hiword(lParam)) )
            ClientToScreen( hwnd, @pt )
            PsPopupMenu_Show( HWND_CTXMENU, pt.x, pt.y )
            return 0

        case WM_KEYDOWN
            ' Stand-in for a host's Alt handling (Alt itself stays untouched -- the
            ' tiko/Scintilla rule): F2 puts the keyboard highlight on the bar.
            if wParam = VK_F2 then
                PsMenuBar_SetActiveIndex( HWND_MENUBAR, 0 )
                return 0
            end if

        case WM_ACTIVATEAPP
            ' The documented instant-dismissal hook (the control's own foreground
            ' watch is a 100ms poll).
            if wParam = FALSE then
                PsMenuBar_CloseMenu( HWND_MENUBAR )
                PsPopupMenu_CloseChain( HWND_CTXMENU )
            end if

        case WM_ERASEBKGND
            return true

        case WM_CLOSE
            DestroyWindow(hwnd)
            return 0

        case WM_DESTROY
            PostQuitMessage(0)
            return 0

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMMAIN = 0
            return 0

        case WM_SYSCOMMAND
            if (wParam and &hFFF0) = SC_CLOSE then
                SendMessage( hwnd, WM_CLOSE, wParam, lParam )
                exit function
            end if
    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Geometry / behavior self-test (run with PSMENUBAR_SELFTEST=1). No interaction: the
' keyboard tests drive crafted MSG structs through the REAL filter functions -- that
' is the actual runtime code path, unlike synthetic SendMessage input, which would
' bypass the pump the filters live in.
' ========================================================================================
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

' Build a WM_KEYDOWN aimed at the host window and run it through the bar's filter,
' exactly as the pump would.
function SelfTest_Key( byval vk as long ) as boolean
    dim m as MSG
    m.hwnd = HWND_FRMMAIN
    m.message = WM_KEYDOWN
    m.wParam = cast(WPARAM, vk)
    return PsMenuBar_FilterMessage( HWND_MENUBAR, @m )
end function

sub frmMain_RunSelfTest()
    print ""
    print "=== PsMenuBar / PsPopupMenu self-test ==="

    dim as HWND hBar = HWND_MENUBAR
    dim as HWND hFileMenu = PsMenuBar_GetDropDown( hBar, 0 )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hFileMenu)

    ' ------------------------------------------------------------------ 1. bar geometry
    dim as HDC hDC = GetDC( hBar )
    dim as HFONT hOldFont = SelectObject( hDC, ghFont(GUIFONT_10) )
    dim as long xExpect = 0
    dim as boolean okContig = true
    dim as boolean okWidths = true
    for i as long = 0 to PsMenuBar_GetItemCount(hBar) - 1
        dim as RECT rc
        PsMenuBar_GetItemRect( hBar, i, rc )
        if rc.left <> xExpect then okContig = false
        dim as DWSTRING wszText = PsMenuBar_GetItemText( hBar, i )
        dim as SIZE sz
        GetTextExtentPoint32W( hDC, wszText.vptr, len(wszText), @sz )
        dim as long wExpect = sz.cx + (PsMenuBar_GetPadding(hBar) * 2)
        if (rc.right - rc.left) <> wExpect then okWidths = false
        xExpect = rc.right
    next
    SelectObject( hDC, hOldFont )
    ReleaseDC( hBar, hDC )
    SelfTest_Check( "bar items contiguous from x=0", okContig, "right edge=" & str(xExpect) )
    SelfTest_Check( "bar item width = text + 2*padding", okWidths, "padding=" & str(PsMenuBar_GetPadding(hBar)) )
    SelfTest_Check( "GetIdealWidth = last item's right edge", _
        (PsMenuBar_GetIdealWidth(hBar) = xExpect), "ideal=" & str(PsMenuBar_GetIdealWidth(hBar)) )

    ' ------------------------------------------------- 2. popup measurement vs formula
    ' A dedicated popup with known content, measured independently with the same font
    ' and the same DPI-scaled defaults the control scaled at Create.
    dim as HWND hTest = PsPopupMenu_Create( HWND_FRMMAIN )
    PsPopupMenu_SetColors( hTest, @gPopupColors )
    PsPopupMenu_SetFonts( hTest, ghFont(GUIFONT_10) )
    PsPopupMenu_AddItem( hTest, 9001, "Alpha" )
    PsPopupMenu_AddItem( hTest, 9002, "Beta item long caption", "Ctrl+B" )
    PsPopupMenu_AddSeparator( hTest )
    PsPopupMenu_AddItem( hTest, 9004, "Gamma" )

    dim as long wIdeal, hIdeal
    PsPopupMenu_GetIdealSize( hTest, wIdeal, hIdeal )
    dim pwT as CWindow ptr = AfxCWindowPtr(hTest)
    dim as long wMaxCap = 0, wAccel = 0
    hDC = GetDC( hTest )
    hOldFont = SelectObject( hDC, ghFont(GUIFONT_10) )
    dim as SIZE sz
    dim as DWSTRING wszM = "Alpha"
    GetTextExtentPoint32W( hDC, wszM.vptr, len(wszM), @sz ) : if sz.cx > wMaxCap then wMaxCap = sz.cx
    wszM = "Beta item long caption"
    GetTextExtentPoint32W( hDC, wszM.vptr, len(wszM), @sz ) : if sz.cx > wMaxCap then wMaxCap = sz.cx
    wszM = "Gamma"
    GetTextExtentPoint32W( hDC, wszM.vptr, len(wszM), @sz ) : if sz.cx > wMaxCap then wMaxCap = sz.cx
    wszM = "Ctrl+B"
    GetTextExtentPoint32W( hDC, wszM.vptr, len(wszM), @sz ) : wAccel = sz.cx
    SelectObject( hDC, hOldFont )
    ReleaseDC( hTest, hDC )

    dim as long bwT = pwT->ScaleX(1)
    if bwT < 1 then bwT = 1
    dim as long wExpect = pwT->ScaleX(PSPOPUPMENU_DEFAULT_CHECKCOL) + wMaxCap _
        + pwT->ScaleX(PSPOPUPMENU_DEFAULT_ACCELGAP) + wAccel _
        + pwT->ScaleX(PSPOPUPMENU_DEFAULT_CHEVRONCOL) _
        + (pwT->ScaleX(PSPOPUPMENU_DEFAULT_HPADDING) * 2) + (bwT * 2)
    ' Three command rows plus ONE separator, and a separator is short: the height is a
    ' sum of per-row heights, not rows * itemHeight.
    dim as long hItemT = pwT->ScaleY(PSPOPUPMENU_DEFAULT_ITEMHEIGHT)
    dim as long hSepT  = pwT->ScaleY(PSPOPUPMENU_DEFAULT_SEPHEIGHT)
    dim as long hExpect = (3 * hItemT) + hSepT _
        + (pwT->ScaleY(PSPOPUPMENU_DEFAULT_VPADDING) * 2) + (bwT * 2)
    SelfTest_Check( "popup ideal width matches formula", (wIdeal = wExpect), _
        "ideal=" & str(wIdeal) & " expected=" & str(wExpect) )
    SelfTest_Check( "popup ideal height matches formula", (hIdeal = hExpect), _
        "ideal=" & str(hIdeal) & " expected=" & str(hExpect) )

    ' Row rects: still gapless and still in order, but row 2 (the separator) is the
    ' short one. Contiguity is what hit-testing depends on -- a gap between rows would
    ' be a dead strip the mouse falls through.
    dim as boolean okRows = true
    dim as boolean okSepShort = true
    dim as RECT rcPrev, rcRow
    for i as long = 0 to PsPopupMenu_GetItemCount(hTest) - 1
        PsPopupMenu_GetItemRect( hTest, i, rcRow )
        dim as long hWant = hItemT
        if i = 2 then hWant = hSepT          ' the AddSeparator row
        if (rcRow.bottom - rcRow.top) <> hWant then okRows = false
        if i > 0 then
            if rcRow.top <> rcPrev.bottom then okRows = false
        end if
        rcPrev = rcRow
    next
    if hSepT >= hItemT then okSepShort = false
    SelfTest_Check( "popup rows contiguous, separator row is short", okRows, _
        "rows=" & str(PsPopupMenu_GetItemCount(hTest)) & _
        " item=" & str(hItemT) & " sep=" & str(hSepT) )
    SelfTest_Check( "separator height < item height", okSepShort, _
        "item=" & str(hItemT) & " sep=" & str(hSepT) )

    ' Hit-testing must agree with the rects it is derived from. Probe the vertical
    ' middle of every row and require the round trip to come back to that row -- this is
    ' what would break if anything still assumed a uniform stride.
    dim as boolean okHit = true
    dim as long yProbeFail = -1
    for i as long = 0 to PsPopupMenu_GetItemCount(hTest) - 1
        PsPopupMenu_GetItemRect( hTest, i, rcRow )
        dim as long yMid = (rcRow.top + rcRow.bottom) \ 2
        dim as long xMid = (rcRow.left + rcRow.right) \ 2
        if PsPopupMenu_HitTest( hTest, xMid, yMid ) <> i then
            okHit = false
            if yProbeFail = -1 then yProbeFail = i
        end if
    next
    SelfTest_Check( "hit-test round-trips every row centre", okHit, _
        "first bad row=" & str(yProbeFail) )

    ' ------------------------------------------------------------------- 3. clamping
    dim as RECT rcWork
    SystemParametersInfo( SPI_GETWORKAREA, 0, @rcWork, 0 )

    ' Bottom clamp: anchor just above the work-area bottom.
    dim as RECT rcAnchor = type( 100, rcWork.bottom - 30, 220, rcWork.bottom - 6 )
    PsPopupMenu_ShowForRect( hTest, rcAnchor, PM_ALIGN_BELOW )
    dim as RECT rcWin
    GetWindowRect( hTest, @rcWin )
    SelfTest_Check( "bottom clamp keeps popup inside work area", _
        (rcWin.bottom <= rcWork.bottom), _
        "bottom=" & str(rcWin.bottom) & " work=" & str(rcWork.bottom) )
    PsPopupMenu_CloseChain( hTest )

    ' Right-edge flip for PM_ALIGN_RIGHT: anchor hugging the right edge.
    rcAnchor = type( rcWork.right - 60, 200, rcWork.right - 10, 224 )
    PsPopupMenu_ShowForRect( hTest, rcAnchor, PM_ALIGN_RIGHT )
    GetWindowRect( hTest, @rcWin )
    SelfTest_Check( "submenu alignment flips at the right edge", _
        (rcWin.left = rcAnchor.left - wIdeal) andalso (rcWin.right <= rcWork.right), _
        "left=" & str(rcWin.left) & " expected=" & str(rcAnchor.left - wIdeal) )
    PsPopupMenu_CloseChain( hTest )
    DestroyWindow( hTest )

    ' ------------------------------------------- 4. by-id state, two levels deep
    ' IDM_CTX_ADV_TWO lives on a submenu attached to the context menu: the by-id
    ' setters must find it by recursing from the root.
    PsPopupMenu_SetItemChecked( HWND_CTXMENU, IDM_CTX_ADV_TWO, true )
    SelfTest_Check( "checked round-trip two levels deep", _
        PsPopupMenu_GetItemChecked( HWND_CTXMENU, IDM_CTX_ADV_TWO ), "id=" & str(IDM_CTX_ADV_TWO) )
    PsPopupMenu_SetItemEnabled( HWND_CTXMENU, IDM_CTX_ADV_TWO, false )
    SelfTest_Check( "enabled round-trip two levels deep", _
        (PsPopupMenu_GetItemEnabled( HWND_CTXMENU, IDM_CTX_ADV_TWO ) = false), "id=" & str(IDM_CTX_ADV_TWO) )
    PsPopupMenu_SetItemEnabled( HWND_CTXMENU, IDM_CTX_ADV_TWO, true )
    PsPopupMenu_SetItemChecked( HWND_CTXMENU, IDM_CTX_ADV_TWO, false )

    ' ------------------------------------------------- 5. keyboard nav via the filter
    PsMenuBar_OpenMenu( hBar, 0, false )
    SelfTest_Check( "OpenMenu opens the File dropdown", _
        PsMenuBar_IsMenuOpen(hBar) andalso PsPopupMenu_IsOpen(hFileMenu), _
        "active=" & str(PsMenuBar_GetActiveIndex(hBar)) )

    SelfTest_Key( VK_DOWN )
    SelfTest_Check( "Down lands on the first selectable row", _
        (PsPopupMenu_GetCurSel(hFileMenu) = 0), "cursel=" & str(PsPopupMenu_GetCurSel(hFileMenu)) )

    ' Regression (2026-07-21): the poll timer's hover net was wiping the KEYBOARD-set
    ' highlight ~100ms after every keypress (the cursor is never over the popup in
    ' keyboard mode). A keyboard-owned selection must survive the net; a hover-owned
    ' one, with the cursor elsewhere, must still be cleared by it.
    PM_PollHoverNet( hFileMenu )
    SelfTest_Check( "keyboard highlight survives the poll hover net", _
        (PsPopupMenu_GetCurSel(hFileMenu) = 0), "cursel=" & str(PsPopupMenu_GetCurSel(hFileMenu)) )
    dim as PSPOPUPMENU ptr pFileMenu = GetPopupMenuPointer( hFileMenu )
    pFileMenu->bHoverSel = true          ' simulate a mouse-owned selection, cursor away
    PM_PollHoverNet( hFileMenu )
    SelfTest_Check( "hover-owned highlight is still cleared by the net", _
        (PsPopupMenu_GetCurSel(hFileMenu) = -1), "cursel=" & str(PsPopupMenu_GetCurSel(hFileMenu)) )
    SelfTest_Key( VK_DOWN )              ' restore for the checks below
    SelfTest_Key( VK_DOWN )
    SelfTest_Key( VK_DOWN )
    ' Rows: 0 New, 1 Open, 2 --sep--, 3 Open Recent: the third Down must skip the
    ' separator and land on 3.
    SelfTest_Check( "Down skips separators", _
        (PsPopupMenu_GetCurSel(hFileMenu) = 3), "cursel=" & str(PsPopupMenu_GetCurSel(hFileMenu)) )

    ' Right on "Open Recent" opens the (freshly rebuilt) MRU submenu, first row selected.
    dim as long nBuildsBefore = gMruBuildCount
    SelfTest_Key( VK_RIGHT )
    dim as HWND hMru = PsPopupMenu_GetItemSubMenu( hFileMenu, IDM_FILE_RECENT )
    SelfTest_Check( "Right opens the submenu (rebuilt by its InitPopup)", _
        PsPopupMenu_IsOpen(hMru) andalso (gMruBuildCount = nBuildsBefore + 1), _
        "builds=" & str(gMruBuildCount) )
    SelfTest_Check( "submenu opens with its first row selected", _
        (PsPopupMenu_GetCurSel(hMru) = 0), "cursel=" & str(PsPopupMenu_GetCurSel(hMru)) )

    ' Escape unwinds one level at a time; at the root it keeps the bar highlight.
    SelfTest_Key( VK_ESCAPE )
    SelfTest_Check( "Escape closes the submenu, root stays open", _
        (PsPopupMenu_IsOpen(hMru) = false) andalso PsPopupMenu_IsOpen(hFileMenu), "" )
    SelfTest_Key( VK_ESCAPE )
    SelfTest_Check( "Escape at root closes the chain, keeps bar highlight", _
        (PsPopupMenu_IsOpen(hFileMenu) = false) andalso (PsMenuBar_IsMenuOpen(hBar) = false) _
        andalso (PsMenuBar_GetActiveIndex(hBar) = 0), _
        "active=" & str(PsMenuBar_GetActiveIndex(hBar)) )
    SelfTest_Key( VK_ESCAPE )
    SelfTest_Check( "third Escape clears the highlight", _
        (PsMenuBar_GetActiveIndex(hBar) = -1), "active=" & str(PsMenuBar_GetActiveIndex(hBar)) )

    ' Right at a row with no submenu moves to the next bar item (Edit), menu open.
    PsMenuBar_OpenMenu( hBar, 0, true )
    SelfTest_Key( VK_RIGHT )
    SelfTest_Check( "Right on a plain row moves to the next bar item", _
        (PsMenuBar_GetActiveIndex(hBar) = 1) andalso PsMenuBar_IsMenuOpen(hBar), _
        "active=" & str(PsMenuBar_GetActiveIndex(hBar)) )
    SelfTest_Key( VK_LEFT )
    SelfTest_Check( "Left at the root moves to the previous bar item", _
        (PsMenuBar_GetActiveIndex(hBar) = 0), "active=" & str(PsMenuBar_GetActiveIndex(hBar)) )

    ' ---------------------------------------------------------------- 6. dispatch
    ' Enter on "New File": the command callback must see the chain ALREADY closed.
    gTestLastCommand = 0
    gTestMenuOpenInCallback = true
    SelfTest_Key( VK_HOME )
    SelfTest_Key( VK_RETURN )
    SelfTest_Check( "Enter executes the selected command", _
        (gTestLastCommand = IDM_FILE_NEW), "id=" & str(gTestLastCommand) )
    SelfTest_Check( "chain is closed before the command callback runs", _
        (gTestMenuOpenInCallback = false), "" )

    ' ------------------------------------------------------------- 7. outside click
    PsMenuBar_OpenMenu( hBar, 0, false )
    dim m as MSG
    m.hwnd = HWND_FRMMAIN
    m.message = WM_LBUTTONDOWN
    dim as boolean bConsumed = PsMenuBar_FilterMessage( hBar, @m )
    SelfTest_Check( "outside click closes the chain and passes through", _
        (bConsumed = false) andalso (PsMenuBar_IsMenuOpen(hBar) = false) _
        andalso (PsMenuBar_GetActiveIndex(hBar) = -1), "" )

    print "=== self-test complete: " & str(gFailCount) & " failure(s) ==="
end sub


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)  = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10) = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 700, 420 _
            )

    frmMain_BuildMenus( HWND_FRMMAIN )
    frmMain_PositionControls( HWND_FRMMAIN )
    ShowWindow( HWND_MENUBAR, SW_SHOW )

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    dim as boolean bSelfTest = (environ("PSMENUBAR_SELFTEST") = "1")
    if bSelfTest then
        frmMain_RunSelfTest()
        DestroyWindow( HWND_FRMMAIN )
    end if

    ' Message loop -- with the ONE line that wires the whole menu system up: the
    ' filter sees every pumped message and consumes the ones the menus own.
    dim uMsg as MSG
    do while GetMessage( @uMsg, null, 0, 0 )
        if PsMenuBar_FilterMessage( HWND_MENUBAR, @uMsg ) then continue do
        if PsPopupMenu_FilterMessage( HWND_CTXMENU, @uMsg ) then continue do
        TranslateMessage( @uMsg )
        DispatchMessage( @uMsg )
    loop

    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

    if bSelfTest then return gFailCount
    return uMsg.wParam
end function
