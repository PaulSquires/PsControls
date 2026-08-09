'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsDatePicker.inc  --  owner-drawn date picker: an editable date field + a popup calendar
''

#include once "PsDatePicker.bi"
' The tooltip backend switch, pulled in here so a host needs no extra include line.
#include once "PsTipHost.inc"


' Only one calendar can be open at a time (it is a floating popup). This module-global names
' the picker whose calendar is currently dropped down, so the (parameterless) FilterMessage
' can find it -- the same single-open-chain invariant PsPopupMenu keeps with gPM_OpenRoot.
dim shared gDTP_OpenPicker as HWND = 0

dim shared gDTP_Trace as long = -1
private sub DTP_Log( byref s as string )
    if gDTP_Trace = -1 then
        if environ("PSDATEPICKER_TRACE") = "1" then gDTP_Trace = 1 else gDTP_Trace = 0
    end if
    if gDTP_Trace = 1 then
        print "[DTP] " & s
    end if
end sub


' ========================================================================================
' Accessors
' ========================================================================================
function GetDatePickerPointer( byval hCtrl as HWND ) as PSDATEPICKER ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hCtrl)
    if pWindow = 0 then return 0
    return cast( PSDATEPICKER ptr, pWindow->UserData(0) )
end function

function GetDatePickerCalPointer( byval hCal as HWND ) as PSDATEPICKER_CAL ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hCal)
    if pWindow = 0 then return 0
    return cast( PSDATEPICKER_CAL ptr, pWindow->UserData(0) )
end function


