'    PsDatePicker - reusable owner-drawn date picker control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo lays the pickers out as a settings pane: one per row, label on the left, the
' control on the right.
#define PICK_COUNT   4

#define IDC_FRMMAIN_PICK_FIRST   1000
#define IDC_FRMMAIN_PICK_TEST    1098   ' the throwaway control the self-test measures
#define IDC_FRMMAIN_TEXTBOX      1099   ' a plain PsTextBox, to prove Tab walks the nesting

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
