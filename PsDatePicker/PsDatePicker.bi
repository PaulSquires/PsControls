'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsDatePicker.bi  --  owner-drawn date picker: an editable date field + a popup calendar
''

#pragma once

#include once "PsBufferPaint.bi"
' The tooltip backend switch. This control's tip is a TRACKED one, on its POPUP calendar
' rather than on the control itself -- see PSDATEPICKER_CAL.tip below.
#include once "PsTipHost.bi"
#include once "PsTextBox.bi"


' ----------------------------------------------------------------------------------------
' Timer ids (per-window, so every instance can share them).
'   HOTTRACK  - the container's icon hover safety net (WM_MOUSELEAVE is unreliable, and the
'               PsTextBox child covers most of our client so a leave onto it can be missed).
'   CALPOLL   - the calendar popup's foreground watchdog: it is never activated (see below),
'               so it cannot see focus loss and polls GetForegroundWindow to dismiss on
'               alt-tab, exactly as PsPopupMenu does.
' ----------------------------------------------------------------------------------------
#define IDT_CDATEPICKER_HOTTRACK   &hCB90
#define IDT_CDATEPICKER_CALPOLL    &hCB91
#define PSDATEPICKER_HOTTRACK_MS    100
#define PSDATEPICKER_CALPOLL_MS     100

' Custom messages the calendar popup sends UP to its owning container. The calendar is a
' separate top-level window; these are the one-way door back.
'   DATEPICKED  - the user picked a day. lParam = SYSTEMTIME ptr (valid for the synchronous
'                 SendMessage only); the container copies it.
'   CALCLOSED   - the calendar dismissed itself (Esc, outside click, foreground lost).
'                 wParam <> 0 if it closed because of a pick (so the container does not
'                 double-fire DropDownCallback).
#define DTP_CM_DATEPICKED   (WM_USER + 720)
#define DTP_CM_CALCLOSED    (WM_USER + 721)


' ----------------------------------------------------------------------------------------
' Defaults. SIZE values are DPI-scaled at Create; every setter afterwards takes raw pixels
' and the caller scales (the family rule). The BORDER and DIVIDER thicknesses are RULES and
' are never scaled -- a hairline stays a hairline (PsMenuBar's nSeparatorThickness rule).
' ----------------------------------------------------------------------------------------
#define CDTP_DEFAULT_ICONWIDTH      28     ' the calendar-icon cell, right of the field
#define CDTP_DEFAULT_FIELDPAD        6     ' left and right, inside the value cell
#define CDTP_DEFAULT_VERTPAD         5     ' above and below the text, for GetIdealSize
#define CDTP_DEFAULT_CORNERRADIUS    6
#define CDTP_DEFAULT_BORDERTHICK     1     ' not DPI-scaled
#define CDTP_DEFAULT_DIVIDERTHICK    1     ' not DPI-scaled
#define CDTP_DEFAULT_ICONGLYPH       &hE787   ' Segoe Fluent Icons "Calendar"
#define CDTP_DEFAULT_ICONFONTPT      11    ' point size of the control-created default icon font

' Calendar popup geometry (logical, DPI-scaled once at the calendar's Create).
#define CDTP_CAL_PAD                 8     ' outer padding, all four sides
#define CDTP_CAL_GUTTER            12     ' between the two month panels
#define CDTP_CAL_HEADERH           34
#define CDTP_CAL_CHEVW             28
#define CDTP_CAL_WEEKDAYH          22
#define CDTP_CAL_FOOTERH           30     ' the optional "Today" footer band
#define CDTP_CAL_CELLMINW          34
#define CDTP_CAL_CELLMINH          30

' The month/year drill-down grid: 3 columns x 4 rows keeps long month names uncrowded, and
' one formula serves both grids.
#define CDTP_MY_COLS                3
#define CDTP_MY_ROWS                4


' Week start.
enum
    DTP_SUNDAY = 0
    DTP_MONDAY = 1
end enum

