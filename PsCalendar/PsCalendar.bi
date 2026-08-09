'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsCalendar.bi  --  owner-drawn embedded month calendar (non-popup)
''
''  A plain WS_CHILD control: one real HWND, no child windows, no second top-level window,
''  and therefore NO host message-pump obligation at all (there is no FilterMessage).
''
''  Ancestry: the calendar popup inside PsDatePicker. The date arithmetic, the day grid and
''  the MONTHS/YEARS drill-down come from there; the focus/keyboard model, the N x M panel
''  grid and the shared nav band do not -- a popup never needed them.
''
''  PsCalendar and PsDatePicker are deliberately collision-free and may be included in the
''  SAME module: every identifier here is CAL_* / PSCALENDAR_* / PsCalendar_* / CCAL_*.
''

#pragma once

#include once "PsBufferPaint.bi"
' The tooltip backend switch. This control's tip is a TRACKED one -- the control decides
' when and where -- which PsTipHost supports as its third shape; see PsTipHost_TrackShow.
#include once "PsTipHost.bi"


' ----------------------------------------------------------------------------------------
' Timer id (per-window, so every instance can share it).
'   HOTTRACK - the hover safety net. WM_MOUSELEAVE is unreliable (it is missed when another
'              window is created over us, and after certain capture transitions), so the hot
'              cell is re-resolved on a poll as well. Every recent sibling except the
'              display-only PsProgressBar carries one.
' ----------------------------------------------------------------------------------------
#define IDT_PSCALENDAR_HOTTRACK   &hCB95
#define PSCALENDAR_HOTTRACK_MS     100


' ----------------------------------------------------------------------------------------
' Defaults. SIZE values are DPI-scaled ONCE at Create; every setter afterwards takes raw
' pixels and the caller scales (the family rule).
'
' THE THREE THICKNESSES ARE NOT SCALED, ANYWHERE. PsBufferPaint.PaintLine and
' PaintRoundOutline both call pWindow->ScaleY() on the pen width they are handed
' (PsBufferPaint.inc, PaintLine), so pre-scaling here would scale them TWICE and a hairline
' would come out two pixels thick at 175%. This is PsOptionButton's rule, and it is the one
' thing in this file that no assertion running at 100% DPI can catch.
' ----------------------------------------------------------------------------------------
#define CCAL_PAD                 8     ' outer padding, all four sides (inside the border)
#define CCAL_NAVBANDH           34     ' the shared prev / title / next band
#define CCAL_NAVGAP              4     ' between the nav band and the panel grid
#define CCAL_CHEVW              28     ' width of each chevron cell inside the nav band
#define CCAL_GUTTERX            12     ' between month panels, horizontally
#define CCAL_GUTTERY            10     ' between month panels, vertically
#define CCAL_CAPTIONH           24     ' a panel's own month caption (suppressed at 1 panel)
#define CCAL_WEEKDAYH           22     ' the S M T W T F S row
#define CCAL_CELLMINW           34     ' one day cell, for GetIdealSize only -- cells STRETCH
#define CCAL_CELLMINH           30

#define CCAL_DEFAULT_BORDERTHICK 0     ' NOT DPI-scaled. 0 = no frame (the embedded default)
#define CCAL_DEFAULT_CHEVTHICK   2     ' NOT DPI-scaled -- PaintLine scales it
#define CCAL_DEFAULT_FOCUSTHICK  1     ' NOT DPI-scaled -- PaintRoundOutline scales it

' The month/year drill-down grid: 3 columns x 4 rows keeps long month names uncrowded, and
' one formula serves both grids.
#define CCAL_MY_COLS             3
#define CCAL_MY_ROWS             4

' HARD YEAR LIMITS. SYSTEMTIME.wYear is a WORD, so a year walked below 1 does not go
' negative -- it WRAPS to ~65535 and the calendar silently starts counting down from there.
' Navigation is therefore clamped to this range and the chevron goes dead at the limit,
' exactly as it does at a host-supplied stMin/stMax. 1..9999 is the range every date UI uses.
'
' The public PsCalendar_AddMonths / _AddDays helpers are deliberately NOT clamped -- they are
' pure arithmetic and a caller may legitimately want a serial outside this window. Everything
' that writes the anchor or the selection goes through the clamp instead.
#define CCAL_MIN_YEAR            1
#define CCAL_MAX_YEAR         9999

