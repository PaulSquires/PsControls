'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"

dim shared gCaptions( 0 to CHART_COUNT - 1 ) as DWSTRING

#define CELL_COLS        4
#define CELL_ROWS        2
#define SHEET_MARGIN    14
#define CAPTION_HEIGHT  22
#define CELL_GAP        14


' ========================================================================================
' The cell a chart occupies, and the caption strip above it. Computed from the client rect
' every time rather than cached: the form is resizable and the controls have no opinion
' about their own placement (the family rule -- the host positions, the control draws).
' ========================================================================================
private function CellRect( byval hwnd as HWND, byval idx as long ) as RECT
    dim as RECT rc, rcCell
    GetClientRect( hwnd, @rc )

    dim as long margin = SHEET_MARGIN, gap = CELL_GAP
    dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )
    if pWindow then
        margin = pWindow->ScaleX( SHEET_MARGIN )
        gap    = pWindow->ScaleX( CELL_GAP )
    end if

    dim as long availW = (rc.right - rc.left) - margin * 2 - gap * (CELL_COLS - 1)
    dim as long availH = (rc.bottom - rc.top) - margin * 2 - gap * (CELL_ROWS - 1)
    if availW < CELL_COLS then availW = CELL_COLS
    if availH < CELL_ROWS then availH = CELL_ROWS

    dim as long cw = availW \ CELL_COLS
    dim as long chh = availH \ CELL_ROWS
    dim as long col = idx mod CELL_COLS
    dim as long row = idx \ CELL_COLS

    rcCell.left   = rc.left + margin + col * (cw + gap)
    rcCell.top    = rc.top  + margin + row * (chh + gap)
    rcCell.right  = rcCell.left + cw
    rcCell.bottom = rcCell.top + chh
    return rcCell
end function

private function CaptionRect( byval hwnd as HWND, byval idx as long ) as RECT
    dim as RECT rc = CellRect( hwnd, idx )
    dim as long h = CAPTION_HEIGHT
    dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )
    if pWindow then h = pWindow->ScaleY( CAPTION_HEIGHT )
    rc.bottom = rc.top + h
    return rc
end function

private function ChartRect( byval hwnd as HWND, byval idx as long ) as RECT
    dim as RECT rc = CellRect( hwnd, idx )
    dim as long h = CAPTION_HEIGHT
    dim pWindow as CWindow ptr = AfxCWindowPtr( hwnd )
    if pWindow then h = pWindow->ScaleY( CAPTION_HEIGHT )
    rc.top += h
    return rc
end function


' The form draws its own captions; each chart paints its own cell. There is no container
' window between them.
private function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    b.SetFont( ghFont(GUIFONTBOLD_9) )
    b.SetColors( theme.ForeColor, theme.BackColor )
    for i as long = 0 to CHART_COUNT - 1
        dim as RECT rcCap = CaptionRect( hwnd, i )
        b.PaintText( gCaptions(i), @rcCap, DT_LEFT )
    next

    b.EndDoubleBuffer()
    function = 0
end function

private sub frmMain_PositionCharts( byval hwnd as HWND )
    for i as long = 0 to CHART_COUNT - 1
        if ghChart(i) = 0 then continue for
        dim as RECT rc = ChartRect( hwnd, i )
        SetWindowPos( ghChart(i), 0, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, _
                      SWP_NOZORDER )
    next
end sub

