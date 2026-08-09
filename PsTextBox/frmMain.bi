'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


#define IDC_FRMMAIN_TEXTBOX1      1000
#define IDC_FRMMAIN_TEXTBOX2      1001
#define IDC_FRMMAIN_TEXTBOX3      1002
#define IDC_FRMMAIN_TEXTBOX4      1003     ' multiline, editable
#define IDC_FRMMAIN_TEXTBOX5      1004     ' multiline, read-only, preloaded

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