' Panel grid cap. 3x4 = a full year is the realistic maximum for one surface, and 12 is
' already the drill-down grid's cell count. Fixing it lets every layout array be STATIC:
' no redim, no allocation on the layout path, and a shape the assertions can be written
' against. SetPanelGrid rejects anything larger.
#define CCAL_MAX_PANELS         12


' Week start.
enum
    CAL_SUNDAY = 0
    CAL_MONDAY = 1
end enum

' View mode (Win32 MonthCal-style drill-down).
enum
    CAL_VIEW_DAYS = 0
    CAL_VIEW_MONTHS
    CAL_VIEW_YEARS
end enum

' Hit-test parts.
enum
    CAL_HIT_NONE = 0
    CAL_HIT_PREV
    CAL_HIT_NEXT
    CAL_HIT_TITLE          ' the nav band's title -- zooms out
    CAL_HIT_DAYCELL        ' panel 0..11, index 0..41
    CAL_HIT_MONTHCELL      ' index 0..11
    CAL_HIT_YEARCELL       ' index 0..11
end enum

' Part codes for the offscreen render probes (PsCalendar_TestPartTones / TestPartBrightness /
' TestHashPart). Public because a host writing its own painter should be able to assert that
' it is not flooding the control -- PsButton's precedent.
enum
    CAL_PART_CLIENT  = 0
    CAL_PART_NAVBAND = 1
    CAL_PART_PREV    = 2
    CAL_PART_NEXT    = 3
    CAL_PART_TITLE   = 4
    CAL_PART_GRID    = 5
end enum
' Composite codes: a drill-down cell is CAL_PART_MYCELL_BASE + k (k = 0..11); a day cell is
' CAL_PART_DAYCELL_BASE + panel*100 + k (k = 0..41).
#define CAL_PART_MYCELL_BASE    100
#define CAL_PART_DAYCELL_BASE  1000


' A resolved hit.
type CAL_HITRESULT
    part   as long = CAL_HIT_NONE
    panel  as long = -1        ' 0..11 for day cells; 0 for month/year cells
    index  as long = -1        ' 0..41 (days) / 0..11 (months, years)
end type


' ----------------------------------------------------------------------------------------
' Colors for the built-in painter. Copied on Set. Read-modify-write is
' PsCalendar_GetColors, assign the fields you care about, PsCalendar_SetColors.
'
' INHERITED VERBATIM from PsDatePicker's calendar palette (dark), which was tuned against a
' reference screenshot for exactly this widget -- minus its field/icon/footer colours, plus
' CaptionColor and FocusRingColor which this control needs and the popup had no use for.
' "Inherited, seen and accepted" -- a weaker claim than "matched to a reference".
' ----------------------------------------------------------------------------------------
type PSCALENDAR_COLORS
    ' --- chrome ---
    BackColor              as COLORREF = BGR( 33, 37, 43)
    BorderColor            as COLORREF = BGR( 70, 76, 88)   ' only drawn when nBorderThick > 0
    FocusRingColor         as COLORREF = BGR( 86,156,214)   ' around the focused cell
    TitleColor             as COLORREF = BGR(225,228,234)
    TitleColorHot          as COLORREF = BGR(255,255,255)
    CaptionColor           as COLORREF = BGR(200,205,213)   ' a panel's own month caption
    NavGlyphColor          as COLORREF = BGR(190,196,206)
    NavGlyphColorHot       as COLORREF = BGR(255,255,255)
    NavGlyphColorDisabled  as COLORREF = BGR( 70, 74, 82)
    WeekdayColor           as COLORREF = BGR(140,146,156)
    ' --- day cells ---
    DayColor               as COLORREF = BGR(210,214,220)   ' ordinary weekday, current month
    DayColorWeekend        as COLORREF = BGR(150,175,205)   ' Sat/Sun, current month
    ' THESE TWO MUST STAY CLEARLY APART, and the gap is not cosmetic. An ADJACENT day is
    ' dimmed but still CLICKABLE (clicking it selects that date and navigates); a DISABLED day
    ' is outside the range and inert. They originally sat 34 luminance levels apart, which read
    ' as the same grey -- a range like "the next 60 days" then showed most of the month greyed
    ' with no way to tell what could be clicked. Asserted at the PIXEL level, as a RATIO.
    DayColorAdjacent       as COLORREF = BGR(124,131,142)   ' leading/trailing, still clickable
    DayColorDisabled       as COLORREF = BGR( 56, 60, 68)   ' out of range, inert
    DayBackColorHot        as COLORREF = BGR( 52, 58, 68)
    DayForeColorHot        as COLORREF = BGR(255,255,255)
    DayBackColorSelected   as COLORREF = BGR(  0,120,215)
    DayForeColorSelected   as COLORREF = BGR(255,255,255)
    TodayRingColor         as COLORREF = BGR( 86,156,214)   ' subtle outline on today's cell
