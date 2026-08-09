'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
' PsMenuBar + PsPopupMenu demo harness
'
' Build:  fbc64.exe -i "C:\dev" main.bas
' Test:   set PSMENUBAR_SELFTEST=1 before running for the no-interaction self-test
'         (exit code = failure count).
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME          wstr("Custom MenuBar")
#define APPCLASSNAME     wstr("custom_menubar_class")

#DEFINE GUIFONT          wstr("Segoe UI")

#DEFINE GUIFONT_9        0
#DEFINE GUIFONT_10       1
#DEFINE MAXFONTS         2

dim shared ghFont(MAXFONTS) as HFONT

dim shared as HWND HWND_FRMMAIN
dim shared as HWND HWND_MENUBAR
dim shared as HWND HWND_CTXMENU    ' standalone context menu: the PsPopupMenu-alone door


type THEME_TYPE
    BackColorPanel  as COLORREF = BGR(30,33,39)
    ForeColorPanel  as COLORREF = BGR(160,166,178)
    ForeColor       as COLORREF = BGR(215,218,224)
    BackColor       as COLORREF = BGR(33,37,43)
    ForeColorHot    as COLORREF = BGR(255,255,255)
    BackColorHot    as COLORREF = BGR(44,49,58)
    ForeColorSelect as COLORREF = BGR(255,255,255)
    BackColorSelect as COLORREF = BGR(38,79,120)
    ForeColorMuted  as COLORREF = BGR(108,114,127)
    BorderColor     as COLORREF = BGR(60,66,77)
    FocusAccent     as COLORREF = BGR(86,156,214)
end type
dim shared theme as THEME_TYPE


#include once "PsBufferPaint.inc"
#include once "PsMenuBar.inc"        ' pulls in PsPopupMenu.inc itself

' Color structs handed to the controls (copied on Set).
dim shared gBarColors as PSMENUBAR_COLORS
dim shared gPopupColors as PSPOPUPMENU_COLORS

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

    gBarColors.BackColor    = theme.BackColor
    gBarColors.ForeColor    = theme.ForeColor
    gBarColors.BackColorHot = theme.BackColorHot
    gBarColors.ForeColorHot = theme.ForeColorHot

    gPopupColors.BackColor         = theme.BackColor
    gPopupColors.ForeColor         = theme.ForeColor
    gPopupColors.BackColorHot      = theme.BackColorSelect
    gPopupColors.ForeColorHot      = theme.ForeColorSelect
    gPopupColors.ForeColorDisabled = theme.ForeColorMuted
    gPopupColors.BorderColor       = theme.BorderColor
    gPopupColors.SeparatorColor    = theme.BorderColor

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