' Calendar view mode (Win32 MonthCal-style drill-down).
enum
    DTP_VIEW_DAYS = 0
    DTP_VIEW_MONTHS
    DTP_VIEW_YEARS
end enum

' Hit-test parts inside the calendar.
enum
    DTP_HIT_NONE = 0
    DTP_HIT_PREV
    DTP_HIT_NEXT
    DTP_HIT_TITLE          ' the interactive (panel-0) title -- zoom out
    DTP_HIT_DAYCELL        ' index 0..41, panel 0/1
    DTP_HIT_MONTHCELL      ' index 0..11
    DTP_HIT_YEARCELL       ' index 0..11
    DTP_HIT_TODAY          ' the optional "Today" footer
end enum


' A resolved calendar hit.
type DTP_HITRESULT
    part   as long = DTP_HIT_NONE
    panel  as long = -1        ' 0/1 for day cells; 0 for month/year
    index  as long = -1        ' 0..41 (days) / 0..11 (months, years)
end type


' ----------------------------------------------------------------------------------------
' Colors for the built-in painter. Copied on Set. Flat COLORREF fields with defaults --
' read-modify-write is PsDatePicker_GetColors, assign, PsDatePicker_SetColors.
'
' Two default tricks the family uses:
'   IconColor = FieldBackColor, so the icon cell reads flat until hovered.
'   the calendar's hot/selected day colors carry the screenshot's blue-on-white intent.
'
' The FIELD's two colors are pushed into the embedded PsTextBox (the RichEdit draws the
' text); they live here so a host sets every color in one place.
' ----------------------------------------------------------------------------------------
type PSDATEPICKER_COLORS
    ' --- frame / field ---
    BackColor              as COLORREF = BGR( 33, 37, 43)   ' behind the rounded corners
    BorderColor            as COLORREF = BGR( 60, 66, 77)
    FocusBorderColor       as COLORREF = BGR( 86,156,214)   ' while the field has focus
    BorderColorDisabled    as COLORREF = BGR( 48, 52, 60)
    DividerColor           as COLORREF = BGR( 60, 66, 77)
    DividerColorDisabled   as COLORREF = BGR( 48, 52, 60)
    FieldBackColor         as COLORREF = BGR( 44, 49, 58)
    FieldForeColor         as COLORREF = BGR(215,218,224)
    FieldBackColorDisabled as COLORREF = BGR( 38, 42, 49)
    FieldForeColorDisabled as COLORREF = BGR( 90, 96,106)
    ' --- the calendar icon cell ---
    IconColor              as COLORREF = BGR( 44, 49, 58)   ' = FieldBackColor (flat at rest)
    IconGlyphColor         as COLORREF = BGR(190,196,206)
    IconGlyphColorHot      as COLORREF = BGR(255,255,255)
    IconBackColorHot       as COLORREF = BGR( 56, 62, 73)
    IconGlyphColorDisabled as COLORREF = BGR( 90, 96,106)
    ' --- calendar chrome ---
    CalBackColor           as COLORREF = BGR( 33, 37, 43)
    CalBorderColor         as COLORREF = BGR( 70, 76, 88)
    TitleColor             as COLORREF = BGR(225,228,234)
    TitleColorHot          as COLORREF = BGR(255,255,255)
    NavGlyphColor          as COLORREF = BGR(190,196,206)
    NavGlyphColorHot       as COLORREF = BGR(255,255,255)
    NavGlyphColorDisabled  as COLORREF = BGR( 70, 74, 82)
    WeekdayColor           as COLORREF = BGR(140,146,156)
    ' --- calendar day cells ---
    DayColor               as COLORREF = BGR(210,214,220)   ' ordinary weekday, current month
    DayColorWeekend        as COLORREF = BGR(150,175,205)   ' Sat/Sun, current month
    ' THESE TWO MUST STAY CLEARLY APART, and the gap is not cosmetic. An ADJACENT day is
    ' dimmed but still CLICKABLE (clicking it selects that date and navigates); a DISABLED day
    ' is outside the range and inert. They originally sat 34 luminance levels apart, which read
    ' as the same grey -- a range like "the next 60 days" then showed most of the month greyed
    ' with no way to tell what could be clicked. Disabled now sits close to the background;
    ' adjacent stays comfortably readable. Asserted in the self-test, at the PIXEL level.
    DayColorAdjacent       as COLORREF = BGR(124,131,142)   ' leading/trailing, still clickable
    DayColorDisabled       as COLORREF = BGR( 56, 60, 68)   ' out of range, inert
    DayBackColorHot        as COLORREF = BGR( 52, 58, 68)
    DayForeColorHot        as COLORREF = BGR(255,255,255)
    DayBackColorSelected   as COLORREF = BGR(  0,120,215)   ' the screenshot's blue
    DayForeColorSelected   as COLORREF = BGR(255,255,255)
    TodayRingColor         as COLORREF = BGR( 86,156,214)   ' subtle outline on today's cell
    ' --- the optional "Today" footer ---
    TodayFooterColor       as COLORREF = BGR(170,176,186)
    TodayFooterColorHot    as COLORREF = BGR(255,255,255)
    TodayFooterColorDisabled as COLORREF = BGR( 70, 74, 82)  ' today is outside the range
end type


' ----------------------------------------------------------------------------------------
' Paint info for the EDITING BOX (frame + icon), handed to an optional PaintCallback that
' replaces the built-in box painter. It never draws the text (the RichEdit does that).
' ----------------------------------------------------------------------------------------
type PSDATEPICKER_PAINTINFO
    hCtrl       as HWND
    b           as PsBufferPaint ptr
    rcClient    as RECT
    rcFrame     as RECT
    rcField     as RECT        ' the PsTextBox child sits exactly here
    rcDiv       as RECT
    rcIcon      as RECT
    isEnabled   as boolean
    isFocused   as boolean
    isOpen      as boolean     ' the calendar is dropped down
    iconHot     as boolean
    iconPressed as boolean
end type


' ----------------------------------------------------------------------------------------
' Paint info for ONE calendar day cell, handed to an optional DayPaintCallback. THIS is the
' granular hook: the flags are exactly what a host needs to render a day the way it wants.
' The container fills rcCell's background (CalBackColor) before calling.
' ----------------------------------------------------------------------------------------
type PSDATEPICKER_DAYPAINTINFO
    hCtrl       as HWND
    b           as PsBufferPaint ptr
    rcCell      as RECT
    stDate      as SYSTEMTIME  ' the calendar date this cell represents
    dayNumber   as long        ' 1..31 (the number to draw)
    monthIndex  as long        ' 0 = left/only panel, 1 = right panel (two-month mode)
    isAdjacent  as boolean     ' a leading/trailing day from the neighbouring month (dim it)
    isWeekend   as boolean     ' Saturday or Sunday (from the cell's own date)
    isToday     as boolean
    isSelected  as boolean
    isHot       as boolean     ' the mouse is over this cell
    isDisabled  as boolean     ' outside the min/max range (unclickable)
    isEnabled   as boolean     ' the control as a whole
