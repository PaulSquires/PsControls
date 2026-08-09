'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"

' ----------------------------------------------------------------------------------------
' Harness model: a settings page longer than the window, hosted in a PsScrollPanel.
'
'     +-----------------------------------------------+
'     |  form background (a different colour, so the  |
'     |  control is visibly a CONTROL and not the     |
'     |  whole window)                                |
'     |   +---------------------------------------+   |
'     |   | Appearance                        | | |   |
'     |   | ------------------------------    |t| |   |
'     |   |   Theme                    [edit] |h| |   |
'     |   |   Show line numbers          [x]  |u| |   |
'     |   |   ...                             |m| |   |
'     |   +---------------------------------------+   |
'     +-----------------------------------------------+
'
' Everything on the page is a PLAIN Win32 control -- STATIC, BUTTON, EDIT -- parented to
' PsScrollPanel_GetPanel(). They are laid out ONCE per resize, in page coordinates, by the
' layout callback; scrolling never touches them again.
' ----------------------------------------------------------------------------------------
#define PAGE_INSET       20      ' gap between the form's edge and the control, unscaled
#define PAGE_MARGIN      16      ' gap between the page's edge and its content
#define ROW_INDENT       14      ' rows are indented under their section header
#define ROW_HEIGHT       24
#define ROW_GAP          10
#define HDR_HEIGHT       22
#define HDR_RULE_GAP      4      ' header text -> rule
#define HDR_AFTER        12      ' rule -> first row
#define SECTION_GAP      18      ' extra space before every header but the first
#define CTL_WIDTH       170      ' edit boxes
#define CHECK_SIZE       20      ' checkbox glyph cell
#define LABEL_GAP        12

#define ROWKIND_HEADER    0
#define ROWKIND_CHECK     1
#define ROWKIND_EDIT      2

type DEMOROW
    hLabel  as HWND
    hCtl    as HWND
    nKind   as integer
    nY      as integer      ' page-coordinate top, from the last layout pass
    nRuleY  as integer      ' headers only: where the rule under the text goes
end type

dim shared as DEMOROW gRows(any)
dim shared as integer gRowCount = 0

dim shared as HWND    ghScrollPanel
dim shared as HBRUSH  ghEditBrush        ' demo-owned; proves the lResult override works
dim shared as integer gFailCount = 0
dim shared as boolean gQuietScroll = false

dim shared as wstring * 16 gwszStatic = "STATIC"
dim shared as wstring * 16 gwszButton = "BUTTON"
dim shared as wstring * 16 gwszEdit   = "EDIT"


' ========================================================================================
' Build one row of the settings page. The controls are created at 0,0,0,0 -- the layout
' callback is the only thing that ever positions them.
' ========================================================================================
sub frmMain_AddRow( _
            byval hPanel  as HWND, _
            byref sLabel  as string, _
            byval nKind   as integer, _
            byref sValue  as string = "" _
            )

    dim as integer idx = gRowCount
    redim preserve gRows( 0 to idx )
    gRowCount = idx + 1

    dim as HINSTANCE hInst = GetModuleHandleW( NULL )
    dim as wstring * 128 wszText : wszText = sLabel

    dim as DWORD dwLabelStyle = WS_CHILD or WS_VISIBLE or SS_LEFT or SS_CENTERIMAGE
    gRows(idx).hLabel = CreateWindowExW( 0, @gwszStatic, @wszText, dwLabelStyle, _
        0, 0, 0, 0, hPanel, _
        cast(HMENU, cast(LONG_PTR, IDC_FRMMAIN_FIRSTROW + idx * 2)), hInst, NULL )

    gRows(idx).nKind = nKind

    select case nKind
        case ROWKIND_CHECK
            gRows(idx).hCtl = CreateWindowExW( 0, @gwszButton, @wstr(""), _
                WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_AUTOCHECKBOX, _
                0, 0, 0, 0, hPanel, _
                cast(HMENU, cast(LONG_PTR, IDC_FRMMAIN_FIRSTROW + idx * 2 + 1)), hInst, NULL )
            ' Give roughly every other one a tick, so the page is not uniform.
            if (idx and 1) = 0 then SendMessage( gRows(idx).hCtl, BM_SETCHECK, BST_CHECKED, 0 )

        case ROWKIND_EDIT
            dim as wstring * 64 wszValue : wszValue = sValue
            gRows(idx).hCtl = CreateWindowExW( 0, @gwszEdit, @wszValue, _
                WS_CHILD or WS_VISIBLE or WS_TABSTOP or WS_BORDER or ES_AUTOHSCROLL, _
                0, 0, 0, 0, hPanel, _
                cast(HMENU, cast(LONG_PTR, IDC_FRMMAIN_FIRSTROW + idx * 2 + 1)), hInst, NULL )
    end select
end sub


