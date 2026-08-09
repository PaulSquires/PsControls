'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


type DEMO_ROW
    wszLabel as DWSTRING
end type
dim shared gRows(0 to PICK_COUNT - 1) as DEMO_ROW

dim shared as long gFailCount
dim shared as long gTestCount


' ========================================================================================
' Row geometry
' ========================================================================================
#define ROW_HEIGHT      54
#define ROW_TOP         16
#define ROW_MARGIN      22

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
' Demo month name / parse / format helpers
' ========================================================================================
private function DemoMonthAbbrev( byval m as long ) as DWSTRING
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

' Pull up to three integer groups out of free text ("2026-07-24", "7/24/2026", "24.7.26").
private function DemoExtractInts( byval sText as DWSTRING, byref n1 as long, byref n2 as long, byref n3 as long ) as long
    dim as string s = trim( sText )
    dim as long count = 0
    dim as string cur = ""
    dim as long vals(0 to 2) = { 0, 0, 0 }
    for i as long = 1 to len(s)
        dim as string ch = mid(s, i, 1)
        dim as boolean bDigit = (ch >= "0") andalso (ch <= "9")
        if bDigit then
            cur &= ch
        else
            if len(cur) > 0 then
                if count <= 2 then vals(count) = valint(cur)
                count += 1
                cur = ""
            end if
        end if
    next
    if len(cur) > 0 then
        if count <= 2 then vals(count) = valint(cur)
        count += 1
    end if
    n1 = vals(0) : n2 = vals(1) : n3 = vals(2)
    return count
end function

' Month number from a leading month name/abbreviation ("Jul", "July"); 0 if none.
private function DemoMonthFromName( byval sText as DWSTRING ) as long
    dim as string s = lcase( trim( sText ) )
    for i as long = 1 to 12
        dim as string abbrev = lcase( DemoMonthAbbrev(i) )
        if len(abbrev) > 0 then
            if left(s, len(abbrev)) = abbrev then return i
        end if
    next
    return 0
end function

' FORMAT: render a date as "MMM d, yyyy" (the host's chosen display format).
sub PsDatePicker_DemoFormat( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr, byref sOut as DWSTRING )
    if pDate = 0 then sOut = "" : exit sub
    sOut = DemoMonthAbbrev( pDate->wMonth ) & " " & str(pDate->wDay) & ", " & str(pDate->wYear)
end sub

' PARSE: resolve typed text back to a date. Accepts year-first (yyyy m d) or month-first
' (m d yyyy); a token > 31 is taken as the year.
function PsDatePicker_DemoParse( byval hCtrl as HWND, byval sText as DWSTRING, byval pDate as SYSTEMTIME ptr ) as boolean
    if pDate = 0 then return false
    dim as long a, b, c
    dim as long got = DemoExtractInts( sText, a, b, c )

    dim as long y, m, d

    if got >= 3 then
        ' three numbers: year-first if the first token cannot be a day/month
        if a > 31 then
            y = a : m = b : d = c
        else
            m = a : d = b : y = c
        end if
    elseif got = 2 then
        ' two numbers + (hopefully) a month NAME, which is what our own Format produces
        ' ("Jul 24, 2026"). Without this the control could not re-parse its own output --
        ' every commit would fail and silently revert.
        m = DemoMonthFromName( sText )
        if m = 0 then return false
        d = a : y = b
    else
        return false
    end if

    if y < 100 then y += 2000

    if (m < 1) orelse (m > 12) then return false
    if (d < 1) then return false
    if d > PsDatePicker_DaysInMonth( y, m ) then return false
    if (y < 1601) orelse (y > 9999) then return false

    *pDate = PsDatePicker_MakeDate( y, m, d )
    return true
end function

' TOOLTIP: name the weekday under the cursor.
function PsDatePicker_DemoTooltip( byval t as PSDATEPICKER_TOOLTIPINFO ptr ) as boolean
    if t = 0 then return false
    if t->isValid = false then return false
    dim as long dow = PsDatePicker_DowSunday( t->stDate.wYear, t->stDate.wMonth, t->stDate.wDay )
    dim as DWSTRING names(0 to 6) = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }
    t->outText = names(dow) & ", " & DemoMonthAbbrev(t->stDate.wMonth) & " " & str(t->stDate.wDay)
    return true
end function

' DAY PAINT (row 3): a host renderer that owns the whole cell -- proving a callback can, and
' exercising the tone probe. Draws a filled chip for selected/hot, then the number.
sub PsDatePicker_DemoDayPaint( byval p as PSDATEPICKER_DAYPAINTINFO ptr )
    if p = 0 then exit sub
    dim as PsBufferPaint ptr b = p->b
    if b = 0 then exit sub

    dim as RECT rcFill = p->rcCell
    dim as long inset = (rcFill.bottom - rcFill.top) \ 8
    if inset < 1 then inset = 1
    InflateRect( @rcFill, -inset, -inset )
    dim as long ell = (rcFill.bottom - rcFill.top) \ 3
    if ell < 2 then ell = 2

    if p->isSelected then
        b->SetBackColor( BGR(0,120,215) )
        b->PaintRoundRect( @rcFill, ell )
    elseif p->isHot andalso (p->isDisabled = false) then
        b->SetBackColor( BGR(60,66,78) )
        b->PaintRoundRect( @rcFill, ell )
    end if

    ' Same tiering as the control's defaults, and for the same reason: an adjacent day is
    ' dimmed but CLICKABLE, an out-of-range day is inert. Keep them clearly apart.
    dim as COLORREF clr = BGR(210,214,220)
    if p->isDisabled then
        clr = BGR(56,60,68)          ' inert
    elseif p->isSelected then
        clr = BGR(255,255,255)
    elseif p->isAdjacent then
        clr = BGR(124,131,142)       ' other month, still clickable
    elseif p->isWeekend then
        clr = BGR(150,175,205)
    end if
    b->SetForeColor( clr )
    b->PaintText( str(p->dayNumber), @p->rcCell, DT_CENTER )
end sub


' ========================================================================================
' Callbacks: date changed / dropdown / message
' ========================================================================================
private function PickName( byval hCtl as HWND ) as string
    for i as long = 0 to PICK_COUNT - 1
        if hCtl = ghPick(i) then return str(i)
    next
    return "test"
end function

sub PsDatePicker_DemoDateChanged( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr, byval bHasDate as boolean )
    dim as string s
    if bHasDate andalso (pDate <> 0) then
        s = "datechanged   pick=" & PickName(hCtrl) & "  date=" & _
            str(pDate->wYear) & "-" & str(pDate->wMonth) & "-" & str(pDate->wDay)
    else
        s = "datechanged   pick=" & PickName(hCtrl) & "  (cleared)"
    end if
    print s
end sub

sub PsDatePicker_DemoDropDown( byval hCtrl as HWND, byval isOpen as boolean )
    dim as string s = "dropdown      pick=" & PickName(hCtrl) & "  open=" & str(isOpen)
    print s
