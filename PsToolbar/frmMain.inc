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
' Stack the two toolbars across the top of the form, FULL WIDTH.
'
' Full width is the point of the demo: drag the form narrower and watch items move into the
' overflow menu, right-gravity items hold their end, and the CONTROL cell's edit box travel
' with its neighbours.
' ========================================================================================
sub frmMain_PositionToolbars( byval hwnd as HWND )
    dim as RECT rc: GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)

    dim as long nH1 = 0, nH2 = 0, nW = 0
    PsToolbar_GetIdealSize( HWND_TOOLBAR_MAIN, nW, nH1 )
    PsToolbar_GetIdealSize( HWND_TOOLBAR_CUSTOM, nW, nH2 )
    if nH1 <= 0 then nH1 = 34
    if nH2 <= 0 then nH2 = 34

    dim as long y = rc.top
    if IsWindow(HWND_TOOLBAR_MAIN) then
        SetWindowPos( HWND_TOOLBAR_MAIN, 0, rc.left, y, rc.right - rc.left, nH1, SWP_NOZORDER )
    end if
    y += nH1
    if pWindow then y += pWindow->ScaleY(8)

    if IsWindow(HWND_TOOLBAR_CUSTOM) then
        SetWindowPos( HWND_TOOLBAR_CUSTOM, 0, rc.left, y, rc.right - rc.left, nH2, SWP_NOZORDER )
    end if
    y += nH2

    ' Two PLAIN WIN32 BUTTONS, deliberately. A demo form made only of the control under test
    ' is exactly the shape that hid PsComboBox's Tab bug -- these give the dialog manager
    ' something else to walk to, and give the toolbar a real sibling to lose hover to.
    if pWindow then
        dim as long bw = pWindow->ScaleX(120)
        dim as long bh = pWindow->ScaleY(28)
        dim as long bx = rc.left + pWindow->ScaleX(12)
        dim as long by = y + pWindow->ScaleY(16)
        if IsWindow(HWND_BTNPLAIN1) then SetWindowPos( HWND_BTNPLAIN1, 0, bx, by, bw, bh, SWP_NOZORDER )
        if IsWindow(HWND_BTNPLAIN2) then SetWindowPos( HWND_BTNPLAIN2, 0, bx + bw + pWindow->ScaleX(12), by, bw, bh, SWP_NOZORDER )
    end if
end sub


function frmMain_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then frmMain_PositionToolbars( hwnd )
    function = 0
end function


function frmMain_OnCommand( _
            byval hwnd as HWND, _
            byval id as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT
    select case id
    case IDC_FRMMAIN_BTNPLAIN1, IDC_FRMMAIN_BTNPLAIN2
        if codeNotify = BN_CLICKED then
            print "plain BUTTON " & str(id) & " clicked"
        end if
    end select
    function = 0
end function


function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow(hwnd)
    function = 0
end function

function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage(0)
    function = 0
end function


function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,    frmMain_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,   frmMain_OnPaint)
        HANDLE_MSG (hwnd, WM_COMMAND, frmMain_OnCommand)
        HANDLE_MSG (hwnd, WM_CLOSE,   frmMain_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY, frmMain_OnDestroy)

    case WM_ERASEBKGND
        return true
    end select

    function = DefWindowProc(hwnd, uMsg, wParam, lParam)
end function


' ========================================================================================
' Host callbacks
' ========================================================================================
function PsToolbar_TooltipCallback( byval hToolbar as HWND, byval idx as long ) as DWSTRING
    ' Consulted only for items with no tooltip text of their own.
    dim as DWSTRING wszText = PsToolbar_GetText( hToolbar, idx )
    if len(wszText) then return wszText
    return "Item " + wstr(idx)
end function


sub PsToolbar_ClickCallback( byval hToolbar as HWND, byval idx as long, byval id as long )
    ' Fires for a strip click AND for a pick from the overflow menu -- one commit path, so
    ' this handler never has to know which.
    dim as string s = "click: idx=" & str(idx) & " id=" & str(id)
    if PsToolbar_IsItemOverflowed( hToolbar, idx ) then s &= "  (from the overflow menu)"
    print s
