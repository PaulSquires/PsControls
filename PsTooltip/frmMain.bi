'    PsTooltip - reusable owner-drawn tooltip control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


' The demo is EIGHT TARGETS, each a plain Win32 STATIC with its own tip configured differently,
' plus a per-item strip and two plain Win32 BUTTONs.
'
'   0  Plain text                 the default: rounded frame, no stem, default timing
'   1  Balloon                    TIP_STYLE_BALLOON -- the stem points back at the cursor
'   2  Multiline + SetMaxWidth    the gap this control exists to close (see the README)
'   3  Title + glyph              a bold first line and an icon cell beside it
'   4  Callback text              text pulled on demand, and it CHANGES between shows
'   5  Custom painter             a host TIP_PaintCallbackSub replacing the built-in one
'   6  No fade                    SetFadeTime(0) -- appears instantly, for comparison
'   7  Vetoed                     a ShowCallback that returns FALSE half the time
'
' THE TARGETS ARE PLAIN WIN32 STATICS ON PURPOSE, not siblings from this family. The whole
' claim of this control is that it attaches to ANY window, and a demo built only from things
' the author also wrote cannot test that claim. The two BUTTONs at the bottom are here for the
' same reason PsOptionButton's are: a form made only of the thing under test is exactly the
' shape that hid PsComboBox's Tab bug (Learnings.md).
'
' NOTE THE SS_NOTIFY ON EVERY STATIC. Without it a static answers WM_NCHITTEST with
' HTTRANSPARENT and WindowFromPoint returns its PARENT -- the case TIP_CursorOverAttached has
' an explicit third branch for. The demo sets it anyway, because that is what a real host does.

#define TARGET_COUNT   8

#define IDC_FRMMAIN_TARGET_FIRST  1000
#define IDC_FRMMAIN_STRIP         1100
#define IDC_FRMMAIN_BTN_OK        1900
#define IDC_FRMMAIN_BTN_CANCEL    1901

' The per-item strip: one window, four cells, one tip. This is the shape every per-item control
' in the family uses -- a single tool covering the whole HWND, with the hot cell selected by
' PsTooltip_SetToolIndex from the host's WM_MOUSEMOVE. It is the exact call site that sends
' TTM_POP today in PsListBox, PsColumnHeader, PsStatusBar, PsTabBar, PsSelectBar and PsIconPanel.
#define STRIP_CELLS    4

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