end type


' ----------------------------------------------------------------------------------------
' Paint info for the WHOLESALE PaintCallback, which replaces the entire built-in renderer.
' Every rect is already laid out; the callback draws into b and returns.
' The control fills the client with BackColor before calling.
' ----------------------------------------------------------------------------------------
type PSCALENDAR_PAINTINFO
    hCtrl          as HWND
    b              as PsBufferPaint ptr
    rcClient       as RECT
    rcInner        as RECT      ' client less border and padding
    rcNavBand      as RECT
    rcPrev         as RECT
    rcTitle        as RECT
    rcNext         as RECT
    rcGrid         as RECT      ' the whole panel-grid region (DAYS) / drill-down region
    eView          as long      ' CAL_VIEW_*
    nCols          as long
    nRows          as long
    panelCount     as long      ' nCols * nRows
    isEnabled      as boolean
    isFocused      as boolean
    isPrevDisabled as boolean
    isNextDisabled as boolean
    hotPart        as long      ' CAL_HIT_*
    hotPanel       as long
    hotIndex       as long
end type


' ----------------------------------------------------------------------------------------
' Paint info for ONE day cell, handed to an optional DayPaintCallback. THIS is the granular
' hook: the flags are exactly what a host needs to render a day its own way (availability
' shading, appointment dots, a bold deadline). If unset the built-in painter draws it.
'
' The control has already filled the whole client with BackColor, so a callback that draws
' nothing leaves an empty cell rather than garbage. The font is set to the day font before
' each call, so a callback starts from a known state.
'
' hCtrl is THIS control's HWND. (PsDatePicker set the equivalent field to the owning picker,
' which was confusing for a host that had both handles.)
' ----------------------------------------------------------------------------------------
type PSCALENDAR_DAYPAINTINFO
    hCtrl       as HWND
    b           as PsBufferPaint ptr
    rcCell      as RECT
    stDate      as SYSTEMTIME  ' the calendar date this cell represents
    dayNumber   as long        ' 1..31 (the number to draw)
    panelIndex  as long        ' 0..11, row-major within the panel grid
    cellIndex   as long        ' 0..41 within that panel
    isAdjacent  as boolean     ' a leading/trailing day from a neighbouring month (dim it)
    isWeekend   as boolean     ' Saturday or Sunday (from the cell's own date)
    isToday     as boolean
    isSelected  as boolean
    isHot       as boolean     ' the mouse is over this cell
    isFocused   as boolean     ' this is the focus cell AND the control has focus
    isDisabled  as boolean     ' outside the min/max range (unclickable)
    isEnabled   as boolean     ' the control as a whole
end type


' ----------------------------------------------------------------------------------------
' Paint info for ONE drill-down cell (a month in MONTHS view, a year in YEARS view).
' ----------------------------------------------------------------------------------------
type PSCALENDAR_MYPAINTINFO
    hCtrl       as HWND
    b           as PsBufferPaint ptr
    rcCell      as RECT
    eView       as long        ' CAL_VIEW_MONTHS or CAL_VIEW_YEARS
    cellIndex   as long        ' 0..11
    nValue      as long        ' month 1..12, or the four-digit year
    wszLabel    as DWSTRING    ' what the built-in painter would draw
    isSelected  as boolean
    isHot       as boolean
    isFocused   as boolean
    isDimmed    as boolean     ' YEARS view only: the leading/trailing decade cells
    isEnabled   as boolean
end type


type PSCALENDAR_MESSAGEINFO
    hCtrl       as HWND
    uMsg        as UINT
    wParam      as WPARAM
    lParam      as LPARAM
end type

type PSCALENDAR_TOOLTIPINFO
    hCtrl       as HWND
    stDate      as SYSTEMTIME  ' the day under the cursor
    isValid     as boolean     ' false when the cursor is not over a day cell
    outText     as DWSTRING    ' fill this with the tip text (empty = no tip)
end type