end sub


sub PsToolbar_SelChangeCallback( byval hToolbar as HWND, byval idx as long, byval isSelected as boolean )
    dim as string s = "selchange: idx=" & str(idx) & " selected=" & str(isSelected)
    print s
end sub


' The opening edge runs BEFORE a single row is built, which is what makes it the
' just-in-time hook. This handler rebuilds the Run menu every time to prove it.
sub PsToolbar_DropDownCallback( byval hToolbar as HWND, byval idx as long, byval hMenu as HWND, byval isOpen as boolean )
    dim as string s = "dropdown: idx=" & str(idx) & " open=" & str(isOpen)
    print s
    if isOpen = false then exit sub
    if idx = -1 then exit sub          ' the OVERFLOW menu builds itself; adding rows is futile

    dim as long id = PsToolbar_GetItemID( hToolbar, idx )
    select case id
    case IDM_RUN
        PsPopupMenu_Clear( hMenu )
        PsPopupMenu_AddItem( hMenu, IDM_RUN_DEBUG,   "Run in Debugger", "F5" )
        PsPopupMenu_AddItem( hMenu, IDM_RUN_RELEASE, "Run Release",     "Ctrl+F5" )
        PsPopupMenu_AddSeparator( hMenu )
        PsPopupMenu_AddItem( hMenu, IDM_RUN_PROFILE, "Run with Profiler" )
    case IDM_HELP
        PsPopupMenu_Clear( hMenu )
        PsPopupMenu_AddItem( hMenu, IDM_HELP_INDEX, "Help Index", "F1" )
        PsPopupMenu_AddSeparator( hMenu )
        PsPopupMenu_AddItem( hMenu, IDM_HELP_ABOUT, "About..." )
    end select
end sub


' ========================================================================================
' A TBR_KIND_CONTROL cell's child talking to its parent.
'
' The child is parented to the TOOLBAR, so its notifications land there rather than in this
' form procedure -- the toolbar hands them here first, then forwards anything unclaimed to
' the form, so either style works.
'
' The toolbar does not interpret notification codes and cannot: EN_SETFOCUS, CBN_SETFOCUS and
' BN_SETFOCUS are different numbers and a generic cell does not know its child's class. The
' HOST knows, because the host created the child.
' ========================================================================================
function PsToolbar_ControlNotifyCallback( byval m as PSTOOLBAR_CONTROLNOTIFY ptr ) as boolean
    if m = 0 then return false
    if m->uMsg <> WM_COMMAND then return false
    if m->hChild <> HWND_SEARCHBOX then return false

    select case hiword( m->wParam )
    case EN_SETFOCUS
        ' Select the whole thing so typing replaces it -- the usual search-box behaviour.
        SendMessageW( m->hChild, EM_SETSEL, 0, -1 )
        print "search box: focus in (idx=" & str(m->idx) & ", text selected)"
    case EN_KILLFOCUS
        ' Straight into a `dim as string` first: PRINT on a DWSTRING, or on a
        ' string & wstring concat, interleaves nulls (the family's tracing rule).
        dim as string sTxt = AfxGetWindowText( m->hChild )
        dim as string s = "search box: focus out, text='" & sTxt & "'"
        print s
    case EN_CHANGE
        print "search box: text changed"
    end select
    return false          ' observed, not claimed -- let it reach the form too
end function


