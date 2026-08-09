'    PsSelectBar - reusable owner-drawn select bar control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


#define IDC_FRMMAIN_SELECTBAR_OUTPUT   1000
#define IDC_FRMMAIN_SELECTBAR_TRACKS   1001
#define IDC_FRMMAIN_SELECTBAR_CUSTOM   1002

' Panel ids. A host looks panels up by id rather than by index, so that inserting one at
' design time does not silently renumber every switch statement.
#define IDP_COMPILER_RESULTS   100
#define IDP_COMPILER_LOG       101
#define IDP_SEARCH_RESULTS     102
#define IDP_TODO               103
#define IDP_NOTES              104

#define IDP_ACTIVE_TRADES      200
#define IDP_CLOSED_TRADES      201
#define IDP_TRANSACTIONS       202
#define IDP_TRADES_NOTES       203

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
