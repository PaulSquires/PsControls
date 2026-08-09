'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"
#include once "PsBufferPaint.bi"


' One demo row: the label the form draws, the small print under it, and the last result.
type DEMO_ROW
    wszLabel  as DWSTRING
    wszNote   as DWSTRING          ' small print under the label
    wszResult as DWSTRING          ' what the last call answered, "" until it is called
end type
dim shared gRows(0 to DEMO_ROW_COUNT - 1) as DEMO_ROW


' ========================================================================================
' Row geometry. The raiser button is right-aligned against the margin at its own ideal size.
' ========================================================================================
#define ROW_HEIGHT      52
#define ROW_TOP         14
#define ROW_MARGIN      22

private function RowRect( byval hwnd as HWND, byval idx as long ) as RECT
    dim as RECT rc, rcRow
    GetClientRect( hwnd, @rc )
    dim as long h = ROW_HEIGHT
    dim as long t = ROW_TOP
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    if pWindow then
        h = pWindow->ScaleY(ROW_HEIGHT)
        t = pWindow->ScaleY(ROW_TOP)
    end if
    SetRect( @rcRow, rc.left, t + (idx * h), rc.right, t + (idx * h) + h )
    return rcRow
end function


' ========================================================================================
' Turn an answer back into something readable. The point of printing it is that Cancel and Esc
' and the X all produce the SAME id -- which is the documented contract, and looks like a bug
' until you have seen it stated.
' ========================================================================================
private function ResultName( byval id as long ) as DWSTRING
    select case id
    case IDOK     : return "IDOK"
    case IDCANCEL : return "IDCANCEL"
    case IDYES    : return "IDYES"
    case IDNO     : return "IDNO"
    case IDRETRY  : return "IDRETRY"
    end select
    return wstr( id )
end function


' ========================================================================================
' The LIGHT colour set, matched to the reference screenshot. The control's own defaults are
' DARK (inherited from PSBUTTON_COLORS), so this is what a host does to go the other way -- and
' it is the only place in this repo where the screenshot's exact palette lives.
' ========================================================================================
private sub ApplyLightTheme( byval hBox as HWND )
    dim c as MBX_COLORS
    PsMessageBox_GetColors( hBox, @c )

    c.BorderColor           = BGR(200,200,200)
    c.CaptionBackColor      = BGR(243,243,243)
    c.CaptionForeColor      = BGR( 26, 26, 26)
    c.CloseBackColor        = BGR(243,243,243)
    c.CloseForeColor        = BGR( 26, 26, 26)
    c.CloseBackColorHot     = BGR(196, 43, 28)
    c.CloseForeColorHot     = BGR(255,255,255)
    c.CloseBackColorPressed = BGR(200, 70, 60)
    c.CloseForeColorPressed = BGR(255,255,255)
    c.BackColor             = BGR(255,255,255)
    c.ForeColor             = BGR( 16, 82,166)      ' the screenshot's blue message text
    c.FooterBackColor       = BGR(240,240,240)
    c.DividerColor          = BGR(224,224,224)
    c.IconColorWarning      = BGR(247,182, 21)
    PsMessageBox_SetColors( hBox, @c )

    ' The buttons have to move with it, or a light box gets the family's dark buttons.
    dim b as PSBUTTON_COLORS
    b.BackColor           = BGR(251,251,251)
    b.ForeColor           = BGR( 26, 26, 26)
    b.BorderColor         = BGR(209,209,209)
    b.IconColor           = BGR( 26, 26, 26)
    b.BackColorHot        = BGR(246,246,246)
    b.ForeColorHot        = BGR( 26, 26, 26)
    b.BorderColorHot      = BGR(200,200,200)
    b.IconColorHot        = BGR( 26, 26, 26)
    b.BackColorPressed    = BGR(245,245,245)
    b.ForeColorPressed    = BGR( 96, 96, 96)
    b.BorderColorPressed  = BGR(205,205,205)
    b.IconColorPressed    = BGR( 96, 96, 96)
    b.BackColorDisabled   = BGR(251,251,251)
    b.ForeColorDisabled   = BGR(160,160,160)
    b.BorderColorDisabled = BGR(230,230,230)
    b.IconColorDisabled   = BGR(160,160,160)
    b.FocusRingColor      = BGR( 26,111,204)
    b.DefaultBorderColor  = BGR( 26,111,204)
    PsMessageBox_SetButtonColors( hBox, @b )
