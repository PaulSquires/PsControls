'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"

' ----------------------------------------------------------------------------------------
' Harness model: three panes laid out by the HOST, separated by two PsSplitter instances.
'
'     +---------------+---+-------------------+
'     |               | V |   right / top     |
'     |     left      | e |                   |
'     |     pane      | r +===== H bar =======+
'     |               | t |   right / bottom  |
'     +---------------+---+-------------------+
'
' The panes are painted rects, not windows -- which is the whole point: the control has no
' idea what is on either side of it. The vertical bar's position is an x in this window's
' client space; the horizontal bar's is a y. The horizontal bar lives inside the right
' pane, so its CROSS-axis geometry (x and width) tracks the vertical bar -- the control
' preserves whatever cross-axis geometry we give it and never invents its own.
' ----------------------------------------------------------------------------------------
#define SPLITTER_SIZE     6      ' bar thickness, DPI-scaled; this IS the grab area
#define MIN_PANE         80      ' smallest pane we allow, DPI-scaled -> feeds SetRange

dim shared as HWND HWND_SPLITV
dim shared as HWND HWND_SPLITH

' Double-click collapse demo state (host-owned: the control knows nothing about collapsing)
dim shared as boolean gCollapsedH = false
dim shared as integer gSavedPosH  = 0

' Self-test support: a recorder for the position notifications, so the BEGIN/MOVE/END
' sequence can be asserted instead of eyeballed.
#define MAX_REC  64
dim shared as boolean gRecording = false
dim shared as integer gRecCount  = 0
dim shared as integer gRecPhase( 0 to MAX_REC - 1 )
dim shared as integer gRecPos( 0 to MAX_REC - 1 )
dim shared as integer gFailCount = 0


' ========================================================================================
' Reposition the horizontal bar's CROSS axis. Called while the vertical bar is being
' dragged: the right pane's left edge moved, so the horizontal bar must follow it.
' The bar's own position (its y) is left alone.
' ========================================================================================
sub frmMain_PositionHSplitter()
    dim pWindow as CWindow ptr = AfxCWindowPtr(HWND_FRMMAIN)
    if pWindow = 0 then exit sub

    dim as RECT rc : GetClientRect( HWND_FRMMAIN, @rc )
    dim as integer barV = pWindow->ScaleX( SPLITTER_SIZE )
    dim as integer barH = pWindow->ScaleY( SPLITTER_SIZE )
    dim as integer xRight = PsSplitter_GetPos( HWND_SPLITV ) + barV

    SetWindowPos( HWND_SPLITH, 0, _
        xRight, PsSplitter_GetPos( HWND_SPLITH ), rc.right - xRight, barH, _
        SWP_NOZORDER )
end sub


