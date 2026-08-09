'    PsToggle - reusable owner-drawn toggle switch control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo lays the toggles out as a settings pane: one per row, label on the left, switch
' on the right, a hairline between rows -- the arrangement the control was drawn for.
#define TOGGLE_COUNT   8

#define IDC_FRMMAIN_TOGGLE_FIRST   1000
#define IDC_FRMMAIN_TOGGLE_TEST    1099   ' the throwaway control the self-test measures

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
