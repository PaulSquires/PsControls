'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' One settings row: the label the form draws, and the combobox that sits at its right.
type DEMO_ROW
    wszLabel as DWSTRING
    wszNote  as DWSTRING          ' small print under the label, "" for none
end type
dim shared gRows(0 to COMBO_COUNT - 1) as DEMO_ROW

' Serial number appended to items added by the Add button, so every one is distinguishable
' and the widest-item measurement visibly changes as they pile up.
dim shared as long gNextItemSerial = 1


' ========================================================================================
' Row geometry. The combobox cell is right-aligned against the margin and sized to the
' control's own ideal size -- which is the point of an autosizing control.
' ========================================================================================
#define ROW_HEIGHT      54
#define ROW_TOP         14
#define ROW_MARGIN      22
#define BUTTON_BAR_H    46
#define BUTTON_W        96
#define BUTTON_H        26

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
' The form draws the labels and the hairlines; each combobox paints its own cell. There is
' no container window between them -- the same arrangement tiko's Output panel uses for its
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

    for i as long = 0 to COMBO_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )

        ' label
        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right -= margin
        if len( gRows(i).wszNote ) > 0 then
            rcText.bottom = rcRow.top + ((rcRow.bottom - rcRow.top) \ 2)
        end if
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )

        ' small print
        if len( gRows(i).wszNote ) > 0 then
            dim as RECT rcNote = rcRow
            rcNote.left  += margin
            rcNote.right -= margin
            rcNote.top    = rcRow.top + ((rcRow.bottom - rcRow.top) \ 2)
            b.SetColors( theme.ForeColorDisabled, theme.BackColor )
            b.SetFont( ghFont(GUIFONT_9) )
            b.PaintText( gRows(i).wszNote, @rcNote, DT_LEFT )
        end if

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
' Place every combobox at the right end of its row, at its own ideal size.
'
' Row COMBO_MUTABLE may be in auto-size mode, in which case the control has ALREADY sized
' itself (preserving its top-left) and re-placing it here is what keeps it right-aligned as
' it grows. That interaction is the honest cost of auto-size and the demo shows it rather
' than hiding it: the control owns its SIZE, the host still owns its POSITION.
' ========================================================================================
sub frmMain_PositionCombos( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to COMBO_COUNT - 1
        if IsWindow(ghCombo(i)) = 0 then continue for
        dim as RECT rcRow = RowRect( hwnd, i )

        dim as long iw, ih
        PsComboBox_GetIdealSize( ghCombo(i), iw, ih )
        if (iw <= 0) orelse (ih <= 0) then continue for

        dim as long x = rc.right - margin - iw
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - ih) \ 2)
        SetWindowPos( ghCombo(i), 0, x, y, iw, ih, SWP_NOZORDER )
    next

    ' The three push buttons sit in a bar under the last row.
    dim as RECT rcLast = RowRect( hwnd, COMBO_COUNT - 1 )
    dim as long bw = BUTTON_W
    dim as long bh = BUTTON_H
    if pWindow then
        bw = pWindow->ScaleX(BUTTON_W)
        bh = pWindow->ScaleY(BUTTON_H)
    end if
    dim as long by = rcLast.bottom + ((bh) \ 2)
    dim as long bx = rc.left + margin
    dim as long gap = bw \ 8

    dim as HWND hAdd  = GetDlgItem( hwnd, IDC_FRMMAIN_BTN_ADD )
    dim as HWND hDel  = GetDlgItem( hwnd, IDC_FRMMAIN_BTN_DELETE )
    dim as HWND hAuto = GetDlgItem( hwnd, IDC_FRMMAIN_BTN_AUTOSIZE )
    if hAdd  then SetWindowPos( hAdd,  0, bx, by, bw, bh, SWP_NOZORDER )
    if hDel  then SetWindowPos( hDel,  0, bx + bw + gap, by, bw, bh, SWP_NOZORDER )
    if hAuto then SetWindowPos( hAuto, 0, bx + (2 * (bw + gap)), by, bw + (bw \ 2), bh, SWP_NOZORDER )
end sub


