'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "PsChart.bi"
#include once "PsTipHost.inc"


' ========================================================================================
' The one way to get from an HWND to the state. Every public function starts here, and
' every one of them tolerates a null return -- a host passing the wrong handle gets a
' no-op, not a crash.
' ========================================================================================
function GetChartPointer( byval hChart as HWND ) as PSCHART ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr( hChart )
    if pWindow = 0 then return 0
    return cast( PSCHART ptr, pWindow->UserData(0) )
end function


' ########################################################################################
'  PURE FUNCTIONS. No state, no window, no side effects -- so the self-test can assert them
'  without building a control.
' ########################################################################################

' ========================================================================================
' Snap a raw data range to gridlines a human would have chosen.
'
' The step is always 1, 2 or 5 times a power of ten. That set, and not a finer one, because
' those are the intervals people count in: 25 is a legal number and a terrible axis step.
'
' The bounds are then floored and ceiled TO THAT STEP, which is why the returned range is
' usually wider than the data. That is the point -- a top gridline exactly at the maximum
' datum puts the highest bar flush against the plot's edge with nothing above it.
' ========================================================================================
sub CHT_NiceRange( byval fRawMin as double, byval fRawMax as double, _
                   byval nTickHint as long, _
                   byref fOutMin as double, byref fOutMax as double, _
                   byref fOutStep as double )
    dim as long hint = nTickHint
    if hint < 2 then hint = 2
    if hint > 20 then hint = 20

    dim as double lo = fRawMin
    dim as double hi = fRawMax
    if hi < lo then swap lo, hi

    ' A FLAT SERIES. Every value identical is ordinary data (a metric that did not move),
    ' and dividing by a zero span would give an infinite step. Open a synthetic window around
    ' the value instead: +-10% of it, or +-1 when the value itself is zero.
    if hi <= lo then
        if abs(lo) < 0.0000001 then
            lo = -1.0 : hi = 1.0
        else
            dim as double m = abs(lo) * 0.1
            lo -= m : hi += m
        end if
    end if

    dim as double span = hi - lo
    dim as double rawStep = span / csng(hint)

    ' The power of ten at or below rawStep. int() is FLOOR in FreeBASIC (fix() is the one
    ' that truncates toward zero), which is what a negative exponent needs -- a rawStep of
    ' 0.04 must land on 0.01, not on 1.
    dim as double mag = 10.0 ^ int( log(rawStep) / log(10.0) )
    dim as double norm = rawStep / mag

    dim as double nice
    if norm <= 1.0 then
        nice = 1.0
    elseif norm <= 2.0 then
        nice = 2.0
    elseif norm <= 5.0 then
        nice = 5.0
    else
        nice = 10.0
    end if

    dim as double stepv = nice * mag
    if stepv <= 0.0 then stepv = 1.0

    fOutStep = stepv
    fOutMin  = int( lo / stepv ) * stepv
    fOutMax  = -int( -hi / stepv ) * stepv      ' ceil, expressed through int's floor
    if fOutMax <= fOutMin then fOutMax = fOutMin + stepv
end sub

' How many decimals a step of this size needs before consecutive labels stop reading the
' same. A step of 1 or more needs none; 0.2 needs one; 0.25 needs two.
'
' Counts the digits until the step becomes WHOLE, not the digits until it exceeds 1. Those
' two are not the same question and only the first one answers 0.25 correctly -- "until it
' exceeds 1" stops after one digit at 2.5 and labels 0.25 and 0.50 both as "0.3".
function CHT_AutoDecimals( byval fStep as double ) as long
    if fStep <= 0.0 then return 0
    dim as double s = abs( fStep )
    dim as long d = 0
    do while d < 8
        if abs( s - int(s + 0.5) ) < 0.000000001 then exit do
        s *= 10.0
        d += 1
    loop
    return d
end function

' ========================================================================================
' Fixed-point, with an optional thousands separator.
'
' Hand-rolled rather than handed to format(): the mask would have to be rebuilt per decimal
' count anyway, and format()'s grouping follows the user's locale, which would make the same
' chart read differently on two machines and break every assertion in the self-test.
' ========================================================================================
function CHT_FormatNumber( byval fValue as double, byval nDecimals as long, _
                           byval bThousands as boolean ) as DWSTRING
    dim as long d = nDecimals
    if d < 0 then d = 0
    if d > 8 then d = 8

    dim as boolean bNeg = (fValue < 0.0)
    dim as double v = abs( fValue )

    ' Round at the decimal count being shown, BEFORE splitting the number up. Rounding after
    ' the split lets 0.999 print as "0.100" when the carry has nowhere to go.
    dim as double scale = 10.0 ^ d
    ' int() FIRST, then the cast. clngint() ALREADY rounds to nearest, so the usual
    ' "+0.5 then truncate" idiom rounds TWICE here: 1234.5 at one decimal became 12345.5,
    ' then 12346, and printed as 1,234.6. int() is the floor this needs; the cast is only a
    ' type change after it.
    dim as longint scaled = clngint( int( v * scale + 0.5 ) )

    dim as longint ipart = scaled \ clngint(scale)
    dim as longint fpart = scaled mod clngint(scale)

    dim as string si = str( ipart )

    if bThousands andalso (len(si) > 3) then
        dim as string grouped
        dim as long n = len(si)
        for i as long = 0 to n - 1
            if (i > 0) andalso (((n - i) mod 3) = 0) then grouped += ","
            grouped += mid( si, i + 1, 1 )
        next
        si = grouped
    end if

    dim as string sOut = si
    if d > 0 then
        dim as string sf = str( fpart )
        do while len(sf) < d
            sf = "0" & sf
        loop
        sOut += "." & sf
    end if
    if bNeg andalso (scaled <> 0) then sOut = "-" & sOut

    return DWSTRING( sOut )
end function

' ========================================================================================
' Measure a string in a KNOWN font.
'
' Never "whatever the DC happens to hold": the control's own font if it has one, the stock
' GUI font otherwise. PsStatusBar's layout was once right only on the machine it was written
' on for exactly this reason. Stock objects must not be deleted, hence no cleanup of that
' branch.
'
' Returns 0x0 on any failure. Every caller treats that as "cannot measure yet" and re-arms
' the layout rather than laying out against a guess.
' ========================================================================================
sub CHT_MeasureText( byval hWin as HWND, byval hFont as HFONT, _
                     byval wszText as DWSTRING, byref nW as long, byref nH as long )
    nW = 0 : nH = 0
    if hWin = 0 then exit sub

    dim as HDC hDC = GetDC( hWin )
    if hDC = 0 then exit sub

    dim as HFONT hFontUse = hFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    ' The HEIGHT comes from the font's metrics, not from the string. A run of digits has no
    ' descender, so measuring "1200" would return a shorter line than measuring "Apples" and
    ' the two would not sit on the same baseline.
    dim as TEXTMETRICW tm
    if GetTextMetricsW( hDC, @tm ) then nH = tm.tmHeight

    dim as long nChars = len( wszText )
    if nChars > 0 then
        dim as SIZE sz
        if GetTextExtentPoint32W( hDC, wszText.vptr, nChars, @sz ) then
            nW = sz.cx
            if sz.cy > nH then nH = sz.cy
        end if
    end if

    SelectObject( hDC, hOldFont )
    ReleaseDC( hWin, hDC )
end sub


' ########################################################################################
'  INTERNALS
' ########################################################################################

' Offer a message to the host. TRUE means the host handled it and the control's own
' handling must not run.
private function PsChart_CallMessage( byval pChart as PSCHART ptr, byval hwnd as HWND, _
                                      byval uMsg as UINT, byval wParam as WPARAM, _
                                      byval lParam as LPARAM ) as boolean
    if pChart = 0 then return false
    if pChart->MessageCallback = 0 then return false

    ' A PLAIN LOCAL, never static. A form with two charts on it would otherwise have both
    ' writing one struct, and a callback re-entered from inside another would see the
    ' outer call's message.
    dim as PSCHART_MESSAGEINFO m
    m.hChart  = hwnd
    m.uMsg    = uMsg
    m.wParam  = wParam
    m.lParam  = lParam
    m.iSeries = pChart->nHotSeries
    m.iPoint  = pChart->nHotPoint
    return pChart->MessageCallback( @m )
end function

