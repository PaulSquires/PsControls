'    PsNumericUpDown - reusable owner-drawn numeric up/down control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo lays the spinners out as a settings pane: one per row, label on the left, the
' control on the right -- the arrangement it was drawn for.
#define SPIN_COUNT   6

#define IDC_FRMMAIN_SPIN_FIRST   1000
#define IDC_FRMMAIN_SPIN_TEST    1098   ' the throwaway control the self-test measures
#define IDC_FRMMAIN_TEXTBOX      1099   ' a plain PsTextBox, to prove Tab walks the nesting

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
