'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsCalendar.inc  --  owner-drawn embedded month calendar (non-popup)
''

#include once "PsCalendar.bi"
' The tooltip backend switch, pulled in here so a host needs no extra include line.
#include once "PsTipHost.inc"


dim shared gCAL_Trace as long = -1
private sub CAL_Log( byref s as string )
    if gCAL_Trace = -1 then
        if environ("PSCALENDAR_TRACE") = "1" then gCAL_Trace = 1 else gCAL_Trace = 0
    end if
    if gCAL_Trace = 1 then
        print "[CAL] " & s
    end if
end sub


' ========================================================================================
' Accessor
' ========================================================================================
function GetCalendarPointer( byval hCtrl as HWND ) as PSCALENDAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hCtrl)
    if pWindow = 0 then return 0
    return cast( PSCALENDAR ptr, pWindow->UserData(0) )
end function


' Forward declarations.
declare function PsCalendar_WndProc( byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
declare sub      CAL_Render( byval pCal as PSCALENDAR ptr, byval pb as PsBufferPaint ptr )
declare sub      CAL_Relayout( byval pCal as PSCALENDAR ptr )
declare function CAL_HitTestPt( byval pCal as PSCALENDAR ptr, byval pt as POINT ) as CAL_HITRESULT
declare function CAL_InRange( byval pCal as PSCALENDAR ptr, byref st as SYSTEMTIME ) as boolean
declare function CAL_PrevDisabled( byval pCal as PSCALENDAR ptr ) as boolean
declare function CAL_NextDisabled( byval pCal as PSCALENDAR ptr ) as boolean
declare function CAL_FocusCell( byval pCal as PSCALENDAR ptr, byref panel as long, byref index as long ) as boolean
declare sub      CAL_TipHide( byval pCal as PSCALENDAR ptr )
declare function CAL_EffStep( byval pCal as PSCALENDAR ptr ) as long
declare function CAL_NavTarget( byval pCal as PSCALENDAR ptr, byval nDir as long ) as SYSTEMTIME


' ========================================================================================
' PURE DATE MATH  (proleptic Gregorian; serial = days since 1970-01-01, Hinnant's algorithm)
'
' Ported verbatim from PsDatePicker under distinct names. The duplication is deliberate and
' priced in: the two controls must be includable in the SAME module, which rules out sharing
' the symbols, and a fourth copy of thirty lines of arithmetic is cheaper than a third
' vendored file. Every one is a pure function of its inputs.
'
' Cell -> date goes through the serial rather than through month/day special cases, which is
' why every month and year rollover, both week starts, and leap years all fall out for free.
' ========================================================================================
function PsCalendar_IsLeapYear( byval y as long ) as boolean
    dim as boolean b1 = ((y mod 4) = 0) andalso ((y mod 100) <> 0)
    dim as boolean b2 = ((y mod 400) = 0)
    return (b1 orelse b2)
end function

function PsCalendar_DaysInMonth( byval y as long, byval m as long ) as long
    select case m
    case 1, 3, 5, 7, 8, 10, 12 : return 31
    case 4, 6, 9, 11           : return 30
    case 2
        if PsCalendar_IsLeapYear(y) then return 29 else return 28
    case else                  : return 30
    end select
end function

function PsCalendar_DateToSerial( byval y as long, byval m as long, byval d as long ) as longint
    dim as longint yy = y
    if m <= 2 then yy -= 1
    dim as longint era
    if yy >= 0 then era = yy \ 400 else era = (yy - 399) \ 400
    dim as longint yoe = yy - era * 400                       ' [0, 399]
    dim as longint mp
    if m > 2 then mp = m - 3 else mp = m + 9                  ' [0, 11]
    dim as longint doy = (153 * mp + 2) \ 5 + d - 1           ' [0, 365]
    dim as longint doe = yoe * 365 + yoe \ 4 - yoe \ 100 + doy
    return era * 146097 + doe - 719468
end function

sub PsCalendar_SerialToDate( byval serial as longint, byref y as long, byref m as long, byref d as long )
    dim as longint z = serial + 719468
    dim as longint era
    if z >= 0 then era = z \ 146097 else era = (z - 146096) \ 146097
    dim as longint doe = z - era * 146097                     ' [0, 146096]
    dim as longint yoe = (doe - doe \ 1460 + doe \ 36524 - doe \ 146096) \ 365   ' [0, 399]
    dim as longint yy = yoe + era * 400
    dim as longint doy = doe - (365 * yoe + yoe \ 4 - yoe \ 100)    ' [0, 365]
    dim as longint mp = (5 * doy + 2) \ 153                   ' [0, 11]
    dim as longint dd = doy - (153 * mp + 2) \ 5 + 1          ' [1, 31]
    dim as longint mm
    if mp < 10 then mm = mp + 3 else mm = mp - 9              ' [1, 12]
    if mm <= 2 then yy += 1
    y = cast(long, yy) : m = cast(long, mm) : d = cast(long, dd)
end sub

' 0 = Sunday .. 6 = Saturday. 1970-01-01 (serial 0) is a Thursday.
function PsCalendar_DowSunday( byval y as long, byval m as long, byval d as long ) as long
    dim as longint z = PsCalendar_DateToSerial(y, m, d)
    dim as long w = cast(long, ((z mod 7) + 7) mod 7)        ' 0 = Thursday here
    return (w + 4) mod 7                                      ' shift Thursday(0) -> 4, Sunday -> 0
end function

' Column of the 1st of the month, given the week start.
function PsCalendar_LeadingCells( byval y as long, byval m as long, byval nWeekStart as long ) as long
    dim as long dow1 = PsCalendar_DowSunday(y, m, 1)
    if nWeekStart = CAL_MONDAY then return (dow1 + 6) mod 7
    return dow1
end function

function PsCalendar_MakeDate( byval y as long, byval m as long, byval d as long ) as SYSTEMTIME
    dim as SYSTEMTIME st
    st.wYear      = y
    st.wMonth     = m
    st.wDay       = d
    st.wDayOfWeek = PsCalendar_DowSunday(y, m, d)
    return st
end function

' The day is CLAMPED to the target month's length: 2021-01-31 + 1 month = 2021-02-28.
function PsCalendar_AddMonths( byref st as SYSTEMTIME, byval delta as long ) as SYSTEMTIME
    dim as long total = st.wYear * 12 + (st.wMonth - 1) + delta
    dim as long y = total \ 12
    dim as long m = total mod 12
    if m < 0 then
        m += 12
        y -= 1
    end if
    m += 1
    dim as long d = st.wDay
    dim as long dmax = PsCalendar_DaysInMonth(y, m)
    if d > dmax then d = dmax
    return PsCalendar_MakeDate(y, m, d)
end function

' Straight serial arithmetic, so every rollover is free. This is what the arrow keys use.
function PsCalendar_AddDays( byref st as SYSTEMTIME, byval delta as long ) as SYSTEMTIME
    dim as longint s = PsCalendar_DateToSerial( st.wYear, st.wMonth, st.wDay ) + delta
    dim as long yy, mm, dd
    PsCalendar_SerialToDate( s, yy, mm, dd )
    return PsCalendar_MakeDate( yy, mm, dd )
end function

function PsCalendar_DecadeBase( byval y as long ) as long
    return (y \ 10) * 10
end function

' Serial of a SYSTEMTIME's date portion.
private function CAL_Serial( byref st as SYSTEMTIME ) as longint
    return PsCalendar_DateToSerial( st.wYear, st.wMonth, st.wDay )
end function

private function CAL_SameYMD( byref a as SYSTEMTIME, byref b as SYSTEMTIME ) as boolean
    dim as boolean b1 = (a.wYear = b.wYear) andalso (a.wMonth = b.wMonth)
    return (b1 andalso (a.wDay = b.wDay))
end function


' ========================================================================================
' TYPE METHODS
' ========================================================================================
function PSCALENDAR.PanelCount() as long
    dim as long c = this.nCols
    dim as long r = this.nRows
    if c < 1 then c = 1
    if r < 1 then r = 1
    dim as long n = c * r
    if n > CCAL_MAX_PANELS then n = CCAL_MAX_PANELS
    return n
end function

function PSCALENDAR.CellDate( byval panel as long, byval k as long ) as SYSTEMTIME
    if panel < 0 then panel = 0
    if panel > CCAL_MAX_PANELS - 1 then panel = CCAL_MAX_PANELS - 1
    dim as longint s0 = PsCalendar_DateToSerial( this.panelYear(panel), this.panelMonth(panel), 1 ) - this.panelLead(panel)
    dim as long yy, mm, dd
    PsCalendar_SerialToDate( s0 + k, yy, mm, dd )
    return PsCalendar_MakeDate( yy, mm, dd )
end function


