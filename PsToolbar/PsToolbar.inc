'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsToolbar.inc
''

#include once "PsToolbar.bi"
' The tooltip backend switch, pulled in here so a host needs no extra include line.
#include once "PsTipHost.inc"
' Image loader implementation, pulled in here so a host needs no extra include line
' (#include once guards it if the host includes it too).
#include once "PsImage.inc"


' Forward declarations for the private helpers that are used above their definitions.
declare function PsToolbar_OpenMenu( byval pTB as PSTOOLBAR ptr, byval hToolbar as HWND, byval idx as long ) as boolean
declare sub      PsToolbar_ClearDropState( byval pTB as PSTOOLBAR ptr, byval hToolbar as HWND )
declare sub      PsToolbar_ActivateItem( byval pTB as PSTOOLBAR ptr, byval hToolbar as HWND, byval idx as long )


function GetToolbarPointer( byval hToolbar as HWND ) as PSTOOLBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hToolbar)
    dim as PSTOOLBAR ptr pTB = null
    if pWindow then pTB = cast( PSTOOLBAR ptr, pWindow->UserData(0) )
    return pTB
end function


' Run any pending layout. Returns FALSE when the control still has no client area, in which
' case every derived rect is empty and a caller must not trust them.
private function PsToolbar_EnsureLayout( byval pTB as PSTOOLBAR ptr ) as boolean
    if pTB = null then return false
    if pTB->bLayoutDirty then pTB->LayoutItems()
    return (pTB->bLayoutDirty = false)
end function


' Repaint one item. Silently does nothing for an invalid index, which is what makes the
' hover path safe when the cursor is over empty bar area (idx = -1). FALSE = no background
' erase; the item's whole cell is painted every time.
private sub PsToolbar_InvalidateItem( byval pTB as PSTOOLBAR ptr, byval idx as long )
    if pTB = null then exit sub
    if pTB->IsValidItem(idx) = false then exit sub
    if IsRectEmpty( @pTB->items(idx).rc ) then exit sub
    InvalidateRect( pTB->hWin, @pTB->items(idx).rc, FALSE )
end sub

private sub PsToolbar_InvalidateOverflow( byval pTB as PSTOOLBAR ptr )
    if pTB = null then exit sub
    if pTB->bNeedOverflow = false then exit sub
    if IsRectEmpty( @pTB->rcOverflow ) then exit sub
    InvalidateRect( pTB->hWin, @pTB->rcOverflow, FALSE )
end sub




' Per-item text wins; the callback is consulted only for items with none. There is NO fall
' back to the caption: an icon-only item has none, and a captioned item echoing its own
' caption as a tip is noise.
private function PsToolbar_ResolveTooltipText( byval pTB as PSTOOLBAR ptr, byval idx as long ) as DWSTRING
    if pTB = null then return ""
    if pTB->IsValidItem(idx) = false then return ""
    if len( pTB->items(idx).wszTooltip ) then return pTB->items(idx).wszTooltip
    if pTB->TooltipCallback then return pTB->TooltipCallback( pTB->hWin, idx )
    return ""
end function


' ========================================================================================
' Supply the tooltip text to the PsTooltip backend.
'
' This thunk is why PsTipHost never learns the text policy: PsToolbar_ResolveTooltipText
' stays the ONE place that decides what a tip says, and the system backend reaches the
' same function by the other road (comctl32 asking through TTN_GETDISPINFOW), which is
' why that arm stays in the WndProc.
' ========================================================================================
private function PsToolbar_TipTextCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                        byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut = PsToolbar_ResolveTooltipText( GetToolbarPointer(hAttached), idx )
    return wszOut
end function


' ========================================================================================
' Which item is the cursor over, for HOVER and CLICK purposes, and which ZONE of it?
' Returns a model index, or -1 for none (including the overflow button, which has no index
' but does set nZone).
'
' This is the type's HitTestZone plus two rules that belong to interaction rather than to
' geometry:
'   - the cursor must be inside the client area. Cells can legitimately run PAST the right
'     edge when the run overflows, and without this test a cursor over the sibling window
'     next door would light one of them up.
'   - a DISABLED item is never hot and never pressable.
' ========================================================================================
private function GetHotToolbarItem( byval hToolbar as HWND, byref nZone as long ) as long
    nZone = TBR_ZONE_NONE
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1

    dim as POINT pt
    GetCursorPos( @pt )
    ScreenToClient( hToolbar, @pt )

    dim as RECT rcClient
    GetClientRect( hToolbar, @rcClient )
    if PtInRect( @rcClient, pt ) = 0 then return -1

    dim as long idx = pTB->HitTestZone( pt.x, pt.y, nZone )
    if nZone = TBR_ZONE_OVERFLOW then return -1
    if idx = -1 then return -1
    if pTB->items(idx).isEnabled = false then
        nZone = TBR_ZONE_NONE
        return -1
    end if
    return idx
end function


' ========================================================================================
' Enforce a radio group after idxWinner has ALREADY been set selected.
'
' BOTH STATE WRITES LAND BEFORE EITHER CALLBACK, and the losers are notified FIRST
' (PsOptionButton's rule). A host reading the whole group from inside one handler therefore
' never sees a moment with two members checked -- which is the entire reason for separating
' the two loops below rather than notifying as we go.
' ========================================================================================
private sub PsToolbar_ApplyRadio( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval idxWinner as long, _
            byval bNotify as boolean _
            )
    if pTB = null then exit sub
    if pTB->IsValidItem(idxWinner) = false then exit sub
    dim as long nGroup = pTB->items(idxWinner).nGroupID
    if nGroup = 0 then exit sub

    ' Pass 1: every state write.
    dim losers( 0 to pTB->itemCount ) as long
    dim as long nLosers = 0
    for i as long = 0 to pTB->itemCount - 1
        if i = idxWinner then continue for
        with pTB->items(i)
            if .nGroupID <> nGroup then continue for
            if .isSelected = false then continue for
            .isSelected = false
            losers(nLosers) = i
            nLosers += 1
            PsToolbar_InvalidateItem( pTB, i )
        end with
    next

    ' Pass 2: every callback, losers first.
    if bNotify = false then exit sub
    if pTB->SelChangeCallback = 0 then exit sub
    for n as long = 0 to nLosers - 1
        pTB->SelChangeCallback( hToolbar, losers(n), false )
    next
    pTB->SelChangeCallback( hToolbar, idxWinner, true )
end sub


' Flip one toggle, honouring its group. bNotify = FALSE is the silent programmatic form.
private sub PsToolbar_ToggleInternal( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval idx as long, _
            byval bNotify as boolean _
            )
    if pTB = null then exit sub
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then exit sub

    if pItem->nGroupID <> 0 then
        ' Clicking the CHECKED member of a radio group is a silent no-op -- nothing-checked
        ' stays a legal state the user cannot reach, exactly as a radio button behaves.
        if pItem->isSelected then exit sub
        pItem->isSelected = true
        PsToolbar_InvalidateItem( pTB, idx )
        PsToolbar_ApplyRadio( pTB, hToolbar, idx, bNotify )
    else
        dim as boolean bNewState = not pItem->isSelected
        pItem->isSelected = bNewState
        PsToolbar_InvalidateItem( pTB, idx )
        if bNotify andalso (pTB->SelChangeCallback <> 0) then
            pTB->SelChangeCallback( hToolbar, idx, bNewState )
        end if
    end if
end sub


' ========================================================================================
' THE ONE COMMIT PATH (PsDatePicker's rule).
'
' A matched click on the strip, a pick from the OVERFLOW menu, and PsToolbar_Click all land
' here. Nothing about acting on an item is duplicated between them, so a host handler never
' has to care whether the item was on the strip or behind the chevron.
' ========================================================================================
private sub PsToolbar_ActivateItem( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval idx as long _
            )
    if pTB = null then exit sub
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then exit sub
    if pItem->isEnabled = false then exit sub

    select case pItem->itemKind
    case TBR_KIND_TOGGLE
        PsToolbar_ToggleInternal( pTB, hToolbar, idx, true )

    case TBR_KIND_DROPDOWN
        ' Opening the menu IS this item's action, wherever it was activated from. OpenMenu
        ' anchors itself against the strip cell or the overflow button as appropriate.
        PsToolbar_OpenMenu( pTB, hToolbar, idx )

    case TBR_KIND_COMMAND, TBR_KIND_SPLIT
        ' A SPLIT item's action zone latches when it carries a group id, and is momentary
        ' otherwise -- so a split toggle is expressible without a seventh item kind.
        if pItem->nGroupID <> 0 then
            PsToolbar_ToggleInternal( pTB, hToolbar, idx, true )
        else
            ' Snapshot the id before the callback: it may delete items, which would leave
            ' pItem dangling.
            dim as long nId = pItem->id
            if pTB->ClickCallback then pTB->ClickCallback( hToolbar, idx, nId )
        end if
    end select
end sub


' ========================================================================================
' Menus. Every one of them is created on demand, owned by the toolbar, and registered as
' sending its CPM_NOTIFY_* to the toolbar so open/close state can be tracked.
'
' A HOST fills an item's menu and handles its commands through PsPopupMenu's own
' SetSelectCallback -- do NOT call PsPopupMenu_SetNotifyWindow on a toolbar-owned menu, as
' that would displace the toolbar's own registration and leave its dropped state stuck on.
' ========================================================================================
' ========================================================================================
' Re-derive a menu's colors from the bar's, unless the host has claimed them.
'
' PSPOPUPMENU_COLORS carries no defaults except SeparatorColor, so a menu that is never given
' colors renders BLACK ON BLACK -- the separator rule being the only thing visible on it. This
' is what stops that being the out-of-box experience, and it is why theming the bar themes its
' menus: a host sets PSTOOLBAR_COLORS once.
'
' BorderColor is taken from ForeColorDisabled rather than SeparatorColor: the border has to
' delimit the popup against whatever is BEHIND it, which may be a light host window, and the
' mid grey reads against both. SeparatorColor is a chrome line on the bar itself and is often
' close to the bar background.
' ========================================================================================
private sub PsToolbar_ApplyDerivedMenuColors( byval pTB as PSTOOLBAR ptr, byval hMenu as HWND )
    if pTB = null then exit sub
    if hMenu = 0 then exit sub
    if IsWindow( hMenu ) = 0 then exit sub
    ' A host that claimed the colors gets them applied here too, so a menu created LATER
    ' still comes up right rather than falling back to the black default.
    if pTB->bMenuColorsCustom then
        PsPopupMenu_SetColors( hMenu, @pTB->menuColors )
        exit sub
    end if

    dim as PSPOPUPMENU_COLORS c
    c.BackColor         = pTB->colors.BackColor
    c.ForeColor         = pTB->colors.ForeColor
    c.BackColorHot      = pTB->colors.BackColorHot
    c.ForeColorHot      = pTB->colors.ForeColorHot
    c.ForeColorDisabled = pTB->colors.ForeColorDisabled
    c.BorderColor       = pTB->colors.ForeColorDisabled
    c.SeparatorColor    = pTB->colors.SeparatorColor
    PsPopupMenu_SetColors( hMenu, @c )
end sub


' Apply the derivation to every menu this toolbar owns. Called when the bar is re-themed.
private sub PsToolbar_ReapplyAllMenuColors( byval pTB as PSTOOLBAR ptr )
    if pTB = null then exit sub
    if pTB->hOverflowMenu <> 0 then PsToolbar_ApplyDerivedMenuColors( pTB, pTB->hOverflowMenu )
    for i as long = 0 to pTB->itemCount - 1
        if pTB->items(i).hMenu <> 0 then PsToolbar_ApplyDerivedMenuColors( pTB, pTB->items(i).hMenu )
    next
end sub


private function PsToolbar_EnsureItemMenu( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval idx as long _
            ) as HWND
    if pTB = null then return 0
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return 0
    if pItem->hMenu <> 0 then
        if IsWindow( pItem->hMenu ) then return pItem->hMenu
        pItem->hMenu = 0
    end if

    dim as HWND hM = PsPopupMenu_Create( hToolbar, pItem->id )
    if hM = 0 then return 0
    PsPopupMenu_SetNotifyWindow( hM, hToolbar )
    PsPopupMenu_SetFonts( hM, pTB->hTextFont, pTB->hGlyphFont )
    pItem->hMenu = hM
    PsToolbar_ApplyDerivedMenuColors( pTB, hM )
    return hM
end function


private function PsToolbar_EnsureOverflowMenu( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND _
            ) as HWND
    if pTB = null then return 0
    if pTB->hOverflowMenu <> 0 then
        if IsWindow( pTB->hOverflowMenu ) then return pTB->hOverflowMenu
        pTB->hOverflowMenu = 0
    end if

    dim as HWND hM = PsPopupMenu_Create( hToolbar, 0 )
    if hM = 0 then return 0
    PsPopupMenu_SetNotifyWindow( hM, hToolbar )
    PsPopupMenu_SetFonts( hM, pTB->hTextFont, pTB->hGlyphFont )
    pTB->hOverflowMenu = hM
    PsToolbar_ApplyDerivedMenuColors( pTB, hM )
    return hM
end function


' ========================================================================================
' Rebuild the overflow menu from the items that did not fit. Runs on EVERY open, which is
' why adding rows to this menu from a host is pointless (documented on GetOverflowMenu).
'
' Row ids are (model index + 1), so a host's own command ids never enter this menu's command
' stream and cannot collide with it (PsComboBox's rule).
'
' SEPARATOR TRIMMING: a separator is emitted only when a row has already been written AND
' another row follows it -- which is what "pending" encodes. So a leading separator is never
' written, a trailing one is simply never flushed, and a run of consecutive separators
' collapses to one. That is the same rule LayoutItems applies to the strip.
'
' A SPLIT / DROPDOWN item's own menu is deliberately NOT attached here as a submenu:
' PsPopupMenu_SetItemSubMenu TRANSFERS OWNERSHIP, and PsPopupMenu_Clear DESTROYS attached
' submenus -- so the next rebuild would destroy the host's menu out from under it. Picking
' such a row activates the item instead, which for a DROPDOWN means opening its menu against
' the overflow button.
' ========================================================================================
private sub PsToolbar_BuildOverflowMenu( byval pTB as PSTOOLBAR ptr, byval hToolbar as HWND )
    if pTB = null then exit sub
    dim as HWND hM = PsToolbar_EnsureOverflowMenu( pTB, hToolbar )
    if hM = 0 then exit sub

    PsPopupMenu_Clear( hM )

    dim as boolean bPendingSep = false
    dim as boolean bAnyRow     = false

    for i as long = 0 to pTB->itemCount - 1
        with pTB->items(i)
            if .isVisible = false then continue for
            if .bOverflowed = false then continue for
            if .bTrimmed then continue for

            if .itemKind = TBR_KIND_SEPARATOR then
                if bAnyRow then bPendingSep = true
                continue for
            end if
            ' A CONTROL cell is never overflowed, so it cannot reach here -- but if a future
            ' edit made one overflowable, a window in a menu would be a hard failure rather
            ' than a cosmetic one. Skip it explicitly.
            if .itemKind = TBR_KIND_CONTROL then continue for

            if bPendingSep then
                PsPopupMenu_AddSeparator( hM )
                bPendingSep = false
            end if

            ' Caption, then tooltip, then a generated label. An icon-only item with no
            ' tooltip has no human-readable name anywhere in the model, so "#n" is the
            ' honest answer -- give such an item a tooltip if it may ever overflow.
            dim as DWSTRING wszCap = .wszText
            if len(wszCap) = 0 then wszCap = .wszTooltip
            if len(wszCap) = 0 then wszCap = "#" + wstr(i + 1)

            PsPopupMenu_AddItem( hM, i + 1, wszCap, "", (.isEnabled = false), .isSelected )
            bAnyRow = true
        end with
    next
end sub


private sub PsToolbar_NotifyDropDown( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval idx as long, _
            byval hMenu as HWND, _
            byval bOpen as boolean _
            )
    if pTB = null then exit sub
    if pTB->DropDownCallback = 0 then exit sub
    pTB->DropDownCallback( hToolbar, idx, hMenu, bOpen )
end sub


' Forget whatever menu we believe is open and fire the closing edge for it. Safe to call
' when nothing is open.
private sub PsToolbar_ClearDropState( byval pTB as PSTOOLBAR ptr, byval hToolbar as HWND )
    if pTB = null then exit sub

    dim as long  idxWas       = pTB->nDroppedIdx
    dim as boolean bWasOverflow = pTB->bOverflowOpen
    if (idxWas = -1) andalso (bWasOverflow = false) then exit sub

    dim as HWND hWas = 0
    if bWasOverflow then
        hWas = pTB->hOverflowMenu
    elseif pTB->IsValidItem(idxWas) then
        hWas = pTB->items(idxWas).hMenu
    end if

    ' Clear the state BEFORE the callback, so a handler that queries IsDroppedDown sees the
    ' truth rather than the state we are in the middle of leaving.
    pTB->nDroppedIdx   = -1
    pTB->bOverflowOpen = false

    if bWasOverflow then
        PsToolbar_InvalidateOverflow( pTB )
        PsToolbar_NotifyDropDown( pTB, hToolbar, -1, hWas, false )
    else
        PsToolbar_InvalidateItem( pTB, idxWas )
        PsToolbar_NotifyDropDown( pTB, hToolbar, idxWas, hWas, false )
    end if
end sub


' ========================================================================================
' Open a menu. idx = -1 means the OVERFLOW menu.
'
' Order matters and each step is load-bearing:
'   1. close whatever chain is already open, and fire ITS closing edge. PsPopupMenu closes
'      any other root chain itself when a new root is shown, but does so SILENTLY -- so
'      without this our own dropped state would go stale. (This is the residual PsComboBox
'      documents and does not fix; here it is fixed.)
'   2. for an ITEM menu the opening edge fires BEFORE a single row is built, which is what
'      makes it the just-in-time hook for a dynamic menu;
'   3. for the OVERFLOW menu the control builds the rows itself, so its edge fires after;
'   4. the anchor is the cell (or the overflow button) in SCREEN coords. ShowForRect already
'      flips a PM_ALIGN_BELOW popup ABOVE its anchor when it will not fit below, so a tall
'      menu on a bottom-docked toolbar needs nothing extra here.
' ========================================================================================
private function PsToolbar_OpenMenu( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval idx as long _
            ) as boolean

    if pTB = null then return false
    if PsToolbar_EnsureLayout( pTB ) = false then return false

    dim as HWND hM = 0
    dim as RECT rcAnchor

    if idx = -1 then
        if pTB->bNeedOverflow = false then return false
        hM = PsToolbar_EnsureOverflowMenu( pTB, hToolbar )
        if hM = 0 then return false
        rcAnchor = pTB->rcOverflow
    else
        dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
        if pItem = null then return false
        if pItem->isEnabled = false then return false
        if (pItem->itemKind <> TBR_KIND_SPLIT) andalso (pItem->itemKind <> TBR_KIND_DROPDOWN) then return false
        hM = PsToolbar_EnsureItemMenu( pTB, hToolbar, idx )
        if hM = 0 then return false
        ' An overflowed item was activated from the overflow menu, so its own cell has no
        ' rect -- anchor against the button the user actually clicked.
        if pItem->bOverflowed then
            rcAnchor = pTB->rcOverflow
        else
            rcAnchor = pItem->rc
        end if
    end if
    if IsRectEmpty( @rcAnchor ) then return false

    ' 1. anything already open loses, and says so.
    PsToolbar_ClearDropState( pTB, hToolbar )

    ' 2 / 3. the opening edge, and the rows.
    if idx = -1 then
        PsToolbar_BuildOverflowMenu( pTB, hToolbar )
        PsToolbar_NotifyDropDown( pTB, hToolbar, -1, hM, true )
    else
        PsToolbar_NotifyDropDown( pTB, hToolbar, idx, hM, true )
    end if

    if PsPopupMenu_GetItemCount( hM ) = 0 then
        ' Nothing to show. Report the transition back so the host's open/close bookkeeping
        ' stays balanced rather than being left half-open.
        PsToolbar_NotifyDropDown( pTB, hToolbar, idx, hM, false )
        return false
    end if

    ' 4. anchor in screen coords.
    dim as POINT ptTL, ptBR
    ptTL.x = rcAnchor.left  : ptTL.y = rcAnchor.top
    ptBR.x = rcAnchor.right : ptBR.y = rcAnchor.bottom
    ClientToScreen( hToolbar, @ptTL )
    ClientToScreen( hToolbar, @ptBR )
    SetRect( @rcAnchor, ptTL.x, ptTL.y, ptBR.x, ptBR.y )

    if PsPopupMenu_ShowForRect( hM, rcAnchor, PM_ALIGN_BELOW ) = false then
        PsToolbar_NotifyDropDown( pTB, hToolbar, idx, hM, false )
        return false
    end if

    if idx = -1 then
        pTB->bOverflowOpen = true
        PsToolbar_InvalidateOverflow( pTB )
    else
        pTB->nDroppedIdx = idx
        PsToolbar_InvalidateItem( pTB, idx )
    end if
    return true
end function


' ========================================================================================
' Resolve the fonts the PAINTER uses.
'
' These exist because LayoutItems falls back to the stock GUI font when no text font has
' been set, and for a while the painter did NOT -- it handed SetFont(0) straight through, so
' a control with no font MEASURED with one font and PAINTED with whatever the DC happened to
' hold. Every caption was then laid out against the wrong metrics. That is precisely the
' measure-and-paint-must-agree rule the paint callback contract warns hosts about, and it was
' broken inside the control itself; the render probe is what caught it.
'
' The glyph font falls back to the TEXT font before the stock one, because a host that sets
' only one font has told us which font it means.
' ========================================================================================
private function PsToolbar_ResolveTextFont( byval pTB as PSTOOLBAR ptr ) as HFONT
    if pTB = null then return cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    if pTB->hTextFont then return pTB->hTextFont
    return cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