' ========================================================================================
' ENTER / ESCAPE in the search box.
'
' A KEYSTROKE IS NOT A NOTIFICATION, so this does NOT arrive through the control-notify
' callback: keys go to the FOCUSED window, and a message merely destined for a child never
' passes through the toolbar at all. It has to be caught in the message pump, and the
' placement there is the whole difficulty -- see the pump below.
'
' Returns TRUE when the key was consumed, so the pump can stop processing it.
' ========================================================================================
function frmMain_HandleSearchKey( byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    if pMsg->message <> WM_KEYDOWN then return false
    if pMsg->hwnd <> HWND_SEARCHBOX then return false

    select case pMsg->wParam
    case VK_RETURN
        ' Straight into a `dim as string` first (the family's tracing rule).
        dim as string sText = AfxGetWindowText( HWND_SEARCHBOX )
        dim as string s = "search: ENTER pressed, term='" & sText & "'"
        print s
        ' Consumed. Without this a single-line EDIT beeps at the unhandled return, and
        ' IsDialogMessage turns the key into an IDOK command for the form.
        return true

    case VK_ESCAPE
        SetWindowTextW( HWND_SEARCHBOX, "" )
        print "search: ESCAPE pressed, box cleared"
        return true
    end select

    return false
end function


' A dropdown menu's own commands come back through PsPopupMenu's select callback, NOT
' through the toolbar -- the toolbar registers itself as the notify window so it can track
' open/close state, and leaves the command door to the host.
sub frmMain_MenuSelect( byval hPopup as HWND, byval id as long )
    print "menu command: id=" & str(id)
end sub


' ========================================================================================
' A host paint callback that REPLACES the built-in painter for the second toolbar.
'
' Two rules this deliberately demonstrates, both of which have shipped broken in this family:
'   - the frame is stroked with PaintRoundOutline, never PaintBorderRect. The latter FILLS
'     before it strokes and would erase the item (four occurrences and counting).
'   - the caption is drawn in the SAME font the control measured with. Painting a bold
'     variant here while the control measured the regular one makes the cell too small and
'     the caption ellipsize -- PsOptionButton's demo shipped exactly that bug.
' ========================================================================================
sub PsToolbar_CustomPaintCallback( byval p as PSTOOLBAR_PAINTINFO ptr )
    if p = 0 then exit sub
    dim as PsBufferPaint ptr b = p->b
    if b = 0 then exit sub

    b->SetBackColor( theme.BackColor )
    b->PaintRect( @p->rc )

    if p->itemKind = TBR_KIND_SEPARATOR then
        b->SetBackColor( theme.ForeColorScrollBar )
        b->PaintRect( @p->rcIcon )
        exit sub
    end if

    dim as COLORREF clrFore = theme.ForeColor
    dim as COLORREF clrBack = theme.BackColor

    select case p->nMood
    case TBR_MOOD_DISABLED
        clrFore = theme.ForeColorScrollBar
    case TBR_MOOD_PRESSED
        clrBack = theme.FocusAccent
        clrFore = theme.ForeColorSelect
    case TBR_MOOD_HOT
        clrBack = theme.BackColorHot
        clrFore = theme.ForeColorHot
    case else
        if p->isSelected then
            clrBack = theme.BackColorSelect
            clrFore = theme.ForeColorSelect
        end if
    end select

    dim as RECT rcPill = p->rc
    InflateRect( @rcPill, -2, -3 )
    if (p->nMood <> TBR_MOOD_IDLE) orelse p->isSelected then
        b->SetBackColor( clrBack )
        b->PaintRoundRect( @rcPill, 10 )
    end if

    ' A selected item additionally gets an accent OUTLINE -- stroked, never filled.
    if p->isSelected then
        b->SetPenColor( theme.FocusAccent )
        b->PaintRoundOutline( @rcPill, 10, 1 )
    end if

    b->SetColors( clrFore, clrBack )
    if len(p->wszGlyph) then
        b->SetFont( ghFont(SYMBOLFONT_12) )
        b->PaintText( p->wszGlyph, @p->rcIcon, DT_CENTER )
    end if
    if len(p->wszText) then
        ' The SAME font handed to PsToolbar_SetTextFont for this bar. See the header.
        b->SetFont( ghFont(GUIFONT_10) )
        b->PaintText( p->wszText, @p->rcText, DT_LEFT or DT_END_ELLIPSIS )
    end if
    if IsRectEmpty( @p->rcChevron ) = 0 then
        b->SetFont( ghFont(SYMBOLFONT_9) )
        b->PaintText( !"\uE70D", @p->rcChevron, DT_CENTER )
    end if