' Forward declarations.
declare function PsDatePicker_WndProc( byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
declare function DTP_CalWndProc( byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
declare function DTP_EnsureCalendar( byval hCtrl as HWND ) as HWND
declare sub      DTP_SeedCalendar( byval pCtrl as PSDATEPICKER ptr )
declare sub      DTP_FormatIntoField( byval pCtrl as PSDATEPICKER ptr )


' ========================================================================================
' PURE DATE MATH  (proleptic Gregorian; serial = days since 1970-01-01, Hinnant's algorithm)
' Public so a host and the self-test can reuse them; each is a pure function of its inputs.
' ========================================================================================
function PsDatePicker_IsLeapYear( byval y as long ) as boolean
    dim as boolean b1 = ((y mod 4) = 0) andalso ((y mod 100) <> 0)
    dim as boolean b2 = ((y mod 400) = 0)
    return (b1 orelse b2)
end function

function PsDatePicker_DaysInMonth( byval y as long, byval m as long ) as long
    select case m
    case 1, 3, 5, 7, 8, 10, 12 : return 31
    case 4, 6, 9, 11           : return 30
    case 2
        if PsDatePicker_IsLeapYear(y) then return 29 else return 28
    case else                  : return 30
    end select
end function

function PsDatePicker_DateToSerial( byval y as long, byval m as long, byval d as long ) as longint
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

sub PsDatePicker_SerialToDate( byval serial as longint, byref y as long, byref m as long, byref d as long )
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
function PsDatePicker_DowSunday( byval y as long, byval m as long, byval d as long ) as long
    dim as longint z = PsDatePicker_DateToSerial(y, m, d)
    dim as long w = cast(long, ((z mod 7) + 7) mod 7)        ' 0 = Thursday here
    return (w + 4) mod 7                                      ' shift Thursday(0) -> 4, Sunday -> 0
end function

' Column of the 1st of the month, given the week start.
function PsDatePicker_LeadingCells( byval y as long, byval m as long, byval nWeekStart as long ) as long
    dim as long dow1 = PsDatePicker_DowSunday(y, m, 1)
    if nWeekStart = DTP_MONDAY then return (dow1 + 6) mod 7
    return dow1
end function

function PsDatePicker_MakeDate( byval y as long, byval m as long, byval d as long ) as SYSTEMTIME
    dim as SYSTEMTIME st
    st.wYear      = y
    st.wMonth     = m
    st.wDay       = d
    st.wDayOfWeek = PsDatePicker_DowSunday(y, m, d)
    return st
end function

function PsDatePicker_AddMonths( byref st as SYSTEMTIME, byval delta as long ) as SYSTEMTIME
    dim as long total = st.wYear * 12 + (st.wMonth - 1) + delta
    dim as long y = total \ 12
    dim as long m = total mod 12
    if m < 0 then
        m += 12
        y -= 1
    end if
    m += 1
    dim as long d = st.wDay
    dim as long dmax = PsDatePicker_DaysInMonth(y, m)
    if d > dmax then d = dmax
    return PsDatePicker_MakeDate(y, m, d)
end function

function PsDatePicker_DecadeBase( byval y as long ) as long
    return (y \ 10) * 10
end function

' Serial of a SYSTEMTIME's date portion.
private function DTP_Serial( byref st as SYSTEMTIME ) as longint
    return PsDatePicker_DateToSerial( st.wYear, st.wMonth, st.wDay )
end function

private function DTP_SameYMD( byref a as SYSTEMTIME, byref b as SYSTEMTIME ) as boolean
    dim as boolean b1 = (a.wYear = b.wYear) andalso (a.wMonth = b.wMonth)
    return (b1 andalso (a.wDay = b.wDay))
end function


' ========================================================================================
' CALENDAR TYPE METHODS
' ========================================================================================
function PSDATEPICKER_CAL.CellDate( byval panel as long, byval k as long ) as SYSTEMTIME
    if panel < 0 then panel = 0
    if panel > 1 then panel = 1
    dim as longint s0 = PsDatePicker_DateToSerial( this.panelYear(panel), this.panelMonth(panel), 1 ) - this.panelLead(panel)
    dim as long yy, mm, dd
    PsDatePicker_SerialToDate( s0 + k, yy, mm, dd )
    return PsDatePicker_MakeDate( yy, mm, dd )
end function

' The one geometry pass. All rects derived, no DC taken. Ideal size is pure scalar math and
' is computed FIRST so DropDown can size the popup even before it has a client rect.
sub PSDATEPICKER_CAL.LayoutCalendar()
    this.bLayoutDirty = false

    dim as long mc = this.nMonthCount
    if mc < 1 then mc = 1
    if mc > 2 then mc = 2

    dim as long panelIdealW = 7 * this.nCellMinW
    dim as long panelIdealH = this.nHeaderH + this.nWeekdayH + 6 * this.nCellMinH
    this.nIdealW = 2 * this.nPad + mc * panelIdealW + (mc - 1) * this.nGutter
    this.nIdealH = 2 * this.nPad + panelIdealH
    if this.bShowTodayFooter then this.nIdealH += this.nFooterH

    ' per-panel date origins (independent of the client rect)
    dim as SYSTEMTIME stP0 = PsDatePicker_MakeDate( this.stAnchor.wYear, this.stAnchor.wMonth, 1 )
    this.panelYear(0)  = stP0.wYear
    this.panelMonth(0) = stP0.wMonth
    this.panelLead(0)  = PsDatePicker_LeadingCells( stP0.wYear, stP0.wMonth, this.nWeekStart )
    dim as SYSTEMTIME stP1 = PsDatePicker_AddMonths( stP0, 1 )
    this.panelYear(1)  = stP1.wYear
    this.panelMonth(1) = stP1.wMonth
    this.panelLead(1)  = PsDatePicker_LeadingCells( stP1.wYear, stP1.wMonth, this.nWeekStart )

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

    dim as long innerL = this.nPad
    dim as long innerT = this.nPad
    dim as long innerR = W - this.nPad
    dim as long innerB = H - this.nPad

    ' The "Today" footer is carved off the BOTTOM FIRST, so the panels below shrink to fit
    ' rather than being overdrawn by it. It spans the full inner width (it is one band for the
    ' whole calendar, not per-panel) and it is present in EVERY view -- escaping a deep
    ' drill-down is the whole reason it exists.
    SetRect( @this.rcTodayFooter, 0, 0, 0, 0 )
    if this.bShowTodayFooter then
        SetRect( @this.rcTodayFooter, innerL, innerB - this.nFooterH, innerR, innerB )
        innerB -= this.nFooterH
    end if

    dim as long innerW = innerR - innerL

    dim as long panelW = (innerW - (mc - 1) * this.nGutter) \ mc
    dim as long nDrawn = 1
    if this.eView = DTP_VIEW_DAYS then nDrawn = mc

    for p as long = 0 to 1
        dim as long L = innerL + p * (panelW + this.nGutter)
        SetRect( @this.rcPanel(p), L, innerT, L + panelW, innerB )
    next

    ' headers (each drawn panel); nav + interactive title on panel 0 only
    for p as long = 0 to nDrawn - 1
        SetRect( @this.rcHeader(p), this.rcPanel(p).left, this.rcPanel(p).top, _
                 this.rcPanel(p).right, this.rcPanel(p).top + this.nHeaderH )
        if p = 0 then
            SetRect( @this.rcPrev, this.rcHeader(0).left, this.rcHeader(0).top, _
                     this.rcHeader(0).left + this.nChevW, this.rcHeader(0).bottom )
            SetRect( @this.rcNext, this.rcHeader(0).right - this.nChevW, this.rcHeader(0).top, _
                     this.rcHeader(0).right, this.rcHeader(0).bottom )
            SetRect( @this.rcTitle(0), this.rcHeader(0).left + this.nChevW, this.rcHeader(0).top, _
                     this.rcHeader(0).right - this.nChevW, this.rcHeader(0).bottom )
        else
            this.rcTitle(1) = this.rcHeader(1)      ' static caption, no chevrons
        end if
    next

    if this.eView = DTP_VIEW_DAYS then
        for p as long = 0 to nDrawn - 1
            dim as long bodyTop = this.rcHeader(p).bottom
            dim as RECT weekBand
            SetRect( @weekBand, this.rcPanel(p).left, bodyTop, this.rcPanel(p).right, bodyTop + this.nWeekdayH )
            SetRect( @this.rcBody(p), this.rcPanel(p).left, weekBand.bottom, this.rcPanel(p).right, this.rcPanel(p).bottom )

            dim as long cw = (this.rcPanel(p).right - this.rcPanel(p).left) \ 7   ' floor; leftover = right margin
            dim as long rh = (this.rcBody(p).bottom - this.rcBody(p).top) \ 6     ' floor; leftover = bottom margin
            this.cellW(p) = cw
            this.rowH(p)  = rh

            for i as long = 0 to 6
                dim as long x = this.rcPanel(p).left + i * cw
                SetRect( @this.rcWeekday(p, i), x, weekBand.top, x + cw, weekBand.bottom )
            next
            for k as long = 0 to 41
                dim as long col = k mod 7
                dim as long row = k \ 7
                dim as long x = this.rcBody(p).left + col * cw
                dim as long yy = this.rcBody(p).top + row * rh
                SetRect( @this.rcDayCell(p, k), x, yy, x + cw, yy + rh )
            next
        next
    else
        ' MONTHS / YEARS: a 3x4 grid in panel 0's body region. The right half (two-month mode)
        ' is left as blank chrome while drilled down.
        SetRect( @this.rcMyGrid, this.rcPanel(0).left, this.rcHeader(0).bottom, _
                 this.rcPanel(0).right, this.rcPanel(0).bottom )
        dim as long cw = (this.rcMyGrid.right - this.rcMyGrid.left) \ CDTP_MY_COLS
        dim as long rh = (this.rcMyGrid.bottom - this.rcMyGrid.top) \ CDTP_MY_ROWS
        for k as long = 0 to 11
            dim as long col = k mod CDTP_MY_COLS
            dim as long row = k \ CDTP_MY_COLS
            dim as long x = this.rcMyGrid.left + col * cw
            dim as long yy = this.rcMyGrid.top + row * rh
            dim as RECT rc
            SetRect( @rc, x, yy, x + cw, yy + rh )
            this.rcMonthCell(k) = rc
            this.rcYearCell(k)  = rc
        next
    end if
end sub


' ========================================================================================
' CONTAINER TYPE METHODS
' ========================================================================================
sub PSDATEPICKER.LayoutControl()
    this.bLayoutDirty = false
    if this.hWin = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    this.rcFrame = rcClient
    dim as long L = rcClient.left
    dim as long T = rcClient.top
    dim as long R = rcClient.right
    dim as long B = rcClient.bottom

    SetRect( @this.rcIcon, R - this.nIconWidth, T, R, B )
    SetRect( @this.rcDiv, this.rcIcon.left - this.nDividerThick, T, this.rcIcon.left, B )

    ' The field is inset LEFT by the corner radius (so the rounded left corners of the frame
    ' fill survive under the child's square rectangle -- the child paints the same colour, so
    ' the strip reads as one) and inset TOP/BOTTOM by the border (the child covers those rows).
    dim as long bt = this.nBorderThick
    dim as long fieldL = L + this.nCornerRadius
    dim as long fieldR = this.rcDiv.left
    if fieldR < fieldL then fieldR = fieldL
    dim as long fieldT = T + bt
    dim as long fieldB = B - bt
    if fieldB < fieldT then fieldB = fieldT
    SetRect( @this.rcField, fieldL, fieldT, fieldR, fieldB )
end sub

sub PSDATEPICKER.Refresh()
    this.bLayoutDirty = true
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' Small shared helpers
' ========================================================================================
private function DTP_ActiveIconFont( byval pCtrl as PSDATEPICKER ptr ) as HFONT
    if pCtrl = 0 then return 0
    if pCtrl->hIconFont <> 0 then return pCtrl->hIconFont
    return pCtrl->hIconFontDefault
end function

private function DTP_MonthName( byval m as long ) as DWSTRING
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

private function DTP_MonthAbbrev( byval m as long ) as DWSTRING
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

' Two-letter weekday header for column i, given the week start. i is a COLUMN index.
private function DTP_WeekdayAbbrev( byval i as long, byval nWeekStart as long ) as DWSTRING
    dim as long dow = i                       ' Sunday-start: column 0 = Sunday
    if nWeekStart = DTP_MONDAY then dow = (i + 1) mod 7
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

private function DTP_TitleText( byval pCal as PSDATEPICKER_CAL ptr, byval panel as long ) as DWSTRING
    if pCal = 0 then return ""
    select case pCal->eView
    case DTP_VIEW_MONTHS
        return str( pCal->stAnchor.wYear )
    case DTP_VIEW_YEARS
        dim as long nBase = PsDatePicker_DecadeBase( pCal->stAnchor.wYear )
        return str(nBase) & " - " & str(nBase + 9)
    case else   ' DAYS
        return DTP_MonthName( pCal->panelMonth(panel) ) & " " & str( pCal->panelYear(panel) )
    end select
end function

private function DTP_InRange( byval pCal as PSDATEPICKER_CAL ptr, byref st as SYSTEMTIME ) as boolean
    if pCal = 0 then return true
    dim as longint s = DTP_Serial(st)
    if pCal->hasMin then
        if s < DTP_Serial(pCal->stMin) then return false
    end if
    if pCal->hasMax then
        if s > DTP_Serial(pCal->stMax) then return false
    end if
    return true
end function


' ========================================================================================
' Chevron enable/disable  (whole revealed page out of range)
' ========================================================================================
private function DTP_PrevDisabled( byval pCal as PSDATEPICKER_CAL ptr ) as boolean
    if pCal = 0 then return false
    if pCal->hasMin = false then return false
    dim as longint minS = DTP_Serial( pCal->stMin )
    select case pCal->eView
    case DTP_VIEW_DAYS
        dim as SYSTEMTIME stPrev = PsDatePicker_AddMonths( pCal->stAnchor, -1 )
        dim as long lastD = PsDatePicker_DaysInMonth( stPrev.wYear, stPrev.wMonth )
        return PsDatePicker_DateToSerial( stPrev.wYear, stPrev.wMonth, lastD ) < minS
    case DTP_VIEW_MONTHS
        return PsDatePicker_DateToSerial( pCal->stAnchor.wYear - 1, 12, 31 ) < minS
    case DTP_VIEW_YEARS
        dim as long nBase = PsDatePicker_DecadeBase( pCal->stAnchor.wYear )
        return PsDatePicker_DateToSerial( nBase - 1, 12, 31 ) < minS
    end select
    return false
end function

private function DTP_NextDisabled( byval pCal as PSDATEPICKER_CAL ptr ) as boolean
    if pCal = 0 then return false
    if pCal->hasMax = false then return false
    dim as longint maxS = DTP_Serial( pCal->stMax )
    dim as long mc = pCal->nMonthCount
    if mc < 1 then mc = 1
    if mc > 2 then mc = 2
    select case pCal->eView
    case DTP_VIEW_DAYS
        ' the first month newly revealed by "next" is anchor + monthCount
        dim as SYSTEMTIME stNext = PsDatePicker_AddMonths( pCal->stAnchor, mc )
        return PsDatePicker_DateToSerial( stNext.wYear, stNext.wMonth, 1 ) > maxS
    case DTP_VIEW_MONTHS
        return PsDatePicker_DateToSerial( pCal->stAnchor.wYear + 1, 1, 1 ) > maxS
    case DTP_VIEW_YEARS
        dim as long nBase = PsDatePicker_DecadeBase( pCal->stAnchor.wYear )
        return PsDatePicker_DateToSerial( nBase + 10, 1, 1 ) > maxS
    end select
    return false
end function


' ========================================================================================
' Calendar hit-testing  (identity only; the click handler applies range/disable)
' ========================================================================================
private function DTP_CalHitTest( byval pCal as PSDATEPICKER_CAL ptr, byval pt as POINT ) as DTP_HITRESULT
    dim as DTP_HITRESULT hr
    if pCal = 0 then return hr
    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    if pCal->bShowTodayFooter then
        if PtInRect( @pCal->rcTodayFooter, pt ) then hr.part = DTP_HIT_TODAY : return hr
    end if
    if PtInRect( @pCal->rcPrev, pt ) then hr.part = DTP_HIT_PREV : return hr
    if PtInRect( @pCal->rcNext, pt ) then hr.part = DTP_HIT_NEXT : return hr
    if PtInRect( @pCal->rcTitle(0), pt ) then hr.part = DTP_HIT_TITLE : return hr

    select case pCal->eView
    case DTP_VIEW_DAYS
        dim as long mc = pCal->nMonthCount
        if mc < 1 then mc = 1
        if mc > 2 then mc = 2
        for p as long = 0 to mc - 1
            if PtInRect( @pCal->rcBody(p), pt ) then
                dim as long cw = pCal->cellW(p)
                dim as long rh = pCal->rowH(p)
                if (cw > 0) andalso (rh > 0) then
                    dim as long col = (pt.x - pCal->rcBody(p).left) \ cw
                    dim as long row = (pt.y - pCal->rcBody(p).top) \ rh
                    if (col >= 0) andalso (col < 7) andalso (row >= 0) andalso (row < 6) then
                        hr.part  = DTP_HIT_DAYCELL
                        hr.panel = p
                        hr.index = row * 7 + col
                        return hr
                    end if
                end if
            end if
        next
    case DTP_VIEW_MONTHS, DTP_VIEW_YEARS
        if PtInRect( @pCal->rcMyGrid, pt ) then
            dim as long cw = (pCal->rcMyGrid.right - pCal->rcMyGrid.left) \ CDTP_MY_COLS
            dim as long rh = (pCal->rcMyGrid.bottom - pCal->rcMyGrid.top) \ CDTP_MY_ROWS
            if (cw > 0) andalso (rh > 0) then
                dim as long col = (pt.x - pCal->rcMyGrid.left) \ cw
                dim as long row = (pt.y - pCal->rcMyGrid.top) \ rh
                if (col >= 0) andalso (col < CDTP_MY_COLS) andalso (row >= 0) andalso (row < CDTP_MY_ROWS) then
                    dim as long idx = row * CDTP_MY_COLS + col
                    if idx <= 11 then
                        if pCal->eView = DTP_VIEW_MONTHS then hr.part = DTP_HIT_MONTHCELL else hr.part = DTP_HIT_YEARCELL
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
' Tooltip (a tracking tip, only created if the owner set a TooltipCallback). Kept small and
' entirely optional: the calendar works identically with it absent.
' ========================================================================================
private sub DTP_TipEnsure( byval pCal as PSDATEPICKER_CAL ptr )
    if pCal = 0 then return
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( pCal->hOwner )
    if pCtrl = 0 then return
    if pCtrl->TooltipCallback = 0 then return
    ' TRACKED before the tool is built -- it changes the comctl32 tool's flags. The MODE is
    ' taken from the PICKER, because this popup may be a fresh one and would otherwise come
    ' up on the default backend however the host set it.
    PsTipHost_SetTracked( @pCal->tip, true )
    PsTipHost_SetMode( @pCal->tip, pCtrl->nTipMode )
    PsTipHost_Ensure( @pCal->tip, pCal->hWin )
end sub

private sub DTP_TipHide( byval pCal as PSDATEPICKER_CAL ptr )
    if pCal = 0 then return
    if pCal->tipKey = -1 then return
    PsTipHost_TrackHide( @pCal->tip )
    pCal->tipKey = -1
end sub

' Offer a tip for the day at hit hr. Shows/updates/hides the tracking tip as needed.
private sub DTP_TipUpdate( byval pCal as PSDATEPICKER_CAL ptr, byref hr as DTP_HITRESULT )
    if pCal = 0 then return
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( pCal->hOwner )
    if pCtrl = 0 then DTP_TipHide(pCal) : return
    if pCtrl->TooltipCallback = 0 then DTP_TipHide(pCal) : return
    if hr.part <> DTP_HIT_DAYCELL then DTP_TipHide(pCal) : return

    dim as SYSTEMTIME st = pCal->CellDate( hr.panel, hr.index )
    dim as longint key = DTP_Serial(st)
    if key = pCal->tipKey then return          ' already showing this day

    dim as PSDATEPICKER_TOOLTIPINFO t
    t.hCtrl   = pCal->hOwner
    t.stDate  = st
    t.isValid = true
    t.outText = ""
    dim as boolean bWant = pCtrl->TooltipCallback( @t )
    if (bWant = false) orelse (len(t.outText) = 0) then DTP_TipHide(pCal) : return

    DTP_TipEnsure(pCal)

    ' Both a POINT and the CELL RECT, in screen coordinates: comctl32 is told where to put
    ' its corner (keeping the +4/+2 nudge this control has always applied, so the system
    ' backend lands on the same pixels), while PsTooltip is given the anchor and does its own
    ' flip and work-area clamp.
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
' Calendar painting
' ========================================================================================
' A left/right chevron drawn as two strokes (crisp at any size; no glyph font needed).
private sub DTP_PaintChevron( byval b as PsBufferPaint ptr, byref rc as RECT, byval bLeft as boolean, byval clr as COLORREF, byval thick as long )
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

private sub DTP_PaintDayDefault( byval b as PsBufferPaint ptr, byval pColors as PSDATEPICKER_COLORS ptr, byval p as PSDATEPICKER_DAYPAINTINFO ptr )
    if (b = 0) orelse (pColors = 0) orelse (p = 0) then exit sub

    dim as RECT rcCell = p->rcCell
    ' a slightly inset rounded fill for hot / selected, so cells read as separate chips
    dim as RECT rcFill = rcCell
    dim as long inset = (rcCell.bottom - rcCell.top) \ 8
    if inset < 1 then inset = 1
    InflateRect( @rcFill, -inset, -inset )
    dim as long ell = (rcFill.bottom - rcFill.top) \ 3
    if ell < 2 then ell = 2

    if p->isSelected then
        b->SetBackColor( pColors->DayBackColorSelected )
        b->PaintRoundRect( @rcFill, ell )
    elseif p->isHot andalso (p->isDisabled = false) then
        b->SetBackColor( pColors->DayBackColorHot )
        b->PaintRoundRect( @rcFill, ell )
    end if

    if p->isToday andalso (p->isSelected = false) then
        b->SetPenColor( pColors->TodayRingColor )
        b->PaintRoundOutline( @rcFill, ell, 1 )
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
    b->PaintText( str(p->dayNumber), @rcCell, DT_CENTER )
end sub

' Draw the whole calendar into an ALREADY-STARTED buffer.
'
' Split out from DTP_CalPaint so the self-test can drive the REAL painter into an offscreen
' buffer and count what it produced. The per-cell probe this control shipped with could not
' see the bug that made the entire popup one flat colour, because it never ran this routine.
private sub DTP_CalRender( byval pCal as PSDATEPICKER_CAL ptr, byval pCtrl as PSDATEPICKER ptr, byval pb as PsBufferPaint ptr )
    if (pCal = 0) orelse (pCtrl = 0) orelse (pb = 0) then exit sub

    dim as PSDATEPICKER_COLORS ptr pc = @pCtrl->colors
    dim as HFONT hFont = pCtrl->hMainFont
    if hFont then pb->SetFont( hFont )

    dim as boolean bEnabled = pCtrl->isEnabled
    dim as long chevThick = 2

    dim as long mc = pCal->nMonthCount
    if mc < 1 then mc = 1
    if mc > 2 then mc = 2
    dim as long nDrawn = 1
    if pCal->eView = DTP_VIEW_DAYS then nDrawn = mc

    ' ---- header(s) ----
    dim as boolean bPrevOff = DTP_PrevDisabled(pCal)
    dim as boolean bNextOff = DTP_NextDisabled(pCal)

    for p as long = 0 to nDrawn - 1
        ' title
        dim as COLORREF clrTitle = pc->TitleColor
        if (p = 0) andalso (pCal->hotPart = DTP_HIT_TITLE) then clrTitle = pc->TitleColorHot
        pb->SetForeColor( clrTitle )
        if hFont then pb->SetFont( hFont )
        pb->PaintText( DTP_TitleText(pCal, p), @pCal->rcTitle(p), DT_CENTER )

        if p = 0 then
            dim as COLORREF clrPrev = pc->NavGlyphColor
            if bPrevOff then
                clrPrev = pc->NavGlyphColorDisabled
            elseif pCal->hotPart = DTP_HIT_PREV then
                clrPrev = pc->NavGlyphColorHot
            end if
            dim as COLORREF clrNext = pc->NavGlyphColor
            if bNextOff then
                clrNext = pc->NavGlyphColorDisabled
            elseif pCal->hotPart = DTP_HIT_NEXT then
                clrNext = pc->NavGlyphColorHot
            end if
            DTP_PaintChevron( pb, pCal->rcPrev, true,  clrPrev, chevThick )
            DTP_PaintChevron( pb, pCal->rcNext, false, clrNext, chevThick )
        end if
    next

    if pCal->eView = DTP_VIEW_DAYS then
        for p as long = 0 to nDrawn - 1
            ' weekday header row
            pb->SetForeColor( pc->WeekdayColor )
            if hFont then pb->SetFont( hFont )
            for i as long = 0 to 6
                pb->PaintText( DTP_WeekdayAbbrev(i, pCal->nWeekStart), @pCal->rcWeekday(p, i), DT_CENTER )
            next

            ' day cells
            for k as long = 0 to 41
                dim as SYSTEMTIME st = pCal->CellDate( p, k )
                dim as PSDATEPICKER_DAYPAINTINFO d
                d.hCtrl      = pCal->hOwner
                d.b          = pb
                d.rcCell     = pCal->rcDayCell(p, k)
                d.stDate     = st
                d.dayNumber  = st.wDay
                d.monthIndex = p
                d.isAdjacent = (st.wMonth <> pCal->panelMonth(p)) orelse (st.wYear <> pCal->panelYear(p))
                dim as long dow = PsDatePicker_DowSunday(st.wYear, st.wMonth, st.wDay)
                d.isWeekend  = (dow = 0) orelse (dow = 6)
                d.isToday    = DTP_SameYMD(st, pCal->stToday)
                d.isSelected = pCal->hasSelection andalso DTP_SameYMD(st, pCal->stSelected)
                dim as boolean bHotHere = (pCal->hotPart = DTP_HIT_DAYCELL) andalso (pCal->hotPanel = p) andalso (pCal->hotIndex = k)
                d.isHot      = bHotHere
                d.isDisabled = (DTP_InRange(pCal, st) = false)
                d.isEnabled  = bEnabled

                if hFont then pb->SetFont( hFont )
                if pCtrl->DayPaintCallback then
                    pCtrl->DayPaintCallback( @d )
                else
                    DTP_PaintDayDefault( pb, pc, @d )
                end if
            next
        next
    else
        ' MONTHS / YEARS grid
        dim as long baseY = PsDatePicker_DecadeBase( pCal->stAnchor.wYear )
        for k as long = 0 to 11
            dim as RECT rcCell = pCal->rcMonthCell(k)
            dim as DWSTRING sLabel
            dim as boolean bSel = false
            dim as boolean bDis = false
            dim as boolean bHotHere = false

            if pCal->eView = DTP_VIEW_MONTHS then
                sLabel = DTP_MonthAbbrev( k + 1 )
                bSel = (pCal->hasSelection) andalso (pCal->stSelected.wYear = pCal->stAnchor.wYear) andalso (pCal->stSelected.wMonth = (k + 1))
                bHotHere = (pCal->hotPart = DTP_HIT_MONTHCELL) andalso (pCal->hotIndex = k)
            else
                dim as long yy = baseY - 1 + k
                sLabel = str( yy )
                bSel = (pCal->hasSelection) andalso (pCal->stSelected.wYear = yy)
                bDis = (yy < baseY) orelse (yy > baseY + 9)      ' the leading/trailing decade cells
                bHotHere = (pCal->hotPart = DTP_HIT_YEARCELL) andalso (pCal->hotIndex = k)
            end if

            dim as RECT rcFill = rcCell
            dim as long inset = (rcCell.bottom - rcCell.top) \ 6
            if inset < 2 then inset = 2
            InflateRect( @rcFill, -inset, -inset )
            dim as long ell = (rcFill.bottom - rcFill.top) \ 3
            if ell < 2 then ell = 2

            if bSel then
                pb->SetBackColor( pc->DayBackColorSelected )
                pb->PaintRoundRect( @rcFill, ell )
            elseif bHotHere then
                pb->SetBackColor( pc->DayBackColorHot )
                pb->PaintRoundRect( @rcFill, ell )
            end if

            dim as COLORREF clr = pc->DayColor
            if bDis then
                clr = pc->DayColorAdjacent
            elseif bSel then
                clr = pc->DayForeColorSelected
            elseif bHotHere then
                clr = pc->DayForeColorHot
            end if
            pb->SetForeColor( clr )
            if hFont then pb->SetFont( hFont )
            pb->PaintText( sLabel, @rcCell, DT_CENTER )
        next
    end if

    if gDTP_Trace = 1 then
        dim as RECT rcQ
        GetClientRect( pCal->hWin, @rcQ )
        dim as RECT c0 = pCal->rcDayCell(0,0)
        dim as string sg = "CalGeom client=" & str(rcQ.right) & "x" & str(rcQ.bottom) & _
            " cellW=" & str(pCal->cellW(0)) & " rowH=" & str(pCal->rowH(0)) & _
            " cell0=(" & str(c0.left) & "," & str(c0.top) & "," & str(c0.right) & "," & str(c0.bottom) & ")" & _
            " title=[" & DTP_TitleText(pCal,0) & "]" & " font=" & str(pCtrl->hMainFont <> 0)
        DTP_Log( sg )
    end if
    ' ---- the optional "Today" footer -----------------------------------------------------
    if pCal->bShowTodayFooter then
        dim as RECT rcF = pCal->rcTodayFooter
        ' No rule above the band, by request: the gap alone separates it from the grid.
        dim as boolean bTodayOK = DTP_InRange( pCal, pCal->stToday )
        dim as COLORREF clrF = pc->TodayFooterColor
        if bTodayOK = false then
            clrF = pc->TodayFooterColorDisabled
        elseif pCal->hotPart = DTP_HIT_TODAY then
            clrF = pc->TodayFooterColorHot
        end if

        ' The label follows the HOST's Format callback, so the footer reads in the same date
        ' format as the field above it. Falls back to ISO when no callback is set.
        dim as DWSTRING sToday
        if pCtrl->FormatCallback then
            pCtrl->FormatCallback( pCtrl->hWin, @pCal->stToday, sToday )
        else
            sToday = format( pCal->stToday.wYear, "0000" ) & "-" & _
                     format( pCal->stToday.wMonth, "00" ) & "-" & _
                     format( pCal->stToday.wDay, "00" )
        end if

        if hFont then pb->SetFont( hFont )
        pb->SetForeColor( clrF )
        pb->PaintText( "Today: " & sToday, @rcF, DT_CENTER )
    end if

    ' ---- popup border, last, over everything --------------------------------------------
    ' PaintRoundOutline, NOT PaintBorderRect. Every fill-and-stroke primitive routes through
    ' PaintRectFactory, which calls FillPath UNCONDITIONALLY before it strokes -- so
    ' PaintBorderRect used as a frame here floods the entire client with whatever back colour
    ' was last set and erases the calendar. It shipped that way: the last SetBackColor before
    ' this line is the selected day's, so the whole popup came out flat accent-blue.
    ' Curvature 0 makes the outline square. (Fourth occurrence of this bug in the family.)
    dim as RECT rcEdge = pCal->rcClient
    pb->SetPenColor( pc->CalBorderColor )
    pb->PaintRoundOutline( @rcEdge, 0, 1 )
end sub


' WM_PAINT: one buffer per repaint, then hand off to the renderer.
private sub DTP_CalPaint( byval hCal as HWND )
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hCal )
    if pCal = 0 then exit sub
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( pCal->hOwner )
    if pCtrl = 0 then exit sub

    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCal )
    b.SetBackColor( pCtrl->colors.CalBackColor )
    b.PaintClientRect()

    DTP_CalRender( pCal, pCtrl, @b )

    b.EndDoubleBuffer()