end function

private function PsToolbar_ResolveGlyphFont( byval pTB as PSTOOLBAR ptr ) as HFONT
    if pTB = null then return cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    if pTB->hGlyphFont then return pTB->hGlyphFont
    return PsToolbar_ResolveTextFont( pTB )
end function


' ========================================================================================
' The built-in painter: draws one item when no PaintItemCallback is set. idx = -1 draws the
' overflow button.
'
' State precedence is PsToolbar_ResolveMood's, decided once there so every renderer agrees:
'   disabled > pressed > hot > idle,  with isSelected combined on top.
' A disabled item is never hot or pressed (GetHotToolbarItem refuses it), so the first rung
' is unreachable from the others rather than merely winning over them.
'
' The state fill is a ROUNDED rect drawn OVER a flat background fill, not a replacement for
' it -- so the bar background always shows in the gaps between cells and an item reads as a
' pill rather than as a full-height block.
'
' NOTE the frame discipline: nothing here uses PaintBorderRect. It FILLS before it strokes,
' and using it as a frame has erased a control four separate times in this family.
' ========================================================================================
private sub PsToolbar_PaintItemDefault( _
            byval pTB as PSTOOLBAR ptr, _
            byval p as PSTOOLBAR_PAINTINFO ptr _
            )
    if (pTB = null) orelse (p = null) then exit sub
    dim as PsBufferPaint ptr b = p->b
    if b = null then exit sub

    ' The bar background under this cell, always, so the rounded fill below has something
    ' correct to sit on and the inter-cell gaps read as bar rather than as item.
    b->SetBackColor( pTB->colors.BackColor )
    b->PaintRect( @p->rc )

    if p->itemKind = TBR_KIND_SEPARATOR then
        b->SetBackColor( pTB->colors.SeparatorColor )
        b->PaintRect( @p->rcIcon )       ' the rule
        exit sub
    end if
    ' A CONTROL cell is a hole: the host's child window covers it and painting anything more
    ' than the background here would flash underneath it.
    if p->itemKind = TBR_KIND_CONTROL then exit sub

    dim as COLORREF backclr = pTB->colors.BackColor
    dim as COLORREF foreclr = pTB->colors.ForeColor
    dim as boolean  bFillState = false

    select case p->nMood
    case TBR_MOOD_DISABLED
        foreclr = pTB->colors.ForeColorDisabled
        if p->isSelected then
            backclr    = pTB->colors.BackColorSelect
            bFillState = true
        end if
    case TBR_MOOD_PRESSED
        ' Pressed reuses the SELECT pair rather than adding a tenth colour: on a COMMAND item
        ' it is the momentary flash that makes the icon feel like a button, and on a TOGGLE it
        ' previews the latched look the release is about to produce.
        backclr    = pTB->colors.BackColorSelect
        foreclr    = pTB->colors.ForeColorSelect
        bFillState = true
    case TBR_MOOD_HOT
        backclr    = pTB->colors.BackColorHot
        foreclr    = pTB->colors.ForeColorHot
        bFillState = true
    case else
        if p->isSelected then
            backclr    = pTB->colors.BackColorSelect
            foreclr    = pTB->colors.ForeColorSelect
            bFillState = true
        elseif p->isDropped then
            backclr    = pTB->colors.BackColorHot
            foreclr    = pTB->colors.ForeColorHot
            bFillState = true
        elseif p->wszGlyph <> "" then
            ' A per-item idle colour speaks only for the idle state, so a hover always
            ' speaks the bar's language.
            if pTB->IsValidItem( p->itemID ) then
                if pTB->items( p->itemID ).bHasForeColor then foreclr = pTB->items( p->itemID ).ForeColorItem
            end if
        end if
    end select

    if bFillState then
        ' On a SPLIT item a hover lights only the half the pointer is actually over, so the
        ' two zones read as the two targets they are.
        dim as RECT rcFill = p->rc
        if (p->itemKind = TBR_KIND_SPLIT) andalso (IsRectEmpty( @p->rcChevron ) = 0) then
            dim as boolean bHalf = (p->nMood = TBR_MOOD_HOT) orelse (p->nMood = TBR_MOOD_PRESSED)
            if bHalf then
                if p->isChevronHot then
                    rcFill.left = p->rcChevron.left
                else
                    rcFill.right = p->rcChevron.left
                end if
            end if
        end if
        b->SetBackColor( backclr )
        if pTB->nCurvature > 0 then
            b->PaintRoundRect( @rcFill, pTB->nCurvature )
        else
            b->PaintRect( @rcFill )
        end if
    end if

    b->SetColors( foreclr, backclr )

    ' Image first, else glyph. Both draw into the same declared icon cell (rcIcon).
    if IsRectEmpty( @p->rcIcon ) = 0 then
        if p->pImage then
            b->PaintImage( p->pImage, @p->rcIcon, PS_IMGFIT_ASPECT )
        elseif len( p->wszGlyph ) then
            b->SetFont( PsToolbar_ResolveGlyphFont( pTB ) )
            ' PaintText already adds DT_VCENTER / DT_SINGLELINE / DT_NOPREFIX.
            b->PaintText( p->wszGlyph, @p->rcIcon, DT_CENTER )
        end if
    end if

    if len( p->wszText ) then
        if IsRectEmpty( @p->rcText ) = 0 then
            b->SetFont( PsToolbar_ResolveTextFont( pTB ) )
            b->PaintText( p->wszText, @p->rcText, DT_LEFT or DT_END_ELLIPSIS )
        end if
    end if

    if IsRectEmpty( @p->rcChevron ) = 0 then
        ' The hairline that makes a SPLIT read as two targets. Drawn before the glyph so a
        ' chevron overhanging it still wins.
        if p->itemKind = TBR_KIND_SPLIT then
            if pTB->nDividerThick > 0 then
                b->SetPenColor( pTB->colors.SplitDividerColor )
                dim as long xd = p->rcChevron.left - pTB->nDividerThick
                b->PaintLine( pTB->nDividerThick, xd, p->rcChevron.top, xd, p->rcChevron.bottom )
            end if
        end if
        b->SetColors( foreclr, backclr )
        b->SetFont( PsToolbar_ResolveGlyphFont( pTB ) )
        dim as DWSTRING wszC = pTB->wszChevron
        if p->itemID = -1 then wszC = pTB->wszOverflow
        if len(wszC) then b->PaintText( wszC, @p->rcChevron, DT_CENTER )
    end if
end sub


' Populate the PAINTINFO for one item. idx = -1 builds the OVERFLOW button's, which is
' presented to a host callback as a captionless DROPDOWN so one renderer can serve both.
private sub PsToolbar_FillPaintInfo( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval b as PsBufferPaint ptr, _
            byval idx as long, _
            byval p as PSTOOLBAR_PAINTINFO ptr _
            )
    if (pTB = null) orelse (p = null) then exit sub

    p->hToolbar = hToolbar
    p->itemID   = idx
    p->b        = b

    if idx = -1 then
        p->itemKind     = TBR_KIND_DROPDOWN
        p->rc           = pTB->rcOverflow
        SetRectEmpty( @p->rcIcon )
        SetRectEmpty( @p->rcText )
        ' The overflow glyph box is the cell minus its padding, vertically centred on the
        ' icon height -- the same band every other chevron occupies.
        dim as long cellH = p->rc.bottom - p->rc.top
        dim as long hG    = pTB->nIconHeight
        if hG > cellH then hG = cellH
        dim as long yG    = p->rc.top + ((cellH - hG) \ 2)
        SetRect( @p->rcChevron, p->rc.left + pTB->nPadBefore, yG, _
                                p->rc.left + pTB->nPadBefore + pTB->nChevronWidth, yG + hG )
        p->isHot        = pTB->bOverflowHot
        p->isSelected   = false
        p->isPressed    = false
        p->isEnabled    = true
        p->isChevronHot = false
        p->isDropped    = pTB->bOverflowOpen
        p->nMood        = PsToolbar_ResolveMood( true, false, pTB->bOverflowHot )
        p->wszGlyph     = ""
        p->wszText      = ""
        p->pImage       = 0             ' the overflow button is always a chevron glyph
        exit sub
    end if

    with pTB->items(idx)
        p->itemKind   = .itemKind
        p->rc         = .rc
        p->rcIcon     = .rcIcon
        p->rcText     = .rcText
        p->rcChevron  = .rcChevron
        p->isHot      = (idx = pTB->nLastHotIdx)
        ' "The press is still on this item" is exactly "the pressed item is also the hot
        ' one" -- hover tracking maintains that through the capture, so no second flag.
        p->isPressed  = (idx = pTB->nPressedIdx) andalso (pTB->nLastHotIdx = pTB->nPressedIdx)
        p->isSelected = .isSelected
        p->isEnabled  = .isEnabled
        p->isDropped  = (idx = pTB->nDroppedIdx)
        p->isChevronHot = false
        if .itemKind = TBR_KIND_SPLIT then
            if p->isPressed then
                p->isChevronHot = (pTB->nPressedZone = TBR_ZONE_CHEVRON)
            elseif p->isHot then
                p->isChevronHot = (pTB->nHotZone = TBR_ZONE_CHEVRON)
            end if
        end if
        p->nMood      = PsToolbar_ResolveMood( .isEnabled, p->isPressed, p->isHot )
        p->wszGlyph   = .wszGlyph
        p->wszText    = .wszText
        p->pImage     = iif( (.pImage <> 0) andalso (.pImage->IsValid() <> 0), .pImage->Image(), cast( CGpImage ptr, 0 ) )
    end with
end sub


' ========================================================================================
' Render the WHOLE control into a supplied buffer.
'
' SPLIT OUT FROM WM_PAINT ON PURPOSE (PsDatePicker's lesson): a probe that builds its own
' buffer can drive the exact path WM_PAINT uses, instead of driving one item through a host
' callback and reporting healthy numbers while the real painter is producing a flat
' rectangle. That is not hypothetical -- PsDatePicker shipped a probe that passed at 59
' tones while its popup was a solid block of colour.
' ========================================================================================
private sub PsToolbar_RenderAll( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval b as PsBufferPaint ptr, _
            byval rcUpdate as RECT _
            )
    if (pTB = null) orelse (b = null) then exit sub

    b->SetBackColor( pTB->colors.BackColor )
    b->PaintClientRect()

    dim p as PSTOOLBAR_PAINTINFO

    for i as long = 0 to pTB->itemCount - 1
        with pTB->items(i)
            if .isVisible = false then continue for
            if .bOverflowed orelse .bTrimmed then continue for
            if IsRectEmpty( @.rc ) then continue for
        end with
        dim as RECT rcHit
        if IntersectRect( @rcHit, @pTB->items(i).rc, @rcUpdate ) = 0 then continue for

        PsToolbar_FillPaintInfo( pTB, hToolbar, b, i, @p )
        if pTB->PaintItemCallback then
            pTB->PaintItemCallback( @p )
        else
            PsToolbar_PaintItemDefault( pTB, @p )
        end if
    next

    if pTB->bNeedOverflow then
        dim as RECT rcHit
        if IntersectRect( @rcHit, @pTB->rcOverflow, @rcUpdate ) then
            PsToolbar_FillPaintInfo( pTB, hToolbar, b, -1, @p )
            if pTB->PaintItemCallback then
                pTB->PaintItemCallback( @p )
            else
                PsToolbar_PaintItemDefault( pTB, @p )
            end if
        end if
    end if
end sub


function PsToolbar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hwnd)
    if pTB = null then return 0

    ' Consume any pending layout before painting. This is what makes a burst of Add calls
    ' cost one layout pass instead of N.
    if pTB->bLayoutDirty then pTB->LayoutItems()

    ' Capture the update rect BEFORE BeginDoubleBuffer -- BeginPaint clears it. This is a
    ' CONSERVATIVE approximation and correctness never depends on it: GetUpdateRect returns
    ' the BOUNDING BOX of the update region, so it only ever skips items that could not show
    ' a pixel anyway.
    dim as RECT rcUpdate
    if GetUpdateRect( hwnd, @rcUpdate, FALSE ) = 0 then GetClientRect( hwnd, @rcUpdate )

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    PsToolbar_RenderAll( pTB, hwnd, @b, rcUpdate )
    b.EndDoubleBuffer()

    function = 0
end function


function PsToolbar_OnSize( _
            byval hwnd  as HWND, _
            byval state as UINT, _
            byval cx    as long, _
            byval cy    as long _
            ) as LRESULT
    if state <> SIZE_MINIMIZED then
        ' Unlike PsIconPanel -- where a resize moves x0 and nothing else -- a resize here can
        ' change WHICH ITEMS ARE ON THE BAR AT ALL, and moves every CONTROL child with them.
        ' That is the whole of the difference, and it is why the layout is not a one-liner.
        dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hwnd)
        if pTB then pTB->Refresh()
    end if
    function = 0
end function


' Clear any hot-tracked item and stop the safety-net timer. Repaints only if something was
' actually hot, so this is cheap to call repeatedly.
'
' Note this also changes how a live press LOOKS (isPressed is derived from the hot index),
' which is exactly right: the cursor left the item, so the press must stop looking armed.
' The gesture itself survives -- only a release or a capture loss ends it.
private sub PsToolbar_ClearHotTracking( byval pTB as PSTOOLBAR ptr, byval hToolbar as HWND )
    if pTB = null then exit sub
    if pTB->hotTimerOn then
        KillTimer( hToolbar, IDT_PSTOOLBAR_HOTTRACK )
        pTB->hotTimerOn = false
    end if
    dim as boolean bAny = (pTB->nLastHotIdx <> -1) orelse pTB->bOverflowHot
    if bAny then
        pTB->nLastHotIdx  = -1
        pTB->nHotZone     = TBR_ZONE_NONE
        pTB->bOverflowHot = false
        AfxRedrawWindow( hToolbar )
    end if
end sub


' ========================================================================================
' Which child window sent this message, and which CONTROL cell is it in?
'
' WM_COMMAND carries the child in lParam (0 means a menu or accelerator, not a control);
' WM_NOTIFY carries it in the NMHDR; the WM_CTLCOLOR* family carries it in lParam too.
' ========================================================================================
private function PsToolbar_NotifySender( _
            byval uMsg as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as HWND
    select case uMsg
    case WM_COMMAND
        return cast( HWND, lParam )
    case WM_NOTIFY
        dim as NMHDR ptr pnmh = cast( NMHDR ptr, lParam )
        if pnmh = 0 then return 0
        return pnmh->hwndFrom
    case WM_CTLCOLORSTATIC, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, _
         WM_CTLCOLORLISTBOX, WM_CTLCOLORSCROLLBAR, WM_CTLCOLORDLG
        return cast( HWND, lParam )
    end select
    return 0
end function


' ========================================================================================
' Hand a child's notification to the host, then -- if unclaimed -- to the toolbar's own
' parent.
'
' THIS EXISTS BECAUSE PARENTING THE CHILD TO THE TOOLBAR INTERCEPTS ITS NOTIFICATIONS. A
' TBR_KIND_CONTROL child must be parented here so the toolbar can place it in its own client
' coordinates -- and the price is that every message the child sends to "its parent" lands in
' this window procedure instead of the host's form procedure, where it would otherwise have
' gone. Forwarding is what makes an existing form keep working when a control moves onto a
' toolbar (PsScrollPanel's rule, and the same reason it needed an lResult).
'
' Returns TRUE when the message was answered, with the value to return in lResult.
' ========================================================================================
private function PsToolbar_DispatchControlNotify( _
            byval pTB as PSTOOLBAR ptr, _
            byval hToolbar as HWND, _
            byval uMsg as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM, _
            byref lResult as LRESULT _
            ) as boolean

    lResult = 0
    if pTB = null then return false

    dim as HWND hChild = PsToolbar_NotifySender( uMsg, wParam, lParam )

    if pTB->ControlNotifyCallback then
        dim m as PSTOOLBAR_CONTROLNOTIFY
        m.hToolbar = hToolbar
        m.hChild   = hChild
        m.idx      = -1
        if hChild <> 0 then
            for i as long = 0 to pTB->itemCount - 1
                if pTB->items(i).hChild = hChild then
                    m.idx = i
                    exit for
                end if
            next
        end if
        m.uMsg    = uMsg
        m.wParam  = wParam
        m.lParam  = lParam
        m.lResult = 0
        if pTB->ControlNotifyCallback( @m ) then
            lResult = m.lResult
            return true
        end if
    end if

    ' Not claimed: send it where it would have gone if the toolbar were not in the way.
    dim as HWND hOwner = GetParent( hToolbar )
    if hOwner then
        lResult = SendMessage( hOwner, uMsg, wParam, lParam )
        return true
    end if
    return false
end function