' ----------------------------------------------------------------------------------------
' Callback typedefs (CAL_ prefix -- confirmed unused family-wide).
'
' NOTE ON DWSTRING PARAMETERS: they are BYVAL, and that is not a style choice. A
' `byref ... as const DWSTRING` parameter whose value is then COPIED corrupts the process
' heap -- measured, 0xC0000374 STATUS_HEAP_CORRUPTION, in a 30-line program with no windows
' in it. AfxNova's DWSTRING copy constructor is not const-correct. Treat any
' `const DWSTRING` in this tree as a bug. DO NOT "optimise" one back to a const reference.
' ----------------------------------------------------------------------------------------

' The selection moved because of USER interaction -- a click on a day, or an arrow / PgUp /
' PgDn / Home / End keystroke. Programmatic setters are SILENT.
' bHasDate is false when the selection was cleared.
type CAL_SelChangeCallbackSub as sub( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr, byval bHasDate as boolean )

' The user CHOSE a date: double-click, Enter, or Space. This is the "act on it" signal -- a
' host closes its dialog or opens the record here. It fires AFTER SelChange when a
' double-click also moved the selection.
type CAL_DateActivatedCallbackSub as sub( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr )

' Draw the WHOLE control INSTEAD OF the built-in renderer.
type CAL_PaintCallbackSub as sub( byval p as PSCALENDAR_PAINTINFO ptr )

' Draw ONE day cell. If unset the built-in painter draws it.
type CAL_DayPaintCallbackSub as sub( byval p as PSCALENDAR_DAYPAINTINFO ptr )

' Draw ONE drill-down (month / year) cell. If unset the built-in painter draws it.
type CAL_MonthYearPaintCallbackSub as sub( byval p as PSCALENDAR_MYPAINTINFO ptr )

' Observe messages. Return TRUE to suppress default handling. The result is IGNORED for
' WM_KILLFOCUS, WM_DESTROY and WM_NCDESTROY -- a callback must not be able to strand the
' control in a focused or half-destroyed state.
type CAL_MessageCallbackFunc as function( byval m as PSCALENDAR_MESSAGEINFO ptr ) as boolean

' Supply a tooltip for the day under the cursor. Return TRUE if you filled outText.
' Day cells only. The tooltip window is created lazily and ONLY if this is set.
type CAL_TooltipCallbackFunc as function( byval t as PSCALENDAR_TOOLTIPINFO ptr ) as boolean