' ========================================================================================
' Full layout. The LIMITS are computed here and pushed into the controls: a splitter never
' derives them from the parent's client rect, because it cannot know what else lives there.
' ========================================================================================
sub frmMain_Layout( byval hwnd as HWND )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit sub

    dim as RECT rc : GetClientRect( hwnd, @rc )
    dim as integer clientW = rc.right - rc.left
    dim as integer clientH = rc.bottom - rc.top
    if (clientW <= 0) orelse (clientH <= 0) then exit sub

    dim as integer barV     = pWindow->ScaleX( SPLITTER_SIZE )
    dim as integer barH     = pWindow->ScaleY( SPLITTER_SIZE )
    dim as integer minPaneW = pWindow->ScaleX( MIN_PANE )
    dim as integer minPaneH = pWindow->ScaleY( MIN_PANE )

    ' Vertical bar: full client height, x is its position.
    PsSplitter_SetRange( HWND_SPLITV, minPaneW, clientW - minPaneW - barV )
    SetWindowPos( HWND_SPLITV, 0, _
        PsSplitter_GetPos( HWND_SPLITV ), rc.top, barV, clientH, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    ' Horizontal bar: spans the right pane, y is its position.
    dim as integer xRight = PsSplitter_GetPos( HWND_SPLITV ) + barV
    PsSplitter_SetRange( HWND_SPLITH, minPaneH, clientH - minPaneH - barH )
    SetWindowPos( HWND_SPLITH, 0, _
        xRight, PsSplitter_GetPos( HWND_SPLITH ), clientW - xRight, barH, _
        SWP_NOZORDER or SWP_SHOWWINDOW )

    InvalidateRect( hwnd, NULL, FALSE )
end sub


' ========================================================================================
' Paint one pane: filled box plus a label in its top-left corner.
' ========================================================================================
sub frmMain_PaintPane( _
            byref b       as PsBufferPaint, _
            byval rcPane  as RECT, _
            byval sLabel  as string _
            )

    if (rcPane.right <= rcPane.left) orelse (rcPane.bottom <= rcPane.top) then exit sub

    b.SetBackColor( theme.BackColorBox )
    b.PaintRect( @rcPane )

    dim as RECT rcText = rcPane
    rcText.left   += 10
    rcText.top    += 8
    rcText.right  -= 6
    rcText.bottom  = rcText.top + 60
    b.SetFont( ghFont(GUIFONT_10) )
    b.SetForeColor( theme.ForeColor )
    b.SetBackColor( theme.BackColorBox )
    b.PaintText( sLabel, @rcText, DT_LEFT or DT_WORDBREAK )
end sub


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then return 0

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColorPanel )
    b.PaintClientRect()

    dim as RECT rc : GetClientRect( hwnd, @rc )
    dim as integer barV = pWindow->ScaleX( SPLITTER_SIZE )
    dim as integer barH = pWindow->ScaleY( SPLITTER_SIZE )
    dim as integer posV = PsSplitter_GetPos( HWND_SPLITV )
    dim as integer posH = PsSplitter_GetPos( HWND_SPLITH )

    dim as RECT rcLeft, rcTop, rcBottom
    SetRect( @rcLeft,   rc.left,      rc.top,        posV,     rc.bottom )
    SetRect( @rcTop,    posV + barV,  rc.top,        rc.right, posH )
    SetRect( @rcBottom, posV + barV,  posH + barH,   rc.right, rc.bottom )

    frmMain_PaintPane( b, rcLeft, "Left pane" & chr(13,10) & _
        "vertical bar x = " & str(posV) & chr(13,10) & _
        "range " & str(PsSplitter_GetMin(HWND_SPLITV)) & " .. " & str(PsSplitter_GetMax(HWND_SPLITV)) )

    frmMain_PaintPane( b, rcTop, "Right / top pane" & chr(13,10) & _
        "horizontal bar y = " & str(posH) & chr(13,10) & _
        "range " & str(PsSplitter_GetMin(HWND_SPLITH)) & " .. " & str(PsSplitter_GetMax(HWND_SPLITH)) )

    dim as string sBottom = "Right / bottom pane" & chr(13,10) & "double-click the H bar to collapse/restore"
    if gCollapsedH then sBottom = "Right / bottom pane (collapsed)" & chr(13,10) & "double-click the H bar to restore"
    frmMain_PaintPane( b, rcBottom, sBottom )

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Human-readable names for the trace.
' ========================================================================================
function frmMain_PhaseName( byval nPhase as integer ) as string
    select case nPhase
        case PSSPLITTER_PHASE_BEGIN : return "BEGIN"
        case PSSPLITTER_PHASE_MOVE  : return "MOVE "
        case PSSPLITTER_PHASE_END   : return "END  "
    end select
    return "?????"
end function

function frmMain_SplitName( byval hSplitter as HWND ) as string
    if hSplitter = HWND_SPLITV then return "V"
    return "H"
end function


