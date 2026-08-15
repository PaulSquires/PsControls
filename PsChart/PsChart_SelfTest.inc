'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ########################################################################################
'  SELF-TEST
'
'  GEOMETRY IS ASSERTED, NOT EYEBALLED. Every number below is either a pure-function result
'  or a rect the layout produced, compared against what the arithmetic says it must be.
'
'  Split into its own file, as PsColorPicker's is, so the shipping control is not carrying
'  four hundred lines of test harness through every host that includes it.
'
'  Needs a real parent HWND and a DPI-AWARE process: the manifest's <dpiAware>true</dpiAware>
'  is load-bearing, because without it every scaled assertion passes for the wrong reason on
'  a scaled display.
'
'  Output goes to the console with print. print on a DWSTRING interleaves nulls, so every
'  line is built as a "dim as string" first.
' ########################################################################################

dim shared as long gCHT_TestCount
dim shared as long gCHT_FailCount

private sub CHTTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gCHT_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gCHT_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function CHTTest_Near( byval a as double, byval b as double, byval tol as double ) as boolean
    return (abs(a - b) <= tol)
end function

private function CHTTest_RectStr( byref rc as RECT ) as string
    return "(" & str(rc.left) & "," & str(rc.top) & ")-(" & str(rc.right) & "," & str(rc.bottom) & ")"
end function

' A chart sized to a KNOWN client rect. SetWindowPos on this WS_CHILD sets the window rect,
' and with no border on this control that is also the client rect.
private function CHTTest_MakeChart( byval hParent as HWND, byval nType as long, _
                                    byval w as long, byval h as long ) as HWND
    dim as HWND hCtrl = PsChart_Create( hParent, 9000, nType )
    if hCtrl = 0 then return 0
    ' Hidden BEFORE it is sized: the self-test must not flash six charts across the host form.
    ShowWindow( hCtrl, SW_HIDE )
    SetWindowPos( hCtrl, 0, 0, 0, w, h, SWP_NOZORDER or SWP_NOACTIVATE )
    return hCtrl
end function


