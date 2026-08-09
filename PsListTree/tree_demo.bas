'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

' ========================================================================================
'  PsListTree TREE demo -- a focused harness for the treeview features only.
'
'  Unlike main.bas (which exercises the whole control -- flat lists, groups, columns, drag
'  reorder, spanned/non-selectable rows), this demo shows ONLY what the tree refactor added:
'
'    * unlimited parent/child depth built with PsListTree_AddNode
'    * control-drawn expand/collapse twisties + per-depth indentation
'    * expand/collapse by twisty click, by Left/Right, and by double-click
'    * in-place LABEL EDITING: F2 or a click on the already-current row; ENTER commits,
'      ESC cancels, clicking away commits
'
'  It is a single self-contained file on purpose, so it reads as "here is how you drive a
'  PsListTree as a tree". Build with build_tree.bat.
' ========================================================================================

#define UNICODE
#define _WIN32_WINNT &h0602

#include once "windows.bi"
#include once "AfxNova\CWindow.inc"
#include once "AfxNova\AfxStr.inc"
#include once "AfxNova\AfxGdiplus.inc"

using AfxNova


#define APPNAME       wstr("PsListTree Tree Demo")
#define APPCLASSNAME  wstr("pslisttree_tree_demo_class")
#define GUIFONT       wstr("Segoe UI")
#define SYMBOLFONT    wstr("Segoe Fluent Icons")
#define IDC_TREE      1000

' Fonts (kept alive for the app's lifetime; the control only borrows the handles).
dim shared as HFONT ghRowFont      ' row text
dim shared as HFONT ghTwistyFont   ' the chevron glyphs

dim shared as HWND HWND_FRMTREE     ' the top-level window
dim shared as HWND ghTree           ' the PsListTree instance, in tree mode

' A small dark palette so the demo looks like a real tree pane.
type TREETHEME_TYPE
    BackColor       as COLORREF = BGR(30, 34, 40)
    ForeColor       as COLORREF = BGR(215, 218, 224)
    BackColorHot    as COLORREF = BGR(44, 49, 58)
    ForeColorHot    as COLORREF = BGR(235, 238, 244)
    BackColorSelect as COLORREF = BGR(38, 79, 120)
    ForeColorSelect as COLORREF = BGR(255, 255, 255)
    Twisty          as COLORREF = BGR(150, 156, 166)
    FocusAccent     as COLORREF = BGR(86, 156, 214)
    Panel           as COLORREF = BGR(24, 27, 32)
end type
dim shared theme as TREETHEME_TYPE


' The control stack. Order matters: PsListTree calls into PsTextBox (its label editor) and
' PsColumnHeader / PsVScrollBar, so their implementations compile first.
#include once "PsBufferPaint.inc"
#include once "PsVScrollBar.inc"
#include once "PsColumnHeader.inc"
#include once "PsPopupMenu.inc"
#include once "PsTextBox.inc"
#include once "PsListTree.inc"


' ========================================================================================
' Row painter. A pure tree has no columns and no group headers, so every row falls through
' to the single-cell path: fill the background for the row's state, then draw the caption
' at p->indent (the control has already reserved space for the depth indent + twisty band)
' and let the control paint the twisty on top. A focus bar marks the caret row.
' ========================================================================================
sub Tree_PaintRow( byval p as PSLISTTREE_PAINTINFO ptr )
    dim as COLORREF backclr, foreclr
    if p->isSelected then
        backclr = theme.BackColorSelect : foreclr = theme.ForeColorSelect
    elseif p->isHot then
        backclr = theme.BackColorHot    : foreclr = theme.ForeColorHot
    else
        backclr = theme.BackColor       : foreclr = theme.ForeColor
    end if
    p->b->SetForeColor( foreclr )
    p->b->SetBackColor( backclr )
    p->b->PaintRect( @p->rc )

    ' Caption, indented by depth. p->indent already includes the reserved twisty band, so a
    ' parent's caption and a leaf's caption at the same depth line up.
    dim as RECT rcText = p->rc
    rcText.left += p->indent
    p->b->SetFont( ghRowFont )
    p->b->PaintText( p->wszCaption, @rcText, DT_LEFT )

    ' The control draws the twisty itself (ShowTwisty is on) after this callback returns.

    if p->isFocused then
        dim as RECT rcBar = p->rc
        rcBar.right = rcBar.left + 3
        p->b->SetBackColor( theme.FocusAccent )
        p->b->PaintRect( @rcBar )
    end if
