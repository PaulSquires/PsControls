'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsBufferPaint - demo harness
'
' Draws a specimen sheet of every primitive so the rendering can be judged by eye, and
' hosts the geometry self-test (PSBUFFERPAINT_SELFTEST=1).
'
' Note the two host obligations it demonstrates, because a host that skips either gets a
' build that fails confusingly rather than one that misbehaves visibly:
'   1. AfxGdipInit / AfxGdipShutdown must bracket the message loop (see WinMain).
'   2. No identifier may be named `ok` -- PsBufferPaint.bi pulls in GDI+'s Status enum,
'      whose Ok = 0 lands in AfxNova, and every host here says `using AfxNova`.
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("PsBufferPaint specimen")
#define APPCLASSNAME     wstr("PsBufferPaint_demo_class")

#DEFINE GUIFONT          wstr("Segoe UI")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE MAXFONTS         2

dim shared ghFont(MAXFONTS) as HFONT
dim shared as HWND HWND_FRMMAIN


' A minimal theme, so the demo shows the buffer taking colors from a host rather than
' owning any of its own (the family rule: no host globals inside a control).
type THEME_TYPE
    ForeColor             as COLORREF = BGR(190,196,206)
    BackColor             as COLORREF = BGR( 33, 37, 43)
    ForeColorHot          as COLORREF = BGR(215,218,224)
    BackColorHot          as COLORREF = BGR( 44, 49, 58)
    ForeColorSelect       as COLORREF = BGR(255,255,255)
    BackColorSelect       as COLORREF = BGR( 38, 79,120)
    FocusAccent           as COLORREF = BGR( 86,156,214)
    Divider               as COLORREF = BGR( 85, 92,104)
end type
dim shared theme as THEME_TYPE


#include once "PsBufferPaint.inc"
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

    ' Initialize GDI+. Must be running before the first WM_PAINT builds a buffer, and must
    ' outlive every one of them, so it brackets frmMain_Show. This is obligation 1: skip it
    ' and nothing geometric draws.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    ' Show the main form
    function = frmMain_Show( 0 )

    ' Every window is destroyed and every PsBufferPaint has run its destructor by here,
    ' so no CGp* object can still be alive. Precedes CoUninitialize: GDI+ leans on COM.
    AfxGdipShutdown( gdipToken )

    ' Uninitialize the COM library
    CoUninitialize

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
