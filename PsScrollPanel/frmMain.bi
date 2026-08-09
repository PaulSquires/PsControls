'    PsScrollPanel demo harness
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


#define IDC_FRMMAIN_SCROLLPANEL   1000
#define IDC_FRMMAIN_TESTPANEL     1100   ' the self-test's own throwaway instance
#define IDC_FRMMAIN_FIRSTROW      2000   ' row control ids run upward from here

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
