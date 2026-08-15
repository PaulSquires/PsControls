'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsChart.bi  --  owner-drawn static chart (line, column, HLOC, pie)
''

#pragma once

' ONE THING PsBufferPaint COSTS THE HOST: GDI+'s Status enum defines Ok = 0, and it lands at
' GLOBAL scope (AfxGdiPlus.bi has no NAMESPACE statement of its own), so ANY identifier named
' "ok" becomes a duplicate definition -- dropping "using AfxNova" does not save you, and
' FreeBASIC is case-insensitive so ok/Ok/OK all collide. The family convention is bOK.
#include once "PsBufferPaint.bi"
' The tooltip backend switch. The control ships on the SYSTEM (comctl32) backend exactly as
' every sibling does; a host opts an instance into PsTooltip with PsChart_SetTooltipMode.
#include once "PsTipHost.bi"

' The hot-tracking poll. WM_MOUSELEAVE is not reliably delivered when the cursor leaves fast
' (the family has been bitten by this in four controls), so a hot data point is also checked
' on a timer and cleared if the cursor is no longer inside the client rect. This is the ONLY
' timer this control runs, and it runs only while something is hot.
#define IDT_CCHART_HOTTRACK   &hCBC0


' ========================================================================================
' DEFAULTS
'
' The SIZE values are DPI-scaled once, at Create; every setter afterwards takes raw pixels
' and the caller scales (the family rule). The HAIRLINE values -- axis and gridline
' thickness -- are NOT DPI-scaled, ever: a hairline should stay a hairline (PsMenuBar's
' nSeparatorThickness rule).
' ========================================================================================
#define PSCHART_DEFAULT_PADDING          10   ' inset from the client edge on all four sides
#define PSCHART_DEFAULT_CHROMEGAP         6   ' between title/subtitle/legend/plot blocks
#define PSCHART_DEFAULT_TICKLEN           4   ' axis tick, and the gap before a value label
#define PSCHART_DEFAULT_BARGAP            1   ' between bars WITHIN one category group
#define PSCHART_DEFAULT_GROUPGAPPCT      25   ' percent of the slot left empty between groups
#define PSCHART_DEFAULT_HLOCTICK          3   ' the open/close tick's arm length, per side
#define PSCHART_DEFAULT_LINEWIDTH         2
#define PSCHART_DEFAULT_SYMBOLSIZE        6
#define PSCHART_DEFAULT_PIEGAP            1   ' NOT scaled: a slice separator is a hairline
#define PSCHART_DEFAULT_TICKHINT          5   ' how many value gridlines to aim for
#define PSCHART_DEFAULT_LABELGAP          8   ' minimum clear space between two X labels
#define PSCHART_DEFAULT_HOTPOLL_MS      100
#define PSCHART_DEFAULT_HITRADIUS         8   ' line-series hit tolerance, in pixels

' How many distinct series colours the built-in palette holds before it wraps. Ten is the
' point past which a categorical palette stops being distinguishable anyway; a host with
' more series is expected to set colours per series rather than lean on the wrap.
#define PSCHART_PALETTE_COUNT            10

' -1 in nDecimals means "derive from the axis step" -- see CHT_AutoDecimals. It is a
' sentinel rather than 0 because 0 decimals is a real, common request.
#define PSCHART_DECIMALS_AUTO            -1
' 0 in nLabelSkip means "measure the labels and work it out". A stride of 1 (draw every
' label) is a real request and must not be confused with it.
#define PSCHART_SKIP_AUTO                 0


' ========================================================================================
' The chart type. FIXED AT CREATE and immutable afterwards -- PsProgressBar's orientation
' rule, and for the same reason: the four types do not share a data shape (a pie has no
' value axis, an HLOC point carries four numbers where a column carries one), so a control
' that could switch would have to tolerate data authored for a type it is no longer showing.
' ========================================================================================
enum
    PSCHART_TYPE_LINE = 0
    PSCHART_TYPE_COLUMN
    PSCHART_TYPE_HLOC
    PSCHART_TYPE_PIE
end enum

' Point markers on a line series. Off by default: the screenshots this control was drawn
' from show a bare polyline, and a marker per point on a sixty-point series is noise.
enum
    CHT_SYM_NONE = 0
    CHT_SYM_CIRCLE
    CHT_SYM_SQUARE
    CHT_SYM_DIAMOND
    CHT_SYM_TRIANGLE
end enum

' How a LINE series' stroke is broken. SOLID is the default and every existing chart keeps it.
'
' This exists so a forecast can be told apart from the actual data WITHOUT spending a colour
' on it -- the two runs are the same quantity and giving them different colours says they are
' different quantities. Column and HLOC charts ignore it: there is no polyline to break.
enum
    CHT_LINE_SOLID = 0
    CHT_LINE_DASH
    CHT_LINE_DOT
    CHT_LINE_DASHDOT
end enum

' Which edge the legend takes. TOP/BOTTOM lay the entries out in a row, LEFT/RIGHT in a
' column, and the block is measured before the plot rect is carved so the plot never has to
' shrink twice.
enum
    CHT_LEGEND_BOTTOM = 0
    CHT_LEGEND_TOP
    CHT_LEGEND_LEFT
    CHT_LEGEND_RIGHT
end enum

' What a pie slice's label says. NAME_PERCENT is "Oranges 23%".
enum
    CHT_PIELABEL_NONE = 0
    CHT_PIELABEL_VALUE
    CHT_PIELABEL_PERCENT
    CHT_PIELABEL_NAME
    CHT_PIELABEL_NAME_PERCENT
end enum

' Where that label goes.
'   INSIDE   in the wedge, and SUPPRESSED when the wedge is too narrow to hold it.
'   OUTSIDE  always outside the rim, on a two-segment leader line.
'   AUTO     inside when it fits, promoted to an outside leader when it does not. The
'            default, because it is the only one that never silently drops a slice's label.
enum
    CHT_PIEPLACE_INSIDE = 0
    CHT_PIEPLACE_OUTSIDE
    CHT_PIEPLACE_AUTO
end enum


' ========================================================================================
' COLOURS
'
' Defaults are the family's dark palette, NOT the light-background look of the reference
' screenshots. A host reproduces those by calling PsChart_SetColors, exactly as every
' sibling demo re-themes -- a control whose defaults matched one particular host's form
' would be wrong everywhere else.
'
' Read-modify-write through Get/SetColors; there are deliberately no per-field setters.
' ========================================================================================
type PSCHART_COLORS
    BackColor         as COLORREF = BGR( 33, 37, 43)   ' the whole control
    PlotBackColor     as COLORREF = BGR( 33, 37, 43)   ' the plot rect only; differs when a
                                                       ' host wants the plot area to stand out
    GridColor         as COLORREF = BGR( 62, 68, 78)
    AxisColor         as COLORREF = BGR( 90, 97,108)
    LabelColor        as COLORREF = BGR(150,156,166)   ' both axes' text
    TitleColor        as COLORREF = BGR(215,218,224)
    SubtitleColor     as COLORREF = BGR(150,156,166)
    LegendTextColor   as COLORREF = BGR(190,196,206)
    SliceLabelColor   as COLORREF = BGR(255,255,255)   ' pie labels drawn INSIDE a wedge
    SliceLabelOutColor as COLORREF = BGR(190,196,206)  ' ...and outside it, over the backdrop
    LeaderColor       as COLORREF = BGR(120,126,136)
    SliceGapColor     as COLORREF = BGR( 33, 37, 43)   ' normally = the plot background
    ' HLOC up/down. Only consulted when PsChart_SetHlocUpDownColors turned them on; the
    ' default HLOC chart draws every bar in its series colour, as the reference does.
    UpColor           as COLORREF = BGR( 72,151, 13)
    DownColor         as COLORREF = BGR(208, 80, 80)
    ForeColorDisabled as COLORREF = BGR(110,115,125)
end type


