'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsButton - demo harness
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("Custom Button")
#define APPCLASSNAME     wstr("custom_button_class")

#DEFINE GUIFONT          wstr("Segoe UI")
#DEFINE SYMBOLFONT       wstr("Segoe Fluent Icons")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE GUIFONTBOLD_10   2
#DEFINE SYMBOLFONT_10    3
#DEFINE SYMBOLFONT_12    4
#DEFINE MAXFONTS         5

dim shared ghFont(MAXFONTS) as HFONT

dim shared as HWND HWND_FRMMAIN

#include once "frmMain.bi"
dim shared as HWND ghButton(0 to BUTTON_COUNT - 1)


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
#include once "PsButton.inc"
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

    ' Load the Segoe Fluent Icons ttf file that supplies the demo's glyphs. The CONTROL has no
    ' opinion about which font its glyphs come from -- it just draws the string it is given
    ' with the HFONT it is handed -- so this is the demo's business, not the control's.
    dim as DWSTRING wszFontFile
    wszFontFile = AfxGetExePathName + "SegoeFluentIcons.ttf"
    if AddFontResourceEx(wszFontFile.vptr, FR_PRIVATE, NULL) = 0 then
        MessageBox( 0, _
                    "Unable to load application font 'SegoeFluentIcons.ttf'. Aborting application." , _
                    "Error", _
                    MB_OK or MB_ICONWARNING or MB_DEFBUTTON1 or MB_APPLMODAL )
        return 1
    end if

    ' Initialize GDI+ (PsBufferPaint draws all geometry through it). Must be running before the
    ' first WM_PAINT builds a buffer, and must outlive every one of them, so it brackets
    ' frmMain_Show.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    ' Show the main form
    function = frmMain_Show( 0 )

    ' Unload the font file. Must mirror the AddFontResourceEx call above, flags included --
    ' plain RemoveFontResource does not match an FR_PRIVATE registration and leaks it.
    if len(wszFontFile) then RemoveFontResourceEx( wszFontFile.vptr, FR_PRIVATE, NULL )

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