' ----------------------------------------------------------------------------------------
' The one geometry pass. All rects DERIVED, no DC taken, nothing measured -- every metric is
' a declared layout input, so this routine (alone among the family's layout passes that draw
' text) never needs a font.
'
' The ideal size is computed FIRST and unconditionally, so PsCalendar_GetIdealSize answers
' correctly even before the control has ever been sized -- which is exactly when a host that
' wants to size-to-fit needs to ask.
'
' Structure:
'     [border] [pad] nav band [navgap] panel grid [pad] [border]
'
' In DAYS view the grid is nCols x nRows month panels, ROW-MAJOR (panel p sits at
' col = p mod nCols, row = p \ nCols), each panel being caption / weekday row / 6x7 cells.
' In MONTHS and YEARS view the panel grid DOES NOT EXIST: the 3x4 drill-down grid takes the
' whole of rcGrid. That is deliberate -- PsDatePicker drilled down inside panel 0 and left
' the second month's half of the popup blank, which read as a bug at two panels and would
' read as a worse one at twelve.
'
' Cells STRETCH: the panel width is floor-divided by 7 and the body height by 6, and the
' leftover pixels become a right / bottom margin. nCellMinW / nCellMinH feed GetIdealSize
' only; below ideal the cells shrink rather than clip.
' ----------------------------------------------------------------------------------------
sub PSCALENDAR.LayoutCalendar()
    this.bLayoutDirty = false

    dim as long nc = this.nCols
    dim as long nr = this.nRows
    if nc < 1 then nc = 1
    if nr < 1 then nr = 1
    if nc * nr > CCAL_MAX_PANELS then
        nc = 1 : nr = 1
    end if
    dim as long np = nc * nr

    ' THE PER-PANEL CAPTION IS SUPPRESSED AT ONE PANEL. The nav band's title already reads
    ' "January 2026" there, so a caption below it would just repeat it; above one panel the
    ' title becomes a range ("Jan - Dec 2026") and each panel needs to name itself. Derived
    ' from the panel count rather than exposed as a flag -- asserted both ways.
    dim as long capH = 0
    if np > 1 then capH = this.nCaptionH

    ' ---- ideal size: pure scalar, independent of the client rect ----
    dim as long panelIdealW = 7 * this.nCellMinW
    dim as long panelIdealH = capH + this.nWeekdayH + 6 * this.nCellMinH
    dim as long edge = this.nPad + this.nBorderThick
    this.nIdealW = 2 * edge + nc * panelIdealW + (nc - 1) * this.nGutterX
    this.nIdealH = 2 * edge + this.nNavBandH + this.nNavGap + _
                   nr * panelIdealH + (nr - 1) * this.nGutterY

    ' ---- per-panel date origins (independent of the client rect, so cell->date and the
    '      title text are both valid even when the control has no size yet) ----
    dim as SYSTEMTIME st0 = PsCalendar_MakeDate( this.stAnchor.wYear, this.stAnchor.wMonth, 1 )
    for p as long = 0 to CCAL_MAX_PANELS - 1
        dim as SYSTEMTIME stP = PsCalendar_AddMonths( st0, p )
        this.panelYear(p)  = stP.wYear
        this.panelMonth(p) = stP.wMonth
        this.panelLead(p)  = PsCalendar_LeadingCells( stP.wYear, stP.wMonth, this.nWeekStart )
    next

    if this.hWin = 0 then
        this.bLayoutDirty = true
        exit sub
    end if

    GetClientRect( this.hWin, @this.rcClient )
    dim as long W = this.rcClient.right
    dim as long H = this.rcClient.bottom
    if (W <= 0) orelse (H <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    SetRect( @this.rcInner, edge, edge, W - edge, H - edge )

    ' ---- the shared nav band: prev | title | next, full inner width ----
    SetRect( @this.rcNavBand, this.rcInner.left, this.rcInner.top, _
             this.rcInner.right, this.rcInner.top + this.nNavBandH )
    SetRect( @this.rcPrev, this.rcNavBand.left, this.rcNavBand.top, _
             this.rcNavBand.left + this.nChevW, this.rcNavBand.bottom )
    SetRect( @this.rcNext, this.rcNavBand.right - this.nChevW, this.rcNavBand.top, _
             this.rcNavBand.right, this.rcNavBand.bottom )
    SetRect( @this.rcTitle, this.rcNavBand.left + this.nChevW, this.rcNavBand.top, _
             this.rcNavBand.right - this.nChevW, this.rcNavBand.bottom )

    SetRect( @this.rcGrid, this.rcInner.left, this.rcNavBand.bottom + this.nNavGap, _
             this.rcInner.right, this.rcInner.bottom )

    ' ---- clear the panel arrays: at fewer than CCAL_MAX_PANELS panels the tail must not
    '      carry stale rects from a previous, larger grid, or a hit test would resolve into
    '      a panel that is no longer drawn.
    for p as long = 0 to CCAL_MAX_PANELS - 1
        SetRect( @this.rcPanel(p), 0, 0, 0, 0 )
        SetRect( @this.rcCaption(p), 0, 0, 0, 0 )
        SetRect( @this.rcBody(p), 0, 0, 0, 0 )
        this.cellW(p) = 0
        this.rowH(p)  = 0
        for i as long = 0 to 6
            SetRect( @this.rcWeekday(p, i), 0, 0, 0, 0 )
        next
        for k as long = 0 to 41
            SetRect( @this.rcDayCell(p, k), 0, 0, 0, 0 )
        next
    next
    SetRect( @this.rcMyGrid, 0, 0, 0, 0 )
    for k as long = 0 to 11
        SetRect( @this.rcMyCell(k), 0, 0, 0, 0 )
    next

    if this.eView <> CAL_VIEW_DAYS then
        ' ---- MONTHS / YEARS: a 3x4 grid over the WHOLE grid region ----
        this.rcMyGrid = this.rcGrid
        dim as long mw = (this.rcMyGrid.right - this.rcMyGrid.left) \ CCAL_MY_COLS
        dim as long mh = (this.rcMyGrid.bottom - this.rcMyGrid.top) \ CCAL_MY_ROWS
        for k as long = 0 to 11
            dim as long col = k mod CCAL_MY_COLS
            dim as long row = k \ CCAL_MY_COLS
            SetRect( @this.rcMyCell(k), _
                     this.rcMyGrid.left + col * mw, this.rcMyGrid.top + row * mh, _
                     this.rcMyGrid.left + col * mw + mw, this.rcMyGrid.top + row * mh + mh )
        next
        exit sub
    end if

    ' ---- DAYS: the panel grid ----
    dim as long gridW = this.rcGrid.right - this.rcGrid.left
    dim as long gridH = this.rcGrid.bottom - this.rcGrid.top
    dim as long panelW = (gridW - (nc - 1) * this.nGutterX) \ nc
    dim as long panelH = (gridH - (nr - 1) * this.nGutterY) \ nr
    if panelW < 0 then panelW = 0
    if panelH < 0 then panelH = 0

    for p as long = 0 to np - 1
        dim as long col = p mod nc
        dim as long row = p \ nc
        dim as long L = this.rcGrid.left + col * (panelW + this.nGutterX)
        dim as long T = this.rcGrid.top  + row * (panelH + this.nGutterY)
        SetRect( @this.rcPanel(p), L, T, L + panelW, T + panelH )

        dim as long bodyTop = T
        if capH > 0 then
            SetRect( @this.rcCaption(p), L, T, L + panelW, T + capH )
            bodyTop = T + capH
        end if

        dim as RECT weekBand
        SetRect( @weekBand, L, bodyTop, L + panelW, bodyTop + this.nWeekdayH )
        SetRect( @this.rcBody(p), L, weekBand.bottom, L + panelW, this.rcPanel(p).bottom )

        dim as long cw = panelW \ 7                                              ' floor; leftover = right margin
        dim as long rh = (this.rcBody(p).bottom - this.rcBody(p).top) \ 6        ' floor; leftover = bottom margin
        if cw < 0 then cw = 0
        if rh < 0 then rh = 0
        this.cellW(p) = cw
        this.rowH(p)  = rh

        for i as long = 0 to 6
            SetRect( @this.rcWeekday(p, i), L + i * cw, weekBand.top, L + i * cw + cw, weekBand.bottom )
        next
        for k as long = 0 to 41
            dim as long ccol = k mod 7
            dim as long crow = k \ 7
            dim as long x  = this.rcBody(p).left + ccol * cw
            dim as long yy = this.rcBody(p).top  + crow * rh
            SetRect( @this.rcDayCell(p, k), x, yy, x + cw, yy + rh )
        next
    next
end sub


' ========================================================================================
' Labels. Every one goes through the per-instance localization table first and falls back to
' the built-in English. An EMPTY table entry means "not overridden", so a host may localize
' any subset -- and clearing an entry reverts just that one.
' ========================================================================================
private function CAL_MonthName( byval pCal as PSCALENDAR ptr, byval m as long ) as DWSTRING
    if (m < 1) orelse (m > 12) then return ""
    if pCal then
        if len( pCal->wszMonthName(m) ) then return pCal->wszMonthName(m)
    end if
    select case m
    case 1  : return "January"
    case 2  : return "February"
    case 3  : return "March"
    case 4  : return "April"
    case 5  : return "May"
    case 6  : return "June"
    case 7  : return "July"
    case 8  : return "August"
    case 9  : return "September"
    case 10 : return "October"
    case 11 : return "November"
    case 12 : return "December"
    end select
    return ""
end function

private function CAL_MonthAbbrev( byval pCal as PSCALENDAR ptr, byval m as long ) as DWSTRING
    if (m < 1) orelse (m > 12) then return ""
    if pCal then
        if len( pCal->wszMonthAbbrev(m) ) then return pCal->wszMonthAbbrev(m)
    end if
    select case m
    case 1  : return "Jan"
    case 2  : return "Feb"
    case 3  : return "Mar"
    case 4  : return "Apr"
    case 5  : return "May"
    case 6  : return "Jun"
    case 7  : return "Jul"
    case 8  : return "Aug"
    case 9  : return "Sep"
    case 10 : return "Oct"
    case 11 : return "Nov"
    case 12 : return "Dec"
    end select
    return ""
end function

' Header label for COLUMN i. The localization table is indexed by the DAY OF WEEK (0 = Sunday)
' and never by the column, so a host sets it once and it stays correct through a week-start
' change -- which is exactly the thing a column-indexed table would get silently wrong.
private function CAL_WeekdayAbbrev( byval pCal as PSCALENDAR ptr, byval i as long, byval nWeekStart as long ) as DWSTRING
    if (i < 0) orelse (i > 6) then return ""
    dim as long dow = i                       ' Sunday-start: column 0 = Sunday
    if nWeekStart = CAL_MONDAY then dow = (i + 1) mod 7
    if pCal then
        if len( pCal->wszWeekdayAbbrev(dow) ) then return pCal->wszWeekdayAbbrev(dow)
    end if
    select case dow
    case 0 : return "Su"
    case 1 : return "Mo"
    case 2 : return "Tu"
    case 3 : return "We"
    case 4 : return "Th"
    case 5 : return "Fr"
    case 6 : return "Sa"
    end select
    return ""
end function

' The nav band's caption. In DAYS view it describes the whole displayed PAGE, not one month,
' which is what makes one shared band work at 1 panel and at 12.
private function CAL_TitleText( byval pCal as PSCALENDAR ptr ) as DWSTRING
    if pCal = 0 then return ""
    select case pCal->eView
    case CAL_VIEW_MONTHS
        return str( pCal->stAnchor.wYear )
    case CAL_VIEW_YEARS
        dim as long nBase = PsCalendar_DecadeBase( pCal->stAnchor.wYear )
        return str(nBase) & " - " & str(nBase + 9)
    case else   ' DAYS
        dim as long np = pCal->PanelCount()
        if np <= 1 then
            return CAL_MonthName( pCal, pCal->panelMonth(0) ) & " " & str( pCal->panelYear(0) )
        end if
        dim as long y0 = pCal->panelYear(0),    m0 = pCal->panelMonth(0)
        dim as long y1 = pCal->panelYear(np-1), m1 = pCal->panelMonth(np-1)
        if y0 = y1 then
            return CAL_MonthAbbrev( pCal, m0 ) & " - " & CAL_MonthAbbrev( pCal, m1 ) & " " & str(y1)
        end if
        return CAL_MonthAbbrev( pCal, m0 ) & " " & str(y0) & " - " & _
               CAL_MonthAbbrev( pCal, m1 ) & " " & str(y1)
    end select
end function


' ========================================================================================
' Range
' ========================================================================================
private function CAL_InRange( byval pCal as PSCALENDAR ptr, byref st as SYSTEMTIME ) as boolean
    if pCal = 0 then return true
    dim as longint s = CAL_Serial(st)
    if pCal->hasMin then
        if s < CAL_Serial(pCal->stMin) then return false
    end if
    if pCal->hasMax then
        if s > CAL_Serial(pCal->stMax) then return false
    end if
    return true
end function

' Add months to a month-anchor, CLAMPED to [CCAL_MIN_YEAR, CCAL_MAX_YEAR].
'
' The arithmetic runs in `long` and the clamp is applied BEFORE the result is written into a
' SYSTEMTIME, which is the whole point: wYear is a WORD, so a year that has already gone
' negative arrives here as ~65535 and there is nothing left to detect. Every write to the
' anchor goes through this.
private function CAL_AddMonthsClamped( byref st as SYSTEMTIME, byval delta as long ) as SYSTEMTIME
    dim as long total   = cast(long, st.wYear) * 12 + (cast(long, st.wMonth) - 1) + delta
    dim as long loTotal = CCAL_MIN_YEAR * 12
    dim as long hiTotal = CCAL_MAX_YEAR * 12 + 11
    if total < loTotal then total = loTotal
    if total > hiTotal then total = hiTotal
    ' total is now >= 12, so the negative-modulo correction AddMonths needs cannot arise.
    return PsCalendar_MakeDate( total \ 12, (total mod 12) + 1, 1 )
end function

' Where a chevron press would land the anchor. ONE implementation, used both to perform the
' step and to decide whether the chevron is dead -- so the two cannot drift apart.
private function CAL_NavTarget( byval pCal as PSCALENDAR ptr, byval nDir as long ) as SYSTEMTIME
    dim as SYSTEMTIME st
    if pCal = 0 then return st
    dim as long y = pCal->stAnchor.wYear
    select case pCal->eView
    case CAL_VIEW_MONTHS
        y += nDir
    case CAL_VIEW_YEARS
        y += nDir * 10
    case else
        return CAL_AddMonthsClamped( pCal->stAnchor, nDir * CAL_EffStep(pCal) )
    end select
    if y < CCAL_MIN_YEAR then y = CCAL_MIN_YEAR
    if y > CCAL_MAX_YEAR then y = CCAL_MAX_YEAR
    return PsCalendar_MakeDate( y, pCal->stAnchor.wMonth, 1 )
end function

' Clamp a date into [min, max]. Used by every keyboard move, so an arrow key can never walk
' the selection out of the range -- nor past the hard year limits.
private function CAL_ClampToRange( byval pCal as PSCALENDAR ptr, byref st as SYSTEMTIME ) as SYSTEMTIME
    if pCal = 0 then return st
    dim as longint s = CAL_Serial(st)
    ' The hard year window first, so it holds even with no host range set.
    dim as longint yLo = PsCalendar_DateToSerial( CCAL_MIN_YEAR, 1, 1 )
    dim as longint yHi = PsCalendar_DateToSerial( CCAL_MAX_YEAR, 12, 31 )
    if s < yLo then s = yLo
    if s > yHi then s = yHi
    if pCal->hasMin then
        dim as longint mn = CAL_Serial(pCal->stMin)
        if s < mn then s = mn
    end if
    if pCal->hasMax then
        dim as longint mx = CAL_Serial(pCal->stMax)
        if s > mx then s = mx
    end if
    dim as long yy, mm, dd
    PsCalendar_SerialToDate( s, yy, mm, dd )
    return PsCalendar_MakeDate( yy, mm, dd )
end function


' ========================================================================================
' Chevron enable/disable
'
' The test is "does pressing this reveal anything usable", NOT "is the whole new page in
' range" -- so with a step SMALLER than the page (a sliding two-month range picker,
' SetNavStep(1)) the button stays live until the genuinely new month runs out, one month
' later than a naive test would allow. That subtlety is inherited from PsDatePicker's
' two-month popup and generalized here to any (step, panelCount) pair:
'
'     newly revealed by NEXT: months [anchor + max(step, np) .. anchor + step + np - 1]
'     newly revealed by PREV: months [anchor - step .. anchor - max(1, step - np + 1)]
' ========================================================================================
private function CAL_EffStep( byval pCal as PSCALENDAR ptr ) as long
    if pCal = 0 then return 1
    if pCal->nNavStep > 0 then return pCal->nNavStep
    return pCal->PanelCount()               ' 0 = one whole page
end function

' Would this direction move the anchor at all? At the hard year limits the clamped target IS
' the current anchor, so the chevron is dead. Asking CAL_NavTarget rather than re-deriving the
' limit keeps the disable rule and the clamp from ever disagreeing.
private function CAL_NavWouldStall( byval pCal as PSCALENDAR ptr, byval nDir as long ) as boolean
    if pCal = 0 then return false
    dim as SYSTEMTIME stTo  = CAL_NavTarget( pCal, nDir )
    dim as SYSTEMTIME stNow = PsCalendar_MakeDate( pCal->stAnchor.wYear, pCal->stAnchor.wMonth, 1 )
    return CAL_SameYMD( stTo, stNow )
end function

private function CAL_PrevDisabled( byval pCal as PSCALENDAR ptr ) as boolean
    if pCal = 0 then return false
    if CAL_NavWouldStall( pCal, -1 ) then return true
    if pCal->hasMin = false then return false
    dim as longint minS = CAL_Serial( pCal->stMin )
    select case pCal->eView
    case CAL_VIEW_DAYS
        dim as long np = pCal->PanelCount()
        dim as long s  = CAL_EffStep( pCal )
        dim as long lastNew = s - np + 1
        if lastNew < 1 then lastNew = 1
        dim as SYSTEMTIME stPrev = PsCalendar_AddMonths( pCal->stAnchor, -lastNew )
        dim as long lastD = PsCalendar_DaysInMonth( stPrev.wYear, stPrev.wMonth )
        return PsCalendar_DateToSerial( stPrev.wYear, stPrev.wMonth, lastD ) < minS
    case CAL_VIEW_MONTHS
        return PsCalendar_DateToSerial( pCal->stAnchor.wYear - 1, 12, 31 ) < minS
    case CAL_VIEW_YEARS
        dim as long nBase = PsCalendar_DecadeBase( pCal->stAnchor.wYear )
        return PsCalendar_DateToSerial( nBase - 1, 12, 31 ) < minS
    end select
    return false
end function

private function CAL_NextDisabled( byval pCal as PSCALENDAR ptr ) as boolean
    if pCal = 0 then return false
    if CAL_NavWouldStall( pCal, 1 ) then return true
    if pCal->hasMax = false then return false
    dim as longint maxS = CAL_Serial( pCal->stMax )
    select case pCal->eView
    case CAL_VIEW_DAYS
        dim as long np = pCal->PanelCount()
        dim as long s  = CAL_EffStep( pCal )
        dim as long firstNew = s
        if np > firstNew then firstNew = np
        dim as SYSTEMTIME stNext = PsCalendar_AddMonths( pCal->stAnchor, firstNew )
        return PsCalendar_DateToSerial( stNext.wYear, stNext.wMonth, 1 ) > maxS
    case CAL_VIEW_MONTHS
        return PsCalendar_DateToSerial( pCal->stAnchor.wYear + 1, 1, 1 ) > maxS
    case CAL_VIEW_YEARS
        dim as long nBase = PsCalendar_DecadeBase( pCal->stAnchor.wYear )
        return PsCalendar_DateToSerial( nBase + 10, 1, 1 ) > maxS
    end select
    return false
end function


' ========================================================================================
' Cell lookup and the focus cell
' ========================================================================================
' Locate a date in the displayed grid. Runs TWO passes on purpose: a date at a month boundary
' appears in two panels at once (as a trailing day of one and a real day of the next), and the
' focus ring belongs on the one where it is a real day.
private function CAL_FindCell( byval pCal as PSCALENDAR ptr, byref st as SYSTEMTIME, byref panel as long, byref index as long ) as boolean
    panel = -1 : index = -1
    if pCal = 0 then return false
    dim as long np = pCal->PanelCount()
    dim as longint target = CAL_Serial(st)

    for pass as long = 0 to 1
        for p as long = 0 to np - 1
            dim as longint s0 = PsCalendar_DateToSerial( pCal->panelYear(p), pCal->panelMonth(p), 1 ) - pCal->panelLead(p)
            dim as longint k  = target - s0
            if (k >= 0) andalso (k <= 41) then
                dim as boolean bOwn = (st.wYear = pCal->panelYear(p)) andalso (st.wMonth = pCal->panelMonth(p))
                if (pass = 1) orelse bOwn then
                    panel = p
                    index = cast(long, k)
                    return true
                end if
            end if
        next
    next
    return false
end function

' Which cell carries the focus ring. A PURE function of the laid-out state -- there is no
' stored focus cursor in DAYS view, because the arrows move the SELECTION directly
' (MonthCal's model) and a second cursor would be one more thing to keep coherent.
'
' The fallback chain exists so a focused control with NOTHING selected still shows the user
' where they are: selection -> today -> day 1 of panel 0.
private function CAL_FocusCell( byval pCal as PSCALENDAR ptr, byref panel as long, byref index as long ) as boolean
    panel = -1 : index = -1
    if pCal = 0 then return false
    if pCal->eView <> CAL_VIEW_DAYS then return false
    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    if pCal->hasSelection then
        if CAL_FindCell( pCal, pCal->stSelected, panel, index ) then return true
    end if
    if CAL_FindCell( pCal, pCal->stToday, panel, index ) then return true
    panel = 0
    index = pCal->panelLead(0)
    return true