' ========================================================================================
' PsToolbar Window procedure
'
' CAPTURE IS DECIDED PER ZONE HERE, WHICH IS NEW TO THIS FAMILY. The family's one-line test
' -- "does something consume the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing?" -- comes
' out differently for two zones of the SAME control:
'
'   ACTION zone of COMMAND / TOGGLE / SPLIT
'       Yes: the press/cancel gesture (press, slide off, release, nothing happens). Capture
'       is taken and the full price paid, in the order the family learned to pay it --
'       WM_LBUTTONUP releases FIRST and unconditionally before any callback runs; the message
'       callback's result is IGNORED for WM_LBUTTONUP so it cannot strand it;
'       WM_CAPTURECHANGED cancels the press; WM_DESTROY releases it.
'
'   CHEVRON zone of SPLIT, all of DROPDOWN, and the OVERFLOW button
'       No: the menu opens on the button-DOWN, so from that instant the popup owns the
'       interaction and there is no gesture left to protect. NO capture is taken --
'       PsComboBox's negative, reached independently.
'
' Do not "correct" either half toward the other. A capture note copied into a new control is
' an assumption to re-test, not a law to inherit.
'
' There is deliberately NO CS_DBLCLKS on this window class: with double-click messages
' enabled the second of two rapid clicks arrives as WM_LBUTTONDBLCLK instead of
' WM_LBUTTONDOWN, so every other click on a toggle would be swallowed.
' ========================================================================================
function PsToolbar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSTOOLBAR ptr pTB = GetToolbarPointer( hwnd )

    ' Per-message scratch info passed to the user MessageCallback. A plain local (NOT static)
    ' so that multiple control instances never share it.
    dim m as PSTOOLBAR_MESSAGEINFO

    select case uMsg
        ' Prepare the callback MessageInfo for only the messages that we hand to the host.
        case WM_MOUSEMOVE, WM_MOUSEHOVER, WM_MOUSELEAVE, _
             WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP
            with m
                .hToolbar = hwnd
                .uMsg     = uMsg
                .wParam   = wParam
                .lParam   = lParam
                .idx      = -1
                .nZone    = TBR_ZONE_NONE
            end with
    end select

    select case uMsg
        HANDLE_MSG (hwnd, WM_SIZE,  PsToolbar_OnSize)
        HANDLE_MSG (hwnd, WM_PAINT, PsToolbar_OnPaint)

    case WM_TIMER
        if (pTB <> 0) andalso (wParam = IDT_PSTOOLBAR_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no longer
            ' over our client area, clear the hot item ourselves.
            dim as RECT rcClient : GetClientRect( hwnd, @rcClient )
            if isMouseOverRECT( hwnd, rcClient ) = false then
                PsToolbar_ClearHotTracking( pTB, hwnd )
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        ' Track that we are over the control in order to catch the eventual WM_MOUSEHOVER
        ' and WM_MOUSELEAVE events.
        dim tme as TrackMouseEvent
        tme.cbSize = sizeof(TrackMouseEvent)
        tme.dwFlags = TME_HOVER or TME_LEAVE
        tme.hwndTrack = hwnd
        tme.dwHoverTime = 250    ' HOVER_DEFAULT is 500
        if pTB then tme.dwHoverTime = pTB->HoverTime
        TrackMouseEvent(@tme)

        ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits quickly
        ' (or onto another control), which would strand the hot item painted.
        if (pTB <> 0) andalso (pTB->hotTimerOn = false) then
            SetTimer( hwnd, IDT_PSTOOLBAR_HOTTRACK, PSTOOLBAR_HOTTRACK_MS, NULL )
            pTB->hotTimerOn = true
        end if

        if pTB then
            dim as long nZone
            dim as long idx = GetHotToolbarItem( hwnd, nZone )
            dim as boolean bOverHot = (nZone = TBR_ZONE_OVERFLOW)

            ' Repaint only what changed. The ZONE is part of that: sliding across a SPLIT
            ' button's divider changes which half lights up without changing the item.
            ' Each term is hoisted to a boolean local first. A relational operator on two
            ' longs yields an INTEGER in FreeBASIC while one on two booleans yields a
            ' BOOLEAN, and mixing those in an orelse chain is warning 38 (Learnings.md).
            dim as boolean bItemChanged = (idx <> pTB->nLastHotIdx)
            dim as boolean bZoneChanged = (nZone <> pTB->nHotZone)
            dim as boolean bOvrChanged  = (bOverHot <> pTB->bOverflowHot)
            dim as boolean bChanged = bItemChanged orelse bZoneChanged orelse bOvrChanged
            if bChanged then
                PsToolbar_InvalidateItem( pTB, idx )                 ' newly hot
                PsToolbar_InvalidateItem( pTB, pTB->nLastHotIdx )    ' previously hot
                PsToolbar_InvalidateOverflow( pTB )
                pTB->nLastHotIdx  = idx
                pTB->nHotZone     = nZone
                pTB->bOverflowHot = bOverHot
                ' Item changed: pop the current tip so it re-queries TTN_GETDISPINFOW for the
                ' new item on the next hover.
                if bItemChanged then
                    ' Item changed. On the system backend this pops the tip so it re-queries
                    ' TTN_GETDISPINFOW for the new item; on PsTooltip it moves the tool index, which
                    ' hides a visible tip and re-resolves on the next dwell. One call site either way --
                    ' and the index is what lets the PsTooltip backend report WHICH item a tip is for,
                    ' which comctl32 never knew.
                    PsTipHost_SetToolIndex( @pTB->tip, idx )
                end if
            end if
            m.idx   = idx
            m.nZone = nZone
            if pTB->MessageCallback then
                if pTB->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSEHOVER
        if pTB then
            m.idx = GetHotToolbarItem( hwnd, m.nZone )
            if pTB->MessageCallback then
                if pTB->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_MOUSELEAVE
        if pTB then
            ' Fast path for clearing the hot item (the WM_TIMER poll is the fallback for when
            ' this message never arrives). m.idx stays -1: the mouse is gone.
            PsToolbar_ClearHotTracking( pTB, hwnd )
            if pTB->MessageCallback then
                if pTB->MessageCallback( @m ) then return 0
            end if
        end if

    case WM_LBUTTONDOWN
        if pTB then
            ' Consume the one-shot reopen guard NOW, whatever else happens below: it is armed
            ' by the pump filter for exactly this click, and leaving it set would swallow a
            ' later, unrelated one.
            dim as boolean bSuppressOpen = pTB->bSuppressNextOpen
            pTB->bSuppressNextOpen = false

            dim as long nZone
            dim as long idx = GetHotToolbarItem( hwnd, nZone )
            m.idx   = idx
            m.nZone = nZone
            if pTB->MessageCallback then
                ' Suppressing the press is allowed: no capture has been taken yet, so there is
                ' no bookkeeping to strand.
                if pTB->MessageCallback( @m ) then return 0
            end if

            ' No SetFocus, ever: a toolbar must not steal focus from the host's editor.

            if nZone = TBR_ZONE_OVERFLOW then
                if bSuppressOpen = false then PsToolbar_OpenMenu( pTB, hwnd, -1 )
                return 0
            end if

            if idx <> -1 then
                dim as long nKind = pTB->items(idx).itemKind
                dim as boolean bOpensMenu = (nKind = TBR_KIND_DROPDOWN) orelse _
                                            ((nKind = TBR_KIND_SPLIT) andalso (nZone = TBR_ZONE_CHEVRON))
                if bOpensMenu then
                    ' NO CAPTURE for this zone -- the popup owns the interaction from here.
                    if bSuppressOpen = false then PsToolbar_OpenMenu( pTB, hwnd, idx )
                else
                    SetCapture( hwnd )
                    pTB->nPressedIdx  = idx
                    pTB->nPressedZone = nZone
                    PsToolbar_InvalidateItem( pTB, idx )
                end if
            end if
            return 0
        end if

    case WM_LBUTTONUP
        if pTB then
            ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the window
            ' losing capture SYNCHRONOUSLY, and our handler for it cancels the press -- so
            ' reading nPressedIdx after the release would always find -1 and no click would
            ' ever fire.
            dim as long idxPressed  = pTB->nPressedIdx
            dim as long nZonePressed = pTB->nPressedZone
            ' Release capture unconditionally, before any callback runs. Nothing below -- not
            ' a callback, not a host handler -- gets the chance to leave it held.
            if GetCapture() = hwnd then ReleaseCapture()

            dim as long nZone
            dim as long idx = GetHotToolbarItem( hwnd, nZone )
            pTB->CancelPress()
            PsToolbar_InvalidateItem( pTB, idxPressed )

            m.idx   = idx
            m.nZone = nZone
            ' The result is deliberately IGNORED here (see TBR_MessageCallbackFunc).
            if pTB->MessageCallback then pTB->MessageCallback( @m )

            ' Only a matched press+release on the SAME item AND the same zone acts. Press,
            ' slide off, release = nothing happened, which is the whole reason for the
            ' capture; and sliding from a split button's action half onto its chevron half is
            ' the same kind of miss.
            if (idxPressed <> -1) andalso (idx = idxPressed) andalso (nZone = nZonePressed) then
                PsToolbar_ActivateItem( pTB, hwnd, idxPressed )
            end if
            return 0
        end if

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell).
        ' The gesture is over; forget it and repaint the item that was armed.
        if pTB then
            dim as long idxPressed = pTB->nPressedIdx
            pTB->CancelPress()
            PsToolbar_InvalidateItem( pTB, idxPressed )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (PsPopupMenu). No
        ' capture is taken for the right button, so an up can arrive without a down.
        if pTB then
            m.idx = GetHotToolbarItem( hwnd, m.nZone )
            if pTB->MessageCallback then
                if pTB->MessageCallback( @m ) then return 0
            end if
        end if

    case CPM_NOTIFY_COMMAND
        ' A row of one of our menus was executed. The chain is ALREADY closed by the time
        ' this arrives, so opening another menu from here is safe.
        if pTB then
            dim as HWND    hRoot        = cast( HWND, lParam )
            dim as boolean bWasOverflow = pTB->bOverflowOpen
            dim as long    nId          = cast( long, wParam )

            PsToolbar_ClearDropState( pTB, hwnd )

            ' Only the OVERFLOW menu's commands are ours. An ITEM menu was filled by the host
            ' with the host's own ids, and the host handles it through that menu's
            ' PsPopupMenu_SetSelectCallback.
            if bWasOverflow andalso (hRoot = pTB->hOverflowMenu) then
                PsToolbar_ActivateItem( pTB, hwnd, nId - 1 )
            end if
            return 0
        end if

    case CPM_NOTIFY_CLOSED
        if pTB then
            PsToolbar_ClearDropState( pTB, hwnd )
            return 0
        end if

    case WM_COMMAND, WM_HSCROLL, WM_VSCROLL, _
         WM_CTLCOLORSTATIC, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, _
         WM_CTLCOLORLISTBOX, WM_CTLCOLORSCROLLBAR, WM_CTLCOLORDLG
        ' A TBR_KIND_CONTROL child talking to its parent. The toolbar deliberately supplies
        ' no default colours here: it positions a host's child and never paints or themes it,
        ' so an unclaimed WM_CTLCOLOR* falls through to the system exactly as it would have.
        if pTB then
            dim as LRESULT lRes
            if PsToolbar_DispatchControlNotify( pTB, hwnd, uMsg, wParam, lParam, lRes ) then
                return lRes
            end if
        end if

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply the current hot item's.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pTB then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' store in an instance-lifetime buffer and point lpszText at it
                pTB->wszTooltip = PsToolbar_ResolveTooltipText( pTB, pTB->nLastHotIdx )
                pdi->lpszText = cast( LPWSTR, pTB->wszTooltip.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if
        ' Not the tooltip: a CONTROL cell's child notifying its parent.
        if pTB then
            dim as LRESULT lRes
            if PsToolbar_DispatchControlNotify( pTB, hwnd, uMsg, wParam, lParam, lRes ) then
                return lRes
            end if
        end if

    case WM_DESTROY
        if pTB then
            ' Don't let the hot-tracking timer outlive the window...
            if pTB->hotTimerOn then
                KillTimer( hwnd, IDT_PSTOOLBAR_HOTTRACK )
                pTB->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pTB->CancelPress()
            ' Every menu is ours; a host's CONTROL child is not, and is left alone. Any
            ' per-item image is ours too (raw pointer, not auto-freed by delete pTB).
            for i as long = 0 to pTB->itemCount - 1
                if pTB->items(i).hMenu <> 0 then
                    if IsWindow( pTB->items(i).hMenu ) then DestroyWindow( pTB->items(i).hMenu )
                    pTB->items(i).hMenu = 0
                end if
                if pTB->items(i).pImage <> 0 then delete pTB->items(i).pImage : pTB->items(i).pImage = 0
            next
            if pTB->hOverflowMenu <> 0 then
                if IsWindow( pTB->hOverflowMenu ) then DestroyWindow( pTB->hOverflowMenu )
                pTB->hOverflowMenu = 0
            end if
        end if

    case WM_NCDESTROY
        ' Delete the stored toolbar data pointer
        if pTB then
            PsTipHost_Destroy( @pTB->tip )
            delete(pTB)
        end if

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Creation.
' ========================================================================================
function PsToolbar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' dwExStyle is passed EXPLICITLY as WS_EX_CONTROLPARENT, and that is the OPPOSITE of the
    ' PsButton fix. This control genuinely IS a container -- a TBR_KIND_CONTROL cell holds a
    ' host child that must be reachable by Tab -- and it is safe here for a reason PsButton
    ' could not use: this control takes no WS_TABSTOP of its own, so it can never be the
    ' childless leaf the dialog manager recurses into and then skips. See Learnings.md.
    '
    ' MEASURED, not assumed: the resulting exstyle is &h10000 (CONTROLPARENT alone) whether
    ' this argument is passed or left to CWindow's WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE
    ' default, because Windows strips WS_EX_WINDOWEDGE from a frameless WS_CHILD itself. So
    ' passing it here is DEFENSIVE rather than load-bearing -- it pins the intent against a
    ' future CWindow default change. What IS load-bearing is that it is not passed as 0:
    ' doing that (the PsButton fix, wrong here) drops CONTROLPARENT and makes the embedded
    ' child unreachable by Tab, and the self-test fails when it is.
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsToolbar_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            WS_EX_CONTROLPARENT _
            )
    ' NO CS_DBLCLKS here -- see the WndProc header.

    dim as PSTOOLBAR ptr pTB = new PSTOOLBAR
    pTB->hWin        = hCtrl
    pTB->idc_Toolbar = CtrlID

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule).
    pTB->nIconWidth    = pWindow->ScaleX( PSTOOLBAR_DEFAULT_ICONWIDTH )
    pTB->nIconHeight   = pWindow->ScaleY( PSTOOLBAR_DEFAULT_ICONHEIGHT )
    pTB->nPadBefore    = pWindow->ScaleX( PSTOOLBAR_DEFAULT_PADBEFORE )
    pTB->nPadAfter     = pWindow->ScaleX( PSTOOLBAR_DEFAULT_PADAFTER )
    pTB->nPadTop       = pWindow->ScaleY( PSTOOLBAR_DEFAULT_PADTOP )
    pTB->nPadBottom    = pWindow->ScaleY( PSTOOLBAR_DEFAULT_PADBOTTOM )
    pTB->nTextGap      = pWindow->ScaleX( PSTOOLBAR_DEFAULT_TEXTGAP )
    pTB->nSepWidth     = pWindow->ScaleX( PSTOOLBAR_DEFAULT_SEPWIDTH )
    pTB->nChevronWidth = pWindow->ScaleX( PSTOOLBAR_DEFAULT_CHEVRONWIDTH )
    pTB->nChevronGap   = pWindow->ScaleX( PSTOOLBAR_DEFAULT_CHEVRONGAP )
    pTB->nCurvature    = pWindow->ScaleX( PSTOOLBAR_DEFAULT_CURVATURE )
    ' nDividerThick is NOT scaled: PaintLine scales the pen it is handed, so scaling here
    ' too would double it above 100% (PsOptionButton's rule).

    ' Segoe Fluent Icons: ChevronDown (E70D) and "See more" (E712). Both settable.
    pTB->wszChevron  = !"\uE70D"
    pTB->wszOverflow = !"\uE712"

    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )
    pWindow->UserData(0) = cast(LONG_PTR, pTB)

    ' AFTER UserData, not before: PsToolbar_TipTextCallback resolves the control through
    ' GetToolbarPointer(hAttached), which reads exactly that slot.
    PsTipHost_Ensure( @pTB->tip, hCtrl, @PsToolbar_TipTextCallback )

    function = hCtrl
end function