' ========================================================================================
' The whole settings page. Deliberately longer than any sane window.
' ========================================================================================
sub frmMain_BuildSettingsPage( byval hPanel as HWND )
    frmMain_AddRow( hPanel, "Appearance",                ROWKIND_HEADER )
    frmMain_AddRow( hPanel, "Colour theme",              ROWKIND_EDIT,  "Dark+" )
    frmMain_AddRow( hPanel, "Editor font",               ROWKIND_EDIT,  "Cascadia Mono" )
    frmMain_AddRow( hPanel, "Show line numbers",         ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Show indent guides",        ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Highlight current line",    ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Show whitespace",           ROWKIND_CHECK )

    frmMain_AddRow( hPanel, "Editor",                    ROWKIND_HEADER )
    frmMain_AddRow( hPanel, "Tab width",                 ROWKIND_EDIT,  "4" )
    frmMain_AddRow( hPanel, "Insert spaces for tabs",    ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Word wrap",                 ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Auto-indent",               ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Bracket matching",          ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Right margin column",       ROWKIND_EDIT,  "100" )

    frmMain_AddRow( hPanel, "Files",                     ROWKIND_HEADER )
    frmMain_AddRow( hPanel, "Default encoding",          ROWKIND_EDIT,  "UTF-8" )
    frmMain_AddRow( hPanel, "Line endings",              ROWKIND_EDIT,  "CRLF" )
    frmMain_AddRow( hPanel, "Trim trailing whitespace",  ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Save on focus loss",        ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Reopen last session",       ROWKIND_CHECK )

    frmMain_AddRow( hPanel, "Build",                     ROWKIND_HEADER )
    frmMain_AddRow( hPanel, "Compiler path",             ROWKIND_EDIT,  "fbc64.exe" )
    frmMain_AddRow( hPanel, "Additional flags",          ROWKIND_EDIT,  "-w all" )
    frmMain_AddRow( hPanel, "Save all before building",  ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Show compiler log",         ROWKIND_CHECK )

    frmMain_AddRow( hPanel, "Advanced",                  ROWKIND_HEADER )
    frmMain_AddRow( hPanel, "Parser threads",            ROWKIND_EDIT,  "4" )
    frmMain_AddRow( hPanel, "Scan debounce (ms)",        ROWKIND_EDIT,  "300" )
    frmMain_AddRow( hPanel, "Verbose diagnostics",       ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Experimental features",     ROWKIND_CHECK )
    frmMain_AddRow( hPanel, "Telemetry",                 ROWKIND_CHECK )
end sub


' ========================================================================================
' LAYOUT CALLBACK. The viewport resized; re-flow the page against the width we are HANDED
' (cxPanel) and return how tall the page now is.
'
' Note what this does NOT do: it never touches the scroll position, never resizes the page
' window, and never calls GetClientRect(hPanel). It lays out controls and returns a number.
' ========================================================================================
function ScrollPanel_Layout( byval hPanel as HWND, byval cxPanel as integer ) as integer
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then return 0

    dim as integer nMargin = pWindow->ScaleX(PAGE_MARGIN)
    dim as integer nIndent = pWindow->ScaleX(ROW_INDENT)
    dim as integer rowH    = pWindow->ScaleY(ROW_HEIGHT)
    dim as integer hdrH    = pWindow->ScaleY(HDR_HEIGHT)
    dim as integer ctlW    = pWindow->ScaleX(CTL_WIDTH)
    dim as integer chkW    = pWindow->ScaleX(CHECK_SIZE)
    dim as integer gapLbl  = pWindow->ScaleX(LABEL_GAP)

    dim as integer y = pWindow->ScaleY(PAGE_MARGIN)

    for i as integer = 0 to gRowCount - 1
        if gRows(i).nKind = ROWKIND_HEADER then
            if i > 0 then y += pWindow->ScaleY(SECTION_GAP)
            gRows(i).nY = y
            dim as integer w = cxPanel - nMargin * 2
            if w < 10 then w = 10
            SetWindowPos( gRows(i).hLabel, 0, nMargin, y, w, hdrH, SWP_NOZORDER )
            y += hdrH + pWindow->ScaleY(HDR_RULE_GAP)
            gRows(i).nRuleY = y
            y += pWindow->ScaleY(HDR_AFTER)
        else
            gRows(i).nY = y
            dim as integer w = iif( gRows(i).nKind = ROWKIND_CHECK, chkW, ctlW )
            dim as integer xCtl = cxPanel - nMargin - w
            dim as integer labX = nMargin + nIndent
            dim as integer labW = xCtl - gapLbl - labX
            if labW < 10 then labW = 10
            SetWindowPos( gRows(i).hLabel, 0, labX, y, labW, rowH, SWP_NOZORDER )
            if gRows(i).hCtl then
                SetWindowPos( gRows(i).hCtl, 0, xCtl, y, w, rowH, SWP_NOZORDER )
            end if
            y += rowH + pWindow->ScaleY(ROW_GAP)
        end if
    next

    return y + pWindow->ScaleY(PAGE_MARGIN)
end function


' ========================================================================================
' PAINT CALLBACK. p->rcClient is the WHOLE page, not the visible slice -- which is exactly
' what makes the section rules easy: they are drawn at the page coordinates the layout pass
' recorded, and Windows clips the repaint to whatever is on screen.
' ========================================================================================
sub ScrollPanel_Paint( byval p as SCP_PAINTINFO ptr )
    p->b->SetBackColor( theme.BackColorBox )
    p->b->PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub
    dim as integer nMargin = pWindow->ScaleX(PAGE_MARGIN)

    p->b->SetBackColor( theme.ForeColorRuler )
    for i as integer = 0 to gRowCount - 1
        if gRows(i).nKind = ROWKIND_HEADER then
            dim as RECT rc
            SetRect( @rc, nMargin, gRows(i).nRuleY, _
                     p->rcClient.right - nMargin, gRows(i).nRuleY + 1 )
            p->b->PaintRect( @rc )
        end if
    next
end sub


' ========================================================================================
' MESSAGE CALLBACK. One job here: give the EDIT boxes a lighter background than the page.
'
' This is the reason SCP_MESSAGEINFO carries lResult -- WM_CTLCOLOREDIT answers with an
' HBRUSH, so "handled" alone could not express it. Everything else falls through: the
' checkbox clicks are NOT claimed here, so the control forwards them to this form's own
' WM_COMMAND handler, which is the point of that forwarding.
' ========================================================================================
function ScrollPanel_Message( byval m as SCP_MESSAGEINFO ptr ) as boolean
    if m->uMsg <> WM_CTLCOLOREDIT then return false
    SetBkColor( cast(HDC, m->wParam), theme.BackColorEdit )
    SetTextColor( cast(HDC, m->wParam), theme.ForeColor )
    m->lResult = cast( LRESULT, ghEditBrush )
    return true
end function


' ========================================================================================
' SCROLL CALLBACK. Fires for USER scrolling only -- wheel, thumb, track paging, and the
' focus-into-view scroll a Tab causes. The programmatic setters are silent, which is what
' the "SetPos (silent)" keyboard exercise below demonstrates.
' ========================================================================================
sub ScrollPanel_Scroll( byval hScrollPanel as HWND, byval newPos as integer )
    if gQuietScroll then return
    dim as string s = "scroll -> pos=" & str(newPos) & _
                      "  (content=" & str(PsScrollPanel_GetContentHeight(hScrollPanel)) & _
                      "  viewport=" & str(PsScrollPanel_GetViewportHeight(hScrollPanel)) & ")"
    print s
end sub


' ========================================================================================
' The host's own layout: the control is inset inside the form, so it is visibly a control
' and the "cursor is off the control" half of the reveal rule is easy to exercise.
' ========================================================================================
sub frmMain_Layout( byval hwnd as HWND )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit sub
    if ghScrollPanel = 0 then exit sub

    dim as RECT rc : GetClientRect( hwnd, @rc )
    dim as integer inset = pWindow->ScaleX(PAGE_INSET)
    dim as integer w = (rc.right - rc.left) - inset * 2
    dim as integer h = (rc.bottom - rc.top) - inset * 2
    if w < 0 then w = 0
    if h < 0 then h = 0

    SetWindowPos( ghScrollPanel, 0, inset, inset, w, h, SWP_NOZORDER or SWP_SHOWWINDOW )
    InvalidateRect( hwnd, NULL, FALSE )
end sub


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
' Geometry / behaviour self-test (run with PSSCROLLPANEL_SELFTEST=1).
'
' Every assertion runs against a SECOND, throwaway PsScrollPanel created for the test and
' never shown: it starts empty, so its page geometry is exactly what the test puts there
' and the demo's own 30 rows cannot skew a measurement. A hidden window still has a real
' rect, so SetWindowPos / GetWindowRect / MapWindowPoints all measure truthfully.
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

' The page's rect in the container's client coordinates -- the space the scroll offset
' lives in, so the top edge must read -pos.
function SelfTest_PanelRect( byval hScp as HWND ) as RECT
    dim as RECT rc
    GetWindowRect( PsScrollPanel_GetPanel(hScp), @rc )
    MapWindowPoints( HWND_DESKTOP, hScp, cast( POINT ptr, @rc ), 2 )
    return rc
end function

function SelfTest_BarRect( byval hScp as HWND ) as RECT
    dim as RECT rc
    GetWindowRect( PsScrollPanel_GetScrollBar(hScp), @rc )
    MapWindowPoints( HWND_DESKTOP, hScp, cast( POINT ptr, @rc ), 2 )
    return rc
end function

function SelfTest_MakeProbe( byval hPanel as HWND, byval y as integer, byval h as integer ) as HWND
    return CreateWindowExW( 0, @gwszStatic, @wstr("probe"), WS_CHILD or WS_VISIBLE, _
        0, y, 60, h, hPanel, cast(HMENU, cast(LONG_PTR, 9000)), GetModuleHandleW(NULL), NULL )
end function

' A child's rect in PAGE coordinates -- the space the scroll position lives in.
function SelfTest_ChildPageRect( byval hPanel as HWND, byval hChild as HWND ) as RECT
    dim as RECT rc
    GetWindowRect( hChild, @rc )
    MapWindowPoints( HWND_DESKTOP, hPanel, cast( POINT ptr, @rc ), 2 )
    return rc
end function

sub frmMain_RunSelfTest()
    print ""
    print "=== PsScrollPanel self-test ==="

    gQuietScroll = true

    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    dim as HWND hT = PsScrollPanel_Create( HWND_FRMMAIN, IDC_FRMMAIN_TESTPANEL )
    PsScrollPanel_SetScrollCallback( hT, @ScrollPanel_Scroll )
    SetWindowPos( hT, 0, 0, 0, 400, 300, SWP_NOZORDER )     ' never shown

    dim as HWND hPanel = PsScrollPanel_GetPanel(hT)
    dim as HWND hSB    = PsScrollPanel_GetScrollBar(hT)
    dim as integer sbW = 400 - PsScrollPanel_GetPanelWidth(hT)

    ' ---- the strip is reserved, and the page gets exactly the rest ----------------------
    SelfTest_Check( "viewport height = client height", _
        PsScrollPanel_GetViewportHeight(hT) = 300, _
        "viewport=" & str(PsScrollPanel_GetViewportHeight(hT)) )

    dim as RECT rcBar = SelfTest_BarRect(hT)
    SelfTest_Check( "page width + bar width = client width", _
        PsScrollPanel_GetPanelWidth(hT) + (rcBar.right - rcBar.left) = 400, _
        "page=" & str(PsScrollPanel_GetPanelWidth(hT)) & " bar=" & str(rcBar.right - rcBar.left) )
    SelfTest_Check( "bar sits at the right edge, full height", _
        (rcBar.left = PsScrollPanel_GetPanelWidth(hT)) andalso (rcBar.right = 400) andalso _
        (rcBar.top = 0) andalso (rcBar.bottom = 300), _
        "bar=(" & str(rcBar.left) & "," & str(rcBar.top) & ")-(" & str(rcBar.right) & _
        "," & str(rcBar.bottom) & ")" )

    ' ---- a page shorter than the viewport is stretched to fill it -----------------------
    PsScrollPanel_SetContentHeight( hT, 120 )
    dim as RECT rcP = SelfTest_PanelRect(hT)
    SelfTest_Check( "short page is stretched to the viewport", _
        (rcP.bottom - rcP.top) = 300, "page h=" & str(rcP.bottom - rcP.top) )
    SelfTest_Check( "short page cannot scroll", PsScrollPanel_GetPos(hT) = 0, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) )
    SelfTest_Check( "short page hides the thumb", PsScrollPanel_IsThumbVisible(hT) = false, _
        "visible=" & str(PsScrollPanel_IsThumbVisible(hT)) )

    ' ---- a real page --------------------------------------------------------------------
    PsScrollPanel_SetContentHeight( hT, 1000 )
    rcP = SelfTest_PanelRect(hT)
    SelfTest_Check( "page height = content height", (rcP.bottom - rcP.top) = 1000, _
        "page h=" & str(rcP.bottom - rcP.top) )
    SelfTest_Check( "page width = client - strip", _
        (rcP.right - rcP.left) = (400 - sbW), _
        "page w=" & str(rcP.right - rcP.left) & " expected=" & str(400 - sbW) )
    SelfTest_Check( "page starts at the top", rcP.top = 0, "top=" & str(rcP.top) )

    ' ---- position: the page's top edge IS -pos ------------------------------------------
    PsScrollPanel_SetPos( hT, 200 )
    rcP = SelfTest_PanelRect(hT)
    SelfTest_Check( "SetPos -> GetPos round-trip", PsScrollPanel_GetPos(hT) = 200, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) )
    SelfTest_Check( "page top = -pos", rcP.top = -200, "top=" & str(rcP.top) )
    SelfTest_Check( "scrollbar mirrors the position", PsVScrollBar_GetPos(hSB) = 200, _
        "bar pos=" & str(PsVScrollBar_GetPos(hSB)) )

    PsScrollPanel_SetPos( hT, 99999 )
    SelfTest_Check( "SetPos clamps to content - viewport", PsScrollPanel_GetPos(hT) = 700, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) & " expected=700" )
    PsScrollPanel_SetPos( hT, -50 )
    SelfTest_Check( "SetPos clamps at zero", PsScrollPanel_GetPos(hT) = 0, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) )

    ' NOTE for anyone adding assertions below: fbc does NOT evaluate call arguments left to
    ' right, so a SelfTest_Check whose bOK expression scrolls and whose sDetail then reads
    ' the position prints the value from BEFORE the move. Every side effect is hoisted into
    ' a local first. (The first cut of this file did not, and the detail strings quietly
    ' disagreed with their own verdicts.)
    dim as boolean bMoved
    dim as integer nGot

    PsScrollPanel_SetPos( hT, 400 )
    bMoved = PsScrollPanel_ScrollBy( hT, -150 )
    nGot   = PsScrollPanel_GetPos(hT)
    SelfTest_Check( "ScrollBy moves relative", bMoved andalso (nGot = 250), _
        "pos=" & str(nGot) & " expected=250" )

    PsScrollPanel_ScrollBy( hT, -9999 )
    bMoved = PsScrollPanel_ScrollBy( hT, -10 )
    nGot   = PsScrollPanel_GetPos(hT)
    SelfTest_Check( "ScrollBy at the top reports no move", _
        (bMoved = false) andalso (nGot = 0), "moved=" & str(bMoved) & " pos=" & str(nGot) )

    ' ---- a shrinking page must not strand the position past the end ---------------------
    PsScrollPanel_SetPos( hT, 700 )
    PsScrollPanel_SetContentHeight( hT, 400 )
    SelfTest_Check( "shrinking the content re-clamps the position", _
        PsScrollPanel_GetPos(hT) = 100, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) & " expected=100 (400-300)" )
    rcP = SelfTest_PanelRect(hT)
    SelfTest_Check( "and the page followed it", rcP.top = -100, "top=" & str(rcP.top) )

    ' ---- the reveal rule, as a truth table ----------------------------------------------
    SelfTest_Check( "reveal: nothing to scroll -> hidden (0,0,0)", _
        PsScrollPanel_ShouldShow(false, false, false) = false, "" )
    SelfTest_Check( "reveal: nothing to scroll, mouse over -> hidden (0,1,0)", _
        PsScrollPanel_ShouldShow(false, true, false) = false, "" )
    SelfTest_Check( "reveal: nothing to scroll, focused -> hidden (0,0,1)", _
        PsScrollPanel_ShouldShow(false, false, true) = false, "" )
    SelfTest_Check( "reveal: nothing to scroll, both -> hidden (0,1,1)", _
        PsScrollPanel_ShouldShow(false, true, true) = false, "" )
    SelfTest_Check( "reveal: scrollable, idle -> hidden (1,0,0)", _
        PsScrollPanel_ShouldShow(true, false, false) = false, "" )
    SelfTest_Check( "reveal: mouse alone reveals (1,1,0)", _
        PsScrollPanel_ShouldShow(true, true, false) = true, "" )
    SelfTest_Check( "reveal: focus alone reveals (1,0,1)", _
        PsScrollPanel_ShouldShow(true, false, true) = true, "" )
    SelfTest_Check( "reveal: both reveal (1,1,1)", _
        PsScrollPanel_ShouldShow(true, true, true) = true, "" )

    ' ---- EnsureVisible: the minimum move ------------------------------------------------
    PsScrollPanel_SetContentHeight( hT, 1000 )
    dim as HWND hProbe = SelfTest_MakeProbe( hPanel, 800, 30 )

    PsScrollPanel_SetPos( hT, 0 )
    bMoved = PsScrollPanel_EnsureVisible( hT, hProbe )
    nGot   = PsScrollPanel_GetPos(hT)
    SelfTest_Check( "EnsureVisible scrolls down to reveal the bottom edge", _
        bMoved andalso (nGot = 530), "pos=" & str(nGot) & " expected=530 (830-300)" )

    ' Above the viewport. The probe is at 800, so the viewport has to start BELOW it, which
    ' this page (max pos 700) cannot do -- hence a second, higher probe.
    dim as HWND hHigh = SelfTest_MakeProbe( hPanel, 100, 30 )
    PsScrollPanel_SetPos( hT, 500 )
    bMoved = PsScrollPanel_EnsureVisible( hT, hHigh )
    nGot   = PsScrollPanel_GetPos(hT)
    SelfTest_Check( "EnsureVisible scrolls up to reveal the top edge", _
        bMoved andalso (nGot = 100), "pos=" & str(nGot) & " expected=100 (child top)" )
    DestroyWindow( hHigh )

    PsScrollPanel_SetPos( hT, 600 )
    bMoved = PsScrollPanel_EnsureVisible( hT, hProbe )
    nGot   = PsScrollPanel_GetPos(hT)
    SelfTest_Check( "EnsureVisible on an already-visible child does nothing", _
        (bMoved = false) andalso (nGot = 600), _
        "pos=" & str(nGot) & " (child 800..830 sits inside 600..900)" )

    PsScrollPanel_SetPos( hT, 0 )
    bMoved = PsScrollPanel_EnsureVisible( hT, hProbe, 40 )
    nGot   = PsScrollPanel_GetPos(hT)
    SelfTest_Check( "EnsureVisible applies the margin", _
        bMoved andalso (nGot = 570), "pos=" & str(nGot) & " expected=570 (830+40-300)" )

    DestroyWindow( hProbe )
    dim as HWND hTall = SelfTest_MakeProbe( hPanel, 100, 400 )
    PsScrollPanel_SetPos( hT, 0 )
    bMoved = PsScrollPanel_EnsureVisible( hT, hTall )
    nGot   = PsScrollPanel_GetPos(hT)
    SelfTest_Check( "EnsureVisible top-aligns a child taller than the viewport", _
        bMoved andalso (nGot = 100), _
        "pos=" & str(nGot) & " expected=100 (child 100..500, viewport 300)" )
    DestroyWindow( hTall )

    bMoved = PsScrollPanel_EnsureVisible( hT, HWND_FRMMAIN )
    SelfTest_Check( "EnsureVisible rejects a window that is not ours", bMoved = false, _
        "moved=" & str(bMoved) )

    ' ---- AutoSizeContent ----------------------------------------------------------------
    dim as HWND hA = SelfTest_MakeProbe( hPanel, 100, 40 )     ' bottom 140
    dim as HWND hB = SelfTest_MakeProbe( hPanel, 460, 40 )     ' bottom 500  <- the deepest
    dim as HWND hC = SelfTest_MakeProbe( hPanel, 300, 40 )     ' bottom 340
    dim as HWND hHidden = SelfTest_MakeProbe( hPanel, 900, 40 )
    ShowWindow( hHidden, SW_HIDE )

    ' NB the control here has never been shown, which is the normal state while a host is
    ' still building its page. IsWindowVisible would report every one of these children as
    ' invisible and measure the page as zero; the child's own WS_VISIBLE bit is the right
    ' question, and this is the assertion that says so.
    dim as integer nAuto = PsScrollPanel_AutoSizeContent( hT, 20 )
    nGot = PsScrollPanel_GetContentHeight(hT)
    SelfTest_Check( "AutoSizeContent = deepest visible child + padding", nAuto = 520, _
        "auto=" & str(nAuto) & " expected=520 (500+20)" )
    SelfTest_Check( "AutoSizeContent ignores hidden children", nGot = 520, _
        "content=" & str(nGot) & " (a hidden child sits at 900..940)" )

    ShowWindow( hHidden, SW_SHOW )
    nAuto = PsScrollPanel_AutoSizeContent( hT, 0 )
    SelfTest_Check( "AutoSizeContent sees it once shown", nAuto = 940, _
        "auto=" & str(nAuto) & " expected=940" )

    DestroyWindow( hA ) : DestroyWindow( hB ) : DestroyWindow( hC ) : DestroyWindow( hHidden )

    ' ---- the wheel ----------------------------------------------------------------------
    ' Range units are pixels, so one notch must be system-lines x line-height. The expected
    ' value is derived with the same conversion the control uses rather than hardcoded --
    ' the system setting and the DPI are both outside our control.
    PsScrollPanel_SetContentHeight( hT, 1000 )
    PsScrollPanel_SetLineHeight( hT, 20 )
    dim as integer nLine = 20
    if pWindow then nLine = pWindow->ScaleY(20)

    dim as ulong sysval = 3
    SystemParametersInfo( SPI_GETWHEELSCROLLLINES, 0, @sysval, 0 )
    dim as integer nStep
    if sysval = cast(ulong, &hFFFFFFFF) then
        nStep = 300
    else
        nStep = cast(integer, sysval) * nLine
    end if

    PsScrollPanel_SetPos( hT, 0 )
    PsVScrollBar_SetWheelStep( hSB, 1 )      ' a different value: clears the accumulator
    PsScrollPanel_HandleWheelDelta( hT, -120 )
    dim as integer nExpect = nStep
    if nExpect > 700 then nExpect = 700
    SelfTest_Check( "one wheel notch = system lines x line height", _
        PsScrollPanel_GetPos(hT) = nExpect, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) & " expected=" & str(nExpect) & _
        " (lines=" & str(sysval) & " lineH=" & str(nLine) & ")" )

    PsScrollPanel_SetPos( hT, 0 )
    PsVScrollBar_SetWheelStep( hSB, 1 )
    PsScrollPanel_HandleWheelDelta( hT, -120 )
    PsScrollPanel_HandleWheelDelta( hT, 120 )
    SelfTest_Check( "the opposite notch comes back to where it started", _
        PsScrollPanel_GetPos(hT) = 0, "pos=" & str(PsScrollPanel_GetPos(hT)) )

    ' Sub-notch accumulation: a precision trackpad sends deltas well under 120, and they
    ' must add up rather than being discarded one at a time.
    PsScrollPanel_SetPos( hT, 0 )
    PsVScrollBar_SetWheelStep( hSB, 1 )
    PsScrollPanel_HandleWheelDelta( hT, -1 )
    dim as integer nAfterOne = PsScrollPanel_GetPos(hT)
    PsScrollPanel_HandleWheelDelta( hT, -59 )
    dim as integer nExpect60 = (60 * nStep) \ 120
    if nExpect60 > 700 then nExpect60 = 700
    SelfTest_Check( "sub-notch deltas accumulate", PsScrollPanel_GetPos(hT) = nExpect60, _
        "after -1=" & str(nAfterOne) & " after -60 total=" & str(PsScrollPanel_GetPos(hT)) & _
        " expected=" & str(nExpect60) )

    PsScrollPanel_SetPos( hT, 700 )
    PsVScrollBar_SetWheelStep( hSB, 1 )
    PsScrollPanel_HandleWheelDelta( hT, -1200 )
    SelfTest_Check( "the wheel clamps at the end of the page", PsScrollPanel_GetPos(hT) = 700, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) )
    PsScrollPanel_HandleWheelDelta( hT, 12000 )
    SelfTest_Check( "the wheel clamps at the top", PsScrollPanel_GetPos(hT) = 0, _
        "pos=" & str(PsScrollPanel_GetPos(hT)) )

    ' ---- the strip is re-derived when its width changes ---------------------------------
    PsScrollPanel_SetScrollBarWidth( hT, 24 )
    dim as integer sbW2 = 400 - PsScrollPanel_GetPanelWidth(hT)
    rcBar = SelfTest_BarRect(hT)
    SelfTest_Check( "SetScrollBarWidth re-derives the page width", _
        (rcBar.right - rcBar.left) = sbW2, _
        "strip=" & str(sbW2) & " bar=" & str(rcBar.right - rcBar.left) )
    SelfTest_Check( "and the wider strip took the space from the page", sbW2 > sbW, _
        "before=" & str(sbW) & " after=" & str(sbW2) )

    DestroyWindow( hT )

    ' ---- reveal + focus-into-view, on the REAL control ----------------------------------
    ' These two need a control that is actually on screen and a page that is actually
    ' populated, so they run against the demo instance rather than the throwaway one. The
    ' reveal tick is driven directly instead of waiting for the 100 ms timer: it is the same
    ' handler, and a pumped wait would make the result depend on scheduling.
    '
    ' The cursor is parked off the window first -- otherwise "mouse away" is whatever the
    ' cursor happened to be doing -- and restored at the end.
    dim as POINT ptSave : GetCursorPos( @ptSave )
    SetCursorPos( 0, 0 )

    dim as HWND hDemoPanel = PsScrollPanel_GetPanel( ghScrollPanel )
    dim as HWND hLastCtl   = 0
    for i as integer = gRowCount - 1 to 0 step -1
        if gRows(i).hCtl then hLastCtl = gRows(i).hCtl : exit for
    next

    PsScrollPanel_SetPos( ghScrollPanel, 0 )
    SetFocus( HWND_FRMMAIN )
    SendMessage( ghScrollPanel, WM_TIMER, IDT_CSCROLLPANEL_REVEAL, 0 )
    SelfTest_Check( "reveal: cursor away and nothing focused -> thumb hidden", _
        PsScrollPanel_IsThumbVisible( ghScrollPanel ) = false, _
        "visible=" & str(PsScrollPanel_IsThumbVisible( ghScrollPanel )) )

    SetFocus( hLastCtl )
    SendMessage( ghScrollPanel, WM_TIMER, IDT_CSCROLLPANEL_REVEAL, 0 )
    SelfTest_Check( "reveal: focus alone reveals the thumb, cursor still away", _
        PsScrollPanel_IsThumbVisible( ghScrollPanel ) = true, _
        "visible=" & str(PsScrollPanel_IsThumbVisible( ghScrollPanel )) )

    dim as RECT rcCtl = SelfTest_ChildPageRect( hDemoPanel, hLastCtl )
    dim as integer nPos  = PsScrollPanel_GetPos( ghScrollPanel )
    dim as integer nView = PsScrollPanel_GetViewportHeight( ghScrollPanel )
    SelfTest_Check( "focus change scrolled the control into view", _
        (nPos > 0) andalso (rcCtl.top >= nPos) andalso (rcCtl.bottom <= nPos + nView), _
        "child=" & str(rcCtl.top) & ".." & str(rcCtl.bottom) & _
        "  viewport=" & str(nPos) & ".." & str(nPos + nView) )
    SelfTest_Check( "and it moved the MINIMUM distance (bottom-aligned)", _
        nPos = (rcCtl.bottom - nView), _
        "pos=" & str(nPos) & " expected=" & str(rcCtl.bottom - nView) )

    SetFocus( HWND_FRMMAIN )
    SendMessage( ghScrollPanel, WM_TIMER, IDT_CSCROLLPANEL_REVEAL, 0 )
    SelfTest_Check( "reveal: focus leaving the page hides the thumb again", _
        PsScrollPanel_IsThumbVisible( ghScrollPanel ) = false, _
        "visible=" & str(PsScrollPanel_IsThumbVisible( ghScrollPanel )) )

    ' Focus is NOT restored by a scroll: a user who scrolls away from the focused control
    ' must not have it yanked back under them on the next tick.
    dim as integer nBefore = PsScrollPanel_GetPos( ghScrollPanel )
    SetFocus( hLastCtl )
    SendMessage( ghScrollPanel, WM_TIMER, IDT_CSCROLLPANEL_REVEAL, 0 )
    PsScrollPanel_SetPos( ghScrollPanel, 0 )
    SendMessage( ghScrollPanel, WM_TIMER, IDT_CSCROLLPANEL_REVEAL, 0 )
    SelfTest_Check( "an unchanged focus does not re-scroll", _
        PsScrollPanel_GetPos( ghScrollPanel ) = 0, _
        "pos=" & str(PsScrollPanel_GetPos( ghScrollPanel )) & " (was " & str(nBefore) & ")" )

    SetFocus( HWND_FRMMAIN )
    SetCursorPos( ptSave.x, ptSave.y )

    gQuietScroll = false
    print "=== self-test complete: " & str(gFailCount) & " failure(s) ==="
