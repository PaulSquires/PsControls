'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsTooltip - demo harness
'
' Hosts the specimen sheet and the geometry self-test (PSTOOLTIP_SELFTEST=1). Live trace with
' PSTOOLTIP_TRACE=1. The process exit code is the self-test failure count.
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "win/commctrl.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("Custom Tooltip")
#define APPCLASSNAME     wstr("custom_tooltip_class")

#DEFINE GUIFONT          wstr("Segoe UI")

' NO SegoeFluentIcons.ttf IN THIS REPO. The title glyph is a paint-time font the HOST supplies,
' so the control imposes no icon font of its own -- and this demo deliberately uses plain
' Unicode characters that render in any text font, so it works on a machine that does not have
' the icon font installed. (PsCheckBox's self-test learned this the hard way: with a missing
' icon font both of its glyph states drew the identical missing-glyph box.)
#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE GUIFONTBOLD_10   2
#DEFINE GUIFONT_14       3
#DEFINE MAXFONTS         4

dim shared ghFont(MAXFONTS) as HFONT

dim shared as HWND HWND_FRMMAIN

#include once "frmMain.bi"


type THEME_TYPE
    ForeColor             as COLORREF = BGR(190,196,206)
    ForeColorDisabled     as COLORREF = BGR( 90, 96,106)
    BackColor             as COLORREF = BGR( 33, 37, 43)
    ForeColorHot          as COLORREF = BGR(215,218,224)
    BackColorHot          as COLORREF = BGR( 44, 49, 58)
    ForeColorSelect       as COLORREF = BGR(255,255,255)
    BackColorSelect       as COLORREF = BGR( 38, 79,120)
    FocusAccent           as COLORREF = BGR( 86,156,214)
    Divider               as COLORREF = BGR( 55, 60, 69)
end type
dim shared theme as THEME_TYPE


#include once "PsBufferPaint.inc"
#include once "PsTooltip.inc"
' THE SELF-TEST IS INCLUDED BY THE DEMO, NOT BY PsTooltip.inc, and that is deliberate rather
' than stylistic: a control whose .inc pulls in its own test file is not self-contained, so
' anyone who copies the four shipped files and nothing else gets `error 23: File not found`.
' Compiling the README's quick-start against only those four files is what catches it.
#include once "PsTooltip_SelfTest.inc"
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

    ' Initialize GDI+ (PsBufferPaint draws all geometry through it, and this control's frame
    ' and stem ARE geometry). Must be running before the first WM_PAINT builds a buffer, and
    ' must outlive every one of them, so it brackets frmMain_Show.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    function = frmMain_Show( 0 )

    ' Every window is destroyed and every PsBufferPaint has run its destructor by here, so no
    ' CGp* object can still be alive. Precedes CoUninitialize: GDI+ leans on COM.
    AfxGdipShutdown( gdipToken )

    CoUninitialize

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
