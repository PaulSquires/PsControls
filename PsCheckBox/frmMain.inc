'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' One settings row: the label the form draws, and the checkbox that sits at its right.
type DEMO_ROW
    wszLabel as DWSTRING
    wszNote  as DWSTRING          ' small print under the label, "" for none
end type
dim shared gRows(0 to CHECKBOX_COUNT - 1) as DEMO_ROW

' How many times each checkbox has notified, drawn back into the row's small print. Two things
' are visible in that number: a cancelled gesture (press, slide off, release) does NOT count, and
' neither does a programmatic PsCheckBox_SetChecked.
dim shared gChanges(0 to CHECKBOX_COUNT - 1) as long


' ========================================================================================
' Row geometry. The checkbox cell is right-aligned against the margin and sized to the control's
' own ideal size -- except the rows that deliberately want more or less than that.
' ========================================================================================
#define ROW_HEIGHT      46
#define ROW_TOP         12
#define ROW_MARGIN      22
#define WIDE_ROW_W     320       ' the settings-row width for ROW_RIGHT_ROW
#define ROW_SLACK      120       ' extra width for the two alignment rows and the hover row

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
' The form draws the labels and the hairlines; each checkbox paints its own cell. There is no
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

    for i as long = 0 to CHECKBOX_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )

        ' label
        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right -= margin
        rcText.bottom = rcRow.top + ((rcRow.bottom - rcRow.top) \ 2)
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )

        ' small print, with the live notification count appended
        dim as DWSTRING wszNote = gRows(i).wszNote
        if gChanges(i) > 0 then wszNote = wszNote + "   [changes: " + wstr(gChanges(i)) + "]"
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
' Place every checkbox at the right end of its row.
'
' MOST rows get exactly the control's own ideal size, which packs the box against the caption.
' Four do not, and each exception is the point of its row:
'
'   ROW_RIGHT_ROW   a fixed, much wider cell. Same control, same BOXALIGN_RIGHT setting as
'                   ROW_RIGHT_IDEAL directly above it -- the ONLY difference is the width the
'                   host hands it, and that is what turns a packed checkbox into a settings row
'                   with the box hard against the right edge.
'   ROW_ALIGNC/R    slack past ideal. Sized to ideal the caption span IS the caption, so LEFT,
'                   CENTER and RIGHT all render identically and the rows demonstrate nothing --
'                   which is exactly how PsButton's demo first shipped. Slack is what makes
'                   alignment visible at all.
'   ROW_HOTROW      slack too, so the hover highlight is a visible band rather than a hairline
'                   around the text.
'   ROW_ELLIPSIS    55% of what it asked for, so DT_END_ELLIPSIS has something to do. Note the
'                   ideal size itself is unaffected -- a host that sizes with GetIdealSize can
'                   never see an ellipsis, which is one of the self-test's claims.
' ========================================================================================
sub frmMain_PositionChecks( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    dim as long wide   = WIDE_ROW_W
    dim as long slack  = ROW_SLACK
    if pWindow then
        margin = pWindow->ScaleX(ROW_MARGIN)
        wide   = pWindow->ScaleX(WIDE_ROW_W)
        slack  = pWindow->ScaleX(ROW_SLACK)
    end if

    for i as long = 0 to CHECKBOX_COUNT - 1
        if IsWindow(ghCheck(i)) = 0 then continue for
        dim as RECT rcRow = RowRect( hwnd, i )

        dim as long iw, ih
        PsCheckBox_GetIdealSize( ghCheck(i), iw, ih )
        if (iw <= 0) orelse (ih <= 0) then continue for

        select case i
        case ROW_RIGHT_ROW
            iw = wide
        case ROW_ALIGNC, ROW_ALIGNR, ROW_HOTROW
            iw += slack
        case ROW_ELLIPSIS
            iw = (iw * 55) \ 100
        end select

        dim as long x = rc.right - margin - iw
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - ih) \ 2)
        SetWindowPos( ghCheck(i), 0, x, y, iw, ih, SWP_NOZORDER )
    next
end sub