end sub


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean

    '  Message cracker macro expects a true to be returned for a successful
    '  OnCreate handler even though returning -1 from a standard WM_CREATE
    '  call would stop creating the window. This is just one of those Windows
    '  inconsistencies.
    return true
end function


function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow(hwnd)
    function = 0
end function


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
        HANDLE_MSG (hwnd, WM_PAINT,         frmMain_OnPaint)
        HANDLE_MSG (hwnd, WM_CLOSE,         frmMain_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,       frmMain_OnDestroy)

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMMAIN = 0
            return 0

        case WM_ERASEBKGND
            return true      ' prevents painting the background

        case WM_SIZE
            frmMain_Layout( hwnd )
            return 0

        case WM_COMMAND
            ' These come from controls on the PAGE, whose parent is the PsScrollPanel's
            ' panel window -- not this form. They arrive here because the control offers
            ' them to its message callback and, unclaimed, forwards them to its own
            ' parent. That is what lets an existing page move onto a PsScrollPanel without
            ' its WM_COMMAND handling being rewritten.
            if hiword(wParam) = BN_CLICKED then
                dim as HWND hCtl = cast(HWND, lParam)
                dim as integer nState = SendMessage( hCtl, BM_GETCHECK, 0, 0 )
                dim as string s = "WM_COMMAND forwarded from the page: id=" & _
                    str(loword(wParam)) & "  checked=" & str(nState)
                print s
            end if
            exit select

        case WM_KEYDOWN
            ' Programmatic scrolling. SetPos is SILENT by design, so no "scroll ->" trace
            ' may appear for these -- that asymmetry with the wheel is the point.
            select case wParam
                case VK_HOME
                    PsScrollPanel_SetPos( ghScrollPanel, 0 )
                    print "SetPos(0) (silent: no scroll callback fires)"
                case VK_END
                    PsScrollPanel_SetPos( ghScrollPanel, PsScrollPanel_GetContentHeight(ghScrollPanel) )
                    print "SetPos(end) (silent) -> pos=" & str(PsScrollPanel_GetPos(ghScrollPanel))
            end select

        case WM_SYSCOMMAND
            if (wParam and &HFFF0) = SC_CLOSE then
                SendMessage( hwnd, WM_CLOSE, wParam, lParam )
                exit function
            end if

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)  = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10) = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 720, 620 _
            )

    ghEditBrush = CreateSolidBrush( theme.BackColorEdit )

    ' ---- the control -------------------------------------------------------------------
    ghScrollPanel = PsScrollPanel_Create( HWND_FRMMAIN, IDC_FRMMAIN_SCROLLPANEL )
    PsScrollPanel_SetColors( ghScrollPanel, theme.BackColorBox, theme.ForeColor )
    PsScrollPanel_SetScrollBarColors( ghScrollPanel, theme.BackColorBox, _
                                     theme.ForeColorScrollBar, theme.ForeColorScrollBarHot )
    PsScrollPanel_SetPaintCallback( ghScrollPanel, @ScrollPanel_Paint )
    PsScrollPanel_SetMessageCallback( ghScrollPanel, @ScrollPanel_Message )
    PsScrollPanel_SetLayoutCallback( ghScrollPanel, @ScrollPanel_Layout )
    PsScrollPanel_SetScrollCallback( ghScrollPanel, @ScrollPanel_Scroll )
    PsScrollPanel_SetLineHeight( ghScrollPanel, ROW_HEIGHT + ROW_GAP )   ' one notch ~ 3 rows

    ' ---- the page ----------------------------------------------------------------------
    ' The host's controls are parented to the PANEL, never to the control itself.
    frmMain_BuildSettingsPage( PsScrollPanel_GetPanel( ghScrollPanel ) )
    PsScrollPanel_SetFont( ghScrollPanel, ghFont(GUIFONT_9) )

    frmMain_Layout( HWND_FRMMAIN )      ' sizes the control -> the layout callback runs

    print "PsScrollPanel test harness"
    print "-------------------------"
    print "A settings page of plain STATIC/BUTTON/EDIT controls, longer than the window."
    print ""
    print "Manual checks (things a script cannot drive):"
    print "  Reveal       move the cursor onto the panel -> the thumb fades in; move it off"
    print "               the control (onto the form's border) -> it goes away again"
    print "  Focus        click an edit box, then move the mouse right off the window: the"
    print "               thumb must STAY, because focus is still on the page"
    print "  Tab          Tab through the page -- focus must scroll off-screen controls into"
    print "               view, moving the minimum distance, and the thumb must be visible"
    print "               the whole time even with the mouse parked outside"
    print "  Wheel        wheel over the page background AND over an edit box (the second"
    print "               only works because unhandled wheel bubbles to the page); a slow"
    print "               trackpad swipe must scroll smoothly, not in jumps"
    print "  Drag         drag the thumb -- the page must track it, and the thumb must not"
    print "               vanish when the cursor slides off the strip mid-drag"
    print "  Resize       narrow the window: rows re-flow (edit boxes stay right-aligned),"
    print "               the page gets taller, the thumb gets shorter. Widen it until"
    print "               everything fits -> the thumb must disappear entirely"
    print "  Seam         with the thumb hidden, the strip must be indistinguishable from"
    print "               the page -- no visible gutter down the right edge"
    print "  Home/End     scroll with the keyboard: SILENT, no 'scroll ->' line may appear"
    print "  Checkboxes   clicking one prints a forwarded WM_COMMAND line"
    print ""

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    if environ("PSSCROLLPANEL_SELFTEST") = "1" then
        frmMain_RunSelfTest()
        DestroyWindow( HWND_FRMMAIN )
    end if

    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
            ' IsDialogMessage is what gives the page Tab navigation -- and it only recurses
            ' into the control because PsScrollPanel's two windows carry WS_EX_CONTROLPARENT.
            if IsDialogMessage(pWindow->hWindow, @uMsg) = 0 then
                TranslateMessage @uMsg    ' Translates virtual-key messages into character messages.
                DispatchMessage @uMsg     ' Dispatches a message to a window procedure.
            end if
        end if  ' accelerators
    loop  ' message loop

    function = uMsg.wParam

    if ghEditBrush then DeleteObject( ghEditBrush )
    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

end function
