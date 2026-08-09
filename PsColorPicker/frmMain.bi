'    PsColorPicker - reusable owner-drawn modal colour picker popup
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


' THE DEMO HOSTS NO PICKER. That is the change the popup conversion made to this file, and it is
' the point: PsColorPicker is not a control you place any more, it is a call you make. So the
' form is three ordinary rows -- a swatch, its value, and a button that opens the popup -- and
' the whole integration is one line per row.
'
'   0  Anchored, opaque      DoModalForRect against the button's own rect, alpha hidden. The
'                            dropdown shape: the popup hangs off the control that opened it.
'   1  Anchored, with alpha   the same, with the alpha row shown -- which is now the DEFAULT, so
'                            this row is the one that does nothing special at all.
'   2  Centred, host-styled   DoModal, centred on the form, reached through Create/RunModal so the
'                            demo can set a paint callback first. That callback calls
'                            PsColorPicker_RenderInfo and then draws an accent frame over the
'                            body -- the DECORATE rather than REPLACE pattern RenderInfo exists
'                            for, and the instance the host-side flood probe measures, because
'                            every occurrence of the PaintBorderRect flood bug in this family has
'                            been in a HOST callback and never in a control's own painter.
'
' THE TAB-ORDER WORRY THIS FILE USED TO CARRY IS GONE WITH THE EMBEDDING. It said a demo made
' only of the control under test is the shape that hides a Tab bug -- true, and the reason the
' old demo mixed in plain Win32 BUTTONs. The popup is now the ONLY window in its own message
' loop, so there is no host tab order for it to be skipped in. What replaced that worry is the
' opposite one, and the buttons below are what exercise it: after the popup closes, focus must
' come back to the form.

#define ROW_COUNT   3

#define IDC_FRMMAIN_BTN_FIRST     1000
#define IDC_FRMMAIN_BTN_CLOSE     1901

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