end sub


' ========================================================================================
' frmMain paint / layout / procedure
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to PICK_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )
        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right -= margin
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )
    next

    dim as RECT rcRow = RowRect( hwnd, PICK_COUNT )
    dim as RECT rcText = rcRow
    rcText.left  += margin
    rcText.right -= margin
    b.SetColors( theme.ForeColor, theme.BackColor )
    b.SetFont( ghFont(GUIFONT_10) )
    b.PaintText( "Plain PsTextBox   (Tab should reach it, and come back)", @rcText, DT_LEFT )

    b.EndDoubleBuffer()
    function = 0
end function

sub frmMain_PositionControls( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to PICK_COUNT - 1
        if IsWindow(ghPick(i)) = 0 then continue for
        dim as RECT rcRow = RowRect( hwnd, i )
        dim as long iw, ih
        PsDatePicker_GetIdealSize( ghPick(i), iw, ih )
        dim as long x = rc.right - margin - iw
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - ih) \ 2)
        SetWindowPos( ghPick(i), 0, x, y, iw, ih, SWP_NOZORDER )
    next

    if IsWindow(ghPlainBox) then
        dim as RECT rcRow = RowRect( hwnd, PICK_COUNT )
        dim as long w = 160, h = 30
        if pWindow then
            w = pWindow->ScaleX(160)
            h = pWindow->ScaleY(30)
        end if
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - h) \ 2)
        SetWindowPos( ghPlainBox, 0, rc.right - margin - w, y, w, h, SWP_NOZORDER )
    end if
end sub

function frmMain_OnSize( byval hwnd as HWND, byval state as UINT, byval cx as long, byval cy as long ) as LRESULT
    if state <> SIZE_MINIMIZED then
        frmMain_PositionControls( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
    function = 0
end function

function frmMain_OnCreate( byval hwnd as HWND, byval lpCreateStructPtr as LPCREATESTRUCT ) as boolean
    return true
end function

function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow(hwnd)
    function = 0
end function

function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage(0)
    function = 0
end function

function frmMain_WndProc( byval hwnd as HWND, byval uMsg as UINT, byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,        frmMain_OnCreate)
        HANDLE_MSG (hwnd, WM_SIZE,          frmMain_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,         frmMain_OnPaint)
        HANDLE_MSG (hwnd, WM_CLOSE,         frmMain_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,       frmMain_OnDestroy)

        case WM_NCDESTROY
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMMAIN = 0
            return 0

        case WM_ERASEBKGND
            return true

        case WM_SYSCOMMAND
            if (wParam and &HFFF0) = SC_CLOSE then
                SendMessage( hwnd, WM_CLOSE, wParam, lParam )
                exit function
            end if
    end select

    function = DefWindowProc(HWnd, uMsg, wParam, lParam)
end function


' ========================================================================================
' SELF-TEST
' ========================================================================================
private sub SelfTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gTestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gFailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function RectW( byref rc as RECT ) as long
    return rc.right - rc.left
end function
private function RectH( byref rc as RECT ) as long
    return rc.bottom - rc.top
end function
private function CenterOf( byref rc as RECT ) as POINT
    dim as POINT pt
    pt.x = rc.left + (RectW(rc) \ 2)
    pt.y = rc.top + (RectH(rc) \ 2)
    return pt
end function

' Count distinct colours the DEMO's day painter produces inside one cell (inset 2px), driven
' into an offscreen buffer -- the family's flood-fill probe. A flooded cell reads as ONE
' colour; a healthy one has at least a fill and the number.
private function SelfTest_DayCellTones( byval hCal as HWND, byval k as long ) as long
    dim as PSDATEPICKER_CAL ptr pCal = GetDatePickerCalPointer(hCal)
    if pCal = 0 then return 0
    dim as PSDATEPICKER ptr pCtrl = GetDatePickerPointer( pCal->hOwner )
    if pCtrl = 0 then return 0

    dim as RECT rcClient
    GetClientRect( hCal, @rcClient )
    if (rcClient.right <= 0) orelse (rcClient.bottom <= 0) then return 0

    dim as HDC hdc = GetDC(hCal)
    if hdc = 0 then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hCal, hdc, rcClient )
    b.SetBackColor( pCtrl->colors.CalBackColor )
    b.PaintClientRect()
    if pCtrl->hMainFont then b.SetFont( pCtrl->hMainFont )

    dim as SYSTEMTIME st = pCal->CellDate( 0, k )
    dim as PSDATEPICKER_DAYPAINTINFO d
    d.hCtrl      = pCal->hOwner
    d.b          = @b
    d.rcCell     = pCal->rcDayCell(0, k)
    d.stDate     = st
    d.dayNumber  = st.wDay
    d.monthIndex = 0
    d.isAdjacent = false
    d.isWeekend  = false
    d.isToday    = false
    d.isSelected = true               ' the filled state, like NUD's at-max probe
    d.isHot      = false
    d.isDisabled = false
    d.isEnabled  = true

    PsDatePicker_DemoDayPaint( @d )

    dim as HDC memDC = b.getMemDC()
    dim as long nDistinct = 0
    if memDC then
        dim as COLORREF seen(0 to 63)
        dim as RECT rcCell = pCal->rcDayCell(0, k)
        for y as long = rcCell.top + 2 to rcCell.bottom - 3
            for x as long = rcCell.left + 2 to rcCell.right - 3
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