end sub


' ========================================================================================
' Every box in this demo needs the same three fonts, and forgetting the glyph font is the most
' likely host mistake -- the icon silently becomes a missing-glyph box. Row 10 deliberately
' skips this to show what that looks like.
'
' NOTE THE TWO GLYPH FONTS. The message icon sits in a 32px cell and the close X is caption
' chrome, so one size cannot serve both; see PsMessageBox_SetCloseGlyphFont.
' ========================================================================================
private sub ApplyDemoFonts( byval hBox as HWND )
    PsMessageBox_SetFont( hBox, ghFont(GUIFONT_10) )
    PsMessageBox_SetCaptionFont( hBox, ghFont(GUIFONT_10) )
    PsMessageBox_SetGlyphFont( hBox, ghFont(SYMBOLFONT_20) )
    PsMessageBox_SetCloseGlyphFont( hBox, ghFont(SYMBOLFONT_10) )
end sub


' ========================================================================================
' A custom painter, replacing the built-in one wholesale (row 8).
'
' THE ONE TRAP THIS DEMONSTRATES AVOIDING. PaintBorderRect goes through PaintRectFactory, which
' ALWAYS fills the rect with _backcolor before stroking it -- so reaching for it to draw the
' window outline or the footer divider floods everything underneath. That mistake has been made
' three separate times in this family (PsToggle, PsComboBox, PsNumericUpDown), every time by
' copying a sibling's callback. PaintLine and PaintRoundOutline stroke without filling.
'
' The demo asserts it is not doing that, with PsMessageBox_CountRenderedTones -- which is exactly
' the pattern a real host writing its own painter should copy.
' ========================================================================================
private sub PsMessageBox_DemoPaint( byval p as MBX_PAINTINFO ptr )
    if p = 0 then exit sub

    ' A full-height accent stripe down the left edge instead of a caption band fill.
    dim as RECT rcStripe = p->rcClient
    rcStripe.right = rcStripe.left + 6
    p->b->SetBackColor( theme.BackColorSelect )
    p->b->PaintRect( @rcStripe )

    ' The caption, drawn in the accent colour on the body's own background.
    if IsRectEmpty( @p->rcCaptionText ) = 0 then
        dim as RECT rcCT = p->rcCaptionText
        rcCT.left += 6
        p->b->SetFont( ghFont(GUIFONTBOLD_10) )
        p->b->SetColors( theme.FocusAccent, theme.BackColor )
        p->b->PaintText( p->wszCaption, @rcCT, DT_LEFT or DT_END_ELLIPSIS )
    end if

    ' The close X still has to be drawn, or the box has no visible way out but Esc.
    if IsRectEmpty( @p->rcClose ) = 0 then
        dim as RECT rcX = p->rcClose
        dim as COLORREF clrX = theme.ForeColor
        if p->isCloseHot then clrX = theme.ForeColorSelect
        p->b->SetFont( ghFont(SYMBOLFONT_10) )
        p->b->SetColors( clrX, theme.BackColor )
        p->b->PaintText( wchr(PSMESSAGEBOX_GLYPH_CLOSE), @rcX, DT_CENTER )
    end if

    ' The icon and the message, with the SAME font and the SAME wrap flag the control measured
    ' with -- a different either, and the height it computed is a lie.
    if (IsRectEmpty( @p->rcIcon ) = 0) andalso (len( p->wszGlyph ) > 0) then
        dim as RECT rcI = p->rcIcon
        p->b->SetFont( ghFont(SYMBOLFONT_20) )
        p->b->SetColors( p->clrIcon, theme.BackColor )
        p->b->PaintText( p->wszGlyph, @rcI, DT_CENTER )
    end if

    if (IsRectEmpty( @p->rcText ) = 0) andalso (len( p->wszText ) > 0) then
        dim as RECT rcT = p->rcText
        p->b->SetFont( ghFont(GUIFONT_10) )
        p->b->SetColors( theme.ForeColor, theme.BackColor )
        p->b->PaintTextEx( p->wszText, @rcT, DT_LEFT or DT_TOP or DT_WORDBREAK )
    end if

    ' A hairline over the footer instead of a filled band. PaintLine, NOT PaintBorderRect.
    if IsRectEmpty( @p->rcFooter ) = 0 then
        p->b->SetPenColor( theme.Divider )
        p->b->PaintLine( 1, p->rcFooter.left + 6, p->rcFooter.top, _
                            p->rcFooter.right, p->rcFooter.top )
    end if

    ' The window outline. PaintRoundOutline strokes; PaintBorderRect would flood.
    p->b->SetPenColor( theme.Divider )
    p->b->PaintRoundOutline( @p->rcClient, 0, 1 )