end sub


' ========================================================================================
' Calendar navigation + click actions
' ========================================================================================
private sub DTP_CalRelayout( byval pCal as PSDATEPICKER_CAL ptr )
    if pCal = 0 then exit sub
    pCal->bLayoutDirty = true
    if pCal->hWin then InvalidateRect( pCal->hWin, NULL, TRUE )
end sub

private sub DTP_NavStep( byval pCal as PSDATEPICKER_CAL ptr, byval nDir as long )
    if pCal = 0 then exit sub
    select case pCal->eView
    case DTP_VIEW_DAYS
        pCal->stAnchor = PsDatePicker_AddMonths( pCal->stAnchor, nDir )
    case DTP_VIEW_MONTHS
        pCal->stAnchor = PsDatePicker_MakeDate( pCal->stAnchor.wYear + nDir, pCal->stAnchor.wMonth, 1 )
    case DTP_VIEW_YEARS
        pCal->stAnchor = PsDatePicker_MakeDate( pCal->stAnchor.wYear + nDir * 10, pCal->stAnchor.wMonth, 1 )
    end select
    DTP_CalRelayout( pCal )
end sub

private sub DTP_CalCommitPick( byval pCal as PSDATEPICKER_CAL ptr, byref st as SYSTEMTIME, byval panel as long )
    if pCal = 0 then exit sub
    pCal->stSelected   = st
    pCal->hasSelection = true
    dim as boolean bAdjacent = (st.wMonth <> pCal->panelMonth(panel)) orelse (st.wYear <> pCal->panelYear(panel))
    if bAdjacent then pCal->stAnchor = PsDatePicker_MakeDate( st.wYear, st.wMonth, 1 )
    pCal->bClosingByPick = true
    ' notify the owner (synchronous; the pointer is valid for the call). The owner formats the
    ' field, closes us and fires DateChanged.
    SendMessageW( pCal->hOwner, DTP_CM_DATEPICKED, 0, cast(LPARAM, @pCal->stSelected) )
end sub

