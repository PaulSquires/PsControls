'    PsBufferPaint - demo harness
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
declare function frmMain_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                                  byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