' ========================================================================================
' Callbacks -- the demo's whole purpose is to prove these fire when they should and stay silent
' when they should not, so every one of them prints.
'
' TWO THINGS TO TRY THAT SHOULD PRODUCE NO LINE AT ALL:
'   - press a checkbox, SLIDE OFF IT, and release. That is the press/cancel gesture the mouse
'     capture exists to protect.
'   - watch rows 3, 4 and 5 at startup: their states were set with PsCheckBox_SetChecked, which is
'     silent. Nothing is printed and no count appears.
' ========================================================================================
sub PsCheckBox_DemoChanged( byval hCheckBox as HWND, byval isChecked as boolean )
    dim as long idx = GetDlgCtrlID(hCheckBox) - IDC_FRMMAIN_CHK_FIRST
    if (idx >= 0) andalso (idx < CHECKBOX_COUNT) then gChanges(idx) += 1

    ' The caption goes through a plain string first: PRINT on a DWSTRING, or on a
    ' string & wstring concat, interleaves nulls (Learnings.md).
    dim as wstring * 512 wszCaption
    wszCaption = PsCheckBox_GetText( hCheckBox )
    dim as string s = "CheckChanged   ctrl=" & str(GetDlgCtrlID(hCheckBox)) & _
                      "  checked=" & str(isChecked) & "   [" & str(wszCaption) & "]"
    print s

    dim as HWND hForm = GetParent( hCheckBox )
    if hForm then InvalidateRect( hForm, NULL, TRUE )
end sub

