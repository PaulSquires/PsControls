'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsHScrollBar. Owner-drawn horizontal scrollbar
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("Custom HScrollBar")
#define APPCLASSNAME     wstr("custom_hscrollbar_class")

#DEFINE GUIFONT          wstr("Segoe UI")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE MAXFONTS         2

dim shared ghFont(MAXFONTS) as HFONT

dim shared as HWND HWND_FRMMAIN


type THEME_TYPE
    BackColorPanel        as COLORREF = BGR(33,37,43)
    BackColorBox          as COLORREF = BGR(44,49,58)
    ForeColor             as COLORREF = BGR(215,218,224)
    ForeColorLabel        as COLORREF = BGR(140,146,155)
    ForeColorRuler        as COLORREF = BGR(90,98,112)
    BackColorScrollBar    as COLORREF = BGR(33,37,43)
    ForeColorScrollBar    as COLORREF = BGR(53,59,69)
    ForeColorScrollBarHot as COLORREF = BGR(90,98,112)
    FocusAccent           as COLORREF = BGR(86,156,214)
    FocusAccentHot        as COLORREF = BGR(120,180,240)
end type
dim shared theme as THEME_TYPE



#include once "PsBufferPaint.inc"
#include once "PsHScrollBar.inc"
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

    ' Show the main form
    ' Initialize GDI+ (PsBufferPaint draws all geometry through it). Must be
    ' running before the first WM_PAINT builds a buffer, and must outlive every one of
    ' them, so it brackets frmMain_Show.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    function = frmMain_Show( 0 )

    ' Uninitialize the COM library
    ' Every window is destroyed and every PsBufferPaint has run its destructor by here,
    ' so no CGp* object can still be alive. Precedes CoUninitialize: GDI+ leans on COM.
    AfxGdipShutdown( gdipToken )

    CoUninitialize

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