' Consume the lazy layout. Everything that reads derived geometry -- the renderer, the hit
' test, every geometry query -- goes through this first so none of them can answer from
' rects belonging to a previous size.
private sub CHT_EnsureLayout( byval pChart as PSCHART ptr )
    if pChart = 0 then exit sub
    if pChart->bLayoutDirty = false then exit sub
    pChart->bLayoutDirty = false
    pChart->LayoutChart()
end sub


' ========================================================================================
' HIT TEST. Client coordinates in, (series, point) out.
'
' Reads the SAME rects the renderer draws from, which is what makes "the tooltip named a
' different bar than the one under the cursor" impossible rather than merely unlikely.
' ========================================================================================
private function CHT_HitTest( byval pChart as PSCHART ptr, byval x as long, byval y as long, _
                              byref iSeries as long, byref iPoint as long ) as boolean
    iSeries = -1
    iPoint  = -1
    if pChart = 0 then return false

    dim as POINT pt
    pt.x = x : pt.y = y

    if pChart->nChartType = PSCHART_TYPE_PIE then
        if pChart->seriesCount <= 0 then return false
        if IsRectEmpty( @pChart->rcPie ) then return false

        const PI_ = 3.14159265358979323846
        dim as long cx = (pChart->rcPie.left + pChart->rcPie.right) \ 2
        dim as long cy = (pChart->rcPie.top + pChart->rcPie.bottom) \ 2
        dim as single radius = csng( pChart->rcPie.right - pChart->rcPie.left ) / 2.0
        if radius <= 0.0 then return false
        dim as single rInner = radius * pChart->fDoughnutRatio

        for j as long = 0 to pChart->series(0).pointCount - 1
            dim as PSCHART_POINT ptr p = @pChart->series(0).points(j)
            if p->fSweep <= 0.0 then continue for

            ' Measure from the SLICE's centre, not the pie's: an exploded wedge has moved and
            ' testing against the pie's centre would answer for the wedge that took its place.
            dim as double midRad = csng(p->fStart + p->fSweep / 2.0) * PI_ / 180.0
            dim as single scx = csng(cx) + csng(cos(midRad)) * csng(p->nExplode)
            dim as single scy = csng(cy) + csng(sin(midRad)) * csng(p->nExplode)

            dim as single dx = csng(x) - scx
            dim as single dy = csng(y) - scy
            dim as single dist = sqr( dx * dx + dy * dy )
            if dist > radius then continue for
            if dist < rInner then continue for

            ' atan2 in degrees, normalised to 0..360 so it can be compared against a sweep
            ' that may itself have started at a negative angle.
            dim as double a = atan2( dy, dx ) * 180.0 / PI_
            dim as double s = p->fStart
            do while s < 0.0
                s += 360.0
            loop
            do while s >= 360.0
                s -= 360.0
            loop
            do while a < 0.0
                a += 360.0
            loop
            dim as double rel = a - s
            if rel < 0.0 then rel += 360.0
            if rel <= p->fSweep then
                iSeries = 0
                iPoint = j
                return true
            end if
        next
        return false
    end if

    ' The axis charts. Later series are tested FIRST so the one drawn on top -- the one the
    ' user can actually see at that pixel -- is the one that answers.
    for i as long = pChart->seriesCount - 1 to 0 step -1
        if pChart->series(i).bVisible = false then continue for
        for j as long = 0 to pChart->series(i).pointCount - 1
            if PtInRect( @pChart->series(i).points(j).rc, pt ) then
                iSeries = i
                iPoint = j
                return true
            end if
        next
    next
    return false
end function


' ========================================================================================
' TOOLTIP TEXT, in priority order:
'   1. text the host authored with PsChart_SetTooltipText
'   2. the host's tooltip callback
'   3. the hot data point, described by the control
' There is no caption fallback -- a chart has no caption of its own.
' ========================================================================================
private function PsChart_ResolveTooltipText( byval pChart as PSCHART ptr ) as DWSTRING
    if pChart = 0 then return ""
    if len( pChart->wszTipText ) then return pChart->wszTipText
    if pChart->TooltipCallback then
        dim as DWSTRING wsz = pChart->TooltipCallback( pChart->hWin )
        if len(wsz) then return wsz
    end if

    dim as long iSer = pChart->nHotSeries
    dim as long iPt  = pChart->nHotPoint
    if pChart->IsValidPoint( iSer, iPt ) = false then return ""
    dim as PSCHART_POINT ptr p = @pChart->series(iSer).points(iPt)

    dim as DWSTRING wsz
    if len( p->Text ) then wsz = p->Text

    select case pChart->nChartType
    case PSCHART_TYPE_HLOC
        dim as DWSTRING v = "O " & pChart->FormatValue( p->OpenVal, CHT_FMT_TOOLTIP ) & _
                            "  H " & pChart->FormatValue( p->HighVal, CHT_FMT_TOOLTIP ) & _
                            "  L " & pChart->FormatValue( p->LowVal, CHT_FMT_TOOLTIP ) & _
                            "  C " & pChart->FormatValue( p->CloseVal, CHT_FMT_TOOLTIP )
        if len(wsz) then wsz += ": " & v else wsz = v
    case PSCHART_TYPE_PIE
        dim as double total = pChart->PieTotal()
        dim as DWSTRING v = pChart->FormatValue( p->Value, CHT_FMT_TOOLTIP )
        if total > 0.0 then
            v += " (" & wstr( clng( int( abs(p->Value) / total * 100.0 + 0.5 ) ) ) & "%)"
        end if
        if len(wsz) then wsz += ": " & v else wsz = v
    case else
        dim as DWSTRING v = pChart->FormatValue( p->Value, CHT_FMT_TOOLTIP )
        ' Name the series too when there is more than one, or a multi-series chart's tip
        ' says "4" and leaves the user to guess which line it came from.
        if (pChart->seriesCount > 1) andalso len( pChart->series(iSer).SeriesName ) then
            v = pChart->series(iSer).SeriesName & " " & v
        end if
        if len(wsz) then wsz += ": " & v else wsz = v
    end select

    return wsz
end function

' The thunk PsTipHost installs on the PsTooltip backend. On the system backend comctl32 asks
' the control's own WndProc through TTN_GETDISPINFOW instead, which is why that arm stays in
' the WndProc rather than being absorbed here.
private function PsChart_TipTextCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                          byval idx as long ) as DWSTRING
    return PsChart_ResolveTooltipText( GetChartPointer( hAttached ) )
end function


' ########################################################################################
'  RENDERING
'
'  ORDER MATTERS AND IS NOT COSMETIC. All GDI+ geometry for a region goes down before any
'  text over it, because the two APIs share one HDC and only GDI+ batches. PsBufferPaint
'  pays that cost internally (every text call flushes first), but the CLIP is the caller's
'  problem: SetClipRect sets both clips, so anything drawn while the plot clip is on must be
'  something that belongs inside the plot.
' ########################################################################################