' ========================================================================================
' Per-instance state. Stored in the CWindow UserData area, reached via GetCalendarPointer.
'
' NOTE the font field names: FreeBASIC is CASE-INSENSITIVE, so a member named `hFont` would
' shadow the TYPE `HFONT` for every member declared after it. Only the first HFONT in a TYPE
' may carry that name, and with three of them none does.
' ========================================================================================
type PSCALENDAR
    hWin            as HWND
    idc_Calendar    as long = 0

    ' --- state ---
    isEnabled       as boolean = true
    isFocused       as boolean = false
    eView           as long = CAL_VIEW_DAYS

    ' --- model ---
    stAnchor        as SYSTEMTIME         ' FIRST displayed month (canonical day = 1)
    stSelected      as SYSTEMTIME
    hasSelection    as boolean = false    ' no-selection is a LEGAL state
    stToday         as SYSTEMTIME
    stMin           as SYSTEMTIME
    stMax           as SYSTEMTIME
    hasMin          as boolean = false
    hasMax          as boolean = false

    ' --- options ---
    nWeekStart      as long = CAL_SUNDAY
    nCols           as long = 1           ' panel grid, nCols * nRows <= CCAL_MAX_PANELS
    nRows           as long = 1
    nNavStep        as long = 0           ' 0 = one page (panelCount months); n > 0 = n months
    bShowTodayRing  as boolean = true

    ' --- hot cell (single-valued) ---
    hotPart         as long = CAL_HIT_NONE
    hotPanel        as long = -1
    hotIndex        as long = -1

    ' --- keyboard focus inside the drill-down grid (DAYS view uses the selection instead) ---
    nMyFocus        as long = 0           ' 0..11

    ' --- housekeeping ---
    ' Set when a WM_KEYDOWN we consumed will be followed by a WM_CHAR (Enter, Space, Esc), so
    ' that character can be swallowed too. Deciding it from the flag rather than re-reading the
    ' control's state matters: the keydown for Esc has ALREADY changed eView by the time its
    ' WM_CHAR arrives, so a state test would look at the wrong view and let the beep through.
    bAteKeyDown     as boolean = false
    hotTimerOn      as boolean = false
    nWheelAccum     as long = 0
    ' The tooltip, whichever backend it is on. Still LAZY -- built on the first day cell
    ' that actually wants a tip, never for a calendar whose host set no callback.
    tip             as PSTIPHOST
    tipKey          as longint = -1       ' serial of the day the tip shows (-1 = hidden)

    ' --- appearance (all HFONTs are CALLER-OWNED and never deleted here) ---
    colors          as PSCALENDAR_COLORS
    hMainFont       as HFONT = 0          ' day numbers, drill-down cells, panel captions
    hTitleFont      as HFONT = 0          ' 0 = fall back to hMainFont
    hWeekdayFont    as HFONT = 0          ' 0 = fall back to hMainFont

    ' --- localization. An empty element means "use the built-in English default", so a host
    ' may override any subset. Weekday index is ALWAYS 0 = Sunday, whatever nWeekStart is.
    wszMonthName(1 to 12)     as DWSTRING
    wszMonthAbbrev(1 to 12)   as DWSTRING
    wszWeekdayAbbrev(0 to 6)  as DWSTRING

    ' --- layout inputs (DPI-scaled once at Create; raw pixels thereafter) ---
    nPad            as long = CCAL_PAD
    nNavBandH       as long = CCAL_NAVBANDH
    nNavGap         as long = CCAL_NAVGAP
    nChevW          as long = CCAL_CHEVW
    nGutterX        as long = CCAL_GUTTERX
    nGutterY        as long = CCAL_GUTTERY
    nCaptionH       as long = CCAL_CAPTIONH
    nWeekdayH       as long = CCAL_WEEKDAYH
    nCellMinW       as long = CCAL_CELLMINW
    nCellMinH       as long = CCAL_CELLMINH
    ' NOT DPI-scaled -- the paint primitives scale the pen themselves (see the banner).
    nBorderThick    as long = CCAL_DEFAULT_BORDERTHICK
    nChevThick      as long = CCAL_DEFAULT_CHEVTHICK
    nFocusThick     as long = CCAL_DEFAULT_FOCUSTHICK

    ' --- layout outputs (DERIVED; LayoutCalendar owns them all) ---
    rcClient        as RECT
    rcInner         as RECT               ' client less border and padding
    rcNavBand       as RECT
    rcPrev          as RECT
    rcTitle         as RECT
    rcNext          as RECT
    rcGrid          as RECT               ' everything below the nav band
    rcPanel  (0 to CCAL_MAX_PANELS-1)              as RECT
    rcCaption(0 to CCAL_MAX_PANELS-1)              as RECT   ' zero-height at 1 panel
    rcBody   (0 to CCAL_MAX_PANELS-1)              as RECT   ' below the weekday row
    rcWeekday(0 to CCAL_MAX_PANELS-1, 0 to 6)      as RECT
    rcDayCell(0 to CCAL_MAX_PANELS-1, 0 to 41)     as RECT   ' 6x7 = 42, FIXED
    rcMyGrid        as RECT               ' drill-down region: the WHOLE of rcGrid
    rcMyCell (0 to 11)                             as RECT
    ' derived per-panel date origin (so cell->date needs no re-measure)
    panelYear (0 to CCAL_MAX_PANELS-1)             as long
    panelMonth(0 to CCAL_MAX_PANELS-1)             as long   ' 1..12
    panelLead (0 to CCAL_MAX_PANELS-1)             as long   ' column of the 1st
    cellW     (0 to CCAL_MAX_PANELS-1)             as long
    rowH      (0 to CCAL_MAX_PANELS-1)             as long

    nIdealW         as long = 0
    nIdealH         as long = 0
    bLayoutDirty    as boolean = true

    ' --- callbacks ---
    SelChangeCallback      as CAL_SelChangeCallbackSub
    DateActivatedCallback  as CAL_DateActivatedCallbackSub
    PaintCallback          as CAL_PaintCallbackSub
    DayPaintCallback       as CAL_DayPaintCallbackSub
    MonthYearPaintCallback as CAL_MonthYearPaintCallbackSub
    MessageCallback        as CAL_MessageCallbackFunc
    TooltipCallback        as CAL_TooltipCallbackFunc

    declare sub      LayoutCalendar()
    declare function PanelCount() as long
    declare function CellDate( byval panel as long, byval k as long ) as SYSTEMTIME