private sub DTP_CalClick( byval pCal as PSDATEPICKER_CAL ptr, byval pt as POINT )
    if pCal = 0 then exit sub
    dim as DTP_HITRESULT hr = DTP_CalHitTest( pCal, pt )

    select case hr.part
    case DTP_HIT_PREV
        if DTP_PrevDisabled(pCal) = false then DTP_NavStep( pCal, -1 )
    case DTP_HIT_NEXT
        if DTP_NextDisabled(pCal) = false then DTP_NavStep( pCal, 1 )
    case DTP_HIT_TITLE
        if pCal->eView = DTP_VIEW_DAYS then
            pCal->eView = DTP_VIEW_MONTHS
            DTP_CalRelayout( pCal )
        elseif pCal->eView = DTP_VIEW_MONTHS then
            pCal->eView = DTP_VIEW_YEARS
            DTP_CalRelayout( pCal )
        end if
    case DTP_HIT_DAYCELL
        dim as SYSTEMTIME st = pCal->CellDate( hr.panel, hr.index )
        if DTP_InRange( pCal, st ) then DTP_CalCommitPick( pCal, st, hr.panel )
    case DTP_HIT_MONTHCELL
        pCal->stAnchor = PsDatePicker_MakeDate( pCal->stAnchor.wYear, hr.index + 1, 1 )
        pCal->eView = DTP_VIEW_DAYS
        DTP_CalRelayout( pCal )
    case DTP_HIT_YEARCELL
        dim as long nBase = PsDatePicker_DecadeBase( pCal->stAnchor.wYear )
        dim as long yy = nBase - 1 + hr.index
        pCal->stAnchor = PsDatePicker_MakeDate( yy, pCal->stAnchor.wMonth, 1 )
        pCal->eView = DTP_VIEW_MONTHS
        DTP_CalRelayout( pCal )
    case DTP_HIT_TODAY
        ' SELECTS today and closes, exactly as clicking today's cell would -- it routes through
        ' the same DTP_CalCommitPick, so there is ONE commit path and the footer cannot drift
        ' from the grid. Inert when today is outside the range, matching how it paints.
        '
        ' (It began as navigate-only, on the theory that it was an escape hatch out of a deep
        ' drill-down. Changed on request: picking today is the common case, and the drill-down
        ' escape comes free anyway because the next open reseeds the view to DAYS.)
        if DTP_InRange( pCal, pCal->stToday ) then
            DTP_CalCommitPick( pCal, pCal->stToday, 0 )
        end if
    end select
end sub

private sub DTP_CalWheel( byval pCal as PSDATEPICKER_CAL ptr, byval nDelta as long )
    if pCal = 0 then exit sub
    pCal->nWheelAccum += nDelta
    dim as long nNotches = pCal->nWheelAccum \ WHEEL_DELTA
    if nNotches = 0 then exit sub
    pCal->nWheelAccum -= nNotches * WHEEL_DELTA
    ' wheel away from the user (positive) goes to the PREVIOUS page, matching a scrollbar's up.
    dim as long nDir = -1
    if nNotches < 0 then nDir = 1
    dim as long steps = abs(nNotches)
    for i as long = 1 to steps
        if nDir < 0 then
            if DTP_PrevDisabled(pCal) then exit for
            DTP_NavStep( pCal, -1 )
        else
            if DTP_NextDisabled(pCal) then exit for
            DTP_NavStep( pCal, 1 )
        end if
    next
end sub


' ========================================================================================
' Calendar window procedure
' ========================================================================================
function DTP_CalWndProc( byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hwnd )

    select case uMsg

    case WM_PAINT
        DTP_CalPaint( hwnd )
        return 0

    case WM_ERASEBKGND
        return true

    case WM_MOUSEACTIVATE
        ' never take activation -- the owner stays foreground, keystrokes keep flowing to it,
        ' and its pump filter is what handles Esc / outside-click dismissal.
        return MA_NOACTIVATE

    case WM_SIZE
        if pCal then
            pCal->bLayoutDirty = true
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_MOUSEMOVE
        if pCal = 0 then exit select
        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        dim as DTP_HITRESULT hr = DTP_CalHitTest( pCal, pt )
        ' a day cell only lights when it is in range
        if hr.part = DTP_HIT_DAYCELL then
            dim as SYSTEMTIME st = pCal->CellDate( hr.panel, hr.index )
            if DTP_InRange( pCal, st ) = false then
                hr.part = DTP_HIT_NONE : hr.panel = -1 : hr.index = -1
            end if
        end if
        ' ...and so does the Today footer: it paints inert when today is out of range, so it
        ' must not light up under the cursor either.
        if hr.part = DTP_HIT_TODAY then
            if DTP_InRange( pCal, pCal->stToday ) = false then hr.part = DTP_HIT_NONE
        end if
        dim as boolean bChanged = (hr.part <> pCal->hotPart) orelse (hr.panel <> pCal->hotPanel) orelse (hr.index <> pCal->hotIndex)
        if bChanged then
            pCal->hotPart  = hr.part
            pCal->hotPanel = hr.panel
            pCal->hotIndex = hr.index
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        DTP_TipUpdate( pCal, hr )

        dim tme as TrackMouseEvent
        tme.cbSize    = sizeof(TrackMouseEvent)
        tme.dwFlags   = TME_LEAVE
        tme.hwndTrack = hwnd
        TrackMouseEvent(@tme)
        return 0

    case WM_MOUSELEAVE
        if pCal then
            if pCal->hotPart <> DTP_HIT_NONE then
                pCal->hotPart = DTP_HIT_NONE : pCal->hotPanel = -1 : pCal->hotIndex = -1
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            DTP_TipHide( pCal )
        end if
        return 0

    case WM_LBUTTONDOWN
        if pCal then
            DTP_TipHide( pCal )
            dim as POINT pt
            GetCursorPos( @pt )
            ScreenToClient( hwnd, @pt )
            DTP_CalClick( pCal, pt )
        end if
        return 0

    case WM_MOUSEWHEEL
        if pCal then
            DTP_CalWheel( pCal, cast(short, hiword(wParam)) )
        end if
        return 0

    case WM_TIMER
        if (pCal <> 0) andalso (wParam = IDT_CDATEPICKER_CALPOLL) then
            dim as HWND hFg = GetForegroundWindow()
            dim as HWND hOwnerRoot = GetAncestor( pCal->hOwner, GA_ROOT )
            dim as boolean bOurs = (hFg = hOwnerRoot) orelse (hFg = hwnd)
            if bOurs = false then
                SendMessageW( pCal->hOwner, DTP_CM_CALCLOSED, 0, 0 )
            end if
            return 0
        end if

    case WM_DESTROY
        if pCal then
            if pCal->pollTimerOn then
                KillTimer( hwnd, IDT_CDATEPICKER_CALPOLL )
                pCal->pollTimerOn = false
            end if
            ' The tip goes with the popup, which is rebuilt on the next open.
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
' Calendar lifecycle: create (lazy), seed, show
' ========================================================================================
function DTP_EnsureCalendar( byval hCtrl as HWND ) as HWND
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( hCtrl )
    if pCtrl = 0 then return 0
    if pCtrl->hCal then return pCtrl->hCal

    dim as HWND hOwner = GetAncestor( hCtrl, GA_ROOT )
    if hOwner = 0 then hOwner = GetParent( hCtrl )

    dim pWindow as CWindow ptr = New CWindow
    dim as HWND hCal = pWindow->Create( hOwner, "", @DTP_CalWndProc, _
            0, 0, 0, 0, _
            WS_POPUP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_TOOLWINDOW or WS_EX_NOACTIVATE )
    if hCal = 0 then
        delete pWindow
        return 0
    end if
    pWindow->ClassStyle = CS_DROPSHADOW

    dim as PSDATEPICKER_CAL ptr pCal = new PSDATEPICKER_CAL
    pCal->hWin   = hCal
    pCal->hOwner = hCtrl
    pCal->idc    = pCtrl->idc_Ctrl

    ' DPI-scale the calendar metrics once, here (the family rule).
    pCal->nPad      = pWindow->ScaleX( CDTP_CAL_PAD )
    pCal->nGutter   = pWindow->ScaleX( CDTP_CAL_GUTTER )
    pCal->nHeaderH  = pWindow->ScaleY( CDTP_CAL_HEADERH )
    pCal->nChevW    = pWindow->ScaleX( CDTP_CAL_CHEVW )
    pCal->nWeekdayH = pWindow->ScaleY( CDTP_CAL_WEEKDAYH )
    pCal->nCellMinW = pWindow->ScaleX( CDTP_CAL_CELLMINW )
    pCal->nCellMinH = pWindow->ScaleY( CDTP_CAL_CELLMINH )

    pWindow->UserData(0) = cast(LONG_PTR, pCal)
    pCtrl->hCal = hCal
    return hCal
end function

sub DTP_SeedCalendar( byval pCtrl as PSDATEPICKER ptr )
    DTP_Log("SeedCalendar enter")
    if pCtrl = 0 then exit sub
    if pCtrl->hCal = 0 then exit sub
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( pCtrl->hCal )
    if pCal = 0 then exit sub

    pCal->nWeekStart   = pCtrl->nWeekStart
    pCal->nMonthCount  = pCtrl->nMonthCount
    pCal->bShowTodayFooter = pCtrl->bShowTodayFooter
    pCal->hasSelection = pCtrl->hasDate
    pCal->stSelected   = pCtrl->stDate
    pCal->hasMin       = pCtrl->hasMin
    pCal->stMin        = pCtrl->stMin
    pCal->hasMax       = pCtrl->hasMax
    pCal->stMax        = pCtrl->stMax
    GetLocalTime( @pCal->stToday )

    dim as SYSTEMTIME nBase
    if pCtrl->hasDate then nBase = pCtrl->stDate else nBase = pCal->stToday
    pCal->stAnchor = PsDatePicker_MakeDate( nBase.wYear, nBase.wMonth, 1 )
    pCal->eView    = DTP_VIEW_DAYS
    pCal->hotPart  = DTP_HIT_NONE
    pCal->hotPanel = -1
    pCal->hotIndex = -1
    pCal->tipKey   = -1
    pCal->bLayoutDirty = true
end sub

private sub DTP_ShowCalendar( byval hCal as HWND, byref rcAnchor as RECT, byval w as long, byval h as long )
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hCal )
    if pCal = 0 then exit sub

    dim as long x = rcAnchor.left
    dim as long y = rcAnchor.bottom - 1          ' merge the popup edge with the control's

    dim as RECT rcTarget = type( x, y, x + w, y + h )
    dim as MONITORINFO mi
    mi.cbSize = sizeof(MONITORINFO)
    dim as HMONITOR hMon = MonitorFromRect( @rcTarget, MONITOR_DEFAULTTONEAREST )
    if GetMonitorInfo( hMon, @mi ) then
        if x + w > mi.rcWork.right then x = mi.rcWork.right - w
        if x < mi.rcWork.left then x = mi.rcWork.left
        if y + h > mi.rcWork.bottom then
            ' no room below: FLIP ABOVE the control rather than clamp up over it (the click
            ' that opened it would otherwise land on a day and self-dismiss).
            dim as long yAbove = rcAnchor.top - h
            if yAbove >= mi.rcWork.top then y = yAbove else y = mi.rcWork.top
        end if
        if y < mi.rcWork.top then y = mi.rcWork.top
    end if

    DTP_Log("ShowCalendar x=" & str(x) & " y=" & str(y) & " w=" & str(w) & " h=" & str(h))
    SetWindowPos( hCal, HWND_TOP, x, y, w, h, SWP_NOACTIVATE or SWP_SHOWWINDOW )
    InvalidateRect( hCal, NULL, TRUE )

    if pCal->pollTimerOn = false then
        SetTimer( hCal, IDT_CDATEPICKER_CALPOLL, PSDATEPICKER_CALPOLL_MS, NULL )
        pCal->pollTimerOn = true
    end if
end sub