' ========================================================================================
' A DATA POINT.
'
' One type serves all four charts rather than four types serving one each, because the
' series array would otherwise have to be four arrays and every renderer would need to know
' which one it was looking at. The cost is four unused doubles on a column point, which is
' 32 bytes -- against a control that redraws the whole surface every WM_PAINT, that is
' nothing worth splitting a type over.
' ========================================================================================
type PSCHART_POINT
    Text      as DWSTRING     ' the category label. ONLY SERIES 0's labels drive the X axis:
                              ' a second series' labels are carried (a tooltip may want them)
                              ' but never drawn, because two series disagreeing about what
                              ' category 3 is called has no rendering that is not a lie.
    Value     as double       ' line / column / pie
    OpenVal   as double       ' HLOC only. Named ...Val because Open and Close are FreeBASIC
    HighVal   as double       ' statements and a field may not shadow one.
    LowVal    as double
    CloseVal  as double
    ItemData  as LONG_PTR     ' host payload, echoed back in the message and tooltip callbacks
    ' An explicit flag rather than a CLR_NONE sentinel, because EVERY COLORREF value is a
    ' legal colour -- the family rule, and the reason PsIconPanel carries bHasForeColor.
    bHasColor as boolean
    PointColor as COLORREF
    nExplode  as long         ' pie only: raw px offset from the centre along the bisector

    ' --- DERIVED. Layout owns every field below this line; authoring one has no effect and
    '     it will be overwritten on the next repaint. -----------------------------------
    rc        as RECT         ' column: the bar. HLOC: the whole column band. line: the hit
                              ' box around the vertex. pie: the label's box.
    ptPlot    as POINT        ' line: the vertex. pie: the wedge's outer midpoint.
    fStart    as single       ' pie: start angle, GDI+ degrees (0 = 3 o'clock, + = clockwise)
    fSweep    as single       ' pie: sweep, same units
    bLabelOut as boolean      ' pie: this label was promoted to an outside leader
    bLabelHid as boolean      ' pie: this label did not fit and was dropped (INSIDE mode)
end type


' ========================================================================================
' A SERIES. The unit the legend names and the palette colours.
' ========================================================================================
type PSCHART_SERIES
    SeriesName as DWSTRING            ' legend text. Named ...Name because NAME is a
                                      ' FreeBASIC statement.
    bHasColor  as boolean
    SeriesColor as COLORREF
    nLineWidth as long = PSCHART_DEFAULT_LINEWIDTH    ' raw px; the caller scales
    nLineStyle as long = CHT_LINE_SOLID               ' line series only
    nSymbol    as long = CHT_SYM_NONE
    nSymbolSize as long = PSCHART_DEFAULT_SYMBOLSIZE
    bSmooth    as boolean = false     ' cardinal spline instead of straight segments
    bVisible   as boolean = true      ' hidden series are excluded from the AXIS RANGE too,
                                      ' not merely skipped when drawing

    points(any) as PSCHART_POINT
    pointCount  as long

    ' fbc cannot parse `redim preserve chart.series(i).points(...)` at a call site -- a
    ' nested array field needs a member procedure to grow it. PsListTree hit the same wall
    ' with PSLISTTREE_ROWINFO.EnsureCells and this is the same fix.
    declare sub EnsurePoints( byval n as long )
end type

sub PSCHART_SERIES.EnsurePoints( byval n as long )
    if n <= 0 then exit sub
    if ubound(this.points) < n - 1 then
        ' Double from 16, the family growth idiom: a chart populated one point at a time
        ' from a query loop must not realloc per point.
        dim as long newcap = 16
        do while newcap < n
            newcap *= 2
        loop
        redim preserve this.points( 0 to newcap - 1 )
    end if
end sub


' ========================================================================================
' WHAT A PAINT CALLBACK RECEIVES.
'
' A PsBufferPaint ptr -- the control's OWN buffer for this repaint -- and every rect layout
' resolved. Never an HDC, and never a GDI+ Graphics: a second Graphics on the same surface
' would put two independently-batching GDI+ objects on one HDC with nothing coordinating
' their flushes (PsBufferPaint.bi says this at length, and it is why PaintPie had to be
' added there rather than here).
'
' Every sentinel is already resolved before this struct reaches a host: fMin/fMax/fStep are
' the REAL range even when autoscaling, nDecimals is the derived count and never -1, and
' nLabelStride is the measured skip and never 0.
'
' THE CALLBACK IS ALL-OR-NOTHING. Supply one and the control's own renderer does not run at
' all -- not the plot, not the axes, not the chrome. That is PsProgressBar's shape rather
' than PsSelectBar's per-item one, because a chart has no per-item unit that means the same
' thing across all four types.
' ========================================================================================
type PSCHART_PAINTINFO
    hChart        as HWND
    b             as PsBufferPaint ptr
    rcClient      as RECT
    rcPlot        as RECT
    rcTitle       as RECT
    rcSubtitle    as RECT
    rcLegend      as RECT
    rcAxisX       as RECT
    rcAxisY       as RECT
    rcPie         as RECT      ' pie charts only; empty otherwise
    nChartType    as long
    fMin          as double
    fMax          as double
    fStep         as double
    nDecimals     as long
    nOriginY      as long      ' the pixel row of value 0, clamped into the plot
    nCatCount     as long
    nLabelStride  as long
    nHotSeries    as long      ' -1 when nothing is hot
    nHotPoint     as long
    isEnabled     as boolean
    wszTitle      as DWSTRING
    wszSubtitle   as DWSTRING
end type

' WHAT A MESSAGE CALLBACK RECEIVES. Return TRUE to suppress the control's own handling.
'
' iSeries/iPoint are resolved for the mouse messages and -1 otherwise, so a host does not
' repeat the hit test the control just did.
'
' ONE MESSAGE'S RETURN IS IGNORED: WM_LBUTTONUP. Nothing here takes capture today, but the
' family rule stands so that a control which later does cannot be stranded by a callback
' that swallowed the up-message that would have released it.
type PSCHART_MESSAGEINFO
    hChart   as HWND
    uMsg     as UINT
    wParam   as WPARAM
    lParam   as LPARAM
    iSeries  as long
    iPoint   as long
end type


' --- callback typedefs --------------------------------------------------------------------
type CHT_PaintCallbackSub    as sub( byval p as PSCHART_PAINTINFO ptr )
type CHT_MessageCallbackFunc as function( byval m as PSCHART_MESSAGEINFO ptr ) as boolean
' The whole-control tooltip text. Consulted only when no authored tip text was set and no
' data point is hot; see PsChart_ResolveTooltipText for the full order.
type CHT_TooltipCallbackFunc as function( byval hChart as HWND ) as DWSTRING
' Format one number for the value axis, a pie label or a tooltip. Return "" to fall back to
' the control's own formatting. nContext is one of the CHT_FMT_* constants below, so a host
' can format an axis tick and a tooltip differently from one function.
type CHT_ValueLabelCallbackFunc as function( byval hChart as HWND, byval fValue as double, _
                                             byval nContext as long ) as DWSTRING
' The hot point changed. iSeries/iPoint are -1 when nothing is hot. USER INTERACTION ONLY --
' a programmatic setter that happens to move data under the cursor does not fire this
' (the family rule: programmatic setters are silent).
type CHT_HotChangeCallbackSub as sub( byval hChart as HWND, byval iSeries as long, _
                                      byval iPoint as long )

enum
    CHT_FMT_AXIS = 0
    CHT_FMT_PIELABEL
    CHT_FMT_TOOLTIP
end enum


' --- pure helpers, defined in the .inc -----------------------------------------------------
' Declared up here because the member procedures below call them.

' Snap a raw data range to readable gridlines. Returns the axis bounds and the step through
' the byref arguments; the step is always a 1, 2 or 5 times a power of ten.
declare sub CHT_NiceRange( byval fRawMin as double, byval fRawMax as double, _
                           byval nTickHint as long, _
                           byref fOutMin as double, byref fOutMax as double, _
                           byref fOutStep as double )
' How many decimal places a step of this size needs for its labels to be distinguishable.
declare function CHT_AutoDecimals( byval fStep as double ) as long
' Fixed-point format with an optional thousands separator. Not str() and not format(): the
' reference charts show "1,200" and str() gives "1200" while format()'s picture string would
' have to be built per decimal count anyway.
declare function CHT_FormatNumber( byval fValue as double, byval nDecimals as long, _
                                   byval bThousands as boolean ) as DWSTRING
' Measure a string in the given font on a scratch DC. Returns 0x0 on any failure, which
' every caller treats as "cannot measure yet" and re-arms layout for.
declare sub CHT_MeasureText( byval hWin as HWND, byval hFont as HFONT, _
                             byval wszText as DWSTRING, byref nW as long, byref nH as long )