end sub


' ========================================================================================
' The NESTED case (row 9). A message callback on the OUTER box watches for a click on its body
' and raises a SECOND modal box from inside the first one's loop.
'
' This is the thing most likely to be broken in a control that runs its own pump: the inner
' box's loop has to end without ending the outer one, and the outer box's parent must stay
' disabled throughout. Returning FALSE lets the outer box carry on handling the click normally.
' ========================================================================================
private function PsMessageBox_DemoNestedMessage( byval m as MBX_MESSAGEINFO ptr ) as boolean
    if m = 0 then return false
    if m->uMsg <> WM_LBUTTONDOWN then return false

    ' Ignore clicks on the caption band -- those are the drag, and the close X lives there too.
    dim as RECT rcCap
    PsMessageBox_GetCaptionRect( m->hMsgBox, rcCap )
    dim as POINT pt
    pt.x = cast( short, loword(m->lParam) )
    pt.y = cast( short, hiword(m->lParam) )
    if PtInRect( @rcCap, pt ) then return false

    dim as HWND hInner = PsMessageBox_Create( m->hMsgBox )
    ApplyDemoFonts( hInner )
    PsMessageBox_SetCaption( hInner, "Nested" )
    PsMessageBox_SetText( hInner, "This box was raised from inside the first box's own modal " & _
                                 "loop. Dismissing it must return you to the first box, not " & _
                                 "to the main window." )
    PsMessageBox_SetIconKind( hInner, MBX_ICON_INFO )
    PsMessageBox_AddPreset( hInner, MBX_BTN_OK )
    dim as long r = PsMessageBox_DoModal( hInner )

    dim as string s = "  nested box returned " & str(r)
    print s
    return false
end function


