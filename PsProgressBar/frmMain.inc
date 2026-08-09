'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' One specimen row: the label the form draws, and the bar that sits to its right.
type DEMO_ROW
    wszLabel as DWSTRING
end type
dim shared gRows(0 to HBAR_COUNT - 1) as DEMO_ROW
dim shared gVLabels(0 to VBAR_COUNT - 1) as DWSTRING

' Drives the determinate rows so the specimen sheet is not a still life. Purely the demo's:
' the CONTROL never animates a position (see "THE POSITION JUMPS" in PsProgressBar.bi).
#define IDT_FRMMAIN_DEMO   &h7001
#define DEMO_TICK_MS       120
dim shared as long gDemoTick


' ========================================================================================
' Row geometry.
' ========================================================================================
#define ROW_HEIGHT      44
#define ROW_TOP         12
#define ROW_MARGIN      20
#define LABEL_WIDTH    300
#define BAR_HEIGHT      18
#define VSTRIP_WIDTH   140
#define VBAR_WIDTH      22

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
' The form draws the labels; each bar paints its own cell. There is no container window
' between them.
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim as RECT rc
    GetClientRect( hwnd, @rc )

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin  = ROW_MARGIN
    dim as long stripW  = VSTRIP_WIDTH
    if pWindow then
        margin = pWindow->ScaleX(ROW_MARGIN)
        stripW = pWindow->ScaleX(VSTRIP_WIDTH)
    end if

    for i as long = 0 to HBAR_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )
        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right  = rcRow.right - stripW
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_9) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )
    next

    ' Captions under the vertical strip.
    dim as long vw = VBAR_WIDTH
    if pWindow then vw = pWindow->ScaleX(VBAR_WIDTH)
    dim as long x0 = rc.right - stripW
    for i as long = 0 to VBAR_COUNT - 1
        dim as RECT rcCap
        SetRect( @rcCap, x0 + (i * (vw + margin \ 2)), rc.bottom - margin - (margin \ 2), _
                         x0 + (i * (vw + margin \ 2)) + vw + margin, rc.bottom - margin \ 3 )
        b.SetColors( theme.ForeColorDisabled, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_9) )
        b.PaintText( gVLabels(i), @rcCap, DT_LEFT )
    next

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Place every bar. Horizontal bars take the space between the label column and the vertical
' strip; the vertical bars run the full height of the strip.
' ========================================================================================
sub frmMain_PositionBars( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    dim as long labelW = LABEL_WIDTH
    dim as long barH   = BAR_HEIGHT
    dim as long stripW = VSTRIP_WIDTH
    dim as long vw     = VBAR_WIDTH
    if pWindow then
        margin = pWindow->ScaleX(ROW_MARGIN)
        labelW = pWindow->ScaleX(LABEL_WIDTH)
        barH   = pWindow->ScaleY(BAR_HEIGHT)
        stripW = pWindow->ScaleX(VSTRIP_WIDTH)
        vw     = pWindow->ScaleX(VBAR_WIDTH)
    end if

    dim as long x = rc.left + margin + labelW
    dim as long w = (rc.right - stripW - margin) - x
    if w < 40 then w = 40

    for i as long = 0 to HBAR_COUNT - 1
        if IsWindow(ghBar(i)) = 0 then continue for
        dim as RECT rcRow = RowRect( hwnd, i )
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - barH) \ 2)
        SetWindowPos( ghBar(i), 0, x, y, w, barH, SWP_NOZORDER )
    next

    ' The vertical strip: full height of the row block, at the right edge.
    dim as long vTop = RowRect( hwnd, 0 ).top
    dim as long vBot = rc.bottom - margin - (margin \ 2) - (margin \ 3)
    dim as long vH   = vBot - vTop
    if vH < 40 then vH = 40
    dim as long vx = rc.right - stripW
    for i as long = 0 to VBAR_COUNT - 1
        if IsWindow(ghVBar(i)) = 0 then continue for
        SetWindowPos( ghVBar(i), 0, vx + (i * (vw + margin \ 2)), vTop, vw, vH, SWP_NOZORDER )
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
    if state <> SIZE_MINIMIZED then
        frmMain_PositionBars( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
    function = 0
end function


' ========================================================================================
' Process WM_CREATE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean
    return true
end function


' ========================================================================================
' Process WM_CLOSE message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    KillTimer( hwnd, IDT_FRMMAIN_DEMO )
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
' Advance the determinate rows. THE DEMO OWNS THIS TIMER, not the control: SetPos jumps, and
' making progress look like progress is the host's job.
' ========================================================================================
private sub frmMain_DemoTick( byval hwnd as HWND )
    gDemoTick += 1

    ' A slow sweep 0..100 and back, so every determinate row is seen at both ends.
    ' NOT named "pos": POS is a FreeBASIC intrinsic (the console column), so a local of that
    ' name is a duplicated definition and the follow-on error is the baffling "function result
    ' assignment outside of the function".
    dim as long phase = gDemoTick mod 200
    dim as long nValue = phase
    if nValue > 100 then nValue = 200 - nValue

    for i as long = 0 to HBAR_COUNT - 1
        if IsWindow(ghBar(i)) = 0 then continue for
        ' Rows 4 and 5 are indeterminate -- their position means nothing, so leave it alone.
        if (i = 4) orelse (i = 5) then continue for
        ' Row 8 is disabled and row 11 is host-painted; both still take a position.
        PsProgressBar_SetPos( ghBar(i), (nValue + (i * 7)) mod 101 )
    next

    for i as long = 0 to VBAR_COUNT - 1
        if IsWindow(ghVBar(i)) = 0 then continue for
        if i = 2 then continue for     ' the indeterminate vertical one
        PsProgressBar_SetPos( ghVBar(i), (nValue + (i * 23)) mod 101 )
    next
end sub


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

        case WM_TIMER
            if wParam = IDT_FRMMAIN_DEMO then
                frmMain_DemoTick( hwnd )
                return 0
            end if

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
' Name this instance, so the console output says which bar is talking.
' ========================================================================================
private function BarName( byval hCtl as HWND ) as string
    for i as long = 0 to HBAR_COUNT - 1
        if hCtl = ghBar(i) then return str(i) & " (" & gRows(i).wszLabel & ")"
    next
    for i as long = 0 to VBAR_COUNT - 1
        if hCtl = ghVBar(i) then return "v" & str(i)
    next
    return "selftest"
end function


' ========================================================================================
' PsProgressBar_MessageCallback -- a click is REPORTED, never acted on.
'
' This is the whole interactive surface of the control: it never reacts to a click itself, so
' anything a host wants (click-to-cancel being the obvious one) is implemented from here.
' ========================================================================================
function PsProgressBar_DemoMessageCallback( byval m as PSPROGRESSBAR_MESSAGEINFO ptr ) as boolean
    select case m->uMsg
    case WM_LBUTTONUP
        dim as string s = "click         bar=" & BarName(m->hProgressBar) & _
                          "  pos=" & str( PsProgressBar_GetPos( m->hProgressBar ) )
        print s
    end select
    ' Return false to allow the control's default handling to proceed.
    return false
end function


' ========================================================================================
' Row 7's text callback. Shows that PRG_TEXT_CALLBACK owns the whole string, not a format.
' ========================================================================================
function PsProgressBar_DemoTextCallback( byval hBar as HWND, _
                                         byval nPos as long, _
                                         byval nMin as long, _
                                         byval nMax as long ) as DWSTRING
    dim as DWSTRING wszOut
    dim as string s = str(nPos) & " of " & str(nMax) & " files"
    wszOut = s
    return wszOut
end function


' ========================================================================================
' Row 12's tooltip callback. Consulted only because that bar has no authored tip text.
' ========================================================================================
function PsProgressBar_DemoTooltipCallback( byval hBar as HWND ) as DWSTRING
    dim as DWSTRING wszOut
    dim as string s = "Live tip: " & str( PsProgressBar_GetPercent( hBar ) ) & "% complete"
    wszOut = s
    return wszOut
end function


' ========================================================================================
' Row 11's own renderer, REPLACING the built-in painter.
'
' It draws a squared-off track with a chevron-notched bar, which makes the point that a
' callback owns the whole look. The control has already filled the client with BackColor, so
' only the track and the bar need drawing.
'
' NOTE WHAT IS NOT USED HERE. The frame is stroked with PaintRoundOutline, never
' PaintBorderRect -- the latter FILLS before it strokes, so used as a frame it erases
' everything underneath. That bug has now been written four separate times in this family,
' every time by copying a sibling's paint callback, which is exactly why
' PsProgressBar_CountRenderedTones is public: a host can assert it has not done it.
' ========================================================================================
sub PsProgressBar_DemoPaintCallback( byval p as PSPROGRESSBAR_PAINTINFO ptr )
    if p = 0 then exit sub
    if p->b = 0 then exit sub

    dim as RECT rcTrack = p->rcTrack

    ' Track: a flat dark groove, square corners.
    p->b->SetBackColor( BGR( 24, 27, 32) )
    p->b->PaintRect( @rcTrack )

    ' Bar: a stack of narrow slanted-looking chips, brightening toward the leading edge.
    dim as RECT rcBar = p->rcBar
    dim as long w = rcBar.right - rcBar.left
    if w > 0 then
        dim as long chip = 6
        dim as long x = rcBar.left
        while x < rcBar.right
            dim as RECT rcChip
            dim as long x2 = x + chip
            if x2 > rcBar.right then x2 = rcBar.right
            SetRect( @rcChip, x, rcBar.top, x2, rcBar.bottom )
            ' Ramp the green channel along the bar so the chips read as one run.
            dim as long t = ((x - rcBar.left) * 120) \ (w + 1)
            p->b->SetBackColor( BGR( 40, 110 + t, 90 ) )
            p->b->PaintRect( @rcChip )
            x += chip + 2
        wend
    end if

    ' Frame LAST, stroked only.
    p->b->SetPenColor( BGR( 90, 96,106) )
    p->b->PaintRoundOutline( @rcTrack, 0, 1 )
end sub


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)     = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10)    = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONTBOLD_9) = pWindow->CreateFont(GUIFONT, 9, FW_HEAVY)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 1000, 660 _
            )

    gRows( 0).wszLabel = "Solid, continuous   (percent text)"
    gRows( 1).wszLabel = "Gradient, continuous   (percent text)"
    gRows( 2).wszLabel = "Solid, BLOCKS"
    gRows( 3).wszLabel = "Gradient, BLOCKS   (one ramp, sliced)"
    gRows( 4).wszLabel = "Indeterminate -- BOUNCE   (gradient)"
    gRows( 5).wszLabel = "Indeterminate -- WRAP   (solid, slides off)"
    gRows( 6).wszLabel = "State: PAUSED   (value text)"
    gRows( 7).wszLabel = "State: ERROR   (callback text)"
    gRows( 8).wszLabel = "Disabled   (one text colour)"
    gRows( 9).wszLabel = "Reverse   (grows right-to-left)"
    gRows(10).wszLabel = "Square + border   (curvature 0)"
    gRows(11).wszLabel = "Host paint callback   (chips)"

    gVLabels(0) = "up"
    gVLabels(1) = "down"
    gVLabels(2) = "float"

    for i as long = 0 to HBAR_COUNT - 1
        ghBar(i) = PsProgressBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_HBAR_FIRST + i, PRG_HORIZONTAL )
        PsProgressBar_SetMessageCallback( ghBar(i), @PsProgressBar_DemoMessageCallback )
        PsProgressBar_SetFont( ghBar(i), ghFont(GUIFONTBOLD_9) )
        PsProgressBar_SetPos( ghBar(i), 45 )
    next

    ' Row 0 -- the defaults, plus a percentage. Stadium curvature, no border, solid accent.
    PsProgressBar_SetTextMode( ghBar(0), PRG_TEXT_PERCENT )

    ' Row 1 -- the same bar, gradient-filled. The default gradient mode is VERTICAL, which on
    ' a horizontal bar is the glossy top-to-bottom sheen rather than a left-to-right ramp.
    PsProgressBar_SetFillStyle( ghBar(1), PRG_FILL_GRADIENT )
    PsProgressBar_SetTextMode( ghBar(1), PRG_TEXT_PERCENT )

    ' Row 2 -- blocks, flat colour.
    PsProgressBar_SetSegmented( ghBar(2), true )

    ' Row 3 -- blocks AND gradient. The two settings are orthogonal; each block takes its
    ' slice of ONE ramp spanning the whole run, so they read as a perforated bar.
    PsProgressBar_SetSegmented( ghBar(3), true )
    PsProgressBar_SetFillStyle( ghBar(3), PRG_FILL_GRADIENT )
    PsProgressBar_SetGradientMode( ghBar(3), LinearGradientModeHorizontal )

    ' Row 4 -- indeterminate, bouncing. The position is now meaningless and is left alone.
    PsProgressBar_SetFillStyle( ghBar(4), PRG_FILL_GRADIENT )
    PsProgressBar_SetMode( ghBar(4), PRG_MODE_INDETERMINATE )

    ' Row 5 -- indeterminate, wrapping: the chunk slides off one end and back in the other.
    PsProgressBar_SetMarqueeStyle( ghBar(5), PRG_MARQUEE_WRAP )
    PsProgressBar_SetMarqueeChunkPercent( ghBar(5), 18 )
    PsProgressBar_SetMode( ghBar(5), PRG_MODE_INDETERMINATE )

    ' Row 6 -- PBM_SETSTATE's paused colours, with the raw value written across it.
    PsProgressBar_SetState( ghBar(6), PRG_STATE_PAUSED )
    PsProgressBar_SetTextMode( ghBar(6), PRG_TEXT_VALUE )

    ' Row 7 -- the error colours, with a host-supplied string.
    PsProgressBar_SetState( ghBar(7), PRG_STATE_ERROR )
    PsProgressBar_SetFillStyle( ghBar(7), PRG_FILL_GRADIENT )
    PsProgressBar_SetTextCallback( ghBar(7), @PsProgressBar_DemoTextCallback )
    PsProgressBar_SetTextMode( ghBar(7), PRG_TEXT_CALLBACK )

    ' Row 8 -- disabled. The split-colour text collapses to one colour here, deliberately:
    ' a greyed bar has no contrast problem to solve.
    PsProgressBar_SetTextMode( ghBar(8), PRG_TEXT_PERCENT )
    PsProgressBar_SetEnabled( ghBar(8), false )

    ' Row 9 -- reverse. Same model, mirrored growth direction.
    PsProgressBar_SetFillStyle( ghBar(9), PRG_FILL_GRADIENT )
    PsProgressBar_SetReverse( ghBar(9), true )

    ' Row 10 -- square corners and a visible track border, i.e. the classic comctl32 look.
    PsProgressBar_SetCurvature( ghBar(10), 0 )
    PsProgressBar_SetBorderThickness( ghBar(10), 1 )
    PsProgressBar_SetBarInset( ghBar(10), pWindow->ScaleY(2) )

    ' Row 11 -- the built-in painter replaced wholesale, plus both tooltip doors: an authored
    ' tip on this one, a callback on the last vertical bar.
    PsProgressBar_SetPaintCallback( ghBar(11), @PsProgressBar_DemoPaintCallback )
    PsProgressBar_SetTooltipText( ghBar(11), "Authored tooltip text (static)" )

    ' --- the vertical strip ---------------------------------------------------------------
    for i as long = 0 to VBAR_COUNT - 1
        ghVBar(i) = PsProgressBar_Create( HWND_FRMMAIN, IDC_FRMMAIN_VBAR_FIRST + i, PRG_VERTICAL )
        PsProgressBar_SetMessageCallback( ghVBar(i), @PsProgressBar_DemoMessageCallback )
        PsProgressBar_SetFillStyle( ghVBar(i), PRG_FILL_GRADIENT )
        PsProgressBar_SetPos( ghVBar(i), 60 )
    next
    ' v0 -- the default: a vertical bar grows BOTTOM-UP.
    ' v1 -- reversed, i.e. top-down.
    PsProgressBar_SetReverse( ghVBar(1), true )
    ' v2 -- indeterminate, so the bounce is seen on the other axis too.
    PsProgressBar_SetSegmented( ghVBar(2), true )
    PsProgressBar_SetMode( ghVBar(2), PRG_MODE_INDETERMINATE )
    PsProgressBar_SetTooltipCallback( ghVBar(2), @PsProgressBar_DemoTooltipCallback )

    frmMain_PositionBars( HWND_FRMMAIN )
    for i as long = 0 to HBAR_COUNT - 1
        ShowWindow( ghBar(i), SW_SHOW )
    next
    for i as long = 0 to VBAR_COUNT - 1
        ShowWindow( ghVBar(i), SW_SHOW )
    next

    if environ("PSPROGRESSBAR_SELFTEST") = "1" then PsProgressBar_RunSelfTest( HWND_FRMMAIN )

    UpdateWindow( pWindow->hWindow )
    ' A self-test run closes the window so the process exits and the suite can run
    ' unattended. Without this the demo sits in its pump and the console output stays
    ' BUFFERED -- killing the run then loses every line, the summary included.
    if environ("PSPROGRESSBAR_SELFTEST") = "1" then
        DestroyWindow( pWindow->hWindow )
        return 0
    end if

    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' The demo's own clock. Nothing in the control needs it -- see frmMain_DemoTick.
    SetTimer( HWND_FRMMAIN, IDT_FRMMAIN_DEMO, DEMO_TICK_MS, NULL )

    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        ' Processes accelerator keys for menu commands
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
            ' NOTE: there is no PsProgressBar_FilterMessage call here, and that is not an
            ' omission -- this control adds no pump obligation at all. Its tooltip is
            ' TTF_SUBCLASS and its animation is a WM_TIMER, so nothing it does needs to see
            ' the host's message loop.
            TranslateMessage @uMsg    ' Translates virtual-key messages into character messages.
            DispatchMessage @uMsg     ' Dispatches a message to a window procedure.
        end if  ' accelerators
    loop  ' message loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

end function