' ========================================================================================
' The user moved a splitter. The control has already moved ITSELF; all we do is lay out
' what it does not own -- our panes, and the other bar's cross-axis geometry.
' ========================================================================================
sub Split_PosChanged( byval hSplitter as HWND, byval newPos as integer, byval nPhase as integer )
    if gRecording then
        if gRecCount < MAX_REC then
            gRecPhase(gRecCount) = nPhase
            gRecPos(gRecCount)   = newPos
            gRecCount += 1
        end if
    end if

    ' The horizontal bar sits inside the right pane, so a vertical move drags it sideways.
    if hSplitter = HWND_SPLITV then frmMain_PositionHSplitter()

    ' A real host would use BEGIN/END to bracket expensive work here (tiko hides the
    ' editor's horizontal scrollbars for the duration of a splitter drag).
    InvalidateRect( HWND_FRMMAIN, NULL, FALSE )

    if gRecording = false then
        dim as string s = "splitter " & frmMain_SplitName(hSplitter) & "  " & _
                          frmMain_PhaseName(nPhase) & "  pos=" & str(newPos)
        print s
    end if
end sub


' ========================================================================================
' Custom paint callback (horizontal bar only): a thin centered line on the panel
' background. The vertical bar intentionally has NO paint callback, so the control's
' built-in painter stays covered too.
' ========================================================================================
sub SplitH_PaintCallback( byval p as PSSPLITTER_PAINTINFO ptr )
    p->b->SetBackColor( theme.BackColorPanel )
    p->b->PaintClientRect()

    dim as RECT rcLine = p->rcClient
    dim as integer midY = (rcLine.top + rcLine.bottom) \ 2
    rcLine.top    = midY
    rcLine.bottom = midY + 1

    dim as COLORREF clr = theme.ForeColorRuler
    if p->isHot orelse p->isDragging then clr = theme.FocusAccentHot
    p->b->SetBackColor( clr )
    p->b->PaintRect( @rcLine )
end sub


' ========================================================================================
' Message callback (horizontal bar only): claim the double-click to collapse/restore the
' bottom pane. Claiming it means no drag starts; the control keeps the capture it already
' took and releases it on the trailing WM_LBUTTONUP.
'
' Note PsSplitter_SetPos is SILENT -- no notification comes back -- so the repaint is ours.
' ========================================================================================
function SplitH_MessageCallback( byval m as PSSPLITTER_MESSAGEINFO ptr ) as boolean
    if m->uMsg <> WM_LBUTTONDBLCLK then return false

    dim as string sWhat
    if gCollapsedH then
        PsSplitter_SetPos( m->hSplitter, gSavedPosH )
        gCollapsedH = false
        sWhat = "restored"
    else
        gSavedPosH = PsSplitter_GetPos( m->hSplitter )
        PsSplitter_SetPos( m->hSplitter, PsSplitter_GetMax( m->hSplitter ) )
        gCollapsedH = true
        sWhat = "collapsed"
    end if

    InvalidateRect( HWND_FRMMAIN, NULL, FALSE )
    dim as string s = "dblclick H -> bottom pane " & sWhat & _
                      "  (SetPos is silent: no END trace follows)"
    print s
    return true
end function


' ========================================================================================
' Geometry / notification self-test (run with PSSPLITTER_SELFTEST=1).
'
' The drag assertions drive the REAL WndProc: the cursor is warped with SetCursorPos and
' the mouse messages are sent to the control, so the position arithmetic under test is the
' same code a real drag runs (the control reads the cursor with GetCursorPos, never from
' lParam). Expectations are computed from where the cursor ACTUALLY landed -- SetCursorPos
' clamps to the virtual screen -- so a small display cannot produce a false failure.
'
' The cursor is saved and restored around the test.
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

' Where the bar ACTUALLY is, read from the window itself in parent client coordinates.
function SelfTest_BarEdge( byval hSplit as HWND ) as integer
    dim as RECT rc
    GetWindowRect( hSplit, @rc )
    MapWindowPoints( HWND_DESKTOP, HWND_FRMMAIN, cast( POINT ptr, @rc ), 2 )
    if PsSplitter_GetOrientation(hSplit) = PSSPLITTER_VERTICAL then return rc.left
    return rc.top
end function

