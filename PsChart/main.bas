'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsChart - demo harness
'
' Set PSCHART_SELFTEST=1 in the environment to run the geometry assertions on startup;
' output goes to the console.
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("Custom Chart")
#define APPCLASSNAME     wstr("custom_chart_class")

#DEFINE GUIFONT          wstr("Segoe UI")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE GUIFONTBOLD_9    2
#DEFINE GUIFONTBOLD_12   3
#DEFINE MAXFONTS         4

dim shared ghFont(MAXFONTS) as HFONT

dim shared as HWND HWND_FRMMAIN

#include once "frmMain.bi"
dim shared as HWND ghChart(0 to CHART_COUNT - 1)


type THEME_TYPE
    ForeColor             as COLORREF = BGR( 40, 40, 40)
    ForeColorDisabled     as COLORREF = BGR(150,150,150)
    BackColor             as COLORREF = BGR(246,246,246)
    ForeColorHot          as COLORREF = BGR(  0,  0,  0)
    BackColorHot          as COLORREF = BGR(234,234,234)
    ForeColorSelect       as COLORREF = BGR(255,255,255)
    BackColorSelect       as COLORREF = BGR( 38, 79,120)
    FocusAccent           as COLORREF = BGR( 86,156,214)
    Divider               as COLORREF = BGR(215,215,215)
end type
dim shared theme as THEME_TYPE


#include once "PsBufferPaint.inc"
#include once "PsChart.inc"
#include once "frmMain.inc"


' ========================================================================================
' WinMain
' ========================================================================================
function WinMain( _
            byval hInstance     as HINSTANCE, _
            byval hPrevInstance as HINSTANCE, _
            byval szCmdLine     as zstring ptr, _
            byval nCmdShow      as long _
            ) as long

    ' Initialize the COM library
    CoInitialize(null)

    ' Initialize GDI+. PsChart draws every wedge, polyline and bar through PsBufferPaint,
    ' which is GDI+ underneath, so this must be running before the first WM_PAINT builds a
    ' buffer and must outlive every one of them -- hence it brackets frmMain_Show.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    function = frmMain_Show( 0 )

    ' Every window is destroyed and every PsBufferPaint has run its destructor by here, so
    ' no CGp* object can still be alive. Precedes CoUninitialize: GDI+ leans on COM.
    AfxGdipShutdown( gdipToken )

    CoUninitialize

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