' ========================================================================================
' Raise the box for one row and record what it answered.
' ========================================================================================
private sub RaiseBox( byval hwnd as HWND, byval idx as long )
    dim as long r = 0

    select case idx

    case DEMO_OK
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Information" )
        PsMessageBox_SetText( h, "The project was built successfully." )
        PsMessageBox_SetIconKind( h, MBX_ICON_INFO )
        PsMessageBox_AddPreset( h, MBX_BTN_OK )
        r = PsMessageBox_DoModal( h )

    case DEMO_OKCANCEL
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Confirm" )
        PsMessageBox_SetText( h, "The selected files will be moved to the recycle bin." )
        PsMessageBox_SetIconKind( h, MBX_ICON_QUESTION )
        PsMessageBox_AddPreset( h, MBX_BTN_OKCANCEL )
        r = PsMessageBox_DoModal( h )

    case DEMO_YESNO
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Warning" )
        PsMessageBox_SetText( h, "Reload the file from disk and lose your edits?" )
        PsMessageBox_SetIconKind( h, MBX_ICON_WARNING )
        PsMessageBox_AddPreset( h, MBX_BTN_YESNO )
        r = PsMessageBox_DoModal( h )

    case DEMO_YESNOCANCEL
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Warning" )
        PsMessageBox_SetText( h, "Three files have unsaved changes. Save them all before closing?" )
        PsMessageBox_SetIconKind( h, MBX_ICON_WARNING )
        PsMessageBox_AddPreset( h, MBX_BTN_YESNOCANCEL )
        r = PsMessageBox_DoModal( h )

    case DEMO_SAVE_LIGHT
        ' The reference screenshot, colour for colour.
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        ApplyLightTheme( h )
        PsMessageBox_SetCaption( h, "Warning" )
        PsMessageBox_SetText( h, "This buffer contains unsaved edits. Do you want to save it?" )
        PsMessageBox_SetIconKind( h, MBX_ICON_WARNING )
        PsMessageBox_AddPreset( h, MBX_BTN_SAVE_DONTSAVE_CANCEL )
        r = PsMessageBox_DoModal( h )

    case DEMO_ERROR_LONG
        ' The auto-height path: far more text than fits one line, so DT_CALCRECT decides how
        ' tall the box is. Note the box does NOT get wider than nMaxWidth for this.
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Build failed" )
        PsMessageBox_SetText( h, "The compiler reported 3 errors and 11 warnings while building " & _
                                "the active configuration. The first error was in " & _
                                "modParser.inc at line 1284: 'Expected identifier, found " & _
                                "HFONT'. Open the compiler log for the full output?" )
        PsMessageBox_SetIconKind( h, MBX_ICON_ERROR )
        PsMessageBox_AddButton( h, "Open Log", IDYES )
        PsMessageBox_AddButton( h, "Close", IDNO )
        PsMessageBox_SetDefaultButton( h, 0 )
        r = PsMessageBox_DoModal( h )

    case DEMO_BARE
        ' No icon and no title. Both bands are still there -- the caption band is declared, not
        ' measured, so it does not collapse just because it has nothing in it.
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetText( h, "No icon, no title, one button." )
        PsMessageBox_AddPreset( h, MBX_BTN_OK )
        PsMessageBox_SetBeep( h, false )
        r = PsMessageBox_DoModal( h )

    case DEMO_FOCUSSPLIT
        ' Default and focus on DIFFERENT buttons: Delete carries the accent border and answers
        ' Enter, but Cancel is what has the focus ring when the box opens. SetFocusButton must
        ' be called AFTER SetDefaultButton or the default takes the focus with it.
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Delete" )
        PsMessageBox_SetText( h, "Permanently delete 42 items? Delete is the default button, " & _
                                "but Cancel has the focus." )
        PsMessageBox_SetIconKind( h, MBX_ICON_WARNING )
        PsMessageBox_AddButton( h, "Delete", IDYES )
        PsMessageBox_AddButton( h, "Cancel", IDCANCEL )
        PsMessageBox_SetDefaultButton( h, 0 )
        PsMessageBox_SetFocusButton( h, 1 )
        r = PsMessageBox_DoModal( h )

    case DEMO_CUSTOMPAINT
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Custom painter" )
        PsMessageBox_SetText( h, "The built-in painter has been replaced wholesale. The buttons " & _
                                "still paint themselves -- they are real child windows." )
        PsMessageBox_SetIconKind( h, MBX_ICON_INFO )
        PsMessageBox_AddPreset( h, MBX_BTN_OKCANCEL )
        PsMessageBox_SetPaintCallback( h, @PsMessageBox_DemoPaint )
        r = PsMessageBox_DoModal( h )

    case DEMO_NESTED
        dim as HWND h = PsMessageBox_Create( hwnd )
        ApplyDemoFonts( h )
        PsMessageBox_SetCaption( h, "Nesting" )
        PsMessageBox_SetText( h, "Click anywhere in this message area (not the caption) to " & _
                                "raise a second modal box on top of this one." )
        PsMessageBox_SetIconKind( h, MBX_ICON_QUESTION )
        PsMessageBox_AddPreset( h, MBX_BTN_OK )
        PsMessageBox_SetMessageCallback( h, @PsMessageBox_DemoNestedMessage )
        r = PsMessageBox_DoModal( h )

    case DEMO_ONELINER
        ' The convenience wrapper. It sets NO fonts, so the icon comes out as a missing-glyph
        ' box -- deliberately shown rather than papered over, because that is exactly what a
        ' host gets if it reaches for the one-liner and expects icons.
        r = PsMessageBox_Show( hwnd, "Raised with the one-line PsMessageBox_Show wrapper. " & _
                                    "It sets no fonts, so the icon is a missing glyph.", _
                                    "One-liner", MBX_ICON_INFO, MBX_BTN_OKCANCEL )
    end select

    gRows(idx).wszResult = "returned " + ResultName( r )
    InvalidateRect( hwnd, NULL, TRUE )
end sub