' The cursor's current position along the bar's drag axis, in client coordinates.
function SelfTest_CursorAxis( byval hSplit as HWND ) as integer
    dim as POINT pt
    GetCursorPos( @pt )
    ScreenToClient( HWND_FRMMAIN, @pt )
    if PsSplitter_GetOrientation(hSplit) = PSSPLITTER_VERTICAL then return pt.x
    return pt.y
end function

sub SelfTest_CursorToClient( byval x as integer, byval y as integer )
    dim as POINT pt : pt.x = x : pt.y = y
    ClientToScreen( HWND_FRMMAIN, @pt )
    SetCursorPos( pt.x, pt.y )
end sub

function SelfTest_Clamp( byval n as integer, byval lo as integer, byval hi as integer ) as integer
    if hi < lo then hi = lo
    if n < lo then n = lo
    if n > hi then n = hi
    return n
end function

' Warp the cursor and deliver a move, then assert the control landed where the (absolute,
' clamp-at-the-edge) mapping says it should.
sub SelfTest_DragTo( _
            byval hSplit  as HWND, _
            byval x       as integer, _
            byval y       as integer, _
            byval nOffset as integer, _
            byval lo      as integer, _
            byval hi      as integer, _
            byval sName   as string _
            )

    SelfTest_CursorToClient( x, y )
    SendMessage( hSplit, WM_MOUSEMOVE, 0, 0 )

    dim as integer nCursor   = SelfTest_CursorAxis( hSplit )
    dim as integer nExpected = SelfTest_Clamp( nCursor - nOffset, lo, hi )
    SelfTest_Check( sName, PsSplitter_GetPos(hSplit) = nExpected, _
        "cursor=" & str(nCursor) & " expected=" & str(nExpected) & _
        " pos=" & str(PsSplitter_GetPos(hSplit)) )
    SelfTest_Check( sName & " (window followed)", SelfTest_BarEdge(hSplit) = nExpected, _
        "edge=" & str(SelfTest_BarEdge(hSplit)) & " expected=" & str(nExpected) )
end sub

