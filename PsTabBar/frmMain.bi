'    PsTabBar - demo harness
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


#define IDC_FRMMAIN_TABBAR      1000
#define IDC_FRMMAIN_TABBAR2     1001
#define IDC_FRMMAIN_NAVLEFT     1010
#define IDC_FRMMAIN_NAVRIGHT    1011

' Demo-local timer that refreshes the nav buttons' enabled state from
' TCM_CANNAVIGATE (tab count and bar width both change it at runtime).
#define IDT_FRMMAIN_NAVPOLL     1

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
