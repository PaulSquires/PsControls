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




' One-shot numeric trace of the first multi-column row's cell rects (phase 4 gate:
' compare against the "Header rects:" line -- they must be identical spans).
dim shared gCellTraceDone as boolean = false

' ========================================================================================
' PsListTree_PaintCallback
' ========================================================================================
sub PsListTree_PaintCallback( byval p as PSLISTTREE_PAINTINFO ptr )

    ' State priority for the row background/foreground: selected > hot > normal
    dim as COLORREF backclr, foreclr
    if p->isSelected then
        backclr = theme.BackColorSelect
        foreclr = theme.ForeColorSelect
    elseif p->isHot then
        backclr = theme.BackColorHot
        foreclr = theme.ForeColorHot
    else
        backclr = theme.BackColor
        foreclr = theme.ForeColor
    end if
    ' Non-selectable item rows are greyed so the user can see they cannot be picked. This is
    ' cosmetic only -- the control blocks selection/focus regardless of how the host paints.
    if (p->isSelectable = false) andalso (p->isHeader = false) then foreclr = &h888888
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )

    ' Paint the entire row background
    p->b->PaintRect( @p->rc )

    if p->isHeader then
        ' Header row: bold, with a +/- collapse indicator. Group headers always span
        ' the full row width -- the control never hands them a cell array.
        dim as DWSTRING wszPrefix
        if p->isCollapsed then wszPrefix = "+ " else wszPrefix = "- "
        p->b->SetFont( ghFont(GUIFONTBOLD_10) )
        p->b->PaintText( wszPrefix & p->wszCaption, @p->rc, DT_LEFT )
    elseif p->columnCount > 0 then
        ' Multi-column row: the background/selection above already spans the full row
        ' (listview-style); each cell now draws inside its own rect, ellipsized, inset
        ' by the header's padding so cells line up with their captions.
        if gCellTraceDone = false then
            gCellTraceDone = true
            dim as string s = "Row cells:   "
            for c as integer = 0 to p->columnCount - 1
                s &= " c" & str(c) & "=[" & str(p->cells[c].rc.left) & ".." & str(p->cells[c].rc.right) & "]"
            next
            print s
        end if
        dim as integer pad = PsColumnHeader_GetPadding( HWND_COLHDR )
        p->b->SetFont( ghFont(GUIFONT_10) )
        for c as integer = 0 to p->columnCount - 1
            dim as RECT rcCell = p->cells[c].rc
            rcCell.left += pad
            rcCell.right -= pad
            p->b->PaintText( p->cells[c].wszText, @rcCell, DT_LEFT or DT_END_ELLIPSIS )
        next
    else
        ' Single-column item row. When tree indent is on (instance C) the control supplies the
        ' real per-depth indent in p->indent (which already includes the twisty band); with it
        ' off (instances A/B) it is 0, so fall back to the legacy fixed 16 -- A/B are unchanged.
        dim as RECT rcText = p->rc
        rcText.left += iif( p->indent > 0, p->indent, 16 )
        p->b->SetFont( ghFont(GUIFONT_10) )
        p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )
    end if

    ' Focus (caret) row: a thin accent bar on the left edge
    if p->isFocused then
        dim as RECT rcBar = p->rc
        rcBar.right = rcBar.left + 3
        p->b->SetBackColor( theme.FocusAccent )
        p->b->PaintRect( @rcBar )
    end if

end sub

' ========================================================================================
' Tree edit / expand-collapse callbacks (demo). newText is byval DWSTRING (never byref const)
' and is converted to a plain string BEFORE printing -- printing a DWSTRING directly
' interleaves nulls (see CLAUDE.md / Learnings.md).
' ========================================================================================
function Tree_BeginLabelEdit( byval h as HWND, byval row as integer ) as boolean
    return true                          ' allow every edit in the demo
end function

function Tree_EndLabelEdit( byval h as HWND, byval row as integer, byval newText as DWSTRING ) as boolean
    dim as string s = newText            ' single conversion; do not print the DWSTRING directly
    if len(trim(s)) = 0 then
        print "  label edit REJECTED (blank), row " & str(row) & " keeps its text"
        return false                     ' veto: the row keeps its old caption
    end if
    print "  row " & str(row) & " renamed to: " & s
    return true
end function

sub Tree_ExpandCollapse( byval h as HWND, byval row as integer, byval bExpanded as boolean )
    dim as string s = "  row " & str(row)
    if bExpanded then s &= " EXPANDED" else s &= " COLLAPSED"
    print s
end sub


' Host-side sort state for instance B (the CONTROL never sorts; the header's click
' callback is just the hook). -1 = unsorted.
dim shared gSortCol as integer = -1
dim shared gSortAsc as boolean = true

' Counts every ColumnResize notification -- lets the self-test assert that programmatic
' SetColumnWidth stays SILENT (family rule) while user drags notify.
dim shared gResizeNotifyCount as integer = 0


' ========================================================================================
' Compare two snapshot rows by the current sort column: numeric for Size (col 1),
' case-insensitive text otherwise. Returns <0 / 0 / >0.
' ========================================================================================
function Demo_CompareRows( byval a0 as string, byval a1 as string, byval a2 as string, _
                           byval b0 as string, byval b1 as string, byval b2 as string ) as integer
    if gSortCol = 1 then
        dim as integer na = val(a1)
        dim as integer nb = val(b1)
        if na < nb then return -1
        if na > nb then return 1
        return 0
    end if
    dim as string ka, kb
    if gSortCol = 2 then
        ka = lcase(a2) : kb = lcase(b2)
    else
        ka = lcase(a0) : kb = lcase(b0)
    end if
    if ka < kb then return -1
    if ka > kb then return 1
    return 0
end function


' ========================================================================================
' Host-side sort of instance B by gSortCol/gSortAsc. The demo's backing data lives in
' the control's cells, so snapshot it, sort each group's item run (headers stay put),
' and repopulate under one BeginUpdate batch.
' ========================================================================================
sub Demo_SortInstanceB()
    dim as HWND h = HWND_FRMEXPLORER2
    dim as integer n = PsListTree_GetCount( h )
    if n <= 0 then return

    redim as boolean isHdr( 0 to n - 1 )
    redim as boolean isCol( 0 to n - 1 )
    redim as string  c0( 0 to n - 1 ), c1( 0 to n - 1 ), c2( 0 to n - 1 )
    redim as integer dat( 0 to n - 1 )
    for i as integer = 0 to n - 1
        isHdr(i) = PsListTree_IsHeader( h, i )
        isCol(i) = PsListTree_IsCollapsed( h, i )
        c0(i) = PsListTree_GetCellText( h, i, 0 )
        c1(i) = PsListTree_GetCellText( h, i, 1 )
        c2(i) = PsListTree_GetCellText( h, i, 2 )
        dat(i) = PsListTree_GetItemData( h, i )
    next

    ' Insertion-sort each header's item run in place.
    dim as integer i = 0
    while i < n
        if isHdr(i) = false then
            i += 1
            continue while
        end if
        dim as integer first = i + 1
        dim as integer last = first
        while last < n
            if isHdr(last) then exit while
            last += 1
        wend
        for a as integer = first + 1 to last - 1
            dim as string k0 = c0(a), k1 = c1(a), k2 = c2(a)
            dim as integer kd = dat(a)
            dim as integer b2 = a - 1
            while b2 >= first
                dim as integer cmp = Demo_CompareRows( c0(b2), c1(b2), c2(b2), k0, k1, k2 )
                dim as boolean moveIt
                if gSortAsc then moveIt = (cmp > 0) else moveIt = (cmp < 0)
                if moveIt = false then exit while
                c0(b2 + 1) = c0(b2) : c1(b2 + 1) = c1(b2) : c2(b2 + 1) = c2(b2)
                dat(b2 + 1) = dat(b2)
                b2 -= 1
            wend
            c0(b2 + 1) = k0 : c1(b2 + 1) = k1 : c2(b2 + 1) = k2
            dat(b2 + 1) = kd
        next
        i = last
    wend

    PsListTree_BeginUpdate( h )
    PsListTree_Clear( h )
    for i as integer = 0 to n - 1
        dim as integer r
        if isHdr(i) then
            r = PsListTree_AddHeader( h, c0(i), dat(i) )
            if isCol(i) then PsListTree_CollapseRow( h, r )
        else
            r = PsListTree_AddString( h, c0(i), dat(i) )
            if len(c1(i)) > 0 then PsListTree_SetCellText( h, r, 1, c1(i) )
            if len(c2(i)) > 0 then PsListTree_SetCellText( h, r, 2, c2(i) )
        end if
    next
    PsListTree_EndUpdate( h )
end sub


' ========================================================================================
' Header_PaintCallback -- demo painter for the PsColumnHeader band (per-column)
' ========================================================================================
sub Header_PaintCallback( byval p as PSCOLUMNHEADER_PAINTINFO ptr )

    ' State priority for the cell background/foreground: pressed > hot > normal
    dim as COLORREF backclr, foreclr
    if p->isPressed then
        backclr = theme.BackColorSelect
        foreclr = theme.ForeColorSelect
    elseif p->isHot then
        backclr = theme.BackColorHot
        foreclr = theme.ForeColorHot
    else
        backclr = theme.BackColor
        foreclr = theme.ForeColor
    end if
    p->b->SetColors( foreclr, backclr )
    p->b->PaintRect( @p->rc )

    ' Caption, inset by the control's padding (the space the layout accounts for)
    dim as RECT rcText = p->rc
    dim as integer pad = PsColumnHeader_GetPadding( p->hHeader )
    rcText.left += pad
    rcText.right -= pad
    p->b->SetFont( ghFont(GUIFONTBOLD_10) )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' Sort glyph on the current sort column (host state -- the control knows nothing
    ' about sorting; a click just re-sorted the host's data).
    if p->itemID = gSortCol then
        dim as RECT rcGlyph = p->rc
        rcGlyph.left = rcGlyph.right - pad - 12
        dim as DWSTRING wszGlyph
        if gSortAsc then wszGlyph = wchr(&hE70E) else wszGlyph = wchr(&hE70D)
        p->b->SetFont( ghFont(SYMBOLFONT_9) )
        p->b->PaintChar( wszGlyph, @rcGlyph, foreclr )
    end if

    ' Divider tick at the right edge, accented (and widened) while its gutter is
    ' hovered or the divider is being dragged. The fill column has no divider.
    if p->isFill = false then
        dim as RECT rcDiv = p->rc
        rcDiv.left = rcDiv.right - 1
        dim as COLORREF divclr = theme.ForeColorScrollBar
        if p->isResizeHot orelse p->isResizing then
            divclr = theme.FocusAccent
            rcDiv.left -= 1
        end if
        p->b->SetBackColor( divclr )
        p->b->PaintRect( @rcDiv )
    end if

end sub


' ========================================================================================
' Header interaction callbacks (demo) -- numeric traces so the gestures are observable
' ========================================================================================
sub Header_WidthChangedCallback( byval hHeader as HWND, byval idx as integer, byval nWidth as integer, byval bLive as boolean )
    gResizeNotifyCount += 1
    ' plain string, not DWSTRING: print interleaves wide chars
    dim as string s = "header resize col=" & str(idx) & " w=" & str(nWidth) & " live=" & str(bLive)
    print s
end sub

sub Header_ClickCallback( byval hHeader as HWND, byval idx as integer )
    ' Click-to-sort: toggle direction on the same column, ascending on a new one.
    if idx = gSortCol then
        gSortAsc = not gSortAsc
    else
        gSortCol = idx
        gSortAsc = true
    end if
    dim as string s = "header click col=" & str(idx) & " -> sort "
    if gSortAsc then s &= "asc" else s &= "desc"
    print s
    Demo_SortInstanceB()
    PsColumnHeader_Refresh( hHeader )   ' repaint the sort glyph
end sub

function Header_AutoSizeCallback( byval hHeader as HWND, byval idx as integer ) as integer
    ' Demo best-fit: measure the column CAPTION with the header font + 2*padding. (A
    ' real host measures its widest cell; the standalone header has no cell data.)
    dim as integer bestW = 0
    dim as DWSTRING wszText = PsColumnHeader_GetColumnText( hHeader, idx )
    dim as HDC hDC = GetDC( hHeader )
    dim as HFONT hOldFont = SelectObject( hDC, ghFont(GUIFONTBOLD_10) )
    dim as SIZE sz
    if GetTextExtentPoint32W( hDC, wszText.vptr, len(wszText), @sz ) then bestW = sz.cx
    SelectObject( hDC, hOldFont )
    ReleaseDC( hHeader, hDC )
    if bestW > 0 then bestW += PsColumnHeader_GetPadding( hHeader ) * 2
    dim as string s = "header autosize col=" & str(idx) & " -> " & str(bestW)
    print s
    return bestW
end function


' ========================================================================================
' Geometry/contract self-test (run with PSLISTTREE_SELFTEST=1). Asserts the column rect
' math, divider hit zones, and the painted cell contract -- captured from the REAL
' paint path, never re-derived (PsHScrollBar's self-test pattern).
' ========================================================================================
dim shared gFailCount as integer = 0
dim shared gSelfTestCells(any) as PSLISTTREE_CELLINFO
dim shared gSelfTestCellCount as integer = -1     ' -1 = capture never ran
' The row's OWN rect, captured alongside its cells. Rows are painted in the surface's
' client coordinates now, so "where is this row" is no longer a constant the test can
' assume -- it has to be read from the same PAINTINFO the cells came from.
dim shared gSelfTestRowRc as RECT
dim shared gSelfTestRowIsHeader as boolean = false
dim shared gSelfTestRowIsSpanned as boolean = false
dim shared gSelfTestWantRow as integer = -2       ' model row to capture; -2 = any row
' Tree fields captured from the same PAINTINFO (added for the tree/edit self-test).
dim shared gSelfTestLevel       as integer = -1
dim shared gSelfTestHasChildren as boolean = false
dim shared gSelfTestIsExpanded  as boolean = false
dim shared gSelfTestIndent      as integer = -1
dim shared gSelfTestTwistyRc    as RECT

' WM_MOUSEMOVE recorder: captures the model row the control reports under the cursor, so
' the "idx is populated on a move" contract can be asserted without a real mouse.
dim shared gMoveIdxSeen as integer = -99