private function frmMain_OnSize( byval hwnd as HWND, byval state as UINT, _
                                 byval cx as long, byval cy as long ) as LRESULT
    if state <> SIZE_MINIMIZED then
        frmMain_PositionCharts( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
    function = 0
end function

private function frmMain_OnEraseBkgnd( byval hwnd as HWND, byval hdc as HDC ) as LRESULT
    function = true      ' OnPaint fills the whole client through the buffer
end function

private function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow( hwnd )
    function = 0
end function

private function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage( 0 )
    function = 0
end function

private function frmMain_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        HANDLE_MSG (hwnd, WM_PAINT,      frmMain_OnPaint)
        HANDLE_MSG (hwnd, WM_SIZE,       frmMain_OnSize)
        HANDLE_MSG (hwnd, WM_ERASEBKGND, frmMain_OnEraseBkgnd)
        HANDLE_MSG (hwnd, WM_CLOSE,      frmMain_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,    frmMain_OnDestroy)
    end select
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' The light palette the reference screenshots use. Applied per chart through the ordinary
' read-modify-write pair, which is the only supported way to theme one.
' ========================================================================================
private sub ApplyLightTheme( byval hChart as HWND )
    dim as PSCHART_COLORS clrs
    PsChart_GetColors( hChart, @clrs )
    clrs.BackColor          = BGR(255,255,255)
    clrs.PlotBackColor      = BGR(255,255,255)
    clrs.GridColor          = BGR( 60, 60, 60)
    clrs.AxisColor          = BGR(190,190,190)
    clrs.LabelColor         = BGR(140,140,140)
    clrs.TitleColor         = BGR( 30, 30, 30)
    clrs.SubtitleColor      = BGR(110,110,110)
    clrs.LegendTextColor    = BGR( 70, 70, 70)
    clrs.SliceLabelColor    = BGR(110,110,110)
    clrs.SliceLabelOutColor = BGR( 70, 70, 70)
    clrs.LeaderColor        = BGR(170,170,170)
    clrs.SliceGapColor      = BGR(255,255,255)
    PsChart_SetColors( hChart, @clrs )
end sub

' The five fruit categories and three series of the reference line/column charts.
private sub LoadFruitSeries( byval hChart as HWND )
    dim as DWSTRING cats(0 to 4) = { "Oranges", "Apples", "Pears", "Bananas", "Pineapples" }
    dim as double vals(0 to 2, 0 to 4) = { _
            { 1, 4, 5, 4, 4 }, _
            { 7, 3, 8, 5, 5 }, _
            { 5, 5, 1, 5, 1 } }
    dim as DWSTRING names(0 to 2) = { "Series 1", "Series 2", "Series 3" }

    ' Bracketed: the control lays out once and repaints once instead of fifteen times.
    PsChart_BeginUpdate( hChart )
    for i as long = 0 to 2
        dim as long s = PsChart_AddSeries( hChart, names(i) )
        for j as long = 0 to 4
            PsChart_AddPoint( hChart, s, cats(j), vals(i, j) )
        next
    next
    PsChart_EndUpdate( hChart )
end sub

' Sixty dated bars, generated rather than typed out. The point of this cell is the axis
' behaviour at that density -- the label auto-skip and the value gutter -- not the prices.
private sub LoadHlocSeries( byval hChart as HWND )
    dim as DWSTRING months(0 to 2) = { "Jan", "Feb", "Mar" }
    PsChart_BeginUpdate( hChart )
    dim as long s = PsChart_AddSeries( hChart, "Index" )
    ' Not named "base": BASE is a FreeBASIC keyword (the constructor-chaining one).
    dim as double startPrice = 1030.0
    for i as long = 0 to 59
        ' A deterministic wobble: the demo must render the same picture every run, or the
        ' screenshot in the README stops matching what a reader sees.
        dim as double drift = sin( i / 7.0 ) * 55.0 + sin( i / 2.3 ) * 12.0
        dim as double o = startPrice + drift
        dim as double c = o + sin( i / 1.7 ) * 14.0
        dim as double hi = iif( o > c, o, c ) + 9.0 + (i mod 5) * 2.0
        dim as double lo = iif( o < c, o, c ) - 9.0 - (i mod 3) * 3.0
        dim as DWSTRING lbl = months(i \ 20) & "-" & wstr( (i mod 20) + 1 )
        PsChart_AddHlocPoint( hChart, s, lbl, o, hi, lo, c )
    next
    PsChart_EndUpdate( hChart )
end sub

private sub LoadPieSeries( byval hChart as HWND )
    dim as DWSTRING cats(0 to 8) = { "US", "Germany", "UK", "Japan", "Italy", _
                                     "France", "Canada", "Russia", "China" }
    dim as double vals(0 to 8) = { 4, 4, 1, 4, 4, 1, 1, 2, 2 }
    PsChart_BeginUpdate( hChart )
    dim as long s = PsChart_AddSeries( hChart, "Countries" )
    for j as long = 0 to 8
        PsChart_AddPoint( hChart, s, cats(j), vals(j) )
    next
    PsChart_EndUpdate( hChart )
end sub


' ========================================================================================
' The specimen for the three additive options: a dashed forecast, an area fill under the
' actual data, and a band showing the forecast's uncertainty RANGE.
'
' THE FORECAST IS THE SAME COLOUR AS THE ACTUAL DATA, on purpose. It is the same quantity,
' and giving it a second colour would say it is a different one; the dash is what separates
' them. That is the whole reason PsChart_SetSeriesLineStyle exists.
'
' Series order is the draw order, so the band's two edges go in first, the forecast next and
' the actual line last -- where the forecast overlays the actual for the historical months,
' the solid line is the one on top.
' ========================================================================================
private sub LoadForecastSeries( byval hChart as HWND, byval pWindow as CWindow ptr )
    dim as DWSTRING cats(0 to 11) = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", _
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }
    ' Through Jul the three projections agree with the record, so the band has no width there
    ' and opens only where the data stops -- which is the picture a fan chart exists to draw.
    dim as double act(0 to 6)  = { 100, 108, 104, 115, 121, 118, 127 }
    dim as double fc(0 to 11)  = { 100, 108, 104, 115, 121, 118, 127, 133, 138, 146, 151, 159 }
    dim as double lo(0 to 11)  = { 100, 108, 104, 115, 121, 118, 127, 127, 129, 133, 135, 140 }
    dim as double hi(0 to 11)  = { 100, 108, 104, 115, 121, 118, 127, 139, 148, 159, 168, 180 }

    dim as COLORREF clrData = BGR( 46,120,200)
    dim as COLORREF clrEdge = BGR(150,175,205)

    PsChart_BeginUpdate( hChart )
    dim as long sLo = PsChart_AddSeries( hChart, "Low" )
    dim as long sHi = PsChart_AddSeries( hChart, "High" )
    dim as long sFc = PsChart_AddSeries( hChart, "Forecast" )
    dim as long sAc = PsChart_AddSeries( hChart, "Actual" )
    for j as long = 0 to 11
        PsChart_AddPoint( hChart, sLo, cats(j), lo(j) )
        PsChart_AddPoint( hChart, sHi, cats(j), hi(j) )
        PsChart_AddPoint( hChart, sFc, cats(j), fc(j) )
        if j <= 6 then PsChart_AddPoint( hChart, sAc, cats(j), act(j) )
    next
    PsChart_EndUpdate( hChart )

    ' The band's own edges: a hairline dotted rule each, so the range has a boundary without
    ' a second solid line competing with the forecast for attention.
    PsChart_SetSeriesColor( hChart, sLo, clrEdge )
    PsChart_SetSeriesColor( hChart, sHi, clrEdge )
    PsChart_SetSeriesLineStyle( hChart, sLo, CHT_LINE_DOT )
    PsChart_SetSeriesLineStyle( hChart, sHi, CHT_LINE_DOT )
    PsChart_SetSeriesLineWidth( hChart, sLo, pWindow->ScaleX(1) )
    PsChart_SetSeriesLineWidth( hChart, sHi, pWindow->ScaleX(1) )

    PsChart_SetSeriesColor( hChart, sFc, clrData )
    PsChart_SetSeriesLineStyle( hChart, sFc, CHT_LINE_DASH )
    PsChart_SetSeriesLineWidth( hChart, sFc, pWindow->ScaleX(2) )

    PsChart_SetSeriesColor( hChart, sAc, clrData )
    PsChart_SetSeriesLineWidth( hChart, sAc, pWindow->ScaleX(2) )
    ' Under the record only, and translucent: an opaque fill would hide the band's lower half
    ' and the gridlines with it.
    PsChart_SetSeriesFill( hChart, sAc, clrData, 60 )

    PsChart_SetSeriesBand( hChart, sLo, sHi, clrData, 46 )