' ========================================================================================
' THE CONTROL'S PER-INSTANCE STATE.
'
' Lives in the CWindow UserData(0) slot, as every sibling's does. Never a global, never a
' window property: a form with four charts on it is the ordinary case here, not the
' exotic one.
' ========================================================================================
type PSCHART
    hWin           as HWND
    nChartType     as long = PSCHART_TYPE_LINE

    ' Fonts are BORROWED. The host owns every HFONT here and destroys it; this control only
    ' selects them while painting. Never DeleteObject one.
    ' Named hLabelFont rather than hFont because a field named hFont case-insensitively
    ' shadows the HFONT type inside a TYPE body (PsTabBar's hTabFont, same trap).
    hLabelFont     as HFONT
    hTitleFont     as HFONT     ' falls back to hLabelFont when unset
    hSubtitleFont  as HFONT     ' falls back to hLabelFont when unset

    colors         as PSCHART_COLORS
    palette( 0 to PSCHART_PALETTE_COUNT - 1 ) as COLORREF

    series(any)    as PSCHART_SERIES
    seriesCount    as long

    ' Bulk population. BeginUpdate/EndUpdate NEST -- a depth counter, not a flag, so a helper
    ' that brackets its own work does not cancel its caller's batch on the way out.
    updateDepth    as long

    bLayoutDirty   as boolean = true
    bEnabled       as boolean = true

    ' --- chrome ---------------------------------------------------------------------------
    wszTitle       as DWSTRING
    wszSubtitle    as DWSTRING
    bLegendVisible as boolean = false
    nLegendPos     as long = CHT_LEGEND_BOTTOM
    nPadding       as long          ' DPI-scaled at Create
    nChromeGap     as long          ' DPI-scaled at Create

    ' --- value axis -----------------------------------------------------------------------
    bAutoScale     as boolean = true
    fUserMin       as double
    fUserMax       as double
    nTickHint      as long = PSCHART_DEFAULT_TICKHINT
    bGridHorz      as boolean = true
    bGridVert      as boolean = false
    nDecimals      as long = PSCHART_DECIMALS_AUTO
    bThousands     as boolean = true
    nLabelSkip     as long = PSCHART_SKIP_AUTO
    ' Hairlines. NOT DPI-scaled, ever.
    nAxisThick     as long = 1
    nGridThick     as long = 1
    nTickLen       as long          ' DPI-scaled at Create

    ' --- column ---------------------------------------------------------------------------
    nBarGap        as long          ' DPI-scaled at Create
    nGroupGapPct   as long = PSCHART_DEFAULT_GROUPGAPPCT

    ' --- HLOC -----------------------------------------------------------------------------
    nHlocTick      as long          ' DPI-scaled at Create
    bHlocUpDown    as boolean = false

    ' --- pie ------------------------------------------------------------------------------
    fDoughnutRatio as single = 0.0
    nPieLabelMode  as long = CHT_PIELABEL_VALUE
    nPieLabelPlace as long = CHT_PIEPLACE_AUTO
    fPieStartAngle as single = -90.0     ' 12 o'clock. GDI+ 0 is 3 o'clock, + is clockwise.
    nPieSliceGap   as long = PSCHART_DEFAULT_PIEGAP   ' hairline, not scaled

    ' --- LAYOUT RESULTS. Derived; every field below is rewritten by LayoutChart. -----------
    rcClient       as RECT
    rcPlot         as RECT
    rcTitle        as RECT
    rcSubtitle     as RECT
    rcLegend       as RECT
    rcAxisX        as RECT
    rcAxisY        as RECT
    rcPie          as RECT
    fMin           as double        ' the RESOLVED range, autoscaled or not
    fMax           as double
    fStep          as double
    nResDecimals   as long          ' the RESOLVED decimal count; never -1
    nOriginY       as long          ' pixel row of value 0, clamped into rcPlot
    nCatCount      as long
    fSlotWidth     as single
    nLabelStride   as long = 1
    nLabelH        as long          ' one line of the label font

    ' --- hot tracking ---------------------------------------------------------------------
    nHotSeries     as long = -1
    nHotPoint      as long = -1
    bTracking      as boolean       ' TrackMouseEvent is armed
    bPolling       as boolean       ' the leave-safety-net timer is running
    nHitRadius     as long          ' DPI-scaled at Create
    tip            as PSTIPHOST
    wszTipText     as DWSTRING      ' authored whole-control tip; wins over the callback
    wszTipBuf      as DWSTRING      ' the buffer TTN_GETDISPINFOW hands comctl32 a pointer
                                    ' into -- must outlive the notification, hence a field

    ' --- callbacks, all optional ----------------------------------------------------------
    PaintCallback      as CHT_PaintCallbackSub
    MessageCallback    as CHT_MessageCallbackFunc
    TooltipCallback    as CHT_TooltipCallbackFunc
    ValueLabelCallback as CHT_ValueLabelCallbackFunc
    HotChangeCallback  as CHT_HotChangeCallbackSub

    declare sub      Refresh()
    declare function IsValidSeries( byval idx as long ) as boolean
    declare function GetSeries( byval idx as long ) as PSCHART_SERIES ptr
    declare function IsValidPoint( byval iSer as long, byval iPt as long ) as boolean
    declare function GetPoint( byval iSer as long, byval iPt as long ) as PSCHART_POINT ptr
    declare function AddSeries() as long
    declare sub      ClearAll()
    declare function CategoryCount() as long
    declare function VisibleSeriesCount() as long
    declare function ResolveSeriesColor( byval idx as long ) as COLORREF
    declare function ValueToY( byval fValue as double ) as long
    declare function YToValue( byval y as long ) as double
    declare function SlotCenterX( byval iCat as long ) as long
    declare function FormatValue( byval fValue as double, byval nContext as long ) as DWSTRING
    declare sub      ResolveRange()
    declare sub      LayoutChart()
    ' The two halves of the layout that produce PER-POINT geometry. Split out of LayoutChart
    ' only because the axis charts and the pie share none of it, not because either is
    ' independently callable -- both assume LayoutChart has already resolved the rects.
    declare sub      LayoutPoints()
    declare sub      LayoutPieSlices()
    ' What slice iPt's label says, per nPieLabelMode. Called by BOTH layout (to measure it)
    ' and the renderer (to draw it) -- one function, so the string that was measured is
    ' provably the string that gets drawn.
    declare function PieLabelText( byval iPt as long ) as DWSTRING
    declare function PieTotal() as double
end type


' ========================================================================================
' Mark the layout stale and ask for a repaint. EVERY mutator ends in this -- the layout is
' lazy, so nothing recomputes until the next WM_PAINT actually needs it, and a host that
' adds two hundred points one at a time still lays out once.
'
' Inside a BeginUpdate batch the invalidate is skipped but the dirty flag is still set, so
' EndUpdate has something to act on.
' ========================================================================================
sub PSCHART.Refresh()
    this.bLayoutDirty = true
    if this.updateDepth > 0 then exit sub
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub

function PSCHART.IsValidSeries( byval idx as long ) as boolean
    if idx < 0 then return false
    if idx >= this.seriesCount then return false
    return true
end function

function PSCHART.GetSeries( byval idx as long ) as PSCHART_SERIES ptr
    if this.IsValidSeries(idx) = false then return 0
    return @this.series(idx)
end function

function PSCHART.IsValidPoint( byval iSer as long, byval iPt as long ) as boolean
    if this.IsValidSeries(iSer) = false then return false
    if iPt < 0 then return false
    if iPt >= this.series(iSer).pointCount then return false
    return true
end function

function PSCHART.GetPoint( byval iSer as long, byval iPt as long ) as PSCHART_POINT ptr
    if this.IsValidPoint(iSer, iPt) = false then return 0
    return @this.series(iSer).points(iPt)
end function

function PSCHART.AddSeries() as long
    dim as long cap = ubound(this.series) + 1
    if this.seriesCount >= cap then
        dim as long newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.series( 0 to newcap - 1 )
    end if

    ' Reset the recycled slot. A slot that once held a series still holds its DWSTRING and
    ' its points array; leaving either behind makes a cleared-and-repopulated chart show the
    ' old series' name, which is exactly the bug PsTabBar's InsertTabAt resets against.
    with this.series(this.seriesCount)
        .SeriesName  = ""
        .bHasColor   = false
        .SeriesColor = 0
        .nLineWidth  = PSCHART_DEFAULT_LINEWIDTH
        .nLineStyle  = CHT_LINE_SOLID
        .nSymbol     = CHT_SYM_NONE
        .nSymbolSize = PSCHART_DEFAULT_SYMBOLSIZE
        .bSmooth     = false
        .bVisible    = true
        .pointCount  = 0
    end with

    this.seriesCount += 1
    this.Refresh()
    return this.seriesCount - 1