function PsChart_RunSelfTest( byval hWndParent as HWND ) as long
    gCHT_TestCount = 0
    gCHT_FailCount = 0
    print "===== PsChart self-test ====="

    ' --- CHT_NiceRange --------------------------------------------------------------------
    scope
        dim as double lo, hi, st

        CHT_NiceRange( 0.0, 8.0, 5, lo, hi, st )
        CHTTest_Check( "NiceRange: 0..8 with 5 ticks steps by 2", _
            CHTTest_Near(st,2.0,0.0001) andalso CHTTest_Near(lo,0.0,0.0001) andalso CHTTest_Near(hi,8.0,0.0001), _
            "min=" & str(lo) & " max=" & str(hi) & " step=" & str(st) )

        CHT_NiceRange( 0.0, 8.0, 8, lo, hi, st )
        CHTTest_Check( "NiceRange: 0..8 with 8 ticks steps by 1", _
            CHTTest_Near(st,1.0,0.0001) andalso CHTTest_Near(lo,0.0,0.0001) andalso CHTTest_Near(hi,8.0,0.0001), _
            "min=" & str(lo) & " max=" & str(hi) & " step=" & str(st) )

        ' The HLOC reference range. The bounds widen to the step, which is the point: the top
        ' gridline must sit ABOVE the highest datum, not on it.
        CHT_NiceRange( 1000.0, 1220.0, 5, lo, hi, st )
        CHTTest_Check( "NiceRange: 1000..1220 snaps to a 50 step", _
            CHTTest_Near(st,50.0,0.0001) andalso CHTTest_Near(lo,1000.0,0.0001) andalso CHTTest_Near(hi,1250.0,0.0001), _
            "min=" & str(lo) & " max=" & str(hi) & " step=" & str(st) )

        ' A FLAT SERIES -- the one input that would divide by zero without a synthetic window.
        CHT_NiceRange( 5.0, 5.0, 5, lo, hi, st )
        CHTTest_Check( "NiceRange: a flat series opens a window around the value", _
            (st > 0.0) andalso (lo <= 5.0) andalso (hi >= 5.0), _
            "min=" & str(lo) & " max=" & str(hi) & " step=" & str(st) )

        CHT_NiceRange( 0.0, 0.0, 5, lo, hi, st )
        CHTTest_Check( "NiceRange: an all-zero series does not divide by zero", _
            (st > 0.0) andalso (hi > lo), _
            "min=" & str(lo) & " max=" & str(hi) & " step=" & str(st) )

        ' ALL-NEGATIVE. int() is floor, so the ceiling of -1/2 has to come out 0, not -1.
        CHT_NiceRange( -8.0, -1.0, 5, lo, hi, st )
        CHTTest_Check( "NiceRange: an all-negative range ceils toward zero", _
            CHTTest_Near(lo,-8.0,0.0001) andalso CHTTest_Near(hi,0.0,0.0001), _
            "min=" & str(lo) & " max=" & str(hi) & " step=" & str(st) )

        ' A SUB-UNIT STEP, where a truncating int() would land on the wrong power of ten.
        CHT_NiceRange( 0.0, 0.4, 5, lo, hi, st )
        CHTTest_Check( "NiceRange: a sub-unit range keeps a sub-unit step", _
            (st > 0.0) andalso (st < 1.0) andalso (hi >= 0.4), _
            "min=" & str(lo) & " max=" & str(hi) & " step=" & str(st) )
    end scope

    ' --- decimals and formatting ------------------------------------------------------------
    scope
        CHTTest_Check( "AutoDecimals: a step of 1 needs none", (CHT_AutoDecimals(1.0) = 0), _
                       "d=" & str(CHT_AutoDecimals(1.0)) )
        CHTTest_Check( "AutoDecimals: a step of 0.2 needs one", (CHT_AutoDecimals(0.2) = 1), _
                       "d=" & str(CHT_AutoDecimals(0.2)) )
        CHTTest_Check( "AutoDecimals: a step of 0.25 needs two", (CHT_AutoDecimals(0.25) = 2), _
                       "d=" & str(CHT_AutoDecimals(0.25)) )

        dim as string s
        s = CHT_FormatNumber( 1200.0, 0, true )
        CHTTest_Check( "FormatNumber: groups thousands", (s = "1,200"), s )
        s = CHT_FormatNumber( 1200.0, 0, false )
        CHTTest_Check( "FormatNumber: grouping is optional", (s = "1200"), s )
        s = CHT_FormatNumber( -1234.5, 1, true )
        CHTTest_Check( "FormatNumber: negative, grouped, one decimal", (s = "-1,234.5"), s )
        s = CHT_FormatNumber( 0.25, 2, false )
        CHTTest_Check( "FormatNumber: a sub-unit value keeps its leading zero", (s = "0.25"), s )
        ' THE CARRY CASE. Rounding after splitting the number gives 0.100 here.
        s = CHT_FormatNumber( 0.999, 2, false )
        CHTTest_Check( "FormatNumber: rounds before splitting, so the carry lands", (s = "1.00"), s )
        s = CHT_FormatNumber( 1000000.0, 0, true )
        CHTTest_Check( "FormatNumber: groups every three digits, not just once", (s = "1,000,000"), s )
    end scope

    if hWndParent = 0 then
        print "----- no parent HWND: geometry assertions skipped -----"
        dim as string sNo = "===== " & str(gCHT_TestCount) & " checks, " & str(gCHT_FailCount) & " failed ====="
        print sNo
        return gCHT_FailCount
    end if

    ' --- column geometry ----------------------------------------------------------------------
    scope
        dim as HWND h = CHTTest_MakeChart( hWndParent, PSCHART_TYPE_COLUMN, 700, 400 )
        if h = 0 then
            CHTTest_Check( "Column: the control was created", false, "PsChart_Create returned 0" )
        else
            PsChart_BeginUpdate( h )
            for i as long = 0 to 2
                dim as long si = PsChart_AddSeries( h, "S" & str(i) )
                PsChart_AddPoint( h, si, "Oranges",    1.0 + i )
                PsChart_AddPoint( h, si, "Apples",     4.0 + i )
                PsChart_AddPoint( h, si, "Pears",      5.0 )
                PsChart_AddPoint( h, si, "Bananas",    4.0 )
                PsChart_AddPoint( h, si, "Pineapples", 4.0 )
            next
            PsChart_EndUpdate( h )

            dim as RECT rcPlot
            PsChart_GetPlotRect( h, rcPlot )
            CHTTest_Check( "Column: the plot rect is non-empty", _
                (rcPlot.right > rcPlot.left) andalso (rcPlot.bottom > rcPlot.top), _
                CHTTest_RectStr(rcPlot) )

            dim as double vmin, vmax
            PsChart_GetValueRange( h, vmin, vmax )
            CHTTest_Check( "Column: the range is forced to include zero", (vmin <= 0.0), _
                           "min=" & str(vmin) & " max=" & str(vmax) )

            ' THE TWO ENDS OF THE MAPPING, to the pixel. An off-by-one here is invisible on a
            ' curve and unmistakable on a bar chart's baseline.
            dim as long yMin = PsChart_ValueToY( h, vmin )
            dim as long yMax = PsChart_ValueToY( h, vmax )
            CHTTest_Check( "ValueToY: the minimum lands on the plot bottom edge", _
                (yMin = rcPlot.bottom), "y=" & str(yMin) & " want " & str(rcPlot.bottom) )
            CHTTest_Check( "ValueToY: the maximum lands on the plot top edge", _
                (yMax = rcPlot.top), "y=" & str(yMax) & " want " & str(rcPlot.top) )

            dim as long nBars = 0, nInside = 0, nOnBase = 0, nOverlap = 0
            for i as long = 0 to PsChart_GetSeriesCount(h) - 1
                for j as long = 0 to PsChart_GetPointCount(h, i) - 1
                    dim as RECT rc
                    PsChart_GetPointRect( h, i, j, rc )
                    nBars += 1
                    if (rc.left >= rcPlot.left) andalso (rc.right <= rcPlot.right) andalso _
                       (rc.top >= rcPlot.top) andalso (rc.bottom <= rcPlot.bottom) then nInside += 1
                    if rc.bottom = rcPlot.bottom then nOnBase += 1
                next
            next
            CHTTest_Check( "Column: 3 series x 5 categories is 15 bars", (nBars = 15), _
                           "n=" & str(nBars) )
            CHTTest_Check( "Column: every bar lies inside the plot rect", (nInside = nBars), _
                           str(nInside) & " of " & str(nBars) )
            CHTTest_Check( "Column: every bar of an all-positive chart shares the baseline", _
                           (nOnBase = nBars), str(nOnBase) & " of " & str(nBars) )

            ' Bars within one category must not overlap each other.
            for j as long = 0 to PsChart_GetPointCount(h, 0) - 1
                for i as long = 0 to PsChart_GetSeriesCount(h) - 2
                    dim as RECT rcA, rcB
                    PsChart_GetPointRect( h, i, j, rcA )
                    PsChart_GetPointRect( h, i + 1, j, rcB )
                    if rcA.right > rcB.left then nOverlap += 1
                next
            next
            CHTTest_Check( "Column: bars within one group do not overlap", (nOverlap = 0), _
                           "overlaps=" & str(nOverlap) )

            CHTTest_Check( "Column: five short labels in 700px need no skipping", _
                           (PsChart_GetAxisLabelSkip(h) = 1), _
                           "stride=" & str(PsChart_GetAxisLabelSkip(h)) )

            DestroyWindow( h )
        end if
    end scope

    ' --- negative values -------------------------------------------------------------------------
    scope
        dim as HWND h = CHTTest_MakeChart( hWndParent, PSCHART_TYPE_COLUMN, 700, 400 )
        if h <> 0 then
            dim as long s = PsChart_AddSeries( h, "mixed" )
            PsChart_AddPoint( h, s, "a",  4.0 )
            PsChart_AddPoint( h, s, "b", -3.0 )
            PsChart_AddPoint( h, s, "c",  2.0 )

            dim as long yZero = PsChart_ValueToY( h, 0.0 )
            dim as RECT rcPos, rcNeg
            PsChart_GetPointRect( h, s, 0, rcPos )
            PsChart_GetPointRect( h, s, 1, rcNeg )

            CHTTest_Check( "Negatives: the positive bar sits wholly above the origin", _
                (rcPos.bottom <= yZero) andalso (rcPos.top < yZero), _
                CHTTest_RectStr(rcPos) & " origin=" & str(yZero) )
            CHTTest_Check( "Negatives: the negative bar sits wholly below the origin", _
                (rcNeg.top >= yZero) andalso (rcNeg.bottom > yZero), _
                CHTTest_RectStr(rcNeg) & " origin=" & str(yZero) )
            DestroyWindow( h )
        end if
    end scope

    ' --- line geometry and the auto-skip ------------------------------------------------------------
    scope
        dim as HWND h = CHTTest_MakeChart( hWndParent, PSCHART_TYPE_LINE, 700, 400 )
        if h <> 0 then
            dim as long s = PsChart_AddSeries( h, "line" )
            PsChart_BeginUpdate( h )
            ' Sixty dated categories in a 700px plot -- the HLOC reference's shape, and the
            ' case the auto-skip exists for.
            for i as long = 0 to 59
                PsChart_AddPoint( h, s, "Jan-" & str(i + 1), 1000.0 + (i mod 17) * 13.0 )
            next
            PsChart_EndUpdate( h )

            dim as RECT rcPlot
            PsChart_GetPlotRect( h, rcPlot )

            dim as long nOutside = 0
            for j as long = 0 to PsChart_GetPointCount(h, s) - 1
                dim as long y = PsChart_ValueToY( h, PsChart_GetPointValue(h, s, j) )
                if (y < rcPlot.top) orelse (y > rcPlot.bottom) then nOutside += 1
            next
            CHTTest_Check( "Line: every vertex of in-range data lands inside the plot", _
                           (nOutside = 0), "outside=" & str(nOutside) )

            dim as long stride = PsChart_GetAxisLabelSkip( h )
            CHTTest_Check( "Line: 60 dated labels in 700px force a stride above 1", _
                           (stride > 1), "stride=" & str(stride) )

            ' The stride has to be ENOUGH, not merely greater than one: two drawn labels are
            ' stride slots apart, so that width must clear the widest label.
            dim as long slotsW = clng( (rcPlot.right - rcPlot.left) / 60.0 ) * stride
            dim as long lw, lh
            CHT_MeasureText( h, 0, "Jan-60", lw, lh )
            CHTTest_Check( "Line: the stride leaves the widest label room to fit", _
                           (slotsW >= lw), "slots=" & str(slotsW) & " label=" & str(lw) )

            dim as double vmin, vmax
            PsChart_GetValueRange( h, vmin, vmax )
            dim as double perPx = (vmax - vmin) / csng(rcPlot.bottom - rcPlot.top)
            dim as double back = PsChart_YToValue( h, PsChart_ValueToY( h, 1100.0 ) )
            CHTTest_Check( "YToValue: round-trips to within a pixel", _
                           CHTTest_Near( back, 1100.0, perPx * 1.5 ), _
                           "got " & str(back) & " tol " & str(perPx * 1.5) )

            ' A LINE CHART MUST NOT FORCE ZERO into its range, or a series sitting at 1,100
            ' becomes a flat line across the top of the plot.
            CHTTest_Check( "Line: the range is not dragged down to zero", (vmin > 500.0), _
                           "min=" & str(vmin) )

            DestroyWindow( h )
        end if
    end scope

    ' --- HLOC ---------------------------------------------------------------------------------------
    scope
        dim as HWND h = CHTTest_MakeChart( hWndParent, PSCHART_TYPE_HLOC, 700, 400 )
        if h <> 0 then
            dim as long s = PsChart_AddSeries( h, "ohlc" )
            PsChart_AddHlocPoint( h, s, "d1", 1020.0, 1080.0, 1000.0, 1060.0 )
            PsChart_AddHlocPoint( h, s, "d2", 1060.0, 1100.0, 1040.0, 1050.0 )
            PsChart_AddHlocPoint( h, s, "d3", 1050.0, 1070.0,  990.0, 1010.0 )

            dim as double vmin, vmax
            PsChart_GetValueRange( h, vmin, vmax )
            ' The range must come from LOW and HIGH. Taking it from the close would clip the
            ' wick, which is the one thing this chart exists to show.
            CHTTest_Check( "HLOC: the range covers the lowest low", (vmin <= 990.0), _
                           "min=" & str(vmin) )
            CHTTest_Check( "HLOC: the range covers the highest high", (vmax >= 1100.0), _
                           "max=" & str(vmax) )

            dim as RECT rcPlot, rcBand
            PsChart_GetPlotRect( h, rcPlot )
            PsChart_GetPointRect( h, s, 0, rcBand )
            dim as long yHi = PsChart_ValueToY( h, 1080.0 )
            dim as long yLo = PsChart_ValueToY( h, 1000.0 )
            CHTTest_Check( "HLOC: the hit band spans high to low", _
                (rcBand.top = yHi) andalso (rcBand.bottom = yLo), _
                CHTTest_RectStr(rcBand) & " want top=" & str(yHi) & " bottom=" & str(yLo) )
            ' The band is the whole slot, not the wick: a two-pixel target is not clickable.
            CHTTest_Check( "HLOC: the hit band is a full slot wide", _
                           ((rcBand.right - rcBand.left) > 10), _
                           "w=" & str(rcBand.right - rcBand.left) )
            DestroyWindow( h )
        end if
    end scope

    ' --- pie -------------------------------------------------------------------------------------------
    scope
        dim as HWND h = CHTTest_MakeChart( hWndParent, PSCHART_TYPE_PIE, 400, 400 )
        if h <> 0 then
            dim as long s = PsChart_AddSeries( h, "pie" )
            dim as double vals(0 to 4) = { 4.0, 4.0, 1.0, 4.0, 2.0 }
            for i as long = 0 to 4
                PsChart_AddPoint( h, s, "P" & str(i), vals(i) )
            next

            dim as RECT rcDummy
            PsChart_GetPlotRect( h, rcDummy )        ' forces the layout

            dim as PSCHART ptr pChart = GetChartPointer( h )
            dim as double total = 0.0
            for i as long = 0 to 4
                total += pChart->series(0).points(i).fSweep
            next
            CHTTest_Check( "Pie: the sweeps sum to a full circle", _
                           CHTTest_Near( total, 360.0, 0.01 ), "sum=" & str(total) )

            CHTTest_Check( "Pie: the first slice starts at 12 o clock", _
                           CHTTest_Near( pChart->series(0).points(0).fStart, -90.0, 0.01 ), _
                           "start=" & str(pChart->series(0).points(0).fStart) )

            ' Proportionality: a value of 1 out of 15 is 24 degrees.
            CHTTest_Check( "Pie: a slice sweep is its share of the total", _
                           CHTTest_Near( pChart->series(0).points(2).fSweep, 24.0, 0.01 ), _
                           "sweep=" & str(pChart->series(0).points(2).fSweep) )

            ' HIT TEST AT EACH SLICE CENTROID. This is the assertion that ties the angle
            ' arithmetic to the thing a user actually points at.
            const PI_ = 3.14159265358979323846
            dim as long cx = (pChart->rcPie.left + pChart->rcPie.right) \ 2
            dim as long cy = (pChart->rcPie.top + pChart->rcPie.bottom) \ 2
            dim as single radius = csng( pChart->rcPie.right - pChart->rcPie.left ) / 2.0
            dim as long nHit = 0
            for k as long = 0 to 4
                dim as double midDeg = pChart->series(0).points(k).fStart + _
                                    pChart->series(0).points(k).fSweep / 2.0
                dim as double r = midDeg * PI_ / 180.0
                dim as long px = cx + clng( cos(r) * radius * 0.5 )
                dim as long py = cy + clng( sin(r) * radius * 0.5 )
                dim as long iSer, iPt
                if PsChart_HitTest( h, px, py, iSer, iPt ) then
                    if iPt = k then nHit += 1
                end if
            next
            CHTTest_Check( "Pie: hit-testing each slice centroid names that slice", _
                           (nHit = 5), str(nHit) & " of 5" )

            dim as long iS2, iP2
            dim as boolean bOut = PsChart_HitTest( h, pChart->rcPie.right + 20, cy, iS2, iP2 )
            CHTTest_Check( "Pie: a point beyond the rim hits nothing", _
                           (bOut = false) andalso (iP2 = -1), _
                           "hit=" & str(bOut) & " idx=" & str(iP2) )

            ' The doughnut hole is a hole for the hit test too, not only for the eye.
            PsChart_SetDoughnutRatio( h, 0.5 )
            PsChart_GetPlotRect( h, rcDummy )
            dim as long iS3, iP3
            dim as boolean bHole = PsChart_HitTest( h, cx, cy, iS3, iP3 )
            CHTTest_Check( "Doughnut: the hole hits nothing", (bHole = false), _
                           "hit=" & str(bHole) )

            DestroyWindow( h )
        end if
    end scope

    ' --- layout robustness --------------------------------------------------------------------------------
    scope
        ' A 0x0 client is the state EVERY control is created in, before the host sizes it.
        ' Laying out against it must re-arm rather than produce rects of nonsense.
        dim as HWND h = PsChart_Create( hWndParent, 9001, PSCHART_TYPE_COLUMN )
        if h <> 0 then
            ShowWindow( h, SW_HIDE )
            dim as PSCHART ptr pChart = GetChartPointer( h )
            pChart->bLayoutDirty = false
            pChart->LayoutChart()
            CHTTest_Check( "Layout: a 0x0 client re-arms the dirty flag instead of laying out", _
                           (pChart->bLayoutDirty = true), "dirty=" & str(pChart->bLayoutDirty) )

            SetWindowPos( h, 0, 0, 0, 300, 200, SWP_NOZORDER or SWP_NOACTIVATE )
            dim as RECT rc
            dim as boolean bGot = PsChart_GetPlotRect( h, rc )
            CHTTest_Check( "Layout: an empty chart still produces a plot rect", _
                           bGot andalso (rc.right > rc.left), CHTTest_RectStr(rc) )
            dim as long iSer, iPt
            CHTTest_Check( "HitTest: an empty chart answers nothing and does not fault", _
                           (PsChart_HitTest( h, 50, 50, iSer, iPt ) = false), "idx=" & str(iPt) )
            DestroyWindow( h )
        end if
    end scope

    ' --- the renderer actually renders -----------------------------------------------------------------------
    scope
        dim as HWND h = CHTTest_MakeChart( hWndParent, PSCHART_TYPE_COLUMN, 700, 400 )
        if h <> 0 then
            dim as long nEmpty = PsChart_CountRenderedTones( h )
            dim as ulong hashNoData = PsChart_HashRenderedPart( h )

            dim as long s = PsChart_AddSeries( h, "s" )
            PsChart_AddPoint( h, s, "a", 3.0 )
            PsChart_AddPoint( h, s, "b", 7.0 )
            PsChart_AddPoint( h, s, "c", 5.0 )

            dim as long nData = PsChart_CountRenderedTones( h )
            dim as ulong hashWithData = PsChart_HashRenderedPart( h )

            ' THE POINT OF THESE. A probe that passed while the painter drew a flat rectangle
            ' has shipped in this family before; a tone count that does not rise when data
            ' arrives is exactly that failure, caught.
            CHTTest_Check( "Render: an empty chart still draws its chrome", (nEmpty >= 2), _
                           "tones=" & str(nEmpty) )
            CHTTest_Check( "Render: adding data adds tones to the surface", (nData > nEmpty), _
                           "empty=" & str(nEmpty) & " data=" & str(nData) )
            CHTTest_Check( "Render: the surface hash changes when the data does", _
                           (hashWithData <> hashNoData), _
                           "empty=" & hex(hashNoData) & " data=" & hex(hashWithData) )

            ' And it must be REPEATABLE, or every comparison above is noise.
            dim as ulong hashAgain = PsChart_HashRenderedPart( h )
            CHTTest_Check( "Render: the same state hashes the same twice", _
                           (hashAgain = hashWithData), _
                           "a=" & hex(hashWithData) & " b=" & hex(hashAgain) )
            DestroyWindow( h )
        end if
    end scope

    ' --- per-series line style -------------------------------------------------------------------------------
    ' The whole claim being tested is "a dashed series is distinguishable from a solid one at
    ' the width a chart actually uses", and the only honest measurement of that is the ink:
    ' identical geometry, fewer set pixels. A hash comparison would only prove the picture
    ' changed, which a colour change would prove equally well.
    scope
        dim as HWND h = CHTTest_MakeChart( hWndParent, PSCHART_TYPE_LINE, 700, 400 )
        if h <> 0 then
            dim as PSCHART_COLORS clrs
            PsChart_GetColors( h, @clrs )

            dim as long s = PsChart_AddSeries( h, "s" )
            ' A zig-zag rather than a ramp: a dash pattern that only ever met one slope would
            ' not exercise the join, and the joins are where a dashed polyline goes wrong.
            for j as long = 0 to 11
                PsChart_AddPoint( h, s, str(j), iif( (j mod 2) = 0, 10.0, 40.0 ) )
            next
            PsChart_SetSeriesLineWidth( h, s, 2 )

            dim as boolean bDefSolid = (PsChart_GetSeriesLineStyle( h, s ) = CHT_LINE_SOLID)

            ' Geometry must not move. Captured before the style changes and compared after,
            ' because "fewer pixels" only means "dashed" if the line is in the same place.
            dim as RECT rcBefore
            PsChart_GetPointRect( h, s, 5, rcBefore )

            dim as long inkSolid = PsChart_CountPlotInkPixels( h, clrs.PlotBackColor )

            PsChart_SetSeriesLineStyle( h, s, CHT_LINE_DASH )
            dim as long inkDash = PsChart_CountPlotInkPixels( h, clrs.PlotBackColor )

            PsChart_SetSeriesLineStyle( h, s, CHT_LINE_DOT )
            dim as long inkDot = PsChart_CountPlotInkPixels( h, clrs.PlotBackColor )

            PsChart_SetSeriesLineStyle( h, s, CHT_LINE_DASHDOT )
            dim as long inkDashDot = PsChart_CountPlotInkPixels( h, clrs.PlotBackColor )

            dim as RECT rcAfter
            PsChart_GetPointRect( h, s, 5, rcAfter )

            CHTTest_Check( "LineStyle: a new series defaults to solid", bDefSolid, _
                           "default=" & str(CHT_LINE_SOLID) )
            CHTTest_Check( "LineStyle: a dashed series draws less ink than the solid one", _
                           (inkDash < inkSolid) andalso (inkDash > 0), _
                           "solid=" & str(inkSolid) & " dash=" & str(inkDash) )
            CHTTest_Check( "LineStyle: a dotted series draws less ink again", _
                           (inkDot < inkDash) andalso (inkDot > 0), _
                           "dash=" & str(inkDash) & " dot=" & str(inkDot) )
            CHTTest_Check( "LineStyle: dash-dot lands between dot and dash", _
                           (inkDashDot > inkDot) andalso (inkDashDot < inkSolid), _
                           "dot=" & str(inkDot) & " dashdot=" & str(inkDashDot) & " solid=" & str(inkSolid) )
            CHTTest_Check( "LineStyle: the style does not move the geometry", _
                           EqualRect( @rcBefore, @rcAfter ) <> 0, _
                           CHTTest_RectStr(rcBefore) & " vs " & CHTTest_RectStr(rcAfter) )

            PsChart_SetSeriesLineStyle( h, s, 999 )
            CHTTest_Check( "LineStyle: an unrecognised style is stored as solid", _
                           (PsChart_GetSeriesLineStyle( h, s ) = CHT_LINE_SOLID), _
                           "style=" & str(PsChart_GetSeriesLineStyle( h, s )) )
            CHTTest_Check( "LineStyle: an out-of-range series answers solid and does not fault", _
                           (PsChart_SetSeriesLineStyle( h, 99, CHT_LINE_DASH ) = false) andalso _
                           (PsChart_GetSeriesLineStyle( h, 99 ) = CHT_LINE_SOLID), _
                           "rejected" )

            DestroyWindow( h )
        end if
    end scope

    dim as string sSum = "===== " & str(gCHT_TestCount) & " checks, " & str(gCHT_FailCount) & " failed ====="
    print sSum
    return gCHT_FailCount
end function
