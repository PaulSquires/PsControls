'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' frmMain control ids
#define IDC_FRMMAIN_MENUBAR     1000

' Menu bar item ids (bar entries, not commands)
#define IDMB_FILE               100
#define IDMB_EDIT               101
#define IDMB_VIEW               102
#define IDMB_HELP               103

' Command ids (popup items)
#define IDM_FILE_NEW            4001
#define IDM_FILE_OPEN           4002
#define IDM_FILE_WORDWRAP       4003
#define IDM_FILE_DISABLED       4004
#define IDM_FILE_EXIT           4005
#define IDM_FILE_RECENT         4010    ' submenu parent row on the File menu

#define IDM_MRU_BASE            4100    ' + 1..3 = the rebuilt recent files
#define IDM_MRU_MORE            4110    ' depth-2 submenu parent row on the MRU menu
#define IDM_MRU_OLD_A           4121
#define IDM_MRU_OLD_B           4122

#define IDM_EDIT_UNDO           4201
#define IDM_EDIT_REDO           4202
#define IDM_EDIT_CUT            4203
#define IDM_EDIT_COPY           4204
#define IDM_EDIT_PASTE          4205

#define IDM_VIEW_EXPLORER       4301
#define IDM_VIEW_OUTPUT         4302
#define IDM_VIEW_FULLSCREEN     4303

#define IDM_HELP_ABOUT          4401

#define IDM_CTX_INSERT          4501
#define IDM_CTX_DELETE          4502
#define IDM_CTX_RENAME          4503
#define IDM_CTX_ADVANCED        4510    ' submenu parent row on the context menu
#define IDM_CTX_ADV_ONE         4511
#define IDM_CTX_ADV_TWO         4512

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