end type


type PSDATEPICKER_MESSAGEINFO
    hCtrl       as HWND
    uMsg        as UINT
    wParam      as WPARAM
    lParam      as LPARAM
end type

type PSDATEPICKER_TOOLTIPINFO
    hCtrl       as HWND
    stDate      as SYSTEMTIME  ' the day under the cursor
    isValid     as boolean     ' false when the cursor is not over a day cell
    outText     as DWSTRING     ' fill this with the tip text (empty = no tip)
end type


' ----------------------------------------------------------------------------------------
' Callback typedefs (DTP_ prefix). See the public-API banner in the .inc for contracts.
' ----------------------------------------------------------------------------------------
' Resolve what the user typed. Return TRUE and fill *pDate for a valid date; FALSE for text you
' cannot parse. Empty text is handled by the control (see SetAllowEmpty).
'
' RANGE IS NOT YOUR JOB, and returning FALSE for an out-of-range date is the wrong shape: a date
' you accept is CLAMPED into the SetRange window afterwards (DTP_ClampToRange), never refused.
' So parse what is syntactically a date and let the control decide whether it is reachable.
'
' sText is BYVAL, and that is not a style choice. A `byref ... as const DWSTRING` parameter
' whose value is then COPIED (`dim as DWSTRING c = sText`, the obvious first line of any
' parser) corrupts the process heap -- measured, 0xC0000374 STATUS_HEAP_CORRUPTION, in a
' 30-line program with no windows in it. AfxNova's DWSTRING copy constructor is not
' const-correct, so the const-qualified reference binds to the wrong thing and the copy
' scribbles. byval is the family idiom for DWSTRING parameters (PsTextBox_SetText,
' PsPopupMenu_AddItem, PsBufferPaint.PaintText all take byval) and it is safe.
' DO NOT "optimise" this back to a const reference.
type DTP_ParseCallbackFunc as function( byval hCtrl as HWND, byval sText as DWSTRING, byval pDate as SYSTEMTIME ptr ) as boolean

