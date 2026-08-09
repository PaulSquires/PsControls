'    PsCalendar demo - main form
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' Three calendars: a plain single month, a sliding two-month range picker, and a 3x4 year grid
' with a custom day painter and localized month names.
#define CAL_COUNT                   3

#define IDC_FRMMAIN_CAL_FIRST    1000
#define IDC_FRMMAIN_BTN_BEFORE   1090
#define IDC_FRMMAIN_BTN_AFTER    1091

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