end function

' The date the focus ring is sitting on -- the origin every keyboard move is relative to.
private function CAL_FocusDate( byval pCal as PSCALENDAR ptr ) as SYSTEMTIME
    dim as SYSTEMTIME st
    if pCal = 0 then return st
    if pCal->hasSelection then return pCal->stSelected
    dim as long p, k
    if CAL_FocusCell( pCal, p, k ) then return pCal->CellDate( p, k )
    return pCal->stToday
end function


' ========================================================================================
' Hit-testing  (IDENTITY ONLY -- the click handler applies range and disable)
' ========================================================================================
private function CAL_HitTestPt( byval pCal as PSCALENDAR ptr, byval pt as POINT ) as CAL_HITRESULT
    dim as CAL_HITRESULT hr
    if pCal = 0 then return hr
    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    if PtInRect( @pCal->rcPrev, pt )  then hr.part = CAL_HIT_PREV  : return hr
    if PtInRect( @pCal->rcNext, pt )  then hr.part = CAL_HIT_NEXT  : return hr
    if PtInRect( @pCal->rcTitle, pt ) then hr.part = CAL_HIT_TITLE : return hr

    select case pCal->eView
    case CAL_VIEW_DAYS
        dim as long np = pCal->PanelCount()
        for p as long = 0 to np - 1
            if PtInRect( @pCal->rcBody(p), pt ) then
                dim as long cw = pCal->cellW(p)
                dim as long rh = pCal->rowH(p)
                if (cw > 0) andalso (rh > 0) then
                    dim as long col = (pt.x - pCal->rcBody(p).left) \ cw
                    dim as long row = (pt.y - pCal->rcBody(p).top) \ rh
                    dim as boolean bCol = (col >= 0) andalso (col < 7)
                    dim as boolean bRow = (row >= 0) andalso (row < 6)
                    if bCol andalso bRow then
                        hr.part  = CAL_HIT_DAYCELL
                        hr.panel = p
                        hr.index = row * 7 + col
                        return hr
                    end if
                end if
            end if
        next
    case CAL_VIEW_MONTHS, CAL_VIEW_YEARS
        if PtInRect( @pCal->rcMyGrid, pt ) then
            dim as long cw = (pCal->rcMyGrid.right - pCal->rcMyGrid.left) \ CCAL_MY_COLS
            dim as long rh = (pCal->rcMyGrid.bottom - pCal->rcMyGrid.top) \ CCAL_MY_ROWS
            if (cw > 0) andalso (rh > 0) then
                dim as long col = (pt.x - pCal->rcMyGrid.left) \ cw
                dim as long row = (pt.y - pCal->rcMyGrid.top) \ rh
                dim as boolean bCol = (col >= 0) andalso (col < CCAL_MY_COLS)
                dim as boolean bRow = (row >= 0) andalso (row < CCAL_MY_ROWS)
                if bCol andalso bRow then
                    dim as long idx = row * CCAL_MY_COLS + col
                    if idx <= 11 then
                        if pCal->eView = CAL_VIEW_MONTHS then hr.part = CAL_HIT_MONTHCELL else hr.part = CAL_HIT_YEARCELL
                        hr.index = idx
                        return hr
                    end if
                end if
            end if
        end if
    end select
    return hr
end function


