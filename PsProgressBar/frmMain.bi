'    PsProgressBar - reusable owner-drawn progress bar control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' The demo is a specimen sheet: one labelled row per configuration down the left, and a strip
' of vertical bars on the right, because orientation is a Create-time argument and a vertical
' bar therefore cannot be demonstrated by flipping a switch on a horizontal one.
#define HBAR_COUNT   12
#define VBAR_COUNT    3

#define IDC_FRMMAIN_HBAR_FIRST   1000
#define IDC_FRMMAIN_VBAR_FIRST   1100

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