end sub


' Generate a small multi-colour PNG next to the exe, so BuildMainToolbar can show a REAL image
' item beside the Fluent-glyph items. Procedural, so the repo commits no binary asset.
private sub MakeSampleIconFile( byref wszPath as wstring )
    const S = 32
    dim as BITMAPINFO bi
    bi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER )
    bi.bmiHeader.biWidth       = S
    bi.bmiHeader.biHeight      = -S
    bi.bmiHeader.biPlanes      = 1
    bi.bmiHeader.biBitCount    = 32
    bi.bmiHeader.biCompression = BI_RGB
    dim as ulong ptr p
    dim as HBITMAP hbm = CreateDIBSection( null, @bi, DIB_RGB_COLORS, cast(any ptr ptr, @p), null, 0 )
    if hbm andalso p then
        for y as long = 0 to S - 1
            for x as long = 0 to S - 1
                dim as ulong c = &hFF3399EE
                if (((x \ 8) + (y \ 8)) and 1) then c = &hFFEE9933
                p[y * S + x] = c
            next
        next
        dim as PsImage tmp
        tmp.LoadFromBitmap( hbm )
        if tmp.IsValid() then AfxGdipSaveImageToPng( tmp.Image()->m_pImage, wszPath )
    end if
    if hbm then DeleteObject( hbm )
end sub


' ========================================================================================
' Build the main toolbar: every one of the six item kinds, both gravities, an overflow-able
' run, and a real child window in a CONTROL cell.
' ========================================================================================
sub BuildMainToolbar( byval hTB as HWND, byval pWindow as CWindow ptr )
    dim as long idx

    PsToolbar_AddItem( hTB, TBR_KIND_COMMAND, !"\uE8A5", "New",  IDM_NEW )
    PsToolbar_AddItem( hTB, TBR_KIND_COMMAND, !"\uE8E5", "Open", IDM_OPEN )
    PsToolbar_AddItem( hTB, TBR_KIND_COMMAND, !"\uE74E", "Save", IDM_SAVE )

    ' A REAL image item (a .png), in the same cell a glyph would occupy. sample_icon.png was
    ' written in frmMain_Show before the toolbars were built.
    idx = PsToolbar_AddItem( hTB, TBR_KIND_COMMAND, "", "Image", IDM_OPEN )
    PsToolbar_SetItemImage( hTB, idx, "sample_icon.png" )
    PsToolbar_SetTooltipText( hTB, idx, "A .png image icon" )

    PsToolbar_AddSeparator( hTB )

    ' Icon-only: no caption, so no text gap is charged and the cell is legitimately narrower
    ' than its captioned neighbours.
    idx = PsToolbar_AddItem( hTB, TBR_KIND_COMMAND, !"\uE90F", "", IDM_BUILD )
    PsToolbar_SetTooltipText( hTB, idx, "Build the project" )

    ' SPLIT: the action zone runs the project, the chevron zone drops the Run menu.
    idx = PsToolbar_AddItem( hTB, TBR_KIND_SPLIT, !"\uE768", "Run", IDM_RUN )
    PsToolbar_SetTooltipText( hTB, idx, "Run (click the chevron for options)" )
    PsToolbar_SetItemForeColor( hTB, idx, BGR(120,200,120) )

    ' A disabled item, so the disabled mood is visible without having to break anything.
    idx = PsToolbar_AddItem( hTB, TBR_KIND_COMMAND, !"\uE71A", "Stop", IDM_STOP )
    PsToolbar_SetEnabled( hTB, idx, false )

    PsToolbar_AddSeparator( hTB )

    ' TOGGLE, independent (no group id): latches on its own.
    idx = PsToolbar_AddItem( hTB, TBR_KIND_TOGGLE, !"\uE751", "Wrap", IDM_WORDWRAP )
    PsToolbar_SetSelected( hTB, idx, true )

    ' CONTROL: a real Win32 EDIT the HOST created and owns. The toolbar only positions it.
    ' PARENT IT TO THE TOOLBAR, not to the form: the toolbar places it with SetWindowPos in
    ' its OWN client coordinates, so any other parent would put it somewhere else entirely.
    HWND_SEARCHBOX = CreateWindowExW( WS_EX_CLIENTEDGE, "EDIT", "search...", _
                                      WS_CHILD or WS_VISIBLE or WS_TABSTOP or ES_AUTOHSCROLL, _
                                      0, 0, 0, 0, hTB, _
                                      cast(HMENU, IDC_FRMMAIN_SEARCHBOX), _
                                      GetModuleHandleW(null), null )
    if HWND_SEARCHBOX then
        SendMessageW( HWND_SEARCHBOX, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_9)), true )
        PsToolbar_AddControl( hTB, HWND_SEARCHBOX, pWindow->ScaleX(150), pWindow->ScaleY(22) )
    end if

    ' ---- RIGHT gravity: these pack against the right edge and overflow LAST -------------
    idx = PsToolbar_AddItem( hTB, TBR_KIND_DROPDOWN, !"\uE897", "Help", IDM_HELP )
    PsToolbar_SetItemGravity( hTB, idx, TBR_GRAVITY_RIGHT )

    idx = PsToolbar_AddItem( hTB, TBR_KIND_COMMAND, !"\uE713", "", IDM_SETTINGS )
    PsToolbar_SetItemGravity( hTB, idx, TBR_GRAVITY_RIGHT )
    PsToolbar_SetTooltipText( hTB, idx, "Settings" )
