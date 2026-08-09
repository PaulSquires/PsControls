'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsImage - demo harness
'
' Loads the same picture three ways -- an in-memory HBITMAP, a reloaded .png, a reloaded .bmp
' -- and draws each with PsBufferPaint.PaintImage in ASPECT / STRETCH / CENTER fit, so the
' loader and the fit modes can both be judged by eye. Pass a file path on the command line to
' add a fourth row from a real .ico / .png (transparency shows against the cell background).
'
' Hosts the self-test (PSIMAGE_SELFTEST=1).
'
' Two host obligations, both inherited from PsBufferPaint / CGdiPlus.inc and neither new:
'   1. AfxGdipInit / AfxGdipShutdown must bracket the message loop (see WinMain) -- image
'      decoding builds GDI+ objects.
'   2. No identifier may be named `ok` (GDI+'s Status.Ok lands in AfxNova; every host here
'      says `using AfxNova`). The family convention is bOK.
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("PsImage demo")
#define APPCLASSNAME     wstr("PsImage_demo_class")

#DEFINE GUIFONT          wstr("Segoe UI")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE MAXFONTS         2

dim shared ghFont(MAXFONTS) as HFONT
dim shared as HWND HWND_FRMMAIN


' A minimal host theme -- the buffer and PsImage take colours from the host; they own none.
type THEME_TYPE
    ForeColor             as COLORREF = BGR(190,196,206)
    BackColor             as COLORREF = BGR( 33, 37, 43)
    ForeColorHot          as COLORREF = BGR(215,218,224)
    BackColorHot          as COLORREF = BGR( 44, 49, 58)
    ForeColorSelect       as COLORREF = BGR(255,255,255)
    BackColorSelect       as COLORREF = BGR( 38, 79,120)
    FocusAccent           as COLORREF = BGR( 86,156,214)
    Divider               as COLORREF = BGR(120,128,140)
end type
dim shared theme as THEME_TYPE


#include once "PsBufferPaint.inc"
#include once "PsImage.inc"
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

    CoInitialize(null)

    ' GDI+ must be running before the first Load / WM_PAINT and must outlive every PsImage
    ' and every PsBufferPaint, so it brackets frmMain_Show.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    function = frmMain_Show( 0 )

    AfxGdipShutdown( gdipToken )
    CoUninitialize

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
