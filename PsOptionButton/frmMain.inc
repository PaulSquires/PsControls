'    PsOptionButton - reusable owner-drawn option button (radio button) control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.


' One row of the demo. wszHeading is drawn only on the first row of a group; the rest of a
' group's rows leave it empty, which is what makes the column read as six groups rather than
' fifteen unrelated switches.
type DEMO_ROW
    wszHeading as DWSTRING
    wszNote    as DWSTRING
    nGroup     as long
end type

dim shared gRows( 0 to OPTION_COUNT - 1 ) as DEMO_ROW

' Per-group change counters, drawn next to each heading. A group's counter goes up TWICE per
' user pick -- once for the loser, once for the winner -- which makes the both-edges contract
' visible rather than merely documented.
dim shared gGroupChanges( 0 to 8 ) as long

' One width per GROUP, computed in frmMain_PositionOptions. See the header there for why the
' members of a group must not each take their own ideal width.
dim shared gGroupWidth( 0 to 8 ) as long

dim shared as HWND ghBtnOK, ghBtnCancel


#define ROW_HEIGHT    42
#define ROW_TOP       16
#define ROW_MARGIN    24
#define WIDE_ROW_W   340
#define ROW_SLACK    140
#define BTN_BAR_H     56
' The left-hand heading/note column. Wide enough for the longest note at the 9pt font -- at 300
' every note was silently truncated mid-word ("...wrapping at bot"), which is easy to miss
' because DrawText clips without complaining. The widest control (Units, WIDE_ROW_W) still
' leaves a gap: 900 client - 24 margin - 340 = 536.
#define NOTE_COL_W   470


' The row's rectangle in client coordinates. DPI-scaled through the CWindow, so the demo lays
' out correctly at 150% and 175% without the numbers above changing.
private function RowRect( byval hwnd as HWND, byval idx as long ) as RECT
    dim as RECT rc
    SetRectEmpty( @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )
    if pWindow = 0 then return rc

    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )

    dim as long y = pWindow->ScaleY( ROW_TOP ) + (idx * pWindow->ScaleY( ROW_HEIGHT ))
    SetRect( @rc, pWindow->ScaleX( ROW_MARGIN ), y, _
                  rcClient.right - pWindow->ScaleX( ROW_MARGIN ), _
                  y + pWindow->ScaleY( ROW_HEIGHT ) )
    return rc
end function


' Is this row the FIRST of its group? Used for the heading and the divider.
private function IsGroupStart( byval idx as long ) as boolean
    if idx <= 0 then return true
    return (gRows(idx).nGroup <> gRows(idx - 1).nGroup)
end function