' Render a date the calendar or a setter produced, into display text for the field.
type DTP_FormatCallbackSub as sub( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr, byref sOut as DWSTRING )

' The date changed through USER interaction (typed commit or calendar pick). Programmatic
' setters are SILENT. bHasDate is false when the field was cleared to the no-date state.
type DTP_DateChangedCallbackSub as sub( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr, byval bHasDate as boolean )

' The calendar opened or closed. Fires for programmatic DropDown/CloseUp too; the OPENING
' edge runs before the calendar content is built (the just-in-time hook for a dynamic range).
type DTP_DropDownCallbackSub as sub( byval hCtrl as HWND, byval isOpen as boolean )

' Draw the editing box (frame + icon) INSTEAD OF the built-in painter. Never draws the text.
type DTP_PaintCallbackSub as sub( byval p as PSDATEPICKER_PAINTINFO ptr )

' Draw ONE calendar day cell. If unset the built-in painter draws it.
type DTP_DayPaintCallbackSub as sub( byval p as PSDATEPICKER_DAYPAINTINFO ptr )

' Observe messages. Return TRUE to suppress default handling -- but the result is IGNORED for
' WM_KILLFOCUS (on the field), and for WM_LBUTTONUP, WM_MOUSELEAVE and WM_ENABLE (on the
' control). Each of those has already changed state by the time you are offered it, so
' suppressing it could only desynchronise the control from what the system has done.
type DTP_MessageCallbackFunc as function( byval m as PSDATEPICKER_MESSAGEINFO ptr ) as boolean

' Supply a tooltip for the day under the cursor. Return TRUE if you filled outText.
type DTP_TooltipCallbackFunc as function( byval t as PSDATEPICKER_TOOLTIPINFO ptr ) as boolean