' The raiser buttons all share one click callback; the row is recovered from the control id.
sub frmMain_OnRaiseClicked( byval hButton as HWND, byval id as long )
    dim as long idx = id - IDC_FRMMAIN_BTN_FIRST
    if (idx < 0) orelse (idx >= DEMO_ROW_COUNT) then exit sub
    RaiseBox( HWND_FRMMAIN, idx )
end sub


' ========================================================================================
' Probe the HOST's OWN paint callback, offscreen, at startup.
'
' THIS IS THE PATTERN A REAL HOST SHOULD COPY. The control's self-test can only ever measure the
' BUILT-IN painter -- it knows nothing about PsMessageBox_DemoPaint -- and a host callback is
' exactly where the PaintBorderRect flood has shipped from three times in this family.
'
' The thresholds are calibrated to THIS callback, not copied from the control's. The demo
' painter draws no filled caption or footer band (it uses an accent stripe and a hairline), so
' its healthy numbers are its own; they were measured, and a flood reads 1-2.
' ========================================================================================
private sub frmMain_ProbeCustomPainter( byval hwnd as HWND )
    if environ("PSMESSAGEBOX_SELFTEST") <> "1" then exit sub

    dim as HWND h = PsMessageBox_Create( hwnd )
    if h = 0 then exit sub
    ApplyDemoFonts( h )
    PsMessageBox_SetCaption( h, "Custom painter" )
    PsMessageBox_SetText( h, "The built-in painter has been replaced wholesale. The buttons " & _
                            "still paint themselves -- they are real child windows." )
    PsMessageBox_SetIconKind( h, MBX_ICON_INFO )
    PsMessageBox_AddPreset( h, MBX_BTN_OKCANCEL )
    PsMessageBox_SetPaintCallback( h, @PsMessageBox_DemoPaint )
    PsMessageBox_LayoutForTest( h )

    dim as long tCap  = PsMessageBox_CountRenderedTones( h, MBX_PART_CAPTION )
    dim as long tBody = PsMessageBox_CountRenderedTones( h, MBX_PART_BODY )
    dim as long tFoot = PsMessageBox_CountRenderedTones( h, MBX_PART_FOOTER )

    dim as string sHost = "PASS  "
    if (tCap < 10) orelse (tBody < 10) orelse (tFoot < 3) then sHost = "FAIL  "
    sHost &= "demo paint callback does not flood   [caption=" & str(tCap) & _
             " body=" & str(tBody) & " footer=" & str(tFoot) & "]"
    print sHost

    DestroyWindow( h )
end sub


' ========================================================================================
' Process WM_PAINT message for window/dialog: frmMain
' ========================================================================================
function frmMain_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PsBufferPaint b

    b.BeginDoubleBuffer(hwnd)
    b.SetBackColor( theme.BackColor )
    b.PaintClientRect()

    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to DEMO_ROW_COUNT - 1
        dim as RECT rcRow = RowRect( hwnd, i )

        dim as RECT rcText = rcRow
        rcText.left  += margin
        rcText.right -= margin
        rcText.bottom = rcRow.top + ((rcRow.bottom - rcRow.top) \ 2)
        b.SetColors( theme.ForeColor, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_10) )
        b.PaintText( gRows(i).wszLabel, @rcText, DT_LEFT )

        dim as DWSTRING wszNote = gRows(i).wszNote
        if len( gRows(i).wszResult ) then wszNote = wszNote + "   [" + gRows(i).wszResult + "]"
        dim as RECT rcNote = rcRow
        rcNote.left  += margin
        rcNote.right -= margin
        rcNote.top    = rcRow.top + ((rcRow.bottom - rcRow.top) \ 2)
        b.SetColors( theme.ForeColorDisabled, theme.BackColor )
        b.SetFont( ghFont(GUIFONT_9) )
        b.PaintText( wszNote, @rcNote, DT_LEFT )

        dim as RECT rcLine
        SetRect( @rcLine, rcRow.left + margin, rcRow.bottom - 1, rcRow.right - margin, rcRow.bottom )
        b.SetBackColor( theme.Divider )
        b.PaintRect( @rcLine )
    next

    b.EndDoubleBuffer()

    function = 0
end function


