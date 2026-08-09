'    PsIconPanel - reusable owner-drawn icon panel control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


#define IDC_FRMMAIN_ICONPANEL_LEFT     1000
#define IDC_FRMMAIN_ICONPANEL_CENTER   1001
#define IDC_FRMMAIN_ICONPANEL_RIGHT    1002

' Command ids handed to the click callback. Toggles report through SelChange instead, so
' they need no ids -- but they get them anyway, because a real host looks items up by id.
#define IDM_SAVE        100
#define IDM_REFRESH     101
#define IDM_DELETE      102
#define IDM_SETTINGS    103
#define IDM_FAVOURITE   200
#define IDM_READONLY    201
#define IDM_FIND        202

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