' ========================================================================================
' The calendar popup's per-instance state. A separate top-level WS_POPUP window owned by the
' PsDatePicker control; it back-points to the container via hOwner and reads the container's
' colors / font / callbacks from there (single source of truth).
' ========================================================================================
type PSDATEPICKER_CAL
    hWin            as HWND
    hOwner          as HWND               ' the PsDatePicker control to notify
    idc             as long = 0

    ' --- view-mode state machine ---
    eView           as long = DTP_VIEW_DAYS

    ' --- model ---
    stAnchor        as SYSTEMTIME         ' displayed month (canonical day = 1)
    stSelected      as SYSTEMTIME
    hasSelection    as boolean = false
    stToday         as SYSTEMTIME
    stMin           as SYSTEMTIME
    stMax           as SYSTEMTIME
    hasMin          as boolean = false
    hasMax          as boolean = false

    ' --- options (copied from the owner at open) ---
    nWeekStart      as long = DTP_SUNDAY
    nMonthCount     as long = 1           ' 1 or 2 (configured, not necessarily drawn)

    ' --- hot cell (single-valued) ---
    hotPart         as long = DTP_HIT_NONE
    hotPanel        as long = -1
    hotIndex        as long = -1

    ' --- housekeeping ---
    pollTimerOn     as boolean = false
    bClosingByPick  as boolean = false
    nWheelAccum     as long = 0
    ' The tooltip, whichever backend it is on. It belongs to the CALENDAR POPUP, not to
    ' the picker -- the day cells live there -- so it is destroyed with the popup and
    ' rebuilt with the next one, exactly as the raw handle was.
    tip             as PSTIPHOST
    tipKey          as longint = -1       ' serial of the day the tip currently shows (-1 = hidden)

    ' --- layout inputs (scaled once at Create; raw pixels thereafter) ---
    nPad            as long = CDTP_CAL_PAD
    nGutter         as long = CDTP_CAL_GUTTER
    nHeaderH        as long = CDTP_CAL_HEADERH
    nChevW          as long = CDTP_CAL_CHEVW
    nWeekdayH       as long = CDTP_CAL_WEEKDAYH
    nCellMinW       as long = CDTP_CAL_CELLMINW
    nCellMinH       as long = CDTP_CAL_CELLMINH
    nFooterH        as long = CDTP_CAL_FOOTERH
    bShowTodayFooter as boolean = false

    ' --- layout outputs (derived; LayoutCalendar owns them) ---
    rcClient        as RECT
    rcPanel(0 to 1)             as RECT
    rcHeader(0 to 1)           as RECT
    rcTitle(0 to 1)            as RECT
    rcPrev          as RECT               ' panel 0 only
    rcNext          as RECT               ' panel 0 only
    rcBody(0 to 1)             as RECT     ' the day-grid region (below the weekday row)
    rcWeekday(0 to 1, 0 to 6)  as RECT
    rcDayCell(0 to 1, 0 to 41) as RECT     ' 6x7 = 42, FIXED
    rcTodayFooter   as RECT               ' the "Today" band (empty when the option is off)
    rcMyGrid        as RECT               ' the month/year grid region (panel 0)
    rcMonthCell(0 to 11)       as RECT
    rcYearCell(0 to 11)        as RECT
    ' derived per-panel date origin (so cell->date needs no re-measure)
    panelYear(0 to 1)          as long
    panelMonth(0 to 1)         as long     ' 1..12
    panelLead(0 to 1)          as long     ' leading-cell count (column of the 1st)
    cellW(0 to 1)              as long
    rowH(0 to 1)              as long

    nIdealW         as long = 0
    nIdealH         as long = 0
    bLayoutDirty    as boolean = true

    declare sub      LayoutCalendar()
    declare function CellDate( byval panel as long, byval k as long ) as SYSTEMTIME
end type