' ========================================================================================
' Place every raiser button at the right end of its row, at its own ideal size.
' ========================================================================================
sub frmMain_PositionButtons( byval hwnd as HWND )
    dim as RECT rc
    GetClientRect( hwnd, @rc )
    dim pWindow as CWindow ptr = AfxCWindowPtr(hwnd)
    dim as long margin = ROW_MARGIN
    if pWindow then margin = pWindow->ScaleX(ROW_MARGIN)

    for i as long = 0 to DEMO_ROW_COUNT - 1
        if IsWindow(ghRaise(i)) = 0 then continue for

        dim as RECT rcRow = RowRect( hwnd, i )
        dim as long iw, ih
        PsButton_GetIdealSize( ghRaise(i), iw, ih )
        if (iw <= 0) orelse (ih <= 0) then continue for

        dim as long x = rcRow.right - margin - iw
        dim as long y = rcRow.top + (((rcRow.bottom - rcRow.top) - ih) \ 2)
        SetWindowPos( ghRaise(i), 0, x, y, iw, ih, SWP_NOZORDER or SWP_NOACTIVATE )
    next
end sub


' ========================================================================================
' frmMain message handlers
' ========================================================================================
function frmMain_OnCreate( _
            byval hwnd as HWND, _
            byval lpCreateStructPtr as LPCREATESTRUCT _
            ) as boolean
    return true
end function