' ========================================================================================
' The form draws its own labels; the option buttons paint themselves. There is no container
' window and no group box -- a group here is a shared id, not a frame.
' ========================================================================================
private function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )

    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )
    if pWindow = 0 then
        b.EndDoubleBuffer()
        return 0
    end if

    for i as long = 0 to OPTION_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )

        if IsGroupStart( i ) then
            ' A hairline above every group but the first.
            if i > 0 then
                b.SetPenColor( theme.Divider )
                b.PaintLine( rcRow.left, rcRow.top, rcRow.right, rcRow.top, 1 )
            end if

            dim as RECT rcH = rcRow
            rcH.right = rcH.left + pWindow->ScaleX( NOTE_COL_W )
            rcH.bottom = rcH.top + pWindow->ScaleY( 20 )
            rcH.top += pWindow->ScaleY( 4 )

            dim as DWSTRING wszH = gRows(i).wszHeading & _
                                   "   [changes: " & str( gGroupChanges( gRows(i).nGroup ) ) & "]"
            b.SetFont( ghFont(GUIFONTBOLD_10) )
            b.SetColors( theme.ForeColor, theme.BackColor )
            b.PaintText( wszH, @rcH, DT_LEFT )
        end if

        if len( gRows(i).wszNote ) then
            dim as RECT rcN = rcRow
            rcN.right = rcN.left + pWindow->ScaleX( NOTE_COL_W )
            rcN.top  += pWindow->ScaleY( 20 )
            rcN.bottom = rcN.top + pWindow->ScaleY( 18 )
            b.SetFont( ghFont(GUIFONT_9) )
            b.SetColors( theme.ForeColorDisabled, theme.BackColor )
            b.PaintText( gRows(i).wszNote, @rcN, DT_LEFT )
        end if
    next

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Place every option button.
'
' EVERY MEMBER OF A GROUP GETS ONE SHARED WIDTH -- the widest ideal width in that group -- and
' therefore one shared LEFT edge. That is the whole point of this routine and it is not a
' cosmetic preference.
'
' Giving each button its own ideal width and right-aligning them all to the same margin is the
' obvious thing to write, and it staircases: a longer caption makes a wider control, whose left
' edge sits further left, so the circles of one group march diagonally down the form instead of
' forming a column. A radio group has to read as one question with several answers, and a ragged
' left edge destroys that.
'
' Taking the max rather than a fixed number means the group still sizes itself to its own
' content. The extra width lands in each button's caption SPAN (rcText runs from the box cell to
' the far padding), so with the default OPT_TEXTALIGN_LEFT the captions stay tight against their
' circles and nothing looks stretched.
'
' It also fixes the two groups that already looked right for the wrong reason: OPT_BOXALIGN_RIGHT
' groups line their circles up only while every member happens to share a width, which was true
' here by hand-assignment rather than by rule.
' ========================================================================================
private sub frmMain_PositionOptions( byval hwnd as HWND )
    dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )
    if pWindow = 0 then exit sub

    dim as long wide  = pWindow->ScaleX( WIDE_ROW_W )
    dim as long slack = pWindow->ScaleX( ROW_SLACK )

    ' ---- pass 1: the widest ideal width in each group ---------------------------------
    for g as long = 0 to ubound(gGroupWidth)
        gGroupWidth(g) = 0
    next

    for i as long = 0 to OPTION_COUNT - 1
        if ghOpt(i) = 0 then continue for

        dim as long iw, ih
        PsOptionButton_GetIdealSize( ghOpt(i), iw, ih )
        if iw <= 0 then continue for

        select case gRows(i).nGroup
        case GRP_UNITS
            ' The settings-row look: a fixed width wider than any caption needs.
            iw = wide
        case GRP_ALIGN
            ' DELIBERATE SLACK. Sized to its own ideal width the caption span IS the caption, and
            ' LEFT / CENTER / RIGHT all render identically -- PsButton's demo shipped exactly that
            ' mistake and demonstrated nothing (C:\dev\Learnings.md).
            iw += slack
        end select

        dim as long ng = gRows(i).nGroup
        if (ng >= 0) andalso (ng <= ubound(gGroupWidth)) then
            if iw > gGroupWidth(ng) then gGroupWidth(ng) = iw
        end if
    next

    ' ---- pass 2: place every member at that shared width, hence a shared left edge ------
    for i as long = 0 to OPTION_COUNT - 1
        if ghOpt(i) = 0 then continue for

        dim as RECT rcRow = RowRect( hwnd, i )
        dim as long iw, ih
        PsOptionButton_GetIdealSize( ghOpt(i), iw, ih )
        if ih <= 0 then continue for

        dim as long ng = gRows(i).nGroup
        if (ng < 0) orelse (ng > ubound(gGroupWidth)) then continue for
        dim as long gw = gGroupWidth(ng)
        if gw <= 0 then continue for

        ' Height stays per-control: a caption-less button is legitimately shorter, because the
        ' content height ignores the font when there is no text.
        dim as long x = rcRow.right - gw
        dim as long y = rcRow.top + ((rcRow.bottom - rcRow.top - ih) \ 2)
        SetWindowPos( ghOpt(i), 0, x, y, gw, ih, SWP_NOZORDER or SWP_NOACTIVATE )
    next

    ' The two plain Win32 buttons, bottom right.
    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )
    dim as long bw = pWindow->ScaleX( 96 )
    dim as long bh = pWindow->ScaleY( 28 )
    dim as long by = rcClient.bottom - pWindow->ScaleY( BTN_BAR_H ) + ((pWindow->ScaleY( BTN_BAR_H ) - bh) \ 2)
    dim as long bx = rcClient.right - pWindow->ScaleX( ROW_MARGIN ) - bw
    if ghBtnCancel then SetWindowPos( ghBtnCancel, 0, bx, by, bw, bh, SWP_NOZORDER or SWP_NOACTIVATE )
    bx -= (bw + pWindow->ScaleX( 8 ))
    if ghBtnOK then SetWindowPos( ghBtnOK, 0, bx, by, bw, bh, SWP_NOZORDER or SWP_NOACTIVATE )