type PSDATEPICKER
    hWin            as HWND
    hTextBox        as HWND               ' the embedded PsTextBox (borderless, plain text)
    hCal            as HWND               ' the calendar popup (lazy; 0 until first opened)
    ' Which tooltip backend the popup's day-cell tip should use. Held HERE rather than on
    ' PSDATEPICKER_CAL because the popup is destroyed and rebuilt on every open/close,
    ' and a mode stored there would revert to the default on the second open.
    nTipMode        as long = PSTIP_MODE_SYSTEM
    idc_Ctrl        as long = 0

    ' --- state ---
    isEnabled       as boolean = true
    isFocused       as boolean = false    ' the field has keyboard focus
    isOpen          as boolean = false    ' the calendar is dropped down
    ' One-shot guard: an icon click that DISMISSED the open calendar (via the pump filter)
    ' must not immediately reopen it. Armed by FilterMessage, consumed by the next
    ' WM_LBUTTONDOWN (PsComboBox's pattern).
    bSuppressNextOpen as boolean = false
    iconHot         as boolean = false
    iconPressed     as boolean = false
    hotTimerOn      as boolean = false

    ' --- value ---
    hasDate         as boolean = false
    stDate          as SYSTEMTIME
    stMin           as SYSTEMTIME
    stMax           as SYSTEMTIME
    hasMin          as boolean = false
    hasMax          as boolean = false
    bAllowEmpty     as boolean = true
    lastValidText   as DWSTRING            ' reverted to when a typed value fails to parse

    ' --- options ---
    nWeekStart      as long = DTP_SUNDAY
    nMonthCount     as long = 1
    bShowToday      as boolean = true
    bShowTodayFooter as boolean = false   ' the clickable "Today: ..." band at the calendar's foot

    ' --- appearance ---
    colors          as PSDATEPICKER_COLORS
    hMainFont       as HFONT = 0          ' CALLER-OWNED; never deleted here
    hIconFont       as HFONT = 0          ' CALLER-OWNED; 0 = use hIconFontDefault
    hIconFontDefault as HFONT = 0         ' CONTROL-OWNED; created at Create, deleted at destroy
    wszIconGlyph    as DWSTRING

    ' --- layout (DERIVED rects; LayoutControl owns them; lazy via bLayoutDirty) ---
    nIconWidth      as long = CDTP_DEFAULT_ICONWIDTH    ' DPI-scaled at Create
    nFieldPadLeft   as long = CDTP_DEFAULT_FIELDPAD     ' DPI-scaled at Create
    nFieldPadRight  as long = CDTP_DEFAULT_FIELDPAD     ' DPI-scaled at Create
    nVertPadding    as long = CDTP_DEFAULT_VERTPAD      ' DPI-scaled at Create
    nCornerRadius   as long = CDTP_DEFAULT_CORNERRADIUS ' DPI-scaled at Create
    nBorderThick    as long = CDTP_DEFAULT_BORDERTHICK  ' NOT DPI-scaled
    nDividerThick   as long = CDTP_DEFAULT_DIVIDERTHICK ' NOT DPI-scaled
    rcFrame         as RECT
    rcField         as RECT
    rcDiv           as RECT
    rcIcon          as RECT
    bLayoutDirty    as boolean = true

    ' --- callbacks ---
    ParseCallback        as DTP_ParseCallbackFunc
    FormatCallback       as DTP_FormatCallbackSub
    DateChangedCallback  as DTP_DateChangedCallbackSub
    DropDownCallback     as DTP_DropDownCallbackSub
    PaintCallback        as DTP_PaintCallbackSub
    DayPaintCallback     as DTP_DayPaintCallbackSub
    MessageCallback      as DTP_MessageCallbackFunc
    TooltipCallback      as DTP_TooltipCallbackFunc

    declare sub LayoutControl()
    declare sub Refresh()
end type


' ========================================================================================
' PUBLIC API   (full contracts in the .inc banner)
' ========================================================================================

' --- creation / children / focus ---
declare function PsDatePicker_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND
declare function PsDatePicker_GetTextBoxHandle( byval hCtrl as HWND ) as HWND
declare function PsDatePicker_GetCalendarHandle( byval hCtrl as HWND ) as HWND
declare function PsDatePicker_HasFocus( byval hCtrl as HWND ) as boolean

' --- value (all SILENT; only user interaction notifies) ---
declare function PsDatePicker_GetDate( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean
declare sub      PsDatePicker_SetDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
declare sub      PsDatePicker_ClearDate( byval hCtrl as HWND )
declare function PsDatePicker_HasDate( byval hCtrl as HWND ) as boolean
declare sub      PsDatePicker_GetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byref bHasMin as boolean, byref bHasMax as boolean )
declare sub      PsDatePicker_SetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byval bHasMin as boolean, byval bHasMax as boolean )
declare function PsDatePicker_GetAllowEmpty( byval hCtrl as HWND ) as boolean
declare sub      PsDatePicker_SetAllowEmpty( byval hCtrl as HWND, byval bAllow as boolean )
' Re-run Format on the current date and refresh the field (e.g. after the host changes its
' date-format preference). Silent.
declare sub      PsDatePicker_ReformatText( byval hCtrl as HWND )

