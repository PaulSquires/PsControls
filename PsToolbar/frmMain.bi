'    PsToolbar - reusable owner-drawn toolbar control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


#define IDC_FRMMAIN_TOOLBAR_MAIN     1000
#define IDC_FRMMAIN_TOOLBAR_CUSTOM   1001
#define IDC_FRMMAIN_SEARCHBOX        1002
#define IDC_FRMMAIN_BTNPLAIN1        1003
#define IDC_FRMMAIN_BTNPLAIN2        1004

' Command ids handed to the click callback. Toggles report through SelChange instead, so
' they need no ids -- but they get them anyway, because a real host looks items up by id.
#define IDM_NEW          100
#define IDM_OPEN         101
#define IDM_SAVE         102
#define IDM_BUILD        103
#define IDM_RUN          104
#define IDM_STOP         105
#define IDM_WORDWRAP     106
#define IDM_SETTINGS     107
#define IDM_HELP         108
#define IDM_ALIGNLEFT    109
#define IDM_ALIGNCENTER  110
#define IDM_ALIGNRIGHT   111
#define IDM_BOLD         112

' Ids used inside the host-owned dropdown menus. These never enter the OVERFLOW menu's
' command stream -- that one uses (item index + 1) -- so they cannot collide with it.
#define IDM_RUN_DEBUG    200
#define IDM_RUN_RELEASE  201
#define IDM_RUN_PROFILE  202
#define IDM_HELP_INDEX   210
#define IDM_HELP_ABOUT   211

' The radio group the three alignment toggles share. Any non-zero value; it is a group
' IDENTITY, not an index.
#define GROUP_ALIGN      1

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