' ========================================================================================
' Tooltip: a tracking tip over day cells, created lazily and ONLY if the host set a
' TooltipCallback. The control works identically with it absent.
' ========================================================================================
private sub CAL_TipEnsure( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then return
    ' TRACKED must be declared before the tool is built: it changes the comctl32 tool's
    ' flags (TTF_TRACK or TTF_ABSOLUTE, text pushed not pulled), so a tool built without it
    ' would be of the wrong kind and TTM_TRACKPOSITION would do nothing.
    PsTipHost_SetTracked( @pCal->tip, true )
    PsTipHost_Ensure( @pCal->tip, pCal->hWin )
end sub

private sub CAL_TipHide( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then return
    if pCal->tipKey = -1 then return
    PsTipHost_TrackHide( @pCal->tip )
    pCal->tipKey = -1
end sub

private sub CAL_TipUpdate( byval pCal as PSCALENDAR ptr, byref hr as CAL_HITRESULT )
    if pCal = 0 then return
    if pCal->TooltipCallback = 0 then CAL_TipHide(pCal) : return
    if hr.part <> CAL_HIT_DAYCELL then CAL_TipHide(pCal) : return

    dim as SYSTEMTIME st = pCal->CellDate( hr.panel, hr.index )
    dim as longint key = CAL_Serial(st)
    if key = pCal->tipKey then return          ' already showing this day

    dim as PSCALENDAR_TOOLTIPINFO t
    t.hCtrl   = pCal->hWin
    t.stDate  = st
    t.isValid = true
    t.outText = ""
    dim as boolean bWant = pCal->TooltipCallback( @t )
    if (bWant = false) orelse (len(t.outText) = 0) then CAL_TipHide(pCal) : return

    CAL_TipEnsure(pCal)

    ' Both a POINT and the CELL RECT, in screen coordinates, because the two backends place
    ' differently: comctl32 is told where to put its corner, PsTooltip is given the anchor and
    ' does its own flip and work-area clamp. The point keeps the +4/+2 nudge this control has
    ' always applied, so the system backend lands on exactly the pixels it used to.
    dim as RECT rcCell = pCal->rcDayCell( hr.panel, hr.index )
    dim as POINT pp
    pp.x = rcCell.left + 4
    pp.y = rcCell.bottom + 2
    ClientToScreen( pCal->hWin, @pp )

    dim as RECT rcScr = rcCell
    dim as POINT ptTL : ptTL.x = rcCell.left  : ptTL.y = rcCell.top
    dim as POINT ptBR : ptBR.x = rcCell.right : ptBR.y = rcCell.bottom
    ClientToScreen( pCal->hWin, @ptTL )
    ClientToScreen( pCal->hWin, @ptBR )
    rcScr.left = ptTL.x : rcScr.top = ptTL.y : rcScr.right = ptBR.x : rcScr.bottom = ptBR.y

    PsTipHost_TrackShow( @pCal->tip, pp, rcScr, t.outText )
    pCal->tipKey = key
end sub


' ========================================================================================
' Painting
'
' The renderer takes an ALREADY-STARTED buffer and is split from the WM_PAINT wrapper that
' owns it. That split is the only reason the offscreen probes can drive the exact code path
' WM_PAINT uses -- PsDatePicker shipped a per-cell probe that reported 59 healthy tones while
' the whole popup was a flat rectangle, because it never ran the real painter.
' ========================================================================================
private function CAL_TitleFontOf( byval pCal as PSCALENDAR ptr ) as HFONT
    if pCal = 0 then return 0
    if pCal->hTitleFont then return pCal->hTitleFont
    return pCal->hMainFont
end function

private function CAL_WeekdayFontOf( byval pCal as PSCALENDAR ptr ) as HFONT
    if pCal = 0 then return 0
    if pCal->hWeekdayFont then return pCal->hWeekdayFont
    return pCal->hMainFont
end function

' A left/right chevron drawn as two strokes: crisp at any size, and no glyph font needed.
' PsBufferPaint.PaintLine DPI-scales the pen width it is handed, so nChevThick must arrive
' here UNSCALED (a glyph's stroke should grow with the glyph; a hairline should not).
private sub CAL_PaintChevron( byval b as PsBufferPaint ptr, byref rc as RECT, byval bLeft as boolean, byval clr as COLORREF, byval thick as long )
    if b = 0 then exit sub
    dim as long cx = (rc.left + rc.right) \ 2
    dim as long cy = (rc.top + rc.bottom) \ 2
    dim as long d  = (rc.bottom - rc.top) \ 6
    if d < 3 then d = 3
    if thick < 1 then thick = 1
    b->SetPenColor( clr )
    if bLeft then
        b->PaintLine( thick, cx + d, cy - d, cx - d, cy )
        b->PaintLine( thick, cx - d, cy, cx + d, cy + d )
    else
        b->PaintLine( thick, cx - d, cy - d, cx + d, cy )
        b->PaintLine( thick, cx + d, cy, cx - d, cy + d )
    end if
end sub

' PaintRoundOutline, NEVER PaintBorderRect. Every fill-and-stroke primitive routes through
' PaintRectFactory, which calls FillPath UNCONDITIONALLY before it strokes -- so
' PaintBorderRect used as a ring floods whatever is beneath it with the last back colour set.
' That bug has landed five times in this family, once inside a control's own painter.
private sub CAL_PaintRing( byval b as PsBufferPaint ptr, byref rc as RECT, byval clr as COLORREF, byval nCurve as long, byval thick as long )
    if b = 0 then exit sub
    if thick < 1 then thick = 1
    b->SetPenColor( clr )
    b->PaintRoundOutline( @rc, nCurve, thick )
end sub

' The inset rounded chip a day cell's hot / selected fill uses. Shared so the focus ring can
' sit exactly one pixel outside it instead of fighting it.
private sub CAL_DayChip( byref rcCell as RECT, byref rcFill as RECT, byref nEll as long )
    rcFill = rcCell
    dim as long inset = (rcCell.bottom - rcCell.top) \ 8
    if inset < 1 then inset = 1
    InflateRect( @rcFill, -inset, -inset )
    nEll = (rcFill.bottom - rcFill.top) \ 3
    if nEll < 2 then nEll = 2
end sub

private sub CAL_PaintDayDefault( byval b as PsBufferPaint ptr, byval pColors as PSCALENDAR_COLORS ptr, byval p as PSCALENDAR_DAYPAINTINFO ptr, byval bShowTodayRing as boolean )
    if (b = 0) orelse (pColors = 0) orelse (p = 0) then exit sub

    dim as RECT rcFill
    dim as long ell
    CAL_DayChip( p->rcCell, rcFill, ell )

    if p->isSelected then
        b->SetBackColor( pColors->DayBackColorSelected )
        b->PaintRoundRect( @rcFill, ell )
    elseif p->isHot andalso (p->isDisabled = false) then
        b->SetBackColor( pColors->DayBackColorHot )
        b->PaintRoundRect( @rcFill, ell )
    end if

    dim as boolean bRing = bShowTodayRing andalso p->isToday andalso (p->isSelected = false)
    if bRing then
        CAL_PaintRing( b, rcFill, pColors->TodayRingColor, ell, 1 )
    end if

    dim as COLORREF clr
    if p->isDisabled then
        clr = pColors->DayColorDisabled
    elseif p->isSelected then
        clr = pColors->DayForeColorSelected
    elseif p->isHot then
        clr = pColors->DayForeColorHot
    elseif p->isAdjacent then
        clr = pColors->DayColorAdjacent
    elseif p->isWeekend then
        clr = pColors->DayColorWeekend
    else
        clr = pColors->DayColor
    end if

    b->SetForeColor( clr )
    b->PaintText( str(p->dayNumber), @p->rcCell, DT_CENTER )
end sub

private sub CAL_PaintMyDefault( byval b as PsBufferPaint ptr, byval pColors as PSCALENDAR_COLORS ptr, byval p as PSCALENDAR_MYPAINTINFO ptr )
    if (b = 0) orelse (pColors = 0) orelse (p = 0) then exit sub

    dim as RECT rcFill = p->rcCell
    dim as long inset = (p->rcCell.bottom - p->rcCell.top) \ 6
    if inset < 2 then inset = 2
    InflateRect( @rcFill, -inset, -inset )
    dim as long ell = (rcFill.bottom - rcFill.top) \ 3
    if ell < 2 then ell = 2

    if p->isSelected then
        b->SetBackColor( pColors->DayBackColorSelected )
        b->PaintRoundRect( @rcFill, ell )
    elseif p->isHot then
        b->SetBackColor( pColors->DayBackColorHot )
        b->PaintRoundRect( @rcFill, ell )
    end if

    dim as COLORREF clr = pColors->DayColor
    if p->isDimmed then
        clr = pColors->DayColorAdjacent
    elseif p->isSelected then
        clr = pColors->DayForeColorSelected
    elseif p->isHot then
        clr = pColors->DayForeColorHot
    end if
    b->SetForeColor( clr )
    b->PaintText( p->wszLabel, @p->rcCell, DT_CENTER )
end sub


' Draw everything into an already-started buffer whose client has already been filled with
' BackColor. The frame is drawn by the WM_PAINT wrapper, AFTER this, so it is never painted
' over -- and so a wholesale PaintCallback cannot lose it.
private sub CAL_Render( byval pCal as PSCALENDAR ptr, byval pb as PsBufferPaint ptr )
    if (pCal = 0) orelse (pb = 0) then exit sub
    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    dim as PSCALENDAR_COLORS ptr pc = @pCal->colors
    dim as boolean bEnabled  = pCal->isEnabled
    dim as boolean bPrevOff  = CAL_PrevDisabled(pCal)
    dim as boolean bNextOff  = CAL_NextDisabled(pCal)
    dim as long    np        = pCal->PanelCount()

    ' ---- the wholesale hook replaces everything between the background and the frame ----
    if pCal->PaintCallback then
        dim as PSCALENDAR_PAINTINFO pi
        pi.hCtrl          = pCal->hWin
        pi.b              = pb
        pi.rcClient       = pCal->rcClient
        pi.rcInner        = pCal->rcInner
        pi.rcNavBand      = pCal->rcNavBand
        pi.rcPrev         = pCal->rcPrev
        pi.rcTitle        = pCal->rcTitle
        pi.rcNext         = pCal->rcNext
        pi.rcGrid         = pCal->rcGrid
        pi.eView          = pCal->eView
        pi.nCols          = pCal->nCols
        pi.nRows          = pCal->nRows
        pi.panelCount     = np
        pi.isEnabled      = bEnabled
        pi.isFocused      = pCal->isFocused
        pi.isPrevDisabled = bPrevOff
        pi.isNextDisabled = bNextOff
        pi.hotPart        = pCal->hotPart
        pi.hotPanel       = pCal->hotPanel
        pi.hotIndex       = pCal->hotIndex
        pCal->PaintCallback( @pi )
        exit sub
    end if

    ' ---- nav band ----
    dim as HFONT hTitle = CAL_TitleFontOf( pCal )
    if hTitle then pb->SetFont( hTitle )
    dim as COLORREF clrTitle = pc->TitleColor
    if pCal->hotPart = CAL_HIT_TITLE then clrTitle = pc->TitleColorHot
    pb->SetForeColor( clrTitle )
    pb->PaintText( CAL_TitleText(pCal), @pCal->rcTitle, DT_CENTER )

    dim as COLORREF clrPrev = pc->NavGlyphColor
    if bPrevOff then
        clrPrev = pc->NavGlyphColorDisabled
    elseif pCal->hotPart = CAL_HIT_PREV then
        clrPrev = pc->NavGlyphColorHot
    end if
    dim as COLORREF clrNext = pc->NavGlyphColor
    if bNextOff then
        clrNext = pc->NavGlyphColorDisabled
    elseif pCal->hotPart = CAL_HIT_NEXT then
        clrNext = pc->NavGlyphColorHot
    end if
    CAL_PaintChevron( pb, pCal->rcPrev, true,  clrPrev, pCal->nChevThick )
    CAL_PaintChevron( pb, pCal->rcNext, false, clrNext, pCal->nChevThick )

    ' ---- the focus cell, resolved once so both the cell info and the ring agree ----
    dim as long focPanel = -1, focIndex = -1
    dim as boolean bHaveFocus = false
    if pCal->isFocused andalso (pCal->eView = CAL_VIEW_DAYS) then
        bHaveFocus = CAL_FocusCell( pCal, focPanel, focIndex )
    end if

    ' ---- the SELECTED cell, resolved the same way, and for the same reason ----
    '
    ' EXACTLY ONE cell may render as selected. A date on a month boundary is displayed TWICE
    ' whenever its neighbouring month is also on the page -- as a real day in its own panel and
    ' as a dimmed leading/trailing day in the next one. Deciding `isSelected` by comparing the
    ' cell's date to the selection therefore lights BOTH copies, and since the selected chip is
    ' the loudest thing on the surface, the user cannot tell which cell the keyboard is on.
    '
    ' Resolving the cell once, through the same CAL_FindCell the focus ring uses, guarantees the
    ' chip and the ring always land together. CAL_FindCell prefers the panel that OWNS the date
    ' and falls back to a neighbour that merely shows it, so the selection can never vanish --
    ' it just stops appearing twice.
    dim as long selPanel = -1, selIndex = -1
    dim as boolean bHaveSel = false
    if pCal->hasSelection then
        bHaveSel = CAL_FindCell( pCal, pCal->stSelected, selPanel, selIndex )
    end if

    if pCal->eView = CAL_VIEW_DAYS then
        dim as HFONT hWeek = CAL_WeekdayFontOf( pCal )
        for p as long = 0 to np - 1
            ' panel caption (absent at one panel -- the nav-band title already names the month)
            if pCal->rcCaption(p).bottom > pCal->rcCaption(p).top then
                if hTitle then pb->SetFont( hTitle )
                pb->SetForeColor( pc->CaptionColor )
                pb->PaintText( CAL_MonthName( pCal, pCal->panelMonth(p) ), @pCal->rcCaption(p), DT_CENTER )
            end if

            ' weekday header row
            if hWeek then pb->SetFont( hWeek )
            pb->SetForeColor( pc->WeekdayColor )
            for i as long = 0 to 6
                pb->PaintText( CAL_WeekdayAbbrev( pCal, i, pCal->nWeekStart ), @pCal->rcWeekday(p, i), DT_CENTER )
            next

            ' day cells
            for k as long = 0 to 41
                dim as SYSTEMTIME st = pCal->CellDate( p, k )
                dim as PSCALENDAR_DAYPAINTINFO d
                d.hCtrl      = pCal->hWin
                d.b          = pb
                d.rcCell     = pCal->rcDayCell(p, k)
                d.stDate     = st
                d.dayNumber  = st.wDay
                d.panelIndex = p
                d.cellIndex  = k
                d.isAdjacent = (st.wMonth <> pCal->panelMonth(p)) orelse (st.wYear <> pCal->panelYear(p))
                dim as long dow = PsCalendar_DowSunday( st.wYear, st.wMonth, st.wDay )
                d.isWeekend  = (dow = 0) orelse (dow = 6)
                d.isToday    = CAL_SameYMD( st, pCal->stToday )
                d.isSelected = bHaveSel andalso (selPanel = p) andalso (selIndex = k)
                dim as boolean bHotHere = (pCal->hotPart = CAL_HIT_DAYCELL) andalso (pCal->hotPanel = p) andalso (pCal->hotIndex = k)
                d.isHot      = bHotHere
                d.isFocused  = bHaveFocus andalso (focPanel = p) andalso (focIndex = k)
                d.isDisabled = (CAL_InRange( pCal, st ) = false)
                d.isEnabled  = bEnabled

                if pCal->hMainFont then pb->SetFont( pCal->hMainFont )
                if pCal->DayPaintCallback then
                    pCal->DayPaintCallback( @d )
                else
                    CAL_PaintDayDefault( pb, pc, @d, pCal->bShowTodayRing )
                end if
            next
        next

        ' THE FOCUS RING IS DRAWN BY THE CONTROL, AFTER the cell painter -- built-in or host.
        ' A host callback that ignores isFocused therefore cannot make the keyboard focus
        ' invisible, which is the failure mode worth being defensive about. The flag is in the
        ' info struct so a host can style AROUND the ring, not so it can be responsible for it.
        if bHaveFocus then
            dim as RECT rcFill
            dim as long ell
            CAL_DayChip( pCal->rcDayCell(focPanel, focIndex), rcFill, ell )
            InflateRect( @rcFill, 1, 1 )
            CAL_PaintRing( pb, rcFill, pc->FocusRingColor, ell + 1, pCal->nFocusThick )
        end if
    else
        ' ---- MONTHS / YEARS ----
        dim as long nBase = PsCalendar_DecadeBase( pCal->stAnchor.wYear )
        for k as long = 0 to 11
            dim as PSCALENDAR_MYPAINTINFO m
            m.hCtrl     = pCal->hWin
            m.b         = pb
            m.rcCell    = pCal->rcMyCell(k)
            m.eView     = pCal->eView
            m.cellIndex = k
            m.isEnabled = bEnabled
            m.isFocused = pCal->isFocused andalso (pCal->nMyFocus = k)

            if pCal->eView = CAL_VIEW_MONTHS then
                m.nValue     = k + 1
                m.wszLabel   = CAL_MonthAbbrev( pCal, k + 1 )
                m.isSelected = pCal->hasSelection andalso (pCal->stSelected.wYear = pCal->stAnchor.wYear) andalso (pCal->stSelected.wMonth = (k + 1))
                m.isHot      = (pCal->hotPart = CAL_HIT_MONTHCELL) andalso (pCal->hotIndex = k)
                m.isDimmed   = false
            else
                ' The decade grid shows 12 cells for 10 years: index 0 is base-1 and index 11
                ' is base+10. Both are dimmed but both are still CLICKABLE -- they are the
                ' one-step-either-way escape out of the decade.
                dim as long yy = nBase - 1 + k
                m.nValue     = yy
                m.wszLabel   = str( yy )
                m.isSelected = pCal->hasSelection andalso (pCal->stSelected.wYear = yy)
                m.isHot      = (pCal->hotPart = CAL_HIT_YEARCELL) andalso (pCal->hotIndex = k)
                m.isDimmed   = (yy < nBase) orelse (yy > nBase + 9)
            end if

            if pCal->hMainFont then pb->SetFont( pCal->hMainFont )
            if pCal->MonthYearPaintCallback then
                pCal->MonthYearPaintCallback( @m )
            else
                CAL_PaintMyDefault( pb, pc, @m )
            end if
        next

        if pCal->isFocused then
            dim as long kf = pCal->nMyFocus
            if (kf >= 0) andalso (kf <= 11) then
                dim as RECT rcF = pCal->rcMyCell(kf)
                dim as long inset = (rcF.bottom - rcF.top) \ 6
                if inset < 2 then inset = 2
                InflateRect( @rcF, -(inset - 1), -(inset - 1) )
                dim as long ell = (rcF.bottom - rcF.top) \ 3
                if ell < 2 then ell = 2
                CAL_PaintRing( pb, rcF, pc->FocusRingColor, ell, pCal->nFocusThick )
            end if
        end if
    end if

    if gCAL_Trace = 1 then
        dim as RECT c0 = pCal->rcDayCell(0, 0)
        dim as string sg = "Geom client=" & str(pCal->rcClient.right) & "x" & str(pCal->rcClient.bottom) & _
            " grid=" & str(pCal->nCols) & "x" & str(pCal->nRows) & " np=" & str(np) & _
            " ideal=" & str(pCal->nIdealW) & "x" & str(pCal->nIdealH) & _
            " cellW=" & str(pCal->cellW(0)) & " rowH=" & str(pCal->rowH(0)) & _
            " cell0=(" & str(c0.left) & "," & str(c0.top) & "," & str(c0.right) & "," & str(c0.bottom) & ")" & _
            " title=[" & CAL_TitleText(pCal) & "]" & " focus=" & str(focPanel) & "/" & str(focIndex)
        CAL_Log( sg )
    end if
end sub


' Background, content, frame -- the WHOLE visual, into an already-started buffer. WM_PAINT and
' every offscreen probe both go through here, so a probe cannot be measuring a different
' composition from the one the user sees. PsDatePicker's probe drove one day cell through the
' host callback and reported 59 healthy tones while the entire popup was a flat rectangle.
private sub CAL_RenderFull( byval pCal as PSCALENDAR ptr, byval pb as PsBufferPaint ptr )
    if (pCal = 0) orelse (pb = 0) then exit sub
    pb->SetBackColor( pCal->colors.BackColor )
    pb->PaintClientRect()

    CAL_Render( pCal, pb )

    ' The frame goes on LAST, over everything, so no painter -- built-in or host-supplied --
    ' can lose it.
    if pCal->nBorderThick > 0 then
        dim as RECT rcEdge = pCal->rcClient
        CAL_PaintRing( pb, rcEdge, pCal->colors.BorderColor, 0, pCal->nBorderThick )
    end if
end sub

' WM_PAINT: one buffer per repaint.
private sub CAL_Paint( byval hCtrl as HWND )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer( hCtrl )
    if pCal = 0 then exit sub
    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCtrl )
    CAL_RenderFull( pCal, @b )
    b.EndDoubleBuffer()