' --- calendar behaviour ---
declare function PsDatePicker_GetWeekStart( byval hCtrl as HWND ) as long
declare sub      PsDatePicker_SetWeekStart( byval hCtrl as HWND, byval nWeekStart as long )
declare function PsDatePicker_GetMonthCount( byval hCtrl as HWND ) as long
declare sub      PsDatePicker_SetMonthCount( byval hCtrl as HWND, byval nCount as long )
' The optional "Today" footer: a clickable band at the foot of the calendar showing today's
' date. Clicking it SELECTS today and closes the popup -- identical to clicking today's cell
' in the grid, and it routes through the same commit path, so DateChangedCallback fires once.
' Reachable from every view, so it doubles as the way out of a deep drill-down. Greyed and
' inert when today falls outside the min/max range. OFF by default.
declare function PsDatePicker_GetShowTodayFooter( byval hCtrl as HWND ) as boolean
declare sub      PsDatePicker_SetShowTodayFooter( byval hCtrl as HWND, byval bShow as boolean )
declare function PsDatePicker_GetShowToday( byval hCtrl as HWND ) as boolean
declare sub      PsDatePicker_SetShowToday( byval hCtrl as HWND, byval bShow as boolean )
declare sub      PsDatePicker_DropDown( byval hCtrl as HWND )
declare sub      PsDatePicker_CloseUp( byval hCtrl as HWND )
declare function PsDatePicker_IsDroppedDown( byval hCtrl as HWND ) as boolean

' --- layout / appearance ---
declare function PsDatePicker_GetFont( byval hCtrl as HWND ) as HFONT
declare sub      PsDatePicker_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
declare function PsDatePicker_GetIconFont( byval hCtrl as HWND ) as HFONT
declare sub      PsDatePicker_SetIconFont( byval hCtrl as HWND, byval hFont as HFONT )
declare function PsDatePicker_GetIconGlyph( byval hCtrl as HWND ) as DWSTRING
declare sub      PsDatePicker_SetIconGlyph( byval hCtrl as HWND, byref sGlyph as DWSTRING )
declare function PsDatePicker_GetIconWidth( byval hCtrl as HWND ) as long
declare sub      PsDatePicker_SetIconWidth( byval hCtrl as HWND, byval nWidth as long )
declare function PsDatePicker_GetCornerRadius( byval hCtrl as HWND ) as long
declare sub      PsDatePicker_SetCornerRadius( byval hCtrl as HWND, byval nRadius as long )
declare function PsDatePicker_GetBorderThickness( byval hCtrl as HWND ) as long
declare sub      PsDatePicker_SetBorderThickness( byval hCtrl as HWND, byval nThickness as long )
declare sub      PsDatePicker_GetIdealSize( byval hCtrl as HWND, byref nWidth as long, byref nHeight as long )
declare function PsDatePicker_GetFrameRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsDatePicker_GetFieldRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsDatePicker_GetIconRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
declare function PsDatePicker_HitTest( byval hCtrl as HWND, byval pt as POINT ) as long   ' returns 1 = icon, 0 = none
declare function PsDatePicker_GetEnabled( byval hCtrl as HWND ) as boolean
declare sub      PsDatePicker_SetEnabled( byval hCtrl as HWND, byval isEnabled as boolean )
declare sub      PsDatePicker_Refresh( byval hCtrl as HWND )

' --- colours ---
declare sub      PsDatePicker_GetColors( byval hCtrl as HWND, byval pColors as PSDATEPICKER_COLORS ptr )
declare sub      PsDatePicker_SetColors( byval hCtrl as HWND, byval pColors as PSDATEPICKER_COLORS ptr )

