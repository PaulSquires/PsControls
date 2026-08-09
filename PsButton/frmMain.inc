'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' One settings row: the label the form draws, and the button that sits at its right.
type DEMO_ROW
    wszLabel as DWSTRING
    wszNote  as DWSTRING          ' small print under the label, "" for none
end type
dim shared gRows(0 to BUTTON_COUNT - 1) as DEMO_ROW

' How many times each button has been clicked, drawn back into the row's small print so a
' cancelled gesture (press, slide off, release) is visibly NOT counted.
dim shared gClicks(0 to BUTTON_COUNT - 1) as long


' ========================================================================================
' Row geometry. The button cell is right-aligned against the margin and sized to the control's
' own ideal size -- except the ellipsis row, which is deliberately starved.
' ========================================================================================
#define ROW_HEIGHT      50
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
' The form draws the labels and the hairlines; each button paints its own cell. There is no
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

    for i as long = 0 to BUTTON_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )

        ' label
        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right -= margin
        rcText.bottom = rcRow.top + ((rcRow.bottom - rcRow.top) \ 2)
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )

        ' small print, with the live click count appended
        dim as DWSTRING wszNote = gRows(i).wszNote
        if gClicks(i) > 0 then wszNote = wszNote + "   [clicks: " + wstr(gClicks(i)) + "]"
        dim as RECT rcNote = rcRow
        rcNote.left  += margin
        rcNote.right -= margin
        rcNote.top    = rcRow.top + ((rcRow.bottom - rcRow.top) \ 2)
        b.SetColors( theme.ForeColorDisabled, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_9) )
        b.PaintText( wszNote, @rcNote, DT_LEFT )

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
' Place every button at the right end of its row, at its own ideal size.
'
' The ELLIPSIS row is the deliberate exception: it is given 55% of the width it asked for, so
' DT_END_ELLIPSIS has something to do. Note the ideal size itself is unaffected -- a host that
' sizes with GetIdealSize can never see an ellipsis, which is one of the self-test's claims.
' ========================================================================================
sub frmMain_PositionButtons( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to BUTTON_COUNT - 1
        if IsWindow(ghButton(i)) = 0 then continue for
        dim as RECT rcRow = RowRect( hwnd, i )

        dim as long iw, ih
        PsButton_GetIdealSize( ghButton(i), iw, ih )
        if (iw <= 0) orelse (ih <= 0) then continue for

        if i = BUTTON_ELLIPSIS then iw = (iw * 55) \ 100

        ' The two ALIGNMENT rows are deliberately given MORE than they asked for. Sized to
        ' ideal, the caption span is exactly as wide as the caption, so LEFT, CENTER and RIGHT
        ' all render identically and the rows demonstrate nothing -- which is exactly how the
        ' first version of this demo shipped. Slack is what makes alignment visible at all.
        if (i = BUTTON_ALIGNL) orelse (i = BUTTON_ALIGNR) then
            dim as long slack = 110
            if pWindow then slack = pWindow->ScaleX(110)
            iw += slack
        end if

        dim as long x = rc.right - margin - iw
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - ih) \ 2)
        SetWindowPos( ghButton(i), 0, x, y, iw, ih, SWP_NOZORDER )
    next
end sub


' ========================================================================================
' Callbacks -- the demo's whole purpose is to prove these fire when they should and stay silent
' when they should not, so every one of them prints.
'
' In particular: press a button, SLIDE OFF IT, and release. No line should appear. That is the
' press/cancel gesture the mouse capture exists to protect.
' ========================================================================================
sub PsButton_DemoClick( byval hButton as HWND, byval id as long )
    dim as long idx = id - IDC_FRMMAIN_BTN_FIRST
    if (idx >= 0) andalso (idx < BUTTON_COUNT) then gClicks(idx) += 1

    ' The caption goes through a plain string first: PRINT on a DWSTRING, or on a
    ' string & wstring concat, interleaves nulls (Learnings.md).
    dim as wstring * 512 wszCaption
    wszCaption = PsButton_GetText( hButton )
    dim as string s = "Click       ctrl=" & str(GetDlgCtrlID(hButton)) & _
                      "  id=" & str(id) & "   [" & str(wszCaption) & "]"
    print s

    dim as HWND hForm = GetParent( hButton )
    if hForm then InvalidateRect( hForm, NULL, TRUE )
