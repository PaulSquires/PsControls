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
' Stack the three panels down the top of the form, full width. The control has no opinion
' about its own placement -- the host positions it like any window. Full width is what
' makes the justification visible: drag the form narrower and the three runs slide.
' ========================================================================================
sub frmMain_PositionIconPanels( byval hwnd as HWND )
    dim as RECT rc: GetClientRect( hwnd, @rc )
    dim as long nHeight = 40
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then nHeight = pWindow->ScaleY(40)

    dim as HWND hPanels(0 to 2) = { HWND_ICONPANEL_LEFT, HWND_ICONPANEL_CENTER, HWND_ICONPANEL_RIGHT }
    dim as long y = rc.top
    for i as long = 0 to 2
        if IsWindow(hPanels(i)) then
            SetWindowPos( hPanels(i), 0, _
                rc.left, y, rc.right - rc.left, nHeight, _
                SWP_NOZORDER )
        end if
        y += nHeight
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
    if state <> SIZE_MINIMIZED then frmMain_PositionIconPanels( hwnd )
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
' Name this instance, so the console output says which panel is talking.
' ========================================================================================
private function PanelName( byval hCtl as HWND ) as string
    if hCtl = HWND_ICONPANEL_LEFT   then return "left  "
    if hCtl = HWND_ICONPANEL_CENTER then return "center"
    return "right "
end function


' ========================================================================================
' PsIconPanel_ClickCallback  -- a COMMAND item was clicked (matched press + release).
' ========================================================================================
sub PsIconPanel_ClickCallback( byval hCtl as HWND, byval idx as long, byval id as long )
    print PanelName(hCtl) & " click   idx=" & str(idx) & " id=" & str(id)
end sub


' ========================================================================================
' PsIconPanel_SelChangeCallback  -- a TOGGLE item latched or unlatched. Never fires for
' the programmatic PsIconPanel_SetSelected below, which is exactly the point.
' ========================================================================================
sub PsIconPanel_SelChangeCallback( byval hCtl as HWND, byval idx as long, byval isSelected as boolean )
    print PanelName(hCtl) & " toggle  idx=" & str(idx) & " selected=" & str(isSelected)
end sub


' ========================================================================================
' PsIconPanel_TooltipCallback  -- consulted only for items with no tooltip text of their
' own (here: the Settings item), which is how both tooltip paths get exercised.
' ========================================================================================
function PsIconPanel_TooltipCallback( byval hCtl as HWND, byval idx as long ) as DWSTRING
    return "on-demand tip for item " & idx
end function


' ========================================================================================
' PsIconPanel_MessageCallback  -- right-click is reported, never acted on: a context menu
' is the host's business.
' ========================================================================================
function PsIconPanel_MessageCallback( byval m as PSICONPANEL_MESSAGEINFO ptr ) as boolean
    if m->uMsg = WM_RBUTTONUP then
        print PanelName(m->hIconPanel) & " rclick  idx=" & str(m->idx)
    end if
    ' Return false to allow the control's default handling to proceed.
    return false
end function


' ========================================================================================
' PsIconPanel_PillPaintCallback  -- the third panel's own renderer, REPLACING the built-in
' painter for every one of its items. It draws the compact "pill" highlight the built-in
' painter deliberately does not: fill an inflated rcIcon instead of the whole cell.
'
' Note it must still paint p->rc's background itself -- the control background-filled the
' client before the callbacks ran, so here that means simply leaving it alone.
' ========================================================================================
sub PsIconPanel_PillPaintCallback( byval p as PSICONPANEL_PAINTINFO ptr )

    if p->itemKind = IP_KIND_SEPARATOR then
        p->b->SetBackColor( theme.ForeColorScrollBar )
        p->b->PaintRect( @p->rcIcon )
        exit sub
    end if

    dim as COLORREF foreclr = theme.ForeColor
    dim as boolean bFillPill = false
    dim as COLORREF pillclr

    if p->isEnabled = false then
        foreclr = theme.ForeColorScrollBar
    elseif p->isPressed then
        pillclr = theme.FocusAccent : bFillPill = true : foreclr = theme.ForeColorSelect
    elseif p->isHot then
        pillclr = theme.BackColorHot : bFillPill = true : foreclr = theme.ForeColorHot
    elseif p->isSelected then
        pillclr = theme.BackColorSelect : bFillPill = true : foreclr = theme.ForeColorSelect
    end if

    if bFillPill then
        dim as RECT rcPill = p->rcIcon
        InflateRect( @rcPill, 4, 4 )
        p->b->SetBackColor( pillclr )
        p->b->PaintRoundRect( @rcPill, 8 )
    end if

    p->b->SetForeColor( foreclr )
    p->b->SetFont( ghFont(SYMBOLFONT_12) )
    p->b->PaintText( p->wszGlyph, @p->rcIcon, DT_CENTER )