private sub CHT_RenderColumn( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    for i as long = 0 to pChart->seriesCount - 1
        if pChart->series(i).bVisible = false then continue for
        dim as COLORREF clrSer = pChart->ResolveSeriesColor( i )
        for j as long = 0 to pChart->series(i).pointCount - 1
            dim as PSCHART_POINT ptr p = @pChart->series(i).points(j)
            ' A value of exactly zero has a zero-height rect. Drawing it would put a stray
            ' hairline on the baseline where the data says there is nothing.
            if p->rc.bottom <= p->rc.top then continue for
            dim as COLORREF clr = iif( p->bHasColor, p->PointColor, clrSer )
            b->SetBackColor( clr )
            b->PaintRect( @p->rc )
        next
    next
end sub

private sub CHT_RenderSymbol( byval b as PsBufferPaint ptr, byval nSymbol as long, _
                              byval ptc as POINT, byval nSize as long )
    dim as long r = nSize \ 2
    if r < 1 then r = 1

    select case nSymbol
    case CHT_SYM_CIRCLE
        dim as RECT rc
        rc.left = ptc.x - r : rc.top = ptc.y - r
        rc.right = ptc.x + r + 1 : rc.bottom = ptc.y + r + 1
        b->PaintEllipse( @rc )
    case CHT_SYM_SQUARE
        dim as RECT rc
        rc.left = ptc.x - r : rc.top = ptc.y - r
        rc.right = ptc.x + r + 1 : rc.bottom = ptc.y + r + 1
        b->PaintRect( @rc )
    case CHT_SYM_DIAMOND
        dim as POINT pts(0 to 3)
        pts(0).x = ptc.x     : pts(0).y = ptc.y - r
        pts(1).x = ptc.x + r : pts(1).y = ptc.y
        pts(2).x = ptc.x     : pts(2).y = ptc.y + r
        pts(3).x = ptc.x - r : pts(3).y = ptc.y
        b->PaintPolygon( @pts(0), 4 )
    case CHT_SYM_TRIANGLE
        dim as POINT pts(0 to 2)
        pts(0).x = ptc.x     : pts(0).y = ptc.y - r
        pts(1).x = ptc.x + r : pts(1).y = ptc.y + r
        pts(2).x = ptc.x - r : pts(2).y = ptc.y + r
        b->PaintPolygon( @pts(0), 3 )
    end select
end sub

private sub CHT_RenderLine( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    for i as long = 0 to pChart->seriesCount - 1
        if pChart->series(i).bVisible = false then continue for
        dim as long n = pChart->series(i).pointCount
        if n <= 0 then continue for

        dim as COLORREF clrSer = pChart->ResolveSeriesColor( i )

        redim as POINT pts( 0 to n - 1 )
        for j as long = 0 to n - 1
            pts(j) = pChart->series(i).points(j).ptPlot
        next

        b->SetPenColor( clrSer )
        if pChart->series(i).bSmooth then
            b->PaintCurve( @pts(0), n, pChart->series(i).nLineWidth )
        else
            b->PaintPolyline( @pts(0), n, pChart->series(i).nLineWidth )
        end if

        if pChart->series(i).nSymbol <> CHT_SYM_NONE then
            for j as long = 0 to n - 1
                dim as PSCHART_POINT ptr p = @pChart->series(i).points(j)
                b->SetBackColor( iif( p->bHasColor, p->PointColor, clrSer ) )
                CHT_RenderSymbol( b, pChart->series(i).nSymbol, p->ptPlot, _
                                  pChart->series(i).nSymbolSize )
            next
        end if
    next
end sub

' ----------------------------------------------------------------------------------------
' HLOC: a vertical high-to-low wick, an OPEN tick pointing LEFT and a CLOSE tick pointing
' RIGHT. That is the classic bar and the one in the reference screenshot -- not a
' candlestick, which fills the open-close body and is a different chart.
' ----------------------------------------------------------------------------------------
private sub CHT_RenderHloc( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    for i as long = 0 to pChart->seriesCount - 1
        if pChart->series(i).bVisible = false then continue for
        dim as COLORREF clrSer = pChart->ResolveSeriesColor( i )

        for j as long = 0 to pChart->series(i).pointCount - 1
            dim as PSCHART_POINT ptr p = @pChart->series(i).points(j)

            dim as COLORREF clr = clrSer
            if p->bHasColor then
                clr = p->PointColor
            elseif pChart->bHlocUpDown then
                ' Compared against the OPEN, not against the previous bar's close. Both
                ' conventions exist; this one is self-contained, so a bar means the same
                ' thing whether or not its neighbour is on screen.
                clr = iif( p->CloseVal >= p->OpenVal, pChart->colors.UpColor, pChart->colors.DownColor )
            end if
            b->SetPenColor( clr )

            dim as long cx = p->ptPlot.x
            dim as long yHi = pChart->ValueToY( p->HighVal )
            dim as long yLo = pChart->ValueToY( p->LowVal )
            if yHi > yLo then swap yHi, yLo
            ' PaintLine takes an inclusive span and refuses a degenerate one; a bar whose
            ' high equals its low still has to leave a mark.
            if yLo <= yHi then yLo = yHi + 1
            b->PaintLine( 1, cx, yHi, cx, yLo )

            dim as long tick = pChart->nHlocTick
            if tick < 1 then tick = 1
            dim as long yO = pChart->ValueToY( p->OpenVal )
            dim as long yC = pChart->ValueToY( p->CloseVal )
            b->PaintLine( 1, cx - tick, yO, cx, yO )
            b->PaintLine( 1, cx, yC, cx + tick, yC )
        next
    next
end sub

private sub CHT_RenderPie( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    if pChart->seriesCount <= 0 then exit sub
    if IsRectEmpty( @pChart->rcPie ) then exit sub

    const PI_ = 3.14159265358979323846

    for j as long = 0 to pChart->series(0).pointCount - 1
        dim as PSCHART_POINT ptr p = @pChart->series(0).points(j)
        if p->fSweep <= 0.0 then continue for

        ' The palette is enumerated PER SLICE here, not per series: on a pie the slices are
        ' the categories the colours have to distinguish, and the series is the whole pie.
        dim as COLORREF clr = iif( p->bHasColor, p->PointColor, _
                                   pChart->palette( j mod PSCHART_PALETTE_COUNT ) )

        dim as RECT rcSlice = pChart->rcPie
        if p->nExplode <> 0 then
            dim as double midRad = csng(p->fStart + p->fSweep / 2.0) * PI_ / 180.0
            OffsetRect( @rcSlice, clng( cos(midRad) * p->nExplode ), _
                                  clng( sin(midRad) * p->nExplode ) )
        end if

        b->SetBackColor( clr )
        b->SetPenColor( pChart->colors.SliceGapColor )
        if pChart->fDoughnutRatio > 0.0 then
            b->PaintPieRing( @rcSlice, pChart->fDoughnutRatio, p->fStart, p->fSweep, _
                             pChart->nPieSliceGap )
        else
            b->PaintPie( @rcSlice, p->fStart, p->fSweep, pChart->nPieSliceGap )
        end if
    next
end sub

' The pie's labels and leader lines, drawn AFTER every wedge so a leader crossing a
' neighbouring slice is not painted over by it.
private sub CHT_RenderPieLabels( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    if pChart->nPieLabelMode = CHT_PIELABEL_NONE then exit sub
    if pChart->seriesCount <= 0 then exit sub

    ' Geometry first, in one pass, then all the text in a second. The two APIs share one
    ' HDC and interleaving them costs a GDI+ flush per label.
    if pChart->nPieLabelPlace <> CHT_PIEPLACE_INSIDE then
        b->SetPenColor( pChart->colors.LeaderColor )
        for j as long = 0 to pChart->series(0).pointCount - 1
            dim as PSCHART_POINT ptr p = @pChart->series(0).points(j)
            if p->bLabelOut = false then continue for
            if IsRectEmpty( @p->rc ) then continue for
            ' Two segments: a diagonal stub from the rim out to the label's line, then a
            ' horizontal run into the text. A single diagonal straight to the text meets it
            ' at an angle and reads as a pointer at whatever the label happens to overlap.
            dim as long ly = (p->rc.top + p->rc.bottom) \ 2
            dim as long lx = iif( p->rc.left >= p->ptPlot.x, p->rc.left, p->rc.right )
            b->PaintLine( 1, p->ptPlot.x, p->ptPlot.y, lx, ly )
            b->PaintLine( 1, iif(lx < p->ptPlot.x, lx, p->ptPlot.x), ly, _
                             iif(lx < p->ptPlot.x, p->ptPlot.x, lx), ly )
        next
    end if

    b->SetFont( pChart->hLabelFont )
    for j as long = 0 to pChart->series(0).pointCount - 1
        dim as PSCHART_POINT ptr p = @pChart->series(0).points(j)
        if p->bLabelHid then continue for
        if IsRectEmpty( @p->rc ) then continue for
        dim as DWSTRING wsz = pChart->PieLabelText( j )
        if len(wsz) = 0 then continue for
        b->SetForeColor( iif( p->bLabelOut, pChart->colors.SliceLabelOutColor, _
                                            pChart->colors.SliceLabelColor ) )
        b->PaintText( wsz, @p->rc, DT_CENTER )
    next
end sub


' ----------------------------------------------------------------------------------------
' Gridlines and the two axis rules. Hairlines: PaintLine DPI-scales its width, so these ask
' for 1 and get 1 -- a gridline that thickened at 175% would start competing with the data.
' ----------------------------------------------------------------------------------------
private sub CHT_RenderGrid( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    if pChart->bGridHorz andalso (pChart->fStep > 0.0) then
        b->SetPenColor( pChart->colors.GridColor )
        dim as double v = pChart->fMin
        dim as long guard = 0
        do while (v <= pChart->fMax + pChart->fStep * 0.5) andalso (guard < 1000)
            dim as long y = pChart->ValueToY( v )
            b->PaintLine( pChart->nGridThick, pChart->rcPlot.left, y, pChart->rcPlot.right, y )
            v += pChart->fStep
            guard += 1
        loop
    end if

    if pChart->bGridVert andalso (pChart->nCatCount > 0) then
        b->SetPenColor( pChart->colors.GridColor )
        for i as long = 0 to pChart->nCatCount - 1
            dim as long x = pChart->SlotCenterX( i )
            b->PaintLine( pChart->nGridThick, x, pChart->rcPlot.top, x, pChart->rcPlot.bottom )
        next
    end if

    ' The category rule, drawn over the gridlines so it reads as the baseline rather than as
    ' one more gridline that happens to be at the bottom.
    b->SetPenColor( pChart->colors.AxisColor )
    b->PaintLine( pChart->nAxisThick, pChart->rcPlot.left, pChart->rcPlot.bottom, _
                                      pChart->rcPlot.right, pChart->rcPlot.bottom )
    ' The zero rule, but only when zero is genuinely INSIDE the plot. On an all-positive
    ' chart it coincides with the baseline and drawing it twice just darkens that one row.
    if (pChart->nOriginY > pChart->rcPlot.top) andalso (pChart->nOriginY < pChart->rcPlot.bottom) then
        b->PaintLine( pChart->nAxisThick, pChart->rcPlot.left, pChart->nOriginY, _
                                          pChart->rcPlot.right, pChart->nOriginY )
    end if
end sub

private sub CHT_RenderAxisLabels( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    b->SetFont( pChart->hLabelFont )
    b->SetForeColor( pChart->colors.LabelColor )

    ' --- value labels, right-aligned into the gutter ---
    if pChart->fStep > 0.0 then
        dim as double v = pChart->fMin
        dim as long guard = 0
        do while (v <= pChart->fMax + pChart->fStep * 0.5) andalso (guard < 1000)
            dim as long y = pChart->ValueToY( v )
            dim as RECT rc
            rc.left   = pChart->rcAxisY.left
            rc.right  = pChart->rcAxisY.right - pChart->nTickLen
            rc.top    = y - pChart->nLabelH \ 2
            rc.bottom = rc.top + pChart->nLabelH
            b->PaintText( pChart->FormatValue( v, CHT_FMT_AXIS ), @rc, DT_RIGHT )
            v += pChart->fStep
            guard += 1
        loop
    end if

    ' --- category labels, every nLabelStride'th ---
    if (pChart->nCatCount > 0) andalso (pChart->seriesCount > 0) then
        dim as long stride = pChart->nLabelStride
        if stride < 1 then stride = 1
        dim as long slot = clng( pChart->fSlotWidth )
        if slot < 1 then slot = 1
        for i as long = 0 to pChart->series(0).pointCount - 1
            if (i mod stride) <> 0 then continue for
            dim as long cx = pChart->SlotCenterX( i )
            dim as RECT rc
            ' The box is the stride's worth of slots, so a label allowed through by the skip
            ' has all the room the skip was calculated to give it.
            rc.left   = cx - (slot * stride) \ 2
            rc.right  = cx + (slot * stride) \ 2
            rc.top    = pChart->rcAxisX.top + pChart->nTickLen
            rc.bottom = rc.top + pChart->nLabelH
            b->PaintText( pChart->series(0).points(i).Text, @rc, DT_CENTER )
        next
    end if
end sub

private sub CHT_RenderLegend( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    if pChart->bLegendVisible = false then exit sub
    if IsRectEmpty( @pChart->rcLegend ) then exit sub
    if pChart->seriesCount <= 0 then exit sub

    dim as long swatch = pChart->nLabelH
    dim as boolean bVert = (pChart->nLegendPos = CHT_LEGEND_LEFT) orelse _
                           (pChart->nLegendPos = CHT_LEGEND_RIGHT)

    dim as long x = pChart->rcLegend.left
    dim as long y = pChart->rcLegend.top

    ' A pie's legend enumerates SLICES; every other chart's enumerates series.
    dim as long nEntries
    if pChart->nChartType = PSCHART_TYPE_PIE then
        nEntries = pChart->series(0).pointCount
    else
        nEntries = pChart->seriesCount
    end if

    for k as long = 0 to nEntries - 1
        dim as DWSTRING wsz
        dim as COLORREF clr
        if pChart->nChartType = PSCHART_TYPE_PIE then
            dim as PSCHART_POINT ptr p = @pChart->series(0).points(k)
            wsz = p->Text
            clr = iif( p->bHasColor, p->PointColor, pChart->palette( k mod PSCHART_PALETTE_COUNT ) )
        else
            if pChart->series(k).bVisible = false then continue for
            wsz = pChart->series(k).SeriesName
            clr = pChart->ResolveSeriesColor( k )
        end if

        dim as long tw, th
        CHT_MeasureText( pChart->hWin, pChart->hLabelFont, wsz, tw, th )

        dim as long entryW = swatch + pChart->nChromeGap + tw + pChart->nChromeGap * 2
        if (bVert = false) andalso (x + entryW > pChart->rcLegend.right) andalso _
           (x > pChart->rcLegend.left) then
            ' Out of room on this row. A horizontal legend gets ONE row (the layout reserved
            ' one row's height), so the remaining entries are dropped rather than drawn over
            ' the plot. Better a visibly short legend than a chart with text through it.
            exit for
        end if

        dim as RECT rcSw
        rcSw.left = x
        rcSw.top = y
        rcSw.right = x + swatch
        rcSw.bottom = rcSw.top + swatch
        b->SetBackColor( clr )
        b->PaintRect( @rcSw )

        dim as RECT rcTx
        rcTx.left = rcSw.right + pChart->nChromeGap
        rcTx.right = rcTx.left + tw
        rcTx.top = y
        rcTx.bottom = y + swatch
        b->SetFont( pChart->hLabelFont )
        b->SetForeColor( pChart->colors.LegendTextColor )
        b->PaintText( wsz, @rcTx, DT_LEFT )

        if bVert then
            y += swatch + pChart->nChromeGap
            if y >= pChart->rcLegend.bottom then exit for
        else
            x += entryW
        end if
    next
end sub


' ========================================================================================
' THE WHOLE RENDER.
'
' Split out of WM_PAINT deliberately, so the offscreen probes below drive this exact code
' rather than a parallel implementation. PsDatePicker once shipped a probe that passed while
' the real painter drew a flat rectangle; that cannot happen if there is only one painter.
' ========================================================================================
private sub CHT_Render( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr )
    if pChart = 0 then exit sub
    if b = 0 then exit sub

    CHT_EnsureLayout( pChart )

    b->SetBackColor( pChart->colors.BackColor )
    b->PaintClientRect()

    ' A host callback replaces EVERYTHING except the background fill above -- so a callback
    ' that only wants to add an annotation still has to draw the chart itself. That is the
    ' all-or-nothing contract stated in the .bi, and it is PsProgressBar's shape.
    if pChart->PaintCallback then
        dim as PSCHART_PAINTINFO p
        p.hChart       = pChart->hWin
        p.b            = b
        p.rcClient     = pChart->rcClient
        p.rcPlot       = pChart->rcPlot
        p.rcTitle      = pChart->rcTitle
        p.rcSubtitle   = pChart->rcSubtitle
        p.rcLegend     = pChart->rcLegend
        p.rcAxisX      = pChart->rcAxisX
        p.rcAxisY      = pChart->rcAxisY
        p.rcPie        = pChart->rcPie
        p.nChartType   = pChart->nChartType
        p.fMin         = pChart->fMin
        p.fMax         = pChart->fMax
        p.fStep        = pChart->fStep
        p.nDecimals    = pChart->nResDecimals
        p.nOriginY     = pChart->nOriginY
        p.nCatCount    = pChart->nCatCount
        p.nLabelStride = pChart->nLabelStride
        p.nHotSeries   = pChart->nHotSeries
        p.nHotPoint    = pChart->nHotPoint
        p.isEnabled    = pChart->bEnabled
        p.wszTitle     = pChart->wszTitle
        p.wszSubtitle  = pChart->wszSubtitle
        pChart->PaintCallback( @p )
        exit sub
    end if

    if IsRectEmpty( @pChart->rcPlot ) = 0 then
        if pChart->colors.PlotBackColor <> pChart->colors.BackColor then
            b->SetBackColor( pChart->colors.PlotBackColor )
            b->PaintRect( @pChart->rcPlot )
        end if
    end if

    if pChart->nChartType = PSCHART_TYPE_PIE then
        CHT_RenderPie( pChart, b )
        CHT_RenderPieLabels( pChart, b )
    else
        CHT_RenderGrid( pChart, b )

        ' The series are the only thing clipped. Everything else is positioned by layout to
        ' be inside the control already, and clipping it would only cost two flushes.
        b->SetClipRect( @pChart->rcPlot )
        select case pChart->nChartType
        case PSCHART_TYPE_COLUMN : CHT_RenderColumn( pChart, b )
        case PSCHART_TYPE_HLOC   : CHT_RenderHloc( pChart, b )
        case else                : CHT_RenderLine( pChart, b )
        end select
        b->ResetClip()

        CHT_RenderAxisLabels( pChart, b )
    end if

    CHT_RenderLegend( pChart, b )

    if len( pChart->wszTitle ) then
        b->SetFont( iif( pChart->hTitleFont <> 0, pChart->hTitleFont, pChart->hLabelFont ) )
        b->SetForeColor( pChart->colors.TitleColor )
        b->PaintText( pChart->wszTitle, @pChart->rcTitle, DT_LEFT )
    end if
    if len( pChart->wszSubtitle ) then
        b->SetFont( iif( pChart->hSubtitleFont <> 0, pChart->hSubtitleFont, pChart->hLabelFont ) )
        b->SetForeColor( pChart->colors.SubtitleColor )
        b->PaintText( pChart->wszSubtitle, @pChart->rcSubtitle, DT_LEFT )
    end if
end sub

private function PsChart_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSCHART ptr pChart = GetChartPointer( hwnd )
    if pChart = null then return 0
    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )
    CHT_Render( pChart, @b )
    b.EndDoubleBuffer()
    function = 0
end function


' ########################################################################################
'  HOT TRACKING
'
'  The chart is static -- nothing here changes what is drawn. The hot point exists so the
'  tooltip can name what is under the cursor and so a host can react through the hot-change
'  callback.
' ########################################################################################

private sub CHT_SetHot( byval pChart as PSCHART ptr, byval iSer as long, byval iPt as long )
    if pChart = 0 then exit sub
    if (pChart->nHotSeries = iSer) andalso (pChart->nHotPoint = iPt) then exit sub

    pChart->nHotSeries = iSer
    pChart->nHotPoint = iPt

    ' Move the tooltip's tool index so a tip already showing for the previous point goes
    ' down and re-resolves on the next dwell, rather than following the cursor with stale
    ' text. -1 is the whole-control tip.
    dim as long idx = iif( iPt >= 0, iPt, -1 )
    PsTipHost_SetToolIndex( @pChart->tip, idx )

    ' AFTER the state is updated, so a handler calling PsChart_GetHotPoint sees the new one.
    if pChart->HotChangeCallback then
        pChart->HotChangeCallback( pChart->hWin, iSer, iPt )
    end if
end sub

private sub CHT_StopPolling( byval pChart as PSCHART ptr )
    if pChart = 0 then exit sub
    if pChart->bPolling = false then exit sub
    KillTimer( pChart->hWin, IDT_CCHART_HOTTRACK )
    pChart->bPolling = false
end sub

private function PsChart_OnMouseMove( byval hwnd as HWND, byval x as long, byval y as long ) as LRESULT
    dim as PSCHART ptr pChart = GetChartPointer( hwnd )
    if pChart = 0 then return 0

    CHT_EnsureLayout( pChart )

    dim as long iSer, iPt
    CHT_HitTest( pChart, x, y, iSer, iPt )
    CHT_SetHot( pChart, iSer, iPt )

    if pChart->bTracking = false then
        dim as TRACKMOUSEEVENT tme
        tme.cbSize = sizeof(TRACKMOUSEEVENT)
        tme.dwFlags = TME_LEAVE
        tme.hwndTrack = hwnd
        if TrackMouseEvent( @tme ) then pChart->bTracking = true
    end if

    ' The safety net. WM_MOUSELEAVE is not reliably delivered when the cursor leaves quickly
    ' (four controls in this family have been bitten), and a stranded hot point means a
    ' tooltip that will not go away. The poll costs nothing when nothing is hot, because it
    ' only runs while something is.
    if (pChart->bPolling = false) andalso (iPt >= 0) then
        SetTimer( hwnd, IDT_CCHART_HOTTRACK, PSCHART_DEFAULT_HOTPOLL_MS, 0 )
        pChart->bPolling = true
    end if

    function = 0
end function


' ########################################################################################
'  WINDOW PROCEDURE
' ########################################################################################
function PsChart_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                          byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    dim as PSCHART ptr pChart = GetChartPointer( hwnd )

    ' The host sees every message EXCEPT the destroy pair. Offering those would let a
    ' callback suppress the cleanup that frees the state it is holding a pointer to.
    select case uMsg
    case WM_DESTROY, WM_NCDESTROY
        ' not offered
    case else
        if PsChart_CallMessage( pChart, hwnd, uMsg, wParam, lParam ) then return 0
    end select

    select case uMsg
    case WM_PAINT
        return PsChart_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase. WM_PAINT fills the whole client through the double buffer, so an
        ' erase here would only add a frame of flicker.
        return true

    case WM_SIZE
        if pChart then pChart->Refresh()
        return 0

    case WM_MOUSEMOVE
        return PsChart_OnMouseMove( hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) )

    case WM_MOUSELEAVE
        if pChart then
            pChart->bTracking = false
            CHT_SetHot( pChart, -1, -1 )
            CHT_StopPolling( pChart )
        end if
        return 0

    case WM_TIMER
        if (pChart <> 0) andalso (wParam = IDT_CCHART_HOTTRACK) then
            dim as RECT rcClient
            GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                pChart->bTracking = false
                CHT_SetHot( pChart, -1, -1 )
                CHT_StopPolling( pChart )
            elseif pChart->nHotPoint < 0 then
                ' Still inside, but over nothing. Nothing left to strand, so stop burning a
                ' timer until the next hit puts one back.
                CHT_StopPolling( pChart )
            end if
        end if
        return 0

    case WM_NOTIFY
        ' The comctl32 backend pulls its text through here rather than through PsTipHost's
        ' callback, which is why this arm stays per control.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pChart then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' Into a FIELD, not a local: comctl32 keeps the pointer and reads it after
                ' this function has returned.
                pChart->wszTipBuf = PsChart_ResolveTooltipText( pChart )
                pdi->lpszText = cast( LPWSTR, pChart->wszTipBuf.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_SETFONT
        if pChart then
            pChart->hLabelFont = cast( HFONT, wParam )
            pChart->Refresh()
        end if
        return 0

    case WM_GETFONT
        if pChart then return cast( LRESULT, pChart->hLabelFont )
        return 0

    case WM_ENABLE
        if pChart then
            pChart->bEnabled = (wParam <> 0)
            if pChart->bEnabled = false then
                CHT_SetHot( pChart, -1, -1 )
                CHT_StopPolling( pChart )
                PsTipHost_Pop( @pChart->tip )
            end if
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_GETDLGCODE
        ' A chart takes no keyboard input and must not become a tab stop. Claiming a key it
        ' does not use would also oblige it to swallow that key's WM_CHAR, or the system
        ' beeps (the family rule).
        return DLGC_STATIC

    case WM_NCDESTROY
        if pChart then
            CHT_StopPolling( pChart )
            PsTipHost_Destroy( @pChart->tip )
            delete( pChart )
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr( hwnd )
        if pWindow then delete( pWindow )
        return 0
    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ########################################################################################
'  CREATE
' ########################################################################################
function PsChart_Create( byval hWndParent as HWND, byval CtrlID as long, _
                         byval nChartType as long = PSCHART_TYPE_LINE ) as HWND
    if hWndParent = 0 then return 0

    dim pWindow as CWindow ptr = new CWindow
    if pWindow = 0 then return 0

    ' THE TRAILING 0 IS THE EX-STYLE AND MUST BE PASSED. CWindow.Create otherwise defaults it
    ' to WS_EX_CONTROLPARENT or WS_EX_WINDOWEDGE, which silently declares the control a
    ' container and gives it a border nothing asked for.
    dim as HWND hCtrl = pWindow->Create( hWndParent, "", @PsChart_WndProc, 0, 0, 0, 0, _
                        WS_CHILD or WS_VISIBLE or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, 0 )
    if hCtrl = 0 then
        delete( pWindow )
        return 0
    end if

    dim as PSCHART ptr pChart = new PSCHART
    if pChart = 0 then
        DestroyWindow( hCtrl )
        delete( pWindow )
        return 0
    end if

    pChart->hWin = hCtrl
    select case nChartType
    case PSCHART_TYPE_COLUMN, PSCHART_TYPE_HLOC, PSCHART_TYPE_PIE
        pChart->nChartType = nChartType
    case else
        pChart->nChartType = PSCHART_TYPE_LINE
    end select

    ' ONLY THE CREATE-TIME DEFAULTS ARE DPI-SCALED. Every setter afterwards takes raw pixels
    ' and the caller scales -- the family rule, so a host that computes a size from measured
    ' text is not scaled twice.
    pChart->nPadding    = pWindow->ScaleX( PSCHART_DEFAULT_PADDING )
    pChart->nChromeGap  = pWindow->ScaleX( PSCHART_DEFAULT_CHROMEGAP )
    pChart->nTickLen    = pWindow->ScaleX( PSCHART_DEFAULT_TICKLEN )
    pChart->nBarGap     = pWindow->ScaleX( PSCHART_DEFAULT_BARGAP )
    pChart->nHlocTick   = pWindow->ScaleX( PSCHART_DEFAULT_HLOCTICK )
    pChart->nHitRadius  = pWindow->ScaleX( PSCHART_DEFAULT_HITRADIUS )
    if pChart->nBarGap < 1 then pChart->nBarGap = 1
    if pChart->nHlocTick < 1 then pChart->nHlocTick = 1
    if pChart->nHitRadius < 1 then pChart->nHitRadius = 1

    ' The categorical palette. Seeded to the reference charts' family -- a blue, an orange
    ' and a green that stay distinguishable side by side -- then extended with seven more
    ' that keep their distance from those three and from each other.
    pChart->palette(0) = BGR( 86,156,214)      ' blue
    pChart->palette(1) = BGR(230,159, 80)      ' orange
    pChart->palette(2) = BGR(140,196, 90)      ' green
    pChart->palette(3) = BGR(197,134,192)      ' magenta
    pChart->palette(4) = BGR(224,196, 90)      ' yellow
    pChart->palette(5) = BGR(120,190,190)      ' teal
    pChart->palette(6) = BGR(224,120,120)      ' red
    pChart->palette(7) = BGR(150,160,200)      ' periwinkle
    pChart->palette(8) = BGR(190,150,110)      ' tan
    pChart->palette(9) = BGR(150,160,170)      ' grey

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )
    ' BEFORE PsTipHost_Ensure: the tip thunk resolves the control through this slot, and an
    ' Ensure that ran first would find a null state.
    pWindow->UserData(0) = cast( LONG_PTR, pChart )

    PsTipHost_Ensure( @pChart->tip, hCtrl, @PsChart_TipTextCallback )

    function = hCtrl
end function


' ########################################################################################
'  PUBLIC API
' ########################################################################################

function PsChart_GetChartType( byval hChart as HWND ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return -1
    return pChart->nChartType
end function

' --- data ---------------------------------------------------------------------------------
function PsChart_AddSeries( byval hChart as HWND, byval wszName as DWSTRING = "" ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return -1
    dim as long idx = pChart->AddSeries()
    if idx >= 0 then pChart->series(idx).SeriesName = wszName
    return idx
end function

function PsChart_GetSeriesCount( byval hChart as HWND ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0
    return pChart->seriesCount
end function

function PsChart_SetSeriesName( byval hChart as HWND, byval iSeries as long, byval wszName as DWSTRING ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    s->SeriesName = wszName
    pChart->Refresh()
    return true
end function

function PsChart_GetSeriesName( byval hChart as HWND, byval iSeries as long ) as DWSTRING
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return ""
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return ""
    return s->SeriesName
end function

function PsChart_SetSeriesColor( byval hChart as HWND, byval iSeries as long, byval clr as COLORREF ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    s->SeriesColor = clr
    s->bHasColor = true
    pChart->Refresh()
    return true
end function

function PsChart_ClearSeriesColor( byval hChart as HWND, byval iSeries as long ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    s->bHasColor = false
    pChart->Refresh()
    return true
end function

function PsChart_SetSeriesLineWidth( byval hChart as HWND, byval iSeries as long, byval nWidth as long ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    if nWidth < 1 then nWidth = 1
    s->nLineWidth = nWidth
    pChart->Refresh()
    return true
end function

function PsChart_SetSeriesSymbol( byval hChart as HWND, byval iSeries as long, byval nSymbol as long, byval nSize as long = PSCHART_DEFAULT_SYMBOLSIZE ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    s->nSymbol = nSymbol
    if nSize < 2 then nSize = 2
    s->nSymbolSize = nSize
    pChart->Refresh()
    return true
end function

function PsChart_SetSeriesSmooth( byval hChart as HWND, byval iSeries as long, byval bSmooth as boolean ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    s->bSmooth = bSmooth
    pChart->Refresh()
    return true
end function

function PsChart_SetSeriesVisible( byval hChart as HWND, byval iSeries as long, byval bVisible as boolean ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    s->bVisible = bVisible
    ' Hiding the series the cursor was over leaves a hot point that no longer draws.
    if (bVisible = false) andalso (pChart->nHotSeries = iSeries) then
        pChart->nHotSeries = -1
        pChart->nHotPoint = -1
    end if
    pChart->Refresh()
    return true
end function

function PsChart_IsSeriesVisible( byval hChart as HWND, byval iSeries as long ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    return s->bVisible
end function

' The one place a point is appended. Both AddPoint and AddHlocPoint funnel through it so the
' growth policy and the slot reset live once.
private function CHT_AppendPoint( byval pChart as PSCHART ptr, byval iSeries as long, _
                                  byval wszLabel as DWSTRING ) as PSCHART_POINT ptr
    if pChart = 0 then return 0
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return 0

    s->EnsurePoints( s->pointCount + 1 )
    if ubound(s->points) < s->pointCount then return 0

    dim as PSCHART_POINT ptr p = @s->points( s->pointCount )
    ' Reset the recycled slot in full. A slot reused after a ClearPoints still holds the old
    ' point's colour flag and explode offset, and inheriting either is a bug that only shows
    ' on the second population.
    with *p
        .Text = wszLabel
        .Value = 0.0 : .OpenVal = 0.0 : .HighVal = 0.0 : .LowVal = 0.0 : .CloseVal = 0.0
        .ItemData = 0
        .bHasColor = false : .PointColor = 0
        .nExplode = 0
        .fStart = 0.0 : .fSweep = 0.0
        .bLabelOut = false : .bLabelHid = false
        SetRectEmpty( @.rc )
        .ptPlot.x = 0 : .ptPlot.y = 0
    end with

    s->pointCount += 1
    return p
end function

function PsChart_AddPoint( byval hChart as HWND, byval iSeries as long, _
                           byval wszLabel as DWSTRING, byval fValue as double ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return -1
    dim as PSCHART_POINT ptr p = CHT_AppendPoint( pChart, iSeries, wszLabel )
    if p = 0 then return -1
    p->Value = fValue
    pChart->Refresh()
    return pChart->series(iSeries).pointCount - 1
end function

function PsChart_AddHlocPoint( byval hChart as HWND, byval iSeries as long, _
                               byval wszLabel as DWSTRING, byval fOpenVal as double, _
                               byval fHighVal as double, byval fLowVal as double, _
                               byval fCloseVal as double ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return -1
    dim as PSCHART_POINT ptr p = CHT_AppendPoint( pChart, iSeries, wszLabel )
    if p = 0 then return -1
    p->OpenVal  = fOpenVal
    p->HighVal  = fHighVal
    p->LowVal   = fLowVal
    p->CloseVal = fCloseVal
    ' Value carries the close, so an HLOC series handed to a tooltip or a value query has one
    ' number that means what the point is "worth" without the caller switching on the type.
    p->Value    = fCloseVal
    if p->HighVal < p->LowVal then swap p->HighVal, p->LowVal
    pChart->Refresh()
    return pChart->series(iSeries).pointCount - 1
end function

function PsChart_GetPointCount( byval hChart as HWND, byval iSeries as long ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return 0
    return s->pointCount
end function

function PsChart_SetPointValue( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval fValue as double ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return false
    p->Value = fValue
    pChart->Refresh()
    return true
end function

function PsChart_GetPointValue( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as double
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0.0
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return 0.0
    return p->Value
end function

function PsChart_SetPointLabel( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval wszLabel as DWSTRING ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return false
    p->Text = wszLabel
    pChart->Refresh()
    return true
end function

function PsChart_GetPointLabel( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as DWSTRING
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return ""
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return ""
    return p->Text
end function

function PsChart_SetPointColor( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval clr as COLORREF ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return false
    p->PointColor = clr
    p->bHasColor = true
    pChart->Refresh()
    return true
end function

function PsChart_ClearPointColor( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return false
    p->bHasColor = false
    pChart->Refresh()
    return true
end function

function PsChart_SetPointItemData( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval nData as LONG_PTR ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return false
    p->ItemData = nData
    return true                    ' payload only: nothing on screen changed, so no Refresh
end function

function PsChart_GetPointItemData( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as LONG_PTR
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return 0
    return p->ItemData
end function

function PsChart_SetPointExplode( byval hChart as HWND, byval iPoint as long, byval nOffset as long ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_POINT ptr p = pChart->GetPoint( 0, iPoint )
    if p = 0 then return false
    if nOffset < 0 then nOffset = 0
    p->nExplode = nOffset
    pChart->Refresh()
    return true
end function

function PsChart_ClearPoints( byval hChart as HWND, byval iSeries as long ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    dim as PSCHART_SERIES ptr s = pChart->GetSeries( iSeries )
    if s = 0 then return false
    ' Release the labels. The slots survive as capacity and would otherwise hold every string
    ' this series has ever shown.
    for j as long = 0 to s->pointCount - 1
        s->points(j).Text = ""
    next
    s->pointCount = 0
    if pChart->nHotSeries = iSeries then
        pChart->nHotSeries = -1
        pChart->nHotPoint = -1
    end if
    pChart->Refresh()
    return true
end function

sub PsChart_Clear( byval hChart as HWND )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->ClearAll()
end sub

sub PsChart_BeginUpdate( byval hChart as HWND )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->updateDepth += 1
end sub

sub PsChart_EndUpdate( byval hChart as HWND )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if pChart->updateDepth <= 0 then exit sub
    pChart->updateDepth -= 1
    ' Only the OUTERMOST EndUpdate repaints. A nested pair must not end its caller's batch.
    if pChart->updateDepth = 0 then pChart->Refresh()
end sub

' --- value axis ------------------------------------------------------------------------------
sub PsChart_SetAutoScale( byval hChart as HWND, byval bAuto as boolean )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->bAutoScale = bAuto
    pChart->Refresh()
end sub

function PsChart_GetAutoScale( byval hChart as HWND ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    return pChart->bAutoScale
end function

sub PsChart_SetValueRange( byval hChart as HWND, byval fLo as double, byval fHi as double )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->fUserMin = fLo
    pChart->fUserMax = fHi
    ' Turning autoscaling off here rather than making the host do it: stating a range and
    ' having it silently ignored is the worse of the two surprises.
    pChart->bAutoScale = false
    pChart->Refresh()
end sub

sub PsChart_GetValueRange( byval hChart as HWND, byref fLo as double, byref fHi as double )
    fLo = 0.0 : fHi = 0.0
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    CHT_EnsureLayout( pChart )
    fLo = pChart->fMin
    fHi = pChart->fMax
end sub

sub PsChart_SetTickCountHint( byval hChart as HWND, byval nTicks as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->nTickHint = nTicks
    pChart->Refresh()
end sub

sub PsChart_SetGridLines( byval hChart as HWND, byval bHorz as boolean, byval bVert as boolean )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->bGridHorz = bHorz
    pChart->bGridVert = bVert
    InvalidateRect( hChart, NULL, TRUE )
end sub

sub PsChart_SetValueFormat( byval hChart as HWND, byval nDecimals as long, byval bThousands as boolean )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->nDecimals = nDecimals
    pChart->bThousands = bThousands
    ' A full Refresh, not just an invalidate: the label widths change and the Y gutter is
    ' measured from them.
    pChart->Refresh()
end sub

sub PsChart_SetAxisLabelSkip( byval hChart as HWND, byval nSkip as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if nSkip < 0 then nSkip = 0
    pChart->nLabelSkip = nSkip
    pChart->Refresh()
end sub

function PsChart_GetAxisLabelSkip( byval hChart as HWND ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0
    CHT_EnsureLayout( pChart )
    return pChart->nLabelStride
end function

' --- chrome -----------------------------------------------------------------------------------
sub PsChart_SetTitle( byval hChart as HWND, byval wszText as DWSTRING )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->wszTitle = wszText
    pChart->Refresh()
end sub

function PsChart_GetTitle( byval hChart as HWND ) as DWSTRING
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return ""
    return pChart->wszTitle
end function

sub PsChart_SetSubtitle( byval hChart as HWND, byval wszText as DWSTRING )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->wszSubtitle = wszText
    pChart->Refresh()
end sub

function PsChart_GetSubtitle( byval hChart as HWND ) as DWSTRING
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return ""
    return pChart->wszSubtitle
end function

sub PsChart_SetLegendVisible( byval hChart as HWND, byval bVisible as boolean )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->bLegendVisible = bVisible
    pChart->Refresh()
end sub

function PsChart_IsLegendVisible( byval hChart as HWND ) as boolean
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    return pChart->bLegendVisible
end function

sub PsChart_SetLegendPosition( byval hChart as HWND, byval nPos as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    select case nPos
    case CHT_LEGEND_TOP, CHT_LEGEND_BOTTOM, CHT_LEGEND_LEFT, CHT_LEGEND_RIGHT
        pChart->nLegendPos = nPos
        pChart->Refresh()
    end select
end sub

sub PsChart_SetPadding( byval hChart as HWND, byval nPadding as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if nPadding < 0 then nPadding = 0
    pChart->nPadding = nPadding
    pChart->Refresh()
end sub

sub PsChart_SetFont( byval hChart as HWND, byval hFont as HFONT )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    ' BORROWED. The host owns this handle and destroys it; never DeleteObject one here.
    pChart->hLabelFont = hFont
    pChart->Refresh()
end sub

sub PsChart_SetTitleFont( byval hChart as HWND, byval hFont as HFONT )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->hTitleFont = hFont
    pChart->Refresh()
end sub

sub PsChart_SetSubtitleFont( byval hChart as HWND, byval hFont as HFONT )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->hSubtitleFont = hFont
    pChart->Refresh()
end sub

' --- column -------------------------------------------------------------------------------------
sub PsChart_SetBarGap( byval hChart as HWND, byval nGap as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if nGap < 0 then nGap = 0
    pChart->nBarGap = nGap
    pChart->Refresh()
end sub

sub PsChart_SetGroupGapPercent( byval hChart as HWND, byval nPct as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if nPct < 0 then nPct = 0
    if nPct > 90 then nPct = 90
    pChart->nGroupGapPct = nPct
    pChart->Refresh()
end sub

' --- HLOC ----------------------------------------------------------------------------------------
sub PsChart_SetHlocTickWidth( byval hChart as HWND, byval nWidth as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if nWidth < 1 then nWidth = 1
    pChart->nHlocTick = nWidth
    InvalidateRect( hChart, NULL, TRUE )
end sub

sub PsChart_SetHlocUpDownColors( byval hChart as HWND, byval bEnable as boolean )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->bHlocUpDown = bEnable
    InvalidateRect( hChart, NULL, TRUE )
end sub

' --- pie -----------------------------------------------------------------------------------------
sub PsChart_SetDoughnutRatio( byval hChart as HWND, byval fRatio as single )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    dim as single r = fRatio
    if r < 0.0 then r = 0.0
    if r > 0.9 then r = 0.9
    pChart->fDoughnutRatio = r
    pChart->Refresh()
end sub

function PsChart_GetDoughnutRatio( byval hChart as HWND ) as single
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0.0
    return pChart->fDoughnutRatio
end function

sub PsChart_SetPieLabelMode( byval hChart as HWND, byval nMode as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->nPieLabelMode = nMode
    pChart->Refresh()
end sub

sub PsChart_SetPieLabelPlacement( byval hChart as HWND, byval nPlace as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->nPieLabelPlace = nPlace
    pChart->Refresh()
end sub

sub PsChart_SetPieStartAngle( byval hChart as HWND, byval fDegrees as single )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->fPieStartAngle = fDegrees
    pChart->Refresh()
end sub

sub PsChart_SetPieSliceGap( byval hChart as HWND, byval nGap as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if nGap < 0 then nGap = 0
    pChart->nPieSliceGap = nGap
    InvalidateRect( hChart, NULL, TRUE )
end sub

' --- colours ---------------------------------------------------------------------------------------
sub PsChart_GetColors( byval hChart as HWND, byval pColors as PSCHART_COLORS ptr )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if (pChart = 0) orelse (pColors = 0) then exit sub
    *pColors = pChart->colors
end sub

sub PsChart_SetColors( byval hChart as HWND, byval pColors as PSCHART_COLORS ptr )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if (pChart = 0) orelse (pColors = 0) then exit sub
    pChart->colors = *pColors
    InvalidateRect( hChart, NULL, TRUE )
end sub

sub PsChart_SetPaletteColor( byval hChart as HWND, byval idx as long, byval clr as COLORREF )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    if (idx < 0) orelse (idx >= PSCHART_PALETTE_COUNT) then exit sub
    pChart->palette(idx) = clr
    InvalidateRect( hChart, NULL, TRUE )
end sub

function PsChart_GetPaletteColor( byval hChart as HWND, byval idx as long ) as COLORREF
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0
    if (idx < 0) orelse (idx >= PSCHART_PALETTE_COUNT) then return 0
    return pChart->palette(idx)
end function

' --- query -----------------------------------------------------------------------------------------
function PsChart_HitTest( byval hChart as HWND, byval x as long, byval y as long, _
                          byref iSeries as long, byref iPoint as long ) as boolean
    iSeries = -1 : iPoint = -1
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    CHT_EnsureLayout( pChart )
    return CHT_HitTest( pChart, x, y, iSeries, iPoint )
end function

function PsChart_GetPlotRect( byval hChart as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    CHT_EnsureLayout( pChart )
    rc = pChart->rcPlot
    return true
end function

function PsChart_GetPointRect( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    CHT_EnsureLayout( pChart )
    dim as PSCHART_POINT ptr p = pChart->GetPoint( iSeries, iPoint )
    if p = 0 then return false
    rc = p->rc
    return true
end function

function PsChart_ValueToY( byval hChart as HWND, byval fValue as double ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0
    CHT_EnsureLayout( pChart )
    return pChart->ValueToY( fValue )
end function

function PsChart_YToValue( byval hChart as HWND, byval y as long ) as double
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0.0
    CHT_EnsureLayout( pChart )
    return pChart->YToValue( y )
end function

function PsChart_GetHotPoint( byval hChart as HWND, byref iSeries as long, byref iPoint as long ) as boolean
    iSeries = -1 : iPoint = -1
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return false
    iSeries = pChart->nHotSeries
    iPoint = pChart->nHotPoint
    return (iPoint >= 0)
end function

' --- tooltips ----------------------------------------------------------------------------------------
sub PsChart_SetTooltipText( byval hChart as HWND, byval wszText as DWSTRING )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->wszTipText = wszText
end sub

function PsChart_GetTooltipText( byval hChart as HWND ) as DWSTRING
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return ""
    return pChart->wszTipText
end function

sub PsChart_SetTooltipMode( byval hChart as HWND, byval nMode as long )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    PsTipHost_SetMode( @pChart->tip, nMode )
end sub

function PsChart_GetTooltipMode( byval hChart as HWND ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pChart->tip )
end function

' --- callbacks -----------------------------------------------------------------------------------------
sub PsChart_SetPaintCallback( byval hChart as HWND, byval userfunc as CHT_PaintCallbackSub )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->PaintCallback = userfunc
    InvalidateRect( hChart, NULL, TRUE )
end sub

sub PsChart_SetMessageCallback( byval hChart as HWND, byval userfunc as CHT_MessageCallbackFunc )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->MessageCallback = userfunc
end sub

sub PsChart_SetTooltipCallback( byval hChart as HWND, byval userfunc as CHT_TooltipCallbackFunc )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->TooltipCallback = userfunc
end sub

sub PsChart_SetValueLabelCallback( byval hChart as HWND, byval userfunc as CHT_ValueLabelCallbackFunc )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->ValueLabelCallback = userfunc
    pChart->Refresh()
end sub

sub PsChart_SetHotChangeCallback( byval hChart as HWND, byval userfunc as CHT_HotChangeCallbackSub )
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then exit sub
    pChart->HotChangeCallback = userfunc
end sub


' ########################################################################################
'  RENDER PROBES
'
'  Both drive CHT_Render -- the SAME function WM_PAINT calls -- into an offscreen buffer and
'  read the result back. Anything that measured a parallel implementation would be measuring
'  the wrong thing, and that failure has shipped in this family before.
' ########################################################################################

' Render offscreen and hand the caller the memory DC to read. Returns false if anything in
' the chain failed, in which case nothing was drawn and nothing should be measured.
private function CHT_RenderOffscreen( byval pChart as PSCHART ptr, byval b as PsBufferPaint ptr, _
                                      byref rcOut as RECT ) as boolean
    if (pChart = 0) orelse (b = 0) then return false
    GetClientRect( pChart->hWin, @rcOut )
    if (rcOut.right <= rcOut.left) orelse (rcOut.bottom <= rcOut.top) then return false

    dim as HDC hScreen = GetDC( pChart->hWin )
    if hScreen = 0 then return false
    b->BeginDoubleBuffer( pChart->hWin, hScreen, rcOut )
    CHT_Render( pChart, b )
    ' Deliberately NOT EndDoubleBuffer: that would blit the buffer to the screen and free it.
    ' The caller reads the surface first through getMemDC and lets the destructor clean up.
    ReleaseDC( pChart->hWin, hScreen )
    return true
end function

function PsChart_CountRenderedTones( byval hChart as HWND ) as long
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0

    dim as PsBufferPaint b
    dim as RECT rc
    if CHT_RenderOffscreen( pChart, @b, rc ) = false then return 0
    dim as HDC hdc = b.getMemDC()
    if hdc = 0 then return 0

    ' A small fixed sample grid rather than every pixel: the question this answers is "did
    ' anything actually get drawn", and a full scan of a 700x400 surface is 280,000 GetPixel
    ' calls for the same answer.
    dim as COLORREF seen( 0 to 63 )
    dim as long nSeen = 0
    for gy as long = 0 to 31
        for gx as long = 0 to 31
            dim as long x = rc.left + (rc.right - rc.left) * gx \ 32
            dim as long y = rc.top + (rc.bottom - rc.top) * gy \ 32
            dim as COLORREF c = GetPixel( hdc, x, y )
            dim as boolean bFound = false
            for k as long = 0 to nSeen - 1
                if seen(k) = c then bFound = true : exit for
            next
            if (bFound = false) andalso (nSeen < 64) then
                seen(nSeen) = c
                nSeen += 1
            end if
        next
    next
    return nSeen
end function

function PsChart_HashRenderedPart( byval hChart as HWND ) as ulong
    dim as PSCHART ptr pChart = GetChartPointer( hChart )
    if pChart = 0 then return 0

    dim as PsBufferPaint b
    dim as RECT rc
    if CHT_RenderOffscreen( pChart, @b, rc ) = false then return 0
    dim as HDC hdc = b.getMemDC()
    if hdc = 0 then return 0

    ' FNV-1a over the same sample grid. Two renders that differ anywhere in the grid produce
    ' different hashes; identical ones produce the same. That is all a regression check needs.
    dim as ulong h = &h811C9DC5
    for gy as long = 0 to 31
        for gx as long = 0 to 31
            dim as long x = rc.left + (rc.right - rc.left) * gx \ 32
            dim as long y = rc.top + (rc.bottom - rc.top) * gy \ 32
            dim as ulong c = GetPixel( hdc, x, y )
            for shift as long = 0 to 24 step 8
                h = h xor ((c shr shift) and &hFF)
                h *= 16777619
            next
        next
    next
    return h
end function


#include once "PsChart_SelfTest.inc"