' ========================================================================================
' Field <-> date bridge
' ========================================================================================
' Format the current date into the field via the host's FormatCallback. Silent.
sub DTP_FormatIntoField( byval pCtrl as PSDATEPICKER ptr )
    if pCtrl = 0 then exit sub
    if pCtrl->hTextBox = 0 then exit sub

    if pCtrl->hasDate = false then
        PsTextBox_SetText( pCtrl->hTextBox, "" )
        pCtrl->lastValidText = ""
        exit sub
    end if

    dim as DWSTRING sOut
    if pCtrl->FormatCallback then
        pCtrl->FormatCallback( pCtrl->hWin, @pCtrl->stDate, sOut )
    else
        ' a plain ISO fallback so the control is usable before a host wires Format
        sOut = format( pCtrl->stDate.wYear, "0000" ) & "-" & _
               format( pCtrl->stDate.wMonth, "00" ) & "-" & _
               format( pCtrl->stDate.wDay, "00" )
    end if
    PsTextBox_SetText( pCtrl->hTextBox, sOut )   ' programmatic; silent
    pCtrl->lastValidText = sOut
end sub

' Clamp a date into the control's range (if any).
private function DTP_ClampToRange( byval pCtrl as PSDATEPICKER ptr, byref st as SYSTEMTIME ) as SYSTEMTIME
    if pCtrl = 0 then return st
    dim as longint s = DTP_Serial(st)
    if pCtrl->hasMin then
        if s < DTP_Serial(pCtrl->stMin) then return pCtrl->stMin
    end if
    if pCtrl->hasMax then
        if s > DTP_Serial(pCtrl->stMax) then return pCtrl->stMax
    end if
    return st
end function

' Commit whatever the user typed: parse, validate, clamp, reformat, and report once.
private sub DTP_CommitTypedText( byval pCtrl as PSDATEPICKER ptr )
    DTP_Log("CommitTypedText enter")
    if pCtrl = 0 then exit sub
    if pCtrl->hTextBox = 0 then exit sub

    dim as DWSTRING sText = PsTextBox_GetText( pCtrl->hTextBox )
    dim as boolean bWasDate = pCtrl->hasDate
    dim as SYSTEMTIME stOld = pCtrl->stDate

    ' empty field
    dim as string sTrim = trim( sText )
    if len( sTrim ) = 0 then
        if pCtrl->bAllowEmpty then
            dim as boolean bChanged = (bWasDate = true)
            pCtrl->hasDate = false
            DTP_FormatIntoField( pCtrl )     ' clears + records
            if bChanged andalso (pCtrl->DateChangedCallback <> 0) then
                pCtrl->DateChangedCallback( pCtrl->hWin, @pCtrl->stDate, false )
            end if
        else
            ' empty not allowed -> revert to the last valid text
            PsTextBox_SetText( pCtrl->hTextBox, pCtrl->lastValidText )
        end if
        exit sub
    end if

    ' non-empty: ask the host to parse
    dim as SYSTEMTIME stNew
    dim as boolean bOK = false
    if pCtrl->ParseCallback then
        bOK = pCtrl->ParseCallback( pCtrl->hWin, sText, @stNew )
    end if

    if bOK = false then
        ' unparseable -> revert to the last valid text (or empty)
        PsTextBox_SetText( pCtrl->hTextBox, pCtrl->lastValidText )
        exit sub
    end if

    stNew = DTP_ClampToRange( pCtrl, stNew )
    stNew = PsDatePicker_MakeDate( stNew.wYear, stNew.wMonth, stNew.wDay )
    pCtrl->stDate  = stNew
    pCtrl->hasDate = true
    DTP_FormatIntoField( pCtrl )

    dim as boolean bChanged = (bWasDate = false) orelse (DTP_SameYMD(stNew, stOld) = false)
    if bChanged andalso (pCtrl->DateChangedCallback <> 0) then
        pCtrl->DateChangedCallback( pCtrl->hWin, @pCtrl->stDate, true )
    end if
end sub


