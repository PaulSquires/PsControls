'    PsCalendar demo - main form + self-test
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' Self-test counters.
dim shared as long gFailCount = 0
dim shared as long gTestCount = 0

' What the demo reports back from its callbacks, painted along the bottom of the window.
dim shared as DWSTRING gStatus


' ========================================================================================
' Demo layout
' ========================================================================================
#define DEMO_MARGIN      20
#define DEMO_LABELH      22
#define DEMO_STATUSH     28
#define DEMO_BTNW       120
#define DEMO_BTNH        26

dim shared as RECT gRcCal(0 to CAL_COUNT - 1)
dim shared as DWSTRING gCalLabel(0 to CAL_COUNT - 1)


' ========================================================================================
' Demo callbacks
' ========================================================================================
' `format` lives in vbcompat.bi, which nothing else here needs -- so pad by hand rather than
' pull in a header for three calls.
private function DemoPad( byval n as long, byval nWidth as long ) as DWSTRING
    dim as DWSTRING s = str(n)
    while len(s) < nWidth
        s = "0" & s
    wend
    return s
end function

private function DemoDateText( byval p as SYSTEMTIME ptr ) as DWSTRING
    if p = 0 then return "(none)"
    return DemoPad(p->wYear, 4) & "-" & DemoPad(p->wMonth, 2) & "-" & DemoPad(p->wDay, 2)
end function

private function DemoWhich( byval hCtrl as HWND ) as DWSTRING
    for i as long = 0 to CAL_COUNT - 1
        if ghCal(i) = hCtrl then return "cal" & str(i)
    next
    return "cal?"
end function

private sub PsCalendar_DemoSelChange( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr, byval bHasDate as boolean )
    if bHasDate then
        gStatus = DemoWhich(hCtrl) & "  SelChange -> " & DemoDateText(pDate)
    else
        gStatus = DemoWhich(hCtrl) & "  SelChange -> (cleared)"
    end if
    if HWND_FRMMAIN then InvalidateRect( HWND_FRMMAIN, NULL, TRUE )
end sub

private sub PsCalendar_DemoDateActivated( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr )
    gStatus = DemoWhich(hCtrl) & "  ACTIVATED -> " & DemoDateText(pDate) & "   (double-click, Enter or Space)"
    if HWND_FRMMAIN then InvalidateRect( HWND_FRMMAIN, NULL, TRUE )
end sub

' A tooltip over any day of the current month.
private function PsCalendar_DemoTooltip( byval t as PSCALENDAR_TOOLTIPINFO ptr ) as boolean
    if t = 0 then return false
    if t->isValid = false then return false
    t->outText = "Day " & str(t->stDate.wDay) & " of month " & str(t->stDate.wMonth)
    return true
end function

' A host day painter: shade every Friday, and mark the 1st and the 15th with a dot under the
' number. Deliberately does NOT draw a focus ring -- the control draws that itself, after this
' callback returns, which is what stops a host painter from making keyboard focus invisible.
private sub PsCalendar_DemoDayPaint( byval p as PSCALENDAR_DAYPAINTINFO ptr )
    if p = 0 then return
    dim as PsBufferPaint ptr b = p->b
    if b = 0 then return

    dim as RECT rcFill = p->rcCell
    dim as long inset = (p->rcCell.bottom - p->rcCell.top) \ 8
    if inset < 1 then inset = 1
    InflateRect( @rcFill, -inset, -inset )
    dim as long ell = (rcFill.bottom - rcFill.top) \ 3
    if ell < 2 then ell = 2

    dim as long dow = PsCalendar_DowSunday( p->stDate.wYear, p->stDate.wMonth, p->stDate.wDay )
    dim as boolean bFriday = (dow = 5) andalso (p->isAdjacent = false)

    if p->isSelected then
        b->SetBackColor( BGR(0,120,215) )
        b->PaintRoundRect( @rcFill, ell )
    elseif p->isHot andalso (p->isDisabled = false) then
        b->SetBackColor( BGR(52,58,68) )
        b->PaintRoundRect( @rcFill, ell )
    elseif bFriday then
        b->SetBackColor( BGR(44,52,44) )
        b->PaintRoundRect( @rcFill, ell )
    end if

    dim as COLORREF clr = BGR(210,214,220)
    if p->isDisabled then
        clr = BGR(56,60,68)
    elseif p->isSelected then
        clr = BGR(255,255,255)
    elseif p->isHot then
        clr = BGR(255,255,255)
    elseif p->isAdjacent then
        clr = BGR(124,131,142)
    end if
    b->SetForeColor( clr )
    b->PaintText( str(p->dayNumber), @p->rcCell, DT_CENTER )

    dim as boolean bDay1  = (p->stDate.wDay = 1)
    dim as boolean bDay15 = (p->stDate.wDay = 15)
    dim as boolean bOwnMonth = (p->isAdjacent = false)
    dim as boolean bMark = (bDay1 orelse bDay15) andalso bOwnMonth
    if bMark andalso (p->isDisabled = false) then
        dim as long cx = (p->rcCell.left + p->rcCell.right) \ 2
        dim as long yy = p->rcCell.bottom - inset - 2
        dim as RECT rcDot
        SetRect( @rcDot, cx - 2, yy - 2, cx + 2, yy + 2 )
        b->SetBackColor( BGR(230,160, 60) )
        b->PaintEllipse( @rcDot )
    end if
end sub


' ========================================================================================
' Form painting / layout
' ========================================================================================
private sub frmMain_PositionControls( byval hwnd as HWND )
    if hwnd = 0 then exit sub
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow = 0 then exit sub

    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )
    dim as long M  = pWindow->ScaleX(DEMO_MARGIN)
    dim as long LH = pWindow->ScaleY(DEMO_LABELH)
    dim as long SH = pWindow->ScaleY(DEMO_STATUSH)
    dim as long W  = rcClient.right
    dim as long H  = rcClient.bottom

    ' cal0 and cal1 sit at their IDEAL size, side by side, under their labels.
    dim as long w0, h0, w1, h1
    PsCalendar_GetIdealSize( ghCal(0), w0, h0 )
    PsCalendar_GetIdealSize( ghCal(1), w1, h1 )

    dim as long y0 = M + LH
    SetRect( @gRcCal(0), M, y0, M + w0, y0 + h0 )
    dim as long x1 = gRcCal(0).right + M
    SetRect( @gRcCal(1), x1, y0, x1 + w1, y0 + h1 )

    dim as long topH = h0
    if h1 > topH then topH = h1

    ' cal2 takes whatever is left, which is normally BELOW its ideal size -- deliberately, so
    ' the demo exercises the stretch path rather than only the size-to-fit one.
    dim as long y2 = y0 + topH + M + LH
    dim as long btnRow = H - M - pWindow->ScaleY(DEMO_BTNH)
    dim as long bottom = btnRow - M - SH
    if bottom < y2 + 40 then bottom = y2 + 40
    SetRect( @gRcCal(2), M, y2, W - M, bottom )

    for i as long = 0 to CAL_COUNT - 1
        SetWindowPos( ghCal(i), NULL, gRcCal(i).left, gRcCal(i).top, _
                      gRcCal(i).right - gRcCal(i).left, gRcCal(i).bottom - gRcCal(i).top, _
                      SWP_NOZORDER or SWP_NOACTIVATE )
    next

    dim as long bw = pWindow->ScaleX(DEMO_BTNW)
    dim as long bh = pWindow->ScaleY(DEMO_BTNH)
    SetWindowPos( ghBtnBefore, NULL, M, btnRow, bw, bh, SWP_NOZORDER or SWP_NOACTIVATE )
    SetWindowPos( ghBtnAfter,  NULL, M + bw + M, btnRow, bw, bh, SWP_NOZORDER or SWP_NOACTIVATE )
end sub

private sub frmMain_OnPaint( byval hwnd as HWND )
    ' NO BeginPaint HERE. PsBufferPaint.BeginDoubleBuffer(hwnd) calls BeginPaint itself, so
    ' an outer BeginPaint/EndPaint pair validates the update region first and the buffer then
    ' paints into an empty rect: the form comes up with a white background and none of its
    ' labels. Nothing in a build or an assertion can see that -- it took looking at the render.
    dim as RECT rcClient
    GetClientRect( hwnd, @rcClient )

    dim as PsBufferPaint b
    b.BeginDoubleBuffer( hwnd )
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long LH = 22
    if pWindow then LH = pWindow->ScaleY(DEMO_LABELH)

    b.SetFont( ghFont(GUIFONTBOLD_10) )
    b.SetForeColor( theme.ForeColorHot )
    for i as long = 0 to CAL_COUNT - 1
        ' The label rect runs to the CLIENT edge, not to the calendar's right edge. Bounded by
        ' the control it describes, the middle caption was silently clipped mid-word --
        ' DrawText truncates without complaining, and no assertion can see it.
        dim as RECT rcLab
        SetRect( @rcLab, gRcCal(i).left, gRcCal(i).top - LH, rcClient.right, gRcCal(i).top )
        b.PaintText( gCalLabel(i), @rcLab, DT_LEFT )
    next

    dim as long M = 20
    dim as long SH = 28
    if pWindow then
        M  = pWindow->ScaleX(DEMO_MARGIN)
        SH = pWindow->ScaleY(DEMO_STATUSH)
    end if
    dim as RECT rcStatus
    SetRect( @rcStatus, M, gRcCal(2).bottom, rcClient.right - M, gRcCal(2).bottom + SH )
    b.SetFont( ghFont(GUIFONT_10) )
    b.SetForeColor( theme.FocusAccent )
    b.PaintText( gStatus, @rcStatus, DT_LEFT )

    b.EndDoubleBuffer()
end sub