' Only the down-message is reported, and only for one row: printing every WM_MOUSEMOVE would bury
' everything else. Returns FALSE so the control's own handling proceeds -- suppressing the DOWN
' would suppress the press itself.
function PsCheckBox_DemoMessage( byval m as PSCHECKBOX_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false
    if m->uMsg = WM_LBUTTONDOWN then
        dim as string s = "  (message callback saw WM_LBUTTONDOWN on ctrl=" & _
                          str(GetDlgCtrlID(m->hCheckBox)) & ")"
        print s
    end if
    return false
end function

' Consulted only because this checkbox has no tooltip text of its own. Answering "" here would
' show no tip at all -- there is no caption fallback in this control.
function PsCheckBox_DemoTooltip( byval hCheckBox as HWND ) as DWSTRING
    return "Supplied on demand by the tooltip callback"
end function


' ========================================================================================
' The built-in painter replaced WHOLESALE: a square swatch instead of a glyph, filled when
' checked and outlined when not, with the caption in bold beside it.
'
' THE ONE TRAP THIS DEMONSTRATES AVOIDING. PaintBorderRect goes through PaintRectFactory, which
' ALWAYS fills the rect with _backcolor before stroking it -- so reaching for it to draw an
' outline floods everything underneath. That mistake has now been made three separate times in
' this family (PsToggle, PsComboBox, PsNumericUpDown), every time by copying a sibling's callback.
' PaintRoundOutline is the one that strokes without filling; PaintRectFactory with an explicit
' fill colour is the one to use when the fill IS wanted, which is the case for the swatch below.
'
' frmMain_Show probes this callback with PsCheckBox_CountRenderedTones for exactly that reason.
' ========================================================================================
sub PsCheckBox_DemoPaint( byval p as PSCHECKBOX_PAINTINFO ptr )
    if p = 0 then exit sub

    dim as COLORREF clrLine = theme.ForeColor
    dim as COLORREF clrBack = theme.BackColor
    if p->isEnabled = false then
        clrLine = theme.ForeColorDisabled
    elseif p->isPressed then
        clrLine = theme.ForeColorSelect
    elseif p->isHot then
        clrLine = theme.ForeColorSelect
    end if

    ' The swatch. Filled when checked, hollow when not -- so the state is carried by the FILL
    ' here rather than by a glyph, which is a legitimate thing for a host painter to decide.
    dim as RECT rcB = p->rcBox
    if p->isChecked then
        p->b->SetBackColor( theme.BackColorSelect )
    else
        p->b->SetBackColor( clrBack )
    end if
    p->b->SetPenColor( clrLine )
    p->b->PaintRectFactory( @rcB, PS_SOLID, 1, 0 )

    ' the caption, using the SAME font family the control measured with -- a wider font here
    ' would make that measurement a lie and the text would clip
    if IsRectEmpty( @p->rcText ) = 0 then
        dim as RECT rcT = p->rcText
        p->b->SetFont( ghFont(GUIFONTBOLD_10) )
        p->b->SetColors( clrLine, clrBack )
        p->b->PaintText( p->wszText, @rcT, DT_LEFT or DT_END_ELLIPSIS )
    end if

    ' The focus ring is the built-in painter's job too, so a callback that wants one draws it.
    ' Curvature 0 gives the square outline this square swatch wants.
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
        frmMain_PositionChecks( hwnd )
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
            100, 60, 900, 780 _
            )

    gRows(0).wszLabel  = "Box on the LEFT"
    gRows(0).wszNote   = "the default -- press and slide off: nothing toggles"
    gRows(ROW_RIGHT_IDEAL).wszLabel = "Box on the RIGHT, ideal width"
    gRows(ROW_RIGHT_IDEAL).wszNote  = "packed: the box sits one gap from the caption"
    gRows(ROW_RIGHT_ROW).wszLabel   = "Box on the RIGHT, row width"
    gRows(ROW_RIGHT_ROW).wszNote    = "SAME settings, wider cell -- the box pins to the right edge"
    gRows(ROW_CHECKED).wszLabel     = "Checked at create"
    gRows(ROW_CHECKED).wszNote      = "set with SetChecked, which is SILENT: no count appears"
    gRows(ROW_DIS_ON).wszLabel      = "Disabled, checked"
    gRows(ROW_DIS_ON).wszNote       = "a disabled tick still reads as ticked -- its own colour"
    gRows(ROW_DIS_OFF).wszLabel     = "Disabled, unchecked"
    gRows(ROW_DIS_OFF).wszNote      = "through EnableWindow, so it eats input and Tab skips it"
    gRows(ROW_NOCAPTION).wszLabel   = "No caption"
    gRows(ROW_NOCAPTION).wszNote    = "no gap charged, and the ideal height drops to the box"
    gRows(ROW_ALIGNC).wszLabel      = "CHK_TEXTALIGN_CENTER"
    gRows(ROW_ALIGNC).wszNote       = "given slack past ideal, or the span IS the caption"
    gRows(ROW_ALIGNR).wszLabel      = "CHK_TEXTALIGN_RIGHT"
    gRows(ROW_ALIGNR).wszNote       = "same slack -- the box has not moved in either"
    gRows(ROW_HOTROW).wszLabel      = "Hover row highlight"
    gRows(ROW_HOTROW).wszNote       = "the flat default overridden by ONE colour field"
    gRows(ROW_CUSTOM).wszLabel      = "Custom painter"
    gRows(ROW_CUSTOM).wszNote       = "square swatch drawn by the host; callback prints the DOWN"
    gRows(ROW_TOOLTIP).wszLabel     = "Tooltip"
    gRows(ROW_TOOLTIP).wszNote      = "no caption, so the callback is asked -- hover and wait"
    gRows(ROW_ELLIPSIS).wszLabel    = "Forced narrow"
    gRows(ROW_ELLIPSIS).wszNote     = "sized to 55% of ideal, so DT_END_ELLIPSIS has work to do"
    gRows(ROW_GLYPHS).wszLabel      = "Custom glyphs"
    gRows(ROW_GLYPHS).wszNote       = "E739 / E73E instead of the default E739 / E73A pair"

    for i as long = 0 to CHECKBOX_COUNT - 1
        ghCheck(i) = PsCheckBox_Create( HWND_FRMMAIN, IDC_FRMMAIN_CHK_FIRST + i )
        PsCheckBox_SetFont( ghCheck(i), ghFont(GUIFONT_10) )
        PsCheckBox_SetGlyphFont( ghCheck(i), ghFont(SYMBOLFONT_12) )
        PsCheckBox_SetCheckChangedCallback( ghCheck(i), @PsCheckBox_DemoChanged )
        PsCheckBox_SetText( ghCheck(i), "Enable dark mode" )
    next

    ' Row 0 -- the plain case, box on the left. Everything else is a variation on it.

    ' Rows 1 and 2 -- THE SAME CONTROL, THE SAME SETTINGS. Only the width the host gives them
    ' differs, and that is the whole argument for pinning the box to the padding rather than
    ' packing it against the caption: one model, both looks.
    PsCheckBox_SetBoxAlign( ghCheck(ROW_RIGHT_IDEAL), CHK_BOXALIGN_RIGHT )
    PsCheckBox_SetBoxAlign( ghCheck(ROW_RIGHT_ROW), CHK_BOXALIGN_RIGHT )

    ' Row 3 -- checked before it is ever shown. SetChecked is SILENT, so no notification is
    ' printed and the row's change count stays absent until you click it.
    PsCheckBox_SetChecked( ghCheck(ROW_CHECKED), true )

    ' Rows 4 / 5 -- both disabled states, because each has its own colour and a disabled ticked
    ' box must still read as ticked.
    PsCheckBox_SetText( ghCheck(ROW_DIS_ON), "Cannot change this" )
    PsCheckBox_SetChecked( ghCheck(ROW_DIS_ON), true )
    PsCheckBox_SetEnabled( ghCheck(ROW_DIS_ON), false )

    PsCheckBox_SetText( ghCheck(ROW_DIS_OFF), "Cannot change this either" )
    PsCheckBox_SetEnabled( ghCheck(ROW_DIS_OFF), false )

    ' Row 6 -- box only. No caption means no gap is charged, and the ideal HEIGHT drops to the
    ' box: it is visibly shorter than its neighbours, which is honest rather than a bug (nothing
    ' in it is as tall as a line of text).
    PsCheckBox_SetText( ghCheck(ROW_NOCAPTION), "" )

    ' Rows 7 / 8 -- caption alignment WITHIN THE SPAN. frmMain_PositionChecks gives both slack
    ' past their ideal width; the box stays pinned to its padding in both, which is the point.
    PsCheckBox_SetText( ghCheck(ROW_ALIGNC), "Centred" )
    PsCheckBox_SetTextAlign( ghCheck(ROW_ALIGNC), CHK_TEXTALIGN_CENTER )

    PsCheckBox_SetText( ghCheck(ROW_ALIGNR), "Right" )
    PsCheckBox_SetTextAlign( ghCheck(ROW_ALIGNR), CHK_TEXTALIGN_RIGHT )

    ' Row 9 -- the hover highlight. The control ships FLAT: BackColorHot and BackColorPressed are
    ' defaulted equal to BackColor. Setting them is all it takes to get a menu-style row.
    scope
        dim as PSCHECKBOX_COLORS c
        PsCheckBox_GetColors( ghCheck(ROW_HOTROW), @c )
        c.BackColorHot     = theme.BackColorHot
        c.BackColorPressed = theme.BackColorSelect
        PsCheckBox_SetColors( ghCheck(ROW_HOTROW), @c )
    end scope
    PsCheckBox_SetText( ghCheck(ROW_HOTROW), "Highlight the whole row" )

    ' Row 10 -- the built-in painter replaced wholesale.
    PsCheckBox_SetText( ghCheck(ROW_CUSTOM), "Host-drawn swatch" )
    PsCheckBox_SetPaintCallback( ghCheck(ROW_CUSTOM), @PsCheckBox_DemoPaint )
    PsCheckBox_SetMessageCallback( ghCheck(ROW_CUSTOM), @PsCheckBox_DemoMessage )

    ' Row 11 -- no caption, but a tooltip: with nothing on screen but a box, the tip is the only
    ' way to find out what it does. Deliberately given NO text of its own, so the CALLBACK is the
    ' thing being exercised.
    PsCheckBox_SetText( ghCheck(ROW_TOOLTIP), "" )
    PsCheckBox_SetTooltipCallback( ghCheck(ROW_TOOLTIP), @PsCheckBox_DemoTooltip )

    ' Row 12 -- a caption far too long for the width the host will give it.
    PsCheckBox_SetText( ghCheck(ROW_ELLIPSIS), "Automatically restore the previous session" )

    ' Row 13 -- the codepoints swapped. E739 stays as the empty box; E73E (a bare CheckMark, the
    ' glyph PsButton's demo puts on its OK button) replaces the composite E73A, so checking it
    ' shows a tick with no box around it.
    PsCheckBox_SetText( ghCheck(ROW_GLYPHS), "Bare tick instead of a box" )
    PsCheckBox_SetGlyphUnchecked( ghCheck(ROW_GLYPHS), wchr(&hE739) )
    PsCheckBox_SetGlyphChecked( ghCheck(ROW_GLYPHS), wchr(&hE73E) )

    dim as long iw, ih
    PsCheckBox_GetIdealSize( ghCheck(0), iw, ih )
    dim as string s = "row 0 ideal size (box + caption): " & str(iw) & "x" & str(ih)
    print s
    PsCheckBox_GetIdealSize( ghCheck(ROW_NOCAPTION), iw, ih )
    s = "row 6 ideal size (box only):      " & str(iw) & "x" & str(ih)
    print s

    frmMain_PositionChecks( HWND_FRMMAIN )
    for i as long = 0 to CHECKBOX_COUNT - 1
        ShowWindow( ghCheck(i), SW_SHOW )
    next

    ' Self-gated on PSCHECKBOX_SELFTEST=1.
    PsCheckBox_RunSelfTest( HWND_FRMMAIN )

    ' The control's own self-test can only probe the BUILT-IN painter -- it knows nothing about
    ' this demo. The host paint callback above is exactly the place the family's PaintBorderRect
    ' flood has shipped from three times, so it is probed HERE, by the host, which is also the
    ' pattern a real host should copy for its own painter.
    if environ("PSCHECKBOX_SELFTEST") = "1" then
        dim as long tCon  = PsCheckBox_CountRenderedTones( ghCheck(ROW_CUSTOM), CHK_PART_CONTROL )
        dim as long tTxt  = PsCheckBox_CountRenderedTones( ghCheck(ROW_CUSTOM), CHK_PART_TEXT )
        dim as string sHost = "PASS  "
        if (tCon <= 1) orelse (tTxt <= 1) then sHost = "FAIL  "
        sHost &= "demo paint callback does not flood   [content tones=" & str(tCon) & _
                 " caption tones=" & str(tTxt) & "]"
        print sHost

        ' ...and that the host painter, which carries the state in its FILL rather than in a
        ' glyph, still moves the pixels when the state changes. Same claim as the control's own
        ' probe, made against a completely different renderer.
        PsCheckBox_SetChecked( ghCheck(ROW_CUSTOM), false )
        dim as ulong hOff = PsCheckBox_HashRenderedPart( ghCheck(ROW_CUSTOM), CHK_PART_BOX )
        PsCheckBox_SetChecked( ghCheck(ROW_CUSTOM), true )
        dim as ulong hOn  = PsCheckBox_HashRenderedPart( ghCheck(ROW_CUSTOM), CHK_PART_BOX )
        PsCheckBox_SetChecked( ghCheck(ROW_CUSTOM), false )
        dim as string sHash = "PASS  "
        if hOff = hOn then sHash = "FAIL  "
        sHash &= "demo paint callback renders the state   [off=&h" & hex(hOff) & _
                 " on=&h" & hex(hOn) & "]"
        print sHash
    end if

    ' A self-test run closes the window so the process exits and the suite can run
    ' unattended. Without this the demo sits in its pump and the console output stays
    ' BUFFERED -- killing the run then loses every line, the summary included.
    if environ("PSCHECKBOX_SELFTEST") = "1" then
        DestroyWindow( pWindow->hWindow )
        return 0
    end if

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Give the first checkbox the focus. A real dialog does this for you in WM_INITDIALOG; an
    ' ordinary window must do it itself, and without it IsDialogMessage ignores the FIRST Tab
    ' entirely -- it only acts when the focused window is a DESCENDANT of the window it is
    ' passed, and at startup the focus is on the form itself. The symptom is "Tab does nothing
    ' until you click a control", which reads as the tabstops being broken.
    SetFocus( ghCheck(0) )


    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    '
    ' THERE IS NO PsCheckBox_FilterMessage. This control owns no popup and no second top-level
    ' window, so unlike PsComboBox / PsNumericUpDown / PsTextBox it adds NO pump obligation.
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then

            ' IsDialogMessage is what makes Tab move between the checkboxes. A host without it
            ' still gets full mouse behaviour and, once a control has focus, Space and Enter.
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