end sub


' ========================================================================================
' Demo callbacks
' ========================================================================================

' BOTH EDGES ARRIVE HERE. The loser fires first with isChecked = false, then the winner with
' true -- so this handler is told about the option being vacated as well as the one being taken,
' without having to remember which was which.
private sub PsOptionButton_DemoChanged( byval hOptionButton as HWND, byval isChecked as boolean )
    dim as long nGroup = PsOptionButton_GetGroupID( hOptionButton )
    if (nGroup >= 0) andalso (nGroup <= 8) then gGroupChanges( nGroup ) += 1

    ' Copy through a fixed wstring before printing: "print" on a DWSTRING, or on a
    ' string & wstring concat, interleaves nulls (C:\dev\Learnings.md).
    dim as wstring * 256 wszCap = PsOptionButton_GetText( hOptionButton )
    dim as string sLine = "changed  group=" & str(nGroup) & _
                          "  id=" & str( PsOptionButton_GetID( hOptionButton ) ) & _
                          "  checked=" & str(isChecked) & _
                          "  caption=" & wszCap
    print sLine

    ' Reading the group from inside the callback is COHERENT at both edges: both state writes
    ' land before either callback runs, so this never reports two checked members or none.
    print "         group now answers: id=" & str( PsOptionButton_FindCheckedID( HWND_FRMMAIN, nGroup ) )

    InvalidateRect( HWND_FRMMAIN, NULL, TRUE )
end sub