end sub

' Only the down-message is reported, and only for one row: printing every WM_MOUSEMOVE would
' bury everything else. Returns FALSE so the control's own handling proceeds -- suppressing the
' DOWN would suppress the press itself.
function PsButton_DemoMessage( byval m as PSBUTTON_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false
    if m->uMsg = WM_LBUTTONDOWN then
        dim as string s = "  (message callback saw WM_LBUTTONDOWN on ctrl=" & _
                          str(GetDlgCtrlID(m->hButton)) & ")"
        print s
    end if
    return false
end function

' Consulted only because this button has no tooltip text of its own. Answering "" here would
' show no tip at all -- there is no caption fallback in this control.
function PsButton_DemoTooltip( byval hButton as HWND ) as DWSTRING
    return "Supplied on demand by the tooltip callback"
end function


' ========================================================================================
' The built-in painter replaced WHOLESALE: square, outlined chrome with a filled bar under the
' caption instead of the rounded fill.
'
' THE ONE TRAP THIS DEMONSTRATES AVOIDING. PaintBorderRect goes through PaintRectFactory, which
' ALWAYS fills the rect with _backcolor before stroking it -- so reaching for it to draw an
' outline floods everything underneath. That mistake has now been made three separate times in
' this family (PsToggle, PsComboBox, PsNumericUpDown), every time by copying a sibling's callback.
' PaintRoundOutline is the one that strokes without filling; PaintRectFactory with an explicit
' fill colour is the one to use when the fill is wanted.
' ========================================================================================
sub PsButton_DemoPaint( byval p as PSBUTTON_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF clrLine = theme.ForeColor
    dim as COLORREF clrFill = theme.BackColorHot
    if p->isEnabled = false then
        clrLine = theme.ForeColorDisabled
        clrFill = theme.BackColor
    elseif p->isPressed then
        clrLine = theme.ForeColorSelect
        clrFill = theme.BackColorSelect
    elseif p->isHot then
        clrLine = theme.ForeColorSelect
    end if

    ' square chrome, outlined AND filled -- PaintRectFactory does both in one call
    dim as RECT rcBtn = p->rcButton
    p->b->SetBackColor( clrFill )
    p->b->SetPenColor( clrLine )
    p->b->PaintRectFactory( @rcBtn, PS_SOLID, 1, 0 )

    ' the caption, using the SAME font handed to PsButton_SetFont -- the width was measured
    ' with it, and a different font here would make that measurement a lie
    if IsRectEmpty( @p->rcText ) = 0 then
        dim as RECT rcT = p->rcText
        p->b->SetFont( ghFont(GUIFONT_10) )
        p->b->SetColors( clrLine, clrFill )
        p->b->PaintText( p->wszText, @rcT, DT_CENTER or DT_END_ELLIPSIS )
    end if

    ' The focus ring is the built-in painter's job too, so a callback that wants one draws it.
    ' Curvature 0 gives the square outline this square chrome wants.
    if p->isFocused then
        p->b->SetPenColor( theme.FocusAccent )
        p->b->PaintRoundOutline( @p->rcVisual, 0, 1 )
    end if
end sub


' ========================================================================================
' frmMain message handlers
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean
    return true
end function

function frmMain_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        frmMain_PositionButtons( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
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


' Generate a small multi-colour PNG next to the exe, so a button can show a REAL image beside
' a glyph. Procedural, so the repo commits no binary asset.
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
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)
    ghFont(SYMBOLFONT_10)  = pWindow->CreateFont(SYMBOLFONT, 10, FW_NORMAL)
    ghFont(SYMBOLFONT_12)  = pWindow->CreateFont(SYMBOLFONT, 12, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 760, 720 _
            )

    gRows(0).wszLabel  = "Plain command button"
    gRows(0).wszNote   = "caption only, centred -- press and slide off: no click"
    gRows(1).wszLabel  = "Left glyph"
    gRows(1).wszNote   = "icon pinned at padLeft; the gap is charged because there IS a caption"
    gRows(2).wszLabel  = "Right glyph"
    gRows(2).wszNote   = "icon pinned at padRight, mirrored"
    gRows(3).wszLabel  = "Both glyphs"
    gRows(3).wszNote   = "caption centred in the SPAN between them, not on the button"
    gRows(4).wszLabel  = "Icon only"
    gRows(4).wszNote   = "no caption, so no gap is charged and the height drops to the icon"
    gRows(5).wszLabel  = "BTN_ALIGN_LEFT"
    gRows(5).wszNote   = "given slack past its ideal width, so the alignment is visible at all"
    gRows(6).wszLabel  = "BTN_ALIGN_RIGHT"
    gRows(6).wszNote   = "same slack, other end -- the icon has not moved in either"
    gRows(7).wszLabel  = "Disabled"
    gRows(7).wszNote   = "through EnableWindow, so it swallows input for real and Tab skips it"
    gRows(8).wszLabel  = "Default button"
    gRows(8).wszNote   = "DefaultBorderColor overlays the mood's border -- hover it, press it"
    gRows(9).wszLabel  = "Custom painter"
    gRows(9).wszNote   = "square chrome drawn by the host; message callback prints the DOWN"
    gRows(10).wszLabel = "Tooltip"
    gRows(10).wszNote  = "no text of its own, so the callback is asked -- hover and wait"
    gRows(11).wszLabel = "Forced narrow"
    gRows(11).wszNote  = "sized to 55% of ideal, so DT_END_ELLIPSIS has something to do"

    ' Write the sample .png once, before any button loads it.
    MakeSampleIconFile( "sample_icon.png" )

    for i as long = 0 to BUTTON_COUNT - 1
        ghButton(i) = PsButton_Create( HWND_FRMMAIN, IDC_FRMMAIN_BTN_FIRST + i )
        PsButton_SetFont( ghButton(i), ghFont(GUIFONT_10) )
        PsButton_SetGlyphFont( ghButton(i), ghFont(SYMBOLFONT_10) )
        PsButton_SetClickCallback( ghButton(i), @PsButton_DemoClick )
    next

    ' Row 0 -- the plain case. Everything else is a variation on it.
    PsButton_SetText( ghButton(0), "Save Changes" )

    ' Row 1 -- a left glyph. Note it is set AFTER the caption; order does not matter, because
    ' every setter marks the layout dirty and one measuring pass covers the lot.
    PsButton_SetText( ghButton(1), "Save" )
    PsButton_SetGlyphLeft( ghButton(1), wchr(&hE74E) )

    ' Row 2 -- a right glyph, plus a REAL image (a .png) on the LEFT, to show an image and a
    ' glyph coexisting on one button.
    PsButton_SetText( ghButton(2), "Next" )
    PsButton_SetGlyphRight( ghButton(2), wchr(&hE76C) )
    PsButton_SetImageLeft( ghButton(2), "sample_icon.png" )

    ' Row 3 -- both. The caption centres in what is left between the two cells.
    PsButton_SetText( ghButton(3), "Refresh" )
    PsButton_SetGlyphLeft( ghButton(3), wchr(&hE72C) )
    PsButton_SetGlyphRight( ghButton(3), wchr(&hE76C) )

    ' Row 4 -- icon only. No caption means no gap is charged on either side, and the ideal
    ' HEIGHT drops to the icon: it is visibly shorter than its neighbours, which is honest
    ' rather than a bug (nothing in it is as tall as a line of text).
    PsButton_SetGlyphLeft( ghButton(4), wchr(&hE713) )

    ' Row 5 / 6 -- alignment. frmMain_PositionButtons gives both of these slack past their ideal
    ' width, because at ideal size the span IS the caption and all three alignments look the
    ' same. The icon stays pinned to its padding in both, which is the point being shown.
    PsButton_SetText( ghButton(BUTTON_ALIGNL), "Open Project" )
    PsButton_SetGlyphLeft( ghButton(BUTTON_ALIGNL), wchr(&hE838) )
    PsButton_SetTextAlign( ghButton(BUTTON_ALIGNL), BTN_ALIGN_LEFT )

    PsButton_SetText( ghButton(BUTTON_ALIGNR), "Run" )
    PsButton_SetGlyphRight( ghButton(BUTTON_ALIGNR), wchr(&hE768) )
    PsButton_SetTextAlign( ghButton(BUTTON_ALIGNR), BTN_ALIGN_RIGHT )

    ' Row 7 -- disabled WITH content, which is the case the disabled colours exist for: it must
    ' still read as a real button, just an unavailable one.
    PsButton_SetText( ghButton(BUTTON_DISABLED), "Delete" )
    PsButton_SetGlyphLeft( ghButton(BUTTON_DISABLED), wchr(&hE74D) )
    PsButton_SetEnabled( ghButton(BUTTON_DISABLED), false )

    ' Row 8 -- the default button. isDefault is APPEARANCE ONLY: it does not claim Enter from
    ' anywhere else in the form, so Tab to it first and then press Enter or Space.
    PsButton_SetText( ghButton(BUTTON_DEFAULT), "OK" )
    PsButton_SetGlyphLeft( ghButton(BUTTON_DEFAULT), wchr(&hE73E) )
    PsButton_SetDefault( ghButton(BUTTON_DEFAULT), true )

    ' Row 9 -- the built-in painter replaced wholesale, plus square corners so the custom
    ' chrome and the control's own geometry agree.
    PsButton_SetText( ghButton(BUTTON_CUSTOM), "Custom painter" )
    PsButton_SetCornerCurvature( ghButton(BUTTON_CUSTOM), 0 )
    PsButton_SetPaintCallback( ghButton(BUTTON_CUSTOM), @PsButton_DemoPaint )
    PsButton_SetMessageCallback( ghButton(BUTTON_CUSTOM), @PsButton_DemoMessage )

    ' Row 10 -- icon only again, but this time with a tooltip: with no caption on screen, the
    ' tip is the only way to find out what the button does. Deliberately given NO text of its
    ' own, so the callback is the thing being exercised.
    PsButton_SetGlyphLeft( ghButton(BUTTON_TOOLTIP), wchr(&hE946) )
    PsButton_SetTooltipCallback( ghButton(BUTTON_TOOLTIP), @PsButton_DemoTooltip )

    ' PSBUTTON_DEMO_PSTIP=1 puts the SAME button on the PsTooltip backend instead of the
    ' comctl32 one, so the two can be compared by hovering the identical button in two runs.
    ' Env-gated rather than default because the default backend is what every existing host
    ' gets, and a demo that silently showed the other one would misrepresent the control.
    '
    ' NO ASSERTION CAN REACH WHAT THIS SHOWS. The self-test proves the wiring -- which backend
    ' is live, that the callback is installed, that the hover time survives a switch -- but a
    ' tip only appears after a real dwell driven by a real message pump, so whether it looks
    ' right, points at the right place and honours the hover time in practice is the
    ' interactive pass's to answer.
    if environ("PSBUTTON_DEMO_PSTIP") = "1" then
        ' Armed BEFORE the switch builds the tip, which is the ordering a host uses: one call
        ' at startup themes every tip in the process, including ones controls build lazily.
        dim tipColors as PSTOOLTIP_COLORS
        PsTooltip_SetDefaultColors( @tipColors )      ' the shipped dark palette, explicitly
        PsTooltip_SetDefaultFonts( ghFont(GUIFONT_9) )
        PsTooltip_SetDefaultMaxWidth( 220 )           ' wraps without a hand-sent TTM_SETMAXTIPWIDTH
        PsTooltip_SetDefaultDelays( 400 )             ' and the hover time, honoured either way

        PsButton_SetTooltipMode( ghButton(BUTTON_TOOLTIP), PSTIP_MODE_PS )

        ' A title and a balloon stem are things the comctl32 path in this family cannot do at
        ' all. Set through PsTooltip's own API on the handle, which is the documented door --
        ' the control mirrors none of these setters.
        dim as HWND hPsTip = PsButton_GetPsTooltipHandle( ghButton(BUTTON_TOOLTIP) )
        if hPsTip then
            PsTooltip_SetTitle( hPsTip, "Add item" )
            PsTooltip_SetStyle( hPsTip, TIP_STYLE_BALLOON )
        end if
        print "PSBUTTON_DEMO_PSTIP=1: row 10 is on the PsTooltip backend"
    end if

    ' Row 11 -- a caption far too long for the width the host will give it.
    PsButton_SetText( ghButton(BUTTON_ELLIPSIS), "Save Configuration As..." )
    PsButton_SetGlyphLeft( ghButton(BUTTON_ELLIPSIS), wchr(&hE74E) )

    dim as long iw, ih
    PsButton_GetIdealSize( ghButton(0), iw, ih )
    dim as string s = "row 0 ideal size (text only): " & str(iw) & "x" & str(ih)
    print s
    PsButton_GetIdealSize( ghButton(4), iw, ih )
    s = "row 4 ideal size (icon only): " & str(iw) & "x" & str(ih)
    print s

    frmMain_PositionButtons( HWND_FRMMAIN )
    for i as long = 0 to BUTTON_COUNT - 1
        ShowWindow( ghButton(i), SW_SHOW )
    next

    ' Self-gated on PSBUTTON_SELFTEST=1.
    PsButton_RunSelfTest( HWND_FRMMAIN )

    ' The control's own self-test can only probe the BUILT-IN painter -- it knows nothing about
    ' this demo. The host paint callback above is exactly the place the family's PaintBorderRect
    ' flood has shipped from three times, so it is probed HERE, by the host, which is also the
    ' pattern a real host should copy for its own painter.
    if environ("PSBUTTON_SELFTEST") = "1" then
        dim as long tHost = PsButton_CountRenderedTones( ghButton(BUTTON_CUSTOM), BTN_PART_BUTTON )
        dim as long tHostText = PsButton_CountRenderedTones( ghButton(BUTTON_CUSTOM), BTN_PART_TEXT )
        dim as string sHost = "PASS  "
        if (tHost <= 1) orelse (tHostText <= 1) then sHost = "FAIL  "
        sHost &= "demo paint callback does not flood   [chrome tones=" & str(tHost) & _
                 " caption tones=" & str(tHostText) & "]"
        print sHost
    end if

    ' A self-test run closes the window and lets the process exit, so the suite can be run
    ' unattended. Without this the demo sits in its pump and the console output stays BUFFERED
    ' -- killing the run then loses every line of it, including the summary (PsTooltip's demo
    ' does the same, and tiko's About box carries the same hook for the same reason).
    if environ("PSBUTTON_SELFTEST") = "1" then
        DestroyWindow( pWindow->hWindow )
        return 0
    end if

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Give the first button the focus. A real dialog does this for you in WM_INITDIALOG; an
    ' ordinary window must do it itself, and without it IsDialogMessage ignores the FIRST Tab
    ' entirely -- it only acts when the focused window is a DESCENDANT of the window it is
    ' passed, and at startup the focus is on the form itself. The symptom is "Tab does nothing
    ' until you click a control", which reads as the tabstops being broken.
    SetFocus( ghButton(0) )


    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    '
    ' THERE IS NO PsButton_FilterMessage. This control owns no popup and no second top-level
    ' window, so unlike PsComboBox / PsNumericUpDown / PsTextBox it adds NO pump obligation.
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then

            ' IsDialogMessage is what makes Tab move between the buttons. A host without it
            ' still gets full mouse behaviour and, once a button has focus, Space and Enter.
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