end type


' ========================================================================================
' PUBLIC API   (full contracts in the .inc banner)
' ========================================================================================

' --- creation ---
declare function PsCalendar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' --- value (ALL SILENT; only user interaction notifies) ---
declare function PsCalendar_GetSelDate( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean
declare sub      PsCalendar_SetSelDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
declare sub      PsCalendar_ClearSelDate( byval hCtrl as HWND )
declare function PsCalendar_HasSelDate( byval hCtrl as HWND ) as boolean
declare sub      PsCalendar_GetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byref bHasMin as boolean, byref bHasMax as boolean )
declare sub      PsCalendar_SetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byval bHasMin as boolean, byval bHasMax as boolean )
declare function PsCalendar_GetToday( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean

' --- displayed page ---
declare function PsCalendar_GetAnchorDate( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean
declare sub      PsCalendar_SetAnchorDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
declare function PsCalendar_GetView( byval hCtrl as HWND ) as long
declare sub      PsCalendar_SetView( byval hCtrl as HWND, byval eView as long )
' The nav band's caption for the current view and page ("January 2026", "Jan - Dec 2026",
' "2026", "2020 - 2029"). Public so a host can mirror it and the self-test can assert the
' localization actually reaches the output without reading pixels.
declare function PsCalendar_GetTitleText( byval hCtrl as HWND ) as DWSTRING

' --- behaviour ---
declare function PsCalendar_GetWeekStart( byval hCtrl as HWND ) as long
declare sub      PsCalendar_SetWeekStart( byval hCtrl as HWND, byval nWeekStart as long )
declare sub      PsCalendar_GetPanelGrid( byval hCtrl as HWND, byref nCols as long, byref nRows as long )
' Rejected (and left unchanged) when nCols*nRows exceeds CCAL_MAX_PANELS or either is < 1.
declare sub      PsCalendar_SetPanelGrid( byval hCtrl as HWND, byval nCols as long, byval nRows as long )
declare function PsCalendar_GetPanelCount( byval hCtrl as HWND ) as long
' 0 = a chevron steps one whole PAGE (panelCount months); n > 0 = n months. Default 0, so a
' single-panel calendar steps a month and a 3x4 year grid steps a year, with no host code.
declare function PsCalendar_GetNavStep( byval hCtrl as HWND ) as long
declare sub      PsCalendar_SetNavStep( byval hCtrl as HWND, byval nStep as long )
declare function PsCalendar_GetShowTodayRing( byval hCtrl as HWND ) as boolean
declare sub      PsCalendar_SetShowTodayRing( byval hCtrl as HWND, byval bShow as boolean )

' --- localization. Empty string = revert that entry to the built-in English default. ---
declare function PsCalendar_GetMonthName( byval hCtrl as HWND, byval m as long ) as DWSTRING
declare sub      PsCalendar_SetMonthName( byval hCtrl as HWND, byval m as long, byval sName as DWSTRING )
declare function PsCalendar_GetMonthAbbrev( byval hCtrl as HWND, byval m as long ) as DWSTRING
declare sub      PsCalendar_SetMonthAbbrev( byval hCtrl as HWND, byval m as long, byval sName as DWSTRING )
' i is ALWAYS 0 = Sunday .. 6 = Saturday, independent of nWeekStart.
declare function PsCalendar_GetWeekdayAbbrev( byval hCtrl as HWND, byval i as long ) as DWSTRING
declare sub      PsCalendar_SetWeekdayAbbrev( byval hCtrl as HWND, byval i as long, byval sName as DWSTRING )
' Bulk convenience wrappers over the three setters above. Elements are read from lbound()
' upward and any missing tail is left alone.
declare sub      PsCalendar_SetMonthNames( byval hCtrl as HWND, sNames() as DWSTRING )
declare sub      PsCalendar_SetMonthAbbrevs( byval hCtrl as HWND, sNames() as DWSTRING )
declare sub      PsCalendar_SetWeekdayAbbrevs( byval hCtrl as HWND, sNames() as DWSTRING )

' --- layout / appearance ---
declare function PsCalendar_GetFont( byval hCtrl as HWND ) as HFONT
declare sub      PsCalendar_SetFont( byval hCtrl as HWND, byval hFont as HFONT )   ' sets all three
declare function PsCalendar_GetTitleFont( byval hCtrl as HWND ) as HFONT
declare sub      PsCalendar_SetTitleFont( byval hCtrl as HWND, byval hFont as HFONT )
declare function PsCalendar_GetWeekdayFont( byval hCtrl as HWND ) as HFONT
declare sub      PsCalendar_SetWeekdayFont( byval hCtrl as HWND, byval hFont as HFONT )
declare function PsCalendar_GetBorderThickness( byval hCtrl as HWND ) as long
declare sub      PsCalendar_SetBorderThickness( byval hCtrl as HWND, byval nThickness as long )
declare function PsCalendar_GetChevronThickness( byval hCtrl as HWND ) as long
declare sub      PsCalendar_SetChevronThickness( byval hCtrl as HWND, byval nThickness as long )
declare function PsCalendar_GetPadding( byval hCtrl as HWND ) as long
declare sub      PsCalendar_SetPadding( byval hCtrl as HWND, byval nPad as long )
declare sub      PsCalendar_GetGutters( byval hCtrl as HWND, byref nGutterX as long, byref nGutterY as long )
declare sub      PsCalendar_SetGutters( byval hCtrl as HWND, byval nGutterX as long, byval nGutterY as long )
declare sub      PsCalendar_GetCellMinSize( byval hCtrl as HWND, byref nW as long, byref nH as long )
declare sub      PsCalendar_SetCellMinSize( byval hCtrl as HWND, byval nW as long, byval nH as long )
' PURE SCALAR MATH -- takes no DC and is valid BEFORE the control has ever been sized.
declare sub      PsCalendar_GetIdealSize( byval hCtrl as HWND, byref nWidth as long, byref nHeight as long )
declare function PsCalendar_GetEnabled( byval hCtrl as HWND ) as boolean
' Goes through EnableWindow, so the disable is enforced by the system rather than being a
' cosmetic flag; WM_ENABLE syncs isEnabled back (PsToggle's rule).
declare sub      PsCalendar_SetEnabled( byval hCtrl as HWND, byval isEnabled as boolean )
declare function PsCalendar_HasFocus( byval hCtrl as HWND ) as boolean
declare sub      PsCalendar_Refresh( byval hCtrl as HWND )

' --- geometry accessors (all return FALSE for an out-of-range index) ---
declare function PsCalendar_HitTest( byval hCtrl as HWND, byval pt as POINT ) as CAL_HITRESULT
' The client less the border and the padding -- everything the control actually draws in.
declare function PsCalendar_GetInnerRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsCalendar_GetNavBandRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsCalendar_GetPrevRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsCalendar_GetNextRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsCalendar_GetTitleRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsCalendar_GetGridRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsCalendar_GetPanelRect( byval hCtrl as HWND, byval panel as long, byref rc as RECT ) as boolean
declare function PsCalendar_GetCaptionRect( byval hCtrl as HWND, byval panel as long, byref rc as RECT ) as boolean
declare function PsCalendar_GetWeekdayRect( byval hCtrl as HWND, byval panel as long, byval i as long, byref rc as RECT ) as boolean
declare function PsCalendar_GetDayCellRect( byval hCtrl as HWND, byval panel as long, byval k as long, byref rc as RECT ) as boolean
declare function PsCalendar_GetMyCellRect( byval hCtrl as HWND, byval k as long, byref rc as RECT ) as boolean
declare function PsCalendar_GetCellDate( byval hCtrl as HWND, byval panel as long, byval k as long, byref st as SYSTEMTIME ) as boolean
declare function PsCalendar_GetPanelMonth( byval hCtrl as HWND, byval panel as long, byref nYear as long, byref nMonth as long ) as boolean
' Which cell carries the focus ring. When nothing is selected this falls back to today if it
' is displayed, else day 1 of panel 0 -- a PURE function of the laid-out state, so the
' fallback rule is assertable without reading a pixel.
declare function PsCalendar_GetFocusCell( byval hCtrl as HWND, byref panel as long, byref index as long ) as boolean
declare function PsCalendar_IsPrevDisabled( byval hCtrl as HWND ) as boolean
declare function PsCalendar_IsNextDisabled( byval hCtrl as HWND ) as boolean

' --- colours ---
declare sub      PsCalendar_GetColors( byval hCtrl as HWND, byval pColors as PSCALENDAR_COLORS ptr )
declare sub      PsCalendar_SetColors( byval hCtrl as HWND, byval pColors as PSCALENDAR_COLORS ptr )

' --- callbacks ---
declare sub      PsCalendar_SetSelChangeCallback( byval hCtrl as HWND, byval usersub as CAL_SelChangeCallbackSub )
declare sub      PsCalendar_SetDateActivatedCallback( byval hCtrl as HWND, byval usersub as CAL_DateActivatedCallbackSub )
declare sub      PsCalendar_SetPaintCallback( byval hCtrl as HWND, byval usersub as CAL_PaintCallbackSub )
declare sub      PsCalendar_SetDayPaintCallback( byval hCtrl as HWND, byval usersub as CAL_DayPaintCallbackSub )
declare sub      PsCalendar_SetMonthYearPaintCallback( byval hCtrl as HWND, byval usersub as CAL_MonthYearPaintCallbackSub )
declare sub      PsCalendar_SetMessageCallback( byval hCtrl as HWND, byval userfunc as CAL_MessageCallbackFunc )
' WHICH TOOLTIP DRAWS. PSTIP_MODE_SYSTEM (the default) is the comctl32 tracking tip this
' control has always used; PSTIP_MODE_PS is PsTooltip. The text is unaffected -- both are
' fed by the same TooltipCallback, which is asked per day cell exactly as before.
'
' A tracked tip is SHOWN BY THE CONTROL, so the hover delays below do not apply to it on
' either backend. They are declared for consistency with the family and take effect only if
' a future non-tracked tip is added here.
declare function PsCalendar_SetTooltipMode( byval hCalendar as HWND, byval nMode as long ) as boolean
declare function PsCalendar_GetTooltipMode( byval hCalendar as HWND ) as long
declare function PsCalendar_GetTooltipHandle( byval hCalendar as HWND ) as HWND
declare function PsCalendar_GetPsTooltipHandle( byval hCalendar as HWND ) as HWND
declare sub      PsCalendar_SetTooltipCallback( byval hCtrl as HWND, byval userfunc as CAL_TooltipCallbackFunc )

' NOTE: there is deliberately NO PsCalendar_FilterMessage. This control wraps no child and
' owns no popup, so it adds NO host message-pump obligation (PsButton, PsCheckBox and
' PsProgressBar are the only siblings that can also say this). A host that wants Tab
' navigation still needs IsDialogMessage in its own pump, as for any control.

' --- actions (these FIRE the callbacks -- they are actions, not setters) ---
' Select stDate as if the user had clicked it: fires SelChange. Ignored when out of range.
declare sub      PsCalendar_SelectDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
' Fire DateActivated for the current selection, if any. The door a host accelerator uses.
declare sub      PsCalendar_Activate( byval hCtrl as HWND )

' --- pure date-math helpers (public so a host and the self-test can reuse them) ---
declare function PsCalendar_IsLeapYear( byval y as long ) as boolean
declare function PsCalendar_DaysInMonth( byval y as long, byval m as long ) as long
declare function PsCalendar_DateToSerial( byval y as long, byval m as long, byval d as long ) as longint
declare sub      PsCalendar_SerialToDate( byval serial as longint, byref y as long, byref m as long, byref d as long )
declare function PsCalendar_DowSunday( byval y as long, byval m as long, byval d as long ) as long   ' 0=Sun..6=Sat
declare function PsCalendar_LeadingCells( byval y as long, byval m as long, byval nWeekStart as long ) as long
declare function PsCalendar_MakeDate( byval y as long, byval m as long, byval d as long ) as SYSTEMTIME
declare function PsCalendar_AddMonths( byref st as SYSTEMTIME, byval delta as long ) as SYSTEMTIME
declare function PsCalendar_AddDays( byref st as SYSTEMTIME, byval delta as long ) as SYSTEMTIME
declare function PsCalendar_DecadeBase( byval y as long ) as long

' --- accessor (public so the self-test can read laid-out geometry directly) ---
declare function GetCalendarPointer( byval hCtrl as HWND ) as PSCALENDAR ptr

' --- offscreen render probes. Public API, not test scaffolding: a host that supplies its own
'     PaintCallback should be able to assert it is not flooding the control. All three drive
'     the REAL renderer into an offscreen buffer -- the same code path WM_PAINT uses.
declare function PsCalendar_TestPartTones( byval hCtrl as HWND, byval nPart as long ) as long
declare function PsCalendar_TestPartBrightness( byval hCtrl as HWND, byval nPart as long ) as long
declare function PsCalendar_TestHashPart( byval hCtrl as HWND, byval nPart as long ) as ulongint