private function SelfTest_MoveRecorder( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false
    if m->uMsg = WM_MOUSEMOVE then gMoveIdxSeen = m->idx
    return false        ' observe only; the control's own handling must still run
end function

' SelChange recorder: the callback fires only for USER-driven selection moves, so the
' self-test can assert both that it fires (keyboard) and that it does not (setters).
dim shared gSelChangeCount as integer = 0
dim shared gSelChangeRow as integer = -99

sub SelfTest_SelChange( byval hCtl as HWND, byval row as integer )
    gSelChangeCount += 1
    gSelChangeRow = row
end sub

' Activate recorder: double-click / ENTER fire the row's default action.
dim shared gActivateCount as integer = 0
dim shared gActivateRow as integer = -99

sub SelfTest_Activate( byval hCtl as HWND, byval row as integer )
    gActivateCount += 1
    gActivateRow = row
end sub

' Edit-lifecycle test callbacks: one vetoes BeginEdit, one rejects the committed text.
function SelfTest_VetoBegin( byval hCtl as HWND, byval row as integer ) as boolean
    return false
end function
function SelfTest_RejectEnd( byval hCtl as HWND, byval row as integer, byval newText as DWSTRING ) as boolean
    return false
end function

' A child window's rect expressed in its PARENT's client coordinates. The border tests are
' entirely about where the container puts its children, and that is the coordinate space the
' question lives in -- GetClientRect on the child would answer 0,0,w,h and say nothing.
function SelfTest_ChildRectInParent( byval hChild as HWND, byval hParent as HWND ) as RECT
    dim as RECT rc
    GetWindowRect( hChild, @rc )
    MapWindowPoints( HWND_DESKTOP, hParent, cast(LPPOINT, @rc), 2 )
    return rc
end function

function SelfTest_RectStr( byref rc as RECT ) as string
    return "[" & str(rc.left) & "," & str(rc.top) & ".." & str(rc.right) & "," & str(rc.bottom) & "]"
end function

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

' Swapped-in row paint callback: copies the requested model row's column info out of
' the live PAINTINFO (the pointer is only valid during the call, so deep-copy).
sub SelfTest_RowCapturePaint( byval p as PSLISTTREE_PAINTINFO ptr )
    if (gSelfTestWantRow <> -2) andalso (p->itemID <> gSelfTestWantRow) then exit sub
    gSelfTestCellCount   = p->columnCount
    gSelfTestRowIsHeader = p->isHeader
    gSelfTestRowIsSpanned = p->isSpanned
    gSelfTestRowRc       = p->rc
    gSelfTestLevel       = p->level
    gSelfTestHasChildren = p->hasChildren
    gSelfTestIsExpanded  = p->isExpanded
    gSelfTestIndent      = p->indent
    gSelfTestTwistyRc    = p->rcTwisty
    if p->columnCount > 0 then
        redim gSelfTestCells( 0 to p->columnCount - 1 )
        for c as integer = 0 to p->columnCount - 1
            gSelfTestCells(c) = p->cells[c]
        next
    end if
end sub

' Force a synchronous repaint of an instance's listbox child; TRUE if the capture
' callback saw the requested row (it must be scrolled into view).
function SelfTest_GrabRow( byval hCtl as HWND, byval modelRow as integer ) as boolean
    gSelfTestWantRow = modelRow
    gSelfTestCellCount = -1
    AfxRedrawWindow( GetDlgItem( hCtl, IDC_FRMMAIN_LISTBOX ) )
    return (gSelfTestCellCount <> -1)
end function

' Find the MODEL index of the first row whose itemData matches, or -1. Used by the drag
' self-test to track a row across reorders by identity rather than by a fixed index.
function SelfTest_RowByData( byval hCtl as HWND, byval dataVal as integer ) as integer
    for i as integer = 0 to PsListTree_GetCount( hCtl ) - 1
        if PsListTree_GetItemData( hCtl, i ) = dataVal then return i
    next
    return -1
end function

' Find the MODEL index of the first header row with the given text, or -1.
function SelfTest_HeaderByText( byval hCtl as HWND, byval sText as DWSTRING ) as integer
    for i as integer = 0 to PsListTree_GetCount( hCtl ) - 1
        if PsListTree_IsHeader( hCtl, i ) andalso (PsListTree_GetText( hCtl, i ) = sText) then return i
    next
    return -1
end function

sub frmMain_RunSelfTest()
    dim as HWND hB   = HWND_FRMEXPLORER2
    dim as HWND hHdr = HWND_COLHDR
    dim pWindow as CWindow ptr = AfxCWindowPtr( hHdr )

    print ""
    print "=== PsListTree columns/header self-test ==="

    ' ---- 1. Column rect math (widths 160 / 70 / fill on the live client width) ----
    dim as RECT rcC : GetClientRect( hHdr, @rcC )
    dim as integer clientW = rcC.right - rcC.left
    dim as RECT rc0, rc1, rc2
    PsColumnHeader_GetColumnRect( hHdr, 0, rc0 )
    PsColumnHeader_GetColumnRect( hHdr, 1, rc1 )
    PsColumnHeader_GetColumnRect( hHdr, 2, rc2 )
    SelfTest_Check( "col0 = [0..160]", (rc0.left = 0) andalso (rc0.right = 160), _
        "l=" & str(rc0.left) & " r=" & str(rc0.right) )
    SelfTest_Check( "col1 = [160..230]", (rc1.left = 160) andalso (rc1.right = 230), _
        "l=" & str(rc1.left) & " r=" & str(rc1.right) )
    SelfTest_Check( "columns contiguous", (rc1.left = rc0.right) andalso (rc2.left = rc1.right), _
        "c1.l=" & str(rc1.left) & " c2.l=" & str(rc2.left) )
    SelfTest_Check( "fill.right = clientW", (rc2.right = clientW), _
        "r=" & str(rc2.right) & " clientW=" & str(clientW) )

    ' ---- 2. Programmatic width setter: clamps to the scaled minimum, and is SILENT ----
    dim as integer minDefault = CCOLHDR_DEFAULT_MINWIDTH
    if pWindow then minDefault = pWindow->ScaleX( CCOLHDR_DEFAULT_MINWIDTH )
    dim as integer notifyBefore = gResizeNotifyCount
    PsListTree_SetColumnWidth( hB, 1, 5 )
    SelfTest_Check( "SetColumnWidth clamps to scaled min", _
        (PsListTree_GetColumnWidth( hB, 1 ) = minDefault), _
        "w=" & str(PsListTree_GetColumnWidth( hB, 1 )) & " min=" & str(minDefault) )
    SelfTest_Check( "programmatic width change is silent", _
        (gResizeNotifyCount = notifyBefore), _
        "notifies=" & str(gResizeNotifyCount - notifyBefore) )
    PsListTree_SetColumnWidth( hB, 1, 70 )    ' restore

    ' ---- 3. Divider hit zones: boundary +/- gutter hits, one past misses, and the
    '         fill column's own divider is never grabbable ----
    dim as integer gutter = CCOLHDR_DIVIDER_GUTTER
    if pWindow then gutter = pWindow->ScaleX( CCOLHDR_DIVIDER_GUTTER )
    PsColumnHeader_GetColumnRect( hHdr, 0, rc0 )
    PsColumnHeader_GetColumnRect( hHdr, 2, rc2 )
    dim as integer bx = rc0.right
    dim as integer midY = (rcC.bottom - rcC.top) \ 2
    SelfTest_Check( "divider hits at boundary", _
        (PsColumnHeader_HitTestDivider( hHdr, bx, midY ) = 0), "x=" & str(bx) )
    SelfTest_Check( "divider hits at boundary-gutter", _
        (PsColumnHeader_HitTestDivider( hHdr, bx - gutter, midY ) = 0), "x=" & str(bx - gutter) )
    SelfTest_Check( "divider hits at boundary+gutter", _
        (PsColumnHeader_HitTestDivider( hHdr, bx + gutter, midY ) = 0), "x=" & str(bx + gutter) )
    SelfTest_Check( "no divider at boundary-(gutter+1)", _
        (PsColumnHeader_HitTestDivider( hHdr, bx - gutter - 1, midY ) = -1), "x=" & str(bx - gutter - 1) )
    SelfTest_Check( "no divider at boundary+(gutter+1)", _
        (PsColumnHeader_HitTestDivider( hHdr, bx + gutter + 1, midY ) = -1), "x=" & str(bx + gutter + 1) )
    SelfTest_Check( "fill column's divider not grabbable", _
        (PsColumnHeader_HitTestDivider( hHdr, rc2.right, midY ) = -1), "x=" & str(rc2.right) )

    ' ---- 4. Painted cells: captured from the real paint path, equal to the
    '         header's rects, y spanning the row height; sparse cells read empty ----
    PsListTree_SetPaintCallback( hB, @SelfTest_RowCapturePaint )
    if SelfTest_GrabRow( hB, 1 ) then
        SelfTest_Check( "item row captures columnCount=3", (gSelfTestCellCount = 3), _
            "n=" & str(gSelfTestCellCount) )
        if gSelfTestCellCount = 3 then
            dim as boolean okX = true
            dim as RECT rcH
            for c as integer = 0 to 2
                PsColumnHeader_GetColumnRect( hHdr, c, rcH )
                if (gSelfTestCells(c).rc.left <> rcH.left) orelse (gSelfTestCells(c).rc.right <> rcH.right) then okX = false
            next
            SelfTest_Check( "cell x-spans == header rects", okX, _
                "c0=[" & str(gSelfTestCells(0).rc.left) & ".." & str(gSelfTestCells(0).rc.right) & _
                "] c1=[" & str(gSelfTestCells(1).rc.left) & ".." & str(gSelfTestCells(1).rc.right) & "]" )
            ' CONTRACT CHANGE, and this assertion is the one that caught it. Cell rects
            ' used to arrive in a row-sized buffer starting at y=0, so this checked
            ' top = 0 and bottom = LB_GETITEMHEIGHT. Both halves are now wrong: rows are
            ' painted in the surface's CLIENT coordinates, and there is no listbox left
            ' to ask for an item height (LB_GETITEMHEIGHT returned 0, which is why the
            ' failure read "rowH=0").
            '
            ' Checked against the ROW'S OWN rect rather than against any absolute origin,
            ' which is what it should always have said -- a cell spans exactly its row,
            ' wherever that row happens to be. A host row painter that derives everything
            ' from p->rc needs no change for this; one that hardcoded 0 for the top does.
            dim as integer rowH = gSelfTestRowRc.bottom - gSelfTestRowRc.top
            SelfTest_Check( "cell y-span = the row's own y-span", _
                (rowH > 0) andalso _
                (gSelfTestCells(0).rc.top = gSelfTestRowRc.top) andalso _
                (gSelfTestCells(0).rc.bottom = gSelfTestRowRc.bottom), _
                "cell=[" & str(gSelfTestCells(0).rc.top) & ".." & str(gSelfTestCells(0).rc.bottom) & _
                "] row=[" & str(gSelfTestRowRc.top) & ".." & str(gSelfTestRowRc.bottom) & _
                "] rowH=" & str(rowH) )
        end if
    else
        SelfTest_Check( "paint captured (item row)", false, "the row paint callback never ran" )
    end if
    SelfTest_Check( "sparse folder Size cell reads empty", _
        (len(PsListTree_GetCellText( hB, 43, 1 )) = 0), _
        "len=" & str(len(PsListTree_GetCellText( hB, 43, 1 ))) )

    ' ---- 5. A width change reflows the painted cells ----
    PsListTree_SetColumnWidth( hB, 0, 150 )
    if SelfTest_GrabRow( hB, 1 ) then
        dim as boolean okW = (gSelfTestCellCount = 3)
        if okW then okW = (gSelfTestCells(1).rc.left = 150)
        SelfTest_Check( "SetColumnWidth(0,150) -> cell1.left = 150", okW, _
            "left=" & str(gSelfTestCells(1).rc.left) )
    else
        SelfTest_Check( "paint captured (width reflow)", false, "the row paint callback never ran" )
    end if
    PsListTree_SetColumnWidth( hB, 0, 160 )   ' restore

    ' ---- 6. Group-header rows span the full width (columnCount = 0) ----
    if SelfTest_GrabRow( hB, 0 ) then
        dim as boolean okHdr = (gSelfTestCellCount = 0)
        if okHdr then okHdr = gSelfTestRowIsHeader
        SelfTest_Check( "group header captures columnCount=0", okHdr, _
            "n=" & str(gSelfTestCellCount) )
    else
        SelfTest_Check( "paint captured (group header)", false, "the row paint callback never ran" )
    end if

    ' ---- 7. Instance A (no columns): the pre-columns contract, proven not assumed ----
    PsListTree_SetPaintCallback( HWND_FRMEXPLORER, @SelfTest_RowCapturePaint )
    if SelfTest_GrabRow( HWND_FRMEXPLORER, -2 ) then
        SelfTest_Check( "no-columns instance captures columnCount=0", _
            (gSelfTestCellCount = 0), "n=" & str(gSelfTestCellCount) )
    else
        SelfTest_Check( "paint captured (instance A)", false, "the row paint callback never ran" )
    end if

    ' ---- 7b. SelChange: fires for user-driven moves ONLY, and never twice for the same
    '          row. The keyboard case is the reason the callback exists at all -- the
    '          control consumes WM_KEYDOWN, so nothing else can observe arrow navigation.
    PsListTree_SetSelChangeCallback( hB, @SelfTest_SelChange )
    dim as HWND hInner = GetDlgItem( hB, IDC_FRMMAIN_LISTBOX )

    gSelChangeCount = 0
    PsListTree_SetCurSel( hB, 2 )
    SelfTest_Check( "SetCurSel is silent", (gSelChangeCount = 0), _
        "fires=" & str(gSelChangeCount) )

    gSelChangeCount = 0
    SendMessage( hInner, WM_KEYDOWN, VK_DOWN, 0 )
    dim as integer rowAfterDown = PsListTree_GetCurSel( hB )
    SelfTest_Check( "VK_DOWN fires exactly once", (gSelChangeCount = 1), _
        "fires=" & str(gSelChangeCount) )
    SelfTest_Check( "SelChange row = GetCurSel", (gSelChangeRow = rowAfterDown), _
        "cb=" & str(gSelChangeRow) & " GetCurSel=" & str(rowAfterDown) )
    SelfTest_Check( "VK_DOWN actually moved off row 2", (rowAfterDown <> 2), _
        "row=" & str(rowAfterDown) )

    gSelChangeCount = 0
    SendMessage( hInner, WM_KEYDOWN, VK_DOWN, 0 )
    SelfTest_Check( "second VK_DOWN fires once more", (gSelChangeCount = 1), _
        "fires=" & str(gSelChangeCount) )

    ' Re-setting the row that is already current must stay silent.
    gSelChangeCount = 0
    PsListTree_SetCurSel( hB, PsListTree_GetCurSel( hB ) )
    SelfTest_Check( "SetCurSel to the current row is silent", (gSelChangeCount = 0), _
        "fires=" & str(gSelChangeCount) )

    ' Clamped navigation does not move the focus, so it must not notify either.
    PsListTree_SetCurSel( hB, 0 )
    gSelChangeCount = 0
    SendMessage( hInner, WM_KEYDOWN, VK_UP, 0 )
    SelfTest_Check( "VK_UP clamped at the top is silent", (gSelChangeCount = 0), _
        "fires=" & str(gSelChangeCount) & " row=" & str(PsListTree_GetCurSel(hB)) )

    ' A host that never sets the callback must be unaffected.
    PsListTree_SetSelChangeCallback( hB, 0 )
    gSelChangeCount = 0
    SendMessage( hInner, WM_KEYDOWN, VK_DOWN, 0 )
    SelfTest_Check( "cleared callback stops firing", (gSelChangeCount = 0), _
        "fires=" & str(gSelChangeCount) )

    ' ---- 7c. Spanned row: an ordinary item row painted as ONE cell across the whole
    '          column band, its text kept in column 0, with a clean round-trip back ----
    PsListTree_SetPaintCallback( hB, @SelfTest_RowCapturePaint )   ' 7b left SelChange only
    dim as DWSTRING sSpanText = PsListTree_GetText( hB, 2 )
    SelfTest_Check( "SetRowSpanColumns returns TRUE", _
        PsListTree_SetRowSpanColumns( hB, 2, true ), "" )
    SelfTest_Check( "GetRowSpanColumns reflects the setter", _
        PsListTree_GetRowSpanColumns( hB, 2 ), "" )
    if SelfTest_GrabRow( hB, 2 ) then
        SelfTest_Check( "spanned row captures columnCount=1", (gSelfTestCellCount = 1), _
            "n=" & str(gSelfTestCellCount) )
        SelfTest_Check( "spanned row sets isSpanned", gSelfTestRowIsSpanned, "" )
        if gSelfTestCellCount = 1 then
            dim as RECT rcA, rcZ
            PsColumnHeader_GetColumnRect( hHdr, 0, rcA )
            PsColumnHeader_GetColumnRect( hHdr, 2, rcZ )
            SelfTest_Check( "spanned cell spans col0.left..lastcol.right", _
                (gSelfTestCells(0).rc.left = rcA.left) andalso (gSelfTestCells(0).rc.right = rcZ.right), _
                "cell=[" & str(gSelfTestCells(0).rc.left) & ".." & str(gSelfTestCells(0).rc.right) & _
                "] band=[" & str(rcA.left) & ".." & str(rcZ.right) & "]" )
            SelfTest_Check( "spanned cell y-span = the row's own y-span", _
                (gSelfTestCells(0).rc.top = gSelfTestRowRc.top) andalso _
                (gSelfTestCells(0).rc.bottom = gSelfTestRowRc.bottom), _
                "cell=[" & str(gSelfTestCells(0).rc.top) & ".." & str(gSelfTestCells(0).rc.bottom) & _
                "] row=[" & str(gSelfTestRowRc.top) & ".." & str(gSelfTestRowRc.bottom) & "]" )
            SelfTest_Check( "spanned cell text = column 0 text", _
                (gSelfTestCells(0).wszText = sSpanText), _
                "len=" & str(len(gSelfTestCells(0).wszText)) )
        end if
    else
        SelfTest_Check( "paint captured (spanned row)", false, "the row paint callback never ran" )
    end if
    ' Round-trip: clearing the flag restores the per-column contract.
    PsListTree_SetRowSpanColumns( hB, 2, false )
    if SelfTest_GrabRow( hB, 2 ) then
        SelfTest_Check( "clearing span restores columnCount=3 and isSpanned=false", _
            (gSelfTestCellCount = 3) andalso (gSelfTestRowIsSpanned = false), _
            "n=" & str(gSelfTestCellCount) & " span=" & str(gSelfTestRowIsSpanned) )
    else
        SelfTest_Check( "paint captured (span cleared)", false, "the row paint callback never ran" )
    end if

    ' ---- 7d. Non-selectable rows: blocked on every path (mouse via the shared gate, the
    '          keyboard skips them, and the programmatic setters refuse them), and marking a
    '          live row non-selectable clears its selection/focus ----
    '  Rows 4..7 are file items (Files group expanded); row 5 is the guinea pig.
    PsListTree_SetCurSel( hB, 4 )
    SelfTest_Check( "SetRowSelectable returns TRUE", _
        PsListTree_SetRowSelectable( hB, 5, false ), "" )
    SelfTest_Check( "GetRowSelectable reflects the setter", _
        (PsListTree_GetRowSelectable( hB, 5 ) = false), "" )

    ' Keyboard DOWN from row 4 skips the non-selectable row 5 and lands on row 6.
    gSelChangeCount = 0
    SendMessage( hInner, WM_KEYDOWN, VK_DOWN, 0 )
    SelfTest_Check( "VK_DOWN skips the non-selectable row", _
        (PsListTree_GetCurSel( hB ) = 6), "cur=" & str(PsListTree_GetCurSel( hB )) )
    ' UP from row 6 skips it again, back to row 4.
    SendMessage( hInner, WM_KEYDOWN, VK_UP, 0 )
    SelfTest_Check( "VK_UP skips it back to row 4", _
        (PsListTree_GetCurSel( hB ) = 4), "cur=" & str(PsListTree_GetCurSel( hB )) )

    ' Programmatic setters refuse it, and leave the current row untouched.
    dim as integer retSetCur = PsListTree_SetCurSel( hB, 5 )
    SelfTest_Check( "SetCurSel on a non-selectable row returns -1", _
        (retSetCur = -1), "ret=" & str(retSetCur) )
    SelfTest_Check( "...and the current row is unchanged", _
        (PsListTree_GetCurSel( hB ) = 4), "cur=" & str(PsListTree_GetCurSel( hB )) )
    SelfTest_Check( "SetSel(true) on a non-selectable row returns FALSE", _
        (PsListTree_SetSel( hB, 5, true ) = false), "" )
    SelfTest_Check( "...and it did not become selected", _
        (PsListTree_GetSel( hB, 5 ) = false), "" )
    PsListTree_SelectAll( hB, true )
    SelfTest_Check( "SelectAll skips the non-selectable row", _
        (PsListTree_GetSel( hB, 5 ) = false), "" )
    SelfTest_Check( "...but selects a selectable neighbour", _
        (PsListTree_GetSel( hB, 4 ) = true), "" )
    PsListTree_SelectAll( hB, false )

    ' Marking a currently selected+focused row non-selectable clears it and drops the caret.
    PsListTree_SetCurSel( hB, 7 )
    PsListTree_SetRowSelectable( hB, 7, false )
    SelfTest_Check( "making the focused row non-selectable deselects it", _
        (PsListTree_GetSel( hB, 7 ) = false), "" )
    SelfTest_Check( "...and drops the caret (GetCurSel = -1)", _
        (PsListTree_GetCurSel( hB ) = -1), "cur=" & str(PsListTree_GetCurSel( hB )) )

    ' Round-trip: re-enabling restores normal behaviour.
    PsListTree_SetRowSelectable( hB, 5, true )
    PsListTree_SetRowSelectable( hB, 7, true )
    dim as integer retReenabled = PsListTree_SetCurSel( hB, 5 )
    SelfTest_Check( "re-enabled row selects again", _
        (retReenabled = 5), "ret=" & str(retReenabled) )

    ' ---- 7e. Non-selectable FIRST row + mid-list skip on instance A (extended-select).
    '          A plain Up from the second row has nowhere selectable to land above it, so the
    '          caret stays put; Home skips the leading non-selectable row; Up/Down step over a
    '          non-selectable row mid-list. (Shift-RANGE skip is only reachable by hand -- the
    '          keydown handler reads GetAsyncKeyState(VK_SHIFT), which SendMessage cannot set --
    '          but it shares the same model-level guard as SelectAll/SetSel, asserted above.)
    dim as HWND hA = HWND_FRMEXPLORER
    dim as HWND hInnerA = GetDlgItem( hA, IDC_FRMMAIN_LISTBOX )
    PsListTree_SetCurSel( hA, 1 )
    SendMessage( hInnerA, WM_KEYDOWN, VK_UP, 0 )
    SelfTest_Check( "Up from row 1 stays put (row 0 non-selectable, nothing above)", _
        (PsListTree_GetCurSel( hA ) = 1), "cur=" & str(PsListTree_GetCurSel( hA )) )
    SendMessage( hInnerA, WM_KEYDOWN, VK_HOME, 0 )
    SelfTest_Check( "Home skips the leading non-selectable row 0 -> row 1", _
        (PsListTree_GetCurSel( hA ) = 1), "cur=" & str(PsListTree_GetCurSel( hA )) )
    PsListTree_SetCurSel( hA, 7 )
    SendMessage( hInnerA, WM_KEYDOWN, VK_DOWN, 0 )
    SelfTest_Check( "Down from row 7 skips non-selectable row 8 -> row 9", _
        (PsListTree_GetCurSel( hA ) = 9), "cur=" & str(PsListTree_GetCurSel( hA )) )
    SendMessage( hInnerA, WM_KEYDOWN, VK_UP, 0 )
    SelfTest_Check( "Up from row 9 skips row 8 -> row 7", _
        (PsListTree_GetCurSel( hA ) = 7), "cur=" & str(PsListTree_GetCurSel( hA )) )
    PsListTree_SetCurSel( hA, 1 )   ' restore

    ' ---- 8. DESTRUCTIVE (last): column delete/insert shift the per-row cells across
    '         the Text<->cells(0) alias, the fill designation self-heals, and a SPANNED
    '         row is EXEMPT from the shift (its text stays in column 0) ----
    dim as DWSTRING sSpanKeep = PsListTree_GetText( hB, 2 )
    PsListTree_SetRowSpanColumns( hB, 2, true )
    PsListTree_DeleteColumn( hB, 2 )
    SelfTest_Check( "delete fill col -> effective fill = last (1)", _
        (PsListTree_GetFillColumn( hB ) = 1), "fill=" & str(PsListTree_GetFillColumn( hB )) )
    SelfTest_Check( "deleted column's cells removed", _
        (len(PsListTree_GetCellText( hB, 1, 2 )) = 0), _
        "len=" & str(len(PsListTree_GetCellText( hB, 1, 2 ))) )
    dim as string sSize = PsListTree_GetCellText( hB, 1, 1 )
    SelfTest_Check( "cells before the deleted column survive", (sSize = "137"), "c1=" & sSize )
    PsListTree_InsertColumn( hB, 0, "Icon", 40 )
    dim as string sShift0 = PsListTree_GetCellText( hB, 1, 0 )
    dim as string sShift1 = PsListTree_GetCellText( hB, 1, 1 )
    dim as boolean okIns = (len(sShift0) = 0) andalso (sShift1 = "file0.bas")
    SelfTest_Check( "insert at 0 shifts Text into column 1", okIns, _
        "c0len=" & str(len(sShift0)) & " c1=" & sShift1 )

    ' The spanned row rode through both a DeleteColumn and an InsertColumn(0) with its
    ' text untouched in column 0 -- the exemption at PsListTree_ShiftCellColumns. Without
    ' it the insert-at-0 would have pushed the caption into column 1 (as it did for row 1
    ' above) and the spanned row would silently paint blank.
    SelfTest_Check( "spanned row is exempt from column shifts", _
        (PsListTree_GetText( hB, 2 ) = sSpanKeep), _
        "keep=" & str(len(sSpanKeep)) & " now=" & str(len(PsListTree_GetText( hB, 2 ))) )
    PsListTree_SetRowSpanColumns( hB, 2, false )

    ' ---- 9. Drag-reorder MODEL move (PsListTree_MoveRows). The gesture itself -- capture,
    '         threshold, indicator, auto-scroll, CanDrop veto -- is interactive-only (capture
    '         can't be synthesized; see Learnings.md); these assert the reorder index math on
    '         instance A, tracking rows by itemData so a move never confuses the next check.
    '         DESTRUCTIVE to instance A's order, so it runs last. ----
    dim as HWND hMv = HWND_FRMEXPLORER
    dim srcMv() as integer

    ' 9a. Single row to the end: itemData 1000 ("Alpha item 0") lands last; focus follows.
    dim as integer r1000 = SelfTest_RowByData( hMv, 1000 )
    dim as integer cntA  = PsListTree_GetCount( hMv )
    redim srcMv( 0 to 0 ) : srcMv(0) = r1000
    dim as integer nf9a = PsListTree_MoveRows( hMv, srcMv(), cntA )
    SelfTest_Check( "MoveRows single -> last row, returns its index", _
        (nf9a = cntA - 1) andalso (SelfTest_RowByData( hMv, 1000 ) = cntA - 1), _
        "nf=" & str(nf9a) & " at=" & str(SelfTest_RowByData( hMv, 1000 )) )
    SelfTest_Check( "MoveRows puts the caret on the moved block", _
        (PsListTree_GetCurSel( hMv ) = nf9a), "cur=" & str(PsListTree_GetCurSel( hMv )) )

    ' 9b. Non-contiguous pair gathers contiguously, in original order, before the target.
    dim as integer p1001 = SelfTest_RowByData( hMv, 1001 )
    dim as integer p1003 = SelfTest_RowByData( hMv, 1003 )
    dim as integer tgt2000 = SelfTest_RowByData( hMv, 2000 )
    redim srcMv( 0 to 1 ) : srcMv(0) = p1001 : srcMv(1) = p1003
    PsListTree_MoveRows( hMv, srcMv(), tgt2000 )
    dim as integer q1001 = SelfTest_RowByData( hMv, 1001 )
    dim as integer q1003 = SelfTest_RowByData( hMv, 1003 )
    SelfTest_Check( "MoveRows gathers a non-contiguous pair, order kept", _
        (q1003 = q1001 + 1), "1001@" & str(q1001) & " 1003@" & str(q1003) )
    SelfTest_Check( "...and lands the block just before the target row", _
        (SelfTest_RowByData( hMv, 2000 ) = q1003 + 1), _
        "2000@" & str(SelfTest_RowByData( hMv, 2000 )) )

    ' 9c. Drop onto a header -> first child (right after the header), regardless of shift.
    dim as integer hBeta = SelfTest_HeaderByText( hMv, "Group Beta" )
    dim as integer r1002 = SelfTest_RowByData( hMv, 1002 )
    redim srcMv( 0 to 0 ) : srcMv(0) = r1002
    PsListTree_MoveRows( hMv, srcMv(), hBeta + 1 )
    SelfTest_Check( "MoveRows onto header -> first child", _
        (SelfTest_RowByData( hMv, 1002 ) = SelfTest_HeaderByText( hMv, "Group Beta" ) + 1), _
        "1002@" & str(SelfTest_RowByData( hMv, 1002 )) & " beta@" & str(SelfTest_HeaderByText( hMv, "Group Beta" )) )

    ' 9d. Selection travels with a moved row (it lives on the struct).
    dim as integer r1006 = SelfTest_RowByData( hMv, 1006 )
    PsListTree_SetSel( hMv, r1006, true )
    redim srcMv( 0 to 0 ) : srcMv(0) = r1006
    dim as integer nf9d = PsListTree_MoveRows( hMv, srcMv(), PsListTree_GetCount( hMv ) )
    SelfTest_Check( "moved row keeps its selection", _
        (nf9d >= 0) andalso PsListTree_GetSel( hMv, nf9d ), "nf=" & str(nf9d) )

    ' 9e. A header in the source set is ignored (structural rows never move).
    dim as integer hBeta2 = SelfTest_HeaderByText( hMv, "Group Beta" )
    dim as integer r2001  = SelfTest_RowByData( hMv, 2001 )
    dim as integer cntB4  = PsListTree_GetCount( hMv )
    redim srcMv( 0 to 1 ) : srcMv(0) = hBeta2 : srcMv(1) = r2001
    PsListTree_MoveRows( hMv, srcMv(), 3 )
    SelfTest_Check( "header in the source set is ignored", _
        (SelfTest_HeaderByText( hMv, "Group Beta" ) >= 0) andalso (PsListTree_GetCount( hMv ) = cntB4), _
        "beta@" & str(SelfTest_HeaderByText( hMv, "Group Beta" )) & " cnt=" & str(PsListTree_GetCount( hMv )) )

    ' 9f. MoveRows moves a NON-SELECTABLE item too: the no-drag rule is a gesture property,
    '     not a model one, so the programmatic primitive is unrestricted (headers aside).
    dim as integer r1005 = SelfTest_RowByData( hMv, 1005 )   ' "Alpha item 5", non-selectable
    redim srcMv( 0 to 0 ) : srcMv(0) = r1005
    PsListTree_MoveRows( hMv, srcMv(), PsListTree_GetCount( hMv ) )
    SelfTest_Check( "MoveRows moves a non-selectable item (gesture-only restriction)", _
        (SelfTest_RowByData( hMv, 1005 ) = PsListTree_GetCount( hMv ) - 1), _
        "1005@" & str(SelfTest_RowByData( hMv, 1005 )) )

    ' ====================================================================================
    ' 10. TREE + IN-PLACE EDITING (instance C). C is the AddNode tree built in WinMain,
    '     with tree indent + control-drawn twisties on. Model layout (before any collapse):
    '       0 src(L0)  1 AfxNova(L1)  2 CWindow.inc(L2)  3 controls(L2)  4 PsListTree.inc(L3)
    '       5 PsTextBox.inc(L3)  6 AfxStr.inc(L2)  7 demo(L1)  8 main.bas(L2)  9 frmMain.inc(L2)
    '      10 docs(L0)  11 README.md(L1)  12 REFACTOR_PLAN.md(L1)  13 LICENSE(L0 leaf)
    ' ====================================================================================
    dim as HWND hC = HWND_FRMEXPLORER3

    ' ---- 10a. Derived tree structure (works on real AddNode depth) ----
    SelfTest_Check( "GetLevel(src)=0 / GetLevel(controls)=2 / GetLevel(PsListTree.inc)=3", _
        (PsListTree_GetLevel(hC,0)=0) andalso (PsListTree_GetLevel(hC,3)=2) andalso (PsListTree_GetLevel(hC,4)=3), _
        "L0=" & str(PsListTree_GetLevel(hC,0)) & " L3=" & str(PsListTree_GetLevel(hC,3)) & " L4=" & str(PsListTree_GetLevel(hC,4)) )
    SelfTest_Check( "HasChildren: src=T, CWindow.inc(leaf)=F", _
        (PsListTree_HasChildren(hC,0) = true) andalso (PsListTree_HasChildren(hC,2) = false), "" )
    SelfTest_Check( "GetChildCount: src=2 (AfxNova,demo), AfxNova=3", _
        (PsListTree_GetChildCount(hC,0)=2) andalso (PsListTree_GetChildCount(hC,1)=3), _
        "src=" & str(PsListTree_GetChildCount(hC,0)) & " afx=" & str(PsListTree_GetChildCount(hC,1)) )
    SelfTest_Check( "GetParent: PsListTree.inc->controls(3), src->-1", _
        (PsListTree_GetParent(hC,4)=3) andalso (PsListTree_GetParent(hC,0)=-1), _
        "p4=" & str(PsListTree_GetParent(hC,4)) & " p0=" & str(PsListTree_GetParent(hC,0)) )

    ' ---- 10b. Subtree collapse hides an ENTIRE subtree at any depth ----
    PsListTree_ExpandAll( hC )
    dim as integer visAll = PsListTree_GetVisibleCount( hC )
    SelfTest_Check( "ExpandAll -> all 14 nodes visible", (visAll = 14), "vis=" & str(visAll) )
    PsListTree_CollapseRow( hC, 3 )                      ' controls (L2): hides PsListTree.inc + PsTextBox.inc
    SelfTest_Check( "collapse a depth-2 node hides its 2 children -> 12", _
        (PsListTree_GetVisibleCount(hC) = 12), "vis=" & str(PsListTree_GetVisibleCount(hC)) )
    PsListTree_ExpandRow( hC, 3 )
    SelfTest_Check( "expand it back -> 14", (PsListTree_GetVisibleCount(hC) = 14), _
        "vis=" & str(PsListTree_GetVisibleCount(hC)) )
    PsListTree_CollapseRow( hC, 1 )                      ' AfxNova (L1): hides its whole 5-node subtree
    SelfTest_Check( "collapse a depth-1 node hides its whole subtree (5) -> 9", _
        (PsListTree_GetVisibleCount(hC) = 9), "vis=" & str(PsListTree_GetVisibleCount(hC)) )
    PsListTree_CollapseRow( hC, 0 )                      ' src (L0): nested collapse, hides all under src
    SelfTest_Check( "collapse the root over an already-collapsed child -> 5 (roots + docs)", _
        (PsListTree_GetVisibleCount(hC) = 5), "vis=" & str(PsListTree_GetVisibleCount(hC)) )
    PsListTree_ExpandAll( hC )
    SelfTest_Check( "ExpandAll restores 14", (PsListTree_GetVisibleCount(hC) = 14), _
        "vis=" & str(PsListTree_GetVisibleCount(hC)) )

    ' ---- 10c. Indent + twisty GEOMETRY, captured from the REAL paint path ----
    dim pWinC as CWindow ptr = AfxCWindowPtr( hC )
    dim as integer indentPx = pWinC->ScaleX( PsListTree_GetIndentWidth(hC) )   ' 16 -> DPI px
    dim as integer twistyPx = pWinC->ScaleX( 16 )                             ' TwistyWidth default 16
    PsListTree_SetPaintCallback( hC, @SelfTest_RowCapturePaint )
    ' Root parent (src, L0): indent = 0*indent + twisty band; twisty at x=0, width = twistyPx.
    if SelfTest_GrabRow( hC, 0 ) then
        SelfTest_Check( "L0 parent: level=0, hasChildren, indent = twistyPx", _
            (gSelfTestLevel=0) andalso (gSelfTestHasChildren = true) andalso (gSelfTestIndent = twistyPx), _
            "lvl=" & str(gSelfTestLevel) & " indent=" & str(gSelfTestIndent) & " twistyPx=" & str(twistyPx) )
        SelfTest_Check( "L0 twisty rect: left=0, width=twistyPx", _
            (gSelfTestTwistyRc.left = 0) andalso ((gSelfTestTwistyRc.right - gSelfTestTwistyRc.left) = twistyPx), _
            "tw=[" & str(gSelfTestTwistyRc.left) & ".." & str(gSelfTestTwistyRc.right) & "]" )
    else
        SelfTest_Check( "paint captured (C row 0)", false, "row 0 never painted" )
    end if
    ' Depth-2 parent (controls, model row 3): indent = 2*indent + twisty; twisty.left = 2*indent.
    if SelfTest_GrabRow( hC, 3 ) then
        SelfTest_Check( "L2 parent: level=2, hasChildren, indent = 2*indentPx + twistyPx", _
            (gSelfTestLevel=2) andalso (gSelfTestHasChildren = true) andalso (gSelfTestIndent = 2*indentPx + twistyPx), _
            "indent=" & str(gSelfTestIndent) & " expect=" & str(2*indentPx + twistyPx) )
        SelfTest_Check( "L2 twisty rect: left = 2*indentPx, width = twistyPx", _
            (gSelfTestTwistyRc.left = 2*indentPx) andalso ((gSelfTestTwistyRc.right - gSelfTestTwistyRc.left) = twistyPx), _
            "tw.left=" & str(gSelfTestTwistyRc.left) & " expect=" & str(2*indentPx) )
    else
        SelfTest_Check( "paint captured (C row 3)", false, "row 3 never painted" )
    end if
    ' Leaf (CWindow.inc, model row 2): no twisty, hasChildren=false, but still indented by depth.
    if SelfTest_GrabRow( hC, 2 ) then
        SelfTest_Check( "L2 leaf: hasChildren=false and EMPTY twisty rect", _
            (gSelfTestHasChildren = false) andalso (gSelfTestTwistyRc.right = gSelfTestTwistyRc.left), _
            "hasCh=" & str(gSelfTestHasChildren) & " twW=" & str(gSelfTestTwistyRc.right - gSelfTestTwistyRc.left) )
        SelfTest_Check( "L2 leaf still indented: indent = 2*indentPx + twistyPx (band reserved)", _
            (gSelfTestIndent = 2*indentPx + twistyPx), "indent=" & str(gSelfTestIndent) )
    else
        SelfTest_Check( "paint captured (C row 2)", false, "row 2 never painted" )
    end if
    PsListTree_SetPaintCallback( hC, @PsListTree_PaintCallback )     ' restore

    ' ---- 10d. FLAT-LIST IDENTITY: with tree visuals OFF (instance A), indent is 0 and the
    '           twisty rect is empty -- the proof that existing hosts are visually untouched. ----
    PsListTree_SetPaintCallback( hA, @SelfTest_RowCapturePaint )
    if SelfTest_GrabRow( hA, 1 ) then
        SelfTest_Check( "tree visuals OFF: indent = 0 and twisty rect empty", _
            (gSelfTestIndent = 0) andalso (gSelfTestTwistyRc.right = gSelfTestTwistyRc.left), _
            "indent=" & str(gSelfTestIndent) & " twW=" & str(gSelfTestTwistyRc.right - gSelfTestTwistyRc.left) )
    else
        SelfTest_Check( "paint captured (A row 1)", false, "row 1 never painted" )
    end if
    PsListTree_SetPaintCallback( hA, @PsListTree_PaintCallback )     ' restore

    ' ---- 10d2. COLUMN-0-ONLY inset: in report (columns) mode the tree indent shifts ONLY
    '            cell 0; the other columns keep their x. Exercised on instance B (which has
    '            columns) by toggling tree indent and comparing the captured cell rects. This
    '            is the one inset path instance C (no columns) cannot reach. ----
    dim pWinB as CWindow ptr = AfxCWindowPtr( hB )
    PsListTree_SetPaintCallback( hB, @SelfTest_RowCapturePaint )
    dim as integer base0 = -1, base1 = -1, on0 = -1, on1 = -1
    if (SelfTest_GrabRow( hB, 1 ) = true) andalso (gSelfTestCellCount >= 2) then      ' row 1 is a level-1 file item
        base0 = gSelfTestCells(0).rc.left
        base1 = gSelfTestCells(1).rc.left
    end if
    PsListTree_SetTreeIndent( hB, true )
    dim as integer bIndent = pWinB->ScaleX( PsListTree_GetIndentWidth(hB) )
    if (SelfTest_GrabRow( hB, 1 ) = true) andalso (gSelfTestCellCount >= 2) then
        on0 = gSelfTestCells(0).rc.left
        on1 = gSelfTestCells(1).rc.left
    end if
    PsListTree_SetTreeIndent( hB, false )
    PsListTree_SetPaintCallback( hB, @PsListTree_PaintCallback )
    SelfTest_Check( "columns mode: tree indent shifts ONLY cell 0 (by 1 level), cell 1 fixed", _
        (on0 = base0 + bIndent) andalso (on1 = base1), _
        "c0 " & str(base0) & "->" & str(on0) & " (+" & str(bIndent) & ")  c1 " & str(base1) & "->" & str(on1) )

    ' ---- 10e. LEGACY HEADER+ITEM byte-compat: a legacy header IS a depth-1 parent, so the
    '           tree queries work on it and collapsing it still hides exactly its items. ----
    dim as integer hBetaC = SelfTest_HeaderByText( hMv, "Group Beta" )
    if hBetaC >= 0 then
        dim as integer betaKids = PsListTree_GetChildCount( hMv, hBetaC )    ' NEW API on a LEGACY header
        SelfTest_Check( "GetChildCount works on a legacy header", (betaKids > 0), "kids=" & str(betaKids) )
        PsListTree_ExpandRow( hMv, hBetaC )
        dim as integer visBefore = PsListTree_GetVisibleCount( hMv )
        PsListTree_CollapseRow( hMv, hBetaC )
        dim as integer hidden = visBefore - PsListTree_GetVisibleCount( hMv )
        SelfTest_Check( "collapsing a legacy header hides EXACTLY its child count", _
            (hidden = betaKids), "hidden=" & str(hidden) & " kids=" & str(betaKids) )
        PsListTree_ExpandRow( hMv, hBetaC )
        SelfTest_Check( "expanding it restores the visible count", _
            (PsListTree_GetVisibleCount(hMv) = visBefore), "vis=" & str(PsListTree_GetVisibleCount(hMv)) )
    else
        SelfTest_Check( "legacy Group Beta header located", false, "not found" )
    end if

    ' ---- 10f. In-place EDIT lifecycle (no message pump needed). The editor is a PsTextBox
    '           child of the surface with control id idc_ListTree+3, so the test can drive its
    '           text directly. ----
    dim as HWND hSurfC = GetDlgItem( hC, IDC_FRMMAIN_LISTBOX )
    ' Accept path: begin, drive text, commit -> model updated. Sub-conditions are hoisted to
    ' boolean locals so no andalso chain mixes a boolean with an integer relational (warning 38).
    dim as boolean okBegin = PsListTree_BeginEdit( hC, 0 )
    dim as boolean okEditing = PsListTree_IsEditing( hC )
    dim as boolean okEditRow = (PsListTree_GetEditRow(hC) = 0)
    SelfTest_Check( "BeginEdit -> IsEditing true, GetEditRow = 0", _
        okBegin andalso okEditing andalso okEditRow, _
        "editing=" & str(okEditing) & " row=" & str(PsListTree_GetEditRow(hC)) )
    dim as HWND hEditC = GetDlgItem( hSurfC, IDC_FRMMAIN_LISTBOX + 3 )
    PsTextBox_SetText( hEditC, "renamed_src" )
    dim as boolean okCommit = PsListTree_EndEdit( hC, true )
    dim as boolean okCommitText = (PsListTree_GetText(hC,0) = "renamed_src")
    dim as boolean okNotEditing = (PsListTree_IsEditing(hC) = false)
    dim as string  sCommit = PsListTree_GetText(hC,0)
    SelfTest_Check( "commit writes the edited text into the model", _
        okCommit andalso okCommitText andalso okNotEditing, "text=" & sCommit )
    ' EndLabelEdit reject: the new text is discarded, the old caption stays.
    PsListTree_SetEndLabelEditCallback( hC, @SelfTest_RejectEnd )
    PsListTree_BeginEdit( hC, 0 )
    dim as HWND hEditC2 = GetDlgItem( hSurfC, IDC_FRMMAIN_LISTBOX + 3 )
    PsTextBox_SetText( hEditC2, "SHOULD_NOT_STICK" )
    PsListTree_EndEdit( hC, true )
    dim as boolean okRejectKept = (PsListTree_GetText(hC,0) = "renamed_src")
    dim as string  sReject = PsListTree_GetText(hC,0)
    SelfTest_Check( "EndLabelEdit=FALSE keeps the old text", okRejectKept, "text=" & sReject )
    PsListTree_SetEndLabelEditCallback( hC, @Tree_EndLabelEdit )
    ' BeginLabelEdit veto: no edit starts.
    PsListTree_SetBeginLabelEditCallback( hC, @SelfTest_VetoBegin )
    dim as boolean okVeto = PsListTree_BeginEdit( hC, 0 )
    dim as boolean okVetoBlocked = (okVeto = false)
    dim as boolean okVetoNotEditing = (PsListTree_IsEditing(hC) = false)
    SelfTest_Check( "BeginLabelEdit=FALSE blocks the edit", _
        okVetoBlocked andalso okVetoNotEditing, "began=" & str(okVeto) )
    PsListTree_SetBeginLabelEditCallback( hC, @Tree_BeginLabelEdit )
    ' Cancel path: begin then EndEdit(false) leaves the model untouched.
    PsListTree_BeginEdit( hC, 0 )
    dim as HWND hEditC3 = GetDlgItem( hSurfC, IDC_FRMMAIN_LISTBOX + 3 )
    PsTextBox_SetText( hEditC3, "cancel_me" )
    PsListTree_EndEdit( hC, false )
    dim as boolean okCancelKept = (PsListTree_GetText(hC,0) = "renamed_src")
    dim as boolean okCancelNotEditing = (PsListTree_IsEditing(hC) = false)
    dim as string  sCancel = PsListTree_GetText(hC,0)
    SelfTest_Check( "EndEdit(false) cancels -> model unchanged", _
        okCancelKept andalso okCancelNotEditing, "text=" & sCancel )

    ' ---- 10g. ENTER commits / ESC cancels THROUGH THE PUMP FILTER. The host pump's
    '           IsDialogMessage eats Enter (default button) and Esc (cancel) before they reach
    '           the editor, so PsListTree_FilterMessage must claim them first. Drive that exact
    '           path with a synthesized WM_KEYDOWN whose hwnd is the editor's RichEdit -- the
    '           real focus target -- just as GetMessage would hand it to the filter. ----
    PsListTree_SetText( hC, 0, "src" )                          ' reset the caption
    ' ENTER: the filter must claim it, commit the edited text, and end the edit.
    PsListTree_BeginEdit( hC, 0 )
    dim as HWND hEditE = GetDlgItem( hSurfC, IDC_FRMMAIN_LISTBOX + 3 )
    PsTextBox_SetText( hEditE, "via_enter" )
    dim mEnter as MSG
    mEnter.hwnd    = PsTextBox_GetRichEditHandle( hEditE )     ' the focused RichEdit
    mEnter.message = WM_KEYDOWN
    mEnter.wParam  = VK_RETURN
    dim as boolean okEnterClaimed = PsListTree_FilterMessage( @mEnter )
    dim as boolean okEnterText    = (PsListTree_GetText(hC,0) = "via_enter")
    dim as boolean okEnterEnded   = (PsListTree_IsEditing(hC) = false)
    dim as string  sEnterNow = PsListTree_GetText(hC,0)
    SelfTest_Check( "FilterMessage claims ENTER -> commits + ends edit", _
        okEnterClaimed andalso okEnterText andalso okEnterEnded, _
        "claimed=" & str(okEnterClaimed) & " text=" & sEnterNow )
    ' ESC: the filter must claim it, cancel (keep 'via_enter'), and end the edit.
    PsListTree_BeginEdit( hC, 0 )
    dim as HWND hEditX = GetDlgItem( hSurfC, IDC_FRMMAIN_LISTBOX + 3 )
    PsTextBox_SetText( hEditX, "should_be_discarded" )
    dim mEsc as MSG
    mEsc.hwnd    = PsTextBox_GetRichEditHandle( hEditX )
    mEsc.message = WM_KEYDOWN
    mEsc.wParam  = VK_ESCAPE
    dim as boolean okEscClaimed = PsListTree_FilterMessage( @mEsc )
    dim as boolean okEscKept    = (PsListTree_GetText(hC,0) = "via_enter")
    dim as boolean okEscEnded   = (PsListTree_IsEditing(hC) = false)
    dim as string  sEscNow = PsListTree_GetText(hC,0)
    SelfTest_Check( "FilterMessage claims ESC -> cancels (text kept) + ends edit", _
        okEscClaimed andalso okEscKept andalso okEscEnded, _
        "claimed=" & str(okEscClaimed) & " text=" & sEscNow )

    ' ---- 10h. EDITING A COLUMN OTHER THAN 0. BeginEdit has always taken a column and has
    '           always seeded and committed the right cell; what it did NOT do was PLACE the
    '           editor anywhere but over column 0, so an edit of column 2 opened a box over
    '           column 0's text. These read the editor's real window rect back and compare it
    '           against the header's real column rect -- neither side is re-derived from the
    '           placement formula.
    '
    '           Instance B is the multi-column one, so it is the only instance that can answer
    '           this; it does not enable label editing in the demo, so the test turns it on for
    '           the duration and puts it back. ----
    dim as HWND hSurfMv = GetDlgItem( HWND_FRMEXPLORER2, IDC_FRMMAIN_LISTBOX )
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, true )

    ' Precondition: the three columns are laid out and column 2 does NOT start where column 0
    ' does. Without this the placement assertion below would pass on the unfixed control.
    dim as RECT rcCol0Mv, rcCol2Mv
    dim as boolean okRect0 = PsListTree_GetColumnRect( HWND_FRMEXPLORER2, 0, rcCol0Mv )
    dim as boolean okRect2 = PsListTree_GetColumnRect( HWND_FRMEXPLORER2, 2, rcCol2Mv )
    dim as boolean okColsApart = (rcCol2Mv.left > rcCol0Mv.left)
    SelfTest_Check( "GetColumnRect: cols 0 and 2 are laid out and separated", _
        okRect0 andalso okRect2 andalso okColsApart, _
        "c0=" & SelfTest_RectStr(rcCol0Mv) & " c2=" & SelfTest_RectStr(rcCol2Mv) )

    ' It agrees with the header's own answer -- the forward is a forward, not a second formula.
    dim as RECT rcCol2Hdr
    PsColumnHeader_GetColumnRect( PsListTree_GetHeader(HWND_FRMEXPLORER2), 2, rcCol2Hdr )
    SelfTest_Check( "GetColumnRect = PsColumnHeader_GetColumnRect on the same column", _
        (rcCol2Mv.left = rcCol2Hdr.left) andalso (rcCol2Mv.right = rcCol2Hdr.right), _
        "fwd=" & SelfTest_RectStr(rcCol2Mv) & " hdr=" & SelfTest_RectStr(rcCol2Hdr) )

    ' THE FIX ITSELF: the editor lands on column 2, not column 0.
    PsListTree_SetCellText( HWND_FRMEXPLORER2, 0, 2, "cell2_before" )
    dim as boolean okBeginC2 = PsListTree_BeginEdit( HWND_FRMEXPLORER2, 0, 2 )
    dim as HWND hEditMv = GetDlgItem( hSurfMv, IDC_FRMMAIN_LISTBOX + 3 )
    dim as RECT rcEditMv = SelfTest_ChildRectInParent( hEditMv, hSurfMv )
    SelfTest_Check( "BeginEdit(col 2) places the editor ON column 2", _
        okBeginC2 andalso (rcEditMv.left = rcCol2Mv.left) andalso (rcEditMv.right = rcCol2Mv.right), _
        "edit=" & SelfTest_RectStr(rcEditMv) & " col2=" & SelfTest_RectStr(rcCol2Mv) )

    ' GetEditCol reports it while the edit is live...
    SelfTest_Check( "GetEditCol reports the column being edited", _
        (PsListTree_GetEditCol(HWND_FRMEXPLORER2) = 2), _
        "col=" & str(PsListTree_GetEditCol(HWND_FRMEXPLORER2)) )

    ' ...the editor was seeded from that CELL, not from the row caption...
    dim as string sSeed = PsTextBox_GetText( hEditMv )
    SelfTest_Check( "the editor is seeded from the column's own cell", _
        (sSeed = "cell2_before"), "seed=" & sSeed )

    ' ...and the commit writes the cell while leaving the caption alone.
    dim as string sCap0Before = PsListTree_GetText( HWND_FRMEXPLORER2, 0 )
    PsTextBox_SetText( hEditMv, "cell2_after" )
    PsListTree_EndEdit( HWND_FRMEXPLORER2, true )
    dim as boolean okCellWrote = (PsListTree_GetCellText(HWND_FRMEXPLORER2,0,2) = "cell2_after")
    dim as boolean okCapIntact = (PsListTree_GetText(HWND_FRMEXPLORER2,0) = sCap0Before)
    SelfTest_Check( "commit writes cell 2 and leaves the caption untouched", _
        okCellWrote andalso okCapIntact, _
        "cell=" & PsListTree_GetCellText(HWND_FRMEXPLORER2,0,2) & " cap=" & PsListTree_GetText(HWND_FRMEXPLORER2,0) )

    ' GetEditCol survives the teardown, which is the whole reason it is readable at all: the
    ' editor is destroyed BEFORE EndLabelEdit runs, so a host filtering on the column inside
    ' that callback would get -1 from a value cleared on teardown.
    SelfTest_Check( "GetEditCol still reports the column AFTER the edit ended", _
        (PsListTree_GetEditCol(HWND_FRMEXPLORER2) = 2), _
        "col=" & str(PsListTree_GetEditCol(HWND_FRMEXPLORER2)) )

    ' The column-0 path is unchanged: still inset past the indent/twisty band, so its left edge
    ' is NOT simply column 0's left. Asserted because the fix restructured that branch.
    PsListTree_BeginEdit( HWND_FRMEXPLORER2, 0, 0 )
    dim as HWND hEditMv0 = GetDlgItem( hSurfMv, IDC_FRMMAIN_LISTBOX + 3 )
    dim as RECT rcEditMv0 = SelfTest_ChildRectInParent( hEditMv0, hSurfMv )
    SelfTest_Check( "BeginEdit(col 0) still ends at column 0's right edge", _
        (rcEditMv0.right = rcCol0Mv.right), _
        "edit=" & SelfTest_RectStr(rcEditMv0) & " col0=" & SelfTest_RectStr(rcCol0Mv) )
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )

    ' ---- 10i. THE GESTURE COLUMN. F2, ENTER and click-to-edit used to be hard-wired to
    '           column 0, so a host whose editable column was not the caption had no way to
    '           reach it: the control consumes WM_KEYDOWN itself, so the key gestures never
    '           surface in a message callback. SetEditColumn redirects all three. ----
    SelfTest_Check( "SetEditColumn defaults to 0", _
        (PsListTree_GetEditColumn(HWND_FRMEXPLORER2) = 0), _
        "col=" & str(PsListTree_GetEditColumn(HWND_FRMEXPLORER2)) )
    PsListTree_SetEditColumn( HWND_FRMEXPLORER2, 2 )
    SelfTest_Check( "SetEditColumn round-trips", _
        (PsListTree_GetEditColumn(HWND_FRMEXPLORER2) = 2), _
        "col=" & str(PsListTree_GetEditColumn(HWND_FRMEXPLORER2)) )

    ' F2 now opens the gesture column. Driven as a real WM_KEYDOWN on the SURFACE, which is
    ' where the control handles keys -- not by calling BeginEdit, which would prove nothing.
    PsListTree_SetCurSel( HWND_FRMEXPLORER2, 0 )
    SendMessageW( hSurfMv, WM_KEYDOWN, VK_F2, 0 )
    dim as boolean okF2Editing = PsListTree_IsEditing( HWND_FRMEXPLORER2 )
    dim as integer nF2Col = PsListTree_GetEditCol( HWND_FRMEXPLORER2 )
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )
    SelfTest_Check( "F2 opens the gesture column, not column 0", _
        okF2Editing andalso (nF2Col = 2), "editing=" & str(okF2Editing) & " col=" & str(nF2Col) )

    ' ENTER is OPT-IN and off by default: with it off the key must be left entirely alone, so
    ' the host's default button still works. Asserted NEGATIVELY -- this is the assertion that
    ' goes red if someone "simplifies" ENTER into the same branch as F2.
    SendMessageW( hSurfMv, WM_KEYDOWN, VK_RETURN, 0 )
    dim as boolean okEnterOff = (PsListTree_IsEditing(HWND_FRMEXPLORER2) = false)
    SelfTest_Check( "ENTER does NOT edit while SetEnterEdits is off", okEnterOff, _
        "editing=" & str(PsListTree_IsEditing(HWND_FRMEXPLORER2)) )

    ' ...and WM_GETDLGCODE must not be claiming ENTER either, or IsDialogMessage stops handing
    ' it to the dialog even though the control does nothing with it.
    dim mDlgEnter as MSG
    mDlgEnter.hwnd    = hSurfMv
    mDlgEnter.message = WM_KEYDOWN
    mDlgEnter.wParam  = VK_RETURN
    dim as LRESULT dcOff = SendMessageW( hSurfMv, WM_GETDLGCODE, VK_RETURN, cast(LPARAM, @mDlgEnter) )
    SelfTest_Check( "WM_GETDLGCODE does NOT claim ENTER while the option is off", _
        ((dcOff and DLGC_WANTALLKEYS) = 0), "code=&h" & hex(dcOff) )

    PsListTree_SetEnterEdits( HWND_FRMEXPLORER2, true )

    ' Claimed now -- but ONLY for an ENTER keydown. An arrow must not drag WANTALLKEYS in with
    ' it, or the claim swallows Tab and breaks the host's navigation (PsToggle's rule).
    dim as LRESULT dcOn = SendMessageW( hSurfMv, WM_GETDLGCODE, VK_RETURN, cast(LPARAM, @mDlgEnter) )
    dim mDlgDown as MSG
    mDlgDown.hwnd    = hSurfMv
    mDlgDown.message = WM_KEYDOWN
    mDlgDown.wParam  = VK_DOWN
    dim as LRESULT dcDown = SendMessageW( hSurfMv, WM_GETDLGCODE, VK_DOWN, cast(LPARAM, @mDlgDown) )
    SelfTest_Check( "WM_GETDLGCODE claims ENTER per-message only", _
        ((dcOn and DLGC_WANTALLKEYS) <> 0) andalso ((dcDown and DLGC_WANTALLKEYS) = 0), _
        "enter=&h" & hex(dcOn) & " down=&h" & hex(dcDown) )

    ' ENTER opens the gesture column.
    PsListTree_SetCurSel( HWND_FRMEXPLORER2, 0 )
    SendMessageW( hSurfMv, WM_KEYDOWN, VK_RETURN, 0 )
    dim as boolean okEnterEditing = PsListTree_IsEditing( HWND_FRMEXPLORER2 )
    dim as integer nEnterCol = PsListTree_GetEditCol( HWND_FRMEXPLORER2 )
    SelfTest_Check( "ENTER opens the gesture column", _
        okEnterEditing andalso (nEnterCol = 2), _
        "editing=" & str(okEnterEditing) & " col=" & str(nEnterCol) )

    ' ...and the WM_CHAR it manufactures is swallowed, or DefWindowProc BEEPS on every press.
    ' Read through the probe, NOT through the WM_CHAR return: DefWindowProc answers WM_CHAR with
    ' 0 exactly as the swallow does, so the obvious `rChar = 0` version of this assertion passes
    ' with the swallow deleted. It was written that way first and the A/B caught it.
    dim as boolean okArmed = PsListTree_IsCharSwallowArmed( HWND_FRMEXPLORER2 )
    SendMessageW( hSurfMv, WM_CHAR, VK_RETURN, 0 )
    dim as boolean okDisarmed = (PsListTree_IsCharSwallowArmed(HWND_FRMEXPLORER2) = false)
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )
    SelfTest_Check( "ENTER arms the WM_CHAR swallow and the char consumes it", _
        okArmed andalso okDisarmed, "armed=" & str(okArmed) & " disarmed=" & str(okDisarmed) )

    PsListTree_SetEnterEdits( HWND_FRMEXPLORER2, false )   ' 10i left it ON; 10j starts clean

    ' ---- 10j. THE ROW'S DEFAULT ACTION. Double-click and ENTER are one concept, so they are
    '           one callback -- a host wiring them separately would keep two handlers in step.
    '           ENTER is claimed only because a callback is set; with none, it must reach the
    '           host's default button untouched. ----
    dim as LRESULT dcNone = SendMessageW( hSurfMv, WM_GETDLGCODE, VK_RETURN, cast(LPARAM, @mDlgEnter) )
    SelfTest_Check( "no activate callback -> ENTER is left alone", _
        ((dcNone and DLGC_WANTALLKEYS) = 0), "code=&h" & hex(dcNone) )

    PsListTree_SetActivateCallback( HWND_FRMEXPLORER2, @SelfTest_Activate )
    dim as LRESULT dcAct = SendMessageW( hSurfMv, WM_GETDLGCODE, VK_RETURN, cast(LPARAM, @mDlgEnter) )
    SelfTest_Check( "an activate callback makes the control claim ENTER", _
        ((dcAct and DLGC_WANTALLKEYS) <> 0), "code=&h" & hex(dcAct) )

    ' ENTER fires it, on the FOCUSED row, and swallows its WM_CHAR.
    gActivateCount = 0 : gActivateRow = -99
    PsListTree_SetCurSel( HWND_FRMEXPLORER2, 3 )
    SendMessageW( hSurfMv, WM_KEYDOWN, VK_RETURN, 0 )
    dim as boolean okActArmed = PsListTree_IsCharSwallowArmed( HWND_FRMEXPLORER2 )
    SendMessageW( hSurfMv, WM_CHAR, VK_RETURN, 0 )
    ' Hoisted: an andalso chain mixing relationals with a boolean local trips warning 38.
    dim as boolean okActOnce = (gActivateCount = 1)
    dim as boolean okActRow  = (gActivateRow = 3)
    SelfTest_Check( "ENTER fires activate on the focused row, char swallowed", _
        okActOnce andalso okActRow andalso okActArmed, _
        "n=" & str(gActivateCount) & " row=" & str(gActivateRow) & " armed=" & str(okActArmed) )

    ' A double-click fires the SAME callback. Driven as a real WM_LBUTTONDBLCLK at a point taken
    ' from the row's OWN painted rect (captured through the real paint path), so the
    ' visible->model mapping is exercised rather than assumed.
    PsListTree_SetPaintCallback( HWND_FRMEXPLORER2, @SelfTest_RowCapturePaint )
    if SelfTest_GrabRow( HWND_FRMEXPLORER2, 2 ) then
        dim as RECT rcRowAct = gSelfTestRowRc
        gSelfTestWantRow = -2
        gActivateCount = 0 : gActivateRow = -99
        dim as LPARAM lpAct = ((rcRowAct.top + 2) shl 16) or (rcRowAct.left + 8)
        SendMessageW( hSurfMv, WM_LBUTTONDBLCLK, 0, lpAct )
        SelfTest_Check( "double-click fires the same activate callback", _
            (gActivateCount = 1) andalso (gActivateRow = 2), _
            "n=" & str(gActivateCount) & " row=" & str(gActivateRow) & _
            " rc=" & SelfTest_RectStr(rcRowAct) )
    else
        SelfTest_Check( "double-click fires the same activate callback", false, "row 2 not painted" )
    end if
    PsListTree_SetPaintCallback( HWND_FRMEXPLORER2, @PsListTree_PaintCallback )

    ' EDIT WINS. With both wired, ENTER opens the editor and does NOT also activate -- asserted
    ' because "fire both" is the natural thing for a later edit to do.
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, true )
    PsListTree_SetEnterEdits( HWND_FRMEXPLORER2, true )
    gActivateCount = 0
    PsListTree_SetCurSel( HWND_FRMEXPLORER2, 1 )
    SendMessageW( hSurfMv, WM_KEYDOWN, VK_RETURN, 0 )
    dim as boolean okEditWon = PsListTree_IsEditing( HWND_FRMEXPLORER2 )
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )
    SelfTest_Check( "SetEnterEdits wins over activate: ENTER edits, does not activate", _
        okEditWon andalso (gActivateCount = 0), _
        "editing=" & str(okEditWon) & " activates=" & str(gActivateCount) )

    PsListTree_SetActivateCallback( HWND_FRMEXPLORER2, 0 )
    PsListTree_SetEnterEdits( HWND_FRMEXPLORER2, false )
    PsListTree_SetEditColumn( HWND_FRMEXPLORER2, 0 )
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, false )

    ' ---- 10k. THE CONTAINER FORWARDS FOCUS. Create returns the CONTAINER, which is the only
    '           handle a host has -- but every key is handled in the surface WndProc, so focus
    '           parked on the container is focus nowhere: arrows, F2 and ENTER all go missing
    '           and the focus bar never appears. A host doing the obvious SetFocus on the
    '           handle it was given landed exactly there. ----
    SetFocus( GetDesktopWindow() )                       ' park it somewhere else first
    SetFocus( HWND_FRMEXPLORER2 )                        ' the CONTAINER, as a host would
    dim as HWND hFocusNow = GetFocus()
    SelfTest_Check( "SetFocus on the container lands on the surface, not the container", _
        (hFocusNow = hSurfMv) andalso (hFocusNow <> HWND_FRMEXPLORER2), _
        "focus=" & hex(cast(integer, hFocusNow)) & " surface=" & hex(cast(integer, hSurfMv)) & _
        " container=" & hex(cast(integer, HWND_FRMEXPLORER2)) )

    ' ---- 10l. THE EDITOR'S COLOURS AND ITS CARET. The editor is created LAZILY INSIDE the
    '           control, so a host has no handle to theme and no moment to do it in -- the
    '           BeginLabelEdit veto runs before the create. Un-themed it renders in PsTextBox's
    '           defaults, black on white, inside whatever the list looks like. ----
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, true )
    PsListTree_SetEditColors( HWND_FRMEXPLORER2, &hFF2010, &h10FF20, &h2010FF, &hFFFFFF )
    PsListTree_SetCellText( HWND_FRMEXPLORER2, 0, 2, "abcdef" )
    PsListTree_BeginEdit( HWND_FRMEXPLORER2, 0, 2 )
    dim as HWND hEdThm = GetDlgItem( hSurfMv, IDC_FRMMAIN_LISTBOX + 3 )
    dim as COLORREF cBack = PsTextBox_GetBackColor( hEdThm )
    dim as COLORREF cFore = PsTextBox_GetForeColor( hEdThm )
    SelfTest_Check( "SetEditColors reaches the lazily-created editor", _
        (cBack = &hFF2010) andalso (cFore = &h10FF20), _
        "back=&h" & hex(cBack) & " fore=&h" & hex(cFore) )

    ' Caret at the END, nothing selected -- the whole point of SetEditSelectAll(false), and the
    ' selection is read back off the REAL RichEdit rather than from a flag we just set.
    dim as integer selS, selE
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )
    PsListTree_SetEditSelectAll( HWND_FRMEXPLORER2, false )
    PsListTree_BeginEdit( HWND_FRMEXPLORER2, 0, 2 )
    dim as HWND hEdCaret = GetDlgItem( hSurfMv, IDC_FRMMAIN_LISTBOX + 3 )
    SendMessageW( PsTextBox_GetRichEditHandle(hEdCaret), EM_GETSEL, cast(WPARAM,@selS), cast(LPARAM,@selE) )
    SelfTest_Check( "SetEditSelectAll(false) leaves the caret at the end, nothing selected", _
        (selS = 6) andalso (selE = 6), "sel=" & str(selS) & ".." & str(selE) & " len=6" )
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )

    ' ...and TRUE (the default) still selects the lot, so the change is opt-in.
    PsListTree_SetEditSelectAll( HWND_FRMEXPLORER2, true )
    PsListTree_BeginEdit( HWND_FRMEXPLORER2, 0, 2 )
    dim as HWND hEdSel = GetDlgItem( hSurfMv, IDC_FRMMAIN_LISTBOX + 3 )
    SendMessageW( PsTextBox_GetRichEditHandle(hEdSel), EM_GETSEL, cast(WPARAM,@selS), cast(LPARAM,@selE) )
    SelfTest_Check( "SetEditSelectAll(true) still selects the whole text", _
        (selS = 0) andalso (selE = 6), "sel=" & str(selS) & ".." & str(selE) )
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, false )

    ' ---- 10m. THE EDITOR'S TEXT LINES UP WITH THE TEXT IT REPLACES. The box covers the whole
    '           cell, but PsTextBox puts its RichEdit borderWidth inside the box and applies its
    '           margins on top of that -- so an editor opened over a cell whose text the HOST
    '           painted at some inset of its own started its text further left, and the row
    '           visibly jumped sideways the moment editing began.
    '
    '           Measured with EM_POSFROMCHAR on the REAL RichEdit and mapped into the surface's
    '           coordinates, which is the space the column rect is in -- not re-derived from the
    '           borderWidth + margin arithmetic under test. ----
    ' THE FIRST EDIT IS ITS OWN CASE. The editor is created lazily and kept for the
    '           instance's life, so every assertion after the first one runs against an editor
    '           that already exists and is already margined. Toggling label editing off destroys
    '           it (EnableLabelEdit(false) ends any edit), which is the only way back to the
    '           create path a user hits on their FIRST double-click.
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, false )     ' destroys the editor
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, true )
    PsListTree_SetCellText( HWND_FRMEXPLORER2, 0, 2, "abcdef" )
    dim as integer nInsetPx = 6
    PsListTree_SetEditTextInset( HWND_FRMEXPLORER2, nInsetPx )
    PsListTree_BeginEdit( HWND_FRMEXPLORER2, 0, 2 )
    dim as HWND hEdIns = GetDlgItem( hSurfMv, IDC_FRMMAIN_LISTBOX + 3 )
    dim as HWND hReIns = PsTextBox_GetRichEditHandle( hEdIns )
    dim as POINTL ptChar
    SendMessageW( hReIns, EM_POSFROMCHAR, cast(WPARAM, @ptChar), 0 )
    dim as POINT ptSurf
    ptSurf.x = ptChar.x : ptSurf.y = ptChar.y
    MapWindowPoints( hReIns, hSurfMv, cast(LPPOINT, @ptSurf), 1 )
    dim as RECT rcColIns
    PsListTree_GetColumnRect( HWND_FRMEXPLORER2, 2, rcColIns )
    dim as integer nGot = ptSurf.x - rcColIns.left
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )
    SelfTest_Check( "the editor's first character sits at the requested inset", _
        (nGot = nInsetPx), "want=" & str(nInsetPx) & " got=" & str(nGot) & _
        "  charX=" & str(ptSurf.x) & " colLeft=" & str(rcColIns.left) )

    ' Unset (-1, the default) must leave PsTextBox's margins alone -- the inset is opt-in, and a
    ' host that never calls it keeps whatever the editor did before.
    PsListTree_SetEditTextInset( HWND_FRMEXPLORER2, -1 )
    PsListTree_BeginEdit( HWND_FRMEXPLORER2, 0, 2 )
    dim as HWND hEdNo = GetDlgItem( hSurfMv, IDC_FRMMAIN_LISTBOX + 3 )
    SendMessageW( PsTextBox_GetRichEditHandle(hEdNo), EM_POSFROMCHAR, cast(WPARAM, @ptChar), 0 )
    ptSurf.x = ptChar.x : ptSurf.y = ptChar.y
    MapWindowPoints( PsTextBox_GetRichEditHandle(hEdNo), hSurfMv, cast(LPPOINT, @ptSurf), 1 )
    dim as integer nGotNo = ptSurf.x - rcColIns.left
    PsListTree_EndEdit( HWND_FRMEXPLORER2, false )
    SelfTest_Check( "inset -1 leaves the editor where it was (opt-in)", _
        (nGotNo < nInsetPx), "unset=" & str(nGotNo) & " inset=" & str(nInsetPx) )
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER2, false )

    ' ---- 10n. THE SURFACE MUST CLIP ITS CHILDREN. It paints every visible row in ONE pass
    '           over its whole client, and the in-place editor is a CHILD of it -- so without
    '           WS_CLIPCHILDREN the row painting goes over the editor and blanks the text being
    '           typed. Z-order is irrelevant: a parent painting without it covers its children
    '           whatever the order, which is why raising the editor did not help.
    '
    '           This asserts the STYLE BIT, which is the mechanism itself. It cannot assert the
    '           pixels: proving "the editor survived a repaint" needs a forced repaint, and a
    '           forced repaint is exactly what made the text come back in the bug report.
    dim as LONG_PTR stySurf = GetWindowLongPtrW( hSurfMv, GWL_STYLE )
    SelfTest_Check( "the row surface clips its children", _
        ((stySurf and WS_CLIPCHILDREN) <> 0), "style=&h" & hex(stySurf) )

    ' ---- 10o. THE TEXT SETTERS REPAINT. SetText and SetCellText were the only model mutators
    '           that did not call NotifyChange, so writing a cell changed what the row SAYS and
    '           left the old text on screen until an unrelated repaint came along. A host cannot
    '           work around it by redrawing the control handle either -- that is the CONTAINER,
    '           and the rows are painted by its surface child.
    '
    '           Observed through the surface's UPDATE REGION, which is what Refresh actually
    '           produces (InvalidateRect on the surface). Validate first so the region seen
    '           afterwards can only have come from the call under test.
    ValidateRect( hSurfMv, NULL )
    dim as RECT rcUpd
    dim as boolean hadUpdBefore = (GetUpdateRect( hSurfMv, @rcUpd, FALSE ) <> 0)
    PsListTree_SetCellText( HWND_FRMEXPLORER2, 1, 2, "notify_probe" )
    dim as boolean okCellInval = (GetUpdateRect( hSurfMv, @rcUpd, FALSE ) <> 0)
    SelfTest_Check( "SetCellText invalidates the surface", _
        okCellInval andalso (hadUpdBefore = false), _
        "before=" & str(hadUpdBefore) & " after=" & str(okCellInval) )

    ValidateRect( hSurfMv, NULL )
    PsListTree_SetText( HWND_FRMEXPLORER2, 1, "notify_probe_caption" )
    SelfTest_Check( "SetText invalidates the surface", _
        (GetUpdateRect( hSurfMv, @rcUpd, FALSE ) <> 0), "" )

    ' ...and a BeginUpdate batch still coalesces, so a bulk load costs exactly one Refresh at
    ' the end rather than one per cell. This is the half that makes the change cheap.
    ValidateRect( hSurfMv, NULL )
    PsListTree_BeginUpdate( HWND_FRMEXPLORER2 )
    for i as integer = 0 to 5
        PsListTree_SetCellText( HWND_FRMEXPLORER2, 1, 2, "batch" & str(i) )
    next
    dim as boolean okQuietInBatch = (GetUpdateRect( hSurfMv, @rcUpd, FALSE ) = 0)
    PsListTree_EndUpdate( HWND_FRMEXPLORER2 )
    dim as boolean okLoudAtEnd = (GetUpdateRect( hSurfMv, @rcUpd, FALSE ) <> 0)
    SelfTest_Check( "inside BeginUpdate the setters stay quiet; EndUpdate repaints once", _
        okQuietInBatch andalso okLoudAtEnd, _
        "duringBatch=" & str(okQuietInBatch) & " atEnd=" & str(okLoudAtEnd) )

    ' ---- 11. Border chrome ------------------------------------------------------------
    '  The border is drawn by the CONTAINER and its whole mechanism is that PositionWindows
    '  insets every child by the width. So the assertable claim is not "a frame was painted"
    '  but "a band of exactly nBorderWidth pixels around the container's client is covered by
    '  no child" -- if that band is clear, the container's own WM_PAINT owns it; if it is not,
    '  a child paints over the frame and no amount of drawing code helps.
    '
    '  Every rect below is read back from the LIVE windows, never re-derived from the formula
    '  under test.
    dim as HWND hBd     = HWND_FRMEXPLORER            ' long list -> scrollbar is shown
    dim as HWND hBdSurf = GetDlgItem( hBd, IDC_FRMMAIN_LISTBOX )
    dim as HWND hBdSB   = PsListTree_GetScrollBar( hBd )
    dim as RECT rcBdCli : GetClientRect( hBd, @rcBdCli )

    ' 11a. OFF by default -- the claim that every pre-existing host is untouched.
    SelfTest_Check( "border defaults to 0 (no chrome, no reserved space)", _
        PsListTree_GetBorderWidth( hBd ) = 0, "w=" & str(PsListTree_GetBorderWidth(hBd)) )

    dim as RECT rcSurf0 = SelfTest_ChildRectInParent( hBdSurf, hBd )
    dim as RECT rcSB0   = SelfTest_ChildRectInParent( hBdSB, hBd )
    dim as boolean bSBShown = (IsWindowVisible( hBdSB ) <> 0)
    SelfTest_Check( "precondition: this instance's scrollbar IS shown", bSBShown, _
        "sb=" & SelfTest_RectStr( rcSB0 ) )
    ' At width 0 the children reach all four edges: surface at the origin, and between the
    ' surface and the scrollbar the full client width and height is consumed.
    SelfTest_Check( "width 0: children reach every edge of the client", _
        (rcSurf0.left = 0) andalso (rcSurf0.top = 0) andalso _
        (rcSurf0.bottom = rcBdCli.bottom) andalso (rcSB0.right = rcBdCli.right), _
        "cli=" & SelfTest_RectStr( rcBdCli ) & " surf=" & SelfTest_RectStr( rcSurf0 ) )

    ' 11b. Width 1 -- the value tiko uses to match a PsTextBox. Assert the CLEAR BAND on all
    '      four sides, taking the outermost edge any child reaches.
    #macro BORDERBAND( bw, tag )
        scope
            PsListTree_SetBorderWidth( hBd, (bw) )
            dim as RECT rs = SelfTest_ChildRectInParent( hBdSurf, hBd )
            dim as RECT rb = SelfTest_ChildRectInParent( hBdSB, hBd )
            dim as long lMin = iif( rs.left   < rb.left,   rs.left,   rb.left )
            dim as long tMin = iif( rs.top    < rb.top,    rs.top,    rb.top )
            dim as long rMax = iif( rs.right  > rb.right,  rs.right,  rb.right )
            dim as long bMax = iif( rs.bottom > rb.bottom, rs.bottom, rb.bottom )
            SelfTest_Check( "width " & (tag) & ": clear band of " & (tag) & "px on all four sides", _
                (lMin = (bw)) andalso (tMin = (bw)) andalso _
                (rMax = rcBdCli.right - (bw)) andalso (bMax = rcBdCli.bottom - (bw)), _
                "band l=" & str(lMin) & " t=" & str(tMin) & _
                " r=" & str(rcBdCli.right - rMax) & " b=" & str(rcBdCli.bottom - bMax) )
        end scope
    #endmacro
    BORDERBAND( 1, "1" )

    ' 11c. Width 3, so nothing above can be passing on a hardcoded 1 -- and so the "raw
    '      pixels, never DPI-scaled" contract is actually exercised. That claim is only
    '      DISCRIMINATING above 100% DPI, where ScaleX(3) <> 3; the detail reports the scaled
    '      value so a reader can see whether this run could tell the difference.
    dim as CWindow ptr pBdWin = AfxCWindowPtr( hBd )
    dim as long nScaled3 = 3
    if pBdWin then nScaled3 = pBdWin->ScaleX( 3 )
    BORDERBAND( 3, "3" )
    SelfTest_Check( "border width is RAW pixels, not DPI-scaled", _
        SelfTest_ChildRectInParent( hBdSurf, hBd ).left = 3, _
        "inset=" & str( SelfTest_ChildRectInParent( hBdSurf, hBd ).left ) & _
        " ScaleX(3)=" & str(nScaled3) & _
        iif( nScaled3 = 3, "  (100% DPI -- NOT discriminating)", "  (discriminating)" ) )

    ' 11d. A negative width is clamped, not honoured (it would inset the children outward).
    PsListTree_SetBorderWidth( hBd, -5 )
    SelfTest_Check( "negative width clamps to 0", PsListTree_GetBorderWidth( hBd ) = 0, _
        "w=" & str(PsListTree_GetBorderWidth(hBd)) )

    ' 11e. Round trip: back at 0 the geometry must be EXACTLY what it was before any of this,
    '      which is what makes the feature safe to leave switched off in every other host.
    dim as RECT rcSurfZ = SelfTest_ChildRectInParent( hBdSurf, hBd )
    dim as RECT rcSBZ   = SelfTest_ChildRectInParent( hBdSB, hBd )
    SelfTest_Check( "0 -> 3 -> 0 restores the original child geometry exactly", _
        (rcSurfZ.left = rcSurf0.left) andalso (rcSurfZ.top = rcSurf0.top) andalso _
        (rcSurfZ.right = rcSurf0.right) andalso (rcSurfZ.bottom = rcSurf0.bottom) andalso _
        (rcSBZ.right = rcSB0.right) andalso (rcSBZ.left = rcSB0.left), _
        "surf " & SelfTest_RectStr( rcSurf0 ) & " -> " & SelfTest_RectStr( rcSurfZ ) )

    ' 11f. Colour round-trips (it is the half of the contract the geometry cannot see).
    dim as COLORREF clrWas = PsListTree_GetBorderColor( hBd )
    PsListTree_SetBorderColor( hBd, &h00123456 )
    SelfTest_Check( "border colour round-trips", _
        PsListTree_GetBorderColor( hBd ) = &h00123456, _
        "got=&h" & hex( PsListTree_GetBorderColor( hBd ) ) )
    PsListTree_SetBorderColor( hBd, clrWas )

    ' 11f-2. INK. Everything above proves the band is RESERVED; none of it would notice the
    '        frame never being drawn into it. Drive the container's real render offscreen and
    '        read the pixels: one inside the band, one just inside it. Distinct, deliberately
    '        unlikely colours, so nothing can pass by coinciding with the theme.
    const as COLORREF CLR_AB_BORDER = &h00204080
    const as COLORREF CLR_AB_BACK   = &h00E0C0A0
    dim as COLORREF clrBackWas = PsListTree_GetBackColor( hBd )
    PsListTree_SetBorderColor( hBd, CLR_AB_BORDER )
    PsListTree_SetBackColor( hBd, CLR_AB_BACK )
    PsListTree_SetBorderWidth( hBd, 4 )              ' >1, so a point can sit strictly inside the band
    dim as long pxTop   = PsListTree_ProbeRenderedPixel( hBd, rcBdCli.right \ 2, 1 )
    dim as long pxLeft  = PsListTree_ProbeRenderedPixel( hBd, 1, rcBdCli.bottom \ 2 )
    dim as long pxRight = PsListTree_ProbeRenderedPixel( hBd, rcBdCli.right - 2, rcBdCli.bottom \ 2 )
    dim as long pxBot   = PsListTree_ProbeRenderedPixel( hBd, rcBdCli.right \ 2, rcBdCli.bottom - 2 )
    dim as long pxIn    = PsListTree_ProbeRenderedPixel( hBd, rcBdCli.right \ 2, rcBdCli.bottom \ 2 )
    SelfTest_Check( "border INK: all four sides render the border colour", _
        (pxTop = CLR_AB_BORDER) andalso (pxLeft = CLR_AB_BORDER) andalso _
        (pxRight = CLR_AB_BORDER) andalso (pxBot = CLR_AB_BORDER), _
        "want=&h" & hex(CLR_AB_BORDER) & " t=&h" & hex(pxTop) & " l=&h" & hex(pxLeft) & _
        " r=&h" & hex(pxRight) & " b=&h" & hex(pxBot) )
    ' The complement: the frame must NOT have flooded the interior. This is the assertion the
    ' family's four PaintBorderRect accidents would have failed.
    SelfTest_Check( "border INK: the interior is NOT flooded with the border colour", _
        (pxIn = CLR_AB_BACK), _
        "want=&h" & hex(CLR_AB_BACK) & " got=&h" & hex(pxIn) )
    ' And with the border off, the same points must all be plain background -- so the two
    ' assertions above cannot be passing on something that is there whatever the width says.
    PsListTree_SetBorderWidth( hBd, 0 )
    dim as long pxOffT = PsListTree_ProbeRenderedPixel( hBd, rcBdCli.right \ 2, 1 )
    dim as long pxOffL = PsListTree_ProbeRenderedPixel( hBd, 1, rcBdCli.bottom \ 2 )
    SelfTest_Check( "border INK: width 0 leaves no frame at all", _
        (pxOffT = CLR_AB_BACK) andalso (pxOffL = CLR_AB_BACK), _
        "t=&h" & hex(pxOffT) & " l=&h" & hex(pxOffL) )
    PsListTree_SetBackColor( hBd, clrBackWas )

    ' 11g. The HEADER band is inset too. It is the one child that spans the full container
    '      width (over the scrollbar strip, listview-style), so it is the child most likely to
    '      be left painting across the frame -- assert it on the instance that actually has one.
    dim as HWND hBdH = HWND_FRMEXPLORER2
    dim as RECT rcBdHCli : GetClientRect( hBdH, @rcBdHCli )
    dim as RECT rcHdr0 = SelfTest_ChildRectInParent( HWND_COLHDR, hBdH )
    PsListTree_SetBorderWidth( hBdH, 2 )
    dim as RECT rcHdr2 = SelfTest_ChildRectInParent( HWND_COLHDR, hBdH )
    SelfTest_Check( "header band is inset by the border on left/top/right", _
        (rcHdr2.left = 2) andalso (rcHdr2.top = 2) andalso (rcHdr2.right = rcBdHCli.right - 2), _
        "0px=" & SelfTest_RectStr( rcHdr0 ) & " 2px=" & SelfTest_RectStr( rcHdr2 ) )
    PsListTree_SetBorderWidth( hBdH, 0 )


    ' ====================================================================================
    ' TOOLTIP BACKEND -- mode, handles, the delays surviving a switch, and the text wiring
    '
    ' A real dwell is what would exercise this end to end and no self-test can produce one,
    ' so what is asserted is the WIRING. The text assertion in particular resolves through
    ' the callback the tip actually holds rather than by calling our own thunk: on the pilot
    ' the direct-call version stayed green with the wiring deleted, which would have shipped
    ' a control that shows no tooltip at all once switched.
    ' ====================================================================================
    scope
        dim as HWND hTipL = PsListTree_Create( HWND_FRMMAIN, 9807 )
        if hTipL = 0 then
            SelfTest_Check( "tooltip: could not create a test control", false, "Create returned 0" )
        else
            SelfTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsListTree_GetTooltipMode( hTipL ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsListTree_GetTooltipMode( hTipL )) )
            SelfTest_Check( "tooltip: a comctl32 window exists, and no PsTooltip does", _
                (PsListTree_GetTooltipHandle( hTipL ) <> 0) andalso _
                (IsWindow(PsListTree_GetTooltipHandle( hTipL )) <> 0) andalso _
                (PsListTree_GetPsTooltipHandle( hTipL ) = 0), _
                "hSys = " & str(cast(integer, PsListTree_GetTooltipHandle( hTipL ))) )

            PsListTree_SetHoverTime( hTipL, 777 )
            SelfTest_Check( "tooltip: SetHoverTime reaches the comctl32 tip", _
                (SendMessageW( PsListTree_GetTooltipHandle( hTipL ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "TTM_GETDELAYTIME = " & str(SendMessageW( PsListTree_GetTooltipHandle( hTipL ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )

            dim as HWND hSysBeforeL = PsListTree_GetTooltipHandle( hTipL )
            ' The switch is made on its own line, NOT inside the assertion call. fbc does not
            ' evaluate call arguments left to right, so a detail string that reads the mode in
            ' the same call reports the mode from BEFORE the switch -- a green assertion with a
            ' trace saying it failed, which is worse than either.
            dim as boolean bSwitchedL = PsListTree_SetTooltipMode( hTipL, PSTIP_MODE_PS )
            SelfTest_Check( "tooltip: switching to PsTooltip succeeds", bSwitchedL, _
                "mode = " & str(PsListTree_GetTooltipMode( hTipL )) )
            SelfTest_Check( "tooltip: the outgoing comctl32 window is DESTROYED, not orphaned", _
                (IsWindow(hSysBeforeL) = 0) andalso (PsListTree_GetTooltipHandle( hTipL ) = 0), _
                "old hSys alive = " & str(IsWindow(hSysBeforeL)) )

            dim as HWND hPsL = PsListTree_GetPsTooltipHandle( hTipL )
            ' ATTACHED TO THE SURFACE, NOT THE CONTAINER -- unlike every other control in the
            ' family, and deliberately: mouse messages go to the surface child, so the system
            ' backend's TTF_SUBCLASS tool has to live there or it never sees the cursor. The
            ' assertion is written to the surface for that reason; simply comparing against
            ' the container would be asserting the bug.
            SelfTest_Check( "tooltip: the PsTooltip is attached to the SURFACE, not the container", _
                (hPsL <> 0) andalso (IsWindow(hPsL) <> 0) andalso _
                (GetParent( PsTooltip_GetAttached( hPsL ) ) = hTipL), _
                "attached = " & str(cast(integer, PsTooltip_GetAttached( hPsL ))) & _
                " parent = " & str(cast(integer, GetParent( PsTooltip_GetAttached( hPsL ) ))) & _
                " control = " & str(cast(integer, hTipL)) )

            dim as PSTOOLTIP ptr pTipL = GetTooltipPointer( hPsL )
            SelfTest_Check( "tooltip: the hover time set on the OLD backend is honoured by the new one", _
                (pTipL <> 0) andalso (pTipL->nInitialDelay = 777), _
                "PsTooltip initial delay = " & str(iif(pTipL, pTipL->nInitialDelay, -1)) & " ms" )
            SelfTest_Check( "tooltip: the PsTooltip backend is WIRED to the control's text policy", _
                (pTipL <> 0) andalso (pTipL->TooltipCallback <> 0), _
                "callback installed = " & str(iif(pTipL andalso pTipL->TooltipCallback, 1, 0)) )

            PsListTree_SetTooltipMode( hTipL, PSTIP_MODE_SYSTEM )
            SelfTest_Check( "tooltip: switching back rebuilds the comctl32 tip, hover time and all", _
                (PsListTree_GetTooltipHandle( hTipL ) <> 0) andalso _
                (PsListTree_GetPsTooltipHandle( hTipL ) = 0) andalso _
                (SendMessageW( PsListTree_GetTooltipHandle( hTipL ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "initial = " & str(SendMessageW( PsListTree_GetTooltipHandle( hTipL ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )
            SelfTest_Check( "tooltip: an unset delay is left at the backend's own default", _
                (SendMessageW( PsListTree_GetTooltipHandle( hTipL ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 ) > 0), _
                "autopop = " & str(SendMessageW( PsListTree_GetTooltipHandle( hTipL ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 )) & " ms" )

            DestroyWindow( hTipL )
        end if
    end scope


    ' ====================================================================================
    ' TOOLTIP BACKEND -- mode, handles, the delays surviving a switch, and the text wiring
    '
    ' A real dwell is what would exercise this end to end and no self-test can produce one,
    ' so what is asserted is the WIRING. The text assertion in particular resolves through
    ' the callback the tip actually holds rather than by calling our own thunk: on the pilot
    ' the direct-call version stayed green with the wiring deleted, which would have shipped
    ' a control that shows no tooltip at all once switched.
    ' ====================================================================================
    scope
        dim as HWND hTipH = PsColumnHeader_Create( HWND_FRMMAIN, 9808 )
        if hTipH = 0 then
            SelfTest_Check( "tooltip: could not create a test control", false, "Create returned 0" )
        else
            SelfTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsColumnHeader_GetTooltipMode( hTipH ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsColumnHeader_GetTooltipMode( hTipH )) )
            SelfTest_Check( "tooltip: a comctl32 window exists, and no PsTooltip does", _
                (PsColumnHeader_GetTooltipHandle( hTipH ) <> 0) andalso _
                (IsWindow(PsColumnHeader_GetTooltipHandle( hTipH )) <> 0) andalso _
                (PsColumnHeader_GetPsTooltipHandle( hTipH ) = 0), _
                "hSys = " & str(cast(integer, PsColumnHeader_GetTooltipHandle( hTipH ))) )

            PsColumnHeader_SetHoverTime( hTipH, 777 )
            SelfTest_Check( "tooltip: SetHoverTime reaches the comctl32 tip", _
                (SendMessageW( PsColumnHeader_GetTooltipHandle( hTipH ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "TTM_GETDELAYTIME = " & str(SendMessageW( PsColumnHeader_GetTooltipHandle( hTipH ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )

            dim as HWND hSysBeforeH = PsColumnHeader_GetTooltipHandle( hTipH )
            ' The switch is made on its own line, NOT inside the assertion call. fbc does not
            ' evaluate call arguments left to right, so a detail string that reads the mode in
            ' the same call reports the mode from BEFORE the switch -- a green assertion with a
            ' trace saying it failed, which is worse than either.
            dim as boolean bSwitchedH = PsColumnHeader_SetTooltipMode( hTipH, PSTIP_MODE_PS )
            SelfTest_Check( "tooltip: switching to PsTooltip succeeds", bSwitchedH, _
                "mode = " & str(PsColumnHeader_GetTooltipMode( hTipH )) )
            SelfTest_Check( "tooltip: the outgoing comctl32 window is DESTROYED, not orphaned", _
                (IsWindow(hSysBeforeH) = 0) andalso (PsColumnHeader_GetTooltipHandle( hTipH ) = 0), _
                "old hSys alive = " & str(IsWindow(hSysBeforeH)) )

            dim as HWND hPsH = PsColumnHeader_GetPsTooltipHandle( hTipH )
            SelfTest_Check( "tooltip: a PsTooltip window exists and is attached to the control", _
                (hPsH <> 0) andalso (IsWindow(hPsH) <> 0) andalso _
                (PsTooltip_GetAttached( hPsH ) = hTipH), _
                "hPs = " & str(cast(integer, hPsH)) )

            dim as PSTOOLTIP ptr pTipH = GetTooltipPointer( hPsH )
            SelfTest_Check( "tooltip: the hover time set on the OLD backend is honoured by the new one", _
                (pTipH <> 0) andalso (pTipH->nInitialDelay = 777), _
                "PsTooltip initial delay = " & str(iif(pTipH, pTipH->nInitialDelay, -1)) & " ms" )
            SelfTest_Check( "tooltip: the PsTooltip backend is WIRED to the control's text policy", _
                (pTipH <> 0) andalso (pTipH->TooltipCallback <> 0), _
                "callback installed = " & str(iif(pTipH andalso pTipH->TooltipCallback, 1, 0)) )

            PsColumnHeader_SetTooltipMode( hTipH, PSTIP_MODE_SYSTEM )
            SelfTest_Check( "tooltip: switching back rebuilds the comctl32 tip, hover time and all", _
                (PsColumnHeader_GetTooltipHandle( hTipH ) <> 0) andalso _
                (PsColumnHeader_GetPsTooltipHandle( hTipH ) = 0) andalso _
                (SendMessageW( PsColumnHeader_GetTooltipHandle( hTipH ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "initial = " & str(SendMessageW( PsColumnHeader_GetTooltipHandle( hTipH ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )
            SelfTest_Check( "tooltip: an unset delay is left at the backend's own default", _
                (SendMessageW( PsColumnHeader_GetTooltipHandle( hTipH ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 ) > 0), _
                "autopop = " & str(SendMessageW( PsColumnHeader_GetTooltipHandle( hTipH ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 )) & " ms" )

            DestroyWindow( hTipH )
        end if
    end scope

    ' ---- WM_MOUSEMOVE reports the MODEL ROW UNDER THE CURSOR ----------------------------
    ' PSLISTTREE_MESSAGEINFO.idx is documented as "the MODEL row index under the mouse
    ' (-1 if none)", and every other mouse message set it -- but WM_MOUSEMOVE did not. The
    ' struct is initialised with .idx = -1, so a host was told "no row" for the whole surface
    ' on every move. Nothing faults: -1 is the documented no-row answer, so a host reads it as
    ' a legitimate miss and simply never acts.
    '
    ' Driven with SendMessage rather than a real cursor. That is legitimate here in a way it
    ' would not be for a click: the claim is only that a field is populated from the message's
    ' own lParam, and no capture, focus or timing is involved.
    '
    ' TWO of the three below are sensitive to the regression, not all three -- measured, not
    ' assumed. The old code answered -1 everywhere, so the off-surface probe passed under it
    ' too and is here to pin the documented no-row answer rather than to catch this bug.
    scope
        dim as HWND hMove = HWND_FRMEXPLORER
        dim as HWND hSurfM = GetDlgItem( hMove, IDC_FRMMAIN_LISTBOX )
        if (hMove <> 0) andalso (hSurfM <> 0) then
            dim as integer rh = PsListTree_GetRowHeight( hMove )
            dim as RECT rcM : GetClientRect( hSurfM, @rcM )

            PsListTree_SetMessageCallback( hMove, @SelfTest_MoveRecorder )

            gMoveIdxSeen = -99
            SendMessageW( hSurfM, WM_MOUSEMOVE, 0, MAKELPARAM( 8, rh \ 2 ) )
            dim as integer idxRow0 = gMoveIdxSeen

            gMoveIdxSeen = -99
            SendMessageW( hSurfM, WM_MOUSEMOVE, 0, MAKELPARAM( 8, (rh * 2) + (rh \ 2) ) )
            dim as integer idxRow2 = gMoveIdxSeen

            ' Off the surface entirely: the documented answer is -1, and the old code said 0.
            ' BELOW the client rect rather than merely below the last row -- this demo's list
            ' is full, so the bottom pixel of the client is still over a real row (measured:
            ' it reported 17). A move can legitimately carry coordinates outside the client,
            ' which is what makes this a fair probe rather than a contrived one.
            gMoveIdxSeen = -99
            SendMessageW( hSurfM, WM_MOUSEMOVE, 0, MAKELPARAM( 8, rcM.bottom + 4 ) )
            dim as integer idxNone = gMoveIdxSeen

            PsListTree_SetMessageCallback( hMove, 0 )

            SelfTest_Check( "WM_MOUSEMOVE reports a real row under the cursor", _
                (idxRow0 >= 0), "idx=" & str(idxRow0) )
            SelfTest_Check( "WM_MOUSEMOVE reports DIFFERENT rows at different heights", _
                (idxRow0 <> idxRow2), "row0=" & str(idxRow0) & " row2=" & str(idxRow2) )
            SelfTest_Check( "WM_MOUSEMOVE reports -1 past the last row", _
                (idxNone = -1), "idx=" & str(idxNone) )
        end if
    end scope

    ' ---- InvalidateRow / InvalidateRows -------------------------------------------------
    ' The claim is about the UPDATE REGION, so that is what is read: GetUpdateRect after the
    ' call, against the row rects captured from the REAL paint path. Nothing here re-derives
    ' a row's position from RowHeight -- that would assert the arithmetic against itself.
    '
    ' ValidateRect before each probe because the self-test never pumps messages, so every
    ' invalidate in this function accumulates into one region until something clears it.
    scope
        dim as HWND hInv  = HWND_FRMEXPLORER
        dim as HWND hSurf = GetDlgItem( hInv, IDC_FRMMAIN_LISTBOX )
        if (hInv <> 0) andalso (hSurf <> 0) then

            PsListTree_SetPaintCallback( hInv, @SelfTest_RowCapturePaint )

            dim as boolean bGot1 = SelfTest_GrabRow( hInv, 1 )
            dim as RECT rcRow1 = gSelfTestRowRc
            dim as boolean bGot3 = SelfTest_GrabRow( hInv, 3 )
            dim as RECT rcRow3 = gSelfTestRowRc

            dim as RECT rcU

            if (bGot1 = false) orelse (bGot3 = false) then
                SelfTest_Check( "InvalidateRow: could not capture the probe rows", false, _
                    "row1=" & str(bGot1) & " row3=" & str(bGot3) )
            else
                ' One row: exactly that row's band, full client width.
                ValidateRect( hSurf, NULL )
                PsListTree_InvalidateRow( hInv, 1 )
                dim as long gotOne = GetUpdateRect( hSurf, @rcU, FALSE )
                SelfTest_Check( "InvalidateRow marks EXACTLY that row's band", _
                    (gotOne <> 0) andalso (rcU.top = rcRow1.top) andalso (rcU.bottom = rcRow1.bottom) _
                    andalso (rcU.left = rcRow1.left) andalso (rcU.right = rcRow1.right), _
                    "update=" & SelfTest_RectStr( rcU ) & " row=" & SelfTest_RectStr( rcRow1 ) )

                ' A span: from the first row's top to the last row's bottom, one region.
                ValidateRect( hSurf, NULL )
                PsListTree_InvalidateRows( hInv, 1, 3 )
                dim as long gotSpan = GetUpdateRect( hSurf, @rcU, FALSE )
                SelfTest_Check( "InvalidateRows spans first.top to last.bottom", _
                    (gotSpan <> 0) andalso (rcU.top = rcRow1.top) andalso (rcU.bottom = rcRow3.bottom), _
                    "update=" & SelfTest_RectStr( rcU ) & " expected top=" & str(rcRow1.top) & _
                    " bottom=" & str(rcRow3.bottom) )

                ' Bounds in either order. A caller diffing two ids has no reason to know
                ' which is the lower model row.
                ValidateRect( hSurf, NULL )
                PsListTree_InvalidateRows( hInv, 3, 1 )
                dim as RECT rcRev
                dim as long gotRev = GetUpdateRect( hSurf, @rcRev, FALSE )
                SelfTest_Check( "InvalidateRows accepts its bounds reversed", _
                    (gotRev <> 0) andalso (rcRev.top = rcU.top) andalso (rcRev.bottom = rcU.bottom), _
                    "reversed=" & SelfTest_RectStr( rcRev ) & " forward=" & SelfTest_RectStr( rcU ) )
            end if

            ' Invalid indices are a silent no-op, not a fault and not a whole-client repaint.
            ' The whole-client case is the one that matters: it would look CORRECT on screen
            ' and quietly undo the entire point of the call.
            ValidateRect( hSurf, NULL )
            PsListTree_InvalidateRow( hInv, -5 )
            PsListTree_InvalidateRow( hInv, PsListTree_GetCount( hInv ) + 500 )
            ' Into a variable FIRST. Inlined into the SelfTest_Check argument list the detail
            ' string printed a stale rect on a passing run -- FreeBASIC does not promise the
            ' argument evaluation order, so RectStr ran before GetUpdateRect cleared it.
            dim as long gotBad = GetUpdateRect( hSurf, @rcU, FALSE )
            SelfTest_Check( "InvalidateRow ignores an out-of-range row", _
                (gotBad = 0), "update=" & SelfTest_RectStr( rcU ) )

            ' A row scrolled below the viewport. This list is long enough that its last row
            ' cannot be on screen; assert that premise rather than assuming it.
            dim as integer lastRow = PsListTree_GetCount( hInv ) - 1
            PsListTree_SetPaintCallback( hInv, @SelfTest_RowCapturePaint )
            dim as boolean bLastOnScreen = SelfTest_GrabRow( hInv, lastRow )
            if bLastOnScreen then
                SelfTest_Check( "InvalidateRow: the off-screen probe needs a row off screen", _
                    false, "row " & str(lastRow) & " is visible; the demo list got shorter" )
            else
                ValidateRect( hSurf, NULL )
                PsListTree_InvalidateRow( hInv, lastRow )
                dim as long gotOff = GetUpdateRect( hSurf, @rcU, FALSE )
                SelfTest_Check( "InvalidateRow ignores a row scrolled out of view", _
                    (gotOff = 0), "update=" & SelfTest_RectStr( rcU ) )
            end if

            ' A row hidden inside a collapsed parent -- not the same case as scrolled away,
            ' and it takes the other exit: the two binary searches cross because no visible
            ' slot falls between the bounds.
            dim as integer rAlpha = SelfTest_HeaderByText( hInv, "Group Alpha" )
            if rAlpha < 0 then
                SelfTest_Check( "InvalidateRow: 'Group Alpha' not found for the collapse probe", _
                    false, "header missing" )
            else
                PsListTree_CollapseRow( hInv, rAlpha )
                ValidateRect( hSurf, NULL )
                PsListTree_InvalidateRow( hInv, rAlpha + 1 )        ' first child, now hidden
                dim as long gotHidden = GetUpdateRect( hSurf, @rcU, FALSE )

                ' And the header itself, which is still visible, still repaints -- otherwise
                ' this test would pass for a version that simply did nothing at all.
                ValidateRect( hSurf, NULL )
                PsListTree_InvalidateRow( hInv, rAlpha )
                dim as RECT rcHdr
                dim as long gotHeader = GetUpdateRect( hSurf, @rcHdr, FALSE )

                PsListTree_ExpandRow( hInv, rAlpha )

                SelfTest_Check( "InvalidateRow ignores a row hidden by a collapsed parent", _
                    (gotHidden = 0), "update=" & SelfTest_RectStr( rcU ) )
                SelfTest_Check( "InvalidateRow still repaints the collapsed parent itself", _
                    (gotHeader <> 0), "update=" & SelfTest_RectStr( rcHdr ) )
            end if

            PsListTree_SetPaintCallback( hInv, @PsListTree_PaintCallback )
            ValidateRect( hSurf, NULL )
        end if
    end scope

    print "=== self-test complete: " & str(gFailCount) & " failure(s) ==="
end sub


' ========================================================================================
' PsListTree_MessageCallback
' ========================================================================================
function PsListTree_MessageCallback( byval m as PSLISTTREE_MESSAGEINFO ptr ) as boolean

    select case m->uMsg
             
        case WM_MOUSEWHEEL
            print "WM_MOUSEWHEEL"
            return true

        case WM_MOUSEHOVER
            print "WM_MOUSEHOVER on idx: "; m->idx
            ' Tooltips are now on-demand via the tooltip callback; nothing to push.
            return true
            
        case WM_LBUTTONUP
            ' Report the resulting selection so multi-select is observable while testing.
            dim as HWND hCtl = GetParent(m->hList)
            ' plain string, not DWSTRING: print interleaves wide chars
            dim as string sKind = "item"
            if PsListTree_IsHeader( hCtl, m->idx ) then sKind = "HEADER"
            print "click row" & str(m->idx) & " (" & sKind & ")  focus=" & _
                  str(PsListTree_GetCurSel(hCtl)) & "  selCount=" & str(PsListTree_GetSelCount(hCtl))
            ' Header collapse/expand is handled inside the control on the mouse-down,
            ' and it preserves the current selection. Return FALSE: the listbox needs
            ' its default WM_LBUTTONUP handling to release the mouse capture it took
            ' on the button-down.
            return false
    
    end select

    ' Return false to allow to fall to the default windows handler
    return false
end function    


' ========================================================================================
' Drag-and-drop callbacks (demo). DragDrop reports every user move; CanDrop shows a veto
' using the drop-target ROWINFO -- here it forbids dropping onto the "Folders" header.
' ========================================================================================
sub PsListTree_DragDropCallback( byval hList as HWND, byval newFirstRow as integer, byval count as integer )
    print "DragDrop: moved " & str(count) & " row(s); block now starts at model row " & str(newFirstRow)
end sub

function PsListTree_CanDropCallback( byval p as PSLISTTREE_DROPINFO ptr ) as boolean
    if (p->onHeader = true) andalso (p->targetInfo <> 0) then
        dim as string sTarget = p->targetInfo->Text     ' plain string: print interleaves DWSTRING nulls
        if sTarget = "Folders" then
            print "CanDrop: rejected -- cannot drop onto the 'Folders' header"
            return false
        end if
    end if
    return true
end function


' ========================================================================================
' PsListTree_TooltipCallback  -- per-row tooltip text, resolved on demand
' ========================================================================================
function PsListTree_TooltipCallback( byval hCtl as HWND, byval row as integer, byval col as integer ) as DWSTRING
    ' Trace which cell the tip was asked for. Build the line as a string FIRST -- a print on a
    ' string & wstring concat interleaves nulls.
    dim as string sTrace = "tooltip requested: row " & row & ", col " & col
    print sTrace
    if PsListTree_IsHeader( hCtl, row ) then
        return "Group header: " & PsListTree_GetText( hCtl, row )
    end if
    return "Row " & row & " (col " & col & ") -> " & PsListTree_GetText( hCtl, row )
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
            100, 100, 900, 640 _
            )

    dim as RECT rc: GetClientRect( HWND_FRMMAIN, @rc )
    dim as long halfW = rc.right \ 2
    ' Reserve a full-width band at the bottom for instance C (the tree + editing showcase);
    ' A and B share the region above it.
    dim as long topH = rc.bottom - pWindow->ScaleY(230)
    if topH < 200 then topH = 200

    ' ---- Instance A (left half): many rows -- exercises scrolling ----
    HWND_FRMEXPLORER = PsListTree_Create( HWND_FRMMAIN, IDC_FRMMAIN_LISTBOX )
    SetWindowPos( HWND_FRMEXPLORER, 0, _
        rc.left, rc.top, halfW, topH, _
        SWP_NOZORDER or SWP_SHOWWINDOW )
    PsListTree_SetPaintCallback( HWND_FRMEXPLORER, @PsListTree_PaintCallback )
    PsListTree_SetMessageCallback( HWND_FRMEXPLORER, @PsListTree_MessageCallback )
    PsListTree_SetBackColor( HWND_FRMEXPLORER, theme.BackColor )
    PsListTree_SetFont( HWND_FRMEXPLORER, ghFont(GUIFONT_10) )
    PsListTree_SetTooltipCallback( HWND_FRMEXPLORER, @PsListTree_TooltipCallback )
    PsListTree_SetExtendedSelect( HWND_FRMEXPLORER, true )   ' Shift/Ctrl range selection
    ' Drag-reorder: instance A drags single rows and whole multi-selections (no CanDrop veto).
    PsListTree_SetDragReorder( HWND_FRMEXPLORER, true )
    PsListTree_SetDragDropCallback( HWND_FRMEXPLORER, @PsListTree_DragDropCallback )
    ' The scrollbar is created/positioned/auto-hidden by the control; we only theme it.
    ' (PsListTree_SetScrollBarPaintCallback is available for full custom drawing.)
    PsListTree_SetScrollBarColors( HWND_FRMEXPLORER, theme.BackColorScrollBar, _
                                 theme.ForeColorScrollBar, theme.ForeColorScrollBarHot )
    ' Two groups; bulk add is batched so the map rebuilds once, not per row.
    PsListTree_BeginUpdate( HWND_FRMEXPLORER )
    ' Two leading items for the non-selectable-row edge cases (A is extended-select, so it
    ' also drives the Shift-range test). Row 0 is non-selectable and is the VERY FIRST row:
    ' pressing Up from row 1 has nowhere selectable to land, so the caret stays on row 1.
    PsListTree_AddString( HWND_FRMEXPLORER, "(non-selectable first row - Up from below stays put)", 999 )  ' row 0
    PsListTree_AddString( HWND_FRMEXPLORER, "second row (selectable - press Up here)", 998 )               ' row 1
    PsListTree_AddHeader( HWND_FRMEXPLORER, "Group Alpha" )                                                ' row 2
    for i as integer = 0 to 20
        PsListTree_AddString( HWND_FRMEXPLORER, "Alpha item " & i, 1000 + i )                              ' rows 3..23
    next
    PsListTree_AddHeader( HWND_FRMEXPLORER, "Group Beta" )
    for i as integer = 0 to 20
        PsListTree_AddString( HWND_FRMEXPLORER, "Beta item " & i, 2000 + i )
    next
    PsListTree_EndUpdate( HWND_FRMEXPLORER )
    PsListTree_SetRowSelectable( HWND_FRMEXPLORER, 0, false )    ' the non-selectable first row
    PsListTree_SetRowSelectable( HWND_FRMEXPLORER, 8, false )    ' "Alpha item 5" -- for the Shift-range test
    PsListTree_SetCurSel( HWND_FRMEXPLORER, 1 )                  ' start on the second row

    ' ---- Instance B (right half): a couple of small groups, one starting
    ' collapsed, PLUS the embedded column header band. Tests: per-instance hover
    ' (must not affect A), empty client-area painting below the last row, collapse
    ' hiding a group's items, and the header/columns integration. ----
    HWND_FRMEXPLORER2 = PsListTree_Create( HWND_FRMMAIN, IDC_FRMMAIN_LISTBOX )
    SetWindowPos( HWND_FRMEXPLORER2, 0, _
        halfW, rc.top, rc.right - halfW, topH, _
        SWP_NOZORDER or SWP_SHOWWINDOW )
    PsListTree_SetPaintCallback( HWND_FRMEXPLORER2, @PsListTree_PaintCallback )
    PsListTree_SetMessageCallback( HWND_FRMEXPLORER2, @PsListTree_MessageCallback )
    PsListTree_SetBackColor( HWND_FRMEXPLORER2, theme.BackColor )
    PsListTree_SetFont( HWND_FRMEXPLORER2, ghFont(GUIFONT_10) )
    PsListTree_SetTooltipCallback( HWND_FRMEXPLORER2, @PsListTree_TooltipCallback )
    PsListTree_SetScrollBarColors( HWND_FRMEXPLORER2, theme.BackColorScrollBar, _
                                 theme.ForeColorScrollBar, theme.ForeColorScrollBarHot )
    ' B uses the OTHER selection mode, so the harness covers both.
    PsListTree_SetMultiSelect( HWND_FRMEXPLORER2, true )     ' checklist style: click toggles
    ' Drag-reorder on B too: it has headers (drop-on-header) and a CanDrop veto guarding "Folders".
    PsListTree_SetDragReorder( HWND_FRMEXPLORER2, true )
    PsListTree_SetDragDropCallback( HWND_FRMEXPLORER2, @PsListTree_DragDropCallback )
    PsListTree_SetCanDropCallback( HWND_FRMEXPLORER2, @PsListTree_CanDropCallback )
    PsListTree_BeginUpdate( HWND_FRMEXPLORER2 )
    PsListTree_AddHeader( HWND_FRMEXPLORER2, "Files" )              ' model row 0
    for i as integer = 0 to 40
        dim as integer r = PsListTree_AddString( HWND_FRMEXPLORER2, "file" & i & ".bas", i )
        ' Columns 1/2 (Size/Type); column 0 is the AddString text itself.
        PsListTree_SetCellText( HWND_FRMEXPLORER2, r, 1, str( (i + 1) * 137 ) )
        PsListTree_SetCellText( HWND_FRMEXPLORER2, r, 2, "FreeBASIC source" )
    next
    PsListTree_AddHeader( HWND_FRMEXPLORER2, "Folders" )           ' model row 42
    for i as integer = 0 to 4
        dim as integer r = PsListTree_AddString( HWND_FRMEXPLORER2, "folder" & i, i )
        ' Size stays "" (sparse cells) -- proves never-set cells paint as empty.
        PsListTree_SetCellText( HWND_FRMEXPLORER2, r, 2, "Folder" )
    next
    PsListTree_EndUpdate( HWND_FRMEXPLORER2 )
    ' Start the "Folders" group collapsed. (Was CollapseRow(6) -- comment rot from when
    ' the file loop added 5 rows; row 6 is a file item now, so that call was a silent
    ' no-op and the group never started collapsed.)
    PsListTree_CollapseRow( HWND_FRMEXPLORER2, 42 )

    ' ---- Spanned-row demo: an ordinary selectable item (model row 3) painted as ONE
    ' cell across the whole column band instead of one cell per column. Its text stays in
    ' column 0; the paint callback gets columnCount = 1 and p->isSpanned = true. Selecting,
    ' hover and keyboard navigation still work on it like any other row. ----
    PsListTree_SetText( HWND_FRMEXPLORER2, 3, "-- spanned row: this item merges all columns --" )
    PsListTree_SetRowSpanColumns( HWND_FRMEXPLORER2, 3, true )

    ' ---- Non-selectable-row demo: model row 20 cannot be selected or focused; the mouse
    ' and keyboard both skip it, and the paint callback greys it. ----
    PsListTree_SetCellText( HWND_FRMEXPLORER2, 20, 1, "--" )
    PsListTree_SetText( HWND_FRMEXPLORER2, 20, "file19.bas  (non-selectable)" )
    PsListTree_SetRowSelectable( HWND_FRMEXPLORER2, 20, false )

    ' ---- Embedded column header on instance B (phase 3). Columns are defined through
    ' the PsListTree wrappers; the header child is the store. Host callbacks go through
    ' the wrappers too -- the resize slot on the child itself belongs to the control. ----
    PsListTree_AddColumn( HWND_FRMEXPLORER2, "Name", 160 )
    PsListTree_AddColumn( HWND_FRMEXPLORER2, "Size", 70 )
    PsListTree_AddColumn( HWND_FRMEXPLORER2, "Type" )    ' fill column (default: last)
    PsListTree_SetHeaderPaintCallback( HWND_FRMEXPLORER2, @Header_PaintCallback )
    PsListTree_SetHeaderBackColor( HWND_FRMEXPLORER2, theme.BackColorScrollBar )
    PsListTree_SetHeaderFont( HWND_FRMEXPLORER2, ghFont(GUIFONTBOLD_10) )
    PsListTree_SetColumnResizeCallback( HWND_FRMEXPLORER2, @Header_WidthChangedCallback )
    PsListTree_SetColumnClickCallback( HWND_FRMEXPLORER2, @Header_ClickCallback )
    PsListTree_SetColumnAutoSizeCallback( HWND_FRMEXPLORER2, @Header_AutoSizeCallback )
    PsListTree_ShowHeader( HWND_FRMEXPLORER2, true )
    HWND_COLHDR = PsListTree_GetHeader( HWND_FRMEXPLORER2 )

    ' Instance A: enable in-place label editing (F2 on the focused row). Structure and look
    ' are unchanged -- this only adds the F2 rename gesture (and the pump obligation below).
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER, true )
    PsListTree_SetBeginLabelEditCallback( HWND_FRMEXPLORER, @Tree_BeginLabelEdit )
    PsListTree_SetEndLabelEditCallback( HWND_FRMEXPLORER, @Tree_EndLabelEdit )

    ' ---- Instance C (full-width bottom band): the TREE + IN-PLACE EDITING showcase.
    ' A real multi-level tree built with AddNode: control-drawn chevron twisties, per-depth
    ' indent, unlimited nesting, and label editing (F2, or a click on the already-current
    ' row). Nothing here is a header -- every parent is a plain AddNode node with a twisty. ----
    HWND_FRMEXPLORER3 = PsListTree_Create( HWND_FRMMAIN, IDC_FRMMAIN_LISTBOX )
    SetWindowPos( HWND_FRMEXPLORER3, 0, _
        rc.left, topH, rc.right, rc.bottom - topH, _
        SWP_NOZORDER or SWP_SHOWWINDOW )
    PsListTree_SetPaintCallback( HWND_FRMEXPLORER3, @PsListTree_PaintCallback )
    PsListTree_SetMessageCallback( HWND_FRMEXPLORER3, @PsListTree_MessageCallback )
    PsListTree_SetBackColor( HWND_FRMEXPLORER3, theme.BackColor )
    PsListTree_SetFont( HWND_FRMEXPLORER3, ghFont(GUIFONT_10) )
    PsListTree_SetTooltipCallback( HWND_FRMEXPLORER3, @PsListTree_TooltipCallback )
    PsListTree_SetScrollBarColors( HWND_FRMEXPLORER3, theme.BackColorScrollBar, _
                                 theme.ForeColorScrollBar, theme.ForeColorScrollBarHot )
    PsListTree_SetExtendedSelect( HWND_FRMEXPLORER3, true )
    ' The optional border, on this instance only, so the other two still show the default
    ' borderless look side by side with it. Raw pixels -- do NOT wrap this in ScaleX.
    PsListTree_SetBorderColor( HWND_FRMEXPLORER3, theme.ForeColorScrollBar )
    PsListTree_SetBorderWidth( HWND_FRMEXPLORER3, 1 )
    ' Tree visuals: per-depth indent + control-drawn chevrons in the icon font.
    PsListTree_SetTreeIndent( HWND_FRMEXPLORER3, true )
    PsListTree_ShowTwisty( HWND_FRMEXPLORER3, true )
    PsListTree_SetTwistyFont( HWND_FRMEXPLORER3, ghFont(SYMBOLFONT_9) )
    PsListTree_SetTwistyColor( HWND_FRMEXPLORER3, theme.ForeColor )
    ' In-place editing: F2 on the focused row, or a click on the row that is already current.
    PsListTree_EnableLabelEdit( HWND_FRMEXPLORER3, true )
    PsListTree_SetClickToEdit( HWND_FRMEXPLORER3, true )
    PsListTree_SetBeginLabelEditCallback( HWND_FRMEXPLORER3, @Tree_BeginLabelEdit )
    PsListTree_SetEndLabelEditCallback( HWND_FRMEXPLORER3, @Tree_EndLabelEdit )
    PsListTree_SetExpandCollapseCallback( HWND_FRMEXPLORER3, @Tree_ExpandCollapse )
    ' Build a 4-level tree. parent = -1 makes a new root; each AddNode appends under its parent.
    PsListTree_BeginUpdate( HWND_FRMEXPLORER3 )
    dim as integer rSrc = PsListTree_AddNode( HWND_FRMEXPLORER3, -1, "src", 0 )                    ' L0
        dim as integer rAfx = PsListTree_AddNode( HWND_FRMEXPLORER3, rSrc, "AfxNova", 0 )          ' L1
            PsListTree_AddNode( HWND_FRMEXPLORER3, rAfx, "CWindow.inc", 0 )                        ' L2 leaf
            dim as integer rCtl = PsListTree_AddNode( HWND_FRMEXPLORER3, rAfx, "controls", 0 )     ' L2
                PsListTree_AddNode( HWND_FRMEXPLORER3, rCtl, "PsListTree.inc", 0 )                  ' L3 leaf
                PsListTree_AddNode( HWND_FRMEXPLORER3, rCtl, "PsTextBox.inc", 0 )                  ' L3 leaf
            PsListTree_AddNode( HWND_FRMEXPLORER3, rAfx, "AfxStr.inc", 0 )                         ' L2 leaf
        dim as integer rDemo = PsListTree_AddNode( HWND_FRMEXPLORER3, rSrc, "demo", 0 )            ' L1
            PsListTree_AddNode( HWND_FRMEXPLORER3, rDemo, "main.bas", 0 )                          ' L2 leaf
            PsListTree_AddNode( HWND_FRMEXPLORER3, rDemo, "frmMain.inc", 0 )                       ' L2 leaf
    dim as integer rDocs = PsListTree_AddNode( HWND_FRMEXPLORER3, -1, "docs", 0 )                  ' L0
        PsListTree_AddNode( HWND_FRMEXPLORER3, rDocs, "README.md", 0 )                             ' L1 leaf
        PsListTree_AddNode( HWND_FRMEXPLORER3, rDocs, "REFACTOR_PLAN.md", 0 )                      ' L1 leaf
    PsListTree_AddNode( HWND_FRMEXPLORER3, -1, "LICENSE  (leaf root - no twisty)", 0 )             ' L0 leaf
    ' Start "controls" collapsed so a collapsed twisty at depth 2 is visible from the outset.
    PsListTree_SetNodeCollapsed( HWND_FRMEXPLORER3, rCtl, true )
    PsListTree_EndUpdate( HWND_FRMEXPLORER3 )
    PsListTree_SetCurSel( HWND_FRMEXPLORER3, 0 )

    ' ---- Geometry gates (numeric, asserted, never eyeballed) ----
    ' 1) Column rects: contiguous from 0, fill column's right edge = header client width.
    scope
        dim as RECT rcH, rcC
        GetClientRect( HWND_COLHDR, @rcC )
        dim as string sTrace = "Header rects:"
        dim as integer prevRight = 0
        dim as boolean okContig = true
        for i as integer = 0 to PsColumnHeader_GetCount( HWND_COLHDR ) - 1
            PsColumnHeader_GetColumnRect( HWND_COLHDR, i, rcH )
            sTrace &= " c" & str(i) & "=[" & str(rcH.left) & ".." & str(rcH.right) & "]"
            if i = 0 then
                if rcH.left <> 0 then okContig = false
            else
                if rcH.left <> prevRight then okContig = false
            end if
            prevRight = rcH.right
        next
        print sTrace
        dim as boolean okFill = (prevRight = rcC.right)
        dim as string sVerdict = "Header geometry: "
        if okContig andalso okFill then sVerdict &= "PASS" else sVerdict &= "FAIL"
        sVerdict &= "  (contiguous=" & str(okContig) & " lastRight=" & str(prevRight) & _
                    " clientW=" & str(rcC.right) & ")"
        print sVerdict
    end scope
    ' 2) Embedding layout: header spans the full container width at the top; the listbox
    '    starts below it; hiding the header gives the strip back.
    scope
        dim as HWND hList2 = GetDlgItem( HWND_FRMEXPLORER2, IDC_FRMMAIN_LISTBOX )
        dim as RECT rcHdr, rcList, rcCont
        GetWindowRect( HWND_COLHDR, @rcHdr )
        GetWindowRect( hList2, @rcList )
        GetWindowRect( HWND_FRMEXPLORER2, @rcCont )
        dim as integer hdrTop  = rcHdr.top - rcCont.top
        dim as integer hdrWide = rcHdr.right - rcHdr.left
        dim as integer contW   = rcCont.right - rcCont.left
        dim as integer hdrHgt  = rcHdr.bottom - rcHdr.top
        dim as integer listTop = rcList.top - rcCont.top
        dim as string sTrace = "Embed: hdrTop=" & str(hdrTop) & " hdrW=" & str(hdrWide) & _
                               "/" & str(contW) & " hdrH=" & str(hdrHgt) & " listTop=" & str(listTop)
        print sTrace
        dim as boolean okShown = (hdrTop = 0) andalso (hdrWide = contW) andalso (listTop = hdrHgt)
        PsListTree_ShowHeader( HWND_FRMEXPLORER2, false )
        GetWindowRect( hList2, @rcList )
        dim as integer listTopHidden = rcList.top - rcCont.top
        PsListTree_ShowHeader( HWND_FRMEXPLORER2, true )
        dim as boolean okHidden = (listTopHidden = 0)
        dim as string sVerdict = "Embed layout: "
        if okShown andalso okHidden then sVerdict &= "PASS" else sVerdict &= "FAIL"
        sVerdict &= "  (listTop hidden=" & str(listTopHidden) & ")"
        print sVerdict
    end scope

    print "PsListTree / PsVScrollBar test harness"
    print "-----------------------------------"
    print "Instance A (left) : "; PsListTree_GetCount( HWND_FRMEXPLORER ); " model rows, "; _
          PsListTree_GetVisibleCount( HWND_FRMEXPLORER ); " visible   [extended select: Shift/Ctrl]"
    print "Instance B (right): "; PsListTree_GetCount( HWND_FRMEXPLORER2 ); " model rows, "; _
          PsListTree_GetVisibleCount( HWND_FRMEXPLORER2 ); " visible   [multi select: click toggles]"
    print
    print "Manual checks (things a script cannot drive):"
    print "  Selection  A: click / Ctrl+click toggles / Shift+click ranges"
    print "             B: every click toggles one row"
    print "  Headers    click a header -> folds, and your item selection SURVIVES"
    print "  Keyboard   arrows, PageUp/Dn, Home/End; Left/Right collapse/expand;"
    print "             Space toggles (the only way to select a header)"
    print "  Hover      row highlights; whip the mouse out -> must clear every time"
    print "  Tooltips   pause on a row; text follows the row as you move"
    print "  Scrollbar  drag thumb (works on the FIRST press); hold the track to"
    print "             auto-repeat; thumb highlights on hover"
    print "  Auto-hide  collapse a group in A until the rows fit -> bar disappears"
    print "             and the list reclaims the width; expand -> it returns"
    print "  2 instances nothing you do in one may disturb the other"
    print "  Header     cursor flips to <-> over a divider (not over the fill column's"
    print "             edge); drag resizes LIVE (traces w=... live=true, final"
    print "             live=false) and the row cells follow; ESC mid-drag snaps back;"
    print "             dbl-click a divider autosizes to the caption; click a column"
    print "             body SORTS instance B (arrow glyph, toggles asc/desc; press,"
    print "             slide off, release -> nothing); tooltip shows column text"
    print "  Self-test  PSLISTTREE_SELFTEST=1 main.exe -> geometry asserts, then exits"
    print

    
    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Headless geometry/contract assertions, then exit (PsHScrollBar's env-gate shape):
    ' a normal launch never runs this.
    if environ("PSLISTTREE_SELFTEST") = "1" then
        frmMain_RunSelfTest()
        DestroyWindow( HWND_FRMMAIN )
    end if

    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' PsListTree label-editing pump obligation: forwards to the active editor's PsTextBox
        ' so its right-click context menu gets keyboard nav / outside-click dismissal. Required
        ' because instances A and C enable label editing; a no-op when nothing is editing.
        if PsListTree_FilterMessage( @uMsg ) then continue do

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