end function

sub PSCHART.ClearAll()
    ' Release every point's DWSTRING rather than merely zeroing the counts. The array slots
    ' survive as capacity and would otherwise keep the strings alive until the control dies.
    for i as long = 0 to this.seriesCount - 1
        for j as long = 0 to this.series(i).pointCount - 1
            this.series(i).points(j).Text = ""
        next
        this.series(i).pointCount = 0
        this.series(i).SeriesName = ""
    next
    this.seriesCount = 0
    this.nHotSeries = -1
    this.nHotPoint = -1
    this.Refresh()
end sub

' The number of categories on the X axis: the LONGEST series, not series 0's count.
' A series with fewer points than its neighbour is legal (a partial month of data next to a
' full one) and simply stops early; taking series 0 would truncate the axis instead.
function PSCHART.CategoryCount() as long
    dim as long n = 0
    for i as long = 0 to this.seriesCount - 1
        if this.series(i).bVisible then
            if this.series(i).pointCount > n then n = this.series(i).pointCount
        end if
    next
    return n
end function

function PSCHART.VisibleSeriesCount() as long
    dim as long n = 0
    for i as long = 0 to this.seriesCount - 1
        if this.series(i).bVisible then n += 1
    next
    return n
end function

' Explicit series colour if one was set, else the palette entry, wrapping. A pie takes its
' colour PER POINT and calls this with the point index instead -- the wedges of one series
' are what the palette is enumerating there.
function PSCHART.ResolveSeriesColor( byval idx as long ) as COLORREF
    if this.IsValidSeries(idx) then
        if this.series(idx).bHasColor then return this.series(idx).SeriesColor
    end if
    dim as long i = idx mod PSCHART_PALETTE_COUNT
    if i < 0 then i = 0
    return this.palette(i)
end function

' ========================================================================================
' The value axis, in one place. Every renderer's Y comes through here and nowhere else.
'
' Returns rcPlot.bottom for the axis minimum and rcPlot.top for the maximum, to the pixel --
' the self-test asserts exactly that, because an off-by-one here is invisible on a curve and
' obvious on a bar chart's baseline.
' ========================================================================================
function PSCHART.ValueToY( byval fValue as double ) as long
    dim as long h = this.rcPlot.bottom - this.rcPlot.top
    if h <= 0 then return this.rcPlot.top
    dim as double span = this.fMax - this.fMin
    if span <= 0.0 then return this.rcPlot.bottom
    dim as double t = (fValue - this.fMin) / span
    ' int() then cast, NOT clng(x + 0.5). clng already rounds to nearest, so the familiar
    ' idiom rounds twice: at the axis maximum t*h is exactly h, and clng(h + 0.5) came back
    ' as h+1 -- one pixel ABOVE the plot's top edge, which put the tallest bar outside the
    ' rect the renderer clips to.
    return this.rcPlot.bottom - clng( int( t * h + 0.5 ) )
end function

function PSCHART.YToValue( byval y as long ) as double
    dim as long h = this.rcPlot.bottom - this.rcPlot.top
    if h <= 0 then return this.fMin
    dim as double t = csng(this.rcPlot.bottom - y) / csng(h)
    return this.fMin + t * (this.fMax - this.fMin)
end function

' The pixel centre of category iCat. Categories are EVENLY SPACED regardless of what their
' labels say -- this is a category axis, not a numeric one, so a gap in the dates is not a
' gap on the axis.
function PSCHART.SlotCenterX( byval iCat as long ) as long
    return this.rcPlot.left + clng( (iCat + 0.5) * this.fSlotWidth )
end function

' Host formatter first, the control's own second. A callback returning "" means "you do it",
' which lets a host special-case a few values without having to format all of them.
function PSCHART.FormatValue( byval fValue as double, byval nContext as long ) as DWSTRING
    if this.ValueLabelCallback then
        dim as DWSTRING wsz = this.ValueLabelCallback( this.hWin, fValue, nContext )
        if len(wsz) then return wsz
    end if
    return CHT_FormatNumber( fValue, this.nResDecimals, this.bThousands )
end function


' ========================================================================================
' Resolve the value axis: fMin, fMax, fStep and nResDecimals.
'
' Called by LayoutChart before anything is measured, because the Y-axis gutter is as wide as
' its widest label and the labels are not known until the range is.
'
' A PIE HAS NO VALUE AXIS and this leaves the range alone for one -- the wedges are fractions
' of a total, and a pie that ran this would compute a range nothing reads.
' ========================================================================================
sub PSCHART.ResolveRange()
    if this.nChartType = PSCHART_TYPE_PIE then
        this.fMin = 0.0 : this.fMax = 1.0 : this.fStep = 1.0
        this.nResDecimals = iif( this.nDecimals = PSCHART_DECIMALS_AUTO, 0, this.nDecimals )
        exit sub
    end if

    if this.bAutoScale = false then
        this.fMin = this.fUserMin
        this.fMax = this.fUserMax
        if this.fMax <= this.fMin then this.fMax = this.fMin + 1.0
        ' Still needs a step: the gridlines and their labels come from it either way.
        dim as double dummyMin, dummyMax, fS
        CHT_NiceRange( this.fMin, this.fMax, this.nTickHint, dummyMin, dummyMax, fS )
        this.fStep = fS
        this.nResDecimals = iif( this.nDecimals = PSCHART_DECIMALS_AUTO, _
                                 CHT_AutoDecimals(fS), this.nDecimals )
        exit sub
    end if

    dim as double rawMin = 0.0, rawMax = 0.0
    dim as boolean bAny = false

    for i as long = 0 to this.seriesCount - 1
        if this.series(i).bVisible = false then continue for
        for j as long = 0 to this.series(i).pointCount - 1
            dim as double lo, hi
            if this.nChartType = PSCHART_TYPE_HLOC then
                ' HLOC reads Low and High, NOT Value. A bar whose range came from the close
                ' price would clip its own wick, which is the one thing an HLOC chart exists
                ' to show.
                lo = this.series(i).points(j).LowVal
                hi = this.series(i).points(j).HighVal
                if hi < lo then swap lo, hi
            else
                lo = this.series(i).points(j).Value
                hi = lo
            end if
            if bAny = false then
                rawMin = lo : rawMax = hi : bAny = true
            else
                if lo < rawMin then rawMin = lo
                if hi > rawMax then rawMax = hi
            end if
        next
    next

    if bAny = false then
        rawMin = 0.0 : rawMax = 1.0
    end if

    ' A COLUMN CHART ALWAYS INCLUDES ZERO. Bars are drawn from a baseline, and a baseline
    ' that is not zero makes a bar four times taller than its neighbour for a value twice as
    ' large. Line and HLOC charts do NOT get this: a stock trading at 1,120 plotted against a
    ' zero baseline is a flat line across the top of the plot.
    if this.nChartType = PSCHART_TYPE_COLUMN then
        if rawMin > 0.0 then rawMin = 0.0
        if rawMax < 0.0 then rawMax = 0.0
    end if

    dim as double fLo, fHi, fS
    CHT_NiceRange( rawMin, rawMax, this.nTickHint, fLo, fHi, fS )
    this.fMin = fLo
    this.fMax = fHi
    this.fStep = fS
    this.nResDecimals = iif( this.nDecimals = PSCHART_DECIMALS_AUTO, _
                             CHT_AutoDecimals(fS), this.nDecimals )
end sub


