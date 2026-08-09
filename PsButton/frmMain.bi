'    PsButton - reusable owner-drawn command button control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo lays the buttons out as a settings pane: one per row, label on the left, button on
' the right -- the same arrangement PsComboBox's demo uses, so the two can be compared directly.
'
'    0  plain text-only button, caption centred
'    1  LEFT glyph + caption
'    2  caption + RIGHT glyph
'    3  BOTH glyphs
'    4  ICON ONLY (no caption -- so no gap is charged, and the height drops to the icon)
'    5  BTN_ALIGN_LEFT, with a left glyph
'    6  BTN_ALIGN_RIGHT, with a right glyph
'    7  DISABLED, through EnableWindow
'    8  DEFAULT button -- the accent border overlay, in every mood
'    9  a custom PAINT CALLBACK replacing the built-in painter wholesale
'   10  TOOLTIP: icon-only, so the tip is the only way to learn what it does
'   11  forced NARROWER than ideal, so the caption ellipsizes
#define BUTTON_COUNT   12

#define IDC_FRMMAIN_BTN_FIRST      1000

' The rows the layout and the handlers need to name.
#define BUTTON_ALIGNL     5
#define BUTTON_ALIGNR     6
#define BUTTON_DISABLED   7
#define BUTTON_DEFAULT    8
#define BUTTON_CUSTOM     9
#define BUTTON_TOOLTIP   10
#define BUTTON_ELLIPSIS  11

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