end sub


' ========================================================================================
' Notification
'
' PROGRAMMATIC SETTERS ARE SILENT; only user interaction notifies. That is the family rule
' (Win32's own TCM_SETCURSEL / TCN_SELCHANGE split), and it is what lets a host call a setter
' from inside its own handler without re-entering itself.
' ========================================================================================
private sub CAL_NotifySelChange( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then exit sub
    if pCal->SelChangeCallback = 0 then exit sub
    pCal->SelChangeCallback( pCal->hWin, @pCal->stSelected, pCal->hasSelection )
end sub

private sub CAL_NotifyActivated( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then exit sub
    if pCal->hasSelection = false then exit sub
    if pCal->DateActivatedCallback = 0 then exit sub
    pCal->DateActivatedCallback( pCal->hWin, @pCal->stSelected )
end sub

private sub CAL_Relayout( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then exit sub
    pCal->bLayoutDirty = true
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, TRUE )
end sub


' Scroll the page, if needed, so that st's OWN month is one of the displayed panels.
'
' "Its own month", not merely "somewhere on screen": a date can be visible as a dimmed
' adjacent cell of a neighbouring month, and leaving the selection sitting there would put the
' focus ring on a cell the user reads as belonging to a different month. Already satisfied at
' twelve panels for any date inside the displayed year, so a year grid does not scroll while
' arrowing through it.
private sub CAL_EnsureMonthVisible( byval pCal as PSCALENDAR ptr, byref st as SYSTEMTIME )
    if pCal = 0 then exit sub
    dim as long np = pCal->PanelCount()
    for p as long = 0 to np - 1
        dim as boolean bHit = (pCal->panelYear(p) = st.wYear) andalso (pCal->panelMonth(p) = st.wMonth)
        if bHit then exit sub
    next

    dim as long am = pCal->panelYear(0) * 12 + (pCal->panelMonth(0) - 1)
    dim as long tm = st.wYear * 12 + (st.wMonth - 1)
    dim as SYSTEMTIME stFirst = PsCalendar_MakeDate( st.wYear, st.wMonth, 1 )
    if tm < am then
        pCal->stAnchor = CAL_AddMonthsClamped( stFirst, 0 )              ' scrolled backwards
    else
        pCal->stAnchor = CAL_AddMonthsClamped( stFirst, -(np - 1) )      ' scrolled forwards
    end if
end sub

' Apply a USER-driven selection: store it, bring its month into view, repaint, and notify --
' but only when something actually changed, so an arrow key held against the range edge does
' not machine-gun the host with identical callbacks.
private sub CAL_SelectUser( byval pCal as PSCALENDAR ptr, byref st as SYSTEMTIME )
    if pCal = 0 then exit sub
    dim as boolean bChanged = (pCal->hasSelection = false) orelse (CAL_SameYMD( st, pCal->stSelected ) = false)
    pCal->stSelected   = st
    pCal->hasSelection = true
    CAL_EnsureMonthVisible( pCal, st )
    CAL_Relayout( pCal )
    if bChanged then CAL_NotifySelChange( pCal )
end sub


' ========================================================================================
' Navigation
' ========================================================================================
private sub CAL_NavStep( byval pCal as PSCALENDAR ptr, byval nDir as long )
    if pCal = 0 then exit sub
    pCal->stAnchor = CAL_NavTarget( pCal, nDir )
    CAL_Relayout( pCal )
end sub

private sub CAL_ZoomOut( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then exit sub
    select case pCal->eView
    case CAL_VIEW_DAYS
        pCal->eView    = CAL_VIEW_MONTHS
        pCal->nMyFocus = pCal->stAnchor.wMonth - 1
        CAL_Relayout( pCal )
    case CAL_VIEW_MONTHS
        pCal->eView    = CAL_VIEW_YEARS
        pCal->nMyFocus = pCal->stAnchor.wYear - PsCalendar_DecadeBase( pCal->stAnchor.wYear ) + 1
        CAL_Relayout( pCal )
    end select
    ' YEARS is the top: the title is inert there, deliberately.
end sub

' Zoom in on a drill-down cell. MONTHS -> DAYS picks the month; YEARS -> MONTHS picks the
' year. Neither one selects a date -- drilling is navigation, not a choice.
private sub CAL_ZoomInCell( byval pCal as PSCALENDAR ptr, byval k as long )
    if pCal = 0 then exit sub
    if (k < 0) orelse (k > 11) then exit sub
    select case pCal->eView
    case CAL_VIEW_MONTHS
        pCal->stAnchor = PsCalendar_MakeDate( pCal->stAnchor.wYear, k + 1, 1 )
        pCal->eView    = CAL_VIEW_DAYS
        CAL_Relayout( pCal )
    case CAL_VIEW_YEARS
        dim as long nBase = PsCalendar_DecadeBase( pCal->stAnchor.wYear )
        dim as long yy    = nBase - 1 + k
        pCal->stAnchor = PsCalendar_MakeDate( yy, pCal->stAnchor.wMonth, 1 )
        pCal->eView    = CAL_VIEW_MONTHS
        pCal->nMyFocus = pCal->stAnchor.wMonth - 1
        CAL_Relayout( pCal )
    end select
end sub

private sub CAL_Click( byval pCal as PSCALENDAR ptr, byval pt as POINT, byval bDouble as boolean )
    if pCal = 0 then exit sub
    dim as CAL_HITRESULT hr = CAL_HitTestPt( pCal, pt )

    select case hr.part
    case CAL_HIT_PREV
        if CAL_PrevDisabled(pCal) = false then CAL_NavStep( pCal, -1 )
    case CAL_HIT_NEXT
        if CAL_NextDisabled(pCal) = false then CAL_NavStep( pCal, 1 )
    case CAL_HIT_TITLE
        CAL_ZoomOut( pCal )
    case CAL_HIT_DAYCELL
        dim as SYSTEMTIME st = pCal->CellDate( hr.panel, hr.index )
        if CAL_InRange( pCal, st ) then
            CAL_SelectUser( pCal, st )
            ' A double-click reports BOTH: SelChange first (it did move the selection), then
            ' Activated. The order matters -- a host reading GetSelDate from inside Activated
            ' must already see the new value.
            if bDouble then CAL_NotifyActivated( pCal )
        end if
    case CAL_HIT_MONTHCELL, CAL_HIT_YEARCELL
        CAL_ZoomInCell( pCal, hr.index )
    end select
end sub

private sub CAL_Wheel( byval pCal as PSCALENDAR ptr, byval nDelta as long )
    if pCal = 0 then exit sub
    pCal->nWheelAccum += nDelta
    dim as long nNotches = pCal->nWheelAccum \ WHEEL_DELTA
    if nNotches = 0 then exit sub
    pCal->nWheelAccum -= nNotches * WHEEL_DELTA
    ' Wheel away from the user (positive) goes to the PREVIOUS page, matching a scrollbar's up.
    dim as long nDir = -1
    if nNotches < 0 then nDir = 1
    for i as long = 1 to abs(nNotches)
        if nDir < 0 then
            if CAL_PrevDisabled(pCal) then exit for
            CAL_NavStep( pCal, -1 )
        else
            if CAL_NextDisabled(pCal) then exit for
            CAL_NavStep( pCal, 1 )
        end if
    next
end sub


' ========================================================================================
' Keyboard
'
' In DAYS view the arrows move the SELECTION directly (Win32 MonthCal's model), scrolling the
' page when the selection leaves it. There is deliberately no separate focus cursor: one
' would be a second piece of state to keep coherent with the selection, the scroll position
' and the range, for no behaviour a user asked for.
'
' Returns TRUE when the key was consumed.
'
' bCtrl is a PARAMETER rather than a GetKeyState() call inside, and that is for testability:
' a headless assertion cannot make GetKeyState report a held Ctrl, so Ctrl+PgUp / Ctrl+PgDn /
' Ctrl+Home would have been reachable only by hand. The WndProc reads the real key state and
' passes it in.
' ========================================================================================
private function CAL_KeyDown( byval pCal as PSCALENDAR ptr, byval vk as long, byval bCtrl as boolean ) as boolean
    if pCal = 0 then return false

    ' ---- drill-down grids: a stored 0..11 cursor, because there is no date to move ----
    if pCal->eView <> CAL_VIEW_DAYS then
        dim as long k = pCal->nMyFocus
        select case vk
        case VK_LEFT  : k -= 1
        case VK_RIGHT : k += 1
        case VK_UP    : k -= CCAL_MY_COLS
        case VK_DOWN  : k += CCAL_MY_COLS
        case VK_HOME  : k = 0
        case VK_END   : k = 11
        case VK_PRIOR : CAL_NavStep( pCal, -1 ) : return true
        case VK_NEXT  : CAL_NavStep( pCal,  1 ) : return true
        case VK_SPACE, VK_RETURN
            CAL_ZoomInCell( pCal, pCal->nMyFocus )
            return true
        case VK_ESCAPE
            ' Esc zooms OUT one level, and is claimed ONLY here -- see WM_GETDLGCODE. In DAYS
            ' view it is left alone so a host dialog's Cancel still works.
            if pCal->eView = CAL_VIEW_YEARS then
                pCal->eView    = CAL_VIEW_MONTHS
                pCal->nMyFocus = pCal->stAnchor.wMonth - 1
            else
                pCal->eView = CAL_VIEW_DAYS
            end if
            CAL_Relayout( pCal )
            return true
        case else
            return false
        end select
        if k < 0  then k = 0
        if k > 11 then k = 11
        if k <> pCal->nMyFocus then
            pCal->nMyFocus = k
            if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
        end if
        return true
    end if

    ' ---- DAYS ----
    dim as SYSTEMTIME stFrom = CAL_FocusDate( pCal )
    dim as SYSTEMTIME stTo   = stFrom

    select case vk
    case VK_LEFT  : stTo = PsCalendar_AddDays( stFrom, -1 )
    case VK_RIGHT : stTo = PsCalendar_AddDays( stFrom,  1 )
    case VK_UP    : stTo = PsCalendar_AddDays( stFrom, -7 )
    case VK_DOWN  : stTo = PsCalendar_AddDays( stFrom,  7 )
    case VK_PRIOR
        if bCtrl then stTo = PsCalendar_AddMonths( stFrom, -12 ) else stTo = PsCalendar_AddMonths( stFrom, -1 )
    case VK_NEXT
        if bCtrl then stTo = PsCalendar_AddMonths( stFrom,  12 ) else stTo = PsCalendar_AddMonths( stFrom,  1 )
    case VK_HOME
        if bCtrl then
            stTo = pCal->stToday
        else
            stTo = PsCalendar_MakeDate( stFrom.wYear, stFrom.wMonth, 1 )
        end if
    case VK_END
        stTo = PsCalendar_MakeDate( stFrom.wYear, stFrom.wMonth, PsCalendar_DaysInMonth( stFrom.wYear, stFrom.wMonth ) )
    case VK_SPACE, VK_RETURN
        ' Activate. With nothing selected yet the focus cell is committed first, so a keyboard
        ' user never has to press an arrow before Enter can mean anything.
        if pCal->hasSelection = false then CAL_SelectUser( pCal, stFrom )
        CAL_NotifyActivated( pCal )
        return true
    case else
        return false
    end select

    stTo = CAL_ClampToRange( pCal, stTo )
    CAL_SelectUser( pCal, stTo )
    return true
end function


' ========================================================================================
' Hot tracking
' ========================================================================================
private sub CAL_UpdateHot( byval pCal as PSCALENDAR ptr, byval pt as POINT )
    if pCal = 0 then exit sub

    dim as CAL_HITRESULT hr
    if pCal->isEnabled then
        hr = CAL_HitTestPt( pCal, pt )
        ' A day cell only lights when it is actually clickable, matching how it paints.
        if hr.part = CAL_HIT_DAYCELL then
            dim as SYSTEMTIME st = pCal->CellDate( hr.panel, hr.index )
            if CAL_InRange( pCal, st ) = false then
                hr.part = CAL_HIT_NONE : hr.panel = -1 : hr.index = -1
            end if
        end if
        ' ...and so do the chevrons: a disabled one paints inert, so it must not hover either.
        if (hr.part = CAL_HIT_PREV) andalso CAL_PrevDisabled(pCal) then hr.part = CAL_HIT_NONE
        if (hr.part = CAL_HIT_NEXT) andalso CAL_NextDisabled(pCal) then hr.part = CAL_HIT_NONE
    end if

    dim as boolean bChanged = (hr.part <> pCal->hotPart) orelse (hr.panel <> pCal->hotPanel) orelse (hr.index <> pCal->hotIndex)
    if bChanged then
        pCal->hotPart  = hr.part
        pCal->hotPanel = hr.panel
        pCal->hotIndex = hr.index
        if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
    end if
    CAL_TipUpdate( pCal, hr )
end sub

private sub CAL_ClearHot( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then exit sub
    if pCal->hotPart <> CAL_HIT_NONE then
        pCal->hotPart = CAL_HIT_NONE : pCal->hotPanel = -1 : pCal->hotIndex = -1
        if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
    end if
    CAL_TipHide( pCal )
end sub

private sub CAL_StartHotTimer( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then exit sub
    if pCal->hotTimerOn then exit sub
    if pCal->hWin = 0 then exit sub
    SetTimer( pCal->hWin, IDT_PSCALENDAR_HOTTRACK, PSCALENDAR_HOTTRACK_MS, NULL )
    pCal->hotTimerOn = true
end sub

private sub CAL_StopHotTimer( byval pCal as PSCALENDAR ptr )
    if pCal = 0 then exit sub
    if pCal->hotTimerOn = false then exit sub
    if pCal->hWin then KillTimer( pCal->hWin, IDT_PSCALENDAR_HOTTRACK )
    pCal->hotTimerOn = false
end sub

' Relay a message to the host. The result is HONOURED except where letting a callback
' suppress the default would strand the control (see the typedef).
private function CAL_CallMessage( byval pCal as PSCALENDAR ptr, byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as boolean
    if pCal = 0 then return false
    if pCal->MessageCallback = 0 then return false
    dim as PSCALENDAR_MESSAGEINFO m
    m.hCtrl  = hwnd
    m.uMsg   = uMsg
    m.wParam = wParam
    m.lParam = lParam
    return pCal->MessageCallback( @m )
end function


' ========================================================================================
' Window procedure
'
' NO MOUSE CAPTURE, and that is the considered answer rather than an omission. Capture is
' owed only where something consumes the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing --
' a drag, or a press/cancel gesture. This control acts on the button-DOWN (MonthCal does, and
' so did the popup this came from), so from that instant there is nothing left to protect:
' no WM_CAPTURECHANGED handler, no CancelPress, and the message callback's result is honoured
' for WM_LBUTTONUP like any other message. PsComboBox reached the same negative the same way.
'
' CS_DBLCLKS IS SET, and it is load-bearing here rather than incidental: a double-click is one
' of the two routes to DateActivated. WM_LBUTTONDBLCLK substitutes for the second rapid
' WM_LBUTTONDOWN, so it runs the same click handler and then fires the activate callback.
'
' FOCUS: WS_TABSTOP, a painted focus ring, and a PER-MESSAGE WM_GETDLGCODE.
' ========================================================================================
function PsCalendar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSCALENDAR ptr pCal = GetCalendarPointer( hwnd )

    select case uMsg

    case WM_PAINT
        if CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam ) then return 0
        CAL_Paint( hwnd )
        return 0

    case WM_ERASEBKGND
        return true

    case WM_SIZE
        if pCal then
            pCal->bLayoutDirty = true
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam )
        return 0

    case WM_MOUSEMOVE
        if pCal = 0 then exit select
        if CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam ) then return 0
        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        CAL_UpdateHot( pCal, pt )

        dim tme as TrackMouseEvent
        tme.cbSize    = sizeof(TrackMouseEvent)
        tme.dwFlags   = TME_LEAVE
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme)
        ' WM_MOUSELEAVE is unreliable -- it is missed when a window is created over us, and
        ' after some capture transitions -- so the poll is a safety net, not the mechanism.
        CAL_StartHotTimer( pCal )
        return 0

    case WM_MOUSELEAVE
        if pCal then CAL_ClearHot( pCal )
        CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam )
        return 0

    case WM_TIMER
        if (pCal <> 0) andalso (wParam = IDT_PSCALENDAR_HOTTRACK) then
            dim as POINT pt
            GetCursorPos( @pt )
            dim as HWND hUnder = WindowFromPoint( pt )
            ScreenToClient( hwnd, @pt )
            dim as RECT rc
            GetClientRect( hwnd, @rc )
            dim as boolean bInside = (PtInRect( @rc, pt ) <> 0) andalso (hUnder = hwnd)
            if bInside then
                CAL_UpdateHot( pCal, pt )
            else
                CAL_ClearHot( pCal )
                CAL_StopHotTimer( pCal )
            end if
            return 0
        end if

    case WM_LBUTTONDOWN, WM_LBUTTONDBLCLK
        if pCal = 0 then exit select
        if CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam ) then return 0
        if pCal->isEnabled = false then return 0
        CAL_TipHide( pCal )
        SetFocus( hwnd )
        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        CAL_Click( pCal, pt, (uMsg = WM_LBUTTONDBLCLK) )
        return 0

    case WM_MOUSEWHEEL
        if pCal = 0 then exit select
        if CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam ) then return 0
        if pCal->isEnabled = false then return 0
        CAL_Wheel( pCal, cast(short, hiword(wParam)) )
        return 0

    case WM_SETFOCUS, WM_KILLFOCUS
        if pCal then
            ' Never carry a pending swallow across a focus change -- the WM_CHAR it was armed
            ' for is not coming, and a stale flag would eat the next character instead.
            pCal->bAteKeyDown = false
            dim as boolean bFocused = (uMsg = WM_SETFOCUS)
            if bFocused <> pCal->isFocused then
                pCal->isFocused = bFocused
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            ' Result deliberately IGNORED -- a callback must not be able to leave the control
            ' painted as focused when it is not.
            CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_GETDLGCODE
        ' Claim ONLY the keys we actually consume, and only when asked about a SPECIFIC
        ' message (lParam points at the MSG when it does).
        '
        ' Claiming unconditionally would return DLGC_WANTALLKEYS for Tab as well, and the
        ' control would swallow the very navigation it opted into by being a tabstop. Claiming
        ' nothing would let a host's IsDialogMessage route the arrows elsewhere before the
        ' control ever saw them. Per-message is the only answer that gets both right.
        '
        ' VK_RETURN IS CLAIMED, which DEPARTS from PsOptionButton (a Win32 radio button leaves
        ' Enter for the dialog's default button, and that control commits to radio semantics).
        ' A calendar has no such precedent, and DateActivated -- the whole point of the
        ' callback -- would otherwise have no keyboard route at all. Asserted, so this is a
        ' decision on the record rather than a drift.
        '
        ' VK_ESCAPE is claimed ONLY while drilled down, where it means "zoom back out". In
        ' DAYS view it is left alone so a host dialog's Cancel keeps working.
        ' THE CHARACTER MESSAGES MUST BE CLAIMED TOO, not just the key-downs. Claiming
        ' VK_RETURN makes IsDialogMessage TranslateMessage the keydown, which manufactures a
        ' WM_CHAR of 0x0D; if nothing claims that, it reaches DefWindowProc and the system
        ' BEEPS on every Enter. The keystroke still works -- the beep is the only symptom,
        ' which is exactly why it survives an assertion suite and has to be heard.
        if lParam then
            dim as MSG ptr pMsg = cast( MSG ptr, lParam )
            select case pMsg->message
            case WM_KEYDOWN, WM_KEYUP
                select case pMsg->wParam
                case VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_PRIOR, VK_NEXT, _
                     VK_HOME, VK_END, VK_SPACE, VK_RETURN
                    return DLGC_WANTALLKEYS
                case VK_ESCAPE
                    if pCal then
                        if pCal->eView <> CAL_VIEW_DAYS then return DLGC_WANTALLKEYS
                    end if
                end select
            case WM_CHAR
                ' Claim the character only when the key-down that produced it was consumed.
                if pCal then
                    if pCal->bAteKeyDown then return DLGC_WANTALLKEYS
                end if
            end select
        end if
        return 0

    case WM_KEYDOWN
        if pCal = 0 then exit select
        if pCal->isEnabled = false then return 0
        ' Only these three produce a following WM_CHAR. Arrows and PgUp/PgDn do not, so arming
        ' the flag for them would leave it set and swallow some unrelated later character.
        dim as boolean bMakesChar = (wParam = VK_RETURN) orelse (wParam = VK_SPACE) orelse (wParam = VK_ESCAPE)
        pCal->bAteKeyDown = false
        ' The host sees the key BEFORE it moves anything, so navigation can be vetoed. A vetoed
        ' key is still consumed, so its character is swallowed too -- a host that suppresses a
        ' keystroke does not want the machine beeping about it either.
        if CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam ) then
            pCal->bAteKeyDown = bMakesChar
            return 0
        end if
        dim as boolean bCtrlDown = ((GetKeyState(VK_CONTROL) and &h8000) <> 0)
        if CAL_KeyDown( pCal, cast(long, wParam), bCtrlDown ) then
            pCal->bAteKeyDown = bMakesChar
            return 0
        end if

    case WM_CHAR
        ' SWALLOW THE CHARACTER PRODUCED BY A KEY WE ALREADY ACTED ON. This exists to stop the
        ' system BEEP on Enter, and the mechanism is worth spelling out because the fix looks
        ' unrelated to the symptom.
        '
        ' WM_GETDLGCODE claims VK_RETURN and VK_SPACE, so IsDialogMessage hands those keys to
        ' us instead of routing them to a default button. WM_KEYDOWN consumes them -- but
        ' TranslateMessage has already queued a matching WM_CHAR (Enter = 13, Space = 32), and
        ' that arrives afterwards regardless. Letting it fall through to DefWindowProc is what
        ' beeps: a window with no text handling has nothing to do with the character. The
        ' keystroke works either way, so the beep is the ONLY symptom -- it survives any
        ' assertion suite and has to be heard.
        '
        ' Swallowing is driven by the FLAG, not by re-reading the control's state, and not
        ' unconditionally:
        '   * A state test would be wrong for Esc. Its key-down zooms out and CHANGES eView, so
        '     by the time the character arrives, "are we drilled down?" reads the view the
        '     key-down just left and lets the beep through.
        '   * Swallowing every WM_CHAR would also eat characters we never consumed -- including
        '     Esc in DAYS view, which is deliberately left for the host dialog's Cancel.
        '
        ' WM_SYSCHAR is deliberately NOT touched: swallowing it would break Alt+F4 and the
        ' host's menu mnemonics.
        '
        ' The host is offered the message (a type-to-jump handler would want it) but the result
        ' is IGNORED for the swallowed case: returning FALSE would mean "fall through to
        ' DefWindowProc", which is precisely the beep this arm exists to prevent.
        if pCal = 0 then exit select
        CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam )
        if pCal->bAteKeyDown then
            pCal->bAteKeyDown = false
            return 0
        end if

    case WM_ENABLE
        ' SetEnabled goes through EnableWindow, so this is where the flag actually lands.
        if pCal then
            dim as boolean bOn = (wParam <> 0)
            if bOn <> pCal->isEnabled then
                pCal->isEnabled = bOn
                if bOn = false then CAL_ClearHot( pCal )
                InvalidateRect( hwnd, NULL, FALSE )
            end if
        end if
        CAL_CallMessage( pCal, hwnd, uMsg, wParam, lParam )
        return 0

    case WM_DESTROY
        if pCal then
            CAL_StopHotTimer( pCal )
            PsTipHost_Destroy( @pCal->tip )
        end if

    case WM_NCDESTROY
        if pCal then delete(pCal)
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' Create
' ========================================================================================
function PsCalendar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' THE EXTENDED STYLE IS PASSED EXPLICITLY, AND IT MUST BE. CWindow.Create defaults its
    ' dwExStyle parameter to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a control that
    ' supplies only dwStyle silently declares itself a CONTAINER. The dialog manager then
    ' descends into it hunting for tabstops, finds no children, and SKIPS IT -- the control
    ' becomes unreachable by Tab. That shipped in PsButton's first commit and was then found
    ' in PsToggle and PsComboBox too. It is ASSERTED in the self-test, not merely remembered.
    '
    ' PsCalendar is a LEAF: one HWND, no children. Zero is correct here. Do NOT copy this line
    ' into a wrap-a-real-child control -- PsListBox, PsTextBox and PsNumericUpDown all NEED
    ' WS_EX_CONTROLPARENT, because their tabstop lives on an inner child.
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsCalendar_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )
    if hCtrl = 0 then
        delete pWindow
        return 0
    end if

    ' Required: a double-click is one of the two routes to DateActivated (see the WndProc).
    pWindow->ClassStyle = CS_DBLCLKS

    dim as PSCALENDAR ptr pCal = new PSCALENDAR
    pCal->hWin         = hCtrl
    pCal->idc_Calendar = CtrlID

    ' DPI-scale the layout metrics ONCE, here (the family rule). Every setter afterwards takes
    ' raw pixels and the caller scales.
    '
    ' nBorderThick / nChevThick / nFocusThick are deliberately ABSENT from this list:
    ' PsBufferPaint.PaintLine and PaintRoundOutline scale the pen width they are handed, so
    ' pre-scaling here would apply the factor twice. No assertion running at 100% DPI can see
    ' that mistake -- it is the reason the interactive pass is run at 175%.
    pCal->nPad       = pWindow->ScaleX( CCAL_PAD )
    pCal->nNavBandH  = pWindow->ScaleY( CCAL_NAVBANDH )
    pCal->nNavGap    = pWindow->ScaleY( CCAL_NAVGAP )
    pCal->nChevW     = pWindow->ScaleX( CCAL_CHEVW )
    pCal->nGutterX   = pWindow->ScaleX( CCAL_GUTTERX )
    pCal->nGutterY   = pWindow->ScaleY( CCAL_GUTTERY )
    pCal->nCaptionH  = pWindow->ScaleY( CCAL_CAPTIONH )
    pCal->nWeekdayH  = pWindow->ScaleY( CCAL_WEEKDAYH )
    pCal->nCellMinW  = pWindow->ScaleX( CCAL_CELLMINW )
    pCal->nCellMinH  = pWindow->ScaleY( CCAL_CELLMINH )

    GetLocalTime( @pCal->stToday )
    pCal->stAnchor = PsCalendar_MakeDate( pCal->stToday.wYear, pCal->stToday.wMonth, 1 )

    pWindow->UserData(0) = cast(LONG_PTR, pCal)
    return hCtrl