' ========================================================================================
' LAYOUT. Lazy, and the single source of truth for every rect the renderer AND the hit test
' use -- so a bar that is drawn somewhere the hit test does not expect is not a class of bug
' that can exist here.
'
' RE-ARMS AND EXITS on a 0x0 client (controls are created 0x0 and sized by the host
' afterwards) and on a failure to get a DC (it measures label text, and measuring against
' "whatever font the DC happens to hold" is how PsStatusBar once produced a layout that was
' right only on the machine it was written on).
' ========================================================================================
sub PSCHART.LayoutChart()
    if this.hWin = 0 then exit sub

    GetClientRect( this.hWin, @this.rcClient )
    dim as long cw = this.rcClient.right - this.rcClient.left
    dim as long ch = this.rcClient.bottom - this.rcClient.top
    if (cw <= 0) orelse (ch <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.ResolveRange()
    this.nCatCount = this.CategoryCount()

    ' One line of the label font, needed before any block can be measured.
    dim as long nW, nH
    CHT_MeasureText( this.hWin, this.hLabelFont, "0123456789", nW, nH )
    if nH <= 0 then
        this.bLayoutDirty = true      ' could not measure; try again on the next paint
        exit sub
    end if
    this.nLabelH = nH

    ' Everything is carved out of this running rect, top-down then sides.
    dim as RECT rc = this.rcClient
    InflateRect( @rc, -this.nPadding, -this.nPadding )
    SetRectEmpty( @this.rcTitle )
    SetRectEmpty( @this.rcSubtitle )
    SetRectEmpty( @this.rcLegend )
    SetRectEmpty( @this.rcAxisX )
    SetRectEmpty( @this.rcAxisY )
    SetRectEmpty( @this.rcPie )

    ' --- title / subtitle -----------------------------------------------------------------
    if len(this.wszTitle) then
        dim as HFONT hf = iif( this.hTitleFont <> 0, this.hTitleFont, this.hLabelFont )
        dim as long tw, th
        CHT_MeasureText( this.hWin, hf, this.wszTitle, tw, th )
        if th <= 0 then th = this.nLabelH
        this.rcTitle = rc
        this.rcTitle.bottom = rc.top + th
        rc.top = this.rcTitle.bottom + this.nChromeGap
    end if
    if len(this.wszSubtitle) then
        dim as HFONT hf = iif( this.hSubtitleFont <> 0, this.hSubtitleFont, this.hLabelFont )
        dim as long tw, th
        CHT_MeasureText( this.hWin, hf, this.wszSubtitle, tw, th )
        if th <= 0 then th = this.nLabelH
        this.rcSubtitle = rc
        this.rcSubtitle.bottom = rc.top + th
        rc.top = this.rcSubtitle.bottom + this.nChromeGap
    end if

    ' --- legend ---------------------------------------------------------------------------
    ' Measured against the SERIES for the axis charts and against series 0's POINTS for a
    ' pie, because a pie's legend enumerates slices; the series is the whole pie.
    if (this.bLegendVisible) andalso (this.seriesCount > 0) then
        dim as long nEntries = 0
        dim as long widest = 0
        if this.nChartType = PSCHART_TYPE_PIE then
            nEntries = this.series(0).pointCount
            for j as long = 0 to nEntries - 1
                dim as long ew, eh
                CHT_MeasureText( this.hWin, this.hLabelFont, this.series(0).points(j).Text, ew, eh )
                if ew > widest then widest = ew
            next
        else
            for i as long = 0 to this.seriesCount - 1
                if this.series(i).bVisible = false then continue for
                nEntries += 1
                dim as long ew, eh
                CHT_MeasureText( this.hWin, this.hLabelFont, this.series(i).SeriesName, ew, eh )
                if ew > widest then widest = ew
            next
        end if

        if nEntries > 0 then
            ' Swatch is a square of the label height; the gaps are the chrome gap so the
            ' legend's rhythm matches the rest of the chart's.
            dim as long swatch = this.nLabelH
            select case this.nLegendPos
            case CHT_LEGEND_LEFT, CHT_LEGEND_RIGHT
                dim as long lw = swatch + this.nChromeGap + widest
                if lw > (rc.right - rc.left) \ 2 then lw = (rc.right - rc.left) \ 2
                this.rcLegend = rc
                if this.nLegendPos = CHT_LEGEND_LEFT then
                    this.rcLegend.right = rc.left + lw
                    rc.left = this.rcLegend.right + this.nChromeGap
                else
                    this.rcLegend.left = rc.right - lw
                    rc.right = this.rcLegend.left - this.nChromeGap
                end if
            case else                                   ' TOP / BOTTOM
                this.rcLegend = rc
                if this.nLegendPos = CHT_LEGEND_TOP then
                    this.rcLegend.bottom = rc.top + swatch
                    rc.top = this.rcLegend.bottom + this.nChromeGap
                else
                    this.rcLegend.top = rc.bottom - swatch
                    rc.bottom = this.rcLegend.top - this.nChromeGap
                end if
            end select
        end if
    end if

    ' --- pie: no axes, a centred square, and we are done ----------------------------------
    if this.nChartType = PSCHART_TYPE_PIE then
        this.rcPlot = rc
        this.nOriginY = rc.bottom
        this.fSlotWidth = 0.0
        this.nLabelStride = 1

        dim as long pw = rc.right - rc.left
        dim as long ph = rc.bottom - rc.top
        if (pw <= 0) orelse (ph <= 0) then
            SetRectEmpty( @this.rcPie )
            exit sub
        end if

        ' Room for whatever sits OUTSIDE the rim: the largest explode offset, plus a ring for
        ' outside labels and their leaders when those can occur. Reserved before the square is
        ' cut, so an exploded slice cannot end up outside the control.
        dim as long inset = 0
        if this.seriesCount > 0 then
            for j as long = 0 to this.series(0).pointCount - 1
                if this.series(0).points(j).nExplode > inset then inset = this.series(0).points(j).nExplode
            next
        end if
        if (this.nPieLabelMode <> CHT_PIELABEL_NONE) andalso _
           (this.nPieLabelPlace <> CHT_PIEPLACE_INSIDE) then
            dim as long lw = 0, lh = 0
            CHT_MeasureText( this.hWin, this.hLabelFont, "MMMMMMMM", lw, lh )
            inset += lw \ 2 + this.nChromeGap * 2
        end if

        dim as long side = iif( pw < ph, pw, ph ) - inset * 2
        ' Below about twenty pixels there is no pie to draw, and a negative side would make
        ' PaintPie's extents negative. Give up rather than draw nonsense.
        if side < 20 then
            side = iif( pw < ph, pw, ph )
            if side < 8 then
                SetRectEmpty( @this.rcPie )
                exit sub
            end if
        end if

        this.rcPie.left = rc.left + (pw - side) \ 2
        this.rcPie.top  = rc.top  + (ph - side) \ 2
        this.rcPie.right = this.rcPie.left + side
        this.rcPie.bottom = this.rcPie.top + side

        this.LayoutPieSlices()
        exit sub
    end if

    ' --- value axis gutter ----------------------------------------------------------------
    ' As wide as the widest label it will actually draw, so the plot never overlaps a number.
    dim as long gutter = 0
    if this.fStep > 0.0 then
        dim as double v = this.fMin
        dim as long guard = 0
        do while (v <= this.fMax + this.fStep * 0.5) andalso (guard < 1000)
            dim as DWSTRING wsz = this.FormatValue( v, CHT_FMT_AXIS )
            dim as long lw, lh
            CHT_MeasureText( this.hWin, this.hLabelFont, wsz, lw, lh )
            if lw > gutter then gutter = lw
            v += this.fStep
            guard += 1
        loop
    end if
    if gutter > 0 then gutter += this.nTickLen

    this.rcAxisY = rc
    this.rcAxisY.right = rc.left + gutter
    rc.left = this.rcAxisY.right

    ' --- category axis strip --------------------------------------------------------------
    this.rcAxisX = rc
    this.rcAxisX.top = rc.bottom - (this.nLabelH + this.nTickLen)
    rc.bottom = this.rcAxisX.top

    ' --- the plot ---------------------------------------------------------------------------
    this.rcPlot = rc
    ' Half a label line of headroom so the topmost gridline's label is not clipped by the
    ' control's own edge, and so a data point AT the maximum is not drawn half outside.
    if (this.rcPlot.bottom - this.rcPlot.top) < 4 then
        this.bLayoutDirty = true
        exit sub
    end if
    this.rcAxisY.top = this.rcPlot.top
    this.rcAxisY.bottom = this.rcPlot.bottom

    ' Value 0 in pixels, CLAMPED into the plot. When the range does not straddle zero the
    ' baseline is simply the nearer edge, which is what makes an all-positive column chart
    ' sit on rcPlot.bottom.
    this.nOriginY = this.ValueToY( 0.0 )
    if this.nOriginY > this.rcPlot.bottom then this.nOriginY = this.rcPlot.bottom
    if this.nOriginY < this.rcPlot.top then this.nOriginY = this.rcPlot.top

    ' --- category slots and the label stride ------------------------------------------------
    dim as long plotW = this.rcPlot.right - this.rcPlot.left
    if this.nCatCount > 0 then
        this.fSlotWidth = csng(plotW) / csng(this.nCatCount)
    else
        this.fSlotWidth = 0.0
    end if

    this.nLabelStride = 1
    if this.nLabelSkip > 0 then
        this.nLabelStride = this.nLabelSkip
    elseif this.nCatCount > 1 then
        ' Auto-skip: measure the WIDEST label rather than the average. One long label in a
        ' run of short ones is exactly the case that collides, and an average hides it.
        dim as long widestCat = 0
        if this.seriesCount > 0 then
            for j as long = 0 to this.series(0).pointCount - 1
                dim as long lw, lh
                CHT_MeasureText( this.hWin, this.hLabelFont, this.series(0).points(j).Text, lw, lh )
                if lw > widestCat then widestCat = lw
            next
        end if
        if (widestCat > 0) andalso (this.fSlotWidth > 0.0) then
            dim as single need = csng(widestCat + PSCHART_DEFAULT_LABELGAP)
            dim as long stride = clng( need / this.fSlotWidth )
            if stride * this.fSlotWidth < need then stride += 1
            if stride < 1 then stride = 1
            this.nLabelStride = stride
        end if
    end if

    this.LayoutPoints()
end sub


' ========================================================================================
' PER-POINT GEOMETRY for the three axis charts.
'
' Every rect written here is used by the renderer AND by the hit test. That is the whole
' reason this is a layout step rather than arithmetic inside the painter: a bar drawn
' somewhere the hit test does not expect is not a bug that can exist if there is only one
' set of numbers.
'
' Points BEYOND the plot's value range still get their true geometry -- clipping is the
' renderer's job (PsBufferPaint.SetClipRect), not layout's, because a line entering and
' leaving the top of the plot must still be drawn through the part that is visible.
' ========================================================================================
sub PSCHART.LayoutPoints()
    dim as long nVis = this.VisibleSeriesCount()

    ' Column bar geometry is per-GROUP and identical for every category, so it is computed
    ' once here rather than per point.
    dim as single groupW = 0.0, barW = 0.0
    if (this.nChartType = PSCHART_TYPE_COLUMN) andalso (nVis > 0) then
        dim as long gap = this.nGroupGapPct
        if gap < 0 then gap = 0
        if gap > 90 then gap = 90
        groupW = this.fSlotWidth * (1.0 - csng(gap) / 100.0)
        barW = (groupW - csng((nVis - 1) * this.nBarGap)) / csng(nVis)
        ' Below one pixel a bar has no fill at all. Keeping it at one is honest -- the chart
        ' is too narrow for the data, and a visible hairline says so where nothing does not.
        if barW < 1.0 then barW = 1.0
    end if

    dim as long kVis = -1                  ' the visible-only index, which is what positions
                                           ' a bar within its group
    for i as long = 0 to this.seriesCount - 1
        if this.series(i).bVisible = false then
            ' A hidden series still gets EMPTY rects rather than stale ones: the hit test
            ' loops every point of every series, and a leftover rect from when the series was
            ' visible would keep answering (PsTabBar's scrolled-out-of-view rule).
            for j as long = 0 to this.series(i).pointCount - 1
                SetRectEmpty( @this.series(i).points(j).rc )
                this.series(i).points(j).ptPlot.x = 0
                this.series(i).points(j).ptPlot.y = 0
            next
            continue for
        end if
        kVis += 1

        for j as long = 0 to this.series(i).pointCount - 1
            dim as PSCHART_POINT ptr p = @this.series(i).points(j)
            dim as long cx = this.SlotCenterX( j )

            select case this.nChartType
            case PSCHART_TYPE_COLUMN
                dim as single gLeft = csng(cx) - groupW / 2.0
                dim as long bl = clng( gLeft + csng(kVis) * (barW + csng(this.nBarGap)) )
                dim as long br = bl + clng( barW )
                if br <= bl then br = bl + 1
                dim as long yv = this.ValueToY( p->Value )
                p->rc.left  = bl
                p->rc.right = br
                if yv <= this.nOriginY then
                    p->rc.top = yv : p->rc.bottom = this.nOriginY
                else
                    p->rc.top = this.nOriginY : p->rc.bottom = yv
                end if
                p->ptPlot.x = (bl + br) \ 2
                p->ptPlot.y = yv

            case PSCHART_TYPE_HLOC
                dim as long yHi = this.ValueToY( p->HighVal )
                dim as long yLo = this.ValueToY( p->LowVal )
                if yHi > yLo then swap yHi, yLo
                ' The hit band is the whole slot, not the two-pixel wick: a user aiming at a
                ' thin bar with a mouse cannot be asked for that precision.
                dim as long half = clng( this.fSlotWidth / 2.0 )
                if half < 1 then half = 1
                p->rc.left   = cx - half
                p->rc.right  = cx + half
                p->rc.top    = yHi
                p->rc.bottom = yLo
                ' A zero-height band (open = high = low = close) would never hit-test.
                if p->rc.bottom <= p->rc.top then p->rc.bottom = p->rc.top + 1
                p->ptPlot.x = cx
                p->ptPlot.y = this.ValueToY( p->CloseVal )

            case else                                   ' PSCHART_TYPE_LINE
                dim as long yv = this.ValueToY( p->Value )
                p->ptPlot.x = cx
                p->ptPlot.y = yv
                dim as long r = this.nHitRadius
                if r < 1 then r = 1
                p->rc.left = cx - r : p->rc.right  = cx + r
                p->rc.top  = yv - r : p->rc.bottom = yv + r
            end select
        next
    next