' ========================================================================================
' Callbacks -- the demo's whole purpose is to prove these fire when they should and stay
' silent when they should not, so every one of them prints.
' ========================================================================================
sub PsComboBox_DemoSelChange( byval hCombo as HWND, byval idxOld as long, byval idxNew as long )
    dim as string s = "SelChange   ctrl=" & str(GetDlgCtrlID(hCombo)) & _
                      "  " & str(idxOld) & " -> " & str(idxNew)
    ' The caption goes through a plain string first: PRINT on a DWSTRING, or on a
    ' string & wstring concat, interleaves nulls (Learnings.md).
    dim as wstring * 512 wszCaption
    wszCaption = PsComboBox_GetText( hCombo )
    s &= "   [" & str(wszCaption) & "]"
    print s

    ' Re-place every combobox on a user pick. This matters for exactly one row -- the
    ' CBO_TEXT_WHENSELECTED one, whose ideal width jumps the first time it is answered -- and
    ' it is the pattern a host needs when it has NOT turned on auto-size: the control owns its
    ' size, the host still owns its position, so a control that grew in place must be moved to
    ' stay right-aligned. Costs nothing for the rows whose width never moves.
    dim as HWND hForm = GetParent( hCombo )
    if hForm then
        frmMain_PositionCombos( hForm )
        InvalidateRect( hForm, NULL, TRUE )
    end if
end sub

sub PsComboBox_DemoDropDown( byval hCombo as HWND, byval isOpen as boolean )
    dim as string s = "DropDown    ctrl=" & str(GetDlgCtrlID(hCombo)) & "  open=" & str(isOpen)
    print s
end sub

' The mutable row rebuilds itself just-in-time on every open, which is what the opening edge
' of the DropDown callback is for. Here it only re-stamps the captions so the hook is visibly
' running; a real host would refetch whatever the list reflects.
sub PsComboBox_DemoJITDropDown( byval hCombo as HWND, byval isOpen as boolean )
    PsComboBox_DemoDropDown( hCombo, isOpen )
    if isOpen = false then exit sub
    dim as string s = "  (JIT hook: " & str(PsComboBox_GetCount(hCombo)) & " items about to be shown)"
    print s
end sub

' ========================================================================================
' The built-in painter replaced WHOLESALE: a square, outlined button with a single solid
' down-triangle instead of the stacked chevrons.
'
' Two contracts this honours, and both matter:
'   - it draws with the same font handed to PsComboBox_SetFont, because the widths were
'     measured with it;
'   - it uses the rects as handed to it rather than re-deriving insets from rcButton.
' The control has already filled the client with the mood's background before calling us.
' ========================================================================================
sub PsComboBox_DemoPaint( byval p as PSCOMBOBOX_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF clrLine = theme.ForeColor
    dim as COLORREF clrFill = theme.BackColorHot
    if p->isEnabled = false then
        clrLine = theme.ForeColorDisabled
        clrFill = theme.BackColor
    elseif p->isOpen then
        clrLine = theme.FocusAccent
        clrFill = theme.BackColorSelect
    elseif p->isHot then
        clrLine = theme.ForeColorSelect
    end if

    ' square chrome, outlined
    dim as RECT rcBtn = p->rcButton
    p->b->SetBackColor( clrFill )
    p->b->SetPenColor( clrLine )
    p->b->PaintRectFactory( @rcBtn, PS_SOLID, 1, 0 )

    ' caption
    if len( p->wszText ) > 0 then
        dim as RECT rcT = p->rcText
        p->b->SetFont( ghFont(GUIFONT_10) )
        p->b->SetColors( clrLine, clrFill )
        p->b->PaintText( p->wszText, @rcT, DT_LEFT or DT_END_ELLIPSIS )
    end if

    ' one solid down-triangle, drawn as a stack of shortening horizontal bars. An odd cell
    ' width ends on a single-pixel bar and so comes to a real point; an even one ends two
    ' pixels wide, which is why the default chevron width is odd.
    dim as long cl = p->rcChevron.left
    dim as long cr = p->rcChevron.right
    dim as long rows = ((cr - cl) + 1) \ 2
    dim as long ct = p->rcChevron.top + (((p->rcChevron.bottom - p->rcChevron.top) - rows) \ 2)
    p->b->SetBackColor( clrLine )
    for i as long = 0 to rows - 1
        dim as RECT rcBar
        SetRect( @rcBar, cl + i, ct + i, cr - i, ct + i + 1 )
        p->b->PaintRect( @rcBar )
    next

    ' The focus ring is the built-in painter's job too, so a callback that wants one draws it.
    '
    ' PaintRoundOutline, NOT PaintBorderRect. PaintBorderRect goes through PaintRectFactory,
    ' which ALWAYS fills the rect with _backcolor before stroking it -- so used here it would
    ' flood rcVisual with whatever colour was last set (the triangle's, just above) and wipe
    ' the chrome, the caption and the triangle, leaving a solid block. PaintRoundOutline
    ' exists for exactly this case: stroke over already-painted pixels without filling.
    ' Curvature 0 gives the square outline this square chrome wants.
    if p->isFocused then
        p->b->SetPenColor( theme.FocusAccent )
        p->b->PaintRoundOutline( @p->rcVisual, 0, 1 )
    end if