end function


' ========================================================================================
' PUBLIC API
'
' Contract, once, for the whole surface below:
'   * Every SETTER is SILENT. Only user interaction fires SelChange / DateActivated. The two
'     exceptions are named "action" rather than "set" -- PsCalendar_SelectDate and
'     PsCalendar_Activate -- and they say so on the tin.
'   * A setter that changes geometry marks the layout dirty and invalidates; the next paint
'     does the one measuring pass. There is no BeginUpdate / EndUpdate.
'   * Every accessor is null-safe and returns a benign zero / FALSE for a bad handle.
' ========================================================================================

' --- value ------------------------------------------------------------------------------
function PsCalendar_GetSelDate( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return false
    if pCal->hasSelection = false then return false
    st = pCal->stSelected
    return true
end function

' SILENT. Brings the date's month into view, and is ignored when out of range -- a selection
' the control would refuse to let the user reach must not be reachable through the setter.
sub PsCalendar_SetSelDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if CAL_InRange( pCal, st ) = false then exit sub
    pCal->stSelected   = st
    pCal->hasSelection = true
    CAL_EnsureMonthVisible( pCal, st )
    CAL_Relayout( pCal )
end sub

sub PsCalendar_ClearSelDate( byval hCtrl as HWND )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->hasSelection = false
    CAL_Relayout( pCal )
end sub

function PsCalendar_HasSelDate( byval hCtrl as HWND ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return false
    return pCal->hasSelection
end function

sub PsCalendar_GetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byref bHasMin as boolean, byref bHasMax as boolean )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    bHasMin = false : bHasMax = false
    if pCal = 0 then exit sub
    stMin = pCal->stMin : stMax = pCal->stMax
    bHasMin = pCal->hasMin : bHasMax = pCal->hasMax
end sub

' An inverted pair is SWAPPED rather than rejected, and an existing selection outside the new
' range is dropped -- leaving it would put the control in a state its own hit test refuses.
sub PsCalendar_SetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byval bHasMin as boolean, byval bHasMax as boolean )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    dim as SYSTEMTIME a = stMin, b = stMax
    if bHasMin andalso bHasMax then
        if CAL_Serial(a) > CAL_Serial(b) then
            dim as SYSTEMTIME t = a : a = b : b = t
        end if
    end if
    pCal->stMin  = a : pCal->hasMin = bHasMin
    pCal->stMax  = b : pCal->hasMax = bHasMax
    if pCal->hasSelection then
        if CAL_InRange( pCal, pCal->stSelected ) = false then pCal->hasSelection = false
    end if
    CAL_Relayout( pCal )