end sub


' The pie's denominator: the sum of the ABSOLUTE values of series 0. Absolute because a
' negative share of a whole has no wedge that means anything, and silently dropping the point
' would make the remaining wedges add up to more than the total they are drawn against.
function PSCHART.PieTotal() as double
    if this.seriesCount <= 0 then return 0.0
    dim as double t = 0.0
    for j as long = 0 to this.series(0).pointCount - 1
        t += abs( this.series(0).points(j).Value )
    next
    return t
end function

function PSCHART.PieLabelText( byval iPt as long ) as DWSTRING
    if this.IsValidPoint( 0, iPt ) = false then return ""
    dim as PSCHART_POINT ptr p = @this.series(0).points(iPt)

    select case this.nPieLabelMode
    case CHT_PIELABEL_NONE
        return ""
    case CHT_PIELABEL_NAME
        return p->Text
    case CHT_PIELABEL_PERCENT, CHT_PIELABEL_NAME_PERCENT
        dim as double total = this.PieTotal()
        dim as long pct = 0
        if total > 0.0 then pct = clng( int( abs(p->Value) / total * 100.0 + 0.5 ) )
        dim as DWSTRING wsz = wstr(pct) & "%"
        if this.nPieLabelMode = CHT_PIELABEL_NAME_PERCENT then
            if len(p->Text) then wsz = p->Text & " " & wsz
        end if
        return wsz
    case else                                           ' CHT_PIELABEL_VALUE
        return this.FormatValue( p->Value, CHT_FMT_PIELABEL )
    end select
end function


