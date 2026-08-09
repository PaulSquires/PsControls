'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsColorPicker - demo harness
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("Custom ColorPicker")
#define APPCLASSNAME     wstr("custom_colorpicker_class")

#DEFINE GUIFONT          wstr("Segoe UI")

' NO SYMBOLFONT, AND NO SegoeFluentIcons.ttf IN THIS REPO. Everything this control draws is
' geometry or plain text -- there is no glyph anywhere in it, so there is no font to load, no
' AddFontResourceEx to mirror on exit, and no missing-glyph failure mode on a machine that
' lacks the font.
#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE GUIFONTBOLD_10   2
#DEFINE GUIFONTBOLD_12   3
#DEFINE MAXFONTS         4

dim shared ghFont(MAXFONTS) as HFONT

dim shared as HWND HWND_FRMMAIN

' NO ghPicker ARRAY ANY MORE. The demo used to keep three live picker handles here for its whole
' run; a modal popup exists only between DoModal's create and its destroy, so there is nothing
' left for the host to hold. What replaced it is gRowColor / gRowAlpha in frmMain.inc -- the
' host's MODEL, which is all a host ever actually wanted.
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
#include once "PsColorPicker.inc"
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

    ' Initialize GDI+ (PsBufferPaint draws all geometry through it). Must be running before the
    ' first WM_PAINT builds a buffer, and must outlive every one of them, so it brackets
    ' frmMain_Show.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    ' Show the main form
    function = frmMain_Show( 0 )

    ' Every window is destroyed and every PsBufferPaint has run its destructor by here, so no
    ' CGp* object can still be alive. Precedes CoUninitialize: GDI+ leans on COM.
    AfxGdipShutdown( gdipToken )

    ' Uninitialize the COM library
    CoUninitialize


end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