end sub

function PsCalendar_GetToday( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return false
    st = pCal->stToday
    return true
end function

' --- displayed page -----------------------------------------------------------------------
function PsCalendar_GetAnchorDate( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return false
    st = pCal->stAnchor
    return true
end function

sub PsCalendar_SetAnchorDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->stAnchor = CAL_AddMonthsClamped( PsCalendar_MakeDate( st.wYear, st.wMonth, 1 ), 0 )
    CAL_Relayout( pCal )
end sub

function PsCalendar_GetView( byval hCtrl as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return CAL_VIEW_DAYS
    return pCal->eView
end function

sub PsCalendar_SetView( byval hCtrl as HWND, byval eView as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if (eView < CAL_VIEW_DAYS) orelse (eView > CAL_VIEW_YEARS) then exit sub
    pCal->eView = eView
    select case eView
    case CAL_VIEW_MONTHS : pCal->nMyFocus = pCal->stAnchor.wMonth - 1
    case CAL_VIEW_YEARS  : pCal->nMyFocus = pCal->stAnchor.wYear - PsCalendar_DecadeBase( pCal->stAnchor.wYear ) + 1
    end select
    CAL_Relayout( pCal )
end sub

function PsCalendar_GetTitleText( byval hCtrl as HWND ) as DWSTRING
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return ""
    if pCal->bLayoutDirty then pCal->LayoutCalendar()
    return CAL_TitleText( pCal )
end function

' --- behaviour ----------------------------------------------------------------------------
function PsCalendar_GetWeekStart( byval hCtrl as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return CAL_SUNDAY
    return pCal->nWeekStart
end function

sub PsCalendar_SetWeekStart( byval hCtrl as HWND, byval nWeekStart as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    dim as long w = CAL_SUNDAY
    if nWeekStart = CAL_MONDAY then w = CAL_MONDAY
    if w = pCal->nWeekStart then exit sub
    pCal->nWeekStart = w
    CAL_Relayout( pCal )
end sub

sub PsCalendar_GetPanelGrid( byval hCtrl as HWND, byref nCols as long, byref nRows as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    nCols = 0 : nRows = 0
    if pCal = 0 then exit sub
    nCols = pCal->nCols : nRows = pCal->nRows
end sub

' REJECTED (and the current grid left untouched) when either dimension is < 1 or the product
' exceeds CCAL_MAX_PANELS. Silently clamping instead would hand back a grid the caller did not
' ask for and could not detect.
sub PsCalendar_SetPanelGrid( byval hCtrl as HWND, byval nCols as long, byval nRows as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if (nCols < 1) orelse (nRows < 1) then exit sub
    if nCols * nRows > CCAL_MAX_PANELS then exit sub
    dim as boolean bSame = (nCols = pCal->nCols) andalso (nRows = pCal->nRows)
    if bSame then exit sub
    pCal->nCols = nCols
    pCal->nRows = nRows
    CAL_Relayout( pCal )
end sub

function PsCalendar_GetPanelCount( byval hCtrl as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return pCal->PanelCount()
end function

function PsCalendar_GetNavStep( byval hCtrl as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return pCal->nNavStep
end function

sub PsCalendar_SetNavStep( byval hCtrl as HWND, byval nStep as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if nStep < 0 then nStep = 0
    pCal->nNavStep = nStep
    ' No layout consequence -- only what the chevrons do next, and whether they paint disabled.
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

function PsCalendar_GetShowTodayRing( byval hCtrl as HWND ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return false
    return pCal->bShowTodayRing
end function

sub PsCalendar_SetShowTodayRing( byval hCtrl as HWND, byval bShow as boolean )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if bShow = pCal->bShowTodayRing then exit sub
    pCal->bShowTodayRing = bShow
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

' --- localization -------------------------------------------------------------------------
function PsCalendar_GetMonthName( byval hCtrl as HWND, byval m as long ) as DWSTRING
    return CAL_MonthName( GetCalendarPointer(hCtrl), m )
end function

sub PsCalendar_SetMonthName( byval hCtrl as HWND, byval m as long, byval sName as DWSTRING )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if (m < 1) orelse (m > 12) then exit sub
    pCal->wszMonthName(m) = sName
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

function PsCalendar_GetMonthAbbrev( byval hCtrl as HWND, byval m as long ) as DWSTRING
    return CAL_MonthAbbrev( GetCalendarPointer(hCtrl), m )
end function

sub PsCalendar_SetMonthAbbrev( byval hCtrl as HWND, byval m as long, byval sName as DWSTRING )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if (m < 1) orelse (m > 12) then exit sub
    pCal->wszMonthAbbrev(m) = sName
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

function PsCalendar_GetWeekdayAbbrev( byval hCtrl as HWND, byval i as long ) as DWSTRING
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if (i < 0) orelse (i > 6) then return ""
    ' The public accessor is indexed by DAY OF WEEK, so it is asked and answered in the same
    ' terms the setter uses -- hence CAL_SUNDAY here rather than the control's week start.
    return CAL_WeekdayAbbrev( pCal, i, CAL_SUNDAY )
end function

sub PsCalendar_SetWeekdayAbbrev( byval hCtrl as HWND, byval i as long, byval sName as DWSTRING )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if (i < 0) orelse (i > 6) then exit sub
    pCal->wszWeekdayAbbrev(i) = sName
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

sub PsCalendar_SetMonthNames( byval hCtrl as HWND, sNames() as DWSTRING )
    dim as long lo = lbound(sNames), hi = ubound(sNames)
    for i as long = lo to hi
        dim as long m = i - lo + 1
        if m > 12 then exit for
        PsCalendar_SetMonthName( hCtrl, m, sNames(i) )
    next
end sub

sub PsCalendar_SetMonthAbbrevs( byval hCtrl as HWND, sNames() as DWSTRING )
    dim as long lo = lbound(sNames), hi = ubound(sNames)
    for i as long = lo to hi
        dim as long m = i - lo + 1
        if m > 12 then exit for
        PsCalendar_SetMonthAbbrev( hCtrl, m, sNames(i) )
    next
end sub

sub PsCalendar_SetWeekdayAbbrevs( byval hCtrl as HWND, sNames() as DWSTRING )
    dim as long lo = lbound(sNames), hi = ubound(sNames)
    for i as long = lo to hi
        dim as long d = i - lo
        if d > 6 then exit for
        PsCalendar_SetWeekdayAbbrev( hCtrl, d, sNames(i) )
    next
end sub

' --- layout / appearance ------------------------------------------------------------------
function PsCalendar_GetFont( byval hCtrl as HWND ) as HFONT
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return pCal->hMainFont
end function

' Sets all three fonts, so a host that wants one face calls this alone. The two overrides
' below then replace individual bands. Fonts are CALLER-OWNED and never deleted here.
sub PsCalendar_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->hMainFont    = hFont
    pCal->hTitleFont   = 0
    pCal->hWeekdayFont = 0
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

function PsCalendar_GetTitleFont( byval hCtrl as HWND ) as HFONT
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return CAL_TitleFontOf( pCal )
end function

sub PsCalendar_SetTitleFont( byval hCtrl as HWND, byval hFont as HFONT )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->hTitleFont = hFont
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

function PsCalendar_GetWeekdayFont( byval hCtrl as HWND ) as HFONT
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return CAL_WeekdayFontOf( pCal )
end function

sub PsCalendar_SetWeekdayFont( byval hCtrl as HWND, byval hFont as HFONT )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->hWeekdayFont = hFont
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

function PsCalendar_GetBorderThickness( byval hCtrl as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return pCal->nBorderThick
end function

' 0 = no frame, which is the DEFAULT: an embedded calendar normally sits inside the host's own
' chrome. The popup this came from always drew one because it floated.
' NOT DPI-scaled -- a hairline stays a hairline, and PaintRoundOutline scales the pen anyway.
sub PsCalendar_SetBorderThickness( byval hCtrl as HWND, byval nThickness as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    if nThickness = pCal->nBorderThick then exit sub
    pCal->nBorderThick = nThickness
    CAL_Relayout( pCal )        ' it changes rcInner, so the whole grid moves
end sub

function PsCalendar_GetChevronThickness( byval hCtrl as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return pCal->nChevThick
end function

sub PsCalendar_SetChevronThickness( byval hCtrl as HWND, byval nThickness as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if nThickness < 1 then nThickness = 1
    pCal->nChevThick = nThickness
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

function PsCalendar_GetPadding( byval hCtrl as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    return pCal->nPad
end function

sub PsCalendar_SetPadding( byval hCtrl as HWND, byval nPad as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if nPad < 0 then nPad = 0
    pCal->nPad = nPad
    CAL_Relayout( pCal )
end sub

sub PsCalendar_GetGutters( byval hCtrl as HWND, byref nGutterX as long, byref nGutterY as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    nGutterX = 0 : nGutterY = 0
    if pCal = 0 then exit sub
    nGutterX = pCal->nGutterX : nGutterY = pCal->nGutterY
end sub

sub PsCalendar_SetGutters( byval hCtrl as HWND, byval nGutterX as long, byval nGutterY as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if nGutterX < 0 then nGutterX = 0
    if nGutterY < 0 then nGutterY = 0
    pCal->nGutterX = nGutterX
    pCal->nGutterY = nGutterY
    CAL_Relayout( pCal )
end sub

sub PsCalendar_GetCellMinSize( byval hCtrl as HWND, byref nW as long, byref nH as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    nW = 0 : nH = 0
    if pCal = 0 then exit sub
    nW = pCal->nCellMinW : nH = pCal->nCellMinH
end sub

' These feed GetIdealSize ONLY. Cells stretch to whatever the control is actually sized to,
' so raising this does not enlarge an already-sized control -- it changes what "ideal" means.
sub PsCalendar_SetCellMinSize( byval hCtrl as HWND, byval nW as long, byval nH as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    if nW < 1 then nW = 1
    if nH < 1 then nH = 1
    pCal->nCellMinW = nW
    pCal->nCellMinH = nH
    CAL_Relayout( pCal )
end sub

' PURE SCALAR MATH. Takes no DC, measures no text, and is valid BEFORE the control has ever
' been sized -- which is exactly when a host that wants to size-to-fit needs to ask.
sub PsCalendar_GetIdealSize( byval hCtrl as HWND, byref nWidth as long, byref nHeight as long )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    nWidth = 0 : nHeight = 0
    if pCal = 0 then exit sub
    pCal->LayoutCalendar()
    nWidth  = pCal->nIdealW
    nHeight = pCal->nIdealH
end sub

function PsCalendar_GetEnabled( byval hCtrl as HWND ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return false
    return pCal->isEnabled
end function

' Goes through EnableWindow, so the disable is enforced by the SYSTEM (no mouse messages, no
' focus) rather than being a cosmetic flag the control has to remember to honour everywhere.
' WM_ENABLE syncs isEnabled back. PsToggle's rule.
sub PsCalendar_SetEnabled( byval hCtrl as HWND, byval isEnabled as boolean )
    if hCtrl = 0 then exit sub
    EnableWindow( hCtrl, isEnabled )
end sub

function PsCalendar_HasFocus( byval hCtrl as HWND ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return false
    return pCal->isFocused
end function

' Re-reads today's date (so a calendar left open across midnight corrects itself) and repaints.
sub PsCalendar_Refresh( byval hCtrl as HWND )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    GetLocalTime( @pCal->stToday )
    CAL_Relayout( pCal )
end sub

' --- geometry accessors -------------------------------------------------------------------
' All of these force the pending layout first. An accessor that read a rect without doing so
' would answer with the PREVIOUS geometry after any setter -- a whole class of assertion that
' passes while testing nothing.
private function CAL_Ready( byval hCtrl as HWND ) as PSCALENDAR ptr
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then return 0
    if pCal->bLayoutDirty then pCal->LayoutCalendar()
    return pCal
end function

function PsCalendar_HitTest( byval hCtrl as HWND, byval pt as POINT ) as CAL_HITRESULT
    dim as CAL_HITRESULT hr
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return hr
    return CAL_HitTestPt( pCal, pt )
end function

function PsCalendar_GetInnerRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    rc = pCal->rcInner : return true
end function

function PsCalendar_GetNavBandRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    rc = pCal->rcNavBand : return true
end function

function PsCalendar_GetPrevRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    rc = pCal->rcPrev : return true
end function

function PsCalendar_GetNextRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    rc = pCal->rcNext : return true
end function

function PsCalendar_GetTitleRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    rc = pCal->rcTitle : return true
end function

function PsCalendar_GetGridRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    rc = pCal->rcGrid : return true
end function

function PsCalendar_GetPanelRect( byval hCtrl as HWND, byval panel as long, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    if (panel < 0) orelse (panel >= pCal->PanelCount()) then return false
    rc = pCal->rcPanel(panel) : return true
end function

function PsCalendar_GetCaptionRect( byval hCtrl as HWND, byval panel as long, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    if (panel < 0) orelse (panel >= pCal->PanelCount()) then return false
    rc = pCal->rcCaption(panel) : return true
end function

function PsCalendar_GetWeekdayRect( byval hCtrl as HWND, byval panel as long, byval i as long, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    if (panel < 0) orelse (panel >= pCal->PanelCount()) then return false
    if (i < 0) orelse (i > 6) then return false
    rc = pCal->rcWeekday(panel, i) : return true
end function

function PsCalendar_GetDayCellRect( byval hCtrl as HWND, byval panel as long, byval k as long, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    if (panel < 0) orelse (panel >= pCal->PanelCount()) then return false
    if (k < 0) orelse (k > 41) then return false
    rc = pCal->rcDayCell(panel, k) : return true
end function

function PsCalendar_GetMyCellRect( byval hCtrl as HWND, byval k as long, byref rc as RECT ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    if (k < 0) orelse (k > 11) then return false
    rc = pCal->rcMyCell(k) : return true
end function

function PsCalendar_GetCellDate( byval hCtrl as HWND, byval panel as long, byval k as long, byref st as SYSTEMTIME ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    if (panel < 0) orelse (panel >= pCal->PanelCount()) then return false
    if (k < 0) orelse (k > 41) then return false
    st = pCal->CellDate( panel, k )
    return true
end function

function PsCalendar_GetPanelMonth( byval hCtrl as HWND, byval panel as long, byref nYear as long, byref nMonth as long ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    nYear = 0 : nMonth = 0
    if pCal = 0 then return false
    if (panel < 0) orelse (panel >= pCal->PanelCount()) then return false
    nYear  = pCal->panelYear(panel)
    nMonth = pCal->panelMonth(panel)
    return true
end function

function PsCalendar_GetFocusCell( byval hCtrl as HWND, byref panel as long, byref index as long ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    panel = -1 : index = -1
    if pCal = 0 then return false
    return CAL_FocusCell( pCal, panel, index )
end function

function PsCalendar_IsPrevDisabled( byval hCtrl as HWND ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    return CAL_PrevDisabled( pCal )
end function

function PsCalendar_IsNextDisabled( byval hCtrl as HWND ) as boolean
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return false
    return CAL_NextDisabled( pCal )
end function

' --- colours ------------------------------------------------------------------------------
sub PsCalendar_GetColors( byval hCtrl as HWND, byval pColors as PSCALENDAR_COLORS ptr )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if (pCal = 0) orelse (pColors = 0) then exit sub
    *pColors = pCal->colors
end sub

sub PsCalendar_SetColors( byval hCtrl as HWND, byval pColors as PSCALENDAR_COLORS ptr )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if (pCal = 0) orelse (pColors = 0) then exit sub
    pCal->colors = *pColors
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

' --- callbacks ----------------------------------------------------------------------------
sub PsCalendar_SetSelChangeCallback( byval hCtrl as HWND, byval usersub as CAL_SelChangeCallbackSub )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal then pCal->SelChangeCallback = usersub
end sub

sub PsCalendar_SetDateActivatedCallback( byval hCtrl as HWND, byval usersub as CAL_DateActivatedCallbackSub )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal then pCal->DateActivatedCallback = usersub
end sub

sub PsCalendar_SetPaintCallback( byval hCtrl as HWND, byval usersub as CAL_PaintCallbackSub )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->PaintCallback = usersub
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

sub PsCalendar_SetDayPaintCallback( byval hCtrl as HWND, byval usersub as CAL_DayPaintCallbackSub )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->DayPaintCallback = usersub
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

sub PsCalendar_SetMonthYearPaintCallback( byval hCtrl as HWND, byval usersub as CAL_MonthYearPaintCallbackSub )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    pCal->MonthYearPaintCallback = usersub
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, FALSE )
end sub

sub PsCalendar_SetMessageCallback( byval hCtrl as HWND, byval userfunc as CAL_MessageCallbackFunc )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal then pCal->MessageCallback = userfunc
end sub


' ========================================================================================
' Tooltip backend.  See PsTipHost.bi -- the tip here is a TRACKED one, shown by the control.
' ========================================================================================
function PsCalendar_GetTooltipHandle( byval hCalendar as HWND ) as HWND
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCalendar)
    if pCal = 0 then return 0
    return PsTipHost_SysHandle( @pCal->tip )
end function

function PsCalendar_GetPsTooltipHandle( byval hCalendar as HWND ) as HWND
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCalendar)
    if pCal = 0 then return 0
    return PsTipHost_PsHandle( @pCal->tip )
end function

function PsCalendar_SetTooltipMode( byval hCalendar as HWND, byval nMode as long ) as boolean
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCalendar)
    if pCal = 0 then return false
    ' The tip is built lazily, on the first cell that wants one, so a mode set before then
    ' simply decides which kind gets built. TRACKED is re-declared by CAL_TipEnsure either way.
    PsTipHost_SetTracked( @pCal->tip, true )
    return PsTipHost_SetMode( @pCal->tip, nMode )
end function

function PsCalendar_GetTooltipMode( byval hCalendar as HWND ) as long
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCalendar)
    if pCal = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pCal->tip )
end function

sub PsCalendar_SetTooltipCallback( byval hCtrl as HWND, byval userfunc as CAL_TooltipCallbackFunc )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal then pCal->TooltipCallback = userfunc
end sub

' --- actions (these FIRE; they are actions, not setters) ----------------------------------
sub PsCalendar_SelectDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then exit sub
    if CAL_InRange( pCal, st ) = false then exit sub
    CAL_SelectUser( pCal, st )
end sub

sub PsCalendar_Activate( byval hCtrl as HWND )
    dim as PSCALENDAR ptr pCal = GetCalendarPointer(hCtrl)
    if pCal = 0 then exit sub
    CAL_NotifyActivated( pCal )
end sub


' ========================================================================================
' Offscreen render probes
'
' Public API rather than test scaffolding: a host that supplies its own PaintCallback should
' be able to assert that it is not flooding the control. All three drive CAL_RenderFull --
' the identical composition WM_PAINT produces -- into an offscreen buffer.
'
' A NOTE ON THRESHOLDS, because this has cost the family twice: do NOT copy a tone floor from
' a sibling. The number a healthy render produces depends entirely on what else falls inside
' the part rect, and PsButton's "> 1" has now passed on a completely destroyed render in two
' different controls. Measure the healthy value and the flooded value in YOUR part, and put
' the floor between them.
' ========================================================================================
private function CAL_PartRect( byval pCal as PSCALENDAR ptr, byval nPart as long, byref rc as RECT ) as boolean
    if pCal = 0 then return false
    if nPart >= CAL_PART_DAYCELL_BASE then
        dim as long v = nPart - CAL_PART_DAYCELL_BASE
        dim as long p = v \ 100
        dim as long k = v mod 100
        if (p < 0) orelse (p >= pCal->PanelCount()) then return false
        if (k < 0) orelse (k > 41) then return false
        rc = pCal->rcDayCell(p, k)
        return true
    end if
    if nPart >= CAL_PART_MYCELL_BASE then
        dim as long k = nPart - CAL_PART_MYCELL_BASE
        if (k < 0) orelse (k > 11) then return false
        rc = pCal->rcMyCell(k)
        return true
    end if
    select case nPart
    case CAL_PART_CLIENT  : rc = pCal->rcClient  : return true
    case CAL_PART_NAVBAND : rc = pCal->rcNavBand : return true
    case CAL_PART_PREV    : rc = pCal->rcPrev    : return true
    case CAL_PART_NEXT    : rc = pCal->rcNext    : return true
    case CAL_PART_TITLE   : rc = pCal->rcTitle   : return true
    case CAL_PART_GRID    : rc = pCal->rcGrid    : return true
    end select
    return false
end function

' Distinct colours inside a part, capped at 256. A flooded render reports 1.
function PsCalendar_TestPartTones( byval hCtrl as HWND, byval nPart as long ) as long
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return 0
    dim as RECT rcPart
    if CAL_PartRect( pCal, nPart, rcPart ) = false then return 0
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return 0

    dim as HDC hdc = GetDC( hCtrl )
    if hdc = 0 then return 0
    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCtrl, hdc, pCal->rcClient )
    CAL_RenderFull( pCal, @b )

    dim as HDC mem = b.getMemDC()
    dim as long nSeen = 0
    dim as COLORREF seen(0 to 255)
    if mem then
        for y as long = rcPart.top to rcPart.bottom - 1 step 2
            for x as long = rcPart.left to rcPart.right - 1 step 2
                dim as COLORREF c = GetPixel( mem, x, y )
                dim as boolean bNew = true
                for i as long = 0 to nSeen - 1
                    if seen(i) = c then bNew = false : exit for
                next
                if bNew then
                    if nSeen < 256 then
                        seen(nSeen) = c
                        nSeen += 1
                    end if
                end if
            next
        next
    end if

    b.EndDoubleBuffer()
    ReleaseDC( hCtrl, hdc )
    return nSeen
end function

' Brightest pixel in a part, 0..255. Used to compare two colours that must stay clearly
' apart -- as a RATIO, never as a difference.
function PsCalendar_TestPartBrightness( byval hCtrl as HWND, byval nPart as long ) as long
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return 0
    dim as RECT rcPart
    if CAL_PartRect( pCal, nPart, rcPart ) = false then return 0
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return 0

    dim as HDC hdc = GetDC( hCtrl )
    if hdc = 0 then return 0
    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCtrl, hdc, pCal->rcClient )
    CAL_RenderFull( pCal, @b )

    dim as HDC mem = b.getMemDC()
    dim as long nMax = 0
    if mem then
        for y as long = rcPart.top to rcPart.bottom - 1
            for x as long = rcPart.left to rcPart.right - 1
                dim as COLORREF c = GetPixel( mem, x, y )
                dim as long lum = (cast(long, c and &hFF) + cast(long, (c shr 8) and &hFF) + cast(long, (c shr 16) and &hFF)) \ 3
                if lum > nMax then nMax = lum
            next
        next
    end if

    b.EndDoubleBuffer()
    ReleaseDC( hCtrl, hdc )
    return nMax
end function

' FNV-1a over a part's pixels. For asserting that a STATE change actually reached the pixels.
'
' READ THIS BEFORE TRUSTING ONE: a hash assertion measures whatever varies, not what its name
' says. PsCheckBox's first version survived an A/B that froze glyph resolution outright,
' because the state being tested also changed the box COLOUR. Isolate the variable -- flatten
' everything else the state touches -- or the assertion is a tautology with a good name.
function PsCalendar_TestHashPart( byval hCtrl as HWND, byval nPart as long ) as ulongint
    dim as PSCALENDAR ptr pCal = CAL_Ready(hCtrl)
    if pCal = 0 then return 0
    dim as RECT rcPart
    if CAL_PartRect( pCal, nPart, rcPart ) = false then return 0
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return 0

    dim as HDC hdc = GetDC( hCtrl )
    if hdc = 0 then return 0
    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCtrl, hdc, pCal->rcClient )
    CAL_RenderFull( pCal, @b )

    dim as HDC mem = b.getMemDC()
    dim as ulongint h = 14695981039346656037ull
    if mem then
        for y as long = rcPart.top to rcPart.bottom - 1
            for x as long = rcPart.left to rcPart.right - 1
                dim as ulongint c = cast(ulongint, GetPixel( mem, x, y ))
                for byteIdx as long = 0 to 3
                    h = h xor ((c shr (byteIdx * 8)) and &hFF)
                    h = h * 1099511628211ull
                next
            next
        next
    end if

    b.EndDoubleBuffer()
    ReleaseDC( hCtrl, hdc )
    return h
end function