' ========================================================================================
' PIE GEOMETRY: one sweep per point of series 0, plus each label's box and whether it fits.
'
' ANGLES ARE GDI+ DEGREES throughout -- 0 is 3 o'clock and a positive sweep runs CLOCKWISE.
' That is the opposite of the mathematical convention, and because screen y grows downward
' it means the ordinary x=cos/y=sin conversion works unchanged, with no sign flip. Getting
' this wrong produces a pie that is merely mirrored, which looks deliberate.
' ========================================================================================
sub PSCHART.LayoutPieSlices()
    if this.seriesCount <= 0 then exit sub
    if IsRectEmpty( @this.rcPie ) then exit sub

    const PI_ = 3.14159265358979323846

    dim as double total = this.PieTotal()
    dim as single ang = this.fPieStartAngle

    dim as long cx = (this.rcPie.left + this.rcPie.right) \ 2
    dim as long cy = (this.rcPie.top + this.rcPie.bottom) \ 2
    dim as single radius = csng( this.rcPie.right - this.rcPie.left ) / 2.0

    for j as long = 0 to this.series(0).pointCount - 1
        dim as PSCHART_POINT ptr p = @this.series(0).points(j)

        p->fStart = ang
        if total > 0.0 then
            p->fSweep = csng( abs(p->Value) / total * 360.0 )
        else
            p->fSweep = 0.0
        end if
        ang += p->fSweep

        p->bLabelOut = false
        p->bLabelHid = false
        SetRectEmpty( @p->rc )

        if p->fSweep <= 0.0 then continue for

        dim as single midDeg = p->fStart + p->fSweep / 2.0
        dim as double rad = csng(midDeg) * PI_ / 180.0
        dim as single ux = csng( cos(rad) )
        dim as single uy = csng( sin(rad) )

        ' The wedge's own centre, shifted along the bisector when it is exploded. Every
        ' derived position below is measured from HERE, not from the pie's centre, or an
        ' exploded slice's label stays behind with the rest of them.
        dim as single scx = csng(cx) + ux * csng(p->nExplode)
        dim as single scy = csng(cy) + uy * csng(p->nExplode)

        ' The outer midpoint of the arc: where a leader line starts and where the renderer
        ' anchors a hot highlight.
        p->ptPlot.x = clng( scx + ux * radius )
        p->ptPlot.y = clng( scy + uy * radius )

        dim as DWSTRING wsz = this.PieLabelText( j )
        if len(wsz) = 0 then continue for

        dim as long lw, lh
        CHT_MeasureText( this.hWin, this.hLabelFont, wsz, lw, lh )
        if (lw <= 0) orelse (lh <= 0) then continue for

        ' Inside labels sit at 65% of the radius -- far enough out that a thin wedge has
        ' widened, close enough in that a fat one does not touch the rim.
        dim as single rIn = radius * 0.65
        dim as single ilx = scx + ux * rIn
        dim as single ily = scy + uy * rIn

        ' Does it fit? The wedge's width at the label's radius is its chord there. Compared
        ' against the measured string rather than a character count, because "1" and "1,200"
        ' are not the same problem.
        dim as double halfSweep = csng(p->fSweep) / 2.0 * PI_ / 180.0
        dim as single chord = csng( 2.0 * rIn * sin(halfSweep) )
        dim as boolean bFits = (chord >= csng(lw)) andalso (rIn >= csng(lh))

        dim as boolean bOutside = false
        select case this.nPieLabelPlace
        case CHT_PIEPLACE_OUTSIDE
            bOutside = true
        case CHT_PIEPLACE_INSIDE
            if bFits = false then
                p->bLabelHid = true
                continue for
            end if
        case else                                       ' CHT_PIEPLACE_AUTO
            bOutside = (bFits = false)
        end select

        if bOutside then
            ' Outside: past the rim by the chrome gap, then the text box placed on whichever
            ' side of the leader the slice faces, so the run always points away from the pie.
            dim as single olx = scx + ux * (radius + csng(this.nChromeGap) * 2.0)
            dim as single oly = scy + uy * (radius + csng(this.nChromeGap) * 2.0)
            p->bLabelOut = true
            p->rc.top    = clng( oly ) - lh \ 2
            p->rc.bottom = p->rc.top + lh
            if ux >= 0.0 then
                p->rc.left = clng( olx )
                p->rc.right = p->rc.left + lw
            else
                p->rc.right = clng( olx )
                p->rc.left = p->rc.right - lw
            end if
        else
            p->rc.left   = clng( ilx ) - lw \ 2
            p->rc.right  = p->rc.left + lw
            p->rc.top    = clng( ily ) - lh \ 2
            p->rc.bottom = p->rc.top + lh
        end if
    next
end sub


' ########################################################################################
'  PUBLIC API
'
'  Flat C-style functions, PsChart_Verb, first argument always the control's HWND -- the
'  family's shape. The handle IS a real HWND: SetWindowPos, ShowWindow, EnableWindow and
'  friends all work on it and there is no wrapper for any of them.
'
'  PROGRAMMATIC SETTERS ARE SILENT. Nothing here fires a callback; only user interaction
'  does. That is what makes it safe to call any of these from inside a callback.
'
'  RAW PIXELS. Every setter that takes a size takes DEVICE pixels and the CALLER scales
'  (pWindow->ScaleX / ScaleY). Only the Create-time defaults are scaled for you.
' ########################################################################################

' --- creation and lifetime ------------------------------------------------------------------
' nChartType is one of the PSCHART_TYPE_* constants and is FIXED for the control's lifetime.
declare function PsChart_Create( byval hWndParent as HWND, byval CtrlID as long, _
                                 byval nChartType as long = PSCHART_TYPE_LINE ) as HWND
declare function PsChart_GetChartType( byval hChart as HWND ) as long

' --- data ------------------------------------------------------------------------------------
' Returns the new series' index, or -1 if hChart is not a chart.
declare function PsChart_AddSeries( byval hChart as HWND, byval wszName as DWSTRING = "" ) as long
declare function PsChart_GetSeriesCount( byval hChart as HWND ) as long
declare function PsChart_SetSeriesName( byval hChart as HWND, byval iSeries as long, byval wszName as DWSTRING ) as boolean
declare function PsChart_GetSeriesName( byval hChart as HWND, byval iSeries as long ) as DWSTRING
declare function PsChart_SetSeriesColor( byval hChart as HWND, byval iSeries as long, byval clr as COLORREF ) as boolean
' Drop an explicit series colour and go back to the palette entry for that index.
declare function PsChart_ClearSeriesColor( byval hChart as HWND, byval iSeries as long ) as boolean
declare function PsChart_SetSeriesLineWidth( byval hChart as HWND, byval iSeries as long, byval nWidth as long ) as boolean
' LINE SERIES ONLY -- a column or HLOC chart has no polyline to break and ignores it. One of
' the CHT_LINE_* constants; an unrecognised one is stored as CHT_LINE_SOLID.
declare function PsChart_SetSeriesLineStyle( byval hChart as HWND, byval series as integer, byval nStyle as long ) as boolean
declare function PsChart_GetSeriesLineStyle( byval hChart as HWND, byval series as integer ) as long
declare function PsChart_SetSeriesSymbol( byval hChart as HWND, byval iSeries as long, byval nSymbol as long, byval nSize as long = PSCHART_DEFAULT_SYMBOLSIZE ) as boolean
declare function PsChart_SetSeriesSmooth( byval hChart as HWND, byval iSeries as long, byval bSmooth as boolean ) as boolean
' A hidden series is excluded from the AXIS RANGE as well as from the drawing, so hiding the
' outlier rescales the chart rather than leaving empty headroom where it used to be.
declare function PsChart_SetSeriesVisible( byval hChart as HWND, byval iSeries as long, byval bVisible as boolean ) as boolean
declare function PsChart_IsSeriesVisible( byval hChart as HWND, byval iSeries as long ) as boolean

' Returns the new point's index within the series, or -1.
declare function PsChart_AddPoint( byval hChart as HWND, byval iSeries as long, _
                                   byval wszLabel as DWSTRING, byval fValue as double ) as long
declare function PsChart_AddHlocPoint( byval hChart as HWND, byval iSeries as long, _
                                       byval wszLabel as DWSTRING, byval fOpenVal as double, _
                                       byval fHighVal as double, byval fLowVal as double, _
                                       byval fCloseVal as double ) as long