sub frmMain_RunSelfTest()
    dim as HWND hV = HWND_SPLITV
    dim as HWND hH = HWND_SPLITH

    print ""
    print "=== PsSplitter self-test (warps the mouse pointer; it is restored at the end) ==="

    dim as POINT ptSave : GetCursorPos( @ptSave )
    gRecording = true          ' silence the trace; record notifications instead

    dim as RECT rcClient : GetClientRect( HWND_FRMMAIN, @rcClient )
    print "client = " & str(rcClient.right) & " x " & str(rcClient.bottom)

    ' ---- position <-> window rect ------------------------------------------------------
    gRecCount = 0
    PsSplitter_SetRange( hV, 100, 400 )
    PsSplitter_SetPos( hV, 250 )
    SelfTest_Check( "V SetPos -> GetPos round-trip", PsSplitter_GetPos(hV) = 250, _
        "pos=" & str(PsSplitter_GetPos(hV)) )
    SelfTest_Check( "V window leading edge = GetPos", SelfTest_BarEdge(hV) = 250, _
        "edge=" & str(SelfTest_BarEdge(hV)) )

    ' ---- clamping ----------------------------------------------------------------------
    PsSplitter_SetPos( hV, 20 )
    SelfTest_Check( "V SetPos clamps to nMin", PsSplitter_GetPos(hV) = 100, _
        "pos=" & str(PsSplitter_GetPos(hV)) & " min=100" )
    PsSplitter_SetPos( hV, 9999 )
    SelfTest_Check( "V SetPos clamps to nMax", PsSplitter_GetPos(hV) = 400, _
        "pos=" & str(PsSplitter_GetPos(hV)) & " max=400" )

    ' ---- SetRange pulls an out-of-range position back in, silently ---------------------
    PsSplitter_SetRange( hV, 100, 300 )
    SelfTest_Check( "SetRange clamps current pos", PsSplitter_GetPos(hV) = 300, _
        "pos=" & str(PsSplitter_GetPos(hV)) & " new max=300" )
    SelfTest_Check( "SetRange moved the window too", SelfTest_BarEdge(hV) = 300, _
        "edge=" & str(SelfTest_BarEdge(hV)) )

    ' ---- inverted range collapses to nMin ----------------------------------------------
    PsSplitter_SetRange( hV, 200, 50 )
    SelfTest_Check( "inverted range (max<min) pins at min", PsSplitter_GetPos(hV) = 200, _
        "pos=" & str(PsSplitter_GetPos(hV)) )

    ' ---- everything above is programmatic: NOTHING may have been notified --------------
    SelfTest_Check( "programmatic setters are silent", gRecCount = 0, _
        "notifications=" & str(gRecCount) )

    ' ---- the HOST moves the bar: nPos must re-sync from the window ---------------------
    PsSplitter_SetRange( hV, 100, 400 )
    dim as RECT rcBar : GetWindowRect( hV, @rcBar )
    SetWindowPos( hV, 0, 275, 0, rcBar.right - rcBar.left, rcClient.bottom, SWP_NOZORDER )
    SelfTest_Check( "host SetWindowPos -> GetPos re-syncs", PsSplitter_GetPos(hV) = 275, _
        "pos=" & str(PsSplitter_GetPos(hV)) & " expected=275" )
    SelfTest_Check( "re-sync is silent", gRecCount = 0, "notifications=" & str(gRecCount) )

    ' ---- a real drag through the real WndProc ------------------------------------------
    gRecCount = 0
    PsSplitter_SetPos( hV, 200 )
    SelfTest_CursorToClient( 205, 60 )
    SendMessage( hV, WM_LBUTTONDOWN, 0, 0 )
    dim as integer nOffset = SelfTest_CursorAxis(hV) - 200
    SelfTest_Check( "V grab takes the capture", GetCapture() = hV, "capture ok" )
    SelfTest_Check( "V grab sets IsDragging", PsSplitter_IsDragging(hV) = true, "dragging" )
    SelfTest_Check( "V grab notifies BEGIN at current pos", _
        (gRecCount = 1) andalso (gRecPhase(0) = PSSPLITTER_PHASE_BEGIN) andalso (gRecPos(0) = 200), _
        "count=" & str(gRecCount) & " pos=" & str(gRecPos(0)) & " grabOffset=" & str(nOffset) )

    SelfTest_DragTo( hV, 305, 60, nOffset, 100, 400, "V drag mid-track" )
    SelfTest_DragTo( hV, 700, 60, nOffset, 100, 400, "V drag past nMax clamps" )
    ' THE non-sticky assertion: coming back inside the range must resume tracking the
    ' cursor immediately. An implementation that accumulated deltas would still be paying
    ' off the excursion above and would land somewhere else entirely.
    SelfTest_DragTo( hV, 380, 60, nOffset, 100, 400, "V drag back inside is not sticky" )

    dim as integer nBefore = gRecCount
    SendMessage( hV, WM_MOUSEMOVE, 0, 0 )      ' same cursor position
    SelfTest_Check( "unchanged position notifies nothing", gRecCount = nBefore, _
        "before=" & str(nBefore) & " after=" & str(gRecCount) )

    SelfTest_DragTo( hV, 20, 60, nOffset, 100, 400, "V drag past nMin clamps" )

    SendMessage( hV, WM_LBUTTONUP, 0, 0 )
    SelfTest_Check( "V release clears IsDragging", PsSplitter_IsDragging(hV) = false, "not dragging" )
    SelfTest_Check( "V release frees the capture", GetCapture() <> hV, "capture released" )

    ' ---- the notification sequence itself ----------------------------------------------
    dim as boolean seqOk = (gRecCount >= 3)
    if seqOk then
        if gRecPhase(0) <> PSSPLITTER_PHASE_BEGIN then seqOk = false
        if gRecPhase(gRecCount - 1) <> PSSPLITTER_PHASE_END then seqOk = false
        for i as integer = 1 to gRecCount - 2
            if gRecPhase(i) <> PSSPLITTER_PHASE_MOVE then seqOk = false
        next
    end if
    dim as string sSeq
    for i as integer = 0 to gRecCount - 1
        sSeq &= trim(frmMain_PhaseName(gRecPhase(i))) & "(" & str(gRecPos(i)) & ") "
    next
    SelfTest_Check( "sequence is BEGIN, MOVE.., END", seqOk, sSeq )
    SelfTest_Check( "END carries the final position", _
        (gRecCount > 0) andalso (gRecPos(gRecCount - 1) = PsSplitter_GetPos(hV)), _
        "end=" & str(gRecPos(gRecCount - 1)) & " pos=" & str(PsSplitter_GetPos(hV)) )

    dim as boolean noDup = true
    for i as integer = 1 to gRecCount - 1
        if gRecPos(i) = gRecPos(i - 1) then
            ' BEGIN->first MOVE and last MOVE->END legitimately repeat a position only for
            ' END; a repeated MOVE would mean the control notified without changing.
            if gRecPhase(i) = PSSPLITTER_PHASE_MOVE then noDup = false
        end if
    next
    SelfTest_Check( "no MOVE repeats the previous position", noDup, "records=" & str(gRecCount) )

    ' ---- horizontal orientation: same machinery, other axis ----------------------------
    gRecCount = 0
    dim as integer xRight = PsSplitter_GetPos(hV) + 40
    PsSplitter_SetRange( hH, 80, 300 )
    PsSplitter_SetPos( hH, 150 )
    SelfTest_Check( "H orientation reported", _
        PsSplitter_GetOrientation(hH) = PSSPLITTER_HORIZONTAL, _
        "orientation=" & str(PsSplitter_GetOrientation(hH)) )
    SelfTest_Check( "H SetPos -> window top = GetPos", SelfTest_BarEdge(hH) = 150, _
        "edge=" & str(SelfTest_BarEdge(hH)) & " pos=" & str(PsSplitter_GetPos(hH)) )

    SelfTest_CursorToClient( xRight, 153 )
    SendMessage( hH, WM_LBUTTONDOWN, 0, 0 )
    dim as integer nOffsetH = SelfTest_CursorAxis(hH) - 150
    SelfTest_DragTo( hH, xRight, 253, nOffsetH, 80, 300, "H drag mid-track" )
    SelfTest_DragTo( hH, xRight, 460, nOffsetH, 80, 300, "H drag past nMax clamps" )
    SendMessage( hH, WM_LBUTTONUP, 0, 0 )
    SelfTest_Check( "H release clears IsDragging", PsSplitter_IsDragging(hH) = false, "not dragging" )

    ' ---- capture stolen mid-drag: keep the position, still close the gesture ------------
    gRecCount = 0
    PsSplitter_SetPos( hV, 250 )
    SelfTest_CursorToClient( 255, 60 )
    SendMessage( hV, WM_LBUTTONDOWN, 0, 0 )
    SelfTest_DragTo( hV, 320, 60, SelfTest_CursorAxis(hV) - 250, 100, 400, "V drag before steal" )
    dim as integer nPosAtSteal = PsSplitter_GetPos(hV)
    ReleaseCapture()                          ' stands in for another window stealing it
    SelfTest_Check( "capture loss keeps the position", PsSplitter_GetPos(hV) = nPosAtSteal, _
        "pos=" & str(PsSplitter_GetPos(hV)) & " at steal=" & str(nPosAtSteal) )
    SelfTest_Check( "capture loss clears IsDragging", PsSplitter_IsDragging(hV) = false, "not dragging" )
    SelfTest_Check( "capture loss still notifies END", _
        (gRecCount > 0) andalso (gRecPhase(gRecCount - 1) = PSSPLITTER_PHASE_END), _
        "last=" & frmMain_PhaseName(gRecPhase(gRecCount - 1)) )

    SetCursorPos( ptSave.x, ptSave.y )
    gRecording = false
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

        case WM_SIZE
            frmMain_Layout( hwnd )
            return 0

        case WM_KEYDOWN
            ' Programmatic SetPos exercise. SetPos is SILENT by design (host-driven
            ' positioning must not echo back through the position callback), so the host
            ' repaints its own content here.
            dim as boolean moved = false
            select case wParam
                case VK_LEFT
                    PsSplitter_SetPos( HWND_SPLITV, PsSplitter_GetPos(HWND_SPLITV) - 10 ) : moved = true
                case VK_RIGHT
                    PsSplitter_SetPos( HWND_SPLITV, PsSplitter_GetPos(HWND_SPLITV) + 10 ) : moved = true
                case VK_UP
                    PsSplitter_SetPos( HWND_SPLITH, PsSplitter_GetPos(HWND_SPLITH) - 10 ) : moved = true
                case VK_DOWN
                    PsSplitter_SetPos( HWND_SPLITH, PsSplitter_GetPos(HWND_SPLITH) + 10 ) : moved = true
            end select
            if moved then
                frmMain_PositionHSplitter()
                InvalidateRect( hwnd, NULL, FALSE )
                dim as string s = "SetPos (silent)  V=" & str(PsSplitter_GetPos(HWND_SPLITV)) & _
                    "  H=" & str(PsSplitter_GetPos(HWND_SPLITH)) & "  <- no callback fired"
                print s
            end if

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
            100, 100, 900, 560 _
            )

    ' ---- Vertical bar: default painter, no message callback ----
    HWND_SPLITV = PsSplitter_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPLITV, PSSPLITTER_VERTICAL )
    PsSplitter_SetColors( HWND_SPLITV, theme.BackColorScrollBar, theme.ForeColorScrollBarHot )
    PsSplitter_SetPosChangedCallback( HWND_SPLITV, @Split_PosChanged )

    ' ---- Horizontal bar: custom painter + double-click collapse ----
    HWND_SPLITH = PsSplitter_Create( HWND_FRMMAIN, IDC_FRMMAIN_SPLITH, PSSPLITTER_HORIZONTAL )
    PsSplitter_SetPosChangedCallback( HWND_SPLITH, @Split_PosChanged )
    PsSplitter_SetPaintCallback( HWND_SPLITH, @SplitH_PaintCallback )
    PsSplitter_SetMessageCallback( HWND_SPLITH, @SplitH_MessageCallback )

    ' Initial positions, before the first layout pass sets the limits.
    PsSplitter_SetPos( HWND_SPLITV, pWindow->ScaleX(240) )
    PsSplitter_SetPos( HWND_SPLITH, pWindow->ScaleY(240) )

    frmMain_Layout( HWND_FRMMAIN )

    print "PsSplitter test harness"
    print "----------------------"
    print "Vertical bar   default painter, position = its x in the form's client space"
    print "Horizontal bar custom painter (thin centered line), double-click = collapse"
    print ""
    print "Manual checks (things a script cannot drive):"
    print "  Cursor       IDC_SIZEWE over the vertical bar, IDC_SIZENS over the horizontal"
    print "               one, and it must HOLD for the whole drag, including over the panes"
    print "  Hover        the bar brightens under the mouse; whip the mouse out fast"
    print "               -> the highlight must clear every time"
    print "  Drag         the bar tracks the cursor with no jump, stops dead at the limits,"
    print "               and resumes the instant the cursor comes back inside them"
    print "  Drag+hover   the bar must STAY highlighted while dragging outside its own rect"
    print "  Steal        Alt-Tab away mid-drag -> the drag ends where it was, no stranded"
    print "               capture (the next click on this window must register first time)"
    print "  Dbl-click    double-click the horizontal bar -> bottom pane collapses/restores"
    print "  Keyboard     arrow keys nudge both bars via SetPos (all SILENT: no callback"
    print "               trace may appear for these)"
    print "  Resize       limits rederive from the client size; the bars stay inside them"
    print ""

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    if environ("PSSPLITTER_SELFTEST") = "1" then
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