end sub


' ========================================================================================
' Tree callbacks. Editing allows any non-blank rename; a blank is rejected (the row keeps
' its text). Expand/collapse just traces. newText is converted to a plain string before
' printing -- printing a DWSTRING directly interleaves nulls.
' ========================================================================================
function Tree_BeginLabelEdit( byval h as HWND, byval row as integer ) as boolean
    return true
end function

function Tree_EndLabelEdit( byval h as HWND, byval row as integer, byval newText as DWSTRING ) as boolean
    dim as string s = newText
    if len(trim(s)) = 0 then
        print "  rename rejected (blank); row " & str(row) & " unchanged"
        return false
    end if
    print "  row " & str(row) & " renamed to: " & s
    return true
end function

sub Tree_ExpandCollapse( byval h as HWND, byval row as integer, byval bExpanded as boolean )
    dim as string s = "  row " & str(row)
    if bExpanded then s &= " expanded" else s &= " collapsed"
    print s
end sub


' ========================================================================================
' Add a node and return its model index. Thin wrapper purely to keep the tree-building
' code below compact and readable.
' ========================================================================================
function AddNode( byval parentRow as integer, byval caption as DWSTRING ) as integer
    return PsListTree_AddNode( ghTree, parentRow, caption )
end function


' ========================================================================================
' Window procedure.
' ========================================================================================
function frmTree_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                          byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT
    select case uMsg
        case WM_SIZE
            if (wParam <> SIZE_MINIMIZED) andalso (ghTree <> 0) then
                dim as RECT rc : GetClientRect( hwnd, @rc )
                ' Leave a small margin so the pane reads as an inset panel.
                dim as integer m = 12
                SetWindowPos( ghTree, 0, m, m, (rc.right - rc.left) - m*2, (rc.bottom - rc.top) - m*2, SWP_NOZORDER or SWP_SHOWWINDOW )
            end if
            return 0

        case WM_ERASEBKGND
            return true                          ' painted in WM_PAINT; avoid a flash

        case WM_PAINT
            dim as PsBufferPaint b
            b.BeginDoubleBuffer( hwnd )
            b.SetBackColor( theme.Panel )
            b.PaintClientRect()
            b.EndDoubleBuffer()
            return 0

        case WM_CLOSE
            DestroyWindow( hwnd )
            return 0

        case WM_DESTROY
            PostQuitMessage( 0 )
            return 0
    end select

    return DefWindowProcW( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' WinMain
' ========================================================================================
function WinMain( byval hInstance as HINSTANCE, byval hPrevInstance as HINSTANCE, _
                  byval szCmdLine as zstring ptr, byval nCmdShow as long ) as long

    CoInitialize( null )

    ' The twisties are Segoe Fluent Icons glyphs; load the bundled font privately.
    dim as DWSTRING wszFontFile = AfxGetExePathName + "SegoeFluentIcons.ttf"
    if AddFontResourceEx( wszFontFile.vptr, FR_PRIVATE, NULL ) = 0 then
        MessageBox( 0, "Unable to load 'SegoeFluentIcons.ttf'. Aborting.", "Error", MB_OK or MB_ICONWARNING )
        return 1
    end if

    ' GDI+ must be running before the first WM_PAINT builds a buffer and outlive them all.
    dim as ULONG_PTR gdipToken = AfxGdipInit()

    dim pWindow as CWindow ptr = new CWindow( APPCLASSNAME )
    ghRowFont    = pWindow->CreateFont( GUIFONT, 10, FW_NORMAL )
    ghTwistyFont = pWindow->CreateFont( SYMBOLFONT, 9, FW_NORMAL )

    HWND_FRMTREE = pWindow->Create( null, APPNAME, @frmTree_WndProc, 120, 120, 460, 620 )

    ' ---- Create the control and put it in TREE mode ----
    ghTree = PsListTree_Create( HWND_FRMTREE, IDC_TREE )
    PsListTree_SetPaintCallback( ghTree, @Tree_PaintRow )
    PsListTree_SetBackColor( ghTree, theme.BackColor )
    PsListTree_SetFont( ghTree, ghRowFont )
    PsListTree_SetExtendedSelect( ghTree, true )              ' explorer-style selection
    ' Tree visuals: per-depth indent + control-drawn chevrons.
    PsListTree_SetTreeIndent( ghTree, true )
    PsListTree_ShowTwisty( ghTree, true )
    PsListTree_SetTwistyFont( ghTree, ghTwistyFont )
    PsListTree_SetTwistyColor( ghTree, theme.Twisty )
    ' In-place editing: F2, or a click on the row that is already current.
    PsListTree_EnableLabelEdit( ghTree, true )
    PsListTree_SetClickToEdit( ghTree, true )
    PsListTree_SetBeginLabelEditCallback( ghTree, @Tree_BeginLabelEdit )
    PsListTree_SetEndLabelEditCallback( ghTree, @Tree_EndLabelEdit )
    PsListTree_SetExpandCollapseCallback( ghTree, @Tree_ExpandCollapse )

    ' ---- Build a deep tree (batched so the visible map rebuilds once) ----
    PsListTree_BeginUpdate( ghTree )
    dim as integer rProj = AddNode( -1, "MyProject" )
        dim as integer rSrc = AddNode( rProj, "src" )
            dim as integer rEngine = AddNode( rSrc, "engine" )
                AddNode( rEngine, "render.bas" )
                AddNode( rEngine, "physics.bas" )
                dim as integer rMath = AddNode( rEngine, "math" )
                    AddNode( rMath, "vec3.bi" )
                    AddNode( rMath, "matrix.bi" )
                    AddNode( rMath, "quat.bi" )
            dim as integer rUI = AddNode( rSrc, "ui" )
                AddNode( rUI, "window.bas" )
                AddNode( rUI, "widgets.bas" )
            AddNode( rSrc, "main.bas" )
        dim as integer rAssets = AddNode( rProj, "assets" )
            AddNode( rAssets, "logo.png" )
            AddNode( rAssets, "theme.json" )
        AddNode( rProj, "README.md" )
    dim as integer rBuild = AddNode( -1, "build" )
        AddNode( rBuild, "debug" )
        AddNode( rBuild, "release" )
    AddNode( -1, "LICENSE.txt" )                             ' a leaf root -- no twisty
    ' Start one branch collapsed so a collapsed twisty is visible at depth from the outset.
    PsListTree_SetNodeCollapsed( ghTree, rMath, true )
    PsListTree_EndUpdate( ghTree )
    PsListTree_SetCurSel( ghTree, 0 )

    ' Fill the window and show it. The control's container is created WITHOUT WS_VISIBLE
    ' (like every PsListTree), so SWP_SHOWWINDOW here is what actually reveals it.
    dim as RECT rc : GetClientRect( HWND_FRMTREE, @rc )
    SetWindowPos( ghTree, 0, 12, 12, rc.right - 24, rc.bottom - 24, SWP_NOZORDER or SWP_SHOWWINDOW )
    ShowWindow( HWND_FRMTREE, SW_SHOW )
    UpdateWindow( HWND_FRMTREE )

    print "PsListTree TREE demo"
    print "-------------------"
    print "  Twisty / Left / Right / dbl-click : expand & collapse (any depth)"
    print "  Arrows / PageUp-Dn / Home / End   : navigate"
    print "  F2, or click the already-current row : rename in place"
    print "  ENTER commits the edit, ESC cancels, clicking away commits"
    print ""

    ' Message loop. PsListTree_FilterMessage MUST run before IsDialogMessage: while an edit is
    ' active it claims ENTER/ESC, which IsDialogMessage would otherwise eat as default-button /
    ' cancel keys before they ever reached the editor.
    dim uMsg as MSG
    do while GetMessage( @uMsg, null, 0, 0 )
        if uMsg.message = WM_QUIT then exit do
        if PsListTree_FilterMessage( @uMsg ) then continue do
        if IsDialogMessage( HWND_FRMTREE, @uMsg ) = 0 then
            TranslateMessage( @uMsg )
            DispatchMessage( @uMsg )
        end if
    loop

    if len(wszFontFile) then RemoveFontResourceEx( wszFontFile.vptr, FR_PRIVATE, NULL )
    AfxGdipShutdown( gdipToken )
    CoUninitialize()
    function = uMsg.wParam
end function

end WinMain( GetModuleHandle(null), null, command(), SW_NORMAL )