end sub


' ========================================================================================
' Buttons
' ========================================================================================
private sub frmMain_OnCommand_Add( byval hwnd as HWND )
    dim as DWSTRING wszNew = "Runtime item " & str(gNextItemSerial)
    gNextItemSerial += 1
    dim as long idx = PsComboBox_AddItem( ghCombo(COMBO_MUTABLE), wszNew, 500 + gNextItemSerial )
    dim as string s = "AddItem -> index " & str(idx) & _
                      ", count=" & str(PsComboBox_GetCount(ghCombo(COMBO_MUTABLE))) & _
                      ", sel=" & str(PsComboBox_GetCurSel(ghCombo(COMBO_MUTABLE)))
    print s
    frmMain_PositionCombos( hwnd )
    InvalidateRect( hwnd, NULL, TRUE )
end sub

' Deletes the FIRST item, which is the interesting case: it exercises the fix-up for a
' selection that sits after the deletion point, and (when item 0 is itself selected) the
' clear-to-minus-one rule.
private sub frmMain_OnCommand_Delete( byval hwnd as HWND )
    dim as long nBefore = PsComboBox_GetCurSel( ghCombo(COMBO_MUTABLE) )
    dim as boolean bDone = PsComboBox_DeleteItem( ghCombo(COMBO_MUTABLE), 0 )
    dim as string s = "DeleteItem(0)=" & str(bDone) & _
                      ", count=" & str(PsComboBox_GetCount(ghCombo(COMBO_MUTABLE))) & _
                      ", sel " & str(nBefore) & " -> " & str(PsComboBox_GetCurSel(ghCombo(COMBO_MUTABLE)))
    print s
    frmMain_PositionCombos( hwnd )
    InvalidateRect( hwnd, NULL, TRUE )
end sub

private sub frmMain_OnCommand_AutoSize( byval hwnd as HWND )
    dim as boolean bNew = (PsComboBox_GetAutoSize( ghCombo(COMBO_MUTABLE) ) = false)
    PsComboBox_SetAutoSize( ghCombo(COMBO_MUTABLE), bNew )
    dim as HWND hBtn = GetDlgItem( hwnd, IDC_FRMMAIN_BTN_AUTOSIZE )
    if hBtn then
        if bNew then
            SetWindowTextW( hBtn, "AutoSize: ON" )
        else
            SetWindowTextW( hBtn, "AutoSize: OFF" )
        end if
    end if
    dim as string s = "SetAutoSize(" & str(bNew) & ")"
    print s
    frmMain_PositionCombos( hwnd )
    InvalidateRect( hwnd, NULL, TRUE )
end sub

function frmMain_OnCommand( _
            byval hwnd    as HWND, _
            byval id      as long, _
            byval hwndCtl as HWND, _
            byval codeNotify as UINT _
            ) as LRESULT

    select case id
    case IDC_FRMMAIN_BTN_ADD
        if codeNotify = BN_CLICKED then frmMain_OnCommand_Add( hwnd )
    case IDC_FRMMAIN_BTN_DELETE
        if codeNotify = BN_CLICKED then frmMain_OnCommand_Delete( hwnd )
    case IDC_FRMMAIN_BTN_AUTOSIZE
        if codeNotify = BN_CLICKED then frmMain_OnCommand_AutoSize( hwnd )
    end select
    function = 0
