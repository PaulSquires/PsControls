'    PsBufferPaint - demo harness
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"


' ========================================================================================
' A specimen sheet: every primitive the buffer exposes, drawn once so the rendering can be
' judged by eye. The rows to look at are the curved ones -- rounded rects, the ellipse and
' the diagonal rule should have smooth edges, while the square fills and the axis-aligned
' rules must stay crisp. A curve that is stair-stepped, or a 1px divider that has gone
' grey and soft, is the failure this sheet is here to make obvious.
' ========================================================================================
private sub PaintSpecimen( byval hwnd as HWND, byval b as PsBufferPaint ptr )

    dim as RECT rcAll = b->rcClient()

    b->SetBackColor( theme.BackColor )
    b->PaintClientRect()

    b->SetFont( ghFont(GUIFONT_9) )

    dim as long x0 = 24, y = 20
    dim as long cellW = 150, cellH = 44, gap = 16

    ' ---- row 1: square fills and borders -----------------------------------------------
    b->SetForeColor( theme.ForeColor )
    dim as RECT rcLabel = type( x0, y, x0 + 400, y + 18 )
    b->PaintText( "Square: PaintRect / PaintBorderRect (never antialiased)", @rcLabel, DT_LEFT )
    y += 22

    dim as RECT r1 = type( x0, y, x0 + cellW, y + cellH )
    b->SetBackColor( theme.BackColorSelect )
    b->PaintRect( @r1 )

    dim as RECT r2 = type( x0 + cellW + gap, y, x0 + cellW * 2 + gap, y + cellH )
    b->SetBackColor( theme.BackColorHot )
    b->SetPenColor( theme.FocusAccent )
    b->PaintBorderRect( @r2, 1 )

    dim as RECT r3 = type( x0 + (cellW + gap) * 2, y, x0 + cellW * 3 + gap * 2, y + cellH )
    b->SetBackColor( theme.BackColorHot )
    b->SetPenColor( theme.FocusAccent )
    b->PaintBorderRect( @r3, 3 )
    y += cellH + 28

    ' ---- row 2: rounded ------------------------------------------------------------------
    b->SetForeColor( theme.ForeColor )
    rcLabel = type( x0, y, x0 + 400, y + 18 )
    b->PaintText( "Rounded: PaintRoundRect / PaintRoundBorderRect / PaintRoundOutline", @rcLabel, DT_LEFT )
    y += 22

    dim as RECT r4 = type( x0, y, x0 + cellW, y + cellH )
    b->SetBackColor( theme.BackColorSelect )
    b->PaintRoundRect( @r4, 20 )

    dim as RECT r5 = type( x0 + cellW + gap, y, x0 + cellW * 2 + gap, y + cellH )
    b->SetBackColor( theme.BackColorHot )
    b->SetPenColor( theme.FocusAccent )
    b->PaintRoundBorderRect( @r5, 20, 2 )

    ' Outline only: fill something first, then stroke over it -- the fill must survive.
    dim as RECT r6 = type( x0 + (cellW + gap) * 2, y, x0 + cellW * 3 + gap * 2, y + cellH )
    b->SetBackColor( theme.BackColorSelect )
    b->PaintRect( @r6 )
    b->SetPenColor( theme.ForeColorSelect )
    b->PaintRoundOutline( @r6, 20, 2 )
    y += cellH + 28

    ' ---- row 3: ellipses and a pill -------------------------------------------------------
    b->SetForeColor( theme.ForeColor )
    rcLabel = type( x0, y, x0 + 400, y + 18 )
    b->PaintText( "PaintEllipse, and a pill (curvature = height)", @rcLabel, DT_LEFT )
    y += 22

    dim as RECT r7 = type( x0, y, x0 + cellH, y + cellH )          ' circle
    b->SetBackColor( theme.FocusAccent )
    b->PaintEllipse( @r7, 0 )

    dim as RECT r8 = type( x0 + cellH + gap, y, x0 + cellH * 3, y + cellH )   ' oval
    b->SetBackColor( theme.BackColorSelect )
    b->SetPenColor( theme.ForeColorSelect )
    b->PaintEllipse( @r8, 2 )

    ' A pill is just a rounded rect whose corner diameter equals its height -- both ends
    ' become exact semicircles. This is the shape PsToggle draws.
    dim as RECT r9 = type( x0 + cellH * 3 + gap * 2, y + 10, x0 + cellH * 3 + gap * 2 + 84, y + cellH - 10 )
    b->SetBackColor( theme.FocusAccent )
    b->PaintRoundRect( @r9, r9.bottom - r9.top )
    dim as RECT rKnob = type( r9.right - (r9.bottom - r9.top) + 2, r9.top + 2, r9.right - 2, r9.bottom - 2 )
    b->SetBackColor( theme.ForeColorSelect )
    b->PaintEllipse( @rKnob, 0 )
    y += cellH + 28

    ' ---- row 4: lines --------------------------------------------------------------------
    b->SetForeColor( theme.ForeColor )
    rcLabel = type( x0, y, x0 + 400, y + 18 )
    b->PaintText( "PaintLine: axis-aligned stay hard-edged, diagonals antialias", @rcLabel, DT_LEFT )
    y += 24

    b->SetPenColor( theme.Divider )
    b->PaintLine( 1, x0, y, x0 + 200, y )                       ' horizontal, 1px
    b->PaintLine( 2, x0, y + 12, x0 + 200, y + 12 )             ' horizontal, 2px
    b->PaintLine( 1, x0 + 230, y - 4, x0 + 230, y + 40 )        ' vertical
    b->SetPenColor( theme.FocusAccent )
    b->PaintLine( 2, x0 + 260, y - 4, x0 + 380, y + 40 )        ' diagonal -> antialiased
end sub


' ========================================================================================
' frmMain_WndProc
' ========================================================================================
function frmMain_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                          byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            return true

        case WM_PAINT
            dim b as PsBufferPaint
            b.BeginDoubleBuffer( hwnd )
            PaintSpecimen( hwnd, @b )
            b.EndDoubleBuffer()
            return 0

        case WM_KEYDOWN
            if wParam = VK_ESCAPE then PostMessage( hwnd, WM_CLOSE, 0, 0 )
            return 0

        case WM_DESTROY
            PostQuitMessage(0)
            return 0
    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow( APPCLASSNAME )

    ghFont(GUIFONT_9)  = pWindow->CreateFont( GUIFONT, 9,  FW_NORMAL )
    ghFont(GUIFONT_10) = pWindow->CreateFont( GUIFONT, 10, FW_NORMAL )

    HWND_FRMMAIN = pWindow->Create( hWndParent, APPNAME, @frmMain_WndProc )
    pWindow->SetClientSize( 640, 460 )
    pWindow->Center()

    ' The self-test needs a CWindow-backed HWND (PaintLine asks it for the DPI scale),
    ' so it runs here rather than from WinMain.
    if environ("PSBUFFERPAINT_SELFTEST") = "1" then PsBufferPaint_RunSelfTest( HWND_FRMMAIN )

    ShowWindow( HWND_FRMMAIN, SW_SHOW )
    UpdateWindow( HWND_FRMMAIN )

    dim as MSG uMsg
    while GetMessage( @uMsg, null, 0, 0 )
        TranslateMessage( @uMsg )
        DispatchMessage( @uMsg )
    wend

    for i as long = 0 to MAXFONTS - 1
        if ghFont(i) then DeleteObject( ghFont(i) )
    next

    delete pWindow
    return uMsg.wParam
end function