end sub


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow( APPCLASSNAME )

    ghFont(GUIFONT_9)     = pWindow->CreateFont( GUIFONT, 9, FW_NORMAL )
    ghFont(GUIFONT_10)    = pWindow->CreateFont( GUIFONT, 10, FW_NORMAL )
    ghFont(GUIFONTBOLD_9) = pWindow->CreateFont( GUIFONT, 9, FW_HEAVY )
    ghFont(GUIFONTBOLD_12)= pWindow->CreateFont( GUIFONT, 12, FW_HEAVY )

    ' Wider than it was: the sheet grew a fourth column when the forecast cell was added, and
    ' at the old width every chart would have lost a quarter of its plot to make room.
    HWND_FRMMAIN = pWindow->Create( null, APPNAME, @frmMain_WndProc, 60, 60, 1760, 860 )

    gCaptions(CHART_LINE)     = "Line -- three series, straight segments"
    gCaptions(CHART_COLUMN)   = "Column -- three series, grouped"
    gCaptions(CHART_HLOC)     = "HLOC -- 60 bars, labels auto-skipped"
    gCaptions(CHART_PIE)      = "Pie -- value labels inside the wedges"
    gCaptions(CHART_DOUGHNUT) = "Doughnut -- exploded slice, name + percent on leaders"
    gCaptions(CHART_NEGATIVE) = "Column -- negative values below the origin"
    gCaptions(CHART_FORECAST) = "Line -- dashed forecast, area fill, uncertainty band"

    ' --- every chart: create, font, theme -------------------------------------------------
    dim as long types(0 to CHART_COUNT - 1) = { _
            PSCHART_TYPE_LINE, PSCHART_TYPE_COLUMN, PSCHART_TYPE_HLOC, _
            PSCHART_TYPE_PIE,  PSCHART_TYPE_PIE,    PSCHART_TYPE_COLUMN, _
            PSCHART_TYPE_LINE }

    for i as long = 0 to CHART_COUNT - 1
        ghChart(i) = PsChart_Create( HWND_FRMMAIN, IDC_FRMMAIN_CHART_FIRST + i, types(i) )
        PsChart_SetFont( ghChart(i), ghFont(GUIFONT_9) )
        ApplyLightTheme( ghChart(i) )
    next

    ' --- 0: the reference line chart ------------------------------------------------------
    LoadFruitSeries( ghChart(CHART_LINE) )
    PsChart_SetLegendVisible( ghChart(CHART_LINE), true )

    ' --- 1: the reference column chart ----------------------------------------------------
    ' Eight ticks rather than the default five, so the axis steps by 1 and matches the
    ' reference's 0..8 gridlines instead of stepping by 2.
    LoadFruitSeries( ghChart(CHART_COLUMN) )
    PsChart_SetTickCountHint( ghChart(CHART_COLUMN), 8 )
    PsChart_SetLegendVisible( ghChart(CHART_COLUMN), true )

    ' --- 2: the reference HLOC chart ------------------------------------------------------
    LoadHlocSeries( ghChart(CHART_HLOC) )
    ' One colour for the whole series, as the reference has it. Flip this to true for the
    ' green/red up-down pair.
    PsChart_SetHlocUpDownColors( ghChart(CHART_HLOC), false )
    PsChart_SetSeriesColor( ghChart(CHART_HLOC), 0, BGR(120,175,215) )

    ' --- 3: the reference pie -------------------------------------------------------------
    LoadPieSeries( ghChart(CHART_PIE) )
    PsChart_SetPieLabelMode( ghChart(CHART_PIE), CHT_PIELABEL_VALUE )
    PsChart_SetPieLabelPlacement( ghChart(CHART_PIE), CHT_PIEPLACE_INSIDE )
    ' The white separator between wedges in the reference is a stroke in the background
    ' colour, three pixels wide.
    PsChart_SetPieSliceGap( ghChart(CHART_PIE), 3 )

    ' --- 4: doughnut, exploded, labels on leaders -----------------------------------------
    LoadPieSeries( ghChart(CHART_DOUGHNUT) )
    PsChart_SetDoughnutRatio( ghChart(CHART_DOUGHNUT), 0.45 )
    PsChart_SetPieLabelMode( ghChart(CHART_DOUGHNUT), CHT_PIELABEL_NAME_PERCENT )
    PsChart_SetPieLabelPlacement( ghChart(CHART_DOUGHNUT), CHT_PIEPLACE_OUTSIDE )
    PsChart_SetPieSliceGap( ghChart(CHART_DOUGHNUT), 2 )
    ' One slice pulled out. The layout reserves room for the largest offset before it cuts
    ' the pie's square, so this shrinks the pie rather than running off the control.
    PsChart_SetPointExplode( ghChart(CHART_DOUGHNUT), 0, pWindow->ScaleX(14) )

    ' --- 5: negatives, plus the symbol and spline options ---------------------------------
    scope
        dim as HWND h = ghChart(CHART_NEGATIVE)
        dim as DWSTRING cats(0 to 5) = { "Q1", "Q2", "Q3", "Q4", "Q5", "Q6" }
        dim as double a(0 to 5) = {  4,  6, -3,  2, -5,  7 }
        dim as double bvals(0 to 5) = {  2, -4,  5, -1,  3, -2 }
        PsChart_BeginUpdate( h )
        dim as long s0 = PsChart_AddSeries( h, "Gain" )
        dim as long s1 = PsChart_AddSeries( h, "Loss" )
        for j as long = 0 to 5
            PsChart_AddPoint( h, s0, cats(j), a(j) )
            PsChart_AddPoint( h, s1, cats(j), bvals(j) )
        next
        PsChart_EndUpdate( h )
        PsChart_SetLegendVisible( h, true )
        PsChart_SetLegendPosition( h, CHT_LEGEND_TOP )
    end scope

    ' --- 6: the dash / fill / band specimen -----------------------------------------------
    LoadForecastSeries( ghChart(CHART_FORECAST), pWindow )
    PsChart_SetLegendVisible( ghChart(CHART_FORECAST), true )

    ' The line cell doubles as the symbol/spline specimen: series 3 is smoothed and carries
    ' circle markers, so both options are visible against two plain neighbours.
    PsChart_SetSeriesSmooth( ghChart(CHART_LINE), 2, true )
    PsChart_SetSeriesSymbol( ghChart(CHART_LINE), 2, CHT_SYM_CIRCLE, pWindow->ScaleX(7) )

    frmMain_PositionCharts( HWND_FRMMAIN )

    ' Geometry assertions, opt-in. Runs against the real form, so the charts it builds are
    ' created on a DPI-aware window exactly as the visible ones are.
    if environ("PSCHART_SELFTEST") = "1" then PsChart_RunSelfTest( HWND_FRMMAIN )

    ' TEMPORARY regression probe -- removed before commit.
    if environ("PSCHART_DEMOHASH") = "1" then
        for i as long = 0 to CHART_COUNT - 1
            SetWindowPos( ghChart(i), 0, 0, 0, 420, 380, SWP_NOZORDER or SWP_NOACTIVATE or SWP_NOMOVE )
            dim as string sh = "DEMOHASH " & str(i) & " = " & hex( PsChart_HashRenderedPart( ghChart(i) ) )
            print sh
        next
        frmMain_PositionCharts( HWND_FRMMAIN )
        PostQuitMessage( 0 )
    end if

    ShowWindow( HWND_FRMMAIN, SW_SHOW )
    UpdateWindow( HWND_FRMMAIN )

    dim as MSG uMsg
    do while GetMessage( @uMsg, null, 0, 0 )
        if uMsg.message = WM_QUIT then exit do
        if (pWindow->AccelHandle = 0) orelse _
           (TranslateAccelerator( pWindow->hWindow, pWindow->AccelHandle, @uMsg ) = 0) then
            if IsDialogMessage( pWindow->hWindow, @uMsg ) = 0 then
                TranslateMessage @uMsg
                DispatchMessage @uMsg
            end if
        end if
    loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject( ghFont(i) )
    next
end function