end function


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
        frmMain_PositionCombos( hwnd )
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
        HANDLE_MSG (hwnd, WM_COMMAND,       frmMain_OnCommand)
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
            100, 100, 700, 540 _
            )

    gRows(0).wszLabel = "The reference screenshot"
    gRows(0).wszNote  = "two items, the longer one selected"
    gRows(1).wszLabel = "Arrow only"
    gRows(1).wszNote  = "SetShowText(false) -- width collapses to padding + chevron"
    gRows(2).wszLabel = "Disabled"
    gRows(2).wszNote  = "greyed through EnableWindow, so it swallows input for real"
    gRows(3).wszLabel = "Nothing selected"
    gRows(3).wszNote  = "nCurSel = -1 with a placeholder; width still comes from the items"
    gRows(4).wszLabel = "Mutable list"
    gRows(4).wszNote  = "the buttons below exercise the selection fix-up"
    gRows(5).wszLabel = "Custom painter"
    gRows(5).wszNote  = "square chrome and a solid triangle, drawn by the host"
    gRows(6).wszLabel = "Collapse until answered"
    gRows(6).wszNote  = "CBO_TEXT_WHENSELECTED + AutoSize -- arrows only until you pick"

    for i as long = 0 to COMBO_COUNT - 1
        ghCombo(i) = PsComboBox_Create( HWND_FRMMAIN, IDC_FRMMAIN_COMBO_FIRST + i )
        PsComboBox_SetFont( ghCombo(i), ghFont(GUIFONT_10) )
        PsComboBox_SetSelChangeCallback( ghCombo(i), @PsComboBox_DemoSelChange )
        PsComboBox_SetDropDownCallback( ghCombo(i), @PsComboBox_DemoDropDown )
    next

    ' Row 0 -- the reference screenshot. Programmatic SetCurSel, so NO SelChange line should
    ' appear in the console for it at startup.
    PsComboBox_AddItem( ghCombo(0), "Add to Existing Window", 1 )
    PsComboBox_AddItem( ghCombo(0), "Open a New Window", 2 )
    PsComboBox_SetCurSel( ghCombo(0), 0 )

    ' Row 1 -- arrow only. It still HAS a selection and the list still checkmarks it; only
    ' the button's caption is suppressed.
    PsComboBox_AddItem( ghCombo(1), "Add to Existing Window", 1 )
    PsComboBox_AddItem( ghCombo(1), "Open a New Window", 2 )
    PsComboBox_SetCurSel( ghCombo(1), 1 )
    PsComboBox_SetShowText( ghCombo(1), false )

    ' Row 2 -- disabled WITH a selection, which is the case the disabled colors exist for:
    ' it must still read as showing a value.
    PsComboBox_AddItem( ghCombo(2), "Add to Existing Window", 1 )
    PsComboBox_AddItem( ghCombo(2), "Open a New Window", 2 )
    PsComboBox_SetCurSel( ghCombo(2), 0 )
    PsComboBox_SetEnabled( ghCombo(2), false )

    ' Row 3 -- no selection, with a placeholder and one disabled item. The placeholder is
    ' NOT measured, so the button is exactly as wide as it will be once something is picked.
    PsComboBox_AddItem( ghCombo(3), "Add to Existing Window", 1 )
    PsComboBox_AddItem( ghCombo(3), "Open a New Window", 2 )
    PsComboBox_AddItem( ghCombo(3), "Not Available Right Now", 3 )
    PsComboBox_SetItemEnabled( ghCombo(3), 2, false )
    PsComboBox_SetPlaceholderText( ghCombo(3), "Choose an action..." )

    ' Row 4 -- the mutable list, driven by the buttons. It rebuilds just-in-time on open.
    PsComboBox_AddItem( ghCombo(COMBO_MUTABLE), "First item", 501 )
    PsComboBox_AddItem( ghCombo(COMBO_MUTABLE), "Second item", 502 )
    PsComboBox_AddItem( ghCombo(COMBO_MUTABLE), "Third item", 503 )
    PsComboBox_SetCurSel( ghCombo(COMBO_MUTABLE), 1 )
    PsComboBox_SetDropDownCallback( ghCombo(COMBO_MUTABLE), @PsComboBox_DemoJITDropDown )

    ' Row 5 -- the built-in painter replaced wholesale, plus square corners so the custom
    ' chrome and the control's own geometry agree.
    PsComboBox_AddItem( ghCombo(5), "Add to Existing Window", 1 )
    PsComboBox_AddItem( ghCombo(5), "Open a New Window", 2 )
    PsComboBox_SetCurSel( ghCombo(5), 0 )
    PsComboBox_SetCornerCurvature( ghCombo(5), 0 )
    PsComboBox_SetPaintCallback( ghCombo(5), @PsComboBox_DemoPaint )
    ' The list is themed independently here, which also claims it: later PsComboBox_SetColors
    ' calls will no longer re-derive the dropdown's colors for this control.
    dim as PSPOPUPMENU_COLORS lc
    lc.BackColor         = theme.BackColor
    lc.ForeColor         = theme.ForeColor
    lc.BackColorHot      = theme.BackColorSelect
    lc.ForeColorHot      = theme.ForeColorSelect
    lc.ForeColorDisabled = theme.ForeColorDisabled
    lc.BorderColor       = theme.FocusAccent
    lc.SeparatorColor    = theme.Divider
    PsComboBox_SetListColors( ghCombo(5), @lc )

    ' Row 6 -- the requested behaviour: no selection, so the button is JUST the chevron; make
    ' a choice and it becomes an ordinary captioned combobox. Auto-size is on so the control
    ' applies the new width itself; the shared SelChange handler re-places it so it stays
    ' right-aligned as it grows. Deliberately NO placeholder -- in this mode one could never
    ' be seen, since the only state that would show it is the collapsed, caption-less one.
    PsComboBox_AddItem( ghCombo(COMBO_WHENSEL), "Add to Existing Window", 1 )
    PsComboBox_AddItem( ghCombo(COMBO_WHENSEL), "Open a New Window", 2 )
    PsComboBox_SetTextMode( ghCombo(COMBO_WHENSEL), CBO_TEXT_WHENSELECTED )
    PsComboBox_SetAutoSize( ghCombo(COMBO_WHENSEL), true )

    ' The three push buttons. Plain Win32 BUTTONs -- this demo is about the combobox.
    pWindow->AddControl( "Button", HWND_FRMMAIN, IDC_FRMMAIN_BTN_ADD, "Add item", 0, 0, 10, 10 )
    pWindow->AddControl( "Button", HWND_FRMMAIN, IDC_FRMMAIN_BTN_DELETE, "Delete first", 0, 0, 10, 10 )
    pWindow->AddControl( "Button", HWND_FRMMAIN, IDC_FRMMAIN_BTN_AUTOSIZE, "AutoSize: OFF", 0, 0, 10, 10 )

    dim as long iw, ih
    PsComboBox_GetIdealSize( ghCombo(0), iw, ih )
    dim as string s = "row 0 ideal size: " & str(iw) & "x" & str(ih)
    print s
    PsComboBox_GetIdealSize( ghCombo(1), iw, ih )
    s = "row 1 ideal size (arrow only): " & str(iw) & "x" & str(ih)
    print s

    frmMain_PositionCombos( HWND_FRMMAIN )
    for i as long = 0 to COMBO_COUNT - 1
        ShowWindow( ghCombo(i), SW_SHOW )
    next

    ' Self-gated on PSCOMBOBOX_SELFTEST=1.
    PsComboBox_RunSelfTest( HWND_FRMMAIN )

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Give the first combobox the focus. A real dialog does this in WM_INITDIALOG; an ordinary
    ' window must do it itself, and without it IsDialogMessage ignores the FIRST Tab entirely --
    ' it only acts when the focused window is a DESCENDANT of the window it is passed, and at
    ' startup the focus is on the form.
    '
    ' Note it starts on a COMBOBOX rather than on one of the plain buttons below, deliberately:
    ' those buttons are what masked the WS_EX_CONTROLPARENT bug for so long, since Tab moved
    ' between them and navigation looked fine while never once reaching a combobox.
    SetFocus( ghCombo(0) )

    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' THE PUMP CONTRACT. One call per combobox, and it must come FIRST: it owns the
        ' dropdown's keyboard navigation and its outside-click dismissal, and it arms the
        ' guard that stops a click on an open combobox's own button from reopening the list
        ' it just dismissed. Each call returns immediately when that control's list is closed.
        dim as boolean bEaten = false
        for i as long = 0 to COMBO_COUNT - 1
            if PsComboBox_FilterMessage( ghCombo(i), @uMsg ) then
                bEaten = true
                exit for
            end if
        next
        if bEaten then continue do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then

            ' IsDialogMessage is what makes Tab move between the comboboxes. A host without
            ' it still gets full mouse behaviour and, once a combobox has focus, the arrow
            ' keys and Enter.
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