end sub


' The second bar: a RADIO GROUP of three toggles plus a plain independent toggle, drawn
' entirely by the host's own painter.
sub BuildCustomToolbar( byval hTB as HWND, byval pWindow as CWindow ptr )
    dim as long idx

    idx = PsToolbar_AddItem( hTB, TBR_KIND_TOGGLE, !"\uE8E4", "Left", IDM_ALIGNLEFT )
    PsToolbar_SetItemGroupID( hTB, idx, GROUP_ALIGN )
    PsToolbar_SetSelected( hTB, idx, true )        ' silent, and it enforces the group

    idx = PsToolbar_AddItem( hTB, TBR_KIND_TOGGLE, !"\uE8E3", "Center", IDM_ALIGNCENTER )
    PsToolbar_SetItemGroupID( hTB, idx, GROUP_ALIGN )

    idx = PsToolbar_AddItem( hTB, TBR_KIND_TOGGLE, !"\uE8E2", "Right", IDM_ALIGNRIGHT )
    PsToolbar_SetItemGroupID( hTB, idx, GROUP_ALIGN )

    PsToolbar_AddSeparator( hTB )

    ' Independent toggle in the same bar: proves group membership is per item, not per bar.
    PsToolbar_AddItem( hTB, TBR_KIND_TOGGLE, !"\uE8DD", "Bold", IDM_BOLD )
