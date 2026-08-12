'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' ========================================================================================
' The demo is a SPECIMEN SHEET, not an application: a 2x3 grid where every cell is one
' chart configured differently, with a caption above it saying which. Four of the six
' reproduce the reference screenshots this control was drawn from; the other two show the
' options those four do not use.
'
' The whole sheet is re-themed LIGHT through PsChart_SetColors, because the control's own
' defaults are the family's dark palette and the references are white. That is deliberate
' and is exactly how a host is expected to theme it.
' ========================================================================================

#define IDC_FRMMAIN_CHART_FIRST   100

enum
    CHART_LINE = 0          ' the reference line chart
    CHART_COLUMN            ' the reference column chart
    CHART_HLOC              ' the reference HLOC chart, 60 dated bars
    CHART_PIE               ' the reference pie, value labels inside
    CHART_DOUGHNUT          ' doughnut + exploded slice + name/percent labels on leaders
    CHART_NEGATIVE          ' columns straddling zero, and a spline series with symbols
    CHART_COUNT
end enum

declare function frmMain_Show( byval hWndParent as HWND ) as LRESULT