function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg

    case WM_PAINT
        frmMain_OnPaint( hwnd )
        return 0

    case WM_ERASEBKGND
        return true

    case WM_SIZE
        frmMain_PositionControls( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
        return 0

    case WM_CTLCOLORBTN, WM_CTLCOLORSTATIC
        SetBkMode( cast(HDC, wParam), TRANSPARENT )
        SetTextColor( cast(HDC, wParam), theme.ForeColor )
        return cast( LRESULT, GetStockObject(NULL_BRUSH) )

    case WM_GETMINMAXINFO
        dim as MINMAXINFO ptr pmm = cast(MINMAXINFO ptr, lParam)
        if pmm then
            pmm->ptMinTrackSize.x = 720
            pmm->ptMinTrackSize.y = 560
        end if
        return 0

    case WM_CLOSE
        DestroyWindow( hwnd )
        return 0

    case WM_DESTROY
        PostQuitMessage(0)
        return 0

    case WM_NCDESTROY
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete pWindow
        return 0

    end select

    function = DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


declare sub PsCalendar_SelfTest()
' Shared with the REPRO harness below, which runs before the self-test's own definitions.
declare function CenterOf( byref rc as RECT ) as POINT
declare function DateStr( byref st as SYSTEMTIME ) as string


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONT_11)     = pWindow->CreateFont(GUIFONT, 11, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)
    ghFont(GUIFONTBOLD_11) = pWindow->CreateFont(GUIFONT, 11, FW_HEAVY)

    HWND_FRMMAIN = pWindow->Create( null, APPNAME, @frmMain_WndProc, 80, 60, 1120, 800, _
                    WS_OVERLAPPEDWINDOW or WS_CLIPCHILDREN or WS_CLIPSIBLINGS, _
                    WS_EX_CONTROLPARENT )

    gCalLabel(0) = "Single month  (border on, Sunday start)"
    gCalLabel(1) = "Two months, sliding  (SetNavStep(1), Monday start, a live range, tooltips)"
    ' One ROW of panels, and the reason is worth recording. The panel cap allows twelve, and
    ' this started at 3 x 4: at 175% DPI that grid wants ~1300px of height, the demo window
    ' has ~540 to give it, and it duly laid out with NINE-PIXEL day rows. Not a bug -- cells
    ' stretch and every assertion passed -- but the demo's centrepiece was unreadable. 3 x 2
    ' was still cramped. Panel ROWS are what cost height, so one row of three reads properly
    ' and still shows everything this calendar is for.
    gCalLabel(2) = "Three months  (custom day painter, French month names)"

    ' A plain Win32 BUTTON BEFORE the calendars in the tab order, and another AFTER. Two of
    ' them, on purpose: a form made only of the control under test is exactly the shape that
    ' hid PsComboBox's Tab bug, because there was nothing to Tab to.
    ghBtnBefore = CreateWindowEx( 0, "BUTTON", "Before (Tab)", _
                    WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                    0, 0, 0, 0, HWND_FRMMAIN, cast(HMENU, IDC_FRMMAIN_BTN_BEFORE), _
                    GetModuleHandle(null), null )
    SendMessageW( ghBtnBefore, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), true )

    for i as long = 0 to CAL_COUNT - 1
        ghCal(i) = PsCalendar_Create( HWND_FRMMAIN, IDC_FRMMAIN_CAL_FIRST + i )
        PsCalendar_SetFont( ghCal(i), ghFont(GUIFONT_10) )
        PsCalendar_SetTitleFont( ghCal(i), ghFont(GUIFONTBOLD_11) )
        PsCalendar_SetWeekdayFont( ghCal(i), ghFont(GUIFONT_9) )
        PsCalendar_SetSelChangeCallback( ghCal(i), @PsCalendar_DemoSelChange )
        PsCalendar_SetDateActivatedCallback( ghCal(i), @PsCalendar_DemoDateActivated )
    next

    dim as SYSTEMTIME stNow
    GetLocalTime( @stNow )
    dim as SYSTEMTIME today = PsCalendar_MakeDate( stNow.wYear, stNow.wMonth, stNow.wDay )

    ' --- cal 0: the plainest possible calendar, plus the optional frame ---
    PsCalendar_SetBorderThickness( ghCal(0), 1 )
    PsCalendar_SetSelDate( ghCal(0), today )

    ' --- cal 1: two months side by side, stepping ONE month at a time (the range-picker
    '     idiom), Monday start, a live range, and tooltips.
    PsCalendar_SetPanelGrid( ghCal(1), 2, 1 )
    PsCalendar_SetNavStep( ghCal(1), 1 )
    PsCalendar_SetWeekStart( ghCal(1), CAL_MONDAY )
    PsCalendar_SetTooltipCallback( ghCal(1), @PsCalendar_DemoTooltip )
    dim as SYSTEMTIME stLo = PsCalendar_AddDays( today, -10 )
    dim as SYSTEMTIME stHi = PsCalendar_AddDays( today,  45 )
    PsCalendar_SetRange( ghCal(1), stLo, stHi, true, true )
    PsCalendar_SetSelDate( ghCal(1), today )

    ' --- cal 2: three months on one surface, host-painted days, localized month names ---
    PsCalendar_SetPanelGrid( ghCal(2), 3, 1 )
    PsCalendar_SetDayPaintCallback( ghCal(2), @PsCalendar_DemoDayPaint )
    PsCalendar_SetAnchorDate( ghCal(2), PsCalendar_MakeDate( today.wYear, 1, 1 ) )
    dim as DWSTRING sFr(1 to 12) = { "Janvier", "Fevrier", "Mars", "Avril", "Mai", "Juin", _
                                     "Juillet", "Aout", "Septembre", "Octobre", "Novembre", "Decembre" }
    dim as DWSTRING sFrAb(1 to 12) = { "Jan", "Fev", "Mar", "Avr", "Mai", "Jun", _
                                       "Jui", "Aou", "Sep", "Oct", "Nov", "Dec" }
    dim as DWSTRING sFrDw(0 to 6) = { "Di", "Lu", "Ma", "Me", "Je", "Ve", "Sa" }
    PsCalendar_SetMonthNames( ghCal(2), sFr() )
    PsCalendar_SetMonthAbbrevs( ghCal(2), sFrAb() )
    PsCalendar_SetWeekdayAbbrevs( ghCal(2), sFrDw() )

    ghBtnAfter = CreateWindowEx( 0, "BUTTON", "After (Tab)", _
                    WS_CHILD or WS_VISIBLE or WS_TABSTOP or BS_PUSHBUTTON, _
                    0, 0, 0, 0, HWND_FRMMAIN, cast(HMENU, IDC_FRMMAIN_BTN_AFTER), _
                    GetModuleHandle(null), null )
    SendMessageW( ghBtnAfter, WM_SETFONT, cast(WPARAM, ghFont(GUIFONT_10)), true )

    gStatus = "Click a day, double-click or press Enter to activate. Tab moves between the two buttons and the three calendars."

    frmMain_PositionControls( HWND_FRMMAIN )
    for i as long = 0 to CAL_COUNT - 1
        ShowWindow( ghCal(i), SW_SHOW )
    next

    ' ------------------------------------------------------------------------------------
    ' PSCALENDAR_REPRO=1 -- headless crash harness.
    '
    ' Drives the prev chevron on the three-month calendar over and over, forcing a REAL
    ' render (CAL_RenderFull, the same composition WM_PAINT produces) after every click, and
    ' printing progress so a crash names the iteration it died on. PSCALENDAR_REPROCOUNT
    ' overrides the iteration count.
    ' ------------------------------------------------------------------------------------
    if environ("PSCALENDAR_REPRO") = "1" then
        dim as long nIter = valint( environ("PSCALENDAR_REPROCOUNT") )
        if nIter <= 0 then nIter = 20000

        dim as PSCALENDAR ptr pR = GetCalendarPointer( ghCal(2) )
        dim as RECT rcPrevR
        PsCalendar_GetPrevRect( ghCal(2), rcPrevR )
        dim as POINT ptPrev = CenterOf( rcPrevR )
        print "[REPRO] hammering prev on cal2 (" & str(PsCalendar_GetPanelCount(ghCal(2))) & _
              " panels, step " & str(CAL_EffStep(pR)) & " months) for " & str(nIter) & " clicks"

        ' The window is SHOWN, deliberately. UpdateWindow only generates WM_PAINT for a visible
        ' window, and WM_PAINT is the path under suspicion: CAL_Paint enters the buffer through
        ' BeginDoubleBuffer(hwnd), which calls BeginPaint, while the offscreen probes enter
        ' through the (hwnd, hdc, rect) overload. A leak in one would be invisible to the other.
        ShowWindow( HWND_FRMMAIN, SW_SHOW )
        UpdateWindow( HWND_FRMMAIN )

        ' Reproduce the state a REAL clicking user is in, not just the click itself:
        '   focus  -- WM_LBUTTONDOWN calls SetFocus, so the control is focused from the first
        '             click onward. Without this the renderer never calls CAL_FocusCell at all
        '             and the whole focus-ring path goes untested, which is how the first
        '             version of this harness ran 12000 clean iterations while proving less
        '             than it looked.
        '   hot    -- the cursor is sitting on the prev chevron the entire time.
        ' PSCALENDAR_REPROMODE isolates the two variables, because changing both at once is how
        ' this harness produced a crash it could not attribute:
        '   F = give the control focus (so the renderer runs CAL_FocusCell and draws the ring)
        '   P = pump messages between clicks (so WM_TIMER hot-tracking and the form's own
        '       WM_PAINT run, as they do for a real user)
        dim as string sMode = environ("PSCALENDAR_REPROMODE")
        if len(sMode) = 0 then sMode = "FP"
        dim as boolean bFocus = (instr(sMode, "F") > 0)
        dim as boolean bPump  = (instr(sMode, "P") > 0)
        print "[REPRO] mode=" & sMode & "  focus=" & str(bFocus) & " pump=" & str(bPump)

        if bFocus then SetFocus( ghCal(2) )

        for i as long = 1 to nIter
            if bFocus then
                pR->isFocused = true
                pR->hotPart   = CAL_HIT_PREV
                pR->hotPanel  = -1
                pR->hotIndex  = -1
            end if
            CAL_Click( pR, ptPrev, false )
            UpdateWindow( ghCal(2) )            ' forces the REAL WM_PAINT, synchronously

            if bPump then
                ' Let the hot-track timer and anything else queued actually run.
                dim as MSG mm
                while PeekMessage( @mm, null, 0, 0, PM_REMOVE )
                    TranslateMessage @mm
                    DispatchMessage @mm
                wend
            end if
            if (i mod 200) = 0 then
                dim as long nGdi  = GetGuiResources( GetCurrentProcess(), 0 )   ' GR_GDIOBJECTS
                dim as long nUser = GetGuiResources( GetCurrentProcess(), 1 )   ' GR_USEROBJECTS
                dim as string sL = "[REPRO] " & str(i) & "  anchor=" & DateStr(pR->stAnchor) & _
                    "  title=[" & PsCalendar_GetTitleText(ghCal(2)) & "]" & _
                    "  gdi=" & str(nGdi) & " user=" & str(nUser)
                print sL
            end if
        next
        print "[REPRO] survived " & str(nIter) & " clicks"
        DestroyWindow( HWND_FRMMAIN )
        return 0
    end if

    if environ("PSCALENDAR_SELFTEST") = "1" then
        PsCalendar_SelfTest()
        ' Headless verification: run the assertions and exit rather than entering the GUI
        ' message loop (which would block). The interactive pass is run without the env var.
        DestroyWindow( HWND_FRMMAIN )
        return iif( gFailCount = 0, 0, 1 )
    end if

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' IsDialogMessage is what makes Tab move between controls. PsCalendar itself adds NO pump
    ' obligation -- there is no PsCalendar_FilterMessage to call.
    dim uMsg as MSG
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do
        if IsDialogMessage( pWindow->hWindow, @uMsg ) then continue do
        TranslateMessage @uMsg
        DispatchMessage @uMsg
    loop

    function = uMsg.wParam
end function


' ========================================================================================
' SELF-TEST                                    set PSCALENDAR_SELFTEST=1 && main.exe
'
' Runs headless: the form is built but never shown and no message pump ever runs, so the
' process exit code is the pass/fail signal. Everything compiles into ONE module, so the
' control's `private` helpers are reachable from here -- which is what lets the keyboard and
' the nav arithmetic be asserted directly instead of through synthesized input.
' ========================================================================================
private sub SelfTest_Check( byref sName as string, byval bOK as boolean, byref sDetail as string = "" )
    gTestCount += 1
    dim as string sLine
    if bOK then
        sLine = "PASS  " & sName
    else
        gFailCount += 1
        sLine = "FAIL  " & sName
    end if
    if len(sDetail) then sLine &= "   [" & sDetail & "]"
    print sLine
end sub

private function RectW( byref rc as RECT ) as long
    return rc.right - rc.left
end function
private function RectH( byref rc as RECT ) as long
    return rc.bottom - rc.top
end function
private function CenterOf( byref rc as RECT ) as POINT
    dim as POINT pt
    pt.x = (rc.left + rc.right) \ 2
    pt.y = (rc.top + rc.bottom) \ 2
    return pt
end function
private function DateStr( byref st as SYSTEMTIME ) as string
    return str(st.wYear) & "-" & str(st.wMonth) & "-" & str(st.wDay)
end function

' Build a calendar, size it to its own ideal, and force the layout -- without ever showing it.
private function MakeTestCal( byval nCols as long, byval nRows as long, byval y as long, byval m as long ) as HWND
    dim as HWND h = PsCalendar_Create( HWND_FRMMAIN, 9000 )
    if h = 0 then return 0
    PsCalendar_SetFont( h, ghFont(GUIFONT_10) )
    PsCalendar_SetPanelGrid( h, nCols, nRows )
    PsCalendar_SetAnchorDate( h, PsCalendar_MakeDate( y, m, 1 ) )
    dim as long w, hh
    PsCalendar_GetIdealSize( h, w, hh )
    SetWindowPos( h, NULL, 0, 0, w, hh, SWP_NOMOVE or SWP_NOACTIVATE or SWP_NOZORDER )
    dim as PSCALENDAR ptr p = GetCalendarPointer(h)
    if p then p->LayoutCalendar()
    return h
end function

' Notification counters, so "silent setter" and "user action notifies" are measured rather
' than assumed.
dim shared as long gCbSel = 0, gCbAct = 0
dim shared as SYSTEMTIME gCbSelDate, gCbActDate
private sub SelfTest_OnSelChange( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr, byval bHasDate as boolean )
    gCbSel += 1
    if pDate then gCbSelDate = *pDate
end sub
private sub SelfTest_OnActivated( byval hCtrl as HWND, byval pDate as SYSTEMTIME ptr )
    gCbAct += 1
    if pDate then gCbActDate = *pDate
end sub

' Census callbacks. These do not paint -- they count what the renderer HANDED them, which is
' the only way to see per-cell flags from outside the control.
dim shared as long gCensusSel, gCensusFocus, gCensusCells
dim shared as long gCensusSelPanel, gCensusSelIndex
private sub SelfTest_CensusDayPaint( byval p as PSCALENDAR_DAYPAINTINFO ptr )
    if p = 0 then return
    gCensusCells += 1
    if p->isSelected then
        gCensusSel += 1
        gCensusSelPanel = p->panelIndex
        gCensusSelIndex = p->cellIndex
    end if
    if p->isFocused then gCensusFocus += 1
end sub

' Drive one full render and count the cells the renderer marked.
private sub SelfTest_Census( byval hCtrl as HWND )
    gCensusSel = 0 : gCensusFocus = 0 : gCensusCells = 0
    gCensusSelPanel = -1 : gCensusSelIndex = -1
    PsCalendar_SetDayPaintCallback( hCtrl, @SelfTest_CensusDayPaint )
    PsCalendar_TestPartTones( hCtrl, CAL_PART_PREV )   ' renders the WHOLE control
    PsCalendar_SetDayPaintCallback( hCtrl, 0 )
end sub

' A day painter that draws NOTHING but a flat fill over the whole cell. Used to prove the tone
' probe can actually see a destroyed render, rather than trusting that it would.
private sub SelfTest_FloodDayPaint( byval p as PSCALENDAR_DAYPAINTINFO ptr )
    if p = 0 then return
    if p->b = 0 then return
    p->b->SetBackColor( BGR(80,80,80) )
    p->b->PaintRect( @p->rcCell )
end sub