end sub


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)
    ghFont(SYMBOLFONT_9)   = pWindow->CreateFont(SYMBOLFONT, 9, FW_NORMAL)
    ghFont(SYMBOLFONT_10)  = pWindow->CreateFont(SYMBOLFONT, 10, FW_NORMAL)
    ghFont(SYMBOLFONT_12)  = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 980, 320 _
            )

    HWND_TOOLBAR_MAIN   = PsToolbar_Create( HWND_FRMMAIN, IDC_FRMMAIN_TOOLBAR_MAIN )
    HWND_TOOLBAR_CUSTOM = PsToolbar_Create( HWND_FRMMAIN, IDC_FRMMAIN_TOOLBAR_CUSTOM )

    dim as HWND hBars(0 to 1) = { HWND_TOOLBAR_MAIN, HWND_TOOLBAR_CUSTOM }
    for i as long = 0 to 1
        PsToolbar_SetTextFont( hBars(i), ghFont(GUIFONT_10) )
        PsToolbar_SetGlyphFont( hBars(i), ghFont(SYMBOLFONT_12) )
        PsToolbar_SetTooltipCallback( hBars(i), @PsToolbar_TooltipCallback )
        PsToolbar_SetClickCallback( hBars(i), @PsToolbar_ClickCallback )
        PsToolbar_SetSelChangeCallback( hBars(i), @PsToolbar_SelChangeCallback )
        PsToolbar_SetDropDownCallback( hBars(i), @PsToolbar_DropDownCallback )
        PsToolbar_SetControlNotifyCallback( hBars(i), @PsToolbar_ControlNotifyCallback )
    next

    ' Write the sample .png once, before any toolbar loads it (a toolbar holding it open would
    ' block a re-write).
    MakeSampleIconFile( "sample_icon.png" )

    BuildMainToolbar( HWND_TOOLBAR_MAIN, pWindow )
    BuildCustomToolbar( HWND_TOOLBAR_CUSTOM, pWindow )

    ' The two dropdown menus are toolbar-owned; the HOST attaches its own select callback for
    ' their commands. (Do NOT call PsPopupMenu_SetNotifyWindow on one -- that would displace
    ' the toolbar's own registration and leave its dropped state stuck on.)
    dim as long idxRun = PsToolbar_FindItemByID( HWND_TOOLBAR_MAIN, IDM_RUN )
    dim as long idxHelp = PsToolbar_FindItemByID( HWND_TOOLBAR_MAIN, IDM_HELP )
    PsPopupMenu_SetSelectCallback( PsToolbar_GetItemMenu( HWND_TOOLBAR_MAIN, idxRun ), @frmMain_MenuSelect )
    PsPopupMenu_SetSelectCallback( PsToolbar_GetItemMenu( HWND_TOOLBAR_MAIN, idxHelp ), @frmMain_MenuSelect )

    ' The second bar replaces the built-in painter entirely.
    PsToolbar_SetPaintItemCallback( HWND_TOOLBAR_CUSTOM, @PsToolbar_CustomPaintCallback )

    ' Two plain Win32 BUTTONs -- see frmMain_PositionToolbars for why they are here.
    HWND_BTNPLAIN1 = CreateWindowExW( 0, "BUTTON", "Plain Button 1", _
                                      WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                                      0, 0, 0, 0, HWND_FRMMAIN, _
                                      cast(HMENU, IDC_FRMMAIN_BTNPLAIN1), GetModuleHandleW(null), null )
    HWND_BTNPLAIN2 = CreateWindowExW( 0, "BUTTON", "Plain Button 2", _
                                      WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                                      0, 0, 0, 0, HWND_FRMMAIN, _
                                      cast(HMENU, IDC_FRMMAIN_BTNPLAIN2), GetModuleHandleW(null), null )
    SendMessageW( HWND_BTNPLAIN1, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_9)), true )
    SendMessageW( HWND_BTNPLAIN2, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_9)), true )

    dim as long iw, ih
    PsToolbar_GetIdealSize( HWND_TOOLBAR_MAIN, iw, ih )
    dim as string s = "main toolbar ideal size: " & str(iw) & "x" & str(ih)
    print s

    frmMain_PositionToolbars( HWND_FRMMAIN )
    for i as long = 0 to 1
        ShowWindow( hBars(i), SW_SHOW )
    next

    ' Self-gated on PSTOOLBAR_SELFTEST=1.
    PsToolbar_RunSelfTest( HWND_FRMMAIN )

    ' The control's own self-test can only probe the BUILT-IN painter -- it knows nothing
    ' about this demo. A host paint callback is exactly where the family's PaintBorderRect
    ' flood has shipped from four times, so it is probed HERE, by the host, which is also the
    ' pattern a real host should copy for its own painter.
    if environ("PSTOOLBAR_SELFTEST") = "1" then
        dim as long tCell = PsToolbar_CountRenderedTones( HWND_TOOLBAR_CUSTOM, TBR_PART_ITEM, 0 )
        dim as long tText = PsToolbar_CountRenderedTones( HWND_TOOLBAR_CUSTOM, TBR_PART_TEXT, 0 )
        dim as string sHost = "PASS  "
        ' Floors MEASURED against this callback, not inherited from a sibling: PsOptionButton
        ' recorded PsButton's "> 1" test passing on a completely destroyed render.
        if (tCell < 8) orelse (tText < 4) then sHost = "FAIL  "
        sHost &= "demo paint callback does not flood   [cell tones=" & str(tCell) & _
                 " caption tones=" & str(tText) & "]"
        print sHost
    end if

    UpdateWindow( pWindow->hWindow )
    ' A self-test run closes the window so the process exits and the suite can run
    ' unattended. Without this the demo sits in its pump and the console output stays
    ' BUFFERED -- killing the run then loses every line, the summary included.
    if environ("PSTOOLBAR_SELFTEST") = "1" then
        DestroyWindow( pWindow->hWindow )
        return 0
    end if

    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Screenshot hook: opens the Run split button's menu so a capture can show a dropdown
    ' without synthetic mouse input. Deterministic, and it exercises the programmatic
    ' DropDown path at the same time.
    ' End-to-end check that a REAL EDIT's EN_SETFOCUS travels child -> toolbar -> host
    ' callback. The self-test drives that path with synthetic messages; this drives it with
    ' Windows actually generating the notification.
    if environ("PSTOOLBAR_DEMO_FOCUS") = "1" then
        SetFocus( HWND_SEARCHBOX )
    end if

    ' Drives ENTER through the REAL pump: focus the box, put a term in it, and queue the
    ' keystroke. GetMessage below retrieves it with hwnd = the EDIT, exactly as a real key
    ' press would arrive.
    if environ("PSTOOLBAR_DEMO_ENTER") = "1" then
        SetFocus( HWND_SEARCHBOX )
        SetWindowTextW( HWND_SEARCHBOX, "widget" )
        PostMessageW( HWND_SEARCHBOX, WM_KEYDOWN, VK_RETURN, 0 )
        PostMessageW( HWND_SEARCHBOX, WM_KEYDOWN, VK_ESCAPE, 0 )
    end if

    if environ("PSTOOLBAR_DEMO_DROPDOWN") = "1" then
        PsToolbar_DropDown( HWND_TOOLBAR_MAIN, PsToolbar_FindItemByID( HWND_TOOLBAR_MAIN, IDM_RUN ) )
    end if

    dim uMsg as MSG

    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' THE MANDATORY PUMP OBLIGATION. Without these two lines a dropdown has no keyboard
        ' navigation, never closes on an outside click, and cannot be closed by clicking the
        ' chevron that opened it.
        if PsToolbar_FilterMessage( HWND_TOOLBAR_MAIN, @uMsg ) then continue do
        if PsToolbar_FilterMessage( HWND_TOOLBAR_CUSTOM, @uMsg ) then continue do

        ' Keys aimed at a CONTROL cell's child. THE POSITION OF THIS TEST IS THE WHOLE
        ' DIFFICULTY, and it is bounded on both sides:
        '
        '   AFTER PsToolbar_FilterMessage, so that while a dropdown is open ENTER commits the
        '   highlighted menu row instead of running the search -- the menu owns the keyboard
        '   for as long as it is up.
        '
        '   BEFORE IsDialogMessage, which turns VK_RETURN into an IDOK command for the form
        '   and VK_ESCAPE into IDCANCEL. Put this after it and the keys never arrive -- the
        '   same trap that silently swallowed Escape in a real dialog once already.
        if frmMain_HandleSearchKey( @uMsg ) then continue do

        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
            if IsDialogMessage(pWindow->hWindow, @uMsg) = 0 then
                TranslateMessage @uMsg
                DispatchMessage @uMsg
            end if
        end if
    loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

end function