sub PsDatePicker_SelfTest()
    print "==== PsDatePicker self-test ===="

    ' ---- pure date math -------------------------------------------------------------------
    SelfTest_Check( "IsLeapYear: 2000 T, 1900 F, 2024 T, 2021 F", _
        PsDatePicker_IsLeapYear(2000) andalso (PsDatePicker_IsLeapYear(1900) = false) andalso _
        PsDatePicker_IsLeapYear(2024) andalso (PsDatePicker_IsLeapYear(2021) = false), "" )

    SelfTest_Check( "DaysInMonth: leap Feb 29, common Feb 28, Apr 30, Dec 31", _
        (PsDatePicker_DaysInMonth(2024,2)=29) andalso (PsDatePicker_DaysInMonth(2021,2)=28) andalso _
        (PsDatePicker_DaysInMonth(2021,4)=30) andalso (PsDatePicker_DaysInMonth(2021,12)=31), "" )

    SelfTest_Check( "DowSunday: 2021-03-01 Mon(1), 2000-01-01 Sat(6), 2026-07-24 Fri(5)", _
        (PsDatePicker_DowSunday(2021,3,1)=1) andalso (PsDatePicker_DowSunday(2000,1,1)=6) andalso _
        (PsDatePicker_DowSunday(2026,7,24)=5), _
        str(PsDatePicker_DowSunday(2021,3,1)) & "," & str(PsDatePicker_DowSunday(2000,1,1)) & "," & str(PsDatePicker_DowSunday(2026,7,24)) )

    dim as long ry, rm, rd
    PsDatePicker_SerialToDate( PsDatePicker_DateToSerial(2021,3,9), ry, rm, rd )
    SelfTest_Check( "serial round-trip 2021-03-09", (ry=2021) andalso (rm=3) andalso (rd=9), _
        str(ry) & "-" & str(rm) & "-" & str(rd) )
    PsDatePicker_SerialToDate( PsDatePicker_DateToSerial(1601,1,1), ry, rm, rd )
    SelfTest_Check( "serial round-trip 1601-01-01 (epoch floor)", (ry=1601) andalso (rm=1) andalso (rd=1), "" )

    SelfTest_Check( "LeadingCells March 2021: Sunday-start 1, Monday-start 0", _
        (PsDatePicker_LeadingCells(2021,3,DTP_SUNDAY)=1) andalso (PsDatePicker_LeadingCells(2021,3,DTP_MONDAY)=0), "" )

    dim as SYSTEMTIME sA = PsDatePicker_MakeDate(2021,12,15)
    dim as SYSTEMTIME sB = PsDatePicker_AddMonths(sA, 1)
    SelfTest_Check( "AddMonths 2021-12 +1 -> 2022-01", (sB.wYear=2022) andalso (sB.wMonth=1), str(sB.wYear) & "-" & str(sB.wMonth) )
    dim as SYSTEMTIME sC = PsDatePicker_AddMonths( PsDatePicker_MakeDate(2021,1,15), -1 )
    SelfTest_Check( "AddMonths 2021-01 -1 -> 2020-12", (sC.wYear=2020) andalso (sC.wMonth=12), "" )
    dim as SYSTEMTIME sD = PsDatePicker_AddMonths( PsDatePicker_MakeDate(2021,1,31), 1 )
    SelfTest_Check( "AddMonths 2021-01-31 +1 -> 2021-02-28 (day clamp)", (sD.wMonth=2) andalso (sD.wDay=28), str(sD.wMonth) & "/" & str(sD.wDay) )

    SelfTest_Check( "DecadeBase: 2021->2020, 2029->2020, 2030->2030", _
        (PsDatePicker_DecadeBase(2021)=2020) andalso (PsDatePicker_DecadeBase(2029)=2020) andalso (PsDatePicker_DecadeBase(2030)=2030), "" )

    ' ---- Format/Parse round-trip ----------------------------------------------------------
    ' THE REGRESSION THIS EXISTS FOR: the demo's Format produced "Jul 24, 2026" and its Parse
    ' could not read a month NAME, so every commit failed and silently reverted -- the control
    ' could not re-parse its own output. Nothing else in this file would have noticed, because
    ' a silent revert looks exactly like "the user typed nothing".
    dim as SYSTEMTIME rtIn = PsDatePicker_MakeDate( 2026, 7, 24 )
    dim as DWSTRING rtText
    PsDatePicker_DemoFormat( 0, @rtIn, rtText )
    dim as SYSTEMTIME rtOut
    dim as boolean rtOK = PsDatePicker_DemoParse( 0, rtText, @rtOut )
    dim as boolean rtSame = false
    if rtOK then rtSame = (rtOut.wYear=2026) andalso (rtOut.wMonth=7) andalso (rtOut.wDay=24)
    SelfTest_Check( "Format -> Parse round-trips (the control can re-read its own output)", _
        rtOK andalso rtSame, "[" & rtText & "] ok=" & str(rtOK) )

    ' Other input shapes the demo parser advertises.
    dim as SYSTEMTIME p1, p2, p3
    dim as boolean o1 = PsDatePicker_DemoParse( 0, "2026-07-24", @p1 )
    dim as boolean o2 = PsDatePicker_DemoParse( 0, "7/24/2026", @p2 )
    dim as boolean o3 = PsDatePicker_DemoParse( 0, "July 24, 2026", @p3 )
    dim as boolean allSame = false
    if o1 andalso o2 andalso o3 then
        allSame = (p1.wMonth=7) andalso (p1.wDay=24) andalso (p1.wYear=2026) andalso _
                  (p2.wMonth=7) andalso (p2.wDay=24) andalso (p2.wYear=2026) andalso _
                  (p3.wMonth=7) andalso (p3.wDay=24) andalso (p3.wYear=2026)
    end if
    SelfTest_Check( "Parse accepts ISO, slashed and full-month-name forms", allSame, "" )

    dim as SYSTEMTIME pBad
    dim as boolean oBad = PsDatePicker_DemoParse( 0, "not a date", @pBad )
    dim as boolean oBad2 = PsDatePicker_DemoParse( 0, "2026-02-30", @pBad )
    SelfTest_Check( "Parse rejects garbage and impossible dates", (oBad = false) andalso (oBad2 = false), "" )

    ' ---- a throwaway control for geometry ------------------------------------------------
    dim as HWND h = PsDatePicker_Create( HWND_FRMMAIN, IDC_FRMMAIN_PICK_TEST )
    PsDatePicker_SetFont( h, ghFont(GUIFONT_10) )
    PsDatePicker_SetDayPaintCallback( h, @PsDatePicker_DemoDayPaint )

    ' ---- container layout ----------------------------------------------------------------
    SetWindowPos( h, 0, 0, 0, 240, 34, SWP_NOZORDER or SWP_NOMOVE )
    dim as RECT rcClientC
    GetClientRect( h, @rcClientC )
    dim as long cw = rcClientC.right, ch = rcClientC.bottom
    dim as RECT rcFrame, rcField, rcIcon
    dim as boolean gF = PsDatePicker_GetFrameRect( h, rcFrame )
    dim as boolean gL = PsDatePicker_GetFieldRect( h, rcField )
    dim as boolean gI = PsDatePicker_GetIconRect( h, rcIcon )
    SelfTest_Check( "container rect queries succeed once sized", gF andalso gL andalso gI, "" )
    SelfTest_Check( "rcFrame is the whole client", (RectW(rcFrame)=cw) andalso (RectH(rcFrame)=ch), "" )
    SelfTest_Check( "the icon cell ends at the client edge and is iconWidth wide", _
        (rcIcon.right=cw) andalso (RectW(rcIcon)=PsDatePicker_GetIconWidth(h)), str(RectW(rcIcon)) )
    dim as long bth = PsDatePicker_GetBorderThickness(h)
    SelfTest_Check( "the field is inset vertically by the border (child cannot cover it)", _
        (rcField.top=bth) andalso (rcField.bottom=ch-bth), "" )
    SelfTest_Check( "the field ends where the divider begins (left of the icon)", _
        (rcField.right = rcIcon.left - 1) orelse (rcField.right = rcIcon.left), str(rcField.right) & " vs " & str(rcIcon.left) )
    SelfTest_Check( "the embedded PsTextBox exists and is our child", _
        (PsDatePicker_GetTextBoxHandle(h) <> 0) andalso (GetParent(PsDatePicker_GetTextBoxHandle(h)) = h), "" )
    SelfTest_Check( "HitTest finds the icon at its centre, NONE outside", _
        (PsDatePicker_HitTest(h, CenterOf(rcIcon))=1) andalso (PsDatePicker_HitTest(h, type<POINT>(-5,-5))=0), "" )

    ' ---- DAYS geometry, single month, March 2021, Sunday-start ----------------------------
    PsDatePicker_SetWeekStart( h, DTP_SUNDAY )
    PsDatePicker_SetMonthCount( h, 1 )
    dim as HWND hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    dim as PSDATEPICKER_CAL ptr pc = GetDatePickerCalPointer( hCal )
    dim as boolean bHaveCal = (hCal <> 0)
    dim as boolean bHavePc  = (pc <> 0)
    dim as boolean bLaidOut = false
    if bHavePc then bLaidOut = (pc->bLayoutDirty = false)
    SelfTest_Check( "calendar prepared and laid out", bHaveCal andalso bHavePc andalso bLaidOut, "" )

    SelfTest_Check( "header sits at the panel top; prev/title/next tile the header row", _
        (pc->rcHeader(0).top = pc->rcPanel(0).top) andalso (pc->rcPrev.right = pc->rcTitle(0).left) andalso (pc->rcTitle(0).right = pc->rcNext.left), "" )
    SelfTest_Check( "prev chevron cell is nChevW wide", RectW(pc->rcPrev) = pc->nChevW, str(RectW(pc->rcPrev)) )
    SelfTest_Check( "weekday col 0 and day col 0 share the panel's left edge", _
        (pc->rcWeekday(0,0).left = pc->rcPanel(0).left) andalso (pc->rcDayCell(0,0).left = pc->rcBody(0).left), "" )
    SelfTest_Check( "42 day cells: last row bottom = body top + 6*rowH (fixed six rows)", _
        pc->rcDayCell(0,41).bottom = pc->rcBody(0).top + 6 * pc->rowH(0), "" )
    SelfTest_Check( "day cell k tiles by (k mod 7, k\7)", _
        (pc->rcDayCell(0,6).left = pc->rcBody(0).left + 6*pc->cellW(0)) andalso (pc->rcDayCell(0,7).top = pc->rcBody(0).top + pc->rowH(0)), "" )

    ' cell -> date mapping (March 2021, Sunday-start, lead = 1)
    dim as SYSTEMTIME c0  = pc->CellDate(0,0)
    dim as SYSTEMTIME c1  = pc->CellDate(0,1)
    dim as SYSTEMTIME c31 = pc->CellDate(0,31)
    dim as SYSTEMTIME c32 = pc->CellDate(0,32)
    dim as SYSTEMTIME c41 = pc->CellDate(0,41)
    SelfTest_Check( "cell 0 = 2021-02-28 (leading, adjacent)", (c0.wYear=2021) andalso (c0.wMonth=2) andalso (c0.wDay=28), str(c0.wYear) & "-" & str(c0.wMonth) & "-" & str(c0.wDay) )
    SelfTest_Check( "cell 1 = 2021-03-01", (c1.wMonth=3) andalso (c1.wDay=1), "" )
    SelfTest_Check( "cell 31 = 2021-03-31", (c31.wMonth=3) andalso (c31.wDay=31), str(c31.wMonth) & "/" & str(c31.wDay) )
    SelfTest_Check( "cell 32 = 2021-04-01 (trailing, adjacent)", (c32.wMonth=4) andalso (c32.wDay=1), "" )
    SelfTest_Check( "cell 41 = 2021-04-10", (c41.wMonth=4) andalso (c41.wDay=10), str(c41.wMonth) & "/" & str(c41.wDay) )

    ' weekend independent of week start
    SelfTest_Check( "weekend test: 2021-03-06 Sat T, 03-07 Sun T, 03-08 Mon F", _
        (PsDatePicker_DowSunday(2021,3,6)=6) andalso (PsDatePicker_DowSunday(2021,3,7)=0) andalso _
        ((PsDatePicker_DowSunday(2021,3,8)=0) orelse (PsDatePicker_DowSunday(2021,3,8)=6)) = false, "" )

    ' Monday-start rotation
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )   ' still Sunday
    PsDatePicker_SetWeekStart( h, DTP_MONDAY )
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    dim as SYSTEMTIME m0 = pc->CellDate(0,0)
    SelfTest_Check( "Monday-start: cell 0 = 2021-03-01 (lead 0)", (m0.wMonth=3) andalso (m0.wDay=1), str(m0.wMonth) & "/" & str(m0.wDay) )
    PsDatePicker_SetWeekStart( h, DTP_SUNDAY )

    ' Dec 2021 rollovers
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 12 )
    pc = GetDatePickerCalPointer( hCal )
    ' Dec 1 2021 is Wednesday (3) -> lead 3 -> Dec31 at 3+30=33, so cell 34 = Jan 1 2022
    dim as SYSTEMTIME dTrail = pc->CellDate(0,34)
    SelfTest_Check( "Dec->Jan trailing: cell 34 = 2022-01-01", (dTrail.wYear=2022) andalso (dTrail.wMonth=1) andalso (dTrail.wDay=1), str(dTrail.wYear) & "-" & str(dTrail.wMonth) & "-" & str(dTrail.wDay) )

    ' Jan 2021 leading rollover
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 1 )
    pc = GetDatePickerCalPointer( hCal )
    dim as SYSTEMTIME jLead = pc->CellDate(0,0)
    SelfTest_Check( "Jan->Dec leading: cell 0 in 2020-12", (jLead.wYear=2020) andalso (jLead.wMonth=12), str(jLead.wYear) & "-" & str(jLead.wMonth) )

    ' ---- two-month layout, Dec 2021 (right panel = Jan 2022) ------------------------------
    PsDatePicker_SetMonthCount( h, 2 )
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 12 )
    pc = GetDatePickerCalPointer( hCal )
    SelfTest_Check( "two-month: right panel offset by gutter, equal widths", _
        (pc->rcPanel(1).left = pc->rcPanel(0).right + pc->nGutter) andalso (RectW(pc->rcPanel(1)) = RectW(pc->rcPanel(0))), "" )
    SelfTest_Check( "two-month: right panel = anchor+1 = Jan 2022", (pc->panelYear(1)=2022) andalso (pc->panelMonth(1)=1), str(pc->panelYear(1)) & "-" & str(pc->panelMonth(1)) )
    ' Jan 1 2022 is Saturday (6) -> lead 6 -> cell 6 in panel 1 = Jan 1
    dim as SYSTEMTIME p1c6 = pc->CellDate(1,6)
    SelfTest_Check( "two-month: panel 1 cell 6 = 2022-01-01", (p1c6.wYear=2022) andalso (p1c6.wMonth=1) andalso (p1c6.wDay=1), str(p1c6.wYear) & "-" & str(p1c6.wMonth) & "-" & str(p1c6.wDay) )
    dim as DTP_HITRESULT hrT1 = DTP_CalHitTest( pc, CenterOf(pc->rcTitle(1)) )
    SelfTest_Check( "two-month: panel-1 title is NOT interactive (hit-test NONE)", hrT1.part = DTP_HIT_NONE, str(hrT1.part) )
    dim as DTP_HITRESULT hrD1 = DTP_CalHitTest( pc, CenterOf(pc->rcDayCell(1,20)) )
    SelfTest_Check( "two-month: hit-test resolves a panel-1 day cell", (hrD1.part = DTP_HIT_DAYCELL) andalso (hrD1.panel = 1) andalso (hrD1.index = 20), "" )
    PsDatePicker_SetMonthCount( h, 1 )

    ' ---- ideal-size formula: 2-month minus 1-month = one panel + a gutter -----------------
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    dim as long ideal1 = pc->nIdealW
    dim as long cellMinW = pc->nCellMinW
    dim as long gutter = pc->nGutter
    PsDatePicker_SetMonthCount( h, 2 )
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    dim as long ideal2 = pc->nIdealW
    SelfTest_Check( "ideal width: 2-month - 1-month = 7*cellMinW + gutter", _
        (ideal2 - ideal1) = (7 * cellMinW + gutter), str(ideal2 - ideal1) & " vs " & str(7*cellMinW+gutter) )
    PsDatePicker_SetMonthCount( h, 1 )

    ' ---- MONTHS / YEARS drill-down geometry ----------------------------------------------
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_MONTHS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    dim as long mycw = (pc->rcMyGrid.right - pc->rcMyGrid.left) \ CDTP_MY_COLS
    dim as long myrh = (pc->rcMyGrid.bottom - pc->rcMyGrid.top) \ CDTP_MY_ROWS
    SelfTest_Check( "MONTHS grid tiles 3x4", _
        (pc->rcMonthCell(0).left = pc->rcMyGrid.left) andalso (pc->rcMonthCell(2).left = pc->rcMyGrid.left + 2*mycw) andalso (pc->rcMonthCell(3).top = pc->rcMyGrid.top + myrh), "" )
    dim as DTP_HITRESULT hrM = DTP_CalHitTest( pc, CenterOf(pc->rcMonthCell(8)) )
    SelfTest_Check( "MONTHS hit-test resolves cell 8", (hrM.part = DTP_HIT_MONTHCELL) andalso (hrM.index = 8), str(hrM.index) )

    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_YEARS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    dim as DTP_HITRESULT hrY = DTP_CalHitTest( pc, CenterOf(pc->rcYearCell(5)) )
    SelfTest_Check( "YEARS hit-test resolves cell 5", (hrY.part = DTP_HIT_YEARCELL) andalso (hrY.index = 5), str(hrY.index) )
    dim as DTP_HITRESULT hrTitle = DTP_CalHitTest( pc, CenterOf(pc->rcTitle(0)) )
    SelfTest_Check( "the panel-0 title IS interactive (hit-test TITLE)", hrTitle.part = DTP_HIT_TITLE, "" )

    ' ---- chevron disable at range edges ---------------------------------------------------
    dim as SYSTEMTIME rmin = PsDatePicker_MakeDate(2021,3,1)
    dim as SYSTEMTIME rmax = PsDatePicker_MakeDate(2021,3,31)
    PsDatePicker_SetRange( h, rmin, rmax, true, true )
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    SelfTest_Check( "DAYS chevrons: prev disabled (Feb<min), next disabled (Apr>max)", _
        DTP_PrevDisabled(pc) andalso DTP_NextDisabled(pc), "" )

    dim as SYSTEMTIME rmax2 = PsDatePicker_MakeDate(2021,4,30)
    PsDatePicker_SetRange( h, rmin, rmax2, true, true )
    PsDatePicker_SetMonthCount( h, 2 )
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    SelfTest_Check( "two-month next edge: next disabled when revealed month (May) > max(Apr 30)", DTP_NextDisabled(pc), "" )
    dim as SYSTEMTIME rmax3 = PsDatePicker_MakeDate(2021,5,31)
    PsDatePicker_SetRange( h, rmin, rmax3, true, true )
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    SelfTest_Check( "two-month next edge: next enabled when revealed month (May) <= max(May 31)", DTP_NextDisabled(pc) = false, "" )
    PsDatePicker_SetMonthCount( h, 1 )

    ' MONTHS / YEARS chevron disable
    PsDatePicker_SetRange( h, PsDatePicker_MakeDate(2021,1,1), PsDatePicker_MakeDate(2030,12,31), true, true )
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_MONTHS, 2021, 3 )
    pc = GetDatePickerCalPointer( hCal )
    SelfTest_Check( "MONTHS chevron: prev disabled (2020-12-31 < min 2021-01-01)", DTP_PrevDisabled(pc), "" )

    ' ---- day-cell flood probe -------------------------------------------------------------
    PsDatePicker_SetRange( h, rmin, rmax, false, false )   ' clear range so nothing is disabled
    hCal = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    dim as long nTones = SelfTest_DayCellTones( hCal, 15 )
    SelfTest_Check( "the demo day painter does not flood the cell (selected fill + number)", nTones > 1, str(nTones) & " tones" )

    ' ---- the WHOLE calendar renders, not just one cell ------------------------------------
    ' The assertion above passed at 59 tones while the real popup was rendering as a single
    ' flat blue rectangle, because it only ever drove ONE day cell through the host callback
    ' and never ran the full-calendar painter, where the bug was (a fill-and-stroke primitive
    ' used as the popup frame, flooding the client with the last back colour set). These drive
    ' the painter the user actually sees.
    dim as HWND hCalFull = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    dim as long nDayTones = PsDatePicker_TestCalendarTones( hCalFull )
    SelfTest_Check( "the DAYS calendar renders content, not one flat colour", nDayTones > 4, str(nDayTones) & " tones" )

    hCalFull = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_MONTHS, 2021, 3 )
    dim as long nMonTones = PsDatePicker_TestCalendarTones( hCalFull )
    SelfTest_Check( "the MONTHS calendar renders content, not one flat colour", nMonTones > 4, str(nMonTones) & " tones" )

    hCalFull = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_YEARS, 2021, 3 )
    dim as long nYrTones = PsDatePicker_TestCalendarTones( hCalFull )
    SelfTest_Check( "the YEARS calendar renders content, not one flat colour", nYrTones > 4, str(nYrTones) & " tones" )

    ' ---- the optional "Today" footer ------------------------------------------------------
    ' Contract: it is present in EVERY view (escaping a deep drill-down is why it exists), it
    ' takes height from the grid rather than overdrawing it, and clicking it NAVIGATES ONLY --
    ' the selected date and the popup are left alone.
    dim as long ihNoFooter, iwNoFooter
    PsDatePicker_SetShowTodayFooter( h, false )
    dim as HWND hcF = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    dim as PSDATEPICKER_CAL ptr pcF = GetDatePickerCalPointer( hcF )
    iwNoFooter = pcF->nIdealW : ihNoFooter = pcF->nIdealH
    dim as boolean bFootEmptyOff = (IsRectEmpty( @pcF->rcTodayFooter ) <> 0)

    PsDatePicker_SetShowTodayFooter( h, true )
    hcF = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    pcF = GetDatePickerCalPointer( hcF )
    SelfTest_Check( "the Today footer is absent when off, present when on", _
        bFootEmptyOff andalso (IsRectEmpty( @pcF->rcTodayFooter ) = 0), "" )
    SelfTest_Check( "turning the footer on adds exactly its height to the ideal size, and no width", _
        (pcF->nIdealH = ihNoFooter + pcF->nFooterH) andalso (pcF->nIdealW = iwNoFooter), _
        str(ihNoFooter) & " -> " & str(pcF->nIdealH) & " (footerH " & str(pcF->nFooterH) & ")" )
    SelfTest_Check( "the footer sits at the bottom and spans the full inner width", _
        (pcF->rcTodayFooter.bottom = pcF->rcClient.bottom - pcF->nPad) andalso _
        (pcF->rcTodayFooter.left = pcF->nPad) andalso (pcF->rcTodayFooter.right = pcF->rcClient.right - pcF->nPad), "" )
    SelfTest_Check( "the day grid STOPS above the footer (it is not overdrawn)", _
        pcF->rcDayCell(0,41).bottom <= pcF->rcTodayFooter.top, _
        "grid bottom " & str(pcF->rcDayCell(0,41).bottom) & " vs footer top " & str(pcF->rcTodayFooter.top) )
    dim as DTP_HITRESULT hrF = DTP_CalHitTest( pcF, CenterOf(pcF->rcTodayFooter) )
    SelfTest_Check( "hit-test resolves the Today footer", hrF.part = DTP_HIT_TODAY, str(hrF.part) )

    ' ...and it actually draws something. Geometry + a hit-test say nothing about ink: the
    ' whole-popup flood bug proved that. Compare the footer band's tone count against a blank
    ' band -- text present means several tones, an empty strip means one or two.
    dim as long nFootTones = PsDatePicker_TestFooterTones( hcF )
    SelfTest_Check( "the Today footer renders text, not an empty band", nFootTones > 2, str(nFootTones) & " tones" )

    ' present in the drill-down views too -- the whole point of it
    hcF = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_YEARS, 2021, 3 )
    pcF = GetDatePickerCalPointer( hcF )
    dim as DTP_HITRESULT hrF2 = DTP_CalHitTest( pcF, CenterOf(pcF->rcTodayFooter) )
    SelfTest_Check( "the Today footer is reachable from the YEARS view (the escape hatch)", _
        hrF2.part = DTP_HIT_TODAY, str(hrF2.part) )
    SelfTest_Check( "the YEARS grid also stops above the footer", _
        pcF->rcMyGrid.bottom <= pcF->rcTodayFooter.top, "" )

    ' Clicking it SELECTS today and closes -- the same commit path a day cell uses. Asserted
    ' from the OWNER's date (the observable contract), not from the calendar's private copy.
    PsDatePicker_ClearDate( h )
    DTP_CalClick( pcF, CenterOf(pcF->rcTodayFooter) )
    dim as SYSTEMTIME stAfter
    dim as boolean bGotAfter = PsDatePicker_GetDate( h, stAfter )
    dim as boolean bIsToday = false
    if bGotAfter then
        dim as boolean bYM = (stAfter.wYear = pcF->stToday.wYear) andalso (stAfter.wMonth = pcF->stToday.wMonth)
        bIsToday = bYM andalso (stAfter.wDay = pcF->stToday.wDay)
    end if
    SelfTest_Check( "clicking Today COMMITS today as the control's date (from the YEARS view)", _
        bGotAfter andalso bIsToday, _
        "got=" & str(bGotAfter) & " " & str(stAfter.wYear) & "-" & str(stAfter.wMonth) & "-" & str(stAfter.wDay) )
    SelfTest_Check( "clicking Today goes through the pick path, so the popup closes", _
        pcF->bClosingByPick, "" )

    ' inert when today is out of range: no commit, and the view does not move either
    PsDatePicker_ClearDate( h )
    PsDatePicker_SetRange( h, PsDatePicker_MakeDate(1990,1,1), PsDatePicker_MakeDate(1990,12,31), true, true )
    hcF = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_YEARS, 1990, 6 )
    pcF = GetDatePickerCalPointer( hcF )
    dim as long viewBefore = pcF->eView
    DTP_CalClick( pcF, CenterOf(pcF->rcTodayFooter) )
    dim as SYSTEMTIME stOut
    SelfTest_Check( "clicking Today is inert when today is outside the range (no commit, no move)", _
        (PsDatePicker_GetDate( h, stOut ) = false) andalso (pcF->eView = viewBefore), _
        "view stayed " & str(pcF->eView) )
    PsDatePicker_SetRange( h, PsDatePicker_MakeDate(1990,1,1), PsDatePicker_MakeDate(1990,12,31), false, false )
    PsDatePicker_SetShowTodayFooter( h, false )

    ' ---- clickable vs unclickable days must LOOK different --------------------------------
    ' The user question this answers: with a range like "the next 60 days", most of the visible
    ' month is dimmed, and an ADJACENT day (dim, but clickable -- it selects and navigates) sat
    ' only ~34 luminance levels from a DISABLED day (dim, and inert). They read as the same
    ' grey, so the calendar looked broken rather than constrained. Measured at the PIXEL level
    ' through the real painter, because asserting the colour FIELDS would only prove the fields.
    ' March 2021, Sunday-start, range = Mar 10 .. Apr 5:
    '   cell 0  = Feb 28 -> adjacent AND out of range -> disabled wins
    '   cell 12 = Mar 11 -> in month, in range        -> brightest
    '   cell 33 = Apr 2  -> adjacent, IN range        -> mid (clickable)
    '   cell 5  = Mar 4  -> in month, out of range    -> dimmest
    PsDatePicker_SetDayPaintCallback( h, 0 )        ' the CONTROL's painter, not the demo's
    PsDatePicker_SetRange( h, PsDatePicker_MakeDate(2021,3,10), PsDatePicker_MakeDate(2021,4,5), true, true )
    dim as HWND hCalRange = PsDatePicker_TestPrepareCalendar( h, DTP_VIEW_DAYS, 2021, 3 )
    dim as long bInMonth  = PsDatePicker_TestPartBrightness( hCalRange, 100 + 12 )
    dim as long bAdjInRng = PsDatePicker_TestPartBrightness( hCalRange, 100 + 33 )
    dim as long bOutRange = PsDatePicker_TestPartBrightness( hCalRange, 100 + 5 )

    ' A RATIO, not a fixed offset, and calibrated so the REJECTED values fail it: the shipped
    ' colours measured 103 vs 67 (1.54x) and read as the same grey; these measure 132 vs 61
    ' (2.16x). 1.8x sits between, so this assertion would have caught the original defect.
    ' It is a judgement call about "clearly distinguishable", stated as one.
    SelfTest_Check( "an in-range adjacent day renders clearly BRIGHTER (>=1.8x) than an out-of-range day -- both dim, but only one is clickable", _
        bAdjInRng * 10 >= bOutRange * 18, _
        "adjacent=" & str(bAdjInRng) & " outOfRange=" & str(bOutRange) & _
        " ratio=" & str(int(bAdjInRng * 100 / bOutRange)) & "%" )
    SelfTest_Check( "an in-month in-range day renders brightest of the three", _
        (bInMonth > bAdjInRng) andalso (bInMonth > bOutRange), _
        "inMonth=" & str(bInMonth) & " adjacent=" & str(bAdjInRng) & " outOfRange=" & str(bOutRange) )
    PsDatePicker_SetRange( h, PsDatePicker_MakeDate(2021,3,10), PsDatePicker_MakeDate(2021,4,5), false, false )
    PsDatePicker_SetDayPaintCallback( h, @PsDatePicker_DemoDayPaint )

    DestroyWindow( h )

    ' ---- summary --------------------------------------------------------------------------

    ' ====================================================================================
    ' TOOLTIP BACKEND -- the TRACKED shape, on a popup that does not exist yet
    '
    ' The day-cell tip belongs to the CALENDAR POPUP, which is created on first open and
    ' destroyed on close. So the mode has to live on the PICKER: a mode stored on the popup
    ' would revert to the default the second time a user opened the calendar, and nothing
    ' would report it. That is what the last two assertions here are for -- everything else
    ' about this control's tooltip needs a popup and a real cursor over a real day cell.
    ' ====================================================================================
    scope
        dim as HWND hD = PsDatePicker_Create( HWND_FRMMAIN, 9807 )
        if hD = 0 then
            SelfTest_Check( "tooltip: could not create a test picker", false, "Create returned 0" )
        else
            SelfTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsDatePicker_GetTooltipMode( hD ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsDatePicker_GetTooltipMode( hD )) )
            SelfTest_Check( "tooltip: no tooltip window while the calendar has never opened", _
                (PsDatePicker_GetTooltipHandle( hD ) = 0) andalso _
                (PsDatePicker_GetPsTooltipHandle( hD ) = 0), _
                "hSys = " & str(cast(integer, PsDatePicker_GetTooltipHandle( hD ))) )

            dim as boolean bSw = PsDatePicker_SetTooltipMode( hD, PSTIP_MODE_PS )
            SelfTest_Check( "tooltip: the mode can be set before any popup exists", bSw, _
                "returned true with hCal = 0" )
            SelfTest_Check( "tooltip: ...and is remembered on the PICKER, not the popup", _
                (PsDatePicker_GetTooltipMode( hD ) = PSTIP_MODE_PS), _
                "mode = " & str(PsDatePicker_GetTooltipMode( hD )) )
            SelfTest_Check( "tooltip: an invalid mode is refused rather than stored", _
                (PsDatePicker_SetTooltipMode( hD, 99 ) = false) andalso _
                (PsDatePicker_GetTooltipMode( hD ) = PSTIP_MODE_PS), _
                "mode = " & str(PsDatePicker_GetTooltipMode( hD )) )

            DestroyWindow( hD )
        end if
    end scope

    dim as string s = "==== self-test: " & str(gTestCount) & " checks, " & str(gFailCount) & " failed ===="
    print s