' ========================================================================================
' Building the bar.
' ========================================================================================
function PsToolbar_AddItem( byval hToolbar as HWND, byval itemKind as long, byval Glyph as DWSTRING, byval Text as DWSTRING = "", byval id as long = 0, byval itemData as integer = 0 ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->AddItemSlot()
    if pItem = null then return -1
    pItem->itemKind = itemKind
    pItem->wszGlyph = Glyph
    pItem->wszText  = Text
    pItem->id       = id
    pItem->itemData = itemData
    return pTB->itemCount - 1
end function

function PsToolbar_InsertItem( byval hToolbar as HWND, byval idx as long, byval itemKind as long, byval Glyph as DWSTRING, byval Text as DWSTRING = "", byval id as long = 0, byval itemData as integer = 0 ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->InsertItemSlot( idx )
    if pItem = null then return -1
    pItem->itemKind = itemKind
    pItem->wszGlyph = Glyph
    pItem->wszText  = Text
    pItem->id       = id
    pItem->itemData = itemData
    return idx
end function

function PsToolbar_AddSeparator( byval hToolbar as HWND ) as long
    return PsToolbar_AddItem( hToolbar, TBR_KIND_SEPARATOR, "", "", 0, 0 )
end function

function PsToolbar_AddControl( byval hToolbar as HWND, byval hChild as HWND, byval nControlWidth as long, byval nControlHeight as long = 0, byval id as long = 0 ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->AddItemSlot()
    if pItem = null then return -1
    pItem->itemKind       = TBR_KIND_CONTROL
    pItem->hChild         = hChild
    pItem->nControlWidth  = nControlWidth
    pItem->nControlHeight = nControlHeight
    pItem->id             = id
    return pTB->itemCount - 1
end function

function PsToolbar_DeleteItem( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    return pTB->RemoveItemAt( idx )
end function

sub PsToolbar_Clear( byval hToolbar as HWND )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->Clear()
end sub

sub PsToolbar_Refresh( byval hToolbar as HWND )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->Refresh()
end sub


' ========================================================================================
' Counts and lookup.
' ========================================================================================
function PsToolbar_GetCount( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->itemCount
end function

function PsToolbar_IsValidItem( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    return pTB->IsValidItem(idx)
end function

function PsToolbar_FindItemByID( byval hToolbar as HWND, byval id as long ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    for i as long = 0 to pTB->itemCount - 1
        if pTB->items(i).id = id then return i
    next
    return -1
end function

function PsToolbar_HitTest( byval hToolbar as HWND, byval x as long, byval y as long ) as long
    dim as long nZone
    return PsToolbar_HitTestEx( hToolbar, x, y, nZone )
end function

function PsToolbar_HitTestEx( byval hToolbar as HWND, byval x as long, byval y as long, byref nZone as long ) as long
    nZone = TBR_ZONE_NONE
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    PsToolbar_EnsureLayout( pTB )
    return pTB->HitTestZone( x, y, nZone )
end function


' ========================================================================================
' Item contents and state.
' ========================================================================================
function PsToolbar_GetItemKind( byval hToolbar as HWND, byval idx as long ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return -1
    return pItem->itemKind
end function

function PsToolbar_GetGlyph( byval hToolbar as HWND, byval idx as long ) as DWSTRING
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return ""
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return ""
    return pItem->wszGlyph
end function

function PsToolbar_SetGlyph( byval hToolbar as HWND, byval idx as long, byval Glyph as DWSTRING ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->wszGlyph = Glyph
    ' Glyph OR image: setting an actual glyph frees any image on this item.
    if (len( Glyph ) > 0) andalso (pItem->pImage <> 0) then
        delete pItem->pImage : pItem->pImage = 0
    end if
    ' Adding or removing a glyph changes whether the icon cell and the text gap are charged
    ' at all, so this is a LAYOUT change, not a repaint.
    pTB->Refresh()
    return true
end function

' ========================================================================================
' Load an image into an item's icon cell, replacing its glyph. "" removes it. TRUE on a
' successful load; on failure the empty image is dropped so nothing draws.
' ========================================================================================
function PsToolbar_SetItemImage( byval hToolbar as HWND, byval idx as long, byval Path as DWSTRING ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false

    if len( Path ) = 0 then
        ' Removal SUCCEEDS. It used to return FALSE, which a caller could not tell apart from
        ' a load that failed -- so "did that do what I asked" had no answer on this path.
        if pItem->pImage then delete pItem->pImage : pItem->pImage = 0
        pTB->Refresh()
        return true
    end if

    if pItem->pImage = 0 then pItem->pImage = new PsImage
    dim as long bOK = pItem->pImage->Load( Path )
    if bOK = 0 then
        delete pItem->pImage : pItem->pImage = 0
    else
        pItem->wszGlyph = ""                 ' glyph OR image
    end if
    pTB->Refresh()
    return (bOK <> 0)
end function

function PsToolbar_GetText( byval hToolbar as HWND, byval idx as long ) as DWSTRING
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return ""
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return ""
    return pItem->wszText
end function

function PsToolbar_SetText( byval hToolbar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->wszText = Text
    pTB->Refresh()
    return true
end function

function PsToolbar_GetItemID( byval hToolbar as HWND, byval idx as long ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return 0
    return pItem->id
end function

function PsToolbar_SetItemID( byval hToolbar as HWND, byval idx as long, byval id as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->id = id
    return true
end function

function PsToolbar_GetItemData( byval hToolbar as HWND, byval idx as long ) as integer
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return 0
    return pItem->itemData
end function

function PsToolbar_SetItemData( byval hToolbar as HWND, byval idx as long, byval itemData as integer ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->itemData = itemData
    return true
end function

function PsToolbar_GetSelected( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    return pItem->isSelected
end function

' SILENT, but it DOES enforce the radio group -- a setter that left two members of a group
' checked would be a worse contract than one that notifies.
function PsToolbar_SetSelected( byval hToolbar as HWND, byval idx as long, byval isSelected as boolean ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    if pItem->isSelected = isSelected then return true
    pItem->isSelected = isSelected
    PsToolbar_InvalidateItem( pTB, idx )
    if isSelected then PsToolbar_ApplyRadio( pTB, hToolbar, idx, false )
    return true
end function

function PsToolbar_GetEnabled( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    return pItem->isEnabled
end function

function PsToolbar_SetEnabled( byval hToolbar as HWND, byval idx as long, byval isEnabled as boolean ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->isEnabled = isEnabled
    ' Disabling the item out from under a live press cancels the gesture -- the capture
    ' itself is the WndProc's to release.
    if (isEnabled = false) andalso (pTB->nPressedIdx = idx) then pTB->CancelPress()
    if (isEnabled = false) andalso (pTB->nLastHotIdx = idx) then
        pTB->nLastHotIdx = -1
        pTB->nHotZone    = TBR_ZONE_NONE
    end if
    PsToolbar_InvalidateItem( pTB, idx )
    return true
end function

function PsToolbar_GetVisible( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    return pItem->isVisible
end function

function PsToolbar_SetVisible( byval hToolbar as HWND, byval idx as long, byval isVisible as boolean ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    if pItem->isVisible = isVisible then return true
    pItem->isVisible = isVisible
    ' A hidden CONTROL cell's child must go with it, or the window would sit there over a
    ' cell that no longer exists.
    if (isVisible = false) andalso (pItem->itemKind = TBR_KIND_CONTROL) andalso (pItem->hChild <> 0) then
        if IsWindow( pItem->hChild ) then ShowWindow( pItem->hChild, SW_HIDE )
    end if
    if (isVisible = false) andalso (pTB->nPressedIdx = idx) then pTB->CancelPress()
    if (isVisible = false) andalso (pTB->nLastHotIdx = idx) then
        pTB->nLastHotIdx = -1
        pTB->nHotZone    = TBR_ZONE_NONE
    end if
    pTB->Refresh()
    return true
end function

function PsToolbar_GetItemGroupID( byval hToolbar as HWND, byval idx as long ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return 0
    return pItem->nGroupID
end function

function PsToolbar_SetItemGroupID( byval hToolbar as HWND, byval idx as long, byval nGroupID as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->nGroupID = nGroupID
    ' Joining a group while already checked would leave two members checked. Enforce it
    ' silently now rather than waiting for the user's first click to discover it.
    if (nGroupID <> 0) andalso pItem->isSelected then
        PsToolbar_ApplyRadio( pTB, hToolbar, idx, false )
    end if
    return true
end function

function PsToolbar_FindCheckedInGroup( byval hToolbar as HWND, byval nGroupID as long ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    if nGroupID = 0 then return -1
    for i as long = 0 to pTB->itemCount - 1
        with pTB->items(i)
            if (.nGroupID = nGroupID) andalso .isSelected then return i
        end with
    next
    return -1
end function

function PsToolbar_GetItemGravity( byval hToolbar as HWND, byval idx as long ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return TBR_GRAVITY_LEFT
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return TBR_GRAVITY_LEFT
    return pItem->nGravity
end function

function PsToolbar_SetItemGravity( byval hToolbar as HWND, byval idx as long, byval nGravity as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->nGravity = nGravity
    pTB->Refresh()
    return true
end function

function PsToolbar_GetItemChild( byval hToolbar as HWND, byval idx as long ) as HWND
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return 0
    return pItem->hChild
end function

function PsToolbar_FindItemByChild( byval hToolbar as HWND, byval hChild as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    if hChild = 0 then return -1
    for i as long = 0 to pTB->itemCount - 1
        if pTB->items(i).hChild = hChild then return i
    next
    return -1
end function

function PsToolbar_Click( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    if pItem->isEnabled = false then return false
    select case pItem->itemKind
    case TBR_KIND_SEPARATOR, TBR_KIND_CONTROL
        return false
    end select
    PsToolbar_ActivateItem( pTB, hToolbar, idx )
    return true
end function

function PsToolbar_Toggle( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    if pItem->itemKind <> TBR_KIND_TOGGLE then return false
    if pItem->isEnabled = false then return false
    PsToolbar_ToggleInternal( pTB, hToolbar, idx, true )
    return true
end function


' ========================================================================================
' Layout.
' ========================================================================================
sub PsToolbar_GetIconSize( byval hToolbar as HWND, byref nIconWidth as long, byref nIconHeight as long )
    nIconWidth = 0 : nIconHeight = 0
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    nIconWidth  = pTB->nIconWidth
    nIconHeight = pTB->nIconHeight
end sub

sub PsToolbar_SetIconSize( byval hToolbar as HWND, byval nIconWidth as long, byval nIconHeight as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nIconWidth  > 0 then pTB->nIconWidth  = nIconWidth
    if nIconHeight > 0 then pTB->nIconHeight = nIconHeight
    pTB->Refresh()
end sub

sub PsToolbar_GetPadding( byval hToolbar as HWND, byref nPadBefore as long, byref nPadAfter as long, byref nPadTop as long, byref nPadBottom as long )
    nPadBefore = 0 : nPadAfter = 0 : nPadTop = 0 : nPadBottom = 0
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    nPadBefore = pTB->nPadBefore
    nPadAfter  = pTB->nPadAfter
    nPadTop    = pTB->nPadTop
    nPadBottom = pTB->nPadBottom
end sub

sub PsToolbar_SetPadding( byval hToolbar as HWND, byval nPadBefore as long, byval nPadAfter as long, byval nPadTop as long, byval nPadBottom as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nPadBefore >= 0 then pTB->nPadBefore = nPadBefore
    if nPadAfter  >= 0 then pTB->nPadAfter  = nPadAfter
    if nPadTop    >= 0 then pTB->nPadTop    = nPadTop
    if nPadBottom >= 0 then pTB->nPadBottom = nPadBottom
    pTB->Refresh()
end sub

function PsToolbar_GetTextGap( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->nTextGap
end function

sub PsToolbar_SetTextGap( byval hToolbar as HWND, byval nTextGap as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nTextGap >= 0 then pTB->nTextGap = nTextGap
    pTB->Refresh()
end sub

function PsToolbar_GetSeparatorWidth( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->nSepWidth
end function

sub PsToolbar_SetSeparatorWidth( byval hToolbar as HWND, byval nSepWidth as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nSepWidth >= 0 then pTB->nSepWidth = nSepWidth
    pTB->Refresh()
end sub

function PsToolbar_GetSeparatorHeight( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->nSepHeight
end function

sub PsToolbar_SetSeparatorHeight( byval hToolbar as HWND, byval nSepHeight as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nSepHeight >= 0 then pTB->nSepHeight = nSepHeight
    pTB->Refresh()
end sub

sub PsToolbar_GetChevronSize( byval hToolbar as HWND, byref nChevronWidth as long, byref nChevronGap as long )
    nChevronWidth = 0 : nChevronGap = 0
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    nChevronWidth = pTB->nChevronWidth
    nChevronGap   = pTB->nChevronGap
end sub

sub PsToolbar_SetChevronSize( byval hToolbar as HWND, byval nChevronWidth as long, byval nChevronGap as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nChevronWidth > 0 then pTB->nChevronWidth = nChevronWidth
    if nChevronGap  >= 0 then pTB->nChevronGap   = nChevronGap
    pTB->Refresh()
end sub

function PsToolbar_GetDividerThickness( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->nDividerThick
end function

sub PsToolbar_SetDividerThickness( byval hToolbar as HWND, byval nThickness as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nThickness >= 0 then pTB->nDividerThick = nThickness
    pTB->Refresh()
end sub

function PsToolbar_GetCornerCurvature( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->nCurvature
end function

sub PsToolbar_SetCornerCurvature( byval hToolbar as HWND, byval nCurvature as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    if nCurvature >= 0 then pTB->nCurvature = nCurvature
    if pTB->hWin then InvalidateRect( pTB->hWin, NULL, FALSE )
end sub

function PsToolbar_SetItemPadding( byval hToolbar as HWND, byval idx as long, byval nPadBefore as long, byval nPadAfter as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->nPadBefore = nPadBefore
    pItem->nPadAfter  = nPadAfter
    pTB->Refresh()
    return true
end function

function PsToolbar_SetItemIconSize( byval hToolbar as HWND, byval idx as long, byval nIconWidth as long, byval nIconHeight as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->nIconWidth  = nIconWidth
    pItem->nIconHeight = nIconHeight
    pTB->Refresh()
    return true
end function

function PsToolbar_SetItemControlSize( byval hToolbar as HWND, byval idx as long, byval nControlWidth as long, byval nControlHeight as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    if nControlWidth  >= 0 then pItem->nControlWidth  = nControlWidth
    if nControlHeight >= 0 then pItem->nControlHeight = nControlHeight
    pTB->Refresh()
    return true
end function

function PsToolbar_GetItemRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    PsToolbar_EnsureLayout( pTB )
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    rc = pItem->rc
    return true
end function

function PsToolbar_GetItemIconRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    PsToolbar_EnsureLayout( pTB )
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    rc = pItem->rcIcon
    return true
end function

function PsToolbar_GetItemTextRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    PsToolbar_EnsureLayout( pTB )
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    rc = pItem->rcText
    return true
end function

function PsToolbar_GetItemChevronRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    PsToolbar_EnsureLayout( pTB )
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    rc = pItem->rcChevron
    return true
end function

function PsToolbar_GetOverflowRect( byval hToolbar as HWND, byref rc as RECT ) as boolean
    SetRectEmpty( @rc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    PsToolbar_EnsureLayout( pTB )
    if pTB->bNeedOverflow = false then return false
    rc = pTB->rcOverflow
    return true
end function

sub PsToolbar_GetIdealSize( byval hToolbar as HWND, byref nWidth as long, byref nHeight as long )
    nWidth = 0 : nHeight = 0
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    ' Deliberately NOT PsToolbar_EnsureLayout: nTotalWidth and nIdealHeight are produced
    ' before LayoutItems bails on a zero client, precisely so a host can call this to decide
    ' how big to make the bar in the first place.
    if pTB->bLayoutDirty then pTB->LayoutItems()
    nWidth  = pTB->nTotalWidth
    nHeight = pTB->nIdealHeight
end sub

function PsToolbar_GetIdealWidth( byval hToolbar as HWND ) as long
    dim as long w, h
    PsToolbar_GetIdealSize( hToolbar, w, h )
    return w
end function

function PsToolbar_IsItemOverflowed( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    PsToolbar_EnsureLayout( pTB )
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    return pItem->bOverflowed
end function

function PsToolbar_GetOverflowCount( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    PsToolbar_EnsureLayout( pTB )
    dim as long n = 0
    for i as long = 0 to pTB->itemCount - 1
        with pTB->items(i)
            if .isVisible andalso .bOverflowed andalso (.bTrimmed = false) then n += 1
        end with
    next
    return n
end function


' ========================================================================================
' Appearance.
' ========================================================================================
sub PsToolbar_GetColors( byval hToolbar as HWND, byval pColors as PSTOOLBAR_COLORS ptr )
    if pColors = null then exit sub
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    *pColors = pTB->colors
end sub

sub PsToolbar_SetColors( byval hToolbar as HWND, byval pColors as PSTOOLBAR_COLORS ptr )
    if pColors = null then exit sub
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->colors = *pColors
    ' Theming the bar themes its menus for free -- unless the host claimed them.
    PsToolbar_ReapplyAllMenuColors( pTB )
    if pTB->hWin then InvalidateRect( pTB->hWin, NULL, TRUE )
end sub

function PsToolbar_SetItemForeColor( byval hToolbar as HWND, byval idx as long, byval clr as COLORREF ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->ForeColorItem = clr
    pItem->bHasForeColor = true
    PsToolbar_InvalidateItem( pTB, idx )
    return true
end function

function PsToolbar_ClearItemForeColor( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->bHasForeColor = false
    PsToolbar_InvalidateItem( pTB, idx )
    return true
end function

function PsToolbar_GetTextFont( byval hToolbar as HWND ) as HFONT
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->hTextFont
end function

' A LAYOUT input: every caption is re-measured, so this moves things.
function PsToolbar_SetTextFont( byval hToolbar as HWND, byval hTextFont as HFONT ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    pTB->hTextFont = hTextFont
    ' Menus we own borrow the same font, so they stay in step with the bar.
    if pTB->hOverflowMenu <> 0 then
        if IsWindow( pTB->hOverflowMenu ) then PsPopupMenu_SetFonts( pTB->hOverflowMenu, pTB->hTextFont, pTB->hGlyphFont )
    end if
    for i as long = 0 to pTB->itemCount - 1
        if pTB->items(i).hMenu <> 0 then
            if IsWindow( pTB->items(i).hMenu ) then PsPopupMenu_SetFonts( pTB->items(i).hMenu, pTB->hTextFont, pTB->hGlyphFont )
        end if
    next
    pTB->Refresh()
    return true
end function

function PsToolbar_GetGlyphFont( byval hToolbar as HWND ) as HFONT
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return pTB->hGlyphFont
end function

' A PAINT-time input only: the icon cell is DECLARED, never measured, so this repaints but
' never re-lays-out. A glyph font too large for its cell simply clips.
function PsToolbar_SetGlyphFont( byval hToolbar as HWND, byval hGlyphFont as HFONT ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    pTB->hGlyphFont = hGlyphFont
    if pTB->hWin then InvalidateRect( pTB->hWin, NULL, FALSE )
    return true
end function

sub PsToolbar_GetGlyphs( byval hToolbar as HWND, byref wszChevron as DWSTRING, byref wszOverflow as DWSTRING )
    wszChevron = "" : wszOverflow = ""
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    wszChevron  = pTB->wszChevron
    wszOverflow = pTB->wszOverflow
end sub

sub PsToolbar_SetGlyphs( byval hToolbar as HWND, byval wszChevron as DWSTRING, byval wszOverflow as DWSTRING )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->wszChevron  = wszChevron
    pTB->wszOverflow = wszOverflow
    if pTB->hWin then InvalidateRect( pTB->hWin, NULL, FALSE )
end sub


' ========================================================================================
' Dropdown menus.
' ========================================================================================
function PsToolbar_GetItemMenu( byval hToolbar as HWND, byval idx as long ) as HWND
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return PsToolbar_EnsureItemMenu( pTB, hToolbar, idx )
end function

function PsToolbar_GetOverflowMenu( byval hToolbar as HWND ) as HWND
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return PsToolbar_EnsureOverflowMenu( pTB, hToolbar )
end function

sub PsToolbar_SetMenuColors( byval hToolbar as HWND, byval pColors as PSPOPUPMENU_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    ' Claimed permanently, and applied to every menu that already exists.
    pTB->bMenuColorsCustom = true
    pTB->menuColors        = *pColors
    if pTB->hOverflowMenu <> 0 then
        if IsWindow( pTB->hOverflowMenu ) then PsPopupMenu_SetColors( pTB->hOverflowMenu, pColors )
    end if
    for i as long = 0 to pTB->itemCount - 1
        if pTB->items(i).hMenu <> 0 then
            if IsWindow( pTB->items(i).hMenu ) then PsPopupMenu_SetColors( pTB->items(i).hMenu, pColors )
        end if
    next
end sub


function PsToolbar_IsDroppedDown( byval hToolbar as HWND ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    return (pTB->nDroppedIdx <> -1) orelse pTB->bOverflowOpen
end function

' -1 = the overflow menu is open, or nothing is. Pair it with IsDroppedDown to tell those
' two apart.
function PsToolbar_GetDroppedItem( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return -1
    return pTB->nDroppedIdx
end function

function PsToolbar_DropDown( byval hToolbar as HWND, byval idx as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    return PsToolbar_OpenMenu( pTB, hToolbar, idx )
end function

sub PsToolbar_CloseUp( byval hToolbar as HWND )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    dim as HWND hOpen = 0
    if pTB->bOverflowOpen then
        hOpen = pTB->hOverflowMenu
    elseif pTB->IsValidItem( pTB->nDroppedIdx ) then
        hOpen = pTB->items( pTB->nDroppedIdx ).hMenu
    end if
    ' PsPopupMenu's Hide is SILENT (setters do not notify), so our own closing edge has to be
    ' fired here rather than waiting for a CPM_NOTIFY_CLOSED that will never arrive.
    if hOpen <> 0 then
        if IsWindow( hOpen ) then PsPopupMenu_CloseChain( hOpen )
    end if
    PsToolbar_ClearDropState( pTB, hToolbar )
end sub


' ========================================================================================
' Tooltips.
' ========================================================================================
function PsToolbar_GetTooltipText( byval hToolbar as HWND, byval idx as long ) as DWSTRING
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return ""
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return ""
    return pItem->wszTooltip
end function

function PsToolbar_SetTooltipText( byval hToolbar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return false
    dim as PSTOOLBAR_ITEM ptr pItem = pTB->GetItem(idx)
    if pItem = null then return false
    pItem->wszTooltip = Text
    return true
end function

' The comctl32 handle, and 0 while on PsTooltip -- the honest answer, since a TTM_* sent
' to a PsTooltip window is silently ignored.
function PsToolbar_GetTooltipHandle( byval hToolbar as HWND ) as HWND
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then return 0
    return PsTipHost_SysHandle( @pTB->tip )
end function

' The PsTooltip window, and 0 while on the system backend. This is the door to PsTooltip's
' own SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph, none of which is mirrored
' on this control.
function PsToolbar_GetPsTooltipHandle( byval hToolbar as HWND ) as HWND
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then return 0
    return PsTipHost_PsHandle( @pTB->tip )
end function

function PsToolbar_SetTooltipMode( byval hToolbar as HWND, byval nMode as long ) as boolean
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then return false
    return PsTipHost_SetMode( @pTB->tip, nMode )
end function

function PsToolbar_GetTooltipMode( byval hToolbar as HWND ) as long
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pTB->tip )
end function

' The three delays go through PsTipHost, which STORES them as well as pushing them, so a
' hover time set before a mode switch is still in force after it. Pushing straight at the
' live tooltip loses it at the switch -- and the tip still appears, just on a different
' beat, with nothing reporting the change.
sub PsToolbar_SetHoverTime( byval hToolbar as HWND, byval milliseconds as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then exit sub
    ' The field is STILL READ by TrackMouseEvent as dwHoverTime, so this setter
    ' drives the hot-tracking dwell as well as the tooltip. Dropping it here would
    ' silently change when the control considers itself hot.
    pTB->HoverTime = milliseconds
    PsTipHost_SetDelay( @pTB->tip, TTDT_INITIAL, milliseconds )
end sub

sub PsToolbar_SetAutoPopTime( byval hToolbar as HWND, byval milliseconds as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then exit sub
    PsTipHost_SetDelay( @pTB->tip, TTDT_AUTOPOP, milliseconds )
end sub

sub PsToolbar_SetReshowTime( byval hToolbar as HWND, byval milliseconds as long )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then exit sub
    PsTipHost_SetDelay( @pTB->tip, TTDT_RESHOW, milliseconds )
end sub


' ========================================================================================
' Callbacks.
' ========================================================================================
sub PsToolbar_SetPaintItemCallback( byval hToolbar as HWND, byval usersub as TBR_PaintItemCallbackSub )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->PaintItemCallback = usersub
    if pTB->hWin then InvalidateRect( pTB->hWin, NULL, TRUE )
end sub

sub PsToolbar_SetMessageCallback( byval hToolbar as HWND, byval userfunc as TBR_MessageCallbackFunc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->MessageCallback = userfunc
end sub

sub PsToolbar_SetTooltipCallback( byval hToolbar as HWND, byval userfunc as TBR_TooltipCallbackFunc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->TooltipCallback = userfunc
end sub

sub PsToolbar_SetClickCallback( byval hToolbar as HWND, byval usersub as TBR_ClickCallbackSub )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->ClickCallback = usersub
end sub

sub PsToolbar_SetSelChangeCallback( byval hToolbar as HWND, byval usersub as TBR_SelChangeCallbackSub )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->SelChangeCallback = usersub
end sub

sub PsToolbar_SetDropDownCallback( byval hToolbar as HWND, byval usersub as TBR_DropDownCallbackSub )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->DropDownCallback = usersub
end sub

sub PsToolbar_SetControlNotifyCallback( byval hToolbar as HWND, byval userfunc as TBR_ControlNotifyFunc )
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = null then exit sub
    pTB->ControlNotifyCallback = userfunc
end sub


' ========================================================================================
' THE message-pump hook. See the contract in PsToolbar.bi.
'
' Two jobs, in this order:
'
'   1. ARM THE REOPEN GUARD. PsPopupMenu's own filter dismisses the chain on an outside click
'      and then returns FALSE ON PURPOSE, so the click still reaches its target ("clicking a
'      toolbar button while a menu is open both closes the menu and presses the button").
'      For a click on the very chevron that opened the menu that is exactly wrong: the menu
'      would close here and the WM_LBUTTONDOWN that follows would immediately reopen it,
'      making the button impossible to close by clicking. So arm a one-shot flag that the
'      very next WM_LBUTTONDOWN consumes.
'
'      THE GUARD IS ARMED ONLY FOR THE ZONE THAT OWNS THE OPEN MENU -- narrower than
'      PsComboBox, which arms for any click on itself. A toolbar has many items, and arming
'      that broadly would make clicking button B while button A's menu is open do nothing at
'      all. Here that click correctly closes A and presses B, which is the behaviour
'      PsPopupMenu's FALSE return exists to permit.
'
'   2. DELEGATE to PsPopupMenu's filter, which owns keyboard navigation, Escape, and the
'      dismissal itself.
'
' Returns FALSE immediately when no menu of ours is open, so a host with several toolbars can
' call it for each one in its pump at negligible cost.
' ========================================================================================
function PsToolbar_FilterMessage( byval hToolbar as HWND, byval pMsg as MSG ptr ) as boolean
    if pMsg = 0 then return false
    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then return false

    dim as HWND hOpen = 0
    if pTB->bOverflowOpen then
        hOpen = pTB->hOverflowMenu
    elseif pTB->IsValidItem( pTB->nDroppedIdx ) then
        hOpen = pTB->items( pTB->nDroppedIdx ).hMenu
    end if
    if hOpen = 0 then return false
    if IsWindow( hOpen ) = 0 then return false
    if PsPopupMenu_IsOpen( hOpen ) = false then return false

    if pMsg->message = WM_LBUTTONDOWN then
        if pMsg->hwnd = hToolbar then
            ' MSG.pt is in SCREEN coordinates for a mouse message.
            dim as POINT pt = pMsg->pt
            ScreenToClient( hToolbar, @pt )
            dim as long nZone
            dim as long idx = pTB->HitTestZone( pt.x, pt.y, nZone )

            if pTB->bOverflowOpen then
                if nZone = TBR_ZONE_OVERFLOW then pTB->bSuppressNextOpen = true
            elseif idx = pTB->nDroppedIdx then
                dim as long nKind = pTB->items(idx).itemKind
                dim as boolean bOwnsIt = (nKind = TBR_KIND_DROPDOWN) orelse (nZone = TBR_ZONE_CHEVRON)
                if bOwnsIt then pTB->bSuppressNextOpen = true
            end if
        end if
    end if

    return PsPopupMenu_FilterMessage( hOpen, pMsg )
end function


' ========================================================================================
' Render probes. See the warning in PsToolbar.bi: DO NOT COPY A THRESHOLD FROM A SIBLING.
'
' Both drive PsToolbar_RenderAll -- the exact path WM_PAINT uses -- rather than poking one
' item through a callback, so a flooded or frozen renderer cannot hide behind them.
' ========================================================================================
private function PsToolbar_PartRect( byval pTB as PSTOOLBAR ptr, byval nPart as long, byval idx as long ) as RECT
    dim as RECT rc
    SetRectEmpty( @rc )
    if pTB = null then return rc

    if nPart = TBR_PART_TOOLBAR then
        GetClientRect( pTB->hWin, @rc )
        return rc
    end if

    if idx = -1 then
        if nPart = TBR_PART_CHEVRON then return pTB->rcOverflow
        if nPart = TBR_PART_ITEM then return pTB->rcOverflow
        return rc
    end if

    if pTB->IsValidItem(idx) = false then return rc
    select case nPart
    case TBR_PART_ITEM    : rc = pTB->items(idx).rc
    case TBR_PART_ICON    : rc = pTB->items(idx).rcIcon
    case TBR_PART_TEXT    : rc = pTB->items(idx).rcText
    case TBR_PART_CHEVRON : rc = pTB->items(idx).rcChevron
    end select
    return rc
end function


' Render the whole bar into a 32bpp DIB and hand the caller the bits plus the clamped part
' rect. Returns FALSE when there is nothing to measure. The caller owns nothing: hbm/hMem are
' returned so it can free them after reading.
private function PsToolbar_RenderToDIB( _
            byval hToolbar as HWND, _
            byval nPart as long, _
            byval idx as long, _
            byref rcPart as RECT, _
            byref cw as long, _
            byref ch as long, _
            byref pBits as any ptr, _
            byref hbm as HBITMAP, _
            byref hMem as HDC _
            ) as boolean

    pBits = 0 : hbm = 0 : hMem = 0 : cw = 0 : ch = 0
    SetRectEmpty( @rcPart )

    dim as PSTOOLBAR ptr pTB = GetToolbarPointer(hToolbar)
    if pTB = 0 then return false
    if PsToolbar_EnsureLayout( pTB ) = false then return false

    dim as RECT rcClient
    GetClientRect( hToolbar, @rcClient )
    cw = rcClient.right - rcClient.left
    ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return false

    rcPart = PsToolbar_PartRect( pTB, nPart, idx )
    if IsRectEmpty( @rcPart ) then return false

    ' Clamp to the client: a cell can legitimately extend past it when the run clips, and the
    ' DIB only covers the client.
    if rcPart.left   < 0  then rcPart.left   = 0
    if rcPart.top    < 0  then rcPart.top    = 0
    if rcPart.right  > cw then rcPart.right  = cw
    if rcPart.bottom > ch then rcPart.bottom = ch
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return false

    dim as HDC hRef = GetDC( hToolbar )
    if hRef = 0 then return false
    hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( hToolbar, hRef )
        return false
    end if

    ' 32bpp top-down, matching what PsBufferPaint.SetupBitmap builds, so the pixels we read
    ' are laid out the way the renderer wrote them.
    dim as BITMAPINFO bmi
    with bmi.bmiHeader
        .biSize        = sizeof(BITMAPINFOHEADER)
        .biWidth       = cw
        .biHeight      = -ch
        .biPlanes      = 1
        .biBitCount    = 32
        .biCompression = BI_RGB
    end with
    hbm = CreateDIBSection( hRef, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )
    ReleaseDC( hToolbar, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        hbm = 0 : hMem = 0 : pBits = 0
        return false
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    scope
        dim as PsBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( hToolbar, hMem, rcClient, hMem )
        PsToolbar_RenderAll( pTB, hToolbar, @b, rcClient )
        b.EndDoubleBuffer()
    end scope

    SelectObject( hMem, hOldBmp )
    return true
end function


function PsToolbar_CountRenderedTones( byval hToolbar as HWND, byval nPart as long, byval idx as long ) as long
    dim as RECT rcPart
    dim as long cw, ch
    dim as any ptr pBits
    dim as HBITMAP hbm
    dim as HDC hMem
    if PsToolbar_RenderToDIB( hToolbar, nPart, idx, rcPart, cw, ch, pBits, hbm, hMem ) = false then return 0

    ' Count distinct tones. The alpha byte is masked off: GDI text leaves it at 0 and GDI+
    ' shapes leave it at 255, so an unmasked compare would report two tones for one colour.
    dim tones(0 to 255) as ulong
    dim as long nTones = 0
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for y as long = rcPart.top to rcPart.bottom - 1
        for x as long = rcPart.left to rcPart.right - 1
            dim as ulong c = px[(y * cw) + x] and &h00FFFFFF
            dim as boolean bFound = false
            for t as long = 0 to nTones - 1
                if tones(t) = c then bFound = true : exit for
            next
            if bFound = false then
                if nTones < 256 then
                    tones(nTones) = c
                    nTones += 1
                end if
            end if
        next
    next

    DeleteObject( hbm )
    DeleteDC( hMem )
    return nTones
end function


function PsToolbar_HashRenderedPart( byval hToolbar as HWND, byval nPart as long, byval idx as long ) as ulong
    dim as RECT rcPart
    dim as long cw, ch
    dim as any ptr pBits
    dim as HBITMAP hbm
    dim as HDC hMem
    if PsToolbar_RenderToDIB( hToolbar, nPart, idx, rcPart, cw, ch, pBits, hbm, hMem ) = false then return 0

    ' FNV-1a over the masked pixels of the part rect.
    dim as ulong h = &h811C9DC5
    dim as ulong ptr px = cast( ulong ptr, pBits )
    for y as long = rcPart.top to rcPart.bottom - 1
        for x as long = rcPart.left to rcPart.right - 1
            dim as ulong c = px[(y * cw) + x] and &h00FFFFFF
            for shiftBy as long = 0 to 24 step 8
                h = h xor ((c shr shiftBy) and &hFF)
                h = h * &h01000193
            next
        next
    next

    DeleteObject( hbm )
    DeleteDC( hMem )
    return h
end function


' ========================================================================================
' Self-test.  Gated on PSTOOLBAR_SELFTEST=1.
'
' Every number is asserted, never eyeballed -- CLAUDE.md's rule, and Learnings.md records
' several "verifications" that were invalid because they were done by eye.
'
' TWO RULES THIS FILE FOLLOWS BECAUSE THE FAMILY LEARNED THEM THE HARD WAY:
'
'   1. THE RISKY ARITHMETIC IS TESTED THROUGH PURE FUNCTIONS, with no window and no message
'      pump, so the overflow ladder can be driven through cases a live control would never
'      reach (PsProgressBar's precedent -- and the only reason its two vacuous assertions
'      were catchable at all).
'
'   2. NO PROBE THRESHOLD IS INHERITED FROM A SIBLING. PsOptionButton recorded PsButton's
'      "> 1" tone test PASSING on a completely destroyed render, because the healthy number
'      depends entirely on what else is inside the part rect. Every floor below was measured
'      against this control's own healthy and flooded output.
'
' WHAT THIS CANNOT COVER, and is therefore left to the author's interactive pass: the pixel
' appearance of the chrome, hover, press-and-slide-off, capture cancel, opening and closing a
' dropdown by clicking its own chevron, clicking another button while a menu is open, and the
' embedded CONTROL child's own behaviour.
' ========================================================================================
dim shared as long   gTBR_TestCount
dim shared as long   gTBR_FailCount
dim shared as string gTBR_Log            ' notification order, recorded by the test callbacks
dim shared as long   gTBR_DropOpens
dim shared as long   gTBR_DropCloses

private sub TBRTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gTBR_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gTBR_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function TBRTest_RectStr( byref rc as RECT ) as string
    return "(" & str(rc.left) & "," & str(rc.top) & ")-(" & str(rc.right) & "," & str(rc.bottom) & ")"
end function

private sub TBRTest_SelChange( byval hToolbar as HWND, byval idx as long, byval isSelected as boolean )
    ' Records the ORDER as well as the values -- "loser first" is not observable any other way.
    gTBR_Log &= "S" & str(idx) & iif( isSelected, "+", "-" ) & ";"
end sub

private sub TBRTest_Click( byval hToolbar as HWND, byval idx as long, byval id as long )
    gTBR_Log &= "C" & str(idx) & "/" & str(id) & ";"
end sub

dim shared as long   gTBR_NotifyIdx
dim shared as HWND   gTBR_NotifyChild
dim shared as UINT   gTBR_NotifyMsg
dim shared as long   gTBR_NotifyCount
dim shared as boolean gTBR_NotifyClaim

private function TBRTest_ControlNotify( byval m as PSTOOLBAR_CONTROLNOTIFY ptr ) as boolean
    if m = 0 then return false
    gTBR_NotifyCount += 1
    gTBR_NotifyIdx    = m->idx
    gTBR_NotifyChild  = m->hChild
    gTBR_NotifyMsg    = m->uMsg
    if gTBR_NotifyClaim then
        m->lResult = 12345
        return true
    end if
    return false
end function

private sub TBRTest_DropDown( byval hToolbar as HWND, byval idx as long, byval hMenu as HWND, byval isOpen as boolean )
    if isOpen then gTBR_DropOpens += 1 else gTBR_DropCloses += 1
end sub


sub PsToolbar_RunSelfTest( byval hWndParent as HWND )
    if environ("PSTOOLBAR_SELFTEST") <> "1" then exit sub

    gTBR_TestCount = 0
    gTBR_FailCount = 0
    print "=== PsToolbar self-test ==="

    dim as string sD                     ' every trace string is built here first: PRINT on a
                                         ' DWSTRING, or on a string & wstring concat,
                                         ' interleaves nulls (Learnings.md).

    ' ====================================================================================
    ' PART A -- PsToolbar_ComputeCellWidth. Pure: no window, no DC, no layout.
    ' ====================================================================================
    const PADB = 6, PADA = 6, ICONW = 16, TGAP = 6, SEPW = 1, CHEVW = 14, CHEVG = 4, DIVT = 1

    dim as long wIconOnly = PsToolbar_ComputeCellWidth( TBR_KIND_COMMAND, ICONW, 0, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wIconOnly) & " expect=" & str(PADB + ICONW + PADA)
    TBRTest_Check( "cell: COMMAND icon-only = pad+icon+pad", (wIconOnly = PADB + ICONW + PADA), sD )

    dim as long wTextOnly = PsToolbar_ComputeCellWidth( TBR_KIND_COMMAND, 0, 40, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wTextOnly) & " expect=" & str(PADB + 40 + PADA) & " (no gap charged)"
    TBRTest_Check( "cell: COMMAND text-only charges NO text gap", (wTextOnly = PADB + 40 + PADA), sD )

    dim as long wBoth = PsToolbar_ComputeCellWidth( TBR_KIND_COMMAND, ICONW, 40, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wBoth) & " expect=" & str(PADB + ICONW + TGAP + 40 + PADA)
    TBRTest_Check( "cell: COMMAND icon+text charges the gap once", (wBoth = PADB + ICONW + TGAP + 40 + PADA), sD )

    sD = "iconOnly=" & str(wIconOnly) & " both=" & str(wBoth)
    TBRTest_Check( "cell: an icon-only item is narrower than a captioned one", (wIconOnly < wBoth), sD )

    dim as long wSep = PsToolbar_ComputeCellWidth( TBR_KIND_SEPARATOR, ICONW, 40, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wSep) & " expect=" & str(PADB + SEPW + PADA) & " (icon/text ignored)"
    TBRTest_Check( "cell: SEPARATOR = pad+rule+pad", (wSep = PADB + SEPW + PADA), sD )

    dim as long wCtrl = PsToolbar_ComputeCellWidth( TBR_KIND_CONTROL, ICONW, 40, TGAP, SEPW, 150, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wCtrl) & " expect=" & str(PADB + 150 + PADA)
    TBRTest_Check( "cell: CONTROL = pad+declared width+pad", (wCtrl = PADB + 150 + PADA), sD )

    dim as long wSplit = PsToolbar_ComputeCellWidth( TBR_KIND_SPLIT, ICONW, 40, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wSplit) & " expect=" & str(PADB + ICONW + TGAP + 40 + CHEVG + DIVT + CHEVW + PADA)
    TBRTest_Check( "cell: SPLIT adds gap + divider + chevron", (wSplit = PADB + ICONW + TGAP + 40 + CHEVG + DIVT + CHEVW + PADA), sD )

    dim as long wSplitBare = PsToolbar_ComputeCellWidth( TBR_KIND_SPLIT, 0, 0, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wSplitBare) & " expect=" & str(PADB + CHEVW + PADA) & " (no action zone to divide)"
    TBRTest_Check( "cell: SPLIT with no content charges NO divider", (wSplitBare = PADB + CHEVW + PADA), sD )

    dim as long wDrop = PsToolbar_ComputeCellWidth( TBR_KIND_DROPDOWN, ICONW, 0, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wDrop) & " expect=" & str(PADB + ICONW + CHEVG + CHEVW + PADA)
    TBRTest_Check( "cell: DROPDOWN adds chevron gap + chevron", (wDrop = PADB + ICONW + CHEVG + CHEVW + PADA), sD )

    dim as long wDropBare = PsToolbar_ComputeCellWidth( TBR_KIND_DROPDOWN, 0, 0, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, PADB, PADA )
    sD = "got=" & str(wDropBare) & " expect=" & str(PADB + CHEVW + PADA)
    TBRTest_Check( "cell: DROPDOWN with no content charges NO chevron gap", (wDropBare = PADB + CHEVW + PADA), sD )

    dim as long wPerItemPad = PsToolbar_ComputeCellWidth( TBR_KIND_COMMAND, ICONW, 0, TGAP, SEPW, 0, CHEVW, CHEVG, DIVT, 20, 30 )
    sD = "got=" & str(wPerItemPad) & " expect=" & str(20 + ICONW + 30)
    TBRTest_Check( "cell: padding is per-item, not fixed", (wPerItemPad = 20 + ICONW + 30), sD )


    ' ====================================================================================
    ' PART B -- PsToolbar_ComputeOverflowSplit. The riskiest arithmetic in the control, and
    ' the whole reason it is a pure function: these cases are driven directly, with no
    ' window, no resize, and no paint.
    ' ====================================================================================
    scope
        ' Six items of 100px each: four LEFT, then two RIGHT.
        const N = 6
        dim cw(0 to N-1) as long
        dim cg(0 to N-1) as long
        dim ck(0 to N-1) as long
        dim ov(0 to N-1) as boolean
        dim as boolean bNeed

        for i as long = 0 to N-1
            cw(i) = 100
            ck(i) = TBR_KIND_COMMAND
            cg(i) = iif( i < 4, TBR_GRAVITY_LEFT, TBR_GRAVITY_RIGHT )
        next

        ' ---- everything fits ----------------------------------------------------------
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 700, 30, ov(), bNeed )
        dim as long nOvr = 0
        for i as long = 0 to N-1
            if ov(i) then nOvr += 1
        next
        sD = "need=" & str(bNeed) & " overflowed=" & str(nOvr)
        TBRTest_Check( "overflow: room to spare -> no chevron at all", (bNeed = false) andalso (nOvr = 0), sD )

        ' ---- EXACT fit is not overflow ------------------------------------------------
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 600, 30, ov(), bNeed )
        nOvr = 0
        for i as long = 0 to N-1
            if ov(i) then nOvr += 1
        next
        sD = "total=600 client=600 need=" & str(bNeed)
        TBRTest_Check( "overflow: an EXACT fit does not overflow", (bNeed = false) andalso (nOvr = 0), sD )

        ' ---- one pixel short ----------------------------------------------------------
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 599, 30, ov(), bNeed )
        sD = "total=600 client=599 need=" & str(bNeed)
        TBRTest_Check( "overflow: one pixel short DOES overflow", (bNeed = true), sD )

        ' ---- evicts from the TAIL of the LEFT run, last item first ---------------------
        sD = "ov(3)=" & str(ov(3)) & " ov(0)=" & str(ov(0)) & " ov(4)=" & str(ov(4)) & " ov(5)=" & str(ov(5))
        TBRTest_Check( "overflow: evicts the LAST left item first", _
                       ov(3) andalso (ov(0) = false) andalso (ov(4) = false) andalso (ov(5) = false), sD )

        ' ---- and keeps walking backwards ----------------------------------------------
        ' client 350, chevron 30 -> avail 320, so three left items must go (600-300=300<=320).
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 350, 30, ov(), bNeed )
        sD = "ov = " & str(ov(0)) & str(ov(1)) & str(ov(2)) & str(ov(3)) & str(ov(4)) & str(ov(5))
        TBRTest_Check( "overflow: walks the left run backwards, 3 evicted", _
                       (ov(0) = false) andalso ov(1) andalso ov(2) andalso ov(3), sD )

        sD = "ov(4)=" & str(ov(4)) & " ov(5)=" & str(ov(5))
        TBRTest_Check( "overflow: RIGHT-gravity items survive while left ones remain", _
                       (ov(4) = false) andalso (ov(5) = false), sD )

        ' ---- the right run is eaten FRONT first, so the outermost item survives --------
        ' client 130, chevron 30 -> avail 100. All four left go (200 left), then right item 4.
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 130, 30, ov(), bNeed )
        sD = "ov(4)=" & str(ov(4)) & " ov(5)=" & str(ov(5))
        TBRTest_Check( "overflow: right run is eaten FRONT first (outermost survives)", _
                       ov(4) andalso (ov(5) = false), sD )

        ' ---- CONTROL cells are pinned -------------------------------------------------
        ck(3) = TBR_KIND_CONTROL
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 599, 30, ov(), bNeed )
        sD = "ov(3)=" & str(ov(3)) & " (CONTROL) ov(2)=" & str(ov(2))
        TBRTest_Check( "overflow: a CONTROL cell is NEVER evicted", (ov(3) = false) andalso ov(2), sD )

        ' ---- nothing but pinned cells left: stop, do not spin --------------------------
        for i as long = 0 to N-1
            ck(i) = TBR_KIND_CONTROL
        next
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 100, 30, ov(), bNeed )
        nOvr = 0
        for i as long = 0 to N-1
            if ov(i) then nOvr += 1
        next
        sD = "need=" & str(bNeed) & " evicted=" & str(nOvr) & " (must terminate)"
        TBRTest_Check( "overflow: an all-pinned run terminates and clips instead of spinning", _
                       (bNeed = true) andalso (nOvr = 0), sD )

        ' ---- a fitting call RESETS the flags from a previous overflowing one -----------
        for i as long = 0 to N-1
            ck(i) = TBR_KIND_COMMAND
        next
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 130, 30, ov(), bNeed )
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), N, 700, 30, ov(), bNeed )
        nOvr = 0
        for i as long = 0 to N-1
            if ov(i) then nOvr += 1
        next
        sD = "evicted after re-fit=" & str(nOvr)
        TBRTest_Check( "overflow: flags are fully reset by a fitting call", (nOvr = 0), sD )

        ' ---- an empty run is legal ----------------------------------------------------
        PsToolbar_ComputeOverflowSplit( cw(), cg(), ck(), 0, 100, 30, ov(), bNeed )
        sD = "need=" & str(bNeed)
        TBRTest_Check( "overflow: zero items never needs a chevron", (bNeed = false), sD )
    end scope


    ' ====================================================================================
    ' PART C -- PsToolbar_ResolveMood. The precedence table, asserted rather than assumed.
    ' ====================================================================================
    TBRTest_Check( "mood: disabled beats pressed", _
        (PsToolbar_ResolveMood( false, true, false ) = TBR_MOOD_DISABLED), "disabled+pressed" )
    TBRTest_Check( "mood: disabled beats hot", _
        (PsToolbar_ResolveMood( false, false, true ) = TBR_MOOD_DISABLED), "disabled+hot" )
    TBRTest_Check( "mood: pressed beats hot", _
        (PsToolbar_ResolveMood( true, true, true ) = TBR_MOOD_PRESSED), "pressed+hot" )
    TBRTest_Check( "mood: hot beats idle", _
        (PsToolbar_ResolveMood( true, false, true ) = TBR_MOOD_HOT), "hot" )
    TBRTest_Check( "mood: nothing set is idle", _
        (PsToolbar_ResolveMood( true, false, false ) = TBR_MOOD_IDLE), "idle" )


    ' ====================================================================================
    ' The live control. Every layout input is pinned to a known value so the arithmetic
    ' below is exact rather than DPI-dependent. Only the CAPTIONS stay measured, and every
    ' assertion involving them is written against what the control itself measured.
    ' ====================================================================================
    dim as HWND h = PsToolbar_Create( hWndParent, 9901 )
    if h = 0 then
        print "FAIL  create   [PsToolbar_Create returned 0]"
        exit sub
    end if
    dim as PSTOOLBAR ptr pT = GetToolbarPointer(h)
    if pT = 0 then
        print "FAIL  pointer   [GetToolbarPointer returned 0]"
        DestroyWindow(h)
        exit sub
    end if

    ' A REAL font, not the stock fallback. The render probes at the end measure antialiasing,
    ' and the stock GUI font in an offscreen DIB renders unsmoothed -- which reports exactly
    ' two tones and would make every probe assertion below meaningless. Deleted at the end.
    dim as HFONT hTestFont = CreateFontW( -12, 0, 0, 0, FW_NORMAL, 0, 0, 0, _
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, _
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH, @wstr("Segoe UI") )
    PsToolbar_SetTextFont( h, hTestFont )
    PsToolbar_SetGlyphFont( h, hTestFont )

    PsToolbar_SetPadding( h, PADB, PADA, 4, 4 )
    PsToolbar_SetIconSize( h, ICONW, 16 )
    PsToolbar_SetTextGap( h, TGAP )
    PsToolbar_SetSeparatorWidth( h, SEPW )
    PsToolbar_SetChevronSize( h, CHEVW, CHEVG )
    PsToolbar_SetDividerThickness( h, DIVT )
    PsToolbar_SetCornerCurvature( h, 0 )

    ' Plain letters, not Segoe Fluent Icons codepoints: the control does not care what the
    ' glyph string is, and a letter renders under the stock GUI font -- so the render probes
    ' cannot fail for want of an icon font being installed in the test process. That exact
    ' precaution is why PsCheckBox's state assertions are meaningful.
    const GLYPH = "A"

    ' The chrome glyphs get plain letters for the same reason, and it is not cosmetic: the
    ' defaults are Segoe Fluent Icons private-use codepoints, and the test font above has no
    ' glyph at those points -- so the overflow chevron would draw nothing and its ink probe
    ' would fail for a reason that has nothing to do with this control.
    PsToolbar_SetGlyphs( h, "V", "M" )

    dim as long iCmd   = PsToolbar_AddItem( h, TBR_KIND_COMMAND,  GLYPH, "New",  101 )
    dim as long iIcon  = PsToolbar_AddItem( h, TBR_KIND_COMMAND,  GLYPH, "",     102 )
    dim as long iSep   = PsToolbar_AddSeparator( h )
    dim as long iTog   = PsToolbar_AddItem( h, TBR_KIND_TOGGLE,   GLYPH, "Wrap", 103 )
    dim as long iSplit = PsToolbar_AddItem( h, TBR_KIND_SPLIT,    GLYPH, "Run",  104 )
    dim as long iDrop  = PsToolbar_AddItem( h, TBR_KIND_DROPDOWN, GLYPH, "More", 105 )
    dim as long iRight = PsToolbar_AddItem( h, TBR_KIND_COMMAND,  GLYPH, "Help", 106 )
    PsToolbar_SetItemGravity( h, iRight, TBR_GRAVITY_RIGHT )

    ' Wide enough that nothing overflows, so the geometry section measures geometry rather
    ' than the overflow ladder.
    dim as long W = 900, HH = 40
    SetWindowPos( h, 0, 0, 0, W, HH, SWP_NOZORDER or SWP_NOACTIVATE )

    dim as long iw, ih
    PsToolbar_GetIdealSize( h, iw, ih )

    ' ====================================================================================
    ' PART D -- live geometry.
    ' ====================================================================================
    sD = "ideal=" & str(iw) & "x" & str(ih)
    TBRTest_Check( "geometry: ideal size is positive", (iw > 0) andalso (ih > 0), sD )

    dim as long sumCells = 0
    for i as long = 0 to pT->itemCount - 1
        sumCells += pT->items(i).nCellWidth
    next
    sD = "ideal=" & str(iw) & " sum(cells)=" & str(sumCells)
    TBRTest_Check( "geometry: ideal width = sum of every cell width", (iw = sumCells), sD )

    dim as long expectH = 4 + iif( pT->nTextHeight > 16, pT->nTextHeight, 16 ) + 4
    sD = "ideal h=" & str(ih) & " expect=" & str(expectH) & " textH=" & str(pT->nTextHeight)
    TBRTest_Check( "geometry: ideal height = padTop + max(text,icon) + padBottom", (ih = expectH), sD )

    dim as RECT rc0, rc1, rcI, rcT, rcC, rcR
    PsToolbar_GetItemRect( h, iCmd, rc0 )
    sD = TBRTest_RectStr(rc0)
    TBRTest_Check( "geometry: the LEFT run starts at the client left edge", (rc0.left = 0), sD )

    PsToolbar_GetItemRect( h, iIcon, rc1 )
    sD = "item0 right=" & str(rc0.right) & " item1 left=" & str(rc1.left)
    TBRTest_Check( "geometry: cells in the left run are contiguous", (rc0.right = rc1.left), sD )

    PsToolbar_GetItemRect( h, iRight, rcR )
    sD = TBRTest_RectStr(rcR) & " clientW=" & str(W)
    TBRTest_Check( "geometry: a RIGHT-gravity item ends at the client right edge", (rcR.right = W), sD )

    sD = "cellW=" & str(rc0.right - rc0.left) & " nCellWidth=" & str(pT->items(iCmd).nCellWidth)
    TBRTest_Check( "geometry: the placed rect is exactly nCellWidth wide", _
                   ((rc0.right - rc0.left) = pT->items(iCmd).nCellWidth), sD )

    PsToolbar_GetItemIconRect( h, iCmd, rcI )
    sD = "icon=" & TBRTest_RectStr(rcI) & " cell=" & TBRTest_RectStr(rc0)
    dim as boolean bIconIn = (rcI.left >= rc0.left) andalso (rcI.right <= rc0.right)
    TBRTest_Check( "geometry: rcIcon sits inside its cell", bIconIn, sD )

    sD = "iconW=" & str(rcI.right - rcI.left) & " expect=" & str(ICONW)
    TBRTest_Check( "geometry: the icon cell is the DECLARED width, never measured", _
                   ((rcI.right - rcI.left) = ICONW), sD )

    sD = "icon.left=" & str(rcI.left) & " expect=" & str(rc0.left + PADB)
    TBRTest_Check( "geometry: the icon is pinned to the leading padding", (rcI.left = rc0.left + PADB), sD )

    PsToolbar_GetItemTextRect( h, iCmd, rcT )
    sD = "text=" & TBRTest_RectStr(rcT) & " icon.right=" & str(rcI.right) & " gap=" & str(TGAP)
    TBRTest_Check( "geometry: the caption span starts one gap after the icon", _
                   (rcT.left = rcI.right + TGAP), sD )

    sD = "text.right=" & str(rcT.right) & " expect=" & str(rc0.right - PADA)
    TBRTest_Check( "geometry: the caption span runs to the trailing padding", _
                   (rcT.right = rc0.right - PADA), sD )

    ' The SPLIT item: chevron pinned right, caption span stopping short of the divider.
    PsToolbar_GetItemRect( h, iSplit, rc1 )
    PsToolbar_GetItemChevronRect( h, iSplit, rcC )
    PsToolbar_GetItemTextRect( h, iSplit, rcT )
    sD = "chev=" & TBRTest_RectStr(rcC) & " cell.right=" & str(rc1.right)
    TBRTest_Check( "geometry: a SPLIT chevron is pinned to the trailing padding", _
                   (rcC.right = rc1.right - PADA) andalso ((rcC.right - rcC.left) = CHEVW), sD )

    sD = "text.right=" & str(rcT.right) & " chev.left=" & str(rcC.left) & " divider+gap=" & str(DIVT + CHEVG)
    TBRTest_Check( "geometry: the SPLIT caption span stops before the divider AND its gap", _
                   (rcT.right = rcC.left - DIVT - CHEVG), sD )

    ' The DROPDOWN item charges a gap, not a divider.
    dim as RECT rcCD, rcTD, rcD
    PsToolbar_GetItemRect( h, iDrop, rcD )
    PsToolbar_GetItemChevronRect( h, iDrop, rcCD )
    PsToolbar_GetItemTextRect( h, iDrop, rcTD )
    sD = "text.right=" & str(rcTD.right) & " chev.left=" & str(rcCD.left) & " chevGap=" & str(CHEVG)
    TBRTest_Check( "geometry: the DROPDOWN caption span stops one chevron gap short", _
                   (rcTD.right = rcCD.left - CHEVG), sD )

    dim as RECT rcSepRule, rcSepCell
    PsToolbar_GetItemRect( h, iSep, rcSepCell )
    PsToolbar_GetItemIconRect( h, iSep, rcSepRule )
    sD = "rule=" & TBRTest_RectStr(rcSepRule) & " width=" & str(rcSepRule.right - rcSepRule.left)
    TBRTest_Check( "geometry: a separator's rule is nSepWidth wide", _
                   ((rcSepRule.right - rcSepRule.left) = SEPW), sD )

    ' An icon-only item carries no caption span at all -- the rect is EMPTY, not zero-width
    ' at some arbitrary x.
    dim as RECT rcTNone
    PsToolbar_GetItemTextRect( h, iIcon, rcTNone )
    sD = TBRTest_RectStr(rcTNone)
    TBRTest_Check( "geometry: an item with no caption has an EMPTY text rect", _
                   (IsRectEmpty( @rcTNone ) <> 0), sD )

    ' ---- hit-testing round trip over every item, and both zones of the split ------------
    dim as boolean bAllHit = true
    dim as string sMiss
    for i as long = 0 to pT->itemCount - 1
        if pT->items(i).itemKind = TBR_KIND_SEPARATOR then continue for
        dim as RECT rcH
        PsToolbar_GetItemRect( h, i, rcH )
        dim as long xMid = rcH.left + ((rcH.right - rcH.left) \ 2)
        dim as long yMid = rcH.top + ((rcH.bottom - rcH.top) \ 2)
        if PsToolbar_HitTest( h, xMid, yMid ) <> i then
            bAllHit = false
            sMiss &= str(i) & " "
        end if
    next
    sD = "misses: " & iif( len(sMiss) = 0, "none", sMiss )
    TBRTest_Check( "hit-test: the centre of every item returns that item", bAllHit, sD )

    dim as long xSep = rcSepCell.left + 1
    sD = "hit at x=" & str(xSep) & " -> " & str(PsToolbar_HitTest( h, xSep, 10 ))
    TBRTest_Check( "hit-test: a separator is never a target", (PsToolbar_HitTest( h, xSep, 10 ) = -1), sD )

    dim as long nZone
    PsToolbar_GetItemRect( h, iSplit, rc1 )
    PsToolbar_GetItemChevronRect( h, iSplit, rcC )
    dim as long idxZ = PsToolbar_HitTestEx( h, rc1.left + 2, 10, nZone )
    sD = "idx=" & str(idxZ) & " zone=" & str(nZone)
    TBRTest_Check( "hit-test: the SPLIT action zone reports ZONE_ACTION", _
                   (idxZ = iSplit) andalso (nZone = TBR_ZONE_ACTION), sD )

    idxZ = PsToolbar_HitTestEx( h, rcC.left + 2, 10, nZone )
    sD = "idx=" & str(idxZ) & " zone=" & str(nZone)
    TBRTest_Check( "hit-test: the SPLIT chevron zone reports ZONE_CHEVRON", _
                   (idxZ = iSplit) andalso (nZone = TBR_ZONE_CHEVRON), sD )

    ' The chevron zone extends to the cell's right edge, so the trailing padding is a target
    ' rather than dead space.
    idxZ = PsToolbar_HitTestEx( h, rc1.right - 1, 10, nZone )
    sD = "idx=" & str(idxZ) & " zone=" & str(nZone) & " x=" & str(rc1.right - 1)
    TBRTest_Check( "hit-test: the SPLIT chevron zone owns the trailing padding", _
                   (idxZ = iSplit) andalso (nZone = TBR_ZONE_CHEVRON), sD )

    ' A DROPDOWN has ONE zone: all of it is the action.
    idxZ = PsToolbar_HitTestEx( h, rcCD.left + 2, 10, nZone )
    sD = "idx=" & str(idxZ) & " zone=" & str(nZone)
    TBRTest_Check( "hit-test: a DROPDOWN reports ACTION even over its chevron", _
                   (idxZ = iDrop) andalso (nZone = TBR_ZONE_ACTION), sD )

    idxZ = PsToolbar_HitTestEx( h, W - 1, 10, nZone )
    sD = "x=" & str(W-1) & " idx=" & str(idxZ)
    TBRTest_Check( "hit-test: the far right edge finds the right-gravity item", (idxZ = iRight), sD )


    ' ====================================================================================
    ' PART E -- overflow, live.
    ' ====================================================================================
    sD = "overflowCount=" & str(PsToolbar_GetOverflowCount(h)) & " at width " & str(W)
    TBRTest_Check( "overflow live: a wide bar overflows nothing", (PsToolbar_GetOverflowCount(h) = 0), sD )

    dim as RECT rcOv
    TBRTest_Check( "overflow live: no chevron rect when nothing overflows", _
                   (PsToolbar_GetOverflowRect( h, rcOv ) = false), TBRTest_RectStr(rcOv) )

    ' Squeeze it. 150px cannot hold the run, so the ladder must run.
    SetWindowPos( h, 0, 0, 0, 150, HH, SWP_NOZORDER or SWP_NOACTIVATE )
    dim as long nOver = PsToolbar_GetOverflowCount(h)
    sD = "overflowCount=" & str(nOver) & " at width 150"
    TBRTest_Check( "overflow live: a narrow bar sheds items", (nOver > 0), sD )

    dim as boolean bHasOvRect = PsToolbar_GetOverflowRect( h, rcOv )
    TBRTest_Check( "overflow live: the chevron rect appears", _
                   bHasOvRect andalso (IsRectEmpty(@rcOv) = 0), TBRTest_RectStr(rcOv) )

    idxZ = PsToolbar_HitTestEx( h, rcOv.left + 2, 10, nZone )
    sD = "idx=" & str(idxZ) & " zone=" & str(nZone)
    TBRTest_Check( "overflow live: the chevron hit-tests as ZONE_OVERFLOW with idx -1", _
                   (idxZ = -1) andalso (nZone = TBR_ZONE_OVERFLOW), sD )

    ' The right-gravity item is the last thing to go, so it must still be placed here.
    PsToolbar_GetItemRect( h, iRight, rcR )
    sD = "right item rect=" & TBRTest_RectStr(rcR) & " overflowed=" & str(PsToolbar_IsItemOverflowed(h, iRight))
    TBRTest_Check( "overflow live: the RIGHT-gravity item is still on the bar", _
                   (PsToolbar_IsItemOverflowed( h, iRight ) = false) andalso (IsRectEmpty(@rcR) = 0), sD )

    sD = "chev.right=" & str(rcOv.right) & " rightItem.left=" & str(rcR.left)
    TBRTest_Check( "overflow live: the chevron sits inboard of the right run", (rcOv.right = rcR.left), sD )

    ' An overflowed item is not on the strip at all: no rect, and unreachable by hit-testing.
    dim as long idxGone = -1
    for i as long = 0 to pT->itemCount - 1
        if PsToolbar_IsItemOverflowed( h, i ) then
            idxGone = i
            exit for
        end if
    next
    dim as RECT rcGone
    PsToolbar_GetItemRect( h, idxGone, rcGone )
    sD = "idx=" & str(idxGone) & " rect=" & TBRTest_RectStr(rcGone)
    TBRTest_Check( "overflow live: an overflowed item has an EMPTY rect", _
                   (idxGone <> -1) andalso (IsRectEmpty( @rcGone ) <> 0), sD )

    ' ---- the overflow MENU: contents, ids, and state ----------------------------------
    scope
        dim as HWND hM = PsToolbar_GetOverflowMenu(h)
        PsToolbar_BuildOverflowMenu( pT, h )
        dim as long nRows = PsPopupMenu_GetItemCount( hM )
        sD = "rows=" & str(nRows) & " overflowCount=" & str(PsToolbar_GetOverflowCount(h))
        TBRTest_Check( "overflow menu: it has rows once items have been shed", (nRows > 0), sD )

        ' Row ids are (model index + 1). PsPopupMenu addresses items BY ID, so a setter that
        ' succeeds is proof the id exists -- and one that fails is proof it does not.
        dim as boolean bIdsOK = true
        dim as string  sIds
        for i as long = 0 to pT->itemCount - 1
            with pT->items(i)
                if .bOverflowed = false then continue for
                if .bTrimmed then continue for
                if .itemKind = TBR_KIND_SEPARATOR then continue for
                if .itemKind = TBR_KIND_CONTROL then continue for
                if PsPopupMenu_SetItemEnabled( hM, i + 1, .isEnabled ) = false then
                    bIdsOK = false
                    sIds &= str(i+1) & " "
                end if
            end with
        next
        sD = "missing ids: " & iif( len(sIds) = 0, "none", sIds )
        TBRTest_Check( "overflow menu: every row's id is (item index + 1)", bIdsOK, sD )

        ' The negative half: an id that is NOT index+1 must not resolve. Without this the
        ' assertion above would pass against a menu that answered every id.
        dim as boolean bBogus = PsPopupMenu_SetItemEnabled( hM, 9999, true )
        sD = "SetItemEnabled(9999) = " & str(bBogus)
        TBRTest_Check( "overflow menu: a non-existent id does NOT resolve", (bBogus = false), sD )

        ' A disabled item becomes a disabled row.
        PsToolbar_SetEnabled( h, idxGone, false )
        PsToolbar_BuildOverflowMenu( pT, h )
        dim as boolean bRowEnabled = PsPopupMenu_GetItemEnabled( hM, idxGone + 1 )
        sD = "item " & str(idxGone) & " enabled=false -> row enabled=" & str(bRowEnabled)
        TBRTest_Check( "overflow menu: a disabled item becomes a disabled row", (bRowEnabled = false), sD )
        PsToolbar_SetEnabled( h, idxGone, true )
    end scope

    ' Widening again must restore everything -- the split is recomputed from scratch, never
    ' accumulated.
    SetWindowPos( h, 0, 0, 0, W, HH, SWP_NOZORDER or SWP_NOACTIVATE )
    sD = "overflowCount back to " & str(PsToolbar_GetOverflowCount(h))
    TBRTest_Check( "overflow live: widening restores every item", (PsToolbar_GetOverflowCount(h) = 0), sD )


    ' ====================================================================================
    ' PART F -- separator trimming.
    ' ====================================================================================
    scope
        ' The bar's LEFT run currently ends with the DROPDOWN. Append a separator so the
        ' left run's last visible item IS one.
        dim as long iTrail = PsToolbar_AddSeparator( h )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as RECT rcTrail
        PsToolbar_GetItemRect( h, iTrail, rcTrail )
        sD = "trimmed=" & str(pT->items(iTrail).bTrimmed) & " rect=" & TBRTest_RectStr(rcTrail)
        TBRTest_Check( "trim: a TRAILING separator is suppressed", _
                       pT->items(iTrail).bTrimmed andalso (IsRectEmpty(@rcTrail) <> 0), sD )

        sD = "overflowCount=" & str(PsToolbar_GetOverflowCount(h))
        TBRTest_Check( "trim: a trimmed separator does NOT go into the overflow menu", _
                       (PsToolbar_GetOverflowCount(h) = 0), sD )

        ' And a LEADING one, at the very front of the left run.
        PsToolbar_InsertItem( h, 0, TBR_KIND_SEPARATOR, "", "", 0 )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as RECT rcLead, rcFirst
        PsToolbar_GetItemRect( h, 0, rcLead )
        PsToolbar_GetItemRect( h, 1, rcFirst )
        sD = "trimmed=" & str(pT->items(0).bTrimmed) & " first real item left=" & str(rcFirst.left)
        TBRTest_Check( "trim: a LEADING separator is suppressed and costs no space", _
                       pT->items(0).bTrimmed andalso (rcFirst.left = 0), sD )

        PsToolbar_DeleteItem( h, 0 )                  ' the leading separator
        PsToolbar_DeleteItem( h, pT->itemCount - 1 )  ' the trailing one
    end scope


    ' ====================================================================================
    ' PART G -- radio groups. Both edges, loser first, and the no-op on the checked member.
    ' ====================================================================================
    scope
        PsToolbar_SetSelChangeCallback( h, @TBRTest_SelChange )
        PsToolbar_SetClickCallback( h, @TBRTest_Click )

        dim as long g1 = PsToolbar_AddItem( h, TBR_KIND_TOGGLE, GLYPH, "L", 201 )
        dim as long g2 = PsToolbar_AddItem( h, TBR_KIND_TOGGLE, GLYPH, "C", 202 )
        dim as long g3 = PsToolbar_AddItem( h, TBR_KIND_TOGGLE, GLYPH, "R", 203 )
        PsToolbar_SetItemGroupID( h, g1, 7 )
        PsToolbar_SetItemGroupID( h, g2, 7 )
        PsToolbar_SetItemGroupID( h, g3, 7 )

        gTBR_Log = ""
        PsToolbar_SetSelected( h, g1, true )
        sD = "log='" & gTBR_Log & "' (must be empty)"
        TBRTest_Check( "group: SetSelected is SILENT", (len(gTBR_Log) = 0), sD )

        sD = "FindCheckedInGroup(7)=" & str(PsToolbar_FindCheckedInGroup( h, 7 )) & " expect=" & str(g1)
        TBRTest_Check( "group: the checked member is findable", _
                       (PsToolbar_FindCheckedInGroup( h, 7 ) = g1), sD )

        ' Checking a second member silently must still uncheck the first: a setter that left
        ' two members checked would be a worse contract than one that notifies.
        PsToolbar_SetSelected( h, g2, true )
        sD = "g1=" & str(PsToolbar_GetSelected(h,g1)) & " g2=" & str(PsToolbar_GetSelected(h,g2))
        TBRTest_Check( "group: SetSelected still ENFORCES exclusivity", _
                       (PsToolbar_GetSelected(h,g1) = false) andalso PsToolbar_GetSelected(h,g2), sD )

        ' The user path: both edges fire, LOSER FIRST.
        gTBR_Log = ""
        PsToolbar_Toggle( h, g3 )
        dim as string sExpect = "S" & str(g2) & "-;S" & str(g3) & "+;"
        sD = "log='" & gTBR_Log & "' expect='" & sExpect & "'"
        TBRTest_Check( "group: both edges notify, LOSER FIRST", (gTBR_Log = sExpect), sD )

        sD = "g2=" & str(PsToolbar_GetSelected(h,g2)) & " g3=" & str(PsToolbar_GetSelected(h,g3))
        TBRTest_Check( "group: the check actually moved", _
                       (PsToolbar_GetSelected(h,g2) = false) andalso PsToolbar_GetSelected(h,g3), sD )

        ' Clicking the CHECKED member is a silent no-op, exactly as a radio button behaves.
        gTBR_Log = ""
        PsToolbar_Toggle( h, g3 )
        sD = "log='" & gTBR_Log & "' still selected=" & str(PsToolbar_GetSelected(h,g3))
        TBRTest_Check( "group: clicking the CHECKED member is a silent no-op", _
                       (len(gTBR_Log) = 0) andalso PsToolbar_GetSelected(h,g3), sD )

        ' An independent toggle in the same bar is untouched by all of that.
        dim as long gInd = PsToolbar_AddItem( h, TBR_KIND_TOGGLE, GLYPH, "B", 204 )
        gTBR_Log = ""
        PsToolbar_Toggle( h, gInd )
        sD = "log='" & gTBR_Log & "' groupCheck=" & str(PsToolbar_FindCheckedInGroup(h,7))
        TBRTest_Check( "group: an independent toggle notifies once and disturbs no group", _
                       (gTBR_Log = "S" & str(gInd) & "+;") andalso (PsToolbar_FindCheckedInGroup(h,7) = g3), sD )

        gTBR_Log = ""
        PsToolbar_Toggle( h, gInd )
        sD = "log='" & gTBR_Log & "' selected=" & str(PsToolbar_GetSelected(h,gInd))
        TBRTest_Check( "group: an independent toggle also un-latches", _
                       (gTBR_Log = "S" & str(gInd) & "-;") andalso (PsToolbar_GetSelected(h,gInd) = false), sD )

        ' Joining a group while already checked must not leave two members checked.
        dim as long gLate = PsToolbar_AddItem( h, TBR_KIND_TOGGLE, GLYPH, "X", 205 )
        PsToolbar_SetSelected( h, gLate, true )
        gTBR_Log = ""
        PsToolbar_SetItemGroupID( h, gLate, 7 )
        dim as long nChecked = 0
        for i as long = 0 to pT->itemCount - 1
            with pT->items(i)
                if (.nGroupID = 7) andalso .isSelected then nChecked += 1
            end with
        next
        sD = "checked members=" & str(nChecked) & " log='" & gTBR_Log & "'"
        TBRTest_Check( "group: joining a group while checked enforces it, silently", _
                       (nChecked = 1) andalso (len(gTBR_Log) = 0), sD )

        ' PsToolbar_Click on a COMMAND is an ACTION and FIRES (PsButton_Click's precedent).
        gTBR_Log = ""
        PsToolbar_Click( h, iCmd )
        sD = "log='" & gTBR_Log & "' expect='C" & str(iCmd) & "/101;'"
        TBRTest_Check( "PsToolbar_Click is an action and FIRES the click callback", _
                       (gTBR_Log = "C" & str(iCmd) & "/101;"), sD )

        ' ...but a disabled item swallows it.
        PsToolbar_SetEnabled( h, iCmd, false )
        gTBR_Log = ""
        dim as boolean bClickDisabled = PsToolbar_Click( h, iCmd )
        sD = "returned=" & str(bClickDisabled) & " log='" & gTBR_Log & "'"
        TBRTest_Check( "PsToolbar_Click refuses a DISABLED item", _
                       (bClickDisabled = false) andalso (len(gTBR_Log) = 0), sD )
        PsToolbar_SetEnabled( h, iCmd, true )

        ' A separator is not clickable through the programmatic door either.
        TBRTest_Check( "PsToolbar_Click refuses a SEPARATOR", _
                       (PsToolbar_Click( h, iSep ) = false), "Click(separator)" )

        PsToolbar_SetSelChangeCallback( h, 0 )
        PsToolbar_SetClickCallback( h, 0 )
    end scope


    ' ====================================================================================
    ' PART H -- the dynamic item set. CLAUDE.md calls stale stored indices the family's most
    ' repeated bug class, and this control is the first sibling to carry BOTH a dynamic set
    ' and a derived visible set.
    '
    ' NOTE the shape of these cases: four or more items, removing the SECOND. PsOptionButton
    ' recorded an ordering assertion that tested NOTHING because it removed the last-but-one
    ' entry, for which compaction and swap-with-last are indistinguishable.
    ' ====================================================================================
    scope
        PsToolbar_Clear( h )
        TBRTest_Check( "mutate: Clear empties the bar", (PsToolbar_GetCount(h) = 0), "count=" & str(PsToolbar_GetCount(h)) )

        for i as long = 0 to 3
            PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "I" & str(i), 300 + i )
        next
        sD = "count=" & str(PsToolbar_GetCount(h))
        TBRTest_Check( "mutate: four items added", (PsToolbar_GetCount(h) = 4), sD )

        ' Remove the SECOND, and check the tail COMPACTED rather than being swapped.
        PsToolbar_DeleteItem( h, 1 )
        sD = "count=" & str(PsToolbar_GetCount(h)) & _
             " ids=" & str(PsToolbar_GetItemID(h,0)) & "," & str(PsToolbar_GetItemID(h,1)) & "," & str(PsToolbar_GetItemID(h,2))
        dim as boolean bCompacted = (PsToolbar_GetCount(h) = 3) andalso _
                                    (PsToolbar_GetItemID(h,0) = 300) andalso _
                                    (PsToolbar_GetItemID(h,1) = 302) andalso _
                                    (PsToolbar_GetItemID(h,2) = 303)
        TBRTest_Check( "mutate: Delete COMPACTS the tail (author order is layout order)", bCompacted, sD )

        ' ---- stored-index fix-up, all three directions ---------------------------------
        PsToolbar_Clear( h )
        for i as long = 0 to 3
            PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "I" & str(i), 300 + i )
        next

        pT->nLastHotIdx = 1 : pT->nPressedIdx = 1
        PsToolbar_DeleteItem( h, 1 )
        sD = "hot=" & str(pT->nLastHotIdx) & " pressed=" & str(pT->nPressedIdx)
        TBRTest_Check( "mutate: an index pointing AT the removed item is dropped", _
                       (pT->nLastHotIdx = -1) andalso (pT->nPressedIdx = -1), sD )

        PsToolbar_Clear( h )
        for i as long = 0 to 3
            PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "I" & str(i), 300 + i )
        next
        pT->nLastHotIdx = 3 : pT->nPressedIdx = 2 : pT->nDroppedIdx = 3
        PsToolbar_DeleteItem( h, 1 )
        sD = "hot=" & str(pT->nLastHotIdx) & " pressed=" & str(pT->nPressedIdx) & " dropped=" & str(pT->nDroppedIdx)
        TBRTest_Check( "mutate: an index PAST the removed item slides down by one", _
                       (pT->nLastHotIdx = 2) andalso (pT->nPressedIdx = 1) andalso (pT->nDroppedIdx = 2), sD )

        PsToolbar_Clear( h )
        for i as long = 0 to 3
            PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "I" & str(i), 300 + i )
        next
        pT->nLastHotIdx = 0 : pT->nPressedIdx = 0
        PsToolbar_DeleteItem( h, 2 )
        sD = "hot=" & str(pT->nLastHotIdx) & " pressed=" & str(pT->nPressedIdx)
        TBRTest_Check( "mutate: an index BEFORE the removed item is untouched", _
                       (pT->nLastHotIdx = 0) andalso (pT->nPressedIdx = 0), sD )

        ' ---- insert pushes stored indices UP -------------------------------------------
        PsToolbar_Clear( h )
        for i as long = 0 to 3
            PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "I" & str(i), 300 + i )
        next
        pT->nLastHotIdx = 2 : pT->nPressedIdx = 1 : pT->nDroppedIdx = 0
        PsToolbar_InsertItem( h, 1, TBR_KIND_COMMAND, GLYPH, "NEW", 999 )
        sD = "hot=" & str(pT->nLastHotIdx) & " pressed=" & str(pT->nPressedIdx) & " dropped=" & str(pT->nDroppedIdx)
        TBRTest_Check( "mutate: Insert pushes at-or-after indices up, and leaves earlier ones", _
                       (pT->nLastHotIdx = 3) andalso (pT->nPressedIdx = 2) andalso (pT->nDroppedIdx = 0), sD )

        sD = "ids=" & str(PsToolbar_GetItemID(h,0)) & "," & str(PsToolbar_GetItemID(h,1)) & "," & str(PsToolbar_GetItemID(h,2))
        TBRTest_Check( "mutate: Insert puts the new item AT the index given", _
                       (PsToolbar_GetItemID(h,0) = 300) andalso (PsToolbar_GetItemID(h,1) = 999) andalso (PsToolbar_GetItemID(h,2) = 301), sD )

        ' Insert at the end is a legal append; past the end is not.
        dim as long iEnd = PsToolbar_InsertItem( h, PsToolbar_GetCount(h), TBR_KIND_COMMAND, GLYPH, "END", 998 )
        sD = "returned=" & str(iEnd) & " count=" & str(PsToolbar_GetCount(h))
        TBRTest_Check( "mutate: Insert at count appends", (iEnd = PsToolbar_GetCount(h) - 1), sD )

        dim as long iBad = PsToolbar_InsertItem( h, PsToolbar_GetCount(h) + 5, TBR_KIND_COMMAND, GLYPH, "X", 997 )
        sD = "returned=" & str(iBad)
        TBRTest_Check( "mutate: Insert past the end is refused", (iBad = -1), sD )

        TBRTest_Check( "mutate: Delete of an invalid index is refused", _
                       (PsToolbar_DeleteItem( h, 999 ) = false), "DeleteItem(999)" )

        PsToolbar_Clear( h )
        sD = "hot=" & str(pT->nLastHotIdx) & " pressed=" & str(pT->nPressedIdx) & " dropped=" & str(pT->nDroppedIdx)
        TBRTest_Check( "mutate: Clear resets every stored index", _
                       (pT->nLastHotIdx = -1) andalso (pT->nPressedIdx = -1) andalso (pT->nDroppedIdx = -1), sD )
    end scope


    ' ====================================================================================
    ' PART I -- visibility. An invisible item does not merely hide: it stops occupying width.
    ' ====================================================================================
    scope
        PsToolbar_Clear( h )
        dim as long v0 = PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "One",   401 )
        dim as long v1 = PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "Two",   402 )
        dim as long v2 = PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "Three", 403 )
        SetWindowPos( h, 0, 0, 0, W, HH, SWP_NOZORDER or SWP_NOACTIVATE )

        dim as long wAll = PsToolbar_GetIdealWidth(h)
        dim as long wV1  = pT->items(v1).nCellWidth
        PsToolbar_SetVisible( h, v1, false )
        dim as long wHidden = PsToolbar_GetIdealWidth(h)
        sD = "all=" & str(wAll) & " hidden=" & str(wHidden) & " item width=" & str(wV1)
        TBRTest_Check( "visible: hiding an item removes its width from the ideal", _
                       (wHidden = wAll - wV1), sD )

        dim as RECT rcHidden, rcAfter, rcBefore
        PsToolbar_GetItemRect( h, v1, rcHidden )
        PsToolbar_GetItemRect( h, v2, rcAfter )
        PsToolbar_GetItemRect( h, v0, rcBefore )
        sD = "hidden=" & TBRTest_RectStr(rcHidden) & " next.left=" & str(rcAfter.left) & " prev.right=" & str(rcBefore.right)
        TBRTest_Check( "visible: a hidden item has no rect and leaves no gap", _
                       (IsRectEmpty( @rcHidden ) <> 0) andalso (rcAfter.left = rcBefore.right), sD )

        sD = "hit-test over the vacated space -> " & str( PsToolbar_HitTest( h, rcAfter.left + 2, 10 ) )
        TBRTest_Check( "visible: a hidden item is unreachable by hit-testing", _
                       (PsToolbar_HitTest( h, rcAfter.left + 2, 10 ) = v2), sD )

        PsToolbar_SetVisible( h, v1, true )
        sD = "restored width=" & str(PsToolbar_GetIdealWidth(h)) & " expect=" & str(wAll)
        TBRTest_Check( "visible: re-showing restores the width exactly", _
                       (PsToolbar_GetIdealWidth(h) = wAll), sD )
    end scope


    ' ====================================================================================
    ' PART J -- window structure. The extended style here is the OPPOSITE of PsButton's fix,
    ' and is asserted in both directions so a future "consistency" edit turns this red.
    ' ====================================================================================
    scope
        dim as LONG_PTR ex = GetWindowLongPtr( h, GWL_EXSTYLE )
        dim as LONG_PTR st = GetWindowLongPtr( h, GWL_STYLE )

        sD = "exstyle=&h" & hex(ex)
        TBRTest_Check( "window: WS_EX_CONTROLPARENT is present (this control IS a container)", _
                       ((ex and WS_EX_CONTROLPARENT) <> 0), sD )

        ' EXACTLY that one bit, nothing else. An earlier version of this asserted
        ' "WS_EX_WINDOWEDGE is not inherited" and was VACUOUS: the A/B pass showed the
        ' exstyle is &h10000 whether the style is passed explicitly or left to CWindow's
        ' default, because Windows strips WS_EX_WINDOWEDGE from a frameless WS_CHILD by
        ' itself. Sixth occurrence in this family of an assertion named for something it did
        ' not test -- caught here only because the break was actually run.
        sD = "exstyle=&h" & hex(ex) & " expect=&h" & hex(WS_EX_CONTROLPARENT)
        TBRTest_Check( "window: the extended style is EXACTLY WS_EX_CONTROLPARENT", _
                       (ex = WS_EX_CONTROLPARENT), sD )

        ' The flag above is only SAFE because the control takes no tab stop of its own -- a
        ' childless WS_EX_CONTROLPARENT window WITH a tabstop is exactly the PsButton bug.
        sD = "style=&h" & hex(st)
        TBRTest_Check( "window: WS_TABSTOP is NOT set (mouse-only by design)", _
                       ((st and WS_TABSTOP) = 0), sD )
    end scope


    ' ====================================================================================
    ' PART K -- menus and the dropped state.
    ' ====================================================================================
    scope
        PsToolbar_Clear( h )
        dim as long d0 = PsToolbar_AddItem( h, TBR_KIND_DROPDOWN, GLYPH, "Menu", 501 )
        PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "Cmd", 502 )
        SetWindowPos( h, 0, 0, 0, W, HH, SWP_NOZORDER or SWP_NOACTIVATE )

        dim as HWND hM1 = PsToolbar_GetItemMenu( h, d0 )
        dim as HWND hM2 = PsToolbar_GetItemMenu( h, d0 )
        sD = "first<>0: " & str(hM1 <> 0) & "  same handle twice: " & str(hM1 = hM2)
        TBRTest_Check( "menu: GetItemMenu creates once and returns the same handle", _
                       (hM1 <> 0) andalso (hM1 = hM2), sD )

        TBRTest_Check( "menu: nothing is dropped to begin with", _
                       (PsToolbar_IsDroppedDown(h) = false), "IsDroppedDown=false" )

        ' ---- a menu is THEMED FROM THE BAR, not left black -----------------------------
        ' PSPOPUPMENU_COLORS carries no defaults except SeparatorColor, so a menu that is
        ' never given colors renders black on black -- the separator rule being the only
        ' thing visible on it. Read the colors back off the popup itself: asserting that the
        ' derivation function ran would only be testing the test.
        dim as PSPOPUPMENU ptr pPM = GetPopupMenuPointer( hM1 )
        dim as PSTOOLBAR_COLORS tc
        PsToolbar_GetColors( h, @tc )
        if pPM = 0 then
            TBRTest_Check( "menu: a new menu is themed from the bar's palette", false, "no popup pointer" )
        else
            sD = "menu.Back=&h" & hex(pPM->colors.BackColor) & " bar.Back=&h" & hex(tc.BackColor) & _
                 "  menu.Fore=&h" & hex(pPM->colors.ForeColor) & " bar.Fore=&h" & hex(tc.ForeColor)
            TBRTest_Check( "menu: a new menu is themed from the bar's palette", _
                           (pPM->colors.BackColor = tc.BackColor) andalso _
                           (pPM->colors.ForeColor = tc.ForeColor), sD )

            ' The failure mode this guards is specifically BLACK-ON-BLACK, so assert the two
            ' are not merely equal to each other but actually distinguishable.
            sD = "back=&h" & hex(pPM->colors.BackColor) & " fore=&h" & hex(pPM->colors.ForeColor)
            TBRTest_Check( "menu: its background and foreground are not both black", _
                           (pPM->colors.BackColor <> pPM->colors.ForeColor), sD )

            ' Re-theming the BAR re-themes the menu -- a host sets one palette.
            dim as PSTOOLBAR_COLORS tc2 = tc
            tc2.BackColor = BGR(1,2,3)
            PsToolbar_SetColors( h, @tc2 )
            sD = "after SetColors, menu.Back=&h" & hex(pPM->colors.BackColor) & " expect=&h" & hex(BGR(1,2,3))
            TBRTest_Check( "menu: re-theming the bar re-themes its menus", _
                           (pPM->colors.BackColor = BGR(1,2,3)), sD )

            ' ...unless the host CLAIMED them, after which a bar re-theme must not touch them.
            dim as PSPOPUPMENU_COLORS mc
            mc.BackColor = BGR(9,9,9)
            mc.ForeColor = BGR(200,200,200)
            PsToolbar_SetMenuColors( h, @mc )
            dim as PSTOOLBAR_COLORS tc3 = tc
            tc3.BackColor = BGR(4,5,6)
            PsToolbar_SetColors( h, @tc3 )
            sD = "menu.Back=&h" & hex(pPM->colors.BackColor) & " expect=&h" & hex(BGR(9,9,9)) & " (claimed)"
            TBRTest_Check( "menu: a claimed palette survives a later bar re-theme", _
                           (pPM->colors.BackColor = BGR(9,9,9)), sD )

            ' A menu created AFTER the claim must get the claimed colors too, not the black
            ' default -- the case a naive "if claimed then exit" guard would get wrong.
            dim as long dLate = PsToolbar_AddItem( h, TBR_KIND_DROPDOWN, GLYPH, "Late", 599 )
            dim as PSPOPUPMENU ptr pPM2 = GetPopupMenuPointer( PsToolbar_GetItemMenu( h, dLate ) )
            if pPM2 = 0 then
                TBRTest_Check( "menu: a menu created after the claim gets the claimed colors", false, "no pointer" )
            else
                sD = "late menu.Back=&h" & hex(pPM2->colors.BackColor) & " expect=&h" & hex(BGR(9,9,9))
                TBRTest_Check( "menu: a menu created after the claim gets the claimed colors", _
                               (pPM2->colors.BackColor = BGR(9,9,9)), sD )
            end if
            PsToolbar_DeleteItem( h, dLate )

            ' Restore, so the render probes below measure the real palette.
            pT->bMenuColorsCustom = false
            PsToolbar_SetColors( h, @tc )
        end if

        TBRTest_Check( "menu: FilterMessage is a no-op when nothing is open", _
                       (PsToolbar_FilterMessage( h, 0 ) = false), "null MSG returns false" )

        ' A programmatic open still reports the transition -- it is a WINDOW-STATE change,
        ' not a value change. An EMPTY menu must not claim to be open.
        PsToolbar_SetDropDownCallback( h, @TBRTest_DropDown )
        gTBR_DropOpens = 0 : gTBR_DropCloses = 0
        dim as boolean bOpenedEmpty = PsToolbar_DropDown( h, d0 )
        sD = "returned=" & str(bOpenedEmpty) & " opens=" & str(gTBR_DropOpens) & " closes=" & str(gTBR_DropCloses)
        TBRTest_Check( "menu: an EMPTY menu refuses to open but keeps the edges balanced", _
                       (bOpenedEmpty = false) andalso (gTBR_DropOpens = 1) andalso (gTBR_DropCloses = 1), sD )

        TBRTest_Check( "menu: a refused open leaves nothing dropped", _
                       (PsToolbar_IsDroppedDown(h) = false), "IsDroppedDown=false" )

        ' With rows, it opens for real.
        PsPopupMenu_AddItem( hM1, 9001, "Row One" )
        PsPopupMenu_AddItem( hM1, 9002, "Row Two" )
        gTBR_DropOpens = 0 : gTBR_DropCloses = 0
        dim as boolean bOpened = PsToolbar_DropDown( h, d0 )
        sD = "returned=" & str(bOpened) & " dropped=" & str(PsToolbar_GetDroppedItem(h)) & " opens=" & str(gTBR_DropOpens)
        TBRTest_Check( "menu: a populated menu opens and records which item owns it", _
                       bOpened andalso PsToolbar_IsDroppedDown(h) andalso (PsToolbar_GetDroppedItem(h) = d0), sD )

        ' ---- the reopen guard is armed ONLY for the zone that owns the open menu -------
        ' The NARROWNESS is the whole point, and it is the one deliberate departure from
        ' PsComboBox, which arms for any click on itself. On a toolbar that would make
        ' clicking button B while button A's menu is open do nothing at all.
        '
        ' Driven with a synthetic MSG rather than a real click: MSG.pt is in SCREEN
        ' coordinates for a mouse message, which is exactly what the filter has to undo.
        if bOpened then
            dim as MSG   msgT
            dim as POINT ptC
            dim as RECT  rcOwner, rcOther
            PsToolbar_GetItemRect( h, d0, rcOwner )
            PsToolbar_GetItemRect( h, 1, rcOther )

            ' (a) a click on ANOTHER item must NOT arm the guard -- that click is meant to
            '     close this menu and press that button.
            pT->bSuppressNextOpen = false
            ptC.x = rcOther.left + 2 : ptC.y = 5
            ClientToScreen( h, @ptC )
            msgT.hwnd = h : msgT.message = WM_LBUTTONDOWN : msgT.pt = ptC
            PsToolbar_FilterMessage( h, @msgT )
            sD = "guard=" & str(pT->bSuppressNextOpen) & " (click was over item 1)"
            TBRTest_Check( "guard: a click on ANOTHER item does NOT arm the reopen guard", _
                           (pT->bSuppressNextOpen = false), sD )

            ' The filter above may have dismissed the chain (PsPopupMenu returns FALSE on an
            ' outside click on purpose), so re-open before testing the other half.
            PsToolbar_DropDown( h, d0 )
            pT->bSuppressNextOpen = false
            ptC.x = rcOwner.left + 2 : ptC.y = 5
            ClientToScreen( h, @ptC )
            msgT.pt = ptC
            PsToolbar_FilterMessage( h, @msgT )
            sD = "guard=" & str(pT->bSuppressNextOpen) & " (click was over the owning item)"
            TBRTest_Check( "guard: a click on the OWNING item DOES arm the reopen guard", _
                           pT->bSuppressNextOpen, sD )

            ' And the guard is a strict ONE-SHOT: the next down-click consumes it whatever
            ' else happens, so it can never swallow a later, unrelated click.
            PsToolbar_DropDown( h, d0 )
            pT->bSuppressNextOpen = true
            SendMessageW( h, WM_LBUTTONDOWN, 0, 0 )
            sD = "guard after one WM_LBUTTONDOWN=" & str(pT->bSuppressNextOpen)
            TBRTest_Check( "guard: the reopen guard is a strict one-shot", _
                           (pT->bSuppressNextOpen = false), sD )
        end if

        ' CloseUp must fire the closing edge itself: PsPopupMenu's Hide is SILENT, so waiting
        ' for a CPM_NOTIFY_CLOSED that never arrives would leave the host's bookkeeping stuck.
        gTBR_DropCloses = 0
        PsToolbar_CloseUp( h )
        sD = "closes=" & str(gTBR_DropCloses) & " dropped=" & str(PsToolbar_IsDroppedDown(h))
        TBRTest_Check( "menu: CloseUp fires the closing edge and clears the state", _
                       (gTBR_DropCloses = 1) andalso (PsToolbar_IsDroppedDown(h) = false), sD )

        ' CloseUp on an already-closed toolbar must not fire a second time.
        gTBR_DropCloses = 0
        PsToolbar_CloseUp( h )
        sD = "closes=" & str(gTBR_DropCloses)
        TBRTest_Check( "menu: CloseUp on a closed bar is a no-op", (gTBR_DropCloses = 0), sD )

        PsToolbar_SetDropDownCallback( h, 0 )
    end scope


    ' ====================================================================================
    ' PART K2 -- a CONTROL cell's child talking to its parent.
    '
    ' Parenting the child to the TOOLBAR is what makes the cell work, and it is also what
    ' intercepts every notification the child sends. These assert that the interception is
    ' undone rather than merely documented.
    ' ====================================================================================
    scope
        PsToolbar_Clear( h )
        dim as long c0 = PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "Cmd", 701 )

        ' A real child window, parented to the toolbar exactly as a host must do it.
        dim as HWND hKid = CreateWindowExW( 0, "EDIT", "kid", _
                                            WS_CHILD or ES_AUTOHSCROLL, _
                                            0, 0, 0, 0, h, cast(HMENU, 4242), _
                                            GetModuleHandleW(null), null )
        dim as long cCtl = PsToolbar_AddControl( h, hKid, 100, 20, 702 )
        SetWindowPos( h, 0, 0, 0, W, HH, SWP_NOZORDER or SWP_NOACTIVATE )

        sD = "GetItemChild=" & str(cast(integer, PsToolbar_GetItemChild(h, cCtl)) <> 0) & _
             " FindItemByChild=" & str(PsToolbar_FindItemByChild( h, hKid )) & " expect=" & str(cCtl)
        TBRTest_Check( "control cell: the child maps back to its item", _
                       (PsToolbar_GetItemChild( h, cCtl ) = hKid) andalso _
                       (PsToolbar_FindItemByChild( h, hKid ) = cCtl), sD )

        sD = "FindItemByChild(h)=" & str(PsToolbar_FindItemByChild( h, h ))
        TBRTest_Check( "control cell: an unknown window maps to -1", _
                       (PsToolbar_FindItemByChild( h, h ) = -1) andalso _
                       (PsToolbar_FindItemByChild( h, 0 ) = -1), sD )

        PsToolbar_SetControlNotifyCallback( h, @TBRTest_ControlNotify )

        ' EN_SETFOCUS as the child itself would send it: HIWORD(wParam) = code,
        ' LOWORD(wParam) = id, lParam = the child.
        gTBR_NotifyCount = 0 : gTBR_NotifyClaim = false
        dim as WPARAM wp = cast( WPARAM, (cast(ulong, EN_SETFOCUS) shl 16) or 4242 )
        SendMessageW( h, WM_COMMAND, wp, cast( LPARAM, hKid ) )
        sD = "count=" & str(gTBR_NotifyCount) & " idx=" & str(gTBR_NotifyIdx) & _
             " expect idx=" & str(cCtl) & " childMatches=" & str(gTBR_NotifyChild = hKid)
        TBRTest_Check( "control cell: a child's WM_COMMAND reaches the notify callback", _
                       (gTBR_NotifyCount = 1) andalso (gTBR_NotifyIdx = cCtl) andalso _
                       (gTBR_NotifyChild = hKid) andalso (gTBR_NotifyMsg = WM_COMMAND), sD )

        ' WM_CTLCOLOR* is a different message family and must arrive too -- it is the one a
        ' host needs in order to theme its child.
        gTBR_NotifyCount = 0
        SendMessageW( h, WM_CTLCOLOREDIT, 0, cast( LPARAM, hKid ) )
        sD = "count=" & str(gTBR_NotifyCount) & " msg=" & str(gTBR_NotifyMsg)
        TBRTest_Check( "control cell: WM_CTLCOLOR* reaches the callback too", _
                       (gTBR_NotifyCount = 1) andalso (gTBR_NotifyMsg = WM_CTLCOLOREDIT), sD )

        ' A message from a window that is NOT one of our cells must report idx = -1 rather
        ' than a wrong index -- the toolbar is the parent of the tooltip and of nothing else,
        ' but a host can still route anything it likes here.
        gTBR_NotifyCount = 0
        SendMessageW( h, WM_COMMAND, wp, cast( LPARAM, HWND_DESKTOP ) )
        sD = "idx=" & str(gTBR_NotifyIdx)
        TBRTest_Check( "control cell: a message from an unknown window reports idx = -1", _
                       (gTBR_NotifyCount = 1) andalso (gTBR_NotifyIdx = -1), sD )

        ' Claiming the message returns the callback's lResult -- the whole reason the struct
        ' carries one, since WM_CTLCOLOR* is answered with an HBRUSH.
        gTBR_NotifyClaim = true
        dim as LRESULT lr = SendMessageW( h, WM_CTLCOLOREDIT, 0, cast( LPARAM, hKid ) )
        sD = "returned=" & str(lr) & " expect=12345"
        TBRTest_Check( "control cell: a claimed message returns the callback's lResult", _
                       (lr = 12345), sD )
        gTBR_NotifyClaim = false

        ' With NO callback the message must still go somewhere: forwarded to the toolbar's
        ' own parent, which is where it would have gone had the toolbar not been in the way.
        PsToolbar_SetControlNotifyCallback( h, 0 )
        gTBR_NotifyCount = 0
        SendMessageW( h, WM_COMMAND, wp, cast( LPARAM, hKid ) )
        sD = "callback count=" & str(gTBR_NotifyCount) & " (no callback set; must not crash)"
        TBRTest_Check( "control cell: with no callback the message is forwarded, not dropped", _
                       (gTBR_NotifyCount = 0), sD )

        DestroyWindow( hKid )
        PsToolbar_Clear( h )
    end scope


    ' ====================================================================================
    ' PART L -- render probes.
    '
    ' Both drive PsToolbar_RenderAll, the exact path WM_PAINT uses. Every floor below was
    ' MEASURED against this control's healthy and flooded output; none was inherited.
    ' ====================================================================================
    scope
        PsToolbar_Clear( h )
        dim as long p0 = PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "Render", 601 )
        dim as long p1 = PsToolbar_AddItem( h, TBR_KIND_COMMAND, GLYPH, "",       602 )
        SetWindowPos( h, 0, 0, 0, 400, HH, SWP_NOZORDER or SWP_NOACTIVATE )

        dim as long tBar  = PsToolbar_CountRenderedTones( h, TBR_PART_TOOLBAR, 0 )
        dim as long tItem = PsToolbar_CountRenderedTones( h, TBR_PART_ITEM, p0 )
        dim as long tText = PsToolbar_CountRenderedTones( h, TBR_PART_TEXT, p0 )
        dim as long tIcon = PsToolbar_CountRenderedTones( h, TBR_PART_ICON, p0 )

        sD = "bar=" & str(tBar) & " item=" & str(tItem) & " text=" & str(tText) & " icon=" & str(tIcon)
        TBRTest_Check( "probe: the bar renders more than a flat fill", (tBar >= 8), sD )
        TBRTest_Check( "probe: an item's cell renders more than a flat fill", (tItem >= 8), sD )

        ' A frozen or blank caption collapses this to the fill colour alone. This is what
        ' catches a caption that is measured but never drawn -- the exact defect a
        ' "does the state reach the pixels" hash test would MISS.
        TBRTest_Check( "probe: the caption draws ink into its span", (tText >= 4), sD )
        TBRTest_Check( "probe: the glyph draws ink into the icon cell", (tIcon >= 4), sD )

        ' Determinism first: without this, every hash assertion below is noise.
        dim as ulong hA = PsToolbar_HashRenderedPart( h, TBR_PART_ITEM, p0 )
        dim as ulong hB = PsToolbar_HashRenderedPart( h, TBR_PART_ITEM, p0 )
        sD = "h1=&h" & hex(hA) & " h2=&h" & hex(hB)
        TBRTest_Check( "probe: two identical renders hash identically", (hA = hB), sD )

        ' ISOLATED glyph test. The icon cell is DECLARED, not measured, so swapping the glyph
        ' cannot move a single rect -- which is asserted here, and is what makes the hash
        ' difference attributable to the glyph DRAW and to nothing else. PsCheckBox and
        ' PsOptionButton both shipped a version of this assertion that passed for the wrong
        ' reason; the width check below is the fix.
        dim as long cellBefore = pT->items(p1).nCellWidth
        PsToolbar_SetGlyph( h, p1, "A" )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as ulong hGlyphA = PsToolbar_HashRenderedPart( h, TBR_PART_ICON, p1 )
        PsToolbar_SetGlyph( h, p1, "W" )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as ulong hGlyphW = PsToolbar_HashRenderedPart( h, TBR_PART_ICON, p1 )
        dim as long cellAfter = pT->items(p1).nCellWidth

        sD = "before=" & str(cellBefore) & " after=" & str(cellAfter)
        TBRTest_Check( "probe: changing the glyph does NOT change the cell (declared, not measured)", _
                       (cellBefore = cellAfter), sD )
        sD = "A=&h" & hex(hGlyphA) & " W=&h" & hex(hGlyphW)
        TBRTest_Check( "probe: the glyph itself reaches the pixels (isolated: geometry fixed)", _
                       (hGlyphA <> hGlyphW), sD )

        ' State changes must reach the pixels too.
        dim as ulong hIdle = PsToolbar_HashRenderedPart( h, TBR_PART_ITEM, p0 )
        PsToolbar_SetSelected( h, p0, true )
        dim as ulong hSel = PsToolbar_HashRenderedPart( h, TBR_PART_ITEM, p0 )
        sD = "idle=&h" & hex(hIdle) & " selected=&h" & hex(hSel)
        TBRTest_Check( "probe: the selected state reaches the pixels", (hIdle <> hSel), sD )
        PsToolbar_SetSelected( h, p0, false )

        PsToolbar_SetEnabled( h, p0, false )
        dim as ulong hDis = PsToolbar_HashRenderedPart( h, TBR_PART_ITEM, p0 )
        sD = "enabled=&h" & hex(hIdle) & " disabled=&h" & hex(hDis)
        TBRTest_Check( "probe: the disabled state reaches the pixels", (hIdle <> hDis), sD )
        PsToolbar_SetEnabled( h, p0, true )

        ' The overflow button is rendered by the same path, so it is probed by it.
        SetWindowPos( h, 0, 0, 0, 60, HH, SWP_NOZORDER or SWP_NOACTIVATE )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as RECT rcOvP
        if PsToolbar_GetOverflowRect( h, rcOvP ) then
            dim as long tOv = PsToolbar_CountRenderedTones( h, TBR_PART_CHEVRON, -1 )
            sD = "overflow chevron tones=" & str(tOv)
            TBRTest_Check( "probe: the overflow chevron draws ink", (tOv >= 4), sD )
        else
            TBRTest_Check( "probe: the overflow chevron draws ink", false, "no overflow rect at width 60" )
        end if
    end scope


    ' ====================================================================================
    ' PART L2 -- a real image in an icon cell.
    '
    ' The strong claim is the EQUIVALENCE: an image charges the exact same cell a glyph does,
    ' proven by measuring both rather than trusting the cell-width formula. Multi-tone image on
    ' purpose, or the render probe below could not tell it from a wiped cell.
    ' ====================================================================================
    scope
        PsToolbar_Clear( h )
        dim as wstring * 260 wszTmp = "__pstbr_st.png"
        scope
            dim as BITMAPINFO bi
            bi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER )
            bi.bmiHeader.biWidth       = 24
            bi.bmiHeader.biHeight      = -24
            bi.bmiHeader.biPlanes      = 1
            bi.bmiHeader.biBitCount    = 32
            bi.bmiHeader.biCompression = BI_RGB
            dim as ulong ptr pB
            dim as HBITMAP hbm = CreateDIBSection( null, @bi, DIB_RGB_COLORS, cast(any ptr ptr, @pB), null, 0 )
            if hbm andalso pB then
                for yy as long = 0 to 23
                    for xx as long = 0 to 23
                        dim as ulong c = &hFF3399EE
                        if ((xx < 12) xor (yy < 12)) then c = &hFFEE9933
                        pB[yy * 24 + xx] = c
                    next
                next
                dim as PsImage tmp
                tmp.LoadFromBitmap( hbm )
                if tmp.IsValid() then AfxGdipSaveImageToPng( tmp.Image()->m_pImage, wszTmp )
            end if
            if hbm then DeleteObject( hbm )
        end scope

        ' A caption-only item (no icon cell) is the baseline.
        dim as long q0 = PsToolbar_AddItem( h, TBR_KIND_COMMAND, "", "Img", 701 )
        SetWindowPos( h, 0, 0, 0, 400, HH, SWP_NOZORDER or SWP_NOACTIVATE )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as long cellNoIcon = pT->items(q0).nCellWidth

        ' Cell width with a glyph.
        PsToolbar_SetGlyph( h, q0, "A" )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as long cellGlyph = pT->items(q0).nCellWidth

        ' Cell width with an image (which also clears the glyph -- either/or).
        dim as boolean okImg = PsToolbar_SetItemImage( h, q0, wszTmp )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as long cellImg = pT->items(q0).nCellWidth

        TBRTest_Check( "SetItemImage loads the file", okImg, "returned " & str(okImg) )
        sD = "noIcon=" & str(cellNoIcon) & " glyph=" & str(cellGlyph) & " image=" & str(cellImg)
        TBRTest_Check( "an image reserves an icon cell (cell grows)", (cellImg > cellNoIcon), sD )
        TBRTest_Check( "an image charges the SAME cell a glyph does", (cellImg = cellGlyph), sD )

        dim as wstring * 8 wg = PsToolbar_GetGlyph( h, q0 )
        TBRTest_Check( "setting an image clears the item glyph", (len(wg) = 0), "glyph=[" & str(wg) & "]" )

        ' Render: the image reached the icon cell.
        SetWindowPos( h, 0, 0, 0, 400, HH, SWP_NOZORDER or SWP_NOACTIVATE )
        dim as long tImg = PsToolbar_CountRenderedTones( h, TBR_PART_ICON, q0 )
        TBRTest_Check( "the image draws ink into the icon cell", (tImg >= 4), "icon tones=" & str(tImg) )

        ' Removing the image reclaims the cell exactly -- and REPORTS SUCCESS. That second
        ' half is asserted because the return used to be FALSE here, identical to what a
        ' failed load returns, so a caller had no way to tell the two apart. Failure is
        ' asserted alongside it, or "always returns true" would pass just as well.
        dim as boolean bRemoved = PsToolbar_SetItemImage( h, q0, "" )
        TBRTest_Check( "removing an image reports SUCCESS, not the failure code", _
            bRemoved, "empty-path removal returned " & str(bRemoved) )
        dim as boolean bBadLoad = PsToolbar_SetItemImage( h, q0, "Z:\no\such\file.png" )
        TBRTest_Check( "...while a load that cannot decode still reports failure", _
            (bBadLoad = false), "bad path returned " & str(bBadLoad) )
        dim as boolean bBadIdx = PsToolbar_SetItemImage( h, 9999, "" )
        TBRTest_Check( "...and so does a bad index", _
            (bBadIdx = false), "bad index returned " & str(bBadIdx) )
        PsToolbar_GetIdealSize( h, iw, ih )
        dim as long cellRemoved = pT->items(q0).nCellWidth
        TBRTest_Check( "removing the image reclaims the icon cell", (cellRemoved = cellNoIcon), _
                       "now=" & str(cellRemoved) & " base=" & str(cellNoIcon) )

        PsToolbar_Clear( h )
        DeleteFileW( wszTmp )
    end scope


    ' ====================================================================================

    ' ====================================================================================
    ' TOOLTIP BACKEND -- mode, handles, the delays surviving a switch, and the text wiring
    '
    ' A real dwell is what would exercise this end to end and no self-test can produce one,
    ' so what is asserted is the WIRING. The text assertion in particular resolves through
    ' the callback the tip actually holds rather than by calling our own thunk: on the pilot
    ' the direct-call version stayed green with the wiring deleted, which would have shipped
    ' a control that shows no tooltip at all once switched.
    ' ====================================================================================
    scope
        dim as HWND hTipT = PsToolbar_Create( hWndParent, 9807 )
        if hTipT = 0 then
            TBRTest_Check( "tooltip: could not create a test control", false, "Create returned 0" )
        else
            TBRTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsToolbar_GetTooltipMode( hTipT ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsToolbar_GetTooltipMode( hTipT )) )
            TBRTest_Check( "tooltip: a comctl32 window exists, and no PsTooltip does", _
                (PsToolbar_GetTooltipHandle( hTipT ) <> 0) andalso _
                (IsWindow(PsToolbar_GetTooltipHandle( hTipT )) <> 0) andalso _
                (PsToolbar_GetPsTooltipHandle( hTipT ) = 0), _
                "hSys = " & str(cast(integer, PsToolbar_GetTooltipHandle( hTipT ))) )

            PsToolbar_SetHoverTime( hTipT, 777 )
            TBRTest_Check( "tooltip: SetHoverTime reaches the comctl32 tip", _
                (SendMessageW( PsToolbar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "TTM_GETDELAYTIME = " & str(SendMessageW( PsToolbar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )

            dim as HWND hSysBeforeT = PsToolbar_GetTooltipHandle( hTipT )
            ' The switch is made on its own line, NOT inside the assertion call. fbc does not
            ' evaluate call arguments left to right, so a detail string that reads the mode in
            ' the same call reports the mode from BEFORE the switch -- a green assertion with a
            ' trace saying it failed, which is worse than either.
            dim as boolean bSwitchedT = PsToolbar_SetTooltipMode( hTipT, PSTIP_MODE_PS )
            TBRTest_Check( "tooltip: switching to PsTooltip succeeds", bSwitchedT, _
                "mode = " & str(PsToolbar_GetTooltipMode( hTipT )) )
            TBRTest_Check( "tooltip: the outgoing comctl32 window is DESTROYED, not orphaned", _
                (IsWindow(hSysBeforeT) = 0) andalso (PsToolbar_GetTooltipHandle( hTipT ) = 0), _
                "old hSys alive = " & str(IsWindow(hSysBeforeT)) )

            dim as HWND hPsT = PsToolbar_GetPsTooltipHandle( hTipT )
            TBRTest_Check( "tooltip: a PsTooltip window exists and is attached to the control", _
                (hPsT <> 0) andalso (IsWindow(hPsT) <> 0) andalso _
                (PsTooltip_GetAttached( hPsT ) = hTipT), _
                "hPs = " & str(cast(integer, hPsT)) )

            dim as PSTOOLTIP ptr pTipT = GetTooltipPointer( hPsT )
            TBRTest_Check( "tooltip: the hover time set on the OLD backend is honoured by the new one", _
                (pTipT <> 0) andalso (pTipT->nInitialDelay = 777), _
                "PsTooltip initial delay = " & str(iif(pTipT, pTipT->nInitialDelay, -1)) & " ms" )
            TBRTest_Check( "tooltip: the PsTooltip backend is WIRED to the control's text policy", _
                (pTipT <> 0) andalso (pTipT->TooltipCallback <> 0), _
                "callback installed = " & str(iif(pTipT andalso pTipT->TooltipCallback, 1, 0)) )

            PsToolbar_SetTooltipMode( hTipT, PSTIP_MODE_SYSTEM )
            TBRTest_Check( "tooltip: switching back rebuilds the comctl32 tip, hover time and all", _
                (PsToolbar_GetTooltipHandle( hTipT ) <> 0) andalso _
                (PsToolbar_GetPsTooltipHandle( hTipT ) = 0) andalso _
                (SendMessageW( PsToolbar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "initial = " & str(SendMessageW( PsToolbar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )
            TBRTest_Check( "tooltip: an unset delay is left at the backend's own default", _
                (SendMessageW( PsToolbar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 ) > 0), _
                "autopop = " & str(SendMessageW( PsToolbar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 )) & " ms" )

            DestroyWindow( hTipT )
        end if
    end scope

    if environ("PSTOOLBAR_TRACE") = "1" then
        print "--- trace: final layout ---"
        SetWindowPos( h, 0, 0, 0, W, HH, SWP_NOZORDER or SWP_NOACTIVATE )
        PsToolbar_GetIdealSize( h, iw, ih )
        for i as long = 0 to pT->itemCount - 1
            dim as RECT rcTr
            PsToolbar_GetItemRect( h, i, rcTr )
            sD = "  item " & str(i) & " kind=" & str(pT->items(i).itemKind) & _
                 " cellW=" & str(pT->items(i).nCellWidth) & _
                 " rc=" & TBRTest_RectStr(rcTr) & _
                 " ovr=" & str(pT->items(i).bOverflowed) & " trim=" & str(pT->items(i).bTrimmed)
            print sD
        next
    end if

    DestroyWindow( h )
    if hTestFont then DeleteObject( hTestFont )

    print "=== PsToolbar self-test: " & str(gTBR_TestCount) & " checks, " & _
          str(gTBR_FailCount) & " failed ==="
end sub