end sub


' Generate a small multi-colour PNG next to the exe, so BuildToolbar can show a REAL image
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
' Fill one panel with the same representative toolbar: two commands, a separator, three
' independent toggles, a separator, a disabled command, and an oversized command.
'
' Everything the layout is supposed to leave alone is exercised here -- a per-item colour,
' a per-item icon size, per-item padding -- so that resizing the form proves the run moves
' as ONE block with its internal spacing untouched.
' ========================================================================================
private sub BuildToolbar( byval hCtl as HWND, byval pWindow as CWindow ptr )

    dim as long idx

    idx = PsIconPanel_AddItem( hCtl, IP_KIND_COMMAND, wchr(&hE74E), IDM_SAVE )
    PsIconPanel_SetTooltipText( hCtl, idx, "Save" )

    idx = PsIconPanel_AddItem( hCtl, IP_KIND_COMMAND, wchr(&hE72C), IDM_REFRESH )
    PsIconPanel_SetTooltipText( hCtl, idx, "Refresh" )
    ' Per-item idle colour: hot / pressed still use the panel's colours.
    PsIconPanel_SetItemForeColor( hCtl, idx, BGR(126,190,126) )

    ' A REAL image item (a .png), sitting in the same cell a glyph would. MakeSampleIconFile
    ' wrote sample_icon.png in frmMain_Show before any panel was populated.
    idx = PsIconPanel_AddItem( hCtl, IP_KIND_COMMAND, "", IDM_REFRESH )
    PsIconPanel_SetImage( hCtl, idx, "sample_icon.png" )
    PsIconPanel_SetTooltipText( hCtl, idx, "PNG image icon" )

    PsIconPanel_AddSeparator( hCtl )

    idx = PsIconPanel_AddItem( hCtl, IP_KIND_TOGGLE, wchr(&hE734), IDM_FAVOURITE )
    PsIconPanel_SetTooltipText( hCtl, idx, "Favourite" )
    ' Programmatic and therefore SILENT: no SelChangeCallback for this.
    PsIconPanel_SetSelected( hCtl, idx, true )

    idx = PsIconPanel_AddItem( hCtl, IP_KIND_TOGGLE, wchr(&hE72E), IDM_READONLY )
    PsIconPanel_SetTooltipText( hCtl, idx, "Read only" )

    idx = PsIconPanel_AddItem( hCtl, IP_KIND_TOGGLE, wchr(&hE721), IDM_FIND )
    PsIconPanel_SetTooltipText( hCtl, idx, "Find panel" )

    PsIconPanel_AddSeparator( hCtl )

    idx = PsIconPanel_AddItem( hCtl, IP_KIND_COMMAND, wchr(&hE74D), IDM_DELETE )
    PsIconPanel_SetTooltipText( hCtl, idx, "Delete (disabled)" )
    PsIconPanel_SetEnabled( hCtl, idx, false )

    ' Oversized, with wider padding of its own: proves a per-item cell survives the
    ' justification untouched.
    idx = PsIconPanel_AddItem( hCtl, IP_KIND_COMMAND, wchr(&hE713), IDM_SETTINGS )
    PsIconPanel_SetItemIconSize( hCtl, idx, pWindow->ScaleX(30), pWindow->ScaleY(30) )
    PsIconPanel_SetItemPadding( hCtl, idx, pWindow->ScaleX(14), pWindow->ScaleX(14) )
    ' No tooltip text: this one goes through the on-demand callback instead.

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
    ghFont(SYMBOLFONT_9)   = pWindow->CreateFont(SYMBOLFONT, 9, FW_NORMAL)
    ghFont(SYMBOLFONT_10)  = pWindow->CreateFont(SYMBOLFONT, 10, FW_NORMAL)
    ghFont(SYMBOLFONT_12)  = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 900, 300 _
            )

    ' Write the sample .png once, before any panel is populated (a panel that has loaded it
    ' holds the file open, so it must exist and be unlocked first).
    MakeSampleIconFile( "sample_icon.png" )

    ' ------------------------------------------------------------------------------------
    ' Three independent instances, one per justification. Nothing is shared: their own
    ' items, their own colours, their own hover and press state, and the third one its own
    ' renderer. Hovering one must never light up another.
    ' ------------------------------------------------------------------------------------
    HWND_ICONPANEL_LEFT   = PsIconPanel_Create( HWND_FRMMAIN, IDC_FRMMAIN_ICONPANEL_LEFT )
    HWND_ICONPANEL_CENTER = PsIconPanel_Create( HWND_FRMMAIN, IDC_FRMMAIN_ICONPANEL_CENTER )
    HWND_ICONPANEL_RIGHT  = PsIconPanel_Create( HWND_FRMMAIN, IDC_FRMMAIN_ICONPANEL_RIGHT )

    dim as HWND hPanels(0 to 2) = { HWND_ICONPANEL_LEFT, HWND_ICONPANEL_CENTER, HWND_ICONPANEL_RIGHT }
    dim as long nJustify(0 to 2) = { IP_JUSTIFY_LEFT, IP_JUSTIFY_CENTER, IP_JUSTIFY_RIGHT }

    for i as long = 0 to 2
        PsIconPanel_SetFont( hPanels(i), ghFont(SYMBOLFONT_12) )
        PsIconPanel_SetJustify( hPanels(i), nJustify(i) )
        PsIconPanel_SetMessageCallback( hPanels(i), @PsIconPanel_MessageCallback )
        PsIconPanel_SetTooltipCallback( hPanels(i), @PsIconPanel_TooltipCallback )
        PsIconPanel_SetClickCallback( hPanels(i), @PsIconPanel_ClickCallback )
        PsIconPanel_SetSelChangeCallback( hPanels(i), @PsIconPanel_SelChangeCallback )
        BuildToolbar( hPanels(i), pWindow )
    next

    ' The middle panel gets its own colours and noticeably wider authored spacing, so the
    ' three runs are visibly different widths while obeying the same layout rule.
    dim as PSICONPANEL_COLORS clrs
    PsIconPanel_GetColors( HWND_ICONPANEL_CENTER, @clrs )
    clrs.BackColor       = theme.BackColorSelect
    clrs.BackColorHot    = theme.FocusAccent
    clrs.BackColorSelect = theme.BackColor
    PsIconPanel_SetColors( HWND_ICONPANEL_CENTER, @clrs )
    PsIconPanel_SetPadding( HWND_ICONPANEL_CENTER, pWindow->ScaleX(12), pWindow->ScaleX(12) )

    ' The third panel replaces the built-in painter entirely.
    PsIconPanel_SetPaintItemCallback( HWND_ICONPANEL_RIGHT, @PsIconPanel_PillPaintCallback )

    print "ideal widths: left=" & str(PsIconPanel_GetIdealWidth(HWND_ICONPANEL_LEFT)) & _
          " center="            & str(PsIconPanel_GetIdealWidth(HWND_ICONPANEL_CENTER)) & _
          " right="             & str(PsIconPanel_GetIdealWidth(HWND_ICONPANEL_RIGHT))

    frmMain_PositionIconPanels( HWND_FRMMAIN )
    for i as long = 0 to 2
        ShowWindow( hPanels(i), SW_SHOW )
    next

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