declare function PsChart_GetPointCount( byval hChart as HWND, byval iSeries as long ) as long
declare function PsChart_SetPointValue( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval fValue as double ) as boolean
declare function PsChart_GetPointValue( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as double
declare function PsChart_SetPointLabel( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval wszLabel as DWSTRING ) as boolean
declare function PsChart_GetPointLabel( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as DWSTRING
declare function PsChart_SetPointColor( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval clr as COLORREF ) as boolean
declare function PsChart_ClearPointColor( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as boolean
declare function PsChart_SetPointItemData( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byval nData as LONG_PTR ) as boolean
declare function PsChart_GetPointItemData( byval hChart as HWND, byval iSeries as long, byval iPoint as long ) as LONG_PTR
' Pie only. Raw pixels of offset along the slice's bisector; the layout reserves room for the
' largest one before it cuts the pie's square, so an exploded slice cannot leave the control.
declare function PsChart_SetPointExplode( byval hChart as HWND, byval iPoint as long, byval nOffset as long ) as boolean

' Drop every point of one series, keeping the series and its styling.
declare function PsChart_ClearPoints( byval hChart as HWND, byval iSeries as long ) as boolean
' Drop everything: series, points, hot state.
declare sub      PsChart_Clear( byval hChart as HWND )

' Bulk population. NESTS -- an inner pair does not end an outer batch. Between them the
' control does not invalidate, so loading a thousand points costs one layout and one repaint.
declare sub      PsChart_BeginUpdate( byval hChart as HWND )
declare sub      PsChart_EndUpdate( byval hChart as HWND )

' --- value axis -------------------------------------------------------------------------------
declare sub      PsChart_SetAutoScale( byval hChart as HWND, byval bAuto as boolean )
declare function PsChart_GetAutoScale( byval hChart as HWND ) as boolean
' Turns autoscaling OFF as a side effect -- stating a range and having it ignored is the
' worse surprise of the two.
declare sub      PsChart_SetValueRange( byval hChart as HWND, byval fLo as double, byval fHi as double )
' The RESOLVED range, autoscaled or not. Forces a pending layout, so it is always current.
declare sub      PsChart_GetValueRange( byval hChart as HWND, byref fLo as double, byref fHi as double )
declare sub      PsChart_SetTickCountHint( byval hChart as HWND, byval nTicks as long )
declare sub      PsChart_SetGridLines( byval hChart as HWND, byval bHorz as boolean, byval bVert as boolean )
' nDecimals PSCHART_DECIMALS_AUTO derives the count from the axis step.
declare sub      PsChart_SetValueFormat( byval hChart as HWND, byval nDecimals as long, byval bThousands as boolean )
' 0 (PSCHART_SKIP_AUTO) measures the labels and works the stride out; 1 draws every label; N
' draws every Nth.
declare sub      PsChart_SetAxisLabelSkip( byval hChart as HWND, byval nSkip as long )
declare function PsChart_GetAxisLabelSkip( byval hChart as HWND ) as long

' --- chrome ------------------------------------------------------------------------------------
declare sub      PsChart_SetTitle( byval hChart as HWND, byval wszText as DWSTRING )
declare function PsChart_GetTitle( byval hChart as HWND ) as DWSTRING
declare sub      PsChart_SetSubtitle( byval hChart as HWND, byval wszText as DWSTRING )
declare function PsChart_GetSubtitle( byval hChart as HWND ) as DWSTRING
declare sub      PsChart_SetLegendVisible( byval hChart as HWND, byval bVisible as boolean )
declare function PsChart_IsLegendVisible( byval hChart as HWND ) as boolean
declare sub      PsChart_SetLegendPosition( byval hChart as HWND, byval nPos as long )
declare sub      PsChart_SetPadding( byval hChart as HWND, byval nPadding as long )
' Borrowed, never owned: the host destroys these. Unset title/subtitle fonts fall back to the
' label font.
declare sub      PsChart_SetFont( byval hChart as HWND, byval hFont as HFONT )
declare sub      PsChart_SetTitleFont( byval hChart as HWND, byval hFont as HFONT )
declare sub      PsChart_SetSubtitleFont( byval hChart as HWND, byval hFont as HFONT )

' --- column -------------------------------------------------------------------------------------
declare sub      PsChart_SetBarGap( byval hChart as HWND, byval nGap as long )
' Percent of each category slot left empty BETWEEN groups. Clamped 0..90.
declare sub      PsChart_SetGroupGapPercent( byval hChart as HWND, byval nPct as long )

' --- HLOC ---------------------------------------------------------------------------------------
declare sub      PsChart_SetHlocTickWidth( byval hChart as HWND, byval nWidth as long )
' Off by default: the reference HLOC chart draws every bar in one colour. On, a bar closing
' above its open takes UpColor and one closing below takes DownColor.
declare sub      PsChart_SetHlocUpDownColors( byval hChart as HWND, byval bEnable as boolean )

' --- pie ----------------------------------------------------------------------------------------
' 0.0 = a solid pie. Clamped 0.0..0.9.
declare sub      PsChart_SetDoughnutRatio( byval hChart as HWND, byval fRatio as single )
declare function PsChart_GetDoughnutRatio( byval hChart as HWND ) as single
declare sub      PsChart_SetPieLabelMode( byval hChart as HWND, byval nMode as long )
declare sub      PsChart_SetPieLabelPlacement( byval hChart as HWND, byval nPlace as long )
' GDI+ degrees: 0 is 3 o'clock, positive runs clockwise. -90 (the default) starts at 12.
declare sub      PsChart_SetPieStartAngle( byval hChart as HWND, byval fDegrees as single )
declare sub      PsChart_SetPieSliceGap( byval hChart as HWND, byval nGap as long )

' --- colours -------------------------------------------------------------------------------------
' Read-modify-write: Get the struct, change the fields you care about, Set it back.
declare sub      PsChart_GetColors( byval hChart as HWND, byval pColors as PSCHART_COLORS ptr )
declare sub      PsChart_SetColors( byval hChart as HWND, byval pColors as PSCHART_COLORS ptr )
declare sub      PsChart_SetPaletteColor( byval hChart as HWND, byval idx as long, byval clr as COLORREF )
declare function PsChart_GetPaletteColor( byval hChart as HWND, byval idx as long ) as COLORREF

' --- query ---------------------------------------------------------------------------------------
' The data point under a CLIENT-coordinate point. Returns false and leaves both indices at -1
' when nothing is there. Forces a pending layout, so it never answers from stale geometry.
declare function PsChart_HitTest( byval hChart as HWND, byval x as long, byval y as long, _
                                  byref iSeries as long, byref iPoint as long ) as boolean
declare function PsChart_GetPlotRect( byval hChart as HWND, byref rc as RECT ) as boolean
declare function PsChart_GetPointRect( byval hChart as HWND, byval iSeries as long, byval iPoint as long, byref rc as RECT ) as boolean
declare function PsChart_ValueToY( byval hChart as HWND, byval fValue as double ) as long
declare function PsChart_YToValue( byval hChart as HWND, byval y as long ) as double
declare function PsChart_GetHotPoint( byval hChart as HWND, byref iSeries as long, byref iPoint as long ) as boolean

' --- tooltips -------------------------------------------------------------------------------------
' Authored text wins over the tooltip callback, which wins over the per-point default.
declare sub      PsChart_SetTooltipText( byval hChart as HWND, byval wszText as DWSTRING )
declare function PsChart_GetTooltipText( byval hChart as HWND ) as DWSTRING
declare sub      PsChart_SetTooltipMode( byval hChart as HWND, byval nMode as long )
declare function PsChart_GetTooltipMode( byval hChart as HWND ) as long

' --- callbacks ------------------------------------------------------------------------------------
declare sub      PsChart_SetPaintCallback( byval hChart as HWND, byval userfunc as CHT_PaintCallbackSub )
declare sub      PsChart_SetMessageCallback( byval hChart as HWND, byval userfunc as CHT_MessageCallbackFunc )
declare sub      PsChart_SetTooltipCallback( byval hChart as HWND, byval userfunc as CHT_TooltipCallbackFunc )
declare sub      PsChart_SetValueLabelCallback( byval hChart as HWND, byval userfunc as CHT_ValueLabelCallbackFunc )
declare sub      PsChart_SetHotChangeCallback( byval hChart as HWND, byval userfunc as CHT_HotChangeCallbackSub )

' --- diagnostics ------------------------------------------------------------------------------------
' Render the control into an OFFSCREEN buffer through the real paint path and report on the
' pixels. The render body is split out of WM_PAINT precisely so these drive the same code the
' screen does -- PsDatePicker once shipped a probe that passed while the painter drew a flat
' rectangle, and that is the failure these are shaped to make impossible.
declare function PsChart_CountRenderedTones( byval hChart as HWND ) as long
' Count the pixels inside the PLOT RECT that are NOT clrBack -- the series' ink, in other
' words, when clrBack is the plot's own background. Every pixel of the rect, not a sample
' grid: this answers "how much of the line got drawn", and a 32x32 grid over a plot is far
' too coarse to see a dash pattern at all. Its cost is a GetPixel per plot pixel, so it
' belongs in a test and nowhere near a repaint.
declare function PsChart_CountPlotInkPixels( byval hChart as HWND, byval clrBack as COLORREF ) as long
declare function PsChart_HashRenderedPart( byval hChart as HWND ) as ulong
declare function PsChart_RunSelfTest( byval hWndParent as HWND ) as long
