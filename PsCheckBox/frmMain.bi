'    PsCheckBox - reusable owner-drawn checkbox control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo lays the checkboxes out as a settings pane: one per row, an explanatory label on the
' left drawn by the form, and the control itself on the right -- the same arrangement PsButton's
' and PsComboBox's demos use, so the three can be compared directly.
'
'    0  box LEFT of the caption -- the default look
'    1  box RIGHT, sized to its IDEAL width      -> packed against the caption
'    2  box RIGHT, sized to a FULL ROW width     -> the settings-row look, same control
'    3  CHECKED at create
'    4  DISABLED and checked
'    5  DISABLED and unchecked  (both, because each has its own colour)
'    6  NO CAPTION -- box only, so no gap is charged and the row is visibly shorter
'    7  CHK_TEXTALIGN_CENTER, given slack past its ideal width
'    8  CHK_TEXTALIGN_RIGHT, same slack
'    9  HOVER ROW HIGHLIGHT opted in -- the flat default overridden by one colour field
'   10  a custom PAINT CALLBACK replacing the built-in painter wholesale
'   11  TOOLTIP: no caption, so the tip is the only way to learn what it does
'   12  forced NARROWER than ideal, so the caption ellipsizes
'   13  CUSTOM GLYPHS -- the two codepoints swapped for a different pair
#define CHECKBOX_COUNT   14

#define IDC_FRMMAIN_CHK_FIRST      1000

' The rows the layout and the handlers need to name.
#define ROW_RIGHT_IDEAL   1
#define ROW_RIGHT_ROW     2
#define ROW_CHECKED       3
#define ROW_DIS_ON        4
#define ROW_DIS_OFF       5
#define ROW_NOCAPTION     6
#define ROW_ALIGNC        7
#define ROW_ALIGNR        8
#define ROW_HOTROW        9
#define ROW_CUSTOM       10
#define ROW_TOOLTIP      11
#define ROW_ELLIPSIS     12
#define ROW_GLYPHS       13

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