end sub


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONT_11)     = pWindow->CreateFont(GUIFONT, 11, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)

    HWND_FRMMAIN = pWindow->Create( null, APPNAME, @frmMain_WndProc, 100, 100, 640, 420 )

    gRows(0).wszLabel = "Date   (single month, ""MMM d, yyyy"")"
    gRows(1).wszLabel = "Date   (two months side by side, + Today footer)"
    gRows(2).wszLabel = "Date   (Monday start; only today .. today+60 selectable)"
    gRows(3).wszLabel = "Date   (empty allowed, custom day paint)"

    for i as long = 0 to PICK_COUNT - 1
        ghPick(i) = PsDatePicker_Create( HWND_FRMMAIN, IDC_FRMMAIN_PICK_FIRST + i )
        PsDatePicker_SetFont( ghPick(i), ghFont(GUIFONT_10) )
        PsDatePicker_SetFormatCallback( ghPick(i), @PsDatePicker_DemoFormat )
        PsDatePicker_SetParseCallback( ghPick(i), @PsDatePicker_DemoParse )
        PsDatePicker_SetDateChangedCallback( ghPick(i), @PsDatePicker_DemoDateChanged )
        PsDatePicker_SetDropDownCallback( ghPick(i), @PsDatePicker_DemoDropDown )
        PsDatePicker_SetTooltipCallback( ghPick(i), @PsDatePicker_DemoTooltip )
    next

    dim as SYSTEMTIME stNow
    GetLocalTime( @stNow )
    dim as SYSTEMTIME today = PsDatePicker_MakeDate( stNow.wYear, stNow.wMonth, stNow.wDay )

    ' Row 0 -- the screenshot's case.
    PsDatePicker_SetDate( ghPick(0), today )

    ' Row 1 -- two months, WITH the Today footer (it spans both panels).
    PsDatePicker_SetMonthCount( ghPick(1), 2 )
    PsDatePicker_SetShowTodayFooter( ghPick(1), true )
    PsDatePicker_SetDate( ghPick(1), today )

    ' Row 2 -- Monday start, range = today .. today+60.
    dim as long y2, m2, d2
    PsDatePicker_SerialToDate( DTP_Serial(today) + 60, y2, m2, d2 )
    dim as SYSTEMTIME plus60 = PsDatePicker_MakeDate( y2, m2, d2 )
    ' Row 2 also shows the footer -- and here today IS in range, so it stays live.
    PsDatePicker_SetShowTodayFooter( ghPick(2), true )
    PsDatePicker_SetWeekStart( ghPick(2), DTP_MONDAY )
    PsDatePicker_SetRange( ghPick(2), today, plus60, true, true )
    PsDatePicker_SetDate( ghPick(2), today )

    ' Row 3 -- empty allowed, custom day paint.
    PsDatePicker_SetAllowEmpty( ghPick(3), true )
    PsDatePicker_SetDayPaintCallback( ghPick(3), @PsDatePicker_DemoDayPaint )
    PsDatePicker_ClearDate( ghPick(3) )

    ' A plain PsTextBox after the pickers -- Tab must reach it through the extra container level.
    ghPlainBox = PsTextBox_Create( HWND_FRMMAIN, IDC_FRMMAIN_TEXTBOX )
    PsTextBox_SetFont( ghPlainBox, ghFont(GUIFONT_10) )
    PsTextBox_SetForeColor( ghPlainBox, theme.ForeColor )
    PsTextBox_SetBackColor( ghPlainBox, theme.BackColorHot )
    PsTextBox_SetBorderColor( ghPlainBox, theme.Divider )
    PsTextBox_SetFocusBorderColor( ghPlainBox, theme.FocusAccent )
    PsTextBox_SetOuterBackColor( ghPlainBox, theme.BackColor )
    PsTextBox_SetCueBannerText( ghPlainBox, "ordinary text" )
    PsTextBox_SetCueBannerColor( ghPlainBox, theme.ForeColorDisabled )

    frmMain_PositionControls( HWND_FRMMAIN )
    for i as long = 0 to PICK_COUNT - 1
        ShowWindow( ghPick(i), SW_SHOW )
    next
    ShowWindow( ghPlainBox, SW_SHOW )

    if environ("PSDATEPICKER_REPRO") = "1" then
        ShowWindow( pWindow->hWindow, SW_SHOW )
        UpdateWindow( pWindow->hWindow )
        SetForegroundWindow( pWindow->hWindow )

        dim as HWND hE0 = PsTextBox_GetRichEditHandle( PsDatePicker_GetTextBoxHandle( ghPick(0) ) )
        dim as HWND hE1 = PsTextBox_GetRichEditHandle( PsDatePicker_GetTextBoxHandle( ghPick(1) ) )
        dim as HWND hE2 = PsTextBox_GetRichEditHandle( PsDatePicker_GetTextBoxHandle( ghPick(2) ) )

        #macro PUMP(n)
            scope
                dim as MSG mm
                for kk as long = 1 to (n)
                    if PeekMessage(@mm, null, 0, 0, PM_REMOVE) then
                        if PsDatePicker_FilterMessage(@mm) = 0 then
                            TranslateMessage @mm
                            DispatchMessage @mm
                        end if
                    end if
                next
            end scope
        #endmacro

        dim as string sStage = environ("PSDATEPICKER_STAGE")
        if len(sStage) = 0 then sStage = "ABC"

        print "[REPRO] start stage=" & sStage
        PUMP(80)
        print "[REPRO] after initial pump"

        if instr(sStage, "A") then
            print "[REPRO] A: focus pick0 field"
            SetFocus( hE0 ) : PUMP(40)
            print "[REPRO] A: move focus to pick1 field (simulates Tab commit)"
            SetFocus( hE1 ) : PUMP(40)
            print "[REPRO] A survived"
        end if

        if instr(sStage, "R") then
            print "[REPRO] R: row-3 config (Monday start, today..today+60)"
            dim as SYSTEMTIME stRnow
            GetLocalTime( @stRnow )
            dim as SYSTEMTIME tR = PsDatePicker_MakeDate( stRnow.wYear, stRnow.wMonth, stRnow.wDay )
            dim as long ry2, rm2, rd2
            PsDatePicker_SerialToDate( PsDatePicker_DateToSerial(tR.wYear,tR.wMonth,tR.wDay) + 60, ry2, rm2, rd2 )
            dim as SYSTEMTIME tR60 = PsDatePicker_MakeDate( ry2, rm2, rd2 )
            print "[REPRO]   range = " & str(tR.wYear) & "-" & str(tR.wMonth) & "-" & str(tR.wDay) & _
                  "  ..  " & str(tR60.wYear) & "-" & str(tR60.wMonth) & "-" & str(tR60.wDay)

            dim as HWND hR = PsDatePicker_GetCalendarHandle( ghPick(2) )
            PsDatePicker_DropDown( ghPick(2) ) : PUMP(60)
            hR = PsDatePicker_GetCalendarHandle( ghPick(2) )
            dim as PSDATEPICKER_CAL ptr pR = GetDatePickerCalPointer( hR )
            if pR then
                print "[REPRO]   anchor=" & str(pR->panelYear(0)) & "-" & str(pR->panelMonth(0)) & _
                      "  lead=" & str(pR->panelLead(0)) & "  weekStart=" & str(pR->nWeekStart)
                print "[REPRO]   PrevDisabled=" & str(DTP_PrevDisabled(pR)) & "  NextDisabled=" & str(DTP_NextDisabled(pR))
                dim as longint sMin = PsDatePicker_DateToSerial(pR->stMin.wYear,pR->stMin.wMonth,pR->stMin.wDay)
                dim as longint sMax = PsDatePicker_DateToSerial(pR->stMax.wYear,pR->stMax.wMonth,pR->stMax.wDay)
                for k as long = 0 to 41
                    dim as SYSTEMTIME cd = pR->CellDate(0,k)
                    dim as longint sc = PsDatePicker_DateToSerial(cd.wYear,cd.wMonth,cd.wDay)
                    dim as boolean bAdj = (cd.wMonth <> pR->panelMonth(0))
                    dim as boolean bOut = (sc < sMin) orelse (sc > sMax)
                    dim as string tag = ""
                    if bAdj then tag &= "ADJ "
                    if bOut then tag &= "OUT-OF-RANGE "
                    if (bAdj = false) andalso (bOut = false) then tag &= "selectable"
                    print "[REPRO]   cell " & str(k) & " = " & str(cd.wYear) & "-" & str(cd.wMonth) & "-" & str(cd.wDay) & "  " & tag
                next
            end if
            ' Measure the rendered chevrons: prev is disabled here, next is not, so the two
            ' must NOT come out the same colour. Brightest pixel in each chevron cell.
            if pR then
                dim as long bp = PsDatePicker_TestPartBrightness( hR, 0 )
                dim as long bn = PsDatePicker_TestPartBrightness( hR, 1 )
                print "[REPRO]   chevron brightness: prev(disabled)=" & str(bp) & "  next(enabled)=" & str(bn)
            end if
            PsDatePicker_CloseUp( ghPick(2) ) : PUMP(20)
            print "[REPRO] R done"
        end if

        if instr(sStage, "B") then
            print "[REPRO] B: DropDown pick0"
            PsDatePicker_DropDown( ghPick(0) ) : PUMP(120)
            print "[REPRO] B: calendar open=" & str(PsDatePicker_IsDroppedDown(ghPick(0)))
            print "[REPRO] B survived"
        end if

        if instr(sStage, "C") then
            print "[REPRO] C: with popup open, focus pick2 field"
            SetFocus( hE2 ) : PUMP(80)
            print "[REPRO] C survived"
        end if

        print "[REPRO] done"
        DestroyWindow( HWND_FRMMAIN )
        return 0
    end if

    if environ("PSDATEPICKER_SELFTEST") = "1" then
        PsDatePicker_SelfTest()
        ' Headless verification: run the assertions and exit rather than entering the GUI
        ' message loop (which would block). The interactive pass is run without the env var.
        DestroyWindow( HWND_FRMMAIN )
        return iif( gFailCount = 0, 0, 1 )
    end if

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    dim uMsg as MSG
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do
        if PsDatePicker_FilterMessage( @uMsg ) then continue do
        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
            TranslateMessage @uMsg
            DispatchMessage @uMsg
        end if
    loop

    function = uMsg.wParam
end function