private function PsOptionButton_DemoMessage( byval m as PSOPTIONBUTTON_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false
    if m->uMsg = WM_LBUTTONDOWN then
        print "message  WM_LBUTTONDOWN on id=" & str( PsOptionButton_GetID( m->hOptionButton ) )
    end if
    return false        ' never suppress: the demo only observes
end function


private function PsOptionButton_DemoTooltip( byval hOptionButton as HWND ) as DWSTRING
    dim as DWSTRING wszOut = "Option id " & str( PsOptionButton_GetID( hOptionButton ) ) & _
                             " in group " & str( PsOptionButton_GetGroupID( hOptionButton ) )
    return wszOut
end function


' A host renderer replacing the built-in one: a square swatch instead of a circle, to make the
' substitution obvious at a glance.
'
' NOTE WHAT DRAWS THE FOCUS RING: PaintRoundOutline, never PaintBorderRect. PaintBorderRect FILLS
' unconditionally, so used as an outline it erases everything beneath it -- the mistake this
' family has made four separate times, every time by copying a sibling's callback.
' PsOptionButton_CountRenderedTones exists so a host can assert it has not done it, and
' frmMain_Show below actually runs that assertion against this very function.
private sub PsOptionButton_DemoPaint( byval p as PSOPTIONBUTTON_PAINTINFO ptr )
    if p = 0 then exit sub
    if p->b = 0 then exit sub

    dim as COLORREF clrSwatch = theme.ForeColorDisabled
    if p->isChecked then clrSwatch = theme.FocusAccent
    if p->isHot then clrSwatch = theme.ForeColorHot

    ' The swatch: a filled square in the circle's rect.
    dim as RECT rcS = p->rcCircle
    p->b->SetBackColor( clrSwatch )
    p->b->PaintRect( @rcS )

    ' PAINT WITH THE FONT THE CONTROL MEASURED WITH. frmMain_Show hands these two rows
    ' GUIFONTBOLD_10 via PsOptionButton_SetFont precisely so this line can use it: the ideal
    ' width was measured with whatever SetFont was given, so painting in a WIDER font makes the
    ' width a lie and the caption ellipsizes for no visible reason. This callback originally
    ' painted bold over a regular-font measurement and "Also custom painted" came out as
    ' "Also custom pain..." -- the exact trap the API docs warn about, demonstrated by the demo
    ' that is supposed to illustrate the rule.
    if IsRectEmpty( @p->rcText ) = 0 then
        dim as RECT rcT = p->rcText
        p->b->SetFont( PsOptionButton_GetFont( p->hOptionButton ) )
        p->b->SetColors( theme.ForeColor, theme.BackColor )
        p->b->PaintText( p->wszText, @rcT, DT_LEFT or DT_END_ELLIPSIS )
    end if

    if p->isFocused then
        dim as RECT rcRing = p->rcVisual
        p->b->SetPenColor( theme.FocusAccent )
        p->b->PaintRoundOutline( @rcRing, 4, 1 )
    end if
end sub


' ========================================================================================
' Standard form plumbing
' ========================================================================================
private function frmMain_OnCreate( byval hwnd as HWND, byval lpCreateStructPtr as LPCREATESTRUCT ) as BOOLEAN
    HWND_FRMMAIN = hwnd
    function = true
end function

' NOTE ALL THREE ARE FUNCTIONS, NOT SUBS. HANDLE_MSG expands to "return HANDLE_WM_xxx(...)",
' which expands in turn to "return fn(hwnd, ...)" -- so a sub here is a syntax error on the
' macro line, reported against the HANDLE_MSG rather than against the handler.
private function frmMain_OnSize( _
            byval hwnd  as HWND, _
            byval state as UINT, _
            byval cx    as long, _
            byval cy    as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        frmMain_PositionOptions( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
    function = 0
end function

private function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow( hwnd )
    function = 0
end function

private function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage( 0 )
    function = 0
end function


private function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG( hwnd, WM_CREATE,  frmMain_OnCreate )
        HANDLE_MSG( hwnd, WM_SIZE,    frmMain_OnSize )
        HANDLE_MSG( hwnd, WM_CLOSE,   frmMain_OnClose )
        HANDLE_MSG( hwnd, WM_DESTROY, frmMain_OnDestroy )

    case WM_PAINT
        return frmMain_OnPaint( hwnd )

    case WM_ERASEBKGND
        return true

    case WM_COMMAND
        ' The two plain Win32 buttons. They exist for the Tab walk, but answering them proves
        ' the walk actually LANDS on them.
        select case loword(wParam)
        case IDC_FRMMAIN_BTN_OK
            print "OK button clicked -- group answers:"
            print "   quality=" & str( PsOptionButton_FindCheckedID( hwnd, GRP_QUALITY ) ) & _
                  "  channel=" & str( PsOptionButton_FindCheckedID( hwnd, GRP_CHANNEL ) ) & _
                  "  units="   & str( PsOptionButton_FindCheckedID( hwnd, GRP_UNITS ) )
            print "   align="  & str( PsOptionButton_FindCheckedID( hwnd, GRP_ALIGN ) ) & _
                  "  paint="   & str( PsOptionButton_FindCheckedID( hwnd, GRP_PAINT ) ) & _
                  "  icon="    & str( PsOptionButton_FindCheckedID( hwnd, GRP_ICON ) )
            return 0
        case IDC_FRMMAIN_BTN_CANCEL
            print "Cancel button clicked"
            return 0
        end select

    case WM_SYSCOMMAND
        if (wParam and &hFFF0) = SC_CLOSE then
            SendMessageW( hwnd, WM_CLOSE, 0, 0 )
            return 0
        end if

    case WM_NCDESTROY
        dim as CWindow ptr pWindow = AfxCWindowPtr( hwnd )
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow( APPCLASSNAME )

    ghFont(GUIFONT_9)      = pWindow->CreateFont( GUIFONT,  9 )
    ghFont(GUIFONT_10)     = pWindow->CreateFont( GUIFONT, 10 )
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont( GUIFONT, 10, FW_BOLD )

    HWND_FRMMAIN = pWindow->Create( hWndParent, APPNAME, @frmMain_WndProc, _
                                    100, 60, 920, 800 )
    pWindow->SetClientSize( 900, 760 )
    pWindow->Center()

    ' Dark caption, so the demo does not read as half-light.
    dim as long nDark = 1
    DwmSetWindowAttribute( HWND_FRMMAIN, 20, @nDark, sizeof(long) )   ' DWMWA_USE_IMMERSIVE_DARK_MODE

    ' ---- the row table -------------------------------------------------------------------
    dim as DWSTRING captions( 0 to OPTION_COUNT - 1 )

    gRows(0).wszHeading = "Quality"
    gRows(0).wszNote    = "Three options. Arrows move AND check, wrapping at both ends."
    gRows(0).nGroup     = GRP_QUALITY   : captions(0) = "Highest"
    gRows(1).nGroup     = GRP_QUALITY   : captions(1) = "Balanced"
    gRows(2).nGroup     = GRP_QUALITY   : captions(2) = "Smallest file"
    gRows(2).wszNote    = "Balanced is checked at create -- so it holds the group's tab stop."

    gRows(3).wszHeading = "Update channel"
    gRows(3).wszNote    = "The middle option is DISABLED. Arrows step over it."
    gRows(3).nGroup     = GRP_CHANNEL   : captions(3) = "Stable"
    gRows(4).nGroup     = GRP_CHANNEL   : captions(4) = "Beta"
    gRows(5).nGroup     = GRP_CHANNEL   : captions(5) = "Nightly"
    gRows(5).wszNote    = "Nothing is checked here at start -- a legal state the user cannot reach."

    gRows(6).wszHeading = "Units"
    gRows(6).wszNote    = "BOXALIGN_RIGHT at a fixed row width, so the circles line up."
    gRows(6).nGroup     = GRP_UNITS     : captions(6) = "Metric"
    gRows(7).nGroup     = GRP_UNITS     : captions(7) = "Imperial"

    gRows(8).wszHeading = "Caption alignment"
    gRows(8).wszNote    = "TEXTALIGN within the SPAN, not the control. Given slack to show it."
    gRows(8).nGroup     = GRP_ALIGN     : captions(8)  = "Left"
    gRows(9).nGroup     = GRP_ALIGN     : captions(9)  = "Centre"
    gRows(10).nGroup    = GRP_ALIGN     : captions(10) = "Right"

    gRows(11).wszHeading = "Paint callback"
    gRows(11).wszNote    = "A host renderer: square swatch instead of a circle."
    gRows(11).nGroup     = GRP_PAINT    : captions(11) = "Custom painted"
    gRows(12).nGroup     = GRP_PAINT    : captions(12) = "Also custom painted"

    gRows(13).wszHeading = "No caption + tooltip"
    gRows(13).wszNote    = "A 28px circle, no caption. Hover for the tooltip callback."
    gRows(13).nGroup     = GRP_ICON     : captions(13) = ""
    gRows(14).nGroup     = GRP_ICON     : captions(14) = ""

    ' ---- create the option buttons --------------------------------------------------------
    for i as long = 0 to OPTION_COUNT - 1
        ghOpt(i) = PsOptionButton_Create( HWND_FRMMAIN, IDC_FRMMAIN_OPT_FIRST + i, gRows(i).nGroup )
        PsOptionButton_SetFont( ghOpt(i), ghFont(GUIFONT_10) )
        PsOptionButton_SetText( ghOpt(i), captions(i) )
        PsOptionButton_SetCheckChangedCallback( ghOpt(i), @PsOptionButton_DemoChanged )
    next

    ' ---- per-group customisation ----------------------------------------------------------

    ' Quality: pre-checked, and a hover row highlight opted into with ONE colour field.
    PsOptionButton_SetChecked( ghOpt(1), true )
    for i as long = 0 to 2
        dim as PSOPTIONBUTTON_COLORS c
        PsOptionButton_GetColors( ghOpt(i), @c )
        c.BackColorHot     = theme.BackColorHot
        c.BackColorPressed = theme.BackColorHot
        PsOptionButton_SetColors( ghOpt(i), @c )
    next

    ' Channel: the middle one is disabled, and nothing is checked.
    PsOptionButton_SetEnabled( ghOpt(4), false )
    PsOptionButton_SetText( ghOpt(4), "Beta  (disabled)" )

    ' Units: box on the right.
    PsOptionButton_SetBoxAlign( ghOpt(6), OPT_BOXALIGN_RIGHT )
    PsOptionButton_SetBoxAlign( ghOpt(7), OPT_BOXALIGN_RIGHT )
    PsOptionButton_SetChecked( ghOpt(6), true )

    ' Alignment.
    PsOptionButton_SetTextAlign( ghOpt(8),  OPT_TEXTALIGN_LEFT )
    PsOptionButton_SetTextAlign( ghOpt(9),  OPT_TEXTALIGN_CENTER )
    PsOptionButton_SetTextAlign( ghOpt(10), OPT_TEXTALIGN_RIGHT )
    PsOptionButton_SetChecked( ghOpt(9), true )

    ' Paint callback, plus a message callback on the first of the pair.
    ' The BOLD font goes through SetFont, not just into the callback -- so the control measures
    ' in the same font the callback paints in. See PsOptionButton_DemoPaint.
    PsOptionButton_SetFont( ghOpt(11), ghFont(GUIFONTBOLD_10) )
    PsOptionButton_SetFont( ghOpt(12), ghFont(GUIFONTBOLD_10) )
    PsOptionButton_SetPaintCallback( ghOpt(11), @PsOptionButton_DemoPaint )
    PsOptionButton_SetPaintCallback( ghOpt(12), @PsOptionButton_DemoPaint )
    PsOptionButton_SetMessageCallback( ghOpt(11), @PsOptionButton_DemoMessage )
    PsOptionButton_SetChecked( ghOpt(11), true )

    ' Icon-sized: a big circle, no caption, tooltips from the callback. nDotSize stays 0 so the
    ' dot scales with the box -- the reason 0 is the default.
    for i as long = 13 to 14
        PsOptionButton_SetBoxSize( ghOpt(i), 28, 28 )
        PsOptionButton_SetTooltipCallback( ghOpt(i), @PsOptionButton_DemoTooltip )
    next
    PsOptionButton_SetChecked( ghOpt(14), true )

    ' ---- two PLAIN WIN32 buttons, so the Tab walk has somewhere to go that is not ours ------
    ghBtnOK = CreateWindowExW( 0, "BUTTON", "Read groups", _
                               WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                               0, 0, 0, 0, HWND_FRMMAIN, _
                               cast(HMENU, cast(UINT_PTR, IDC_FRMMAIN_BTN_OK)), _
                               GetModuleHandleW(NULL), NULL )
    ghBtnCancel = CreateWindowExW( 0, "BUTTON", "Cancel", _
                                   WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                                   0, 0, 0, 0, HWND_FRMMAIN, _
                                   cast(HMENU, cast(UINT_PTR, IDC_FRMMAIN_BTN_CANCEL)), _
                                   GetModuleHandleW(NULL), NULL )
    SendMessageW( ghBtnOK,     WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), 1 )
    SendMessageW( ghBtnCancel, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), 1 )

    frmMain_PositionOptions( HWND_FRMMAIN )
    for i as long = 0 to OPTION_COUNT - 1
        ShowWindow( ghOpt(i), SW_SHOW )
    next

    ' ---- self-test -------------------------------------------------------------------------
    PsOptionButton_RunSelfTest( HWND_FRMMAIN )

    ' HOST-SIDE probe of the demo's OWN paint callback, gated on the same env var. This is the
    ' assertion PsOptionButton_CountRenderedTones was made public for: every occurrence of the
    ' flood bug in this family has been in a HOST callback, never in a control's own painter.
    if environ("PSOPTIONBUTTON_SELFTEST") = "1" then
        ' ---- the demo's own layout claim, asserted rather than eyeballed --------------
        ' frmMain_PositionOptions says every member of a group shares one left edge. That is
        ' exactly the sort of claim that gets checked by looking at the window and then quietly
        ' regresses, so it is measured here instead.
        print "=== demo group alignment ==="
        for g as long = 1 to GRP_ICON
            dim as long nSeen = 0
            dim as long nLeft = 0
            dim as boolean bAligned = true
            dim as string sLefts
            sLefts = ""
            for i as long = 0 to OPTION_COUNT - 1
                if gRows(i).nGroup <> g then continue for
                if ghOpt(i) = 0 then continue for
                dim as RECT rc
                GetWindowRect( ghOpt(i), @rc )
                if nSeen = 0 then
                    nLeft = rc.left
                elseif rc.left <> nLeft then
                    bAligned = false
                end if
                nSeen += 1
                sLefts &= str(rc.left) & " "
            next
            ' The precondition is asserted, not assumed: a group of one aligns trivially and
            ' would report PASS while proving nothing.
            dim as string sA
            if (nSeen >= 2) andalso bAligned then sA = "PASS  " else sA = "FAIL  "
            print sA & "group " & str(g) & ": every member shares one left edge   [members=" & _
                  str(nSeen) & " lefts= " & sLefts & "]"
        next

        print "=== demo paint callback ==="
        dim as long tCtl = PsOptionButton_CountRenderedTones( ghOpt(11), OPT_PART_CONTROL )
        dim as long tTxt = PsOptionButton_CountRenderedTones( ghOpt(11), OPT_PART_TEXT )
        dim as string sP
        if (tCtl > 1) andalso (tTxt > 1) then sP = "PASS  " else sP = "FAIL  "
        print sP & "the demo's paint callback does not flood the control   [control=" & _
              str(tCtl) & " text=" & str(tTxt) & "]"

        dim as ulong hOff, hOn
        PsOptionButton_SetChecked( ghOpt(12), true )
        hOn = PsOptionButton_HashRenderedPart( ghOpt(12), OPT_PART_BOX )
        PsOptionButton_SetChecked( ghOpt(11), true )
        hOff = PsOptionButton_HashRenderedPart( ghOpt(12), OPT_PART_BOX )
        if (hOn <> hOff) andalso (hOn <> 0) then sP = "PASS  " else sP = "FAIL  "
        print sP & "the demo's paint callback renders the two states differently   [&h" & _
              hex(hOn) & " vs &h" & hex(hOff) & "]"
    end if

    ' A self-test run closes the window so the process exits and the suite can run
    ' unattended. Without this the demo sits in its pump and the console output stays
    ' BUFFERED -- killing the run then loses every line, the summary included.
    if environ("PSOPTIONBUTTON_SELFTEST") = "1" then
        DestroyWindow( HWND_FRMMAIN )
        return 0
    end if

    ShowWindow( HWND_FRMMAIN, SW_SHOW )
    UpdateWindow( HWND_FRMMAIN )

    ' WITHOUT THIS THE FIRST TAB DOES NOTHING. IsDialogMessage only acts when the focused window
    ' is a DESCENDANT of the window passed to it, and at startup focus is on the form itself --
    ' a window is not its own child. A real dialog solves this in WM_INITDIALOG; an ordinary
    ' CWindow host has to do it here (C:\dev\Learnings.md).
    SetFocus( ghOpt(1) )

    dim as MSG uMsg
    do while GetMessage( @uMsg, null, 0, 0 )
        if uMsg.message = WM_QUIT then exit do
        if (pWindow->AccelHandle = 0) orelse _
           (TranslateAccelerator( pWindow->hWindow, pWindow->AccelHandle, @uMsg ) = 0) then
            ' IsDialogMessage is what makes Tab work at all. This control adds NOTHING else to
            ' the pump -- no FilterMessage, unlike PsComboBox / PsDatePicker / PsTextBox.
            if IsDialogMessage( pWindow->hWindow, @uMsg ) = 0 then
                TranslateMessage @uMsg
                DispatchMessage @uMsg
            end if
        end if
    loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS - 1
        if ghFont(i) then DeleteObject( ghFont(i) )
    next

end function