sub PsCalendar_SelfTest()

    print ""
    print "==== PsCalendar self-test ===="

    ' ------------------------------------------------------------------------------------
    ' A. Pure date math
    ' ------------------------------------------------------------------------------------
    print "-- date math --"
    SelfTest_Check( "IsLeapYear: 2024 yes, 2100 no, 2000 yes", _
        PsCalendar_IsLeapYear(2024) andalso (PsCalendar_IsLeapYear(2100) = false) andalso PsCalendar_IsLeapYear(2000) )
    SelfTest_Check( "DaysInMonth: Feb 2024 = 29", PsCalendar_DaysInMonth(2024,2) = 29 )
    SelfTest_Check( "DaysInMonth: Feb 2023 = 28", PsCalendar_DaysInMonth(2023,2) = 28 )
    SelfTest_Check( "DaysInMonth: Apr = 30, Jul = 31", _
        (PsCalendar_DaysInMonth(2023,4) = 30) andalso (PsCalendar_DaysInMonth(2023,7) = 31) )
    SelfTest_Check( "serial epoch: 1970-01-01 = 0", PsCalendar_DateToSerial(1970,1,1) = 0 )

    dim as long ry, rm, rd
    PsCalendar_SerialToDate( PsCalendar_DateToSerial(1601,1,1), ry, rm, rd )
    SelfTest_Check( "serial round trip: 1601-01-01 (a NEGATIVE serial, the era branch)", _
        (ry = 1601) andalso (rm = 1) andalso (rd = 1), str(ry) & "-" & str(rm) & "-" & str(rd) )
    PsCalendar_SerialToDate( PsCalendar_DateToSerial(2400,2,29), ry, rm, rd )
    SelfTest_Check( "serial round trip: 2400-02-29 (a 400-year leap day)", _
        (ry = 2400) andalso (rm = 2) andalso (rd = 29), str(ry) & "-" & str(rm) & "-" & str(rd) )

    SelfTest_Check( "DowSunday: 1970-01-01 is a Thursday (4)", PsCalendar_DowSunday(1970,1,1) = 4 )
    SelfTest_Check( "DowSunday: 2000-01-01 is a Saturday (6)", PsCalendar_DowSunday(2000,1,1) = 6 )
    SelfTest_Check( "DowSunday: 2021-03-01 is a Monday (1)", PsCalendar_DowSunday(2021,3,1) = 1 )

    SelfTest_Check( "LeadingCells: Mar 2021, Sunday start = 1", PsCalendar_LeadingCells(2021,3,CAL_SUNDAY) = 1 )
    SelfTest_Check( "LeadingCells: Mar 2021, Monday start = 0", PsCalendar_LeadingCells(2021,3,CAL_MONDAY) = 0 )
    SelfTest_Check( "LeadingCells: Aug 2021 (1st is a Sunday), Monday start = 6", PsCalendar_LeadingCells(2021,8,CAL_MONDAY) = 6 )

    dim as SYSTEMTIME stJan31 = PsCalendar_MakeDate(2021,1,31)
    dim as SYSTEMTIME stFeb   = PsCalendar_AddMonths( stJan31, 1 )
    SelfTest_Check( "AddMonths CLAMPS the day: 2021-01-31 +1m = 2021-02-28", _
        (stFeb.wYear = 2021) andalso (stFeb.wMonth = 2) andalso (stFeb.wDay = 28), DateStr(stFeb) )
    dim as SYSTEMTIME stBack = PsCalendar_AddMonths( PsCalendar_MakeDate(2021,1,15), -1 )
    SelfTest_Check( "AddMonths back over a year boundary: 2021-01-15 -1m = 2020-12-15", _
        (stBack.wYear = 2020) andalso (stBack.wMonth = 12) andalso (stBack.wDay = 15), DateStr(stBack) )
    dim as SYSTEMTIME stRoll = PsCalendar_AddDays( PsCalendar_MakeDate(2020,12,31), 1 )
    SelfTest_Check( "AddDays over a year boundary: 2020-12-31 +1d = 2021-01-01", _
        (stRoll.wYear = 2021) andalso (stRoll.wMonth = 1) andalso (stRoll.wDay = 1), DateStr(stRoll) )
    dim as SYSTEMTIME stLeap = PsCalendar_AddDays( PsCalendar_MakeDate(2024,2,28), 1 )
    SelfTest_Check( "AddDays onto a leap day: 2024-02-28 +1d = 2024-02-29", _
        (stLeap.wMonth = 2) andalso (stLeap.wDay = 29), DateStr(stLeap) )
    SelfTest_Check( "DecadeBase: 2026 / 2020 / 2029 all = 2020", _
        (PsCalendar_DecadeBase(2026) = 2020) andalso (PsCalendar_DecadeBase(2020) = 2020) andalso (PsCalendar_DecadeBase(2029) = 2020) )

    ' ------------------------------------------------------------------------------------
    ' B. Ideal size
    ' ------------------------------------------------------------------------------------
    print "-- ideal size --"
    dim as HWND hFresh = PsCalendar_Create( HWND_FRMMAIN, 9100 )
    dim as long fw, fh
    PsCalendar_GetIdealSize( hFresh, fw, fh )
    SelfTest_Check( "GetIdealSize answers BEFORE the control has ever been sized", _
        (fw > 0) andalso (fh > 0), str(fw) & "x" & str(fh) )

    dim as HWND hI11 = MakeTestCal( 1, 1, 2021, 3 )
    dim as HWND hI21 = MakeTestCal( 2, 1, 2021, 3 )
    dim as HWND hI31 = MakeTestCal( 3, 1, 2021, 3 )
    dim as HWND hI22 = MakeTestCal( 2, 2, 2021, 3 )
    dim as long w11, h11, w21, h21, w31, h31, w22, h22
    PsCalendar_GetIdealSize( hI11, w11, h11 )
    PsCalendar_GetIdealSize( hI21, w21, h21 )
    PsCalendar_GetIdealSize( hI31, w31, h31 )
    PsCalendar_GetIdealSize( hI22, w22, h22 )

    dim as PSCALENDAR ptr pI = GetCalendarPointer( hI21 )
    dim as long cmw = pI->nCellMinW, cmh = pI->nCellMinH
    dim as long gx = pI->nGutterX, gy = pI->nGutterY
    dim as long capH = pI->nCaptionH, wkH = pI->nWeekdayH

    SelfTest_Check( "ideal width: one more COLUMN adds 7*cellMinW + gutterX", _
        (w31 - w21) = (7 * cmw + gx), str(w31 - w21) & " vs " & str(7 * cmw + gx) )
    SelfTest_Check( "ideal height: one more ROW adds a whole panel + gutterY", _
        (h22 - h21) = (capH + wkH + 6 * cmh + gy), str(h22 - h21) & " vs " & str(capH + wkH + 6*cmh + gy) )
    ' The caption is SUPPRESSED at one panel, so 1x1 is exactly one caption shorter than 2x1.
    ' That is the whole derived rule, expressed as a number.
    SelfTest_Check( "ideal height: 1 panel is EXACTLY one caption shorter than 2 panels (the suppression rule)", _
        (h21 - h11) = capH, str(h21 - h11) & " vs capH " & str(capH) )

    PsCalendar_SetPanelGrid( hI21, 4, 4 )
    dim as long qc, qr
    PsCalendar_GetPanelGrid( hI21, qc, qr )
    SelfTest_Check( "SetPanelGrid REJECTS 4x4 (> 12 panels) and leaves the grid untouched", _
        (qc = 2) andalso (qr = 1), str(qc) & "x" & str(qr) )
    PsCalendar_SetPanelGrid( hI21, 0, 3 )
    PsCalendar_GetPanelGrid( hI21, qc, qr )
    SelfTest_Check( "SetPanelGrid rejects a zero dimension", (qc = 2) andalso (qr = 1) )
    PsCalendar_SetPanelGrid( hI21, 3, 4 )
    SelfTest_Check( "SetPanelGrid accepts 3x4 = 12 panels (exactly the cap)", _
        PsCalendar_GetPanelCount( hI21 ) = 12, str(PsCalendar_GetPanelCount(hI21)) )
    PsCalendar_SetPanelGrid( hI21, 2, 1 )

    ' ------------------------------------------------------------------------------------
    ' C. DAYS geometry
    ' ------------------------------------------------------------------------------------
    print "-- DAYS geometry --"
    dim as HWND hG = MakeTestCal( 4, 3, 2021, 1 )
    dim as PSCALENDAR ptr pG = GetCalendarPointer( hG )
    dim as RECT rcNav, rcPrev, rcNext, rcTitle, rcGrid, rcInner
    PsCalendar_GetNavBandRect( hG, rcNav )
    PsCalendar_GetPrevRect( hG, rcPrev )
    PsCalendar_GetNextRect( hG, rcNext )
    PsCalendar_GetTitleRect( hG, rcTitle )
    PsCalendar_GetGridRect( hG, rcGrid )
    rcInner = pG->rcInner

    SelfTest_Check( "nav band spans the full inner width", _
        (rcNav.left = rcInner.left) andalso (rcNav.right = rcInner.right) )
    SelfTest_Check( "nav band sits at the top of the inner area, nNavBandH tall", _
        (rcNav.top = rcInner.top) andalso (RectH(rcNav) = pG->nNavBandH), str(RectH(rcNav)) )
    SelfTest_Check( "prev chevron is nChevW wide at the left of the nav band", _
        (rcPrev.left = rcNav.left) andalso (RectW(rcPrev) = pG->nChevW) )
    SelfTest_Check( "next chevron is nChevW wide at the right of the nav band", _
        (rcNext.right = rcNav.right) andalso (RectW(rcNext) = pG->nChevW) )
    SelfTest_Check( "title fills the gap between the two chevrons, touching neither", _
        (rcTitle.left = rcPrev.right) andalso (rcTitle.right = rcNext.left) )
    SelfTest_Check( "the grid starts nNavGap below the nav band", _
        rcGrid.top = (rcNav.bottom + pG->nNavGap), str(rcGrid.top - rcNav.bottom) )

    dim as RECT rcP0, rcP1, rcP4, rcP5
    PsCalendar_GetPanelRect( hG, 0, rcP0 )
    PsCalendar_GetPanelRect( hG, 1, rcP1 )
    PsCalendar_GetPanelRect( hG, 4, rcP4 )
    PsCalendar_GetPanelRect( hG, 5, rcP5 )
    SelfTest_Check( "panels are ROW-MAJOR: panel 1 is right of panel 0, same top", _
        (rcP1.left > rcP0.left) andalso (rcP1.top = rcP0.top) )
    SelfTest_Check( "panels are ROW-MAJOR: panel 4 starts the next row, same left as panel 0", _
        (rcP4.left = rcP0.left) andalso (rcP4.top > rcP0.top) )
    SelfTest_Check( "panel 5 is at col 1 row 1: shares panel 1's left and panel 4's top", _
        (rcP5.left = rcP1.left) andalso (rcP5.top = rcP4.top) )
    SelfTest_Check( "horizontal gutter between adjacent panels = nGutterX", _
        (rcP1.left - rcP0.right) = pG->nGutterX, str(rcP1.left - rcP0.right) )
    SelfTest_Check( "vertical gutter between panel rows = nGutterY", _
        (rcP4.top - rcP0.bottom) = pG->nGutterY, str(rcP4.top - rcP0.bottom) )

    dim as RECT rcCap0, rcWk0, rcWk6, rcC0, rcC6, rcC7, rcC41
    PsCalendar_GetCaptionRect( hG, 0, rcCap0 )
    PsCalendar_GetWeekdayRect( hG, 0, 0, rcWk0 )
    PsCalendar_GetWeekdayRect( hG, 0, 6, rcWk6 )
    PsCalendar_GetDayCellRect( hG, 0, 0, rcC0 )
    PsCalendar_GetDayCellRect( hG, 0, 6, rcC6 )
    PsCalendar_GetDayCellRect( hG, 0, 7, rcC7 )
    PsCalendar_GetDayCellRect( hG, 0, 41, rcC41 )
    SelfTest_Check( "at 12 panels each panel HAS a caption, nCaptionH tall, at its top", _
        (RectH(rcCap0) = pG->nCaptionH) andalso (rcCap0.top = rcP0.top), str(RectH(rcCap0)) )
    SelfTest_Check( "the weekday row sits directly below the caption", rcWk0.top = rcCap0.bottom )
    SelfTest_Check( "the day grid starts directly below the weekday row", rcC0.top = rcWk0.bottom )
    SelfTest_Check( "weekday column 0 is left-aligned with day cell 0", rcWk0.left = rcC0.left )
    SelfTest_Check( "weekday column 6 is left-aligned with day cell 6", rcWk6.left = rcC6.left )
    SelfTest_Check( "cell 7 is exactly one row below cell 0, same column", _
        (rcC7.left = rcC0.left) andalso ((rcC7.top - rcC0.top) = pG->rowH(0)), str(rcC7.top - rcC0.top) )
    SelfTest_Check( "cell 6 is exactly six columns right of cell 0", _
        (rcC6.left - rcC0.left) = 6 * pG->cellW(0) )
    SelfTest_Check( "cell 41 is the last cell of a fixed 6x7 grid", _
        (rcC41.left = rcC6.left) andalso ((rcC41.top - rcC0.top) = 5 * pG->rowH(0)) )
    SelfTest_Check( "cells FLOOR-divide the panel: 7 columns fit, with < 7px left over", _
        (7 * pG->cellW(0) <= RectW(rcP0)) andalso ((RectW(rcP0) - 7 * pG->cellW(0)) < 7), _
        "panelW " & str(RectW(rcP0)) & " cellW " & str(pG->cellW(0)) )

    dim as HWND hG1 = MakeTestCal( 1, 1, 2021, 3 )
    dim as RECT rcCap1
    PsCalendar_GetCaptionRect( hG1, 0, rcCap1 )
    SelfTest_Check( "at ONE panel the caption is suppressed (zero height)", _
        RectH(rcCap1) = 0, str(RectH(rcCap1)) )
    dim as RECT rcWk1, rcPan1
    PsCalendar_GetWeekdayRect( hG1, 0, 0, rcWk1 )
    PsCalendar_GetPanelRect( hG1, 0, rcPan1 )
    SelfTest_Check( "...and the weekday row starts at the panel top (nothing above it)", _
        rcWk1.top = rcPan1.top, str(rcWk1.top) & " vs " & str(rcPan1.top) )

    ' ------------------------------------------------------------------------------------
    ' D. Cell -> date
    ' ------------------------------------------------------------------------------------
    print "-- cell to date --"
    ' March 2021, Sunday start. The 1st is a Monday, so lead = 1 and cell 0 is Feb 28.
    dim as SYSTEMTIME cd
    PsCalendar_GetCellDate( hG1, 0, 0, cd )
    SelfTest_Check( "Mar 2021 Sunday start: cell 0 = Feb 28 (the leading day)", _
        (cd.wMonth = 2) andalso (cd.wDay = 28), DateStr(cd) )
    PsCalendar_GetCellDate( hG1, 0, 1, cd )
    SelfTest_Check( "Mar 2021: cell 1 = Mar 1", (cd.wMonth = 3) andalso (cd.wDay = 1), DateStr(cd) )
    PsCalendar_GetCellDate( hG1, 0, 31, cd )
    SelfTest_Check( "Mar 2021: cell 31 = Mar 31", (cd.wMonth = 3) andalso (cd.wDay = 31), DateStr(cd) )
    PsCalendar_GetCellDate( hG1, 0, 32, cd )
    SelfTest_Check( "Mar 2021: cell 32 = Apr 1 (the trailing days)", _
        (cd.wMonth = 4) andalso (cd.wDay = 1), DateStr(cd) )
    PsCalendar_GetCellDate( hG1, 0, 41, cd )
    SelfTest_Check( "Mar 2021: cell 41 = Apr 10", (cd.wMonth = 4) andalso (cd.wDay = 10), DateStr(cd) )

    PsCalendar_SetWeekStart( hG1, CAL_MONDAY )
    PsCalendar_GetCellDate( hG1, 0, 0, cd )
    SelfTest_Check( "Mar 2021 MONDAY start: the whole grid rotates, cell 0 = Mar 1", _
        (cd.wMonth = 3) andalso (cd.wDay = 1), DateStr(cd) )
    PsCalendar_SetWeekStart( hG1, CAL_SUNDAY )

    dim as HWND hDec = MakeTestCal( 1, 1, 2020, 12 )
    PsCalendar_GetCellDate( hDec, 0, 33, cd )
    SelfTest_Check( "Dec 2020 -> Jan 2021 rollover: cell 33 = Jan 1 2021", _
        (cd.wYear = 2021) andalso (cd.wMonth = 1) andalso (cd.wDay = 1), DateStr(cd) )
    dim as HWND hJan = MakeTestCal( 1, 1, 2021, 1 )
    PsCalendar_GetCellDate( hJan, 0, 0, cd )
    SelfTest_Check( "Jan 2021 <- Dec 2020 rollover: cell 0 = Dec 27 2020", _
        (cd.wYear = 2020) andalso (cd.wMonth = 12) andalso (cd.wDay = 27), DateStr(cd) )

    dim as long py, pm
    PsCalendar_GetPanelMonth( hG, 0, py, pm )
    SelfTest_Check( "12-panel grid anchored Jan 2021: panel 0 is January", (py = 2021) andalso (pm = 1) )
    PsCalendar_GetPanelMonth( hG, 5, py, pm )
    SelfTest_Check( "...panel 5 is June (panels are consecutive months, row-major)", (py = 2021) andalso (pm = 6), str(py) & "-" & str(pm) )
    PsCalendar_GetPanelMonth( hG, 11, py, pm )
    SelfTest_Check( "...panel 11 is December, so 3x4 shows exactly one year", (py = 2021) andalso (pm = 12), str(py) & "-" & str(pm) )

    ' ------------------------------------------------------------------------------------
    ' E. Title text + localization
    ' ------------------------------------------------------------------------------------
    print "-- title + localization --"
    SelfTest_Check( "title, 1 panel: ""March 2021""", PsCalendar_GetTitleText(hG1) = "March 2021", PsCalendar_GetTitleText(hG1) )
    dim as HWND hT2 = MakeTestCal( 2, 1, 2021, 3 )
    SelfTest_Check( "title, 2 panels in one year: ""Mar - Apr 2021""", _
        PsCalendar_GetTitleText(hT2) = "Mar - Apr 2021", PsCalendar_GetTitleText(hT2) )
    dim as HWND hT4 = MakeTestCal( 4, 1, 2020, 11 )
    SelfTest_Check( "title, a page that SPANS years, names both: ""Nov 2020 - Feb 2021""", _
        PsCalendar_GetTitleText(hT4) = "Nov 2020 - Feb 2021", PsCalendar_GetTitleText(hT4) )
    PsCalendar_SetView( hG1, CAL_VIEW_MONTHS )
    SelfTest_Check( "title, MONTHS view: the year alone", PsCalendar_GetTitleText(hG1) = "2021", PsCalendar_GetTitleText(hG1) )
    PsCalendar_SetView( hG1, CAL_VIEW_YEARS )
    SelfTest_Check( "title, YEARS view: the decade", PsCalendar_GetTitleText(hG1) = "2020 - 2029", PsCalendar_GetTitleText(hG1) )
    PsCalendar_SetView( hG1, CAL_VIEW_DAYS )

    PsCalendar_SetMonthName( hG1, 3, "Mars" )
    SelfTest_Check( "SetMonthName reaches the TITLE, not just the getter", _
        PsCalendar_GetTitleText(hG1) = "Mars 2021", PsCalendar_GetTitleText(hG1) )
    SelfTest_Check( "GetMonthName returns the override", PsCalendar_GetMonthName(hG1,3) = "Mars" )
    PsCalendar_SetMonthName( hG1, 3, "" )
    SelfTest_Check( "clearing ONE entry reverts just that entry to the English default", _
        PsCalendar_GetMonthName(hG1,3) = "March", PsCalendar_GetMonthName(hG1,3) )
    PsCalendar_SetMonthAbbrev( hT2, 4, "Avr" )
    SelfTest_Check( "SetMonthAbbrev reaches the multi-panel range title", _
        PsCalendar_GetTitleText(hT2) = "Mar - Avr 2021", PsCalendar_GetTitleText(hT2) )
    PsCalendar_SetMonthAbbrev( hT2, 4, "" )

    PsCalendar_SetWeekdayAbbrev( hG1, 0, "Di" )
    SelfTest_Check( "GetWeekdayAbbrev is indexed by DAY OF WEEK: 0 = Sunday", _
        PsCalendar_GetWeekdayAbbrev(hG1,0) = "Di", PsCalendar_GetWeekdayAbbrev(hG1,0) )
    ' The table is keyed by weekday, never by column -- so a week-start change must move the
    ' override with the day rather than leave it stuck in column 0. This is the assertion that
    ' would catch a column-indexed table, which is the natural thing to write by mistake.
    dim as PSCALENDAR ptr pL = GetCalendarPointer( hG1 )
    SelfTest_Check( "Sunday start: the Sunday override lands in COLUMN 0", _
        CAL_WeekdayAbbrev( pL, 0, CAL_SUNDAY ) = "Di" )
    SelfTest_Check( "Monday start: the SAME override moves to COLUMN 6, and column 0 becomes Monday", _
        (CAL_WeekdayAbbrev( pL, 6, CAL_MONDAY ) = "Di") andalso (CAL_WeekdayAbbrev( pL, 0, CAL_MONDAY ) = "Mo") )
    PsCalendar_SetWeekdayAbbrev( hG1, 0, "" )

    ' ------------------------------------------------------------------------------------
    ' F. Drill-down
    ' ------------------------------------------------------------------------------------
    print "-- drill-down --"
    PsCalendar_SetView( hG, CAL_VIEW_MONTHS )     ' hG is the 12-PANEL grid
    dim as RECT rcMy0, rcMy5, rcMy11, rcGrid2
    PsCalendar_GetGridRect( hG, rcGrid2 )
    PsCalendar_GetMyCellRect( hG, 0, rcMy0 )
    PsCalendar_GetMyCellRect( hG, 5, rcMy5 )
    PsCalendar_GetMyCellRect( hG, 11, rcMy11 )
    dim as PSCALENDAR ptr pMy = GetCalendarPointer( hG )
    ' The wart fix: PsDatePicker drilled down INSIDE panel 0 and left the rest of the popup
    ' blank. Here the 3x4 grid takes the whole grid region at ANY panel count.
    SelfTest_Check( "MONTHS view at 12 panels: the 3x4 grid spans the WHOLE grid region", _
        (pMy->rcMyGrid.left = rcGrid2.left) andalso (pMy->rcMyGrid.right = rcGrid2.right) andalso _
        (pMy->rcMyGrid.top = rcGrid2.top) andalso (pMy->rcMyGrid.bottom = rcGrid2.bottom) )
    SelfTest_Check( "drill-down cell 0 starts at the grid's top-left corner", _
        (rcMy0.left = rcGrid2.left) andalso (rcMy0.top = rcGrid2.top) )
    SelfTest_Check( "drill-down grid is 3 columns: cell 5 is col 2, row 1", _
        (rcMy5.left = rcMy0.left + 2 * RectW(rcMy0)) andalso (rcMy5.top = rcMy0.top + RectH(rcMy0)) )
    SelfTest_Check( "drill-down grid is 4 rows: cell 11 is col 2, row 3", _
        (rcMy11.left = rcMy5.left) andalso (rcMy11.top = rcMy0.top + 3 * RectH(rcMy0)) )
    ' No day-panel rect may survive into the drill-down -- a stale one would hit-test.
    SelfTest_Check( "no panel rect survives into MONTHS view (a stale one would still hit-test)", _
        (RectW(pMy->rcPanel(0)) = 0) andalso (RectW(pMy->rcDayCell(0,0)) = 0) )
    PsCalendar_SetView( hG, CAL_VIEW_DAYS )

    dim as HWND hD = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pD = GetCalendarPointer( hD )
    dim as RECT rcTt
    PsCalendar_GetTitleRect( hD, rcTt )
    CAL_Click( pD, CenterOf(rcTt), false )
    SelfTest_Check( "clicking the title zooms DAYS -> MONTHS", pD->eView = CAL_VIEW_MONTHS )
    PsCalendar_GetTitleRect( hD, rcTt )
    CAL_Click( pD, CenterOf(rcTt), false )
    SelfTest_Check( "clicking again zooms MONTHS -> YEARS", pD->eView = CAL_VIEW_YEARS )
    PsCalendar_GetTitleRect( hD, rcTt )
    CAL_Click( pD, CenterOf(rcTt), false )
    SelfTest_Check( "the YEARS title is INERT -- there is no level above the decade", pD->eView = CAL_VIEW_YEARS )

    ' Year cell 0 is base-1 and cell 11 is base+10: 12 cells for a 10-year decade, and both
    ' overhangs are clickable rather than decorative.
    dim as RECT rcY0
    PsCalendar_GetMyCellRect( hD, 0, rcY0 )
    CAL_Click( pD, CenterOf(rcY0), false )
    SelfTest_Check( "YEARS cell 0 is the decade's LEADING overhang (2019), and picking it works", _
        (pD->eView = CAL_VIEW_MONTHS) andalso (pD->stAnchor.wYear = 2019), str(pD->stAnchor.wYear) )
    dim as RECT rcM6
    PsCalendar_GetMyCellRect( hD, 6, rcM6 )
    CAL_Click( pD, CenterOf(rcM6), false )
    SelfTest_Check( "MONTHS cell 6 picks July and returns to DAYS", _
        (pD->eView = CAL_VIEW_DAYS) andalso (pD->stAnchor.wMonth = 7) andalso (pD->stAnchor.wYear = 2019), _
        DateStr(pD->stAnchor) )
    SelfTest_Check( "drilling in does NOT select a date -- navigation is not a choice", _
        PsCalendar_HasSelDate(hD) = false )

    ' ------------------------------------------------------------------------------------
    ' G. Hit-test round trip
    ' ------------------------------------------------------------------------------------
    print "-- hit test --"
    dim as CAL_HITRESULT hr
    hr = PsCalendar_HitTest( hG, CenterOf(rcPrev) )
    SelfTest_Check( "hit test: prev chevron", hr.part = CAL_HIT_PREV )
    hr = PsCalendar_HitTest( hG, CenterOf(rcNext) )
    SelfTest_Check( "hit test: next chevron", hr.part = CAL_HIT_NEXT )
    hr = PsCalendar_HitTest( hG, CenterOf(rcTitle) )
    SelfTest_Check( "hit test: title", hr.part = CAL_HIT_TITLE )

    dim as boolean bAllCells = true
    dim as string sBadCell = ""
    for p as long = 0 to 11
        for k as long = 0 to 41 step 7
            dim as RECT rcQ
            PsCalendar_GetDayCellRect( hG, p, k, rcQ )
            dim as CAL_HITRESULT h2 = PsCalendar_HitTest( hG, CenterOf(rcQ) )
            dim as boolean bOK2 = (h2.part = CAL_HIT_DAYCELL) andalso (h2.panel = p) andalso (h2.index = k)
            if bOK2 = false then
                bAllCells = false
                if len(sBadCell) = 0 then sBadCell = "p" & str(p) & " k" & str(k) & " -> part " & str(h2.part) & " p" & str(h2.panel) & " k" & str(h2.index)
            end if
        next
    next
    SelfTest_Check( "hit test round trip over 72 day cells across all 12 panels", bAllCells, sBadCell )

    dim as POINT ptOut
    ptOut.x = -50 : ptOut.y = -50
    hr = PsCalendar_HitTest( hG, ptOut )
    SelfTest_Check( "hit test: a point outside the control resolves to NONE", hr.part = CAL_HIT_NONE )

    dim as RECT rcSomeCell
    PsCalendar_GetDayCellRect( hG, 0, 10, rcSomeCell )
    dim as POINT ptCell = CenterOf(rcSomeCell)
    PsCalendar_SetView( hG, CAL_VIEW_MONTHS )
    hr = PsCalendar_HitTest( hG, ptCell )
    SelfTest_Check( "in MONTHS view the same point is a MONTH cell, never a day cell", _
        hr.part = CAL_HIT_MONTHCELL, "part " & str(hr.part) )
    PsCalendar_SetView( hG, CAL_VIEW_DAYS )

    ' ------------------------------------------------------------------------------------
    ' H. Nav step
    ' ------------------------------------------------------------------------------------
    print "-- nav step --"
    dim as HWND hN1 = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pN1 = GetCalendarPointer( hN1 )
    CAL_NavStep( pN1, 1 )
    SelfTest_Check( "default step at 1 panel = ONE month", _
        (pN1->stAnchor.wMonth = 4) andalso (pN1->stAnchor.wYear = 2021), DateStr(pN1->stAnchor) )

    dim as HWND hN12 = MakeTestCal( 3, 4, 2021, 1 )
    dim as PSCALENDAR ptr pN12 = GetCalendarPointer( hN12 )
    CAL_NavStep( pN12, 1 )
    SelfTest_Check( "default step at 12 panels = a WHOLE YEAR (one page), with no host code", _
        (pN12->stAnchor.wYear = 2022) andalso (pN12->stAnchor.wMonth = 1), DateStr(pN12->stAnchor) )
    CAL_NavStep( pN12, -1 )
    SelfTest_Check( "...and back again", (pN12->stAnchor.wYear = 2021) andalso (pN12->stAnchor.wMonth = 1) )

    dim as HWND hN2 = MakeTestCal( 2, 1, 2021, 3 )
    dim as PSCALENDAR ptr pN2 = GetCalendarPointer( hN2 )
    PsCalendar_SetNavStep( hN2, 1 )
    CAL_NavStep( pN2, 1 )
    SelfTest_Check( "SetNavStep(1) on a 2-panel range picker SLIDES by one month", _
        pN2->stAnchor.wMonth = 4, DateStr(pN2->stAnchor) )
    SelfTest_Check( "GetNavStep reports what was set", PsCalendar_GetNavStep(hN2) = 1 )

    PsCalendar_SetView( hN1, CAL_VIEW_MONTHS )
    CAL_NavStep( pN1, 1 )
    SelfTest_Check( "MONTHS view steps a YEAR regardless of nav step", pN1->stAnchor.wYear = 2022, str(pN1->stAnchor.wYear) )
    PsCalendar_SetView( hN1, CAL_VIEW_YEARS )
    CAL_NavStep( pN1, 1 )
    SelfTest_Check( "YEARS view steps a DECADE", pN1->stAnchor.wYear = 2032, str(pN1->stAnchor.wYear) )
    PsCalendar_SetView( hN1, CAL_VIEW_DAYS )

    dim as HWND hW = MakeTestCal( 1, 1, 2021, 6 )
    dim as PSCALENDAR ptr pW = GetCalendarPointer( hW )
    CAL_Wheel( pW, WHEEL_DELTA )
    SelfTest_Check( "one wheel notch AWAY from the user goes to the previous page", _
        pW->stAnchor.wMonth = 5, DateStr(pW->stAnchor) )
    CAL_Wheel( pW, -WHEEL_DELTA * 2 )
    SelfTest_Check( "two notches toward the user advance two pages", pW->stAnchor.wMonth = 7, DateStr(pW->stAnchor) )
    dim as SYSTEMTIME stBefore = pW->stAnchor
    CAL_Wheel( pW, WHEEL_DELTA \ 3 )
    SelfTest_Check( "a SUB-NOTCH delta accumulates instead of moving (trackpad smoothness)", _
        CAL_SameYMD( pW->stAnchor, stBefore ) )

    ' ------------------------------------------------------------------------------------
    ' I. Chevron enable / disable
    ' ------------------------------------------------------------------------------------
    print "-- chevron disable --"
    dim as HWND hR1 = MakeTestCal( 1, 1, 2021, 3 )
    SelfTest_Check( "no range: both chevrons live", _
        (PsCalendar_IsPrevDisabled(hR1) = false) andalso (PsCalendar_IsNextDisabled(hR1) = false) )
    dim as SYSTEMTIME stMinA = PsCalendar_MakeDate(2021,3,1)
    dim as SYSTEMTIME stMaxA = PsCalendar_MakeDate(2021,3,31)
    PsCalendar_SetRange( hR1, stMinA, stMaxA, true, true )
    SelfTest_Check( "min inside the shown month: prev goes dead (nothing before it is reachable)", _
        PsCalendar_IsPrevDisabled(hR1) )
    SelfTest_Check( "max inside the shown month: next goes dead", PsCalendar_IsNextDisabled(hR1) )

    ' The interesting case, and the one a naive test gets wrong. Two panels stepping ONE month:
    ' "next" reveals anchor+2, not anchor+1, because anchor+1 is already on screen.
    dim as HWND hR2 = MakeTestCal( 2, 1, 2021, 3 )         ' shows Mar + Apr
    PsCalendar_SetNavStep( hR2, 1 )
    dim as SYSTEMTIME stMaxB = PsCalendar_MakeDate(2021,4,20)
    dim as SYSTEMTIME stNone
    PsCalendar_SetRange( hR2, stNone, stMaxB, false, true )
    SelfTest_Check( "2 panels showing Mar+Apr, max Apr 20: next is DEAD -- it would reveal May, which is out", _
        PsCalendar_IsNextDisabled(hR2) )
    dim as SYSTEMTIME stMaxC = PsCalendar_MakeDate(2021,5,15)
    PsCalendar_SetRange( hR2, stNone, stMaxC, false, true )
    SelfTest_Check( "same page, max May 15: next is LIVE -- the newly revealed May is in range", _
        PsCalendar_IsNextDisabled(hR2) = false )
    ' Contrast with ONE panel: there anchor+1 IS the new month, so Apr 20 keeps it live.
    dim as HWND hR3 = MakeTestCal( 1, 1, 2021, 3 )
    PsCalendar_SetRange( hR3, stNone, stMaxB, false, true )
    SelfTest_Check( "1 panel showing Mar, max Apr 20: next is LIVE -- the disable point MOVES with the panel count", _
        PsCalendar_IsNextDisabled(hR3) = false )

    PsCalendar_SetView( hR1, CAL_VIEW_MONTHS )
    SelfTest_Check( "MONTHS view honours the range too", _
        PsCalendar_IsPrevDisabled(hR1) andalso PsCalendar_IsNextDisabled(hR1) )
    PsCalendar_SetView( hR1, CAL_VIEW_YEARS )
    SelfTest_Check( "YEARS view honours the range too", _
        PsCalendar_IsPrevDisabled(hR1) andalso PsCalendar_IsNextDisabled(hR1) )
    PsCalendar_SetView( hR1, CAL_VIEW_DAYS )

    ' ------------------------------------------------------------------------------------
    ' J. Keyboard
    ' ------------------------------------------------------------------------------------
    print "-- keyboard --"
    dim as HWND hK = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pK = GetCalendarPointer( hK )
    PsCalendar_SetSelDate( hK, PsCalendar_MakeDate(2021,3,15) )

    CAL_KeyDown( pK, VK_RIGHT, false )
    SelfTest_Check( "RIGHT moves the selection one day forward", pK->stSelected.wDay = 16, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_LEFT, false )
    SelfTest_Check( "LEFT moves it back", pK->stSelected.wDay = 15, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_DOWN, false )
    SelfTest_Check( "DOWN moves a whole week (+7)", pK->stSelected.wDay = 22, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_UP, false )
    SelfTest_Check( "UP moves back a week (-7)", pK->stSelected.wDay = 15, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_NEXT, false )
    SelfTest_Check( "PgDn moves a month", (pK->stSelected.wMonth = 4) andalso (pK->stSelected.wDay = 15), DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_PRIOR, false )
    SelfTest_Check( "PgUp moves back a month", pK->stSelected.wMonth = 3, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_NEXT, true )
    SelfTest_Check( "Ctrl+PgDn moves a YEAR", pK->stSelected.wYear = 2022, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_PRIOR, true )
    SelfTest_Check( "Ctrl+PgUp moves back a year", pK->stSelected.wYear = 2021, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_HOME, false )
    SelfTest_Check( "Home goes to the 1st of the month", pK->stSelected.wDay = 1, DateStr(pK->stSelected) )
    CAL_KeyDown( pK, VK_END, false )
    SelfTest_Check( "End goes to the LAST day of the month (31 in March)", pK->stSelected.wDay = 31, DateStr(pK->stSelected) )

    ' Crossing a month boundary must scroll the page at ONE panel...
    CAL_KeyDown( pK, VK_RIGHT, false )
    SelfTest_Check( "arrowing past the end of the month scrolls the page (1 panel)", _
        (pK->stSelected.wMonth = 4) andalso (pK->stAnchor.wMonth = 4), _
        "sel " & DateStr(pK->stSelected) & " anchor " & DateStr(pK->stAnchor) )
    ' ...but must NOT at twelve, where the next month is already on screen.
    dim as HWND hK12 = MakeTestCal( 3, 4, 2021, 1 )
    dim as PSCALENDAR ptr pK12 = GetCalendarPointer( hK12 )
    PsCalendar_SetSelDate( hK12, PsCalendar_MakeDate(2021,3,31) )
    CAL_KeyDown( pK12, VK_RIGHT, false )
    SelfTest_Check( "the same move at 12 panels does NOT scroll -- April is already displayed", _
        (pK12->stSelected.wMonth = 4) andalso (pK12->stAnchor.wMonth = 1) andalso (pK12->stAnchor.wYear = 2021), _
        "sel " & DateStr(pK12->stSelected) & " anchor " & DateStr(pK12->stAnchor) )

    ' Clamping
    dim as HWND hKc = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pKc = GetCalendarPointer( hKc )
    PsCalendar_SetRange( hKc, PsCalendar_MakeDate(2021,3,10), PsCalendar_MakeDate(2021,3,20), true, true )
    PsCalendar_SetSelDate( hKc, PsCalendar_MakeDate(2021,3,20) )
    CAL_KeyDown( pKc, VK_RIGHT, false )
    SelfTest_Check( "an arrow at the range's upper edge CLAMPS instead of escaping it", _
        pKc->stSelected.wDay = 20, DateStr(pKc->stSelected) )
    CAL_KeyDown( pKc, VK_DOWN, false )
    SelfTest_Check( "a whole-week jump past the edge clamps to max, not past it", _
        pKc->stSelected.wDay = 20, DateStr(pKc->stSelected) )
    PsCalendar_SetSelDate( hKc, PsCalendar_MakeDate(2021,3,10) )
    CAL_KeyDown( pKc, VK_PRIOR, false )
    SelfTest_Check( "a month jump below the range clamps to min", _
        pKc->stSelected.wDay = 10, DateStr(pKc->stSelected) )

    ' Notification: only when it actually moved.
    PsCalendar_SetSelChangeCallback( hKc, @SelfTest_OnSelChange )
    PsCalendar_SetDateActivatedCallback( hKc, @SelfTest_OnActivated )
    gCbSel = 0 : gCbAct = 0
    CAL_KeyDown( pKc, VK_RIGHT, false )
    SelfTest_Check( "a keyboard move that CHANGES the date notifies once", gCbSel = 1, "count " & str(gCbSel) )
    gCbSel = 0
    PsCalendar_SetSelDate( hKc, PsCalendar_MakeDate(2021,3,20) )
    CAL_KeyDown( pKc, VK_RIGHT, false )
    SelfTest_Check( "a keyboard move that CLAMPS to where it already was stays SILENT", gCbSel = 0, "count " & str(gCbSel) )

    gCbAct = 0
    CAL_KeyDown( pKc, VK_RETURN, false )
    SelfTest_Check( "Enter fires DateActivated", gCbAct = 1, "count " & str(gCbAct) )
    gCbAct = 0
    CAL_KeyDown( pKc, VK_SPACE, false )
    SelfTest_Check( "Space fires DateActivated too", gCbAct = 1, "count " & str(gCbAct) )

    dim as HWND hKe = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pKe = GetCalendarPointer( hKe )
    PsCalendar_SetSelChangeCallback( hKe, @SelfTest_OnSelChange )
    PsCalendar_SetDateActivatedCallback( hKe, @SelfTest_OnActivated )
    gCbSel = 0 : gCbAct = 0
    CAL_KeyDown( pKe, VK_RETURN, false )
    dim as boolean bSelOnce = (gCbSel = 1)
    dim as boolean bActOnce = (gCbAct = 1)
    SelfTest_Check( "Enter with NOTHING selected commits the focus cell first, then activates", _
        bSelOnce andalso bActOnce andalso PsCalendar_HasSelDate(hKe), _
        "sel " & str(gCbSel) & " act " & str(gCbAct) )

    SelfTest_Check( "Esc is NOT consumed in DAYS view (a host dialog's Cancel keeps working)", _
        CAL_KeyDown( pKe, VK_ESCAPE, false ) = false )
    PsCalendar_SetView( hKe, CAL_VIEW_YEARS )
    SelfTest_Check( "Esc IS consumed in YEARS view, and zooms out to MONTHS", _
        CAL_KeyDown( pKe, VK_ESCAPE, false ) andalso (pKe->eView = CAL_VIEW_MONTHS) )
    SelfTest_Check( "Esc again returns to DAYS", _
        CAL_KeyDown( pKe, VK_ESCAPE, false ) andalso (pKe->eView = CAL_VIEW_DAYS) )

    PsCalendar_SetView( hKe, CAL_VIEW_MONTHS )
    pKe->nMyFocus = 4
    CAL_KeyDown( pKe, VK_RIGHT, false )
    SelfTest_Check( "MONTHS view: RIGHT moves the drill-down cursor by 1", pKe->nMyFocus = 5, str(pKe->nMyFocus) )
    CAL_KeyDown( pKe, VK_DOWN, false )
    SelfTest_Check( "MONTHS view: DOWN moves it by a row (3)", pKe->nMyFocus = 8, str(pKe->nMyFocus) )
    pKe->nMyFocus = 0
    CAL_KeyDown( pKe, VK_LEFT, false )
    SelfTest_Check( "the drill-down cursor CLAMPS at 0 rather than wrapping", pKe->nMyFocus = 0 )
    pKe->nMyFocus = 11
    CAL_KeyDown( pKe, VK_RIGHT, false )
    SelfTest_Check( "...and clamps at 11", pKe->nMyFocus = 11 )
    CAL_KeyDown( pKe, VK_SPACE, false )
    SelfTest_Check( "Space in MONTHS view zooms in on the cursor cell (December)", _
        (pKe->eView = CAL_VIEW_DAYS) andalso (pKe->stAnchor.wMonth = 12), DateStr(pKe->stAnchor) )

    ' ------------------------------------------------------------------------------------
    ' K. The focus cell
    ' ------------------------------------------------------------------------------------
    print "-- focus cell --"
    dim as HWND hF = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pF = GetCalendarPointer( hF )
    PsCalendar_SetSelDate( hF, PsCalendar_MakeDate(2021,3,15) )
    dim as long fp, fk
    PsCalendar_GetFocusCell( hF, fp, fk )
    dim as SYSTEMTIME stFc
    PsCalendar_GetCellDate( hF, fp, fk, stFc )
    SelfTest_Check( "focus cell follows the SELECTION when there is one", _
        CAL_SameYMD( stFc, PsCalendar_MakeDate(2021,3,15) ), DateStr(stFc) )

    ' No selection, and today is nowhere near March 2021 -> the last-resort fallback.
    PsCalendar_ClearSelDate( hF )
    PsCalendar_GetFocusCell( hF, fp, fk )
    SelfTest_Check( "with no selection and today off-page, focus falls back to day 1 of panel 0", _
        (fp = 0) andalso (fk = pF->panelLead(0)), "panel " & str(fp) & " index " & str(fk) )

    dim as HWND hFt = PsCalendar_Create( HWND_FRMMAIN, 9200 )
    PsCalendar_SetFont( hFt, ghFont(GUIFONT_10) )
    dim as long tw, th
    PsCalendar_GetIdealSize( hFt, tw, th )
    SetWindowPos( hFt, NULL, 0, 0, tw, th, SWP_NOMOVE or SWP_NOACTIVATE or SWP_NOZORDER )
    PsCalendar_GetFocusCell( hFt, fp, fk )
    PsCalendar_GetCellDate( hFt, fp, fk, stFc )
    dim as SYSTEMTIME stTodayNow
    PsCalendar_GetToday( hFt, stTodayNow )
    SelfTest_Check( "with no selection but TODAY on the page, focus falls back to today", _
        CAL_SameYMD( stFc, stTodayNow ), DateStr(stFc) & " vs " & DateStr(stTodayNow) )

    ' A month boundary shows the same date in two panels at once. The focus ring belongs on the
    ' panel that OWNS the date, not on the neighbour that merely shows it dimmed.
    dim as HWND hFb = MakeTestCal( 2, 1, 2021, 3 )
    PsCalendar_SetSelDate( hFb, PsCalendar_MakeDate(2021,4,1) )
    PsCalendar_GetFocusCell( hFb, fp, fk )
    SelfTest_Check( "a date shown in TWO panels focuses the one that owns it, not the trailing copy", _
        fp = 1, "panel " & str(fp) )

    ' ------------------------------------------------------------------------------------
    ' L. Tab / dialog-manager integration
    ' ------------------------------------------------------------------------------------
    print "-- tab navigation --"
    dim as long exStyle = GetWindowLongPtr( ghCal(0), GWL_EXSTYLE )
    SelfTest_Check( "WS_EX_CONTROLPARENT is NOT set (CWindow.Create defaults it ON, which makes a leaf unreachable by Tab)", _
        (exStyle and WS_EX_CONTROLPARENT) = 0, "exstyle &h" & hex(exStyle) )
    dim as long style = GetWindowLongPtr( ghCal(0), GWL_STYLE )
    SelfTest_Check( "WS_TABSTOP is set", (style and WS_TABSTOP) <> 0 )

    ' End to end: what the dialog manager itself would do.
    dim as HWND hNext1 = GetNextDlgTabItem( HWND_FRMMAIN, ghBtnBefore, FALSE )
    dim as HWND hNext2 = GetNextDlgTabItem( HWND_FRMMAIN, hNext1, FALSE )
    dim as HWND hNext3 = GetNextDlgTabItem( HWND_FRMMAIN, hNext2, FALSE )
    dim as HWND hNext4 = GetNextDlgTabItem( HWND_FRMMAIN, hNext3, FALSE )
    SelfTest_Check( "GetNextDlgTabItem walks button -> cal0 -> cal1 -> cal2 -> button", _
        (hNext1 = ghCal(0)) andalso (hNext2 = ghCal(1)) andalso (hNext3 = ghCal(2)) andalso (hNext4 = ghBtnAfter) )

    ' WM_GETDLGCODE, per message. lParam must point at a real MSG.
    dim as MSG mk
    mk.hwnd = ghCal(0) : mk.message = WM_KEYDOWN
    mk.wParam = VK_RIGHT
    dim as LRESULT dc = SendMessageW( ghCal(0), WM_GETDLGCODE, VK_RIGHT, cast(LPARAM, @mk) )
    SelfTest_Check( "WM_GETDLGCODE claims the arrow keys", (dc and DLGC_WANTALLKEYS) <> 0 )
    mk.wParam = VK_RETURN
    dc = SendMessageW( ghCal(0), WM_GETDLGCODE, VK_RETURN, cast(LPARAM, @mk) )
    SelfTest_Check( "WM_GETDLGCODE claims Enter (a DELIBERATE departure from PsOptionButton -- DateActivated needs a key)", _
        (dc and DLGC_WANTALLKEYS) <> 0 )
    mk.wParam = VK_SPACE
    dc = SendMessageW( ghCal(0), WM_GETDLGCODE, VK_SPACE, cast(LPARAM, @mk) )
    SelfTest_Check( "WM_GETDLGCODE claims Space", (dc and DLGC_WANTALLKEYS) <> 0 )
    ' NEGATIVE, and load-bearing: claiming Tab would swallow the navigation the tabstop exists for.
    mk.wParam = VK_TAB
    dc = SendMessageW( ghCal(0), WM_GETDLGCODE, VK_TAB, cast(LPARAM, @mk) )
    SelfTest_Check( "WM_GETDLGCODE does NOT claim Tab", (dc and DLGC_WANTALLKEYS) = 0 )
    ' NEGATIVE: Esc belongs to the host dialog while we are in DAYS view.
    mk.wParam = VK_ESCAPE
    dc = SendMessageW( ghCal(0), WM_GETDLGCODE, VK_ESCAPE, cast(LPARAM, @mk) )
    SelfTest_Check( "WM_GETDLGCODE does NOT claim Esc in DAYS view", (dc and DLGC_WANTALLKEYS) = 0 )
    PsCalendar_SetView( ghCal(0), CAL_VIEW_MONTHS )
    dc = SendMessageW( ghCal(0), WM_GETDLGCODE, VK_ESCAPE, cast(LPARAM, @mk) )
    SelfTest_Check( "...but DOES claim it once drilled down, where it means zoom out", (dc and DLGC_WANTALLKEYS) <> 0 )
    PsCalendar_SetView( ghCal(0), CAL_VIEW_DAYS )

    ' ------------------------------------------------------------------------------------
    ' K2. EXACTLY ONE cell renders as selected
    '
    ' A date on a month boundary is displayed twice when its neighbouring month is also on the
    ' page: as a real day in its own panel, and as a dimmed leading/trailing day in the next.
    ' Deciding isSelected by comparing each cell's date to the selection lights BOTH -- and the
    ' selected chip is the loudest thing on the surface, so the user cannot tell which cell the
    ' keyboard is actually on. These count what the renderer handed the day painter.
    ' ------------------------------------------------------------------------------------
    print "-- one selected cell --"
    dim as HWND hOne = MakeTestCal( 2, 1, 2021, 3 )      ' shows March + April
    dim as PSCALENDAR ptr pOne = GetCalendarPointer( hOne )

    ' April 1 is a real day of panel 1 AND a trailing day of panel 0's March grid.
    PsCalendar_SetSelDate( hOne, PsCalendar_MakeDate(2021,4,1) )
    SelfTest_Census( hOne )
    SelfTest_Check( "the census sees every cell of both panels (guards a vacuous count below)", _
        gCensusCells = 84, "cells " & str(gCensusCells) )
    SelfTest_Check( "a boundary date shown in TWO panels renders as selected in exactly ONE", _
        gCensusSel = 1, "selected cells = " & str(gCensusSel) )
    SelfTest_Check( "...and it is the panel that OWNS the date, not the trailing copy", _
        gCensusSelPanel = 1, "panel " & str(gCensusSelPanel) )

    ' The duplication runs BOTH ways, which is easy to miss: March 31 belongs to panel 0 and
    ' also appears as a LEADING day in April's grid. (Calling this one "interior" was wrong --
    ' the A/B lit two cells here too.)
    PsCalendar_SetSelDate( hOne, PsCalendar_MakeDate(2021,3,31) )
    SelfTest_Census( hOne )
    SelfTest_Check( "a date duplicated as a LEADING day of the NEXT panel also selects exactly once", _
        (gCensusSel = 1) andalso (gCensusSelPanel = 0), _
        "count " & str(gCensusSel) & " panel " & str(gCensusSelPanel) )

    ' And a date that is genuinely displayed only once must be unaffected by any of this.
    PsCalendar_SetSelDate( hOne, PsCalendar_MakeDate(2021,3,15) )
    SelfTest_Census( hOne )
    SelfTest_Check( "a date displayed only once is still selected, in its own panel", _
        (gCensusSel = 1) andalso (gCensusSelPanel = 0), _
        "count " & str(gCensusSel) & " panel " & str(gCensusSelPanel) )

    ' Nothing selected -> nothing highlighted.
    PsCalendar_ClearSelDate( hOne )
    SelfTest_Census( hOne )
    SelfTest_Check( "no selection means no cell renders as selected", gCensusSel = 0, str(gCensusSel) )

    ' The focus ring is single too, and lands on the same cell as the chip.
    PsCalendar_SetSelDate( hOne, PsCalendar_MakeDate(2021,4,1) )
    pOne->isFocused = true
    SelfTest_Census( hOne )
    SelfTest_Check( "exactly one cell is marked focused", gCensusFocus = 1, str(gCensusFocus) )
    dim as long fPan, fIdx
    PsCalendar_GetFocusCell( hOne, fPan, fIdx )
    SelfTest_Check( "the focus cell and the selected cell are the SAME cell", _
        (fPan = gCensusSelPanel) andalso (fIdx = gCensusSelIndex), _
        "focus " & str(fPan) & "/" & str(fIdx) & "  selected " & str(gCensusSelPanel) & "/" & str(gCensusSelIndex) )
    pOne->isFocused = false

    ' Twelve panels: every month boundary in the year is on screen at once, so a naive
    ' date-comparison would light two cells for eleven of the twelve month starts.
    dim as HWND hOne12 = MakeTestCal( 3, 4, 2021, 1 )
    dim as boolean bAllOne = true
    dim as string sBadSel = ""
    for m as long = 1 to 12
        PsCalendar_SetSelDate( hOne12, PsCalendar_MakeDate(2021, m, 1) )
        SelfTest_Census( hOne12 )
        if gCensusSel <> 1 then
            bAllOne = false
            if len(sBadSel) = 0 then sBadSel = "month " & str(m) & " -> " & str(gCensusSel) & " cells"
        end if
    next
    SelfTest_Check( "the 1st of all twelve months, all displayed at once, each selects exactly one cell", _
        bAllOne, sBadSel )
    DestroyWindow( hOne12 )
    DestroyWindow( hOne )

    ' ------------------------------------------------------------------------------------
    ' L2. The Enter beep
    '
    ' Claiming VK_RETURN with DLGC_WANTALLKEYS makes IsDialogMessage TranslateMessage the
    ' key-down, which manufactures a WM_CHAR of 0x0D. Nothing consuming that character means it
    ' reaches DefWindowProc, and the system beeps -- while the keystroke still works perfectly.
    ' The beep is audio, so none of this can assert the symptom; it asserts the MECHANISM.
    ' ------------------------------------------------------------------------------------
    print "-- Enter beep suppression --"
    dim as PSCALENDAR ptr pBeep = GetCalendarPointer( ghCal(0) )
    dim as MSG mc
    mc.hwnd = ghCal(0) : mc.message = WM_CHAR : mc.wParam = 13

    pBeep->bAteKeyDown = false
    SendMessageW( ghCal(0), WM_KEYDOWN, VK_RETURN, 0 )
    SelfTest_Check( "a consumed Enter arms the character swallow", pBeep->bAteKeyDown )
    dim as LRESULT dcc = SendMessageW( ghCal(0), WM_GETDLGCODE, 13, cast(LPARAM, @mc) )
    SelfTest_Check( "...so WM_GETDLGCODE claims the WM_CHAR the dialog manager is about to send", _
        (dcc and DLGC_WANTALLKEYS) <> 0 )
    ' The DISARMED FLAG is what proves the swallow ran. The return value cannot: DefWindowProc
    ' also returns 0 for WM_CHAR, so `rch = 0` holds just as well when the message fell straight
    ' through and beeped. Measured -- with the swallow removed this case still reported
    ' "returned 0" and failed only on the flag.
    dim as LRESULT rch = SendMessageW( ghCal(0), WM_CHAR, 13, 0 )
    SelfTest_Check( "...and the control swallows that WM_CHAR instead of letting it reach DefWindowProc", _
        pBeep->bAteKeyDown = false, "flag still armed = not swallowed (return value " & str(rch) & " proves nothing here)" )

    ' A key we do NOT consume must leave an ordinary character alone, or a host accelerator on
    ' a printable key would stop working.
    pBeep->bAteKeyDown = false
    SendMessageW( ghCal(0), WM_KEYDOWN, VK_TAB, 0 )
    SelfTest_Check( "a key we do NOT consume leaves the swallow disarmed", pBeep->bAteKeyDown = false )
    mc.wParam = asc("a")
    dcc = SendMessageW( ghCal(0), WM_GETDLGCODE, asc("a"), cast(LPARAM, @mc) )
    SelfTest_Check( "...and an ordinary character is not claimed", (dcc and DLGC_WANTALLKEYS) = 0 )

    ' THE ORDERING CASE, and the reason the swallow is a flag rather than a state test. Esc in
    ' a drill-down view is consumed and CHANGES eView to DAYS -- so by the time its WM_CHAR
    ' arrives, a test like "are we drilled down?" reads the view the key-down just left, decides
    ' the character is not ours, and lets the beep through. The first version of this fix did
    ' exactly that.
    PsCalendar_SetView( ghCal(0), CAL_VIEW_MONTHS )
    pBeep->bAteKeyDown = false
    SendMessageW( ghCal(0), WM_KEYDOWN, VK_ESCAPE, 0 )
    SelfTest_Check( "Esc while drilled down is consumed and zooms out", pBeep->eView = CAL_VIEW_DAYS )
    SelfTest_Check( "...and the swallow stays armed even though eView has already changed under it", _
        pBeep->bAteKeyDown )
    mc.wParam = 27
    dcc = SendMessageW( ghCal(0), WM_GETDLGCODE, 27, cast(LPARAM, @mc) )
    SelfTest_Check( "...so its WM_CHAR is still claimed", (dcc and DLGC_WANTALLKEYS) <> 0 )
    SendMessageW( ghCal(0), WM_CHAR, 27, 0 )

    ' A focus change must not leave a swallow armed for a character that is never coming.
    pBeep->bAteKeyDown = true
    SendMessageW( ghCal(0), WM_KILLFOCUS, 0, 0 )
    SelfTest_Check( "losing focus disarms a pending swallow", pBeep->bAteKeyDown = false )
    PsCalendar_ClearSelDate( ghCal(0) )

    ' ------------------------------------------------------------------------------------
    ' M. Notification / silent setters
    ' ------------------------------------------------------------------------------------
    print "-- notification --"
    dim as HWND hNo = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pNo = GetCalendarPointer( hNo )
    PsCalendar_SetSelChangeCallback( hNo, @SelfTest_OnSelChange )
    PsCalendar_SetDateActivatedCallback( hNo, @SelfTest_OnActivated )

    gCbSel = 0 : gCbAct = 0
    PsCalendar_SetSelDate( hNo, PsCalendar_MakeDate(2021,3,10) )
    PsCalendar_SetAnchorDate( hNo, PsCalendar_MakeDate(2021,3,1) )
    PsCalendar_SetWeekStart( hNo, CAL_MONDAY )
    PsCalendar_SetPanelGrid( hNo, 2, 1 )
    PsCalendar_ClearSelDate( hNo )
    PsCalendar_SetView( hNo, CAL_VIEW_MONTHS )
    PsCalendar_SetView( hNo, CAL_VIEW_DAYS )
    SelfTest_Check( "EVERY programmatic setter is silent (7 of them, zero callbacks)", _
        (gCbSel = 0) andalso (gCbAct = 0), "sel " & str(gCbSel) & " act " & str(gCbAct) )

    PsCalendar_SetPanelGrid( hNo, 1, 1 )
    PsCalendar_SetWeekStart( hNo, CAL_SUNDAY )
    gCbSel = 0
    PsCalendar_SelectDate( hNo, PsCalendar_MakeDate(2021,3,12) )
    SelfTest_Check( "SelectDate is an ACTION and fires", gCbSel = 1, "count " & str(gCbSel) )
    gCbAct = 0
    PsCalendar_Activate( hNo )
    SelfTest_Check( "Activate is an ACTION and fires", gCbAct = 1, "count " & str(gCbAct) )
    PsCalendar_ClearSelDate( hNo )
    gCbAct = 0
    PsCalendar_Activate( hNo )
    SelfTest_Check( "Activate with nothing selected fires NOTHING (there is no date to act on)", gCbAct = 0 )

    dim as RECT rcClickCell
    PsCalendar_SetSelDate( hNo, PsCalendar_MakeDate(2021,3,1) )
    PsCalendar_GetDayCellRect( hNo, 0, 20, rcClickCell )
    gCbSel = 0 : gCbAct = 0
    CAL_Click( pNo, CenterOf(rcClickCell), false )
    SelfTest_Check( "a single click fires SelChange once and Activated NOT at all", _
        (gCbSel = 1) andalso (gCbAct = 0), "sel " & str(gCbSel) & " act " & str(gCbAct) )
    PsCalendar_GetDayCellRect( hNo, 0, 21, rcClickCell )
    gCbSel = 0 : gCbAct = 0
    CAL_Click( pNo, CenterOf(rcClickCell), true )
    SelfTest_Check( "a double click fires BOTH, selection first", _
        (gCbSel = 1) andalso (gCbAct = 1), "sel " & str(gCbSel) & " act " & str(gCbAct) )
    SelfTest_Check( "...and the date the host reads inside Activated is already the NEW one", _
        CAL_SameYMD( gCbActDate, gCbSelDate ), DateStr(gCbActDate) )

    ' Out-of-range cells are inert to the mouse, not merely dim.
    PsCalendar_SetRange( hNo, PsCalendar_MakeDate(2021,3,10), PsCalendar_MakeDate(2021,3,20), true, true )
    PsCalendar_GetDayCellRect( hNo, 0, 1, rcClickCell )     ' Mar 1, below min
    gCbSel = 0
    CAL_Click( pNo, CenterOf(rcClickCell), false )
    SelfTest_Check( "clicking an OUT-OF-RANGE day does nothing at all", gCbSel = 0, "count " & str(gCbSel) )
    PsCalendar_SetRange( hNo, stNone, stNone, false, false )

    ' SetSelDate refuses what the user could not reach.
    dim as HWND hNr = MakeTestCal( 1, 1, 2021, 3 )
    PsCalendar_SetRange( hNr, PsCalendar_MakeDate(2021,3,10), PsCalendar_MakeDate(2021,3,20), true, true )
    PsCalendar_SetSelDate( hNr, PsCalendar_MakeDate(2021,3,1) )
    SelfTest_Check( "SetSelDate refuses an out-of-range date (the setter cannot reach past the hit test)", _
        PsCalendar_HasSelDate(hNr) = false )
    ' Narrowing the range drops a selection that no longer fits.
    PsCalendar_SetRange( hNr, stNone, stNone, false, false )
    PsCalendar_SetSelDate( hNr, PsCalendar_MakeDate(2021,3,1) )
    PsCalendar_SetRange( hNr, PsCalendar_MakeDate(2021,3,10), PsCalendar_MakeDate(2021,3,20), true, true )
    SelfTest_Check( "narrowing the range DROPS a selection that falls outside it", _
        PsCalendar_HasSelDate(hNr) = false )
    ' An inverted pair is swapped rather than rejected.
    PsCalendar_SetRange( hNr, PsCalendar_MakeDate(2021,3,20), PsCalendar_MakeDate(2021,3,10), true, true )
    dim as SYSTEMTIME gMin, gMax
    dim as boolean gHasMin, gHasMax
    PsCalendar_GetRange( hNr, gMin, gMax, gHasMin, gHasMax )
    SelfTest_Check( "an INVERTED min/max pair is swapped, not rejected", _
        (gMin.wDay = 10) andalso (gMax.wDay = 20), DateStr(gMin) & " .. " & DateStr(gMax) )

    ' ------------------------------------------------------------------------------------
    ' N. Render probes
    '
    ' Thresholds are MEASURED here, not inherited. PsButton's "> 1" floor has now passed on a
    ' completely destroyed render in two different controls, because the number depends on
    ' what else falls inside the part rect. Each floor below sits between a value observed
    ' from a healthy render and one observed from a deliberately flooded one.
    ' ------------------------------------------------------------------------------------
    print "-- render probes --"
    dim as HWND hP = MakeTestCal( 1, 1, 2021, 3 )
    PsCalendar_SetSelDate( hP, PsCalendar_MakeDate(2021,3,15) )
    dim as long tClient = PsCalendar_TestPartTones( hP, CAL_PART_CLIENT )
    dim as long tNav    = PsCalendar_TestPartTones( hP, CAL_PART_NAVBAND )
    dim as long tGrid   = PsCalendar_TestPartTones( hP, CAL_PART_GRID )
    dim as long tPrev   = PsCalendar_TestPartTones( hP, CAL_PART_PREV )
    print "     measured tones: client=" & str(tClient) & " nav=" & str(tNav) & " grid=" & str(tGrid) & " prev=" & str(tPrev)
    SelfTest_Check( "the whole client renders many tones (floor 40; a flood reports 1)", tClient > 40, str(tClient) )
    SelfTest_Check( "the nav band renders text and two chevrons (floor 10)", tNav > 10, str(tNav) )
    ' FLOOR 100, NOT 20, AND THE DIFFERENCE IS THE WHOLE POINT. The grid region also contains
    ' the weekday header row and (above one panel) the month captions, and that text survives
    ' a completely destroyed day grid: the A/B below measured 45 tones with EVERY day cell
    ' flooded to one colour. A floor of 20 therefore passed on a render with no days in it at
    ' all -- the same shape of near-miss PsMessageBox and PsOptionButton both hit by inheriting
    ' a sibling's number. 100 sits between the measured healthy (179) and flooded (45) values.
    SelfTest_Check( "the day grid renders (floor 100, measured between healthy 179 and flooded 45)", _
        tGrid > 100, str(tGrid) )
    SelfTest_Check( "a single chevron cell alone is antialiased (floor 4)", tPrev > 4, str(tPrev) )

    dim as long tSelCell = PsCalendar_TestPartTones( hP, CAL_PART_DAYCELL_BASE + 0 * 100 + 16 )
    SelfTest_Check( "the SELECTED day cell renders a filled chip plus a numeral (floor 5)", tSelCell > 5, str(tSelCell) )

    ' A/B the probe rather than trusting it: a callback that floods each cell must be visible.
    PsCalendar_SetDayPaintCallback( hP, @SelfTest_FloodDayPaint )
    dim as long tGridFlood = PsCalendar_TestPartTones( hP, CAL_PART_GRID )
    dim as long tCellFlood = PsCalendar_TestPartTones( hP, CAL_PART_DAYCELL_BASE + 0 * 100 + 16 )
    print "     flooded tones: grid=" & str(tGridFlood) & " cell=" & str(tCellFlood)
    SelfTest_Check( "PROBE A/B: a flooding host painter collapses one cell to a single tone", _
        tCellFlood = 1, str(tCellFlood) )
    SelfTest_Check( "PROBE A/B: ...and the GRID floor discriminates -- a flooded grid falls under it", _
        tGridFlood < 100, str(tGridFlood) & " must be under the floor of 100" )
    PsCalendar_SetDayPaintCallback( hP, 0 )
    SelfTest_Check( "removing the callback restores the built-in render", _
        PsCalendar_TestPartTones( hP, CAL_PART_GRID ) > 100 )

    ' Adjacent vs out-of-range: two dim greys that MUST NOT read the same, because one is
    ' clickable and the other is not. Compared as a RATIO, at the pixel level.
    dim as HWND hB = MakeTestCal( 1, 1, 2021, 3 )
    ' cell 0 = Feb 28 (adjacent, clickable); make cell 41 = Apr 10 out of range.
    PsCalendar_SetRange( hB, stNone, PsCalendar_MakeDate(2021,4,1), false, true )
    dim as long bAdj = PsCalendar_TestPartBrightness( hB, CAL_PART_DAYCELL_BASE + 0 )
    dim as long bOut = PsCalendar_TestPartBrightness( hB, CAL_PART_DAYCELL_BASE + 41 )
    print "     brightness: adjacent=" & str(bAdj) & " out-of-range=" & str(bOut)
    SelfTest_Check( "an ADJACENT (clickable) day renders >= 1.8x brighter than an OUT-OF-RANGE (inert) one", _
        bAdj * 10 >= bOut * 18, str(bAdj) & " vs " & str(bOut) )

    ' Hash probes. Each varies ONE thing, and nothing else the render depends on -- an
    ' un-isolated hash assertion measures whatever moved and reports it under the wrong name.
    dim as HWND hH = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pH = GetCalendarPointer( hH )
    dim as ulongint hashTitleA = PsCalendar_TestHashPart( hH, CAL_PART_TITLE )
    PsCalendar_SetMonthName( hH, 3, "Maerz" )
    dim as ulongint hashTitleB = PsCalendar_TestHashPart( hH, CAL_PART_TITLE )
    SelfTest_Check( "a localization override reaches the PIXELS, not just the getter", _
        hashTitleA <> hashTitleB, "isolated: only the title string differs" )
    PsCalendar_SetMonthName( hH, 3, "" )
    SelfTest_Check( "...and clearing it restores the identical render", _
        PsCalendar_TestHashPart( hH, CAL_PART_TITLE ) = hashTitleA )

    dim as ulongint hashBorder0 = PsCalendar_TestHashPart( hH, CAL_PART_CLIENT )
    PsCalendar_SetBorderThickness( hH, 1 )
    dim as ulongint hashBorder1 = PsCalendar_TestHashPart( hH, CAL_PART_CLIENT )
    SelfTest_Check( "turning the border on changes the render", hashBorder0 <> hashBorder1 )
    ' AND THE HASH ALONE IS NOT ENOUGH, which an A/B proved rather than a hunch. Stroking the
    ' frame with a fill-and-stroke primitive (PaintRoundBorderRect / PaintBorderRect -- both
    ' route through PaintRectFactory, which calls FillPath UNCONDITIONALLY) floods the entire
    ' client. The hash still CHANGES when that happens, so every "the pixels moved" assertion
    ' sails straight through it: the deliberate break passed 172/172. Only a tone COUNT can
    ' tell a frame from a flood.
    dim as long tBordered = PsCalendar_TestPartTones( hH, CAL_PART_CLIENT )
    print "     border on, client tones = " & str(tBordered)
    SelfTest_Check( "the border STROKES the frame, it does not FILL it (floor 40; a flood reports 2)", _
        tBordered > 40, str(tBordered) )
    ' Read rcInner through the PUBLIC accessor, never off the struct. A setter only marks the
    ' layout dirty (the family's lazy-layout rule), so touching pCal->rcInner directly returns
    ' the PREVIOUS geometry and the assertion silently compares a value with itself. This one
    ' failed exactly that way when it was first written -- third occurrence of the
    ' read-derived-state-without-forcing-the-derivation trap in this family.
    dim as RECT rcIn0, rcIn1
    PsCalendar_GetInnerRect( hH, rcIn0 )
    PsCalendar_SetBorderThickness( hH, 0 )
    PsCalendar_GetInnerRect( hH, rcIn1 )
    SelfTest_Check( "...and it moves rcInner by exactly its thickness on every side", _
        ((rcIn0.left - rcIn1.left) = 1) andalso ((rcIn1.right - rcIn0.right) = 1), _
        str(rcIn0.left) & " vs " & str(rcIn1.left) )

    ' The focus ring is drawn by the CONTROL, after the cell painter. Isolated: the only thing
    ' that differs between the two renders is isFocused, and the ring is the only thing on the
    ' cell whose appearance depends on it.
    dim as HWND hFr = MakeTestCal( 1, 1, 2021, 3 )
    dim as PSCALENDAR ptr pFr = GetCalendarPointer( hFr )
    PsCalendar_SetSelDate( hFr, PsCalendar_MakeDate(2021,3,15) )
    dim as long fpr, fkr
    PsCalendar_GetFocusCell( hFr, fpr, fkr )
    pFr->isFocused = false
    dim as ulongint hashUnfocused = PsCalendar_TestHashPart( hFr, CAL_PART_DAYCELL_BASE + fpr * 100 + fkr )
    pFr->isFocused = true
    dim as ulongint hashFocused = PsCalendar_TestHashPart( hFr, CAL_PART_DAYCELL_BASE + fpr * 100 + fkr )
    SelfTest_Check( "the focus ring reaches the pixels of the focus cell", hashUnfocused <> hashFocused )
    ' Same trap as the border, same reason (see above): a flooded ring changes the hash too.
    '
    ' An ABSOLUTE tone floor does not work here, and that is worth knowing rather than
    ' rediscovering: the ring's rect is INSIDE the cell, so flooding it erases the numeral but
    ' leaves the cell's margins and the flood's own antialiased corners -- still 23 tones,
    ' which sails over any floor low enough to be safe. The discriminating comparison is
    ' RELATIVE: stroking a ring can only ADD tones to a cell, while filling one destroys the
    ' numeral underneath and takes tones away.
    pFr->isFocused = false
    dim as long tCellUnfoc = PsCalendar_TestPartTones( hFr, CAL_PART_DAYCELL_BASE + fpr * 100 + fkr )
    pFr->isFocused = true
    dim as long tCellFoc = PsCalendar_TestPartTones( hFr, CAL_PART_DAYCELL_BASE + fpr * 100 + fkr )
    print "     focus cell tones: unfocused=" & str(tCellUnfoc) & " focused=" & str(tCellFoc)
    SelfTest_Check( "the focus ring ADDS ink to the cell -- it strokes, it does not fill over the numeral", _
        tCellFoc >= tCellUnfoc, str(tCellFoc) & " vs unfocused " & str(tCellUnfoc) )
    ' And it survives a host painter that ignores isFocused entirely -- which is the whole
    ' reason the control draws it rather than delegating it.
    PsCalendar_SetDayPaintCallback( hFr, @SelfTest_FloodDayPaint )
    pFr->isFocused = false
    dim as ulongint hashHostUnfoc = PsCalendar_TestHashPart( hFr, CAL_PART_DAYCELL_BASE + fpr * 100 + fkr )
    pFr->isFocused = true
    dim as ulongint hashHostFoc = PsCalendar_TestHashPart( hFr, CAL_PART_DAYCELL_BASE + fpr * 100 + fkr )
    SelfTest_Check( "the focus ring survives a host painter that ignores isFocused", _
        hashHostUnfoc <> hashHostFoc )
    PsCalendar_SetDayPaintCallback( hFr, 0 )
    pFr->isFocused = false

    ' ------------------------------------------------------------------------------------
    ' O. The stretch path
    '
    ' Every other geometry assertion sizes its control to the ideal, which is the one size at
    ' which stretching is a no-op. This group builds its OWN control and squeezes it to 55% of
    ' the ideal in both axes.
    '
    ' It used to measure the demo's own grid instead, which was a mistake twice over: the
    ' assertions then depended on the demo's cosmetic layout (they broke when it was retuned
    ' for readability), and the last panel index was written as a literal 11, so when the demo
    ' dropped to six panels the group silently tested a panel that no longer existed --
    ' GetPanelRect returned FALSE, left the rect zeroed, and "the last panel fits inside the
    ' grid" passed because (0,0) fits inside everything.
    ' ------------------------------------------------------------------------------------
    print "-- stretch (own control, squeezed to 55% of ideal) --"
    dim as HWND hStretch = MakeTestCal( 3, 4, 2021, 1 )
    dim as long sw, sh
    PsCalendar_GetIdealSize( hStretch, sw, sh )
    SetWindowPos( hStretch, NULL, 0, 0, (sw * 55) \ 100, (sh * 55) \ 100, _
                  SWP_NOMOVE or SWP_NOACTIVATE or SWP_NOZORDER )
    dim as PSCALENDAR ptr pS = GetCalendarPointer( hStretch )
    pS->LayoutCalendar()
    dim as RECT rcSt
    GetClientRect( hStretch, @rcSt )
    dim as long nLast = PsCalendar_GetPanelCount( hStretch ) - 1
    print "     ideal " & str(sw) & "x" & str(sh) & "   actual " & str(RectW(rcSt)) & "x" & str(RectH(rcSt)) & _
          "   panels " & str(nLast + 1)
    SelfTest_Check( "the control really is BELOW ideal in both axes (otherwise this group tests nothing)", _
        (RectW(rcSt) < sw) andalso (RectH(rcSt) < sh), str(RectW(rcSt)) & "x" & str(RectH(rcSt)) )
    SelfTest_Check( "below ideal, cells SHRINK rather than clip: they keep a positive size", _
        (pS->cellW(0) > 0) andalso (pS->rowH(0) > 0), "cellW " & str(pS->cellW(0)) & " rowH " & str(pS->rowH(0)) )

    dim as RECT rcLast, rcGridS
    dim as boolean bGotLast = PsCalendar_GetPanelRect( hStretch, nLast, rcLast )
    PsCalendar_GetGridRect( hStretch, rcGridS )
    ' Guards the zeroed-rect vacuous pass described above: without this, a bad index makes the
    ' two containment checks below succeed against an empty rectangle.
    SelfTest_Check( "the last panel actually EXISTS", _
        bGotLast andalso (RectW(rcLast) > 0), "panel " & str(nLast) )
    SelfTest_Check( "the last panel still fits inside the grid region -- nothing overflows the control", _
        (rcLast.right <= rcGridS.right) andalso (rcLast.bottom <= rcGridS.bottom), _
        "panel" & str(nLast) & " " & str(rcLast.right) & "," & str(rcLast.bottom) & " grid " & str(rcGridS.right) & "," & str(rcGridS.bottom) )

    dim as RECT rcLastCell
    dim as boolean bGotCell = PsCalendar_GetDayCellRect( hStretch, nLast, 41, rcLastCell )
    SelfTest_Check( "...and so does the last day cell of the last panel", _
        bGotCell andalso (rcLastCell.right <= rcGridS.right) andalso (rcLastCell.bottom <= rcGridS.bottom) )
    SelfTest_Check( "a stretched grid still hit-tests: the last cell of the last panel round-trips", _
        PsCalendar_HitTest( hStretch, CenterOf(rcLastCell) ).part = CAL_HIT_DAYCELL )
    DestroyWindow( hStretch )

    ' ------------------------------------------------------------------------------------
    print ""

    ' ====================================================================================
    ' TOOLTIP BACKEND -- the TRACKED shape
    '
    ' This control does not use the standard block the rest of the family carries, and the
    ' differences are the point. Its tip is (a) LAZY, built on the first day cell that
    ' actually wants one, so "no tooltip window yet" is the correct state for a fresh
    ' calendar, and (b) TRACKED -- shown by the control, not by a dwell -- so there is no
    ' hover time to assert surviving a switch.
    ' ====================================================================================
    scope
        dim as HWND hC = PsCalendar_Create( HWND_FRMMAIN, 9807 )
        if hC = 0 then
            SelfTest_Check( "tooltip: could not create a test calendar", false, "Create returned 0" )
        else
            dim as PSCALENDAR ptr pC = GetCalendarPointer( hC )
            SelfTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsCalendar_GetTooltipMode( hC ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsCalendar_GetTooltipMode( hC )) )
            SelfTest_Check( "tooltip: NO tooltip window exists until a cell wants one (still lazy)", _
                (PsCalendar_GetTooltipHandle( hC ) = 0) andalso (PsCalendar_GetPsTooltipHandle( hC ) = 0), _
                "hSys = " & str(cast(integer, PsCalendar_GetTooltipHandle( hC ))) & _
                " hPs = " & str(cast(integer, PsCalendar_GetPsTooltipHandle( hC ))) )

            ' CAL_TipEnsure is what the hover path calls; driving it directly is the only way
            ' in, since a real tip needs a real cursor over a real cell.
            CAL_TipEnsure( pC )
            ' Hoisted into one boolean local rather than chained: IsWindow returns a LONG and
            ' mixing it into an andalso beside real booleans is fb warning 38 (workspace rule).
            dim as boolean bLazyBuilt = (IsWindow( PsCalendar_GetTooltipHandle( hC ) ) <> 0)
            bLazyBuilt = bLazyBuilt andalso PsTipHost_IsTracked( @pC->tip )
            SelfTest_Check( "tooltip: the lazy build produces a comctl32 tip, and it is TRACKED", _
                bLazyBuilt, _
                "hSys = " & str(cast(integer, PsCalendar_GetTooltipHandle( hC ))) )

            dim as boolean bSw = PsCalendar_SetTooltipMode( hC, PSTIP_MODE_PS )
            SelfTest_Check( "tooltip: switching to PsTooltip succeeds", bSw, _
                "mode = " & str(PsCalendar_GetTooltipMode( hC )) )
            dim as HWND hPsC = PsCalendar_GetPsTooltipHandle( hC )
            SelfTest_Check( "tooltip: a PsTooltip window exists and the comctl32 one is gone", _
                (hPsC <> 0) andalso (IsWindow(hPsC) <> 0) andalso (PsCalendar_GetTooltipHandle( hC ) = 0), _
                "hPs = " & str(cast(integer, hPsC)) )
            SelfTest_Check( "tooltip: it stays declared TRACKED across the switch", _
                PsTipHost_IsTracked( @pC->tip ), "tracked" )

            ' TrackShow with EMPTY text must hide rather than show an empty tip -- this is the
            ' path a day cell with no tooltip takes, and on the old code it was a separate
            ' early return that is now inside PsTipHost.
            dim as POINT ptZ : dim as RECT rcZ
            PsTipHost_TrackShow( @pC->tip, ptZ, rcZ, "" )
            SelfTest_Check( "tooltip: TrackShow with empty text shows nothing", _
                (PsTooltip_IsVisible( hPsC ) = false), "not visible" )

            DestroyWindow( hC )
        end if
    end scope

    dim as string sSummary = "==== self-test: " & str(gTestCount) & " checks, " & str(gFailCount) & " failed ===="
    print sSummary
    print ""

    DestroyWindow( hFresh )
    DestroyWindow( hI11 ) : DestroyWindow( hI21 ) : DestroyWindow( hI31 ) : DestroyWindow( hI22 )
    DestroyWindow( hG )   : DestroyWindow( hG1 )
    DestroyWindow( hDec ) : DestroyWindow( hJan )
    DestroyWindow( hT2 )  : DestroyWindow( hT4 )
    DestroyWindow( hD )
    DestroyWindow( hN1 )  : DestroyWindow( hN2 ) : DestroyWindow( hN12 ) : DestroyWindow( hW )
    DestroyWindow( hR1 )  : DestroyWindow( hR2 ) : DestroyWindow( hR3 )
    DestroyWindow( hK )   : DestroyWindow( hK12 ) : DestroyWindow( hKc ) : DestroyWindow( hKe )
    DestroyWindow( hF )   : DestroyWindow( hFt ) : DestroyWindow( hFb )
    DestroyWindow( hNo )  : DestroyWindow( hNr )
    DestroyWindow( hP )   : DestroyWindow( hB ) : DestroyWindow( hH ) : DestroyWindow( hFr )
end sub