' ========================================================================================
' Host message-callback relay
' ========================================================================================
private function DTP_CallMessage( byval pCtrl as PSDATEPICKER ptr, byval hCtrl as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as boolean
    if pCtrl = 0 then return false
    if pCtrl->MessageCallback = 0 then return false
    dim as PSDATEPICKER_MESSAGEINFO m
    m.hCtrl  = hCtrl
    m.uMsg   = uMsg
    m.wParam = wParam
    m.lParam = lParam
    return pCtrl->MessageCallback( @m )
end function


' ========================================================================================
' The three hooks into the embedded PsTextBox.
' ========================================================================================
private sub DTP_OnFieldFocus( byval hTextBox as HWND, byval bGotFocus as boolean )
    DTP_Log("OnFieldFocus got=" & str(bGotFocus))
    dim as HWND hCtrl = GetParent( hTextBox )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( hCtrl )
    if pCtrl = 0 then exit sub

    pCtrl->isFocused = bGotFocus
    InvalidateRect( hCtrl, NULL, FALSE )

    if bGotFocus = false then DTP_CommitTypedText( pCtrl )
end sub

private sub DTP_OnFieldEnter( byval hTextBox as HWND )
    dim as HWND hCtrl = GetParent( hTextBox )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( hCtrl )
    if pCtrl = 0 then exit sub
    DTP_CommitTypedText( pCtrl )
end sub

' Relay the field's messages to the host, and CLAIM the keys that open the calendar.
private function DTP_OnFieldMessage( byval m as PSTEXTBOX_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false
    dim as HWND hCtrl = GetParent( m->hTextBox )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( hCtrl )
    if pCtrl = 0 then return false

    dim as boolean bClaimed = DTP_CallMessage( pCtrl, hCtrl, m->uMsg, m->wParam, m->lParam )
    if bClaimed andalso (m->uMsg <> WM_KILLFOCUS) then return true

    if pCtrl->isEnabled = false then return false

    if m->uMsg = WM_KEYDOWN then
        ' Alt is deliberately NOT tested: Alt+Down and a plain Down both open the calendar,
        ' so there is nothing for the modifier to decide.
        dim as boolean bOpenKey = false
        select case m->wParam
        case VK_F4
            bOpenKey = true
        case VK_DOWN
            ' Alt+Down or a plain Down both open the calendar (a single-line field does
            ' nothing useful with Down otherwise).
            bOpenKey = true
        end select
        if bOpenKey then
            if pCtrl->isOpen then PsDatePicker_CloseUp( hCtrl ) else PsDatePicker_DropDown( hCtrl )
            return true
        end if
    end if

    return false
end function


' ========================================================================================
' Editing-box painting
' ========================================================================================
private sub PsDatePicker_RenderDefault( byval pCtrl as PSDATEPICKER ptr, byval b as PsBufferPaint ptr, byval p as PSDATEPICKER_PAINTINFO ptr )
    if (pCtrl = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long cw = p->rcFrame.right - p->rcFrame.left
    dim as long ch = p->rcFrame.bottom - p->rcFrame.top
    if (cw <= 0) orelse (ch <= 0) then exit sub

    dim as long ell = pCtrl->nCornerRadius * 2

    ' frame fill (also the field cell's background for the inset strips the child does not cover)
    dim as COLORREF clrBase = pCtrl->colors.FieldBackColor
    if p->isEnabled = false then clrBase = pCtrl->colors.FieldBackColorDisabled
    dim as RECT rcFrame = p->rcFrame
    b->SetBackColor( clrBase )
    b->PaintRoundRect( @rcFrame, ell )

    ' icon cell, extended inward past the divider so only the outer (right) corners round
    dim as long ext = pCtrl->nCornerRadius + pCtrl->nDividerThick + 1
    dim as RECT rcIconFill
    SetRect( @rcIconFill, p->rcIcon.left - ext, p->rcIcon.top, p->rcIcon.right, p->rcIcon.bottom )
    dim as COLORREF clrIconBack = pCtrl->colors.IconColor
    dim as boolean bIconLit = (p->iconHot orelse p->iconPressed)
    if (p->isEnabled <> false) andalso bIconLit then clrIconBack = pCtrl->colors.IconBackColorHot
    b->SetBackColor( clrIconBack )
    b->PaintRoundRect( @rcIconFill, ell )

    ' divider
    if pCtrl->nDividerThick > 0 then
        dim as COLORREF clrDiv = pCtrl->colors.DividerColor
        if p->isEnabled = false then clrDiv = pCtrl->colors.DividerColorDisabled
        dim as RECT rcDiv = p->rcDiv
        b->SetBackColor( clrDiv )
        b->PaintRect( @rcDiv )
    end if

    ' icon glyph
    dim as COLORREF clrGlyph = pCtrl->colors.IconGlyphColor
    if p->isEnabled = false then
        clrGlyph = pCtrl->colors.IconGlyphColorDisabled
    elseif bIconLit then
        clrGlyph = pCtrl->colors.IconGlyphColorHot
    end if
    dim as HFONT hIcon = DTP_ActiveIconFont( pCtrl )
    if hIcon then b->SetFont( hIcon )
    b->SetForeColor( clrGlyph )
    dim as RECT rcIcon = p->rcIcon
    b->PaintText( pCtrl->wszIconGlyph, @rcIcon, DT_CENTER )

    ' frame outline last
    if pCtrl->nBorderThick > 0 then
        dim as COLORREF clrBorder = pCtrl->colors.BorderColor
        if p->isEnabled = false then
            clrBorder = pCtrl->colors.BorderColorDisabled
        elseif p->isFocused then
            clrBorder = pCtrl->colors.FocusBorderColor
        end if
        dim as RECT rcOutline = p->rcFrame
        b->SetPenColor( clrBorder )
        b->PaintRoundOutline( @rcOutline, ell, pCtrl->nBorderThick )
    end if
end sub

private sub DTP_SyncChild( byval pCtrl as PSDATEPICKER ptr )
    if pCtrl = 0 then exit sub
    if pCtrl->hTextBox = 0 then exit sub
    if pCtrl->bLayoutDirty then pCtrl->LayoutControl()

    dim as RECT rc = pCtrl->rcField
    dim as long cx = rc.right - rc.left
    dim as long cy = rc.bottom - rc.top
    if (cx <= 0) orelse (cy <= 0) then
        ShowWindow( pCtrl->hTextBox, SW_HIDE )
        exit sub
    end if

    dim as RECT rcNow
    GetWindowRect( pCtrl->hTextBox, @rcNow )
    MapWindowPoints( HWND_DESKTOP, pCtrl->hWin, cast(POINT ptr, @rcNow), 2 )
    if EqualRect( @rcNow, @rc ) = 0 then
        SetWindowPos( pCtrl->hTextBox, 0, rc.left, rc.top, cx, cy, SWP_NOZORDER or SWP_NOACTIVATE )
    end if
    ShowWindow( pCtrl->hTextBox, SW_SHOW )
end sub

private sub DTP_ApplyFieldColors( byval pCtrl as PSDATEPICKER ptr )
    if pCtrl = 0 then exit sub
    if pCtrl->hTextBox = 0 then exit sub
    if pCtrl->isEnabled then
        PsTextBox_SetBackColor( pCtrl->hTextBox, pCtrl->colors.FieldBackColor )
        PsTextBox_SetForeColor( pCtrl->hTextBox, pCtrl->colors.FieldForeColor )
    else
        PsTextBox_SetBackColor( pCtrl->hTextBox, pCtrl->colors.FieldBackColorDisabled )
        PsTextBox_SetForeColor( pCtrl->hTextBox, pCtrl->colors.FieldForeColorDisabled )
    end if
end sub

private function PsDatePicker_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hwnd)
    if pCtrl = null then return 0

    if pCtrl->bLayoutDirty then pCtrl->LayoutControl()
    DTP_SyncChild( pCtrl )

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( pCtrl->colors.BackColor )
    b.PaintClientRect()

    dim p as PSDATEPICKER_PAINTINFO
    p.hCtrl       = hwnd
    p.b           = @b
    GetClientRect( hwnd, @p.rcClient )
    p.rcFrame     = pCtrl->rcFrame
    p.rcField     = pCtrl->rcField
    p.rcDiv       = pCtrl->rcDiv
    p.rcIcon      = pCtrl->rcIcon
    p.isEnabled   = pCtrl->isEnabled
    p.isFocused   = pCtrl->isFocused
    p.isOpen      = pCtrl->isOpen
    p.iconHot     = pCtrl->iconHot
    p.iconPressed = pCtrl->iconPressed

    if pCtrl->PaintCallback then
        pCtrl->PaintCallback( @p )
    else
        PsDatePicker_RenderDefault( pCtrl, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Container window procedure
' ========================================================================================
function PsDatePicker_WndProc( byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return PsDatePicker_OnPaint( hwnd )

    case WM_ERASEBKGND
        return true

    case WM_SIZE
        if pCtrl then
            pCtrl->Refresh()
            DTP_SyncChild( pCtrl )
        end if
        return 0

    case WM_SETFOCUS
        if (pCtrl <> 0) andalso (pCtrl->hTextBox <> 0) then
            dim as HWND hEdit = PsTextBox_GetRichEditHandle( pCtrl->hTextBox )
            if hEdit then
                SetFocus( hEdit )
                return 0
            end if
        end if

    case WM_TIMER
        if (pCtrl <> 0) andalso (wParam = IDT_CDATEPICKER_HOTTRACK) then
            dim as RECT rcClient
            GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                if pCtrl->iconHot then
                    pCtrl->iconHot = false
                    InvalidateRect( hwnd, NULL, FALSE )
                end if
                if pCtrl->hotTimerOn then
                    KillTimer( hwnd, IDT_CDATEPICKER_HOTTRACK )
                    pCtrl->hotTimerOn = false
                end if
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        if pCtrl = 0 then exit select
        if pCtrl->isEnabled = false then exit select
        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        if pCtrl->bLayoutDirty then pCtrl->LayoutControl()
        dim as boolean bOverIcon = (PtInRect( @pCtrl->rcIcon, pt ) <> 0)
        if bOverIcon <> pCtrl->iconHot then
            pCtrl->iconHot = bOverIcon
            InvalidateRect( hwnd, NULL, FALSE )
        end if
        if bOverIcon then
            dim tme as TrackMouseEvent
            tme.cbSize    = sizeof(TrackMouseEvent)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)
            if pCtrl->hotTimerOn = false then
                SetTimer( hwnd, IDT_CDATEPICKER_HOTTRACK, PSDATEPICKER_HOTTRACK_MS, NULL )
                pCtrl->hotTimerOn = true
            end if
        end if
        if DTP_CallMessage( pCtrl, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pCtrl then
            if pCtrl->iconHot then
                pCtrl->iconHot = false
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            DTP_CallMessage( pCtrl, hwnd, uMsg, wParam, lParam )
        end if

    case WM_LBUTTONDOWN
        if pCtrl = 0 then return 0
        if pCtrl->isEnabled = false then return 0

        ' Consume the one-shot reopen guard now, whatever else happens below.
        dim as boolean bSuppress = pCtrl->bSuppressNextOpen
        pCtrl->bSuppressNextOpen = false

        dim as POINT pt
        GetCursorPos( @pt )
        ScreenToClient( hwnd, @pt )
        if pCtrl->bLayoutDirty then pCtrl->LayoutControl()
        dim as boolean bOverIcon = (PtInRect( @pCtrl->rcIcon, pt ) <> 0)

        if bOverIcon = false then
            if DTP_CallMessage( pCtrl, hwnd, uMsg, wParam, lParam ) then return 0
            exit select
        end if

        ' put the caret in the field (so keyboard works and the frame lights) without selecting
        if pCtrl->hTextBox then
            dim as HWND hEdit = PsTextBox_GetRichEditHandle( pCtrl->hTextBox )
            if hEdit then
                PsTextBox_SetSelectOnFocus( pCtrl->hTextBox, false )
                SetFocus( hEdit )
                PsTextBox_SetSelectOnFocus( pCtrl->hTextBox, true )
            end if
        end if

        if DTP_CallMessage( pCtrl, hwnd, uMsg, wParam, lParam ) then return 0
        if bSuppress then return 0        ' the click that just dismissed the calendar

        if pCtrl->isOpen then PsDatePicker_CloseUp( hwnd ) else PsDatePicker_DropDown( hwnd )
        return 0

    case WM_LBUTTONUP
        if pCtrl then
            DTP_CallMessage( pCtrl, hwnd, uMsg, wParam, lParam )
        end if

    case WM_ENABLE
        if pCtrl then
            pCtrl->isEnabled = (wParam <> 0)
            if pCtrl->isEnabled = false then
                pCtrl->iconHot = false
                if pCtrl->isOpen then PsDatePicker_CloseUp( hwnd )
            end if
            DTP_ApplyFieldColors( pCtrl )
            InvalidateRect( hwnd, NULL, TRUE )
            DTP_CallMessage( pCtrl, hwnd, uMsg, wParam, lParam )
        end if

    case DTP_CM_DATEPICKED
        if pCtrl then
            dim as SYSTEMTIME ptr pst = cast(SYSTEMTIME ptr, lParam)
            if pst then
                pCtrl->stDate  = *pst
                pCtrl->hasDate = true
                DTP_FormatIntoField( pCtrl )
            end if
            PsDatePicker_CloseUp( hwnd )
            if pCtrl->DateChangedCallback then
                pCtrl->DateChangedCallback( hwnd, @pCtrl->stDate, true )
            end if
            return 0
        end if

    case DTP_CM_CALCLOSED
        if pCtrl then
            PsDatePicker_CloseUp( hwnd )
            return 0
        end if

    case WM_DESTROY
        if pCtrl then
            if pCtrl->hotTimerOn then
                KillTimer( hwnd, IDT_CDATEPICKER_HOTTRACK )
                pCtrl->hotTimerOn = false
            end if
            if gDTP_OpenPicker = hwnd then gDTP_OpenPicker = 0
            if pCtrl->hCal then
                DestroyWindow( pCtrl->hCal )   ' top-level popup; nothing else would free it
                pCtrl->hCal = 0
            end if
        end if

    case WM_NCDESTROY
        if pCtrl then
            if pCtrl->hIconFontDefault then
                DeleteObject( pCtrl->hIconFontDefault )
                pCtrl->hIconFontDefault = 0
            end if
            delete(pCtrl)
        end if
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A DATE PICKER: an editable date field (a borderless PsTextBox) with a calendar icon on the
' right, and a popup calendar the control owns.
'
' THE HOST OWNS DATE FORMAT AND PARSING.
'   SetFormatCallback  - render a SYSTEMTIME the calendar/setters produced into field text.
'   SetParseCallback   - resolve what the user TYPED back into a SYSTEMTIME (return FALSE for
'                        invalid text; the control reverts to the last good text). With no
'                        Parse callback, typed text is never accepted (only the calendar sets
'                        the date); with no Format callback, an ISO yyyy-MM-dd fallback is used.
'
' FOCUS sits on the RichEdit two levels down: GetFocus() = hCtrl is ALWAYS false. Use
'   PsDatePicker_HasFocus(). Tab navigation works without IsDialogMessage because PsTextBox
'   handles VK_TAB itself; that needs WS_EX_CONTROLPARENT on this container (it is set).
'
' THE HOST MESSAGE-PUMP OBLIGATION -- NOT OPTIONAL:
'       do while GetMessage(@uMsg, null, 0, 0)
'           if PsDatePicker_FilterMessage( @uMsg ) then continue do
'           TranslateMessage @uMsg
'           DispatchMessage @uMsg
'       loop
'   It (1) forwards to PsTextBox_FilterMessage (the field's context menu), and (2) drives the
'   calendar: Esc and outside-clicks dismiss it, and the one-shot guard stops an icon click
'   that just dismissed it from instantly reopening it.
'
' The calendar opens with a click on the icon, F4, Alt+Down, or Down. It is mouse-driven for
' selection; Esc / an outside click / alt-tab dismiss it.
' ========================================================================================

function PsDatePicker_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_EX_CONTROLPARENT is load-bearing: PsTextBox walks GetNextDlgTabItem for VK_TAB and
    ' only sees through a container that declares itself one.
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsDatePicker_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_CONTROLPARENT )

    ' CS_DBLCLKS deliberately NOT set: a rapid second icon click is a legitimate
    ' close-then-reopen and must arrive as two WM_LBUTTONDOWN.

    dim as PSDATEPICKER ptr pCtrl = new PSDATEPICKER
    pCtrl->hWin     = hCtrl
    pCtrl->idc_Ctrl = CtrlID
    pCtrl->wszIconGlyph = wchr( CDTP_DEFAULT_ICONGLYPH )

    ' DPI-scale the Create-time defaults (the family rule); border/divider stay hairlines.
    pCtrl->nIconWidth     = pWindow->ScaleX( CDTP_DEFAULT_ICONWIDTH )
    pCtrl->nFieldPadLeft  = pWindow->ScaleX( CDTP_DEFAULT_FIELDPAD )
    pCtrl->nFieldPadRight = pWindow->ScaleX( CDTP_DEFAULT_FIELDPAD )
    pCtrl->nVertPadding   = pWindow->ScaleY( CDTP_DEFAULT_VERTPAD )
    pCtrl->nCornerRadius  = pWindow->ScaleX( CDTP_DEFAULT_CORNERRADIUS )

    ' Default icon font (control-owned, deleted at destroy). Segoe Fluent Icons carries the
    ' calendar glyph; a host can replace it with SetIconFont. This is a control-created glyph
    ' font with clear ownership -- not one of the host's text fonts.
    dim as long fontPx = pWindow->ScaleY( CDTP_DEFAULT_ICONFONTPT * 96 \ 72 )
    pCtrl->hIconFontDefault = CreateFontW( -fontPx, 0, 0, 0, FW_NORMAL, 0, 0, 0, _
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, _
            DEFAULT_PITCH or FF_DONTCARE, @wstr("Segoe Fluent Icons") )

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the pointer BEFORE creating the child (the child's creation sends messages back).
    pWindow->UserData(0) = cast(LONG_PTR, pCtrl)

    ' the editable field: borderless, plain text (dates are free-form)
    pCtrl->hTextBox = PsTextBox_Create( hCtrl, CtrlID )
    if pCtrl->hTextBox then
        PsTextBox_SetBorderWidth( pCtrl->hTextBox, 0 )
        PsTextBox_SetSelectOnFocus( pCtrl->hTextBox, true )
        PsTextBox_SetMargins( pCtrl->hTextBox, pCtrl->nFieldPadLeft, pCtrl->nFieldPadRight )
        DTP_ApplyFieldColors( pCtrl )
        PsTextBox_SetFocusCallback( pCtrl->hTextBox, @DTP_OnFieldFocus )
        PsTextBox_SetEnterPressedCallback( pCtrl->hTextBox, @DTP_OnFieldEnter )
        PsTextBox_SetMessageCallback( pCtrl->hTextBox, @DTP_OnFieldMessage )
        ShowWindow( pCtrl->hTextBox, SW_SHOW )
    end if

    function = hCtrl
end function

function PsDatePicker_GetTextBoxHandle( byval hCtrl as HWND ) as HWND
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    return pCtrl->hTextBox
end function

function PsDatePicker_GetCalendarHandle( byval hCtrl as HWND ) as HWND
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    return pCtrl->hCal
end function

function PsDatePicker_HasFocus( byval hCtrl as HWND ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    if pCtrl->hTextBox = 0 then return false
    return PsTextBox_HasFocus( pCtrl->hTextBox )
end function


' ---- value (all silent) ----
function PsDatePicker_GetDate( byval hCtrl as HWND, byref st as SYSTEMTIME ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    if pCtrl->hasDate = false then return false
    st = pCtrl->stDate
    return true
end function

sub PsDatePicker_SetDate( byval hCtrl as HWND, byref st as SYSTEMTIME )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    dim as SYSTEMTIME s = DTP_ClampToRange( pCtrl, st )
    pCtrl->stDate  = PsDatePicker_MakeDate( s.wYear, s.wMonth, s.wDay )
    pCtrl->hasDate = true
    DTP_FormatIntoField( pCtrl )
end sub

sub PsDatePicker_ClearDate( byval hCtrl as HWND )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->hasDate = false
    DTP_FormatIntoField( pCtrl )
end sub

function PsDatePicker_HasDate( byval hCtrl as HWND ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    return pCtrl->hasDate
end function

sub PsDatePicker_GetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byref bHasMin as boolean, byref bHasMax as boolean )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then
        bHasMin = false : bHasMax = false
        exit sub
    end if
    stMin = pCtrl->stMin : stMax = pCtrl->stMax
    bHasMin = pCtrl->hasMin : bHasMax = pCtrl->hasMax
end sub

sub PsDatePicker_SetRange( byval hCtrl as HWND, byref stMin as SYSTEMTIME, byref stMax as SYSTEMTIME, byval bHasMin as boolean, byval bHasMax as boolean )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->hasMin = bHasMin
    pCtrl->hasMax = bHasMax
    if bHasMin then pCtrl->stMin = PsDatePicker_MakeDate( stMin.wYear, stMin.wMonth, stMin.wDay )
    if bHasMax then pCtrl->stMax = PsDatePicker_MakeDate( stMax.wYear, stMax.wMonth, stMax.wDay )
    ' swap an inverted pair rather than leave an unsatisfiable range
    if bHasMin andalso bHasMax then
        if DTP_Serial(pCtrl->stMin) > DTP_Serial(pCtrl->stMax) then swap pCtrl->stMin, pCtrl->stMax
    end if
    ' re-clamp the current date, silently
    if pCtrl->hasDate then
        dim as SYSTEMTIME s = DTP_ClampToRange( pCtrl, pCtrl->stDate )
        pCtrl->stDate = s
        DTP_FormatIntoField( pCtrl )
    end if
end sub

function PsDatePicker_GetAllowEmpty( byval hCtrl as HWND ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return true
    return pCtrl->bAllowEmpty
end function

sub PsDatePicker_SetAllowEmpty( byval hCtrl as HWND, byval bAllow as boolean )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->bAllowEmpty = bAllow
end sub

sub PsDatePicker_ReformatText( byval hCtrl as HWND )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    DTP_FormatIntoField( pCtrl )
end sub


' ---- calendar behaviour ----
function PsDatePicker_GetWeekStart( byval hCtrl as HWND ) as long
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return DTP_SUNDAY
    return pCtrl->nWeekStart
end function

sub PsDatePicker_SetWeekStart( byval hCtrl as HWND, byval nWeekStart as long )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    if nWeekStart = DTP_MONDAY then pCtrl->nWeekStart = DTP_MONDAY else pCtrl->nWeekStart = DTP_SUNDAY
end sub

function PsDatePicker_GetMonthCount( byval hCtrl as HWND ) as long
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 1
    return pCtrl->nMonthCount
end function

sub PsDatePicker_SetMonthCount( byval hCtrl as HWND, byval nCount as long )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    if nCount < 1 then nCount = 1
    if nCount > 2 then nCount = 2
    pCtrl->nMonthCount = nCount
end sub

function PsDatePicker_GetShowTodayFooter( byval hCtrl as HWND ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    return pCtrl->bShowTodayFooter
end function

' Silent, like every setter. Takes effect at the next open (the calendar is seeded then); if
' the calendar is already open it is re-laid-out and resized immediately, because the band
' changes the popup's ideal HEIGHT and a stale window would clip it.
sub PsDatePicker_SetShowTodayFooter( byval hCtrl as HWND, byval bShow as boolean )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->bShowTodayFooter = bShow
    if pCtrl->hCal then
        dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( pCtrl->hCal )
        if pCal then
            pCal->bShowTodayFooter = bShow
            pCal->bLayoutDirty = true
            pCal->LayoutCalendar()
            if pCtrl->isOpen then
                SetWindowPos( pCtrl->hCal, 0, 0, 0, pCal->nIdealW, pCal->nIdealH, _
                              SWP_NOZORDER or SWP_NOACTIVATE or SWP_NOMOVE )
                InvalidateRect( pCtrl->hCal, NULL, TRUE )
            end if
        end if
    end if
end sub

function PsDatePicker_GetShowToday( byval hCtrl as HWND ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return true
    return pCtrl->bShowToday
end function

sub PsDatePicker_SetShowToday( byval hCtrl as HWND, byval bShow as boolean )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->bShowToday = bShow
end sub

sub PsDatePicker_DropDown( byval hCtrl as HWND )
    DTP_Log("DropDown enter")
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    if pCtrl->isEnabled = false then exit sub
    if pCtrl->isOpen then exit sub

    ' opening edge fires BEFORE the calendar content is built (the just-in-time hook)
    if pCtrl->DropDownCallback then pCtrl->DropDownCallback( hCtrl, true )

    dim as HWND hCal = DTP_EnsureCalendar( hCtrl )
    if hCal = 0 then
        if pCtrl->DropDownCallback then pCtrl->DropDownCallback( hCtrl, false )   ' keep the pairing
        exit sub
    end if

    DTP_SeedCalendar( pCtrl )
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hCal )
    pCal->bLayoutDirty = true
    pCal->LayoutCalendar()                 ' computes ideal size (rects wait for the real size)

    dim as RECT rcAnchor
    GetWindowRect( hCtrl, @rcAnchor )
    DTP_ShowCalendar( hCal, rcAnchor, pCal->nIdealW, pCal->nIdealH )

    pCtrl->isOpen = true
    gDTP_OpenPicker = hCtrl
    InvalidateRect( hCtrl, NULL, FALSE )
end sub

sub PsDatePicker_CloseUp( byval hCtrl as HWND )
    DTP_Log("CloseUp enter")
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub

    dim as boolean bWasOpen = pCtrl->isOpen
    pCtrl->isOpen = false

    if pCtrl->hCal then
        dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( pCtrl->hCal )
        if pCal then
            if pCal->pollTimerOn then
                KillTimer( pCtrl->hCal, IDT_CDATEPICKER_CALPOLL )
                pCal->pollTimerOn = false
            end if
            DTP_TipHide( pCal )
        end if
        ShowWindow( pCtrl->hCal, SW_HIDE )
    end if

    if gDTP_OpenPicker = hCtrl then gDTP_OpenPicker = 0
    InvalidateRect( hCtrl, NULL, FALSE )

    if bWasOpen andalso (pCtrl->DropDownCallback <> 0) then pCtrl->DropDownCallback( hCtrl, false )
end sub

function PsDatePicker_IsDroppedDown( byval hCtrl as HWND ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    return pCtrl->isOpen
end function


' ---- layout / appearance ----
function PsDatePicker_GetFont( byval hCtrl as HWND ) as HFONT
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    return pCtrl->hMainFont
end function

sub PsDatePicker_SetFont( byval hCtrl as HWND, byval hFont as HFONT )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->hMainFont = hFont                    ' caller-owned; used by the field AND the calendar
    if pCtrl->hTextBox then PsTextBox_SetFont( pCtrl->hTextBox, hFont )
    pCtrl->Refresh()
end sub

function PsDatePicker_GetIconFont( byval hCtrl as HWND ) as HFONT
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    return pCtrl->hIconFont
end function

sub PsDatePicker_SetIconFont( byval hCtrl as HWND, byval hFont as HFONT )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->hIconFont = hFont                 ' caller-owned; 0 restores the control's default
    InvalidateRect( hCtrl, NULL, FALSE )
end sub

function PsDatePicker_GetIconGlyph( byval hCtrl as HWND ) as DWSTRING
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return ""
    return pCtrl->wszIconGlyph
end function

sub PsDatePicker_SetIconGlyph( byval hCtrl as HWND, byref sGlyph as DWSTRING )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->wszIconGlyph = sGlyph
    InvalidateRect( hCtrl, NULL, FALSE )
end sub

function PsDatePicker_GetIconWidth( byval hCtrl as HWND ) as long
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    return pCtrl->nIconWidth
end function

sub PsDatePicker_SetIconWidth( byval hCtrl as HWND, byval nWidth as long )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    if nWidth < 0 then nWidth = 0
    pCtrl->nIconWidth = nWidth
    pCtrl->Refresh()
end sub

function PsDatePicker_GetCornerRadius( byval hCtrl as HWND ) as long
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    return pCtrl->nCornerRadius
end function

sub PsDatePicker_SetCornerRadius( byval hCtrl as HWND, byval nRadius as long )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    if nRadius < 0 then nRadius = 0
    pCtrl->nCornerRadius = nRadius
    pCtrl->Refresh()
end sub

function PsDatePicker_GetBorderThickness( byval hCtrl as HWND ) as long
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    return pCtrl->nBorderThick
end function

sub PsDatePicker_SetBorderThickness( byval hCtrl as HWND, byval nThickness as long )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pCtrl->nBorderThick = nThickness
    pCtrl->Refresh()
end sub

' Ideal size: the widest date text the Format callback produces cannot be known in advance,
' so this measures a representative long date ("Wednesday, September 30, 2222") if no date is
' set, otherwise the current text. Plus the icon, divider, corner inset and border.
sub PsDatePicker_GetIdealSize( byval hCtrl as HWND, byref nWidth as long, byref nHeight as long )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then
        nWidth = 0 : nHeight = 0
        exit sub
    end if

    dim as DWSTRING sMeasure
    if pCtrl->hTextBox then sMeasure = PsTextBox_GetText( pCtrl->hTextBox )
    if len( trim(sMeasure) ) = 0 then sMeasure = "Wednesday, September 30, 2222"

    dim as HDC hdc = GetDC( hCtrl )
    dim as HFONT hOld = 0
    if pCtrl->hMainFont then hOld = cast(HFONT, SelectObject( hdc, pCtrl->hMainFont ))
    dim as SIZE sz
    dim as WSTRING ptr pw = cast(WSTRING ptr, sMeasure.vptr)
    GetTextExtentPoint32W( hdc, pw, len(sMeasure), @sz )
    dim as TEXTMETRICW tm
    GetTextMetricsW( hdc, @tm )
    if hOld then SelectObject( hdc, hOld )
    ReleaseDC( hCtrl, hdc )

    dim as long textW = sz.cx
    if textW <= 0 then textW = tm.tmAveCharWidth * 20

    nWidth  = pCtrl->nCornerRadius + pCtrl->nFieldPadLeft + textW + pCtrl->nFieldPadRight _
              + pCtrl->nDividerThick + pCtrl->nIconWidth
    nHeight = tm.tmHeight + 2 * pCtrl->nVertPadding + 2 * pCtrl->nBorderThick
end sub

function PsDatePicker_GetFrameRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    if pCtrl->bLayoutDirty then pCtrl->LayoutControl()
    if pCtrl->bLayoutDirty then return false
    rc = pCtrl->rcFrame
    return true
end function

function PsDatePicker_GetFieldRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    if pCtrl->bLayoutDirty then pCtrl->LayoutControl()
    if pCtrl->bLayoutDirty then return false
    rc = pCtrl->rcField
    return true
end function

function PsDatePicker_GetIconRect( byval hCtrl as HWND, byref rc as RECT ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    if pCtrl->bLayoutDirty then pCtrl->LayoutControl()
    if pCtrl->bLayoutDirty then return false
    rc = pCtrl->rcIcon
    return true
end function

function PsDatePicker_HitTest( byval hCtrl as HWND, byval pt as POINT ) as long
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    if pCtrl->bLayoutDirty then pCtrl->LayoutControl()
    if PtInRect( @pCtrl->rcIcon, pt ) then return 1
    return 0
end function

function PsDatePicker_GetEnabled( byval hCtrl as HWND ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return false
    return pCtrl->isEnabled
end function

sub PsDatePicker_SetEnabled( byval hCtrl as HWND, byval isEnabled as boolean )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    EnableWindow( hCtrl, isEnabled )
    if pCtrl->hTextBox then EnableWindow( pCtrl->hTextBox, isEnabled )
    ' WM_ENABLE syncs the flag, colors and repaint.
end sub

sub PsDatePicker_Refresh( byval hCtrl as HWND )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->Refresh()
end sub


' ---- colours ----
sub PsDatePicker_GetColors( byval hCtrl as HWND, byval pColors as PSDATEPICKER_COLORS ptr )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if (pCtrl = 0) orelse (pColors = 0) then exit sub
    *pColors = pCtrl->colors
end sub

sub PsDatePicker_SetColors( byval hCtrl as HWND, byval pColors as PSDATEPICKER_COLORS ptr )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if (pCtrl = 0) orelse (pColors = 0) then exit sub
    pCtrl->colors = *pColors
    DTP_ApplyFieldColors( pCtrl )
    InvalidateRect( hCtrl, NULL, TRUE )
    if pCtrl->hCal then InvalidateRect( pCtrl->hCal, NULL, TRUE )
end sub


' ---- callbacks ----
sub PsDatePicker_SetParseCallback( byval hCtrl as HWND, byval userfunc as DTP_ParseCallbackFunc )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->ParseCallback = userfunc
end sub

sub PsDatePicker_SetFormatCallback( byval hCtrl as HWND, byval usersub as DTP_FormatCallbackSub )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->FormatCallback = usersub
    DTP_FormatIntoField( pCtrl )            ' re-render the current date in the new format
end sub

sub PsDatePicker_SetDateChangedCallback( byval hCtrl as HWND, byval usersub as DTP_DateChangedCallbackSub )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->DateChangedCallback = usersub
end sub

sub PsDatePicker_SetDropDownCallback( byval hCtrl as HWND, byval usersub as DTP_DropDownCallbackSub )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->DropDownCallback = usersub
end sub

sub PsDatePicker_SetPaintCallback( byval hCtrl as HWND, byval usersub as DTP_PaintCallbackSub )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->PaintCallback = usersub
end sub

sub PsDatePicker_SetDayPaintCallback( byval hCtrl as HWND, byval usersub as DTP_DayPaintCallbackSub )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->DayPaintCallback = usersub
end sub

sub PsDatePicker_SetMessageCallback( byval hCtrl as HWND, byval userfunc as DTP_MessageCallbackFunc )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->MessageCallback = userfunc
end sub


' ========================================================================================
' Tooltip backend (the popup calendar's day-cell tip).  See PsTipHost.bi.
' ========================================================================================
function PsDatePicker_GetTooltipHandle( byval hDatePicker as HWND ) as HWND
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hDatePicker)
    if pCtrl = 0 then return 0
    if pCtrl->hCal = 0 then return 0
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( pCtrl->hCal )
    if pCal = 0 then return 0
    return PsTipHost_SysHandle( @pCal->tip )
end function

function PsDatePicker_GetPsTooltipHandle( byval hDatePicker as HWND ) as HWND
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hDatePicker)
    if pCtrl = 0 then return 0
    if pCtrl->hCal = 0 then return 0
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( pCtrl->hCal )
    if pCal = 0 then return 0
    return PsTipHost_PsHandle( @pCal->tip )
end function

' Stored on the PICKER and applied by DTP_TipEnsure to whichever popup is current, so it
' survives the popup being destroyed and rebuilt. Applied immediately as well, for a calendar
' that happens to be open right now.
function PsDatePicker_SetTooltipMode( byval hDatePicker as HWND, byval nMode as long ) as boolean
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hDatePicker)
    if pCtrl = 0 then return false
    if (nMode <> PSTIP_MODE_SYSTEM) andalso (nMode <> PSTIP_MODE_PS) then return false
    pCtrl->nTipMode = nMode
    if pCtrl->hCal then
        dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( pCtrl->hCal )
        if pCal then
            PsTipHost_SetTracked( @pCal->tip, true )
            return PsTipHost_SetMode( @pCal->tip, nMode )
        end if
    end if
    return true
end function

function PsDatePicker_GetTooltipMode( byval hDatePicker as HWND ) as long
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hDatePicker)
    if pCtrl = 0 then return PSTIP_MODE_SYSTEM
    return pCtrl->nTipMode
end function

sub PsDatePicker_SetTooltipCallback( byval hCtrl as HWND, byval userfunc as DTP_TooltipCallbackFunc )
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then exit sub
    pCtrl->TooltipCallback = userfunc
end sub


' ========================================================================================
' Host message pump.  NOT optional -- see the banner.
' ========================================================================================
function PsDatePicker_FilterMessage( byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false

    ' the field's context menu pump (harmless when nothing is open)
    if PsTextBox_FilterMessage( pMsg ) then return true

    if gDTP_OpenPicker = 0 then return false
    dim as HWND hCtrl = gDTP_OpenPicker
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( hCtrl )
    if pCtrl = 0 then return false
    if pCtrl->isOpen = false then return false
    dim as HWND hCal = pCtrl->hCal

    select case pMsg->message
    case WM_KEYDOWN
        if pMsg->wParam = VK_ESCAPE then
            PsDatePicker_CloseUp( hCtrl )
            return true                     ' consume Esc so it does not reach the host
        end if

    case WM_LBUTTONDOWN, WM_NCLBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN
        if pMsg->hwnd <> hCal then
            ' a click outside the calendar dismisses it. If it landed on our own icon, arm the
            ' one-shot guard so the same click does not reopen it.
            if pMsg->hwnd = hCtrl then pCtrl->bSuppressNextOpen = true
            DTP_Log("FilterMessage outside-click close")
            PsDatePicker_CloseUp( hCtrl )
            return false                    ' let the click still reach its target
        end if
    end select

    return false
end function


' ========================================================================================
' Self-test hook: drive the REAL calendar painter into an OFFSCREEN buffer and count the
' distinct colours it produced over the whole client.
'
' THIS EXISTS BECAUSE THE PER-CELL PROBE MISSED A TOTAL WIPE-OUT. The control shipped with a
' probe that rendered ONE day cell through the host callback and counted tones in it -- which
' passed with 59 tones while the real popup was rendering as a single flat rectangle, because
' the bug was in the last line of the full-calendar painter (a fill-and-stroke primitive used
' as a frame) and that probe never ran it. A probe has to drive the code the user sees.
'
' A healthy calendar produces many tones (chrome + weekday row + 42 day numbers). A flooded
' one produces 1-2. Returns the count.
function PsDatePicker_TestCalendarTones( byval hCal as HWND ) as long
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hCal )
    if pCal = 0 then return 0
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( pCal->hOwner )
    if pCtrl = 0 then return 0

    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    dim as RECT rcClient
    GetClientRect( hCal, @rcClient )
    if (rcClient.right <= 0) orelse (rcClient.bottom <= 0) then return 0

    dim as HDC hdc = GetDC( hCal )
    if hdc = 0 then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCal, hdc, rcClient )
    b.SetBackColor( pCtrl->colors.CalBackColor )
    b.PaintClientRect()

    DTP_CalRender( pCal, pCtrl, @b )

    dim as HDC memDC = b.getMemDC()
    dim as long nDistinct = 0
    if memDC then
        dim as COLORREF seen(0 to 255)
        ' Sample the whole client on a small stride -- a full per-pixel scan of a 448x438
        ' popup through GetPixel is far too slow to sit in a self-test.
        for y as long = rcClient.top + 2 to rcClient.bottom - 3 step 3
            for x as long = rcClient.left + 2 to rcClient.right - 3 step 3
                dim as COLORREF c = GetPixel( memDC, x, y )
                if c = CLR_INVALID then continue for
                dim as boolean bFound = false
                for i as long = 0 to nDistinct - 1
                    if seen(i) = c then bFound = true : exit for
                next
                if (bFound = false) andalso (nDistinct <= ubound(seen)) then
                    seen(nDistinct) = c
                    nDistinct += 1
                end if
            next
        next
    end if

    b.EndDoubleBuffer()
    ReleaseDC( hCal, hdc )
    return nDistinct
end function


' Distinct tones inside the "Today" footer band, rendered offscreen through the REAL painter.
' Geometry and hit-testing prove where the band IS, not that anything was drawn in it.
function PsDatePicker_TestFooterTones( byval hCal as HWND ) as long
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hCal )
    if pCal = 0 then return 0
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( pCal->hOwner )
    if pCtrl = 0 then return 0
    if pCal->bLayoutDirty then pCal->LayoutCalendar()
    if IsRectEmpty( @pCal->rcTodayFooter ) then return 0

    dim as RECT rcClient
    GetClientRect( hCal, @rcClient )
    if (rcClient.right <= 0) orelse (rcClient.bottom <= 0) then return 0

    dim as HDC hdc = GetDC( hCal )
    if hdc = 0 then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCal, hdc, rcClient )
    b.SetBackColor( pCtrl->colors.CalBackColor )
    b.PaintClientRect()
    DTP_CalRender( pCal, pCtrl, @b )

    dim as RECT rc = pCal->rcTodayFooter
    dim as HDC memDC = b.getMemDC()
    dim as long nDistinct = 0
    if memDC then
        dim as COLORREF seen(0 to 63)
        for y as long = rc.top + 2 to rc.bottom - 3
            for x as long = rc.left + 2 to rc.right - 3 step 2
                dim as COLORREF c = GetPixel( memDC, x, y )
                if c = CLR_INVALID then continue for
                dim as boolean bFound = false
                for i as long = 0 to nDistinct - 1
                    if seen(i) = c then bFound = true : exit for
                next
                if (bFound = false) andalso (nDistinct <= ubound(seen)) then
                    seen(nDistinct) = c
                    nDistinct += 1
                end if
            next
        next
    end if

    b.EndDoubleBuffer()
    ReleaseDC( hCal, hdc )
    return nDistinct
end function


' Brightest pixel inside a chrome part, rendered offscreen through the REAL painter.
' nPart: 0 = prev chevron, 1 = next chevron, 100+k = day cell k of panel 0. Used to prove a
' disabled chevron -- and a disabled DAY --
' actually reaches the pixels dimmer than an enabled one (an assertion on the colour FIELD
' would only prove the field, not the render).
function PsDatePicker_TestPartBrightness( byval hCal as HWND, byval nPart as long ) as long
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hCal )
    if pCal = 0 then return -1
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( pCal->hOwner )
    if pCtrl = 0 then return -1
    if pCal->bLayoutDirty then pCal->LayoutCalendar()

    dim as RECT rcClient
    GetClientRect( hCal, @rcClient )
    if (rcClient.right <= 0) orelse (rcClient.bottom <= 0) then return -1

    dim as HDC hdc = GetDC( hCal )
    if hdc = 0 then return -1

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCal, hdc, rcClient )
    b.SetBackColor( pCtrl->colors.CalBackColor )
    b.PaintClientRect()
    DTP_CalRender( pCal, pCtrl, @b )

    dim as RECT rc = pCal->rcPrev
    if nPart = 1 then rc = pCal->rcNext
    ' nPart >= 100: day cell (nPart - 100) of panel 0
    if nPart >= 100 then
        dim as long k = nPart - 100
        if (k >= 0) andalso (k <= 41) then rc = pCal->rcDayCell(0, k)
    end if

    dim as HDC memDC = b.getMemDC()
    dim as long best = 0
    if memDC then
        for y as long = rc.top to rc.bottom - 1
            for x as long = rc.left to rc.right - 1
                dim as COLORREF c = GetPixel( memDC, x, y )
                if c = CLR_INVALID then continue for
                dim as long lum = (cast(long, c and &hFF) + cast(long, (c shr 8) and &hFF) + cast(long, (c shr 16) and &hFF)) \ 3
                if lum > best then best = lum
            next
        next
    end if

    b.EndDoubleBuffer()
    ReleaseDC( hCal, hdc )
    return best
end function


' ========================================================================================
' Self-test hook: build + seed + size the calendar WITHOUT showing it, so the geometry and
' the date mapping can be asserted with the popup hidden.
' ========================================================================================
function PsDatePicker_TestPrepareCalendar( byval hCtrl as HWND, byval eView as long, byval anchorYear as long, byval anchorMonth as long ) as HWND
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer(hCtrl)
    if pCtrl = 0 then return 0
    dim as HWND hCal = DTP_EnsureCalendar( hCtrl )
    if hCal = 0 then return 0

    DTP_SeedCalendar( pCtrl )
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer( hCal )
    if pCal = 0 then return 0

    pCal->eView    = eView
    pCal->stAnchor = PsDatePicker_MakeDate( anchorYear, anchorMonth, 1 )

    pCal->bLayoutDirty = true
    pCal->LayoutCalendar()                  ' compute ideal size (no client yet)
    SetWindowPos( hCal, 0, 0, 0, pCal->nIdealW, pCal->nIdealH, SWP_NOZORDER or SWP_NOACTIVATE or SWP_NOMOVE )
    pCal->bLayoutDirty = true
    pCal->LayoutCalendar()                  ' now lay the rects at the real client size
    return hCal
end function
