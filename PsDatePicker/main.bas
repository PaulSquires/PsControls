'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsDatePicker - demo harness
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("Custom Date Picker")
#define APPCLASSNAME     wstr("custom_datepicker_class")

#DEFINE GUIFONT          wstr("Segoe UI")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE GUIFONT_11       2
#DEFINE GUIFONTBOLD_10   3
#DEFINE MAXFONTS         4

dim shared ghFont(MAXFONTS) as HFONT

dim shared as HWND HWND_FRMMAIN


#include once "frmMain.bi"
dim shared as HWND ghPick(0 to PICK_COUNT - 1)
dim shared as HWND ghPlainBox


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


' Include order matters, and it is the price of embedding a real editing control:
' PsDatePicker needs PsTextBox, which needs PsPopupMenu, and everything paints through
' PsBufferPaint.
#include once "PsBufferPaint.inc"
#include once "PsPopupMenu.inc"
#include once "PsTextBox.inc"
#include once "PsDatePicker.inc"
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

    dim as ULONG_PTR gdipToken = AfxGdipInit()

    function = frmMain_Show( 0 )

    AfxGdipShutdown( gdipToken )

    CoUninitialize

end function


' ========================================================================================
' Main program entry point
' ========================================================================================
end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