' --- callbacks ---
declare sub      PsDatePicker_SetParseCallback( byval hCtrl as HWND, byval userfunc as DTP_ParseCallbackFunc )
declare sub      PsDatePicker_SetFormatCallback( byval hCtrl as HWND, byval usersub as DTP_FormatCallbackSub )
declare sub      PsDatePicker_SetDateChangedCallback( byval hCtrl as HWND, byval usersub as DTP_DateChangedCallbackSub )
declare sub      PsDatePicker_SetDropDownCallback( byval hCtrl as HWND, byval usersub as DTP_DropDownCallbackSub )
declare sub      PsDatePicker_SetPaintCallback( byval hCtrl as HWND, byval usersub as DTP_PaintCallbackSub )
declare sub      PsDatePicker_SetDayPaintCallback( byval hCtrl as HWND, byval usersub as DTP_DayPaintCallbackSub )
declare sub      PsDatePicker_SetMessageCallback( byval hCtrl as HWND, byval userfunc as DTP_MessageCallbackFunc )
' WHICH TOOLTIP DRAWS on the popup calendar's day cells. PSTIP_MODE_SYSTEM (the default)
' is the comctl32 tracking tip this control has always used; PSTIP_MODE_PS is PsTooltip.
' The text is unaffected: both are fed by the same TooltipCallback, per day cell.
'
' THE MODE IS REMEMBERED ON THE PICKER, not on the popup, because the popup is destroyed
' and rebuilt every time the calendar closes and reopens -- a mode stored there would
' silently revert to the default the second time a user opened the calendar.
declare function PsDatePicker_SetTooltipMode( byval hDatePicker as HWND, byval nMode as long ) as boolean
declare function PsDatePicker_GetTooltipMode( byval hDatePicker as HWND ) as long
' 0 unless the calendar is open AND a day cell has already asked for a tip.
declare function PsDatePicker_GetTooltipHandle( byval hDatePicker as HWND ) as HWND
declare function PsDatePicker_GetPsTooltipHandle( byval hDatePicker as HWND ) as HWND
declare sub      PsDatePicker_SetTooltipCallback( byval hCtrl as HWND, byval userfunc as DTP_TooltipCallbackFunc )

' --- host message pump (NOT optional -- see the banner) ---
declare function PsDatePicker_FilterMessage( byval pMsg as MSG ptr ) as boolean

' --- pure date-math helpers (public so a host and the self-test can reuse them) ---
declare function PsDatePicker_IsLeapYear( byval y as long ) as boolean
declare function PsDatePicker_DaysInMonth( byval y as long, byval m as long ) as long
declare function PsDatePicker_DateToSerial( byval y as long, byval m as long, byval d as long ) as longint
declare sub      PsDatePicker_SerialToDate( byval serial as longint, byref y as long, byref m as long, byref d as long )
declare function PsDatePicker_DowSunday( byval y as long, byval m as long, byval d as long ) as long   ' 0=Sun..6=Sat
declare function PsDatePicker_LeadingCells( byval y as long, byval m as long, byval nWeekStart as long ) as long
declare function PsDatePicker_MakeDate( byval y as long, byval m as long, byval d as long ) as SYSTEMTIME
declare function PsDatePicker_AddMonths( byref st as SYSTEMTIME, byval delta as long ) as SYSTEMTIME
declare function PsDatePicker_DecadeBase( byval y as long ) as long

' --- accessor (public so the self-test can read calendar geometry) ---
declare function GetDatePickerCalPointer( byval hCal as HWND ) as PSDATEPICKER_CAL ptr
' Render the real calendar painter offscreen and count distinct tones over the whole client.
declare function PsDatePicker_TestCalendarTones( byval hCal as HWND ) as long
' Distinct tones inside the Today footer band (0 when the footer is off).
declare function PsDatePicker_TestFooterTones( byval hCal as HWND ) as long
' Brightest rendered pixel in a part: 0 = prev chevron, 1 = next chevron, 100+k = day cell k.
declare function PsDatePicker_TestPartBrightness( byval hCal as HWND, byval nPart as long ) as long
' Build + seed + size the calendar WITHOUT showing it, for the self-test. Returns hCal.
declare function PsDatePicker_TestPrepareCalendar( byval hCtrl as HWND, byval eView as long, byval anchorYear as long, byval anchorMonth as long ) as HWND