function frmMain_OnSize( _
            byval hwnd as HWND, _
            byval state as UINT, _
            byval cx as long, _
            byval cy as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        frmMain_PositionButtons( hwnd )
        InvalidateRect( hwnd, NULL, TRUE )
    end if
    function = 0
end function

function frmMain_OnClose( byval hwnd as HWND ) as LRESULT
    DestroyWindow(hwnd)
    function = 0
end function

function frmMain_OnDestroy( byval hwnd as HWND ) as LRESULT
    PostQuitMessage(0)
    function = 0
end function


function frmMain_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    select case uMsg
        HANDLE_MSG (hwnd, WM_CREATE,        frmMain_OnCreate)
        HANDLE_MSG (hwnd, WM_SIZE,          frmMain_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT,         frmMain_OnPaint)
        HANDLE_MSG (hwnd, WM_CLOSE,         frmMain_OnClose)
        HANDLE_MSG (hwnd, WM_DESTROY,       frmMain_OnDestroy)

        case WM_NCDESTROY
            ' delete the allocated memory for window
            dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
            if pWindow then delete(pWindow)
            HWND_FRMMAIN = 0
            return 0

        case WM_ERASEBKGND
            return true      ' prevents painting the background

        case WM_SYSCOMMAND
            if (wParam and &HFFF0) = SC_CLOSE then
                SendMessage( hwnd, WM_CLOSE, wParam, lParam )
                exit function
            end if

    end select

    ' for messages that we don't deal with
    function = DefWindowProc(HWnd, uMsg, wParam, lParam)

end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    '  Create the main window and child controls
    dim pWindow as CWindow ptr = new CWindow(APPCLASSNAME)

    ghFont(GUIFONT_9)      = pWindow->CreateFont(GUIFONT, 9, FW_NORMAL)
    ghFont(GUIFONT_10)     = pWindow->CreateFont(GUIFONT, 10, FW_NORMAL)
    ghFont(GUIFONTBOLD_10) = pWindow->CreateFont(GUIFONT, 10, FW_HEAVY)
    ghFont(SYMBOLFONT_10)  = pWindow->CreateFont(SYMBOLFONT, 10, FW_NORMAL)
    ghFont(SYMBOLFONT_20)  = pWindow->CreateFont(SYMBOLFONT, 20, FW_NORMAL)

    HWND_FRMMAIN = pWindow->Create( _
            null, APPNAME, @frmMain_WndProc, _
            100, 100, 700, 680 _
            )

    gRows(DEMO_OK).wszLabel           = "OK, info icon"
    gRows(DEMO_OK).wszNote            = "one button -- Esc must answer IDOK, not nothing"
    gRows(DEMO_OKCANCEL).wszLabel     = "OK / Cancel"
    gRows(DEMO_OKCANCEL).wszNote      = "Esc, the X and Alt+F4 all answer IDCANCEL"
    gRows(DEMO_YESNO).wszLabel        = "Yes / No"
    gRows(DEMO_YESNO).wszNote         = "no Cancel button, so the preset points Esc at IDNO"
    gRows(DEMO_YESNOCANCEL).wszLabel  = "Yes / No / Cancel"
    gRows(DEMO_YESNOCANCEL).wszNote   = "three buttons, right-aligned, each its own width"
    gRows(DEMO_SAVE_LIGHT).wszLabel   = "Save / Don't Save / Cancel  (light)"
    gRows(DEMO_SAVE_LIGHT).wszNote    = "the reference screenshot -- the control's defaults are dark"
    gRows(DEMO_ERROR_LONG).wszLabel   = "Long message, error icon"
    gRows(DEMO_ERROR_LONG).wszNote    = "DT_CALCRECT decides the height; the width stays at maxWidth"
    gRows(DEMO_BARE).wszLabel         = "No icon, no title"
    gRows(DEMO_BARE).wszNote          = "both bands are still there; the caption band is declared"
    gRows(DEMO_FOCUSSPLIT).wszLabel   = "Default and focus on different buttons"
    gRows(DEMO_FOCUSSPLIT).wszNote    = "Delete is default and owns Enter; Cancel has the ring"
    gRows(DEMO_CUSTOMPAINT).wszLabel  = "Custom paint callback"
    gRows(DEMO_CUSTOMPAINT).wszNote   = "built-in painter replaced wholesale; buttons still paint themselves"
    gRows(DEMO_NESTED).wszLabel       = "Nested modal boxes"
    gRows(DEMO_NESTED).wszNote        = "click the message area to raise a second loop on top"
    gRows(DEMO_ONELINER).wszLabel     = "PsMessageBox_Show one-liner"
    gRows(DEMO_ONELINER).wszNote      = "sets no fonts, so the icon is honestly a missing glyph"

    for i as long = 0 to DEMO_ROW_COUNT - 1
        ghRaise(i) = PsButton_Create( HWND_FRMMAIN, IDC_FRMMAIN_BTN_FIRST + i )
        PsButton_SetFont( ghRaise(i), ghFont(GUIFONT_10) )
        PsButton_SetGlyphFont( ghRaise(i), ghFont(SYMBOLFONT_10) )
        PsButton_SetText( ghRaise(i), "Show" )
        PsButton_SetClickCallback( ghRaise(i), @frmMain_OnRaiseClicked )
    next

    frmMain_PositionButtons( HWND_FRMMAIN )
    for i as long = 0 to DEMO_ROW_COUNT - 1
        ShowWindow( ghRaise(i), SW_SHOW )
    next

    ' Self-gated on PSMESSAGEBOX_SELFTEST=1. Runs before the loop and never goes modal.
    PsMessageBox_RunSelfTest( HWND_FRMMAIN )
    frmMain_ProbeCustomPainter( HWND_FRMMAIN )

    UpdateWindow( pWindow->hWindow )
    ShowWindow( pWindow->hWindow, SW_SHOW )

    ' Give the first raiser button the focus. A real dialog does this in WM_INITDIALOG; an
    ' ordinary window must do it itself, and without it IsDialogMessage ignores the FIRST Tab
    ' entirely -- it only acts when the focused window is a DESCENDANT of the window it is
    ' passed, and at startup the focus is on the form itself.
    SetFocus( ghRaise(0) )


    ' Process windows events
    dim uMsg as MSG

    ' Message loop
    '
    ' THERE IS NO PsMessageBox_FilterMessage. The box runs its OWN nested pump inside DoModal and
    ' calls IsDialogMessage itself, so unlike PsComboBox / PsNumericUpDown / PsTextBox this control
    ' adds NO pump obligation. The IsDialogMessage below is for the demo's own buttons.
    do while GetMessage(@uMsg, null, 0, 0)
        if uMsg.message = WM_QUIT then exit do

        if (pWindow->AccelHandle = 0) orelse (TranslateAccelerator(pWindow->hWindow, pWindow->AccelHandle, @uMsg) = 0) then
            if IsDialogMessage(pWindow->hWindow, @uMsg) = 0 then
                TranslateMessage @uMsg
                DispatchMessage @uMsg
            end if
        end if
    loop  ' message loop

    function = uMsg.wParam

    for i as long = 0 to MAXFONTS
        if ghFont(i) then DeleteObject(ghFont(i))
    next

end function
