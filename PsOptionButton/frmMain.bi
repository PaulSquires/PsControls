'    PsOptionButton - reusable owner-drawn option button (radio button) control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


' The demo lays out SIX groups down one column. Each group is a run of consecutive rows sharing
' a group id, which is exactly how a host would write one.
'
'   rows  0-2    group 1   Quality           three options, default look, "Balanced" pre-checked
'   rows  3-5    group 2   Update channel    the MIDDLE option is disabled -- arrows step over it
'   rows  6-7    group 3   Units             BOXALIGN_RIGHT, sized to a full row width
'   rows  8-10   group 4   Caption align     TEXTALIGN LEFT / CENTER / RIGHT, with slack
'   rows 11-12   group 5   Paint callback    a host renderer replacing the built-in one
'   rows 13-14   group 6   Icon-sized        no caption, tooltip callback, a 28px box
'
' PLUS TWO PLAIN WIN32 BUTTONS at the bottom, and they are not decoration. PsComboBox's demo
' hosted three real BUTTONs, Tab moved briskly between THOSE, and the interactive pass concluded
' navigation worked while never once reaching a combobox (C:\dev\Learnings.md). A demo made only
' of the control under test is exactly the shape that hides a Tab bug -- so the walk here has to
' enter the groups AND leave them again.
'
' EIGHT TAB STOPS, NOT SEVENTEEN: one per group (the checked member, or the first enabled one)
' plus the two buttons. That is the point of the whole control, and it is what you are checking
' when you hold Tab down.

#define OPTION_COUNT   15

#define IDC_FRMMAIN_OPT_FIRST   1000
#define IDC_FRMMAIN_BTN_OK      1900
#define IDC_FRMMAIN_BTN_CANCEL  1901

' Group ids. Deliberately not 0-based, so a forgotten SetGroupID would show up as a merge into
' the default group rather than silently joining a real one.
#define GRP_QUALITY     1
#define GRP_CHANNEL     2
#define GRP_UNITS       3
#define GRP_ALIGN       4
#define GRP_PAINT       5
#define GRP_ICON        6

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
