'    PsMessageBox - reusable owner-drawn modal message box control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo is a list of rows, each a PsButton that raises one message box. The row's small print
' reports what the LAST call returned, which is the only way to see the difference between
' clicking Cancel and pressing Esc -- they answer the same id on purpose.
'
'    0  OK, info icon                       -- the one-button case: Esc must answer OK
'    1  OK / Cancel, question icon
'    2  Yes / No, warning                   -- no Cancel button, so Esc answers NO
'    3  Yes / No / Cancel, warning
'    4  Save / Don't Save / Cancel, LIGHT   -- the reference screenshot, colour for colour
'    5  Error, a long message that wraps    -- the auto-height path
'    6  No icon and no title                -- both bands still present, both empty
'    7  Focus and default on DIFFERENT rows -- Delete is default, Cancel is focused
'    8  A custom PAINT CALLBACK             -- replaces the built-in painter wholesale
'    9  NESTED: click the BODY of the box   -- raises a second modal loop on top of the first
'   10  PsMessageBox_Show one-liner          -- no fonts set, so the glyph is honestly missing
#define DEMO_ROW_COUNT   11

#define IDC_FRMMAIN_BTN_FIRST      1000

' The rows the handlers need to name.
#define DEMO_OK             0
#define DEMO_OKCANCEL       1
#define DEMO_YESNO          2
#define DEMO_YESNOCANCEL    3
#define DEMO_SAVE_LIGHT     4
#define DEMO_ERROR_LONG     5
#define DEMO_BARE           6
#define DEMO_FOCUSSPLIT     7
#define DEMO_CUSTOMPAINT    8
#define DEMO_NESTED         9
#define DEMO_ONELINER      10

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
