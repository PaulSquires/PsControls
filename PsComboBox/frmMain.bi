'    PsComboBox - reusable owner-drawn dropdown selector control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo lays the comboboxes out as a settings pane: one per row, label on the left,
' control on the right -- the arrangement the control was drawn for.
'
'   0  the reference screenshot, reproduced: two items, the longer one selected
'   1  ARROW-ONLY mode (no caption; the width collapses to padding + chevron)
'   2  DISABLED, with a selection, so the greyed state is visible
'   3  NO SELECTION (-1) with a placeholder string
'   4  a MUTABLE list, driven by the two buttons below, exercising the index fix-up
'   5  a custom PAINT CALLBACK replacing the built-in painter wholesale
'   6  CBO_TEXT_WHENSELECTED: arrow-only until answered, then a captioned combobox
#define COMBO_COUNT   7

#define IDC_FRMMAIN_COMBO_FIRST    1000
#define IDC_FRMMAIN_BTN_ADD        1100
#define IDC_FRMMAIN_BTN_DELETE     1101
#define IDC_FRMMAIN_BTN_AUTOSIZE   1102

' The mutable row -- the one the Add/Delete buttons act on.
#define COMBO_MUTABLE   4
' The collapse-until-answered row.
#define COMBO_WHENSEL   6

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
