'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

' ========================================================================================
' PsToolbar -- a reusable owner-drawn command toolbar.
'
' WHAT IT IS, AND WHY IT IS NOT PsIconPanel
'   PsIconPanel is an icon strip whose two headline guarantees are "nothing is measured"
'   (alone in the family its layout never takes a DC) and "a resize moves x0 and nothing
'   else". A toolbar breaks both: captions must be MEASURED, and overflow IS a per-item
'   resize response. It is also STATIC by contract, which a toolbar with a runtime-varying
'   item set cannot be. So this is a sibling, not an extension -- PsIconPanel keeps its
'   contract, its static pairing with PsSelectBar, and its existing host.
'
' A host that already vendors PsBufferPaint.bi / PsPopupMenu.bi gets ONE copy: #include once
' dedupes by resolved path. Include the matching .inc files in dependency order:
'   PsBufferPaint.inc, PsPopupMenu.inc, PsToolbar.inc
' ========================================================================================

#include once "PsBufferPaint.bi"
#include once "PsPopupMenu.bi"
' An item can show a real image file (.ico/.png/.bmp/.jpg) instead of a Fluent glyph.
' PsImage owns the decode; PsBufferPaint.PaintImage draws it. See PsToolbar_SetItemImage.
#include once "PsImage.bi"
' The tooltip backend switch. The control ships on the SYSTEM (comctl32) backend exactly
' as it always has; a host opts an instance into PsTooltip with PsToolbar_SetTooltipMode.
#include once "PsTipHost.bi"


' Polling timer that guarantees hot-tracking is cleared when the mouse leaves the control.
' WM_MOUSELEAVE (TME_LEAVE) is not reliably delivered on fast exits, so a periodic cursor
' check acts as a safety net. Timer IDs are per-window, so every instance can share this id.
#define IDT_PSTOOLBAR_HOTTRACK    &hCB60
#define PSTOOLBAR_HOTTRACK_MS      100

' Default icon cell. DPI-scaled at Create; the setters take raw pixels thereafter.
#define PSTOOLBAR_DEFAULT_ICONWIDTH    16
#define PSTOOLBAR_DEFAULT_ICONHEIGHT   16

' Default space reserved BEFORE and AFTER each item's content.
#define PSTOOLBAR_DEFAULT_PADBEFORE     6
#define PSTOOLBAR_DEFAULT_PADAFTER      6
#define PSTOOLBAR_DEFAULT_PADTOP        4
#define PSTOOLBAR_DEFAULT_PADBOTTOM     4

' Space between the icon cell and the caption. Charged ONLY when there is a caption.
#define PSTOOLBAR_DEFAULT_TEXTGAP       6

' A separator's rule takes the place of the content in its cell.
#define PSTOOLBAR_DEFAULT_SEPWIDTH      1

' The chevron zone on a SPLIT / DROPDOWN item, and on the overflow button.
#define PSTOOLBAR_DEFAULT_CHEVRONWIDTH  14
#define PSTOOLBAR_DEFAULT_CHEVRONGAP    4
' The hairline between a SPLIT button's action zone and its chevron zone. NOT DPI-scaled --
' a hairline should stay a hairline (PsMenuBar's nSeparatorThickness rule).
#define PSTOOLBAR_DEFAULT_DIVIDERTHICK  1

' Corner rounding of an item's hot / pressed / selected fill. 0 = square.
#define PSTOOLBAR_DEFAULT_CURVATURE     4


' ----------------------------------------------------------------------------------------
' What an item IS. Kind is authored at Add time and never changes.
'
'   COMMAND   - momentary. Draws pressed while held, fires ClickCallback on a matched
'               release, never latches.
'   TOGGLE    - latches. Flips isSelected on a matched click and fires SelChangeCallback.
'               With nGroupID <> 0 it is one of a RADIO GROUP: checking it unchecks its
'               siblings. With nGroupID = 0 it is independent.
'   SEPARATOR - a vertical rule the control draws itself. Not hit-testable: never hot,
'               never pressed, and HitTest returns -1 for it.
'   SPLIT     - TWO hit zones in one cell. The action zone behaves exactly like COMMAND
'               (or TOGGLE, if a group id is set); the chevron zone opens the item's menu.
'   DROPDOWN  - the WHOLE cell opens the item's menu. No action zone.
'   CONTROL   - a host-supplied child HWND occupying a declared-width cell. The toolbar
'               only POSITIONS it: it never paints it, never talks to it, and knows nothing
'               about what it is. See TBR_KIND_CONTROL's obligations under AddControl.
' ----------------------------------------------------------------------------------------
enum PSTOOLBAR_ITEMKIND
    TBR_KIND_COMMAND = 0
    TBR_KIND_TOGGLE
    TBR_KIND_SEPARATOR
    TBR_KIND_SPLIT
    TBR_KIND_DROPDOWN
    TBR_KIND_CONTROL
end enum

' Which end of the bar an item belongs to. LEFT items pack forward from the client left in
' author order; RIGHT items pack against the client right, also in author order, so the LAST
' right-gravity item sits hard against the right edge.
enum PSTOOLBAR_GRAVITY
    TBR_GRAVITY_LEFT = 0
    TBR_GRAVITY_RIGHT
end enum

' What a point landed on. Returned by PsToolbar_HitTestEx alongside the item index.
enum PSTOOLBAR_HITZONE
    TBR_ZONE_NONE = 0
    TBR_ZONE_ACTION      ' the item's main body -- click behaviour depends on its kind
    TBR_ZONE_CHEVRON     ' a SPLIT item's dropdown zone (DROPDOWN reports ACTION for all of it)
    TBR_ZONE_OVERFLOW    ' the overflow chevron button; the item index is -1
end enum

' Addressable sub-rects, for the render probes and for a host reasoning about geometry.
enum PSTOOLBAR_PART
    TBR_PART_TOOLBAR = 0
    TBR_PART_ITEM
    TBR_PART_ICON
    TBR_PART_TEXT
    TBR_PART_CHEVRON
end enum

' Visual state, resolved once by PsToolbar_ResolveMood so every renderer agrees.
enum PSTOOLBAR_MOOD
    TBR_MOOD_IDLE = 0
    TBR_MOOD_HOT
    TBR_MOOD_PRESSED
    TBR_MOOD_DISABLED
end enum


' Colors for the built-in painter. Copied on Set. Defaults are tiko's dark theme, inherited
' from PSICONPANEL_COLORS / PSBUTTON_COLORS -- SEEN AND ACCEPTED against a demo, not MATCHED
' to a reference screenshot (no screenshot exists for this control). A host on a light theme
' sets all nine.
type PSTOOLBAR_COLORS
    BackColor          as COLORREF = BGR(33,37,43)     ' bar background + idle cell fill
    ForeColor          as COLORREF = BGR(215,218,224)  ' idle glyph and caption
    BackColorHot       as COLORREF = BGR(44,49,58)
    ForeColorHot       as COLORREF = BGR(255,255,255)
    BackColorSelect    as COLORREF = BGR(38,79,120)    ' latched toggle, and the press flash
    ForeColorSelect    as COLORREF = BGR(255,255,255)
    ForeColorDisabled  as COLORREF = BGR(110,115,125)
    SeparatorColor     as COLORREF = BGR(70,76,88)
    ' The hairline inside a SPLIT button, between its action zone and its chevron zone. Its
    ' own field rather than reusing SeparatorColor: a separator divides GROUPS and wants to
    ' read as bar furniture, while this divides one button and should track the button.
    SplitDividerColor  as COLORREF = BGR(70,76,88)
end type


' ----------------------------------------------------------------------------------------
' One item. Everything above the DERIVED line is authored; everything below belongs to
' LayoutItems and is never set from outside.
'
' NOTE: "width" is a FreeBASIC reserved word (see C:\dev\Learnings.md) and produces errors
' that never name the real problem -- hence nIconWidth / nCellWidth throughout.
' NOTE: no isHot here. Hover is transient and single-valued, so the control's nLastHotIdx is
' the one source of truth. The same argument does NOT apply to isSelected: selection is
' multi-valued and persistent, so it genuinely lives on the item.
' ----------------------------------------------------------------------------------------
type PSTOOLBAR_ITEM
    itemKind       as long = TBR_KIND_COMMAND
    wszGlyph       as DWSTRING          ' Segoe Fluent Icons codepoint(s); "" = no icon cell
    ' An image drawn in the icon cell INSTEAD OF the glyph. The toolbar OWNS this PsImage and
    ' frees it on removal and on destroy. The slot is glyph OR image: SetItemImage clears
    ' wszGlyph, SetGlyph frees pImage. A valid image charges the icon cell exactly as a glyph.
    pImage         as PsImage ptr
    wszText        as DWSTRING          ' caption; "" = no caption and no text gap charged
    wszTooltip     as DWSTRING          ' "" = ask TooltipCallback, then show nothing
    id             as long = 0          ' host command id, reported by ClickCallback
    itemData       as integer = 0       ' free-form host payload
    isSelected     as boolean = false   ' latched (TOGGLE, and a SPLIT with a group id)
    isEnabled      as boolean = true    ' false = greyed, never hot, swallows clicks
    isVisible      as boolean = true    ' false = skipped by layout, hit-test and overflow
    ' 0 = independent. Non-zero makes this TOGGLE one of a mutually exclusive group; see
    ' PsToolbar_SetItemGroupID for the full contract.
    nGroupID       as long = 0
    nGravity       as long = TBR_GRAVITY_LEFT
    ' SPLIT / DROPDOWN only: the menu this item drops. The TOOLBAR owns it -- it is created
    ' on demand and destroyed with the control. Fill it from DropDownCallback's opening edge.
    hMenu          as HWND = 0
    ' CONTROL only. The toolbar positions hChild and nothing else; the HOST creates it,
    ' owns it, and destroys it.
    hChild         as HWND = 0
    nControlWidth  as long = 0          ' the cell's declared content width, raw pixels
    nControlHeight as long = 0          ' 0 = fill the cell's height
    ' Per-item overrides. The inherit sentinels differ because 0 is a legal padding but not a
    ' legal icon size.
    nPadBefore     as long = -1         ' -1 = inherit the bar's nPadBefore
    nPadAfter      as long = -1         ' -1 = inherit the bar's nPadAfter
    nIconWidth     as long = 0          '  0 = inherit the bar's nIconWidth
    nIconHeight    as long = 0          '  0 = inherit the bar's nIconHeight
    ForeColorItem  as COLORREF = 0      ' idle fore color, when bHasForeColor
    ' A boolean rather than a CLR_NONE sentinel: every COLORREF value is a legal color, and
    ' the flag reads honestly at each use site.
    bHasForeColor  as boolean = false
    ' --- DERIVED by LayoutItems ---------------------------------------------------------
    nTextWidth     as long = 0          ' measured caption width (0 when there is no caption)
    nCellWidth     as long = 0          ' padBefore + content + padAfter
    bOverflowed    as boolean = false   ' not on the strip; listed in the overflow menu
    bTrimmed       as boolean = false   ' a separator suppressed for being leading/trailing
    rc             as RECT              ' the full cell, client coords: the FILL and HIT rect
    rcIcon         as RECT              ' the icon draw rect
    rcText         as RECT              ' THE CAPTION SPAN, NOT THE INK -- see PSTOOLBAR_PAINTINFO
    rcChevron      as RECT              ' SPLIT / DROPDOWN only; EMPTY otherwise
end type


type PSTOOLBAR_PAINTINFO
    hToolbar    as HWND                 ' the control, so the callback can query it
    itemID      as long                 ' item index (model index)
    b           as PsBufferPaint ptr    ' the control's buffer for this repaint (no copy)
    itemKind    as long                 ' TBR_KIND_*
    rc          as RECT                 ' the full cell: fill THIS
    rcIcon      as RECT                 ' the icon rect: draw the glyph in THIS
    ' THE CAPTION SPAN, NOT THE INK. rcText is the whole box left over between the icon cell
    ' and the chevron zone, so DT_END_ELLIPSIS has somewhere to ellipsize into. Do not
    ' assume it is tight around the text (PsButton's rule, and the natural thing to get
    ' wrong).
    rcText      as RECT
    rcChevron   as RECT                 ' EMPTY unless SPLIT / DROPDOWN
    ' --- State: the control decides these, you only render. ---
    nMood       as long                 ' TBR_MOOD_*, from PsToolbar_ResolveMood
    isHot       as boolean              ' the mouse is over this item
    isSelected  as boolean              ' a latched TOGGLE
    isPressed   as boolean              ' a live left press is on this item AND the cursor is
                                        '   still over it (slide off and this goes false
                                        '   without ending the gesture)
    isEnabled   as boolean
    ' TRUE when the pointer is specifically over a SPLIT item's chevron zone, so a renderer
    ' can light the two halves independently. Always FALSE for every other kind.
    isChevronHot as boolean
    isDropped   as boolean              ' this item's menu is currently open
    wszGlyph    as DWSTRING
    wszText     as DWSTRING
    ' Resolved image handle for this item's cell, or NULL. When set, draw this image via
    ' p->b->PaintImage INSTEAD OF the glyph -- a custom painter should honour it, image first.
    pImage      as CGpImage ptr
end type

type PSTOOLBAR_MESSAGEINFO
    hToolbar    as HWND
    uMsg        as UINT
    wParam      as WPARAM
    lParam      as LPARAM
    idx         as long       ' item index under the mouse (-1 if none, or the overflow button)
    nZone       as long       ' TBR_ZONE_*
end type


' ----------------------------------------------------------------------------------------
' Draw one item, INSTEAD OF the built-in painter -- setting this replaces the built-in
' rendering for EVERY item, separators and the overflow button included (the overflow button
' arrives with itemID = -1 and itemKind = TBR_KIND_DROPDOWN). Paint through p->b, using
' p->rc as the cell -- do not touch the screen DC.
'
' Three contracts worth honoring:
'   - Fill p->rc, not p->rcIcon. rc includes the item's padding and is what the control
'     background-filled; leaving part of it unpainted shows the bar background through.
'   - Draw the glyph with the font you handed to PsToolbar_SetGlyphFont and the caption with
'     the one from PsToolbar_SetTextFont. The control MEASURED the caption in the text font;
'     painting it in a different one (a bold variant, say) makes the cell too small and the
'     caption ellipsize. That exact mistake shipped in PsOptionButton's demo -- if you want
'     bold, hand the bold font to SetTextFont so measuring and painting agree.
'   - NEVER stroke a frame with PaintBorderRect: it FILLS before it strokes and will erase
'     everything beneath. Use PaintRoundOutline. This bug has shipped four separate times in
'     this family; PsToolbar_CountRenderedTones exists so you can assert you have not.
' ----------------------------------------------------------------------------------------
type TBR_PaintItemCallbackSub as sub( byval p as PSTOOLBAR_PAINTINFO ptr )

' Observe mouse messages. Return TRUE if you handled it and want the control's default
' handling suppressed, FALSE to let it proceed.
'
' CAUTION: the result is honored for every message EXCEPT WM_LBUTTONUP, where it is IGNORED.
' The control holds mouse capture across a press on an ACTION zone, and the up-message is
' what releases it: a callback that suppressed it would strand capture and route every
' subsequent click to this control (the PsListBox bug recorded in Learnings.md). Suppressing
' WM_LBUTTONDOWN suppresses the press, never the capture bookkeeping.
type TBR_MessageCallbackFunc as function( byval m as PSTOOLBAR_MESSAGEINFO ptr ) as boolean

' Supply the tooltip text for an item, on demand (only when a tip is about to show).
' Consulted only when the item has no tooltip text of its own. Return "" for no tooltip.
' Unlike PsStatusBar there is no automatic fall back to the caption: an icon-only item has
' none, and a captioned item showing its own caption as a tip is noise.
type TBR_TooltipCallbackFunc as function( byval hToolbar as HWND, byval idx as long ) as DWSTRING

' ----------------------------------------------------------------------------------------
' A message a TBR_KIND_CONTROL cell's child sent to ITS PARENT -- which is this toolbar,
' because that is where such a child must be parented. WM_COMMAND (EN_CHANGE, EN_SETFOCUS,
' CBN_SELCHANGE, BN_CLICKED, ...), WM_NOTIFY, and the WM_CTLCOLOR* family all arrive here.
'
' THE CONTROL DOES NOT INTERPRET THE NOTIFICATION CODE, and cannot: EN_SETFOCUS, CBN_SETFOCUS
' and BN_SETFOCUS are different numbers, and a generic cell does not know what class the child
' is. You get the raw message, with the item resolved for you.
'
' Return TRUE if you handled it. If the message needs a return value -- WM_CTLCOLOR* wants an
' HBRUSH -- put it in m->lResult first. Return FALSE and the message is FORWARDED to the
' toolbar's own parent, so a host that already handles these in its form procedure keeps
' working without writing a callback at all.
' ----------------------------------------------------------------------------------------
type PSTOOLBAR_CONTROLNOTIFY
    hToolbar as HWND
    idx      as long       ' the CONTROL item that sent it, or -1 if the sender is not one
    hChild   as HWND       ' the sending child window, or 0 when it cannot be identified
    uMsg     as UINT
    wParam   as WPARAM
    lParam   as LPARAM
    lResult  as LRESULT    ' the value to return when you claim the message
end type

type TBR_ControlNotifyFunc as function( byval m as PSTOOLBAR_CONTROLNOTIFY ptr ) as boolean

' A COMMAND item (or a SPLIT item's action zone) was clicked: a matched press+release on the
' same zone. Fires nothing for toggles (they report through SelChangeCallback) and nothing
' for a cancelled gesture (press, slide off, release).
'
' ALSO fires for a pick from the OVERFLOW menu, which is the point of routing both through
' one commit path: a host handler never has to know whether the item was on the strip.
type TBR_ClickCallbackSub as sub( byval hToolbar as HWND, byval idx as long, byval id as long )

' A TOGGLE item latched or unlatched through USER interaction. Fired AFTER the control's
' state is updated, so PsToolbar_GetSelected( h, idx ) = isSelected.
'
' FOR A RADIO GROUP BOTH EDGES FIRE, LOSER FIRST -- and both state writes land before either
' callback, so a host reading the whole group from inside one handler sees a coherent group
' rather than a moment with two members checked (PsOptionButton's rule).
'
' Programmatic PsToolbar_SetSelected does NOT fire this -- only the user does (the family
' rule, and Win32's TCM_SETCURSEL / TCN_SELCHANGE precedent), which also makes it safe to
' call the setter from inside the handler without recursing.
type TBR_SelChangeCallbackSub as sub( byval hToolbar as HWND, byval idx as long, byval isSelected as boolean )

' An item's dropdown menu opened or closed. idx is -1 for the OVERFLOW menu.
'
' It reports a WINDOW-STATE transition, not a value change, so it fires for PROGRAMMATIC
' opens too. The opening edge (isOpen = TRUE) runs BEFORE A SINGLE ROW IS BUILT for a
' SPLIT/DROPDOWN item's menu, which makes it the just-in-time hook for rebuilding a dynamic
' menu: clear hMenu and refill it from here.
'
' The OVERFLOW menu is the exception -- the control fills that one itself, from the items
' that did not fit, and its opening edge fires after it is built. Add rows to it and they
' will be gone at the next open.
type TBR_DropDownCallbackSub as sub( byval hToolbar as HWND, byval idx as long, byval hMenu as HWND, byval isOpen as boolean )


' ========================================================================================
' PURE FUNCTIONS
'
' The risky arithmetic lives here, out of the message pump, so it is assertable with no
' window and no host (PsProgressBar's ComputeBarLength precedent -- and the reason its two
' vacuous assertions were catchable at all).
' ========================================================================================

' The width of one item's cell.
'
'   SEPARATOR  padBefore + nSepWidth      + padAfter
'   CONTROL    padBefore + nControlWidth  + padAfter
'   otherwise  padBefore + content        + padAfter, where content is
'                  [ iconW ] [ gap + textW ] [ divider + chevronW  (SPLIT)   ]
'                                            [ gap     + chevronW  (DROPDOWN)]
'
' THE GAPS ARE CHARGED ONLY WHEN THERE IS SOMETHING ON BOTH SIDES OF THEM (PsButton's rule).
' So an icon-only item is legitimately narrower than a captioned neighbour, and icon-only,
' text-only and icon+text all fall out of this one formula rather than being three cases.
declare function PsToolbar_ComputeCellWidth( _
            byval nKind as long, _
            byval nIconWidth as long, _
            byval nTextWidth as long, _
            byval nTextGap as long, _
            byval nSepWidth as long, _
            byval nControlWidth as long, _
            byval nChevronWidth as long, _
            byval nChevronGap as long, _
            byval nDividerThick as long, _
            byval nPadBefore as long, _
            byval nPadAfter as long _
            ) as long

' Decide which items leave the strip when the run does not fit.
'
' Every array is indexed 0..nCount-1 over the VISIBLE items only, in author order -- the
' caller pre-filters, so this function never has to know what isVisible means.
'
' THE LADDER, in order:
'   1. Everything fits          -> nothing overflows, bNeedOverflow = FALSE, and the
'                                  overflow button is not shown at all.
'   2. Otherwise reserve nOverflowCellW for the chevron, then evict until the rest fits:
'        a. from the TAIL of the LEFT run   (its last item first), then
'        b. from the FRONT of the RIGHT run (its leftmost item first), so the OUTERMOST
'           right-hand items -- the close / settings end of a real toolbar -- survive
'           longest.
'   3. CONTROL items are NEVER evicted. A child HWND cannot live in a popup menu, so they
'      are pinned by necessity rather than by policy. If only CONTROL items remain and the
'      run still does not fit, the loop stops and the tail clips (PsIconPanel's honest-rects
'      rule -- clipping is the paint pass's job).
declare sub PsToolbar_ComputeOverflowSplit( _
            nCellW() as long, _
            nGravity() as long, _
            nKind() as long, _
            byval nCount as long, _
            byval nClientW as long, _
            byval nOverflowCellW as long, _
            bOverflow() as boolean, _
            byref bNeedOverflow as boolean _
            )

' Resolve one item's visual state. Precedence, decided once so every renderer agrees:
'
'       disabled > pressed > hot > idle
'
' isSelected is deliberately NOT a mood: a latched toggle can also be hot or pressed, and
' collapsing them would lose that. The painter combines mood with isSelected itself.
declare function PsToolbar_ResolveMood( _
            byval isEnabled as boolean, _
            byval isPressed as boolean, _
            byval isHot as boolean _
            ) as long


' ========================================================================================
' The control's per-instance state, stored in the CWindow UserData area and reached through
' the private GetToolbarPointer(hwnd) accessor.
' ========================================================================================
type PSTOOLBAR
    hWin               as HWND
    ' The tooltip, whichever backend it is on. Replaces the old hToolTip + HoverTime
    ' pair; PsToolbar_GetTooltipHandle still answers the comctl32 handle, and 0 while
    ' this instance is on PsTooltip.
    tip          as PSTIPHOST
    wszTooltip         as DWSTRING
    items(any)         as PSTOOLBAR_ITEM
    itemCount          as long = 0
    idc_Toolbar        as long = 0
    HoverTime          as long = 250
    ' --- Transient interaction state. UNLIKE PsIconPanel THE ITEM SET HERE IS DYNAMIC, so
    '     every one of these indices can be invalidated by an Insert / Delete / Clear and
    '     MUST be fixed up there. CLAUDE.md calls stale stored indices the family's most
    '     repeated bug class; PsToolbar_FixupIndices is the single place that pays for it.
    nLastHotIdx        as long = -1       ' item the mouse was last over
    nHotZone           as long = TBR_ZONE_NONE   ' which zone of it
    nPressedIdx        as long = -1       ' item under a live left press, or -1
    nPressedZone       as long = TBR_ZONE_NONE
    nDroppedIdx        as long = -1       ' item whose menu is open; -1 = none or overflow
    bOverflowOpen      as boolean = false ' the OVERFLOW menu specifically is open
    bOverflowHot       as boolean = false ' the pointer is over the overflow button
    hotTimerOn         as boolean = false
    ' --- The one-shot reopen guard. PsPopupMenu's filter dismisses the chain on an outside
    '     click and then returns FALSE ON PURPOSE, so the click still reaches its target --
    '     which for a click on the very chevron that opened the menu would close it and then
    '     instantly reopen it. This flag, armed by PsToolbar_FilterMessage and consumed
    '     unconditionally by the next WM_LBUTTONDOWN, is the guard.
    '
    '     REFINEMENT OVER PsComboBox, DELIBERATE: PsComboBox arms for ANY click on itself
    '     (pMsg->hwnd = hCombo). A toolbar has many items, so arming that broadly would make
    '     clicking button B while button A's menu is open do nothing at all. Here the guard
    '     is armed only when the click hit-tests to the zone that OWNS the open menu; a click
    '     anywhere else closes the menu and presses that item, which is exactly the behaviour
    '     PsPopupMenu's FALSE return exists to permit. ---
    bSuppressNextOpen  as boolean = false
    colors             as PSTOOLBAR_COLORS
    ' Caller-supplied fonts (caller owns them). TWO of them, PsMessageBox's precedent: one
    ' font cannot serve both a Segoe Fluent Icons glyph and a UI caption, and this family
    ' never creates fonts.
    '
    ' NOT named hFont: FreeBASIC is case-insensitive, so a member called hFont would shadow
    ' the TYPE name HFONT inside every member procedure of this type, and "dim as HFONT x"
    ' there fails with a misleading "error 14: Expected identifier, found 'HFONT'".
    ' See C:\dev\Learnings.md.
    hTextFont          as HFONT
    hGlyphFont         as HFONT
    ' --- Layout. Item rects are DERIVED, never set from outside; LayoutItems() owns them.
    '     Layout is lazy: mutators mark it dirty, the next paint (or any rect query) runs it,
    '     which coalesces a burst of mutations into one pass. ---
    nIconWidth         as long = PSTOOLBAR_DEFAULT_ICONWIDTH      ' DPI-scaled at Create
    nIconHeight        as long = PSTOOLBAR_DEFAULT_ICONHEIGHT     ' DPI-scaled at Create
    nPadBefore         as long = PSTOOLBAR_DEFAULT_PADBEFORE      ' DPI-scaled at Create
    nPadAfter          as long = PSTOOLBAR_DEFAULT_PADAFTER       ' DPI-scaled at Create
    nPadTop            as long = PSTOOLBAR_DEFAULT_PADTOP         ' DPI-scaled at Create
    nPadBottom         as long = PSTOOLBAR_DEFAULT_PADBOTTOM      ' DPI-scaled at Create
    nTextGap           as long = PSTOOLBAR_DEFAULT_TEXTGAP        ' DPI-scaled at Create
    nSepWidth          as long = PSTOOLBAR_DEFAULT_SEPWIDTH       ' DPI-scaled at Create
    nChevronWidth      as long = PSTOOLBAR_DEFAULT_CHEVRONWIDTH   ' DPI-scaled at Create
    nChevronGap        as long = PSTOOLBAR_DEFAULT_CHEVRONGAP     ' DPI-scaled at Create
    ' NOT DPI-scaled, either of them: PaintLine and PaintRoundOutline scale the pen they are
    ' handed, so scaling here too would double them above 100% (PsOptionButton's rule).
    nDividerThick      as long = PSTOOLBAR_DEFAULT_DIVIDERTHICK
    nCurvature         as long = PSTOOLBAR_DEFAULT_CURVATURE
    nSepHeight         as long = 0        ' 0 = derive from the icon height
    wszChevron         as DWSTRING        ' the SPLIT / DROPDOWN glyph
    wszOverflow        as DWSTRING        ' the overflow button's glyph
    nTextHeight        as long = 0        ' measured, one height for the whole bar
    nTotalWidth        as long = 0        ' summed cell widths of every VISIBLE item
    nIdealHeight       as long = 0
    bNeedOverflow      as boolean = false
    rcOverflow         as RECT            ' the overflow button's cell; EMPTY when not needed
    hOverflowMenu      as HWND = 0        ' created on demand, destroyed with the control
    ' PSPOPUPMENU_COLORS carries NO defaults except SeparatorColor, so a menu that is never
    ' given colors renders BLACK ON BLACK. Every menu this control creates is therefore themed
    ' from the bar's own palette at creation, and re-themed by PsToolbar_SetColors -- which is
    ' also what makes theming the bar theme its menus for free.
    ' PsToolbar_SetMenuColors sets this flag and the derivation stops.
    bMenuColorsCustom  as boolean = false
    menuColors         as PSPOPUPMENU_COLORS   ' only meaningful when bMenuColorsCustom
    bLayoutDirty       as boolean = true
    PaintItemCallback  as TBR_PaintItemCallbackSub    ' optional; replaces the built-in painter
    MessageCallback    as TBR_MessageCallbackFunc     ' optional
    TooltipCallback    as TBR_TooltipCallbackFunc     ' optional
    ClickCallback      as TBR_ClickCallbackSub        ' optional; COMMAND / SPLIT action
    SelChangeCallback  as TBR_SelChangeCallbackSub    ' optional; TOGGLE, user only
    DropDownCallback   as TBR_DropDownCallbackSub     ' optional
    ControlNotifyCallback as TBR_ControlNotifyFunc     ' optional; CONTROL cells' children

    declare sub      LayoutItems()
    declare function GetCount() as long
    declare function AddItemSlot() as PSTOOLBAR_ITEM ptr
    declare function InsertItemSlot( byval idx as long ) as PSTOOLBAR_ITEM ptr
    declare function RemoveItemAt( byval idx as long ) as boolean
    declare function GetItem( byval idx as long ) as PSTOOLBAR_ITEM ptr
    declare function IsValidItem( byval idx as long ) as boolean
    declare function HitTest( byval x as long, byval y as long ) as long
    declare function HitTestZone( byval x as long, byval y as long, byref nZone as long ) as long
    declare function IsInteractive( byval idx as long ) as boolean
    declare sub      FixupIndices( byval idxRemoved as long )
    declare sub      CancelPress()
    declare sub      Clear()
    declare sub      Refresh()
end type


' ========================================================================================
' PURE FUNCTION BODIES
' ========================================================================================

function PsToolbar_ComputeCellWidth( _
            byval nKind as long, _
            byval nIconWidth as long, _
            byval nTextWidth as long, _
            byval nTextGap as long, _
            byval nSepWidth as long, _
            byval nControlWidth as long, _
            byval nChevronWidth as long, _
            byval nChevronGap as long, _
            byval nDividerThick as long, _
            byval nPadBefore as long, _
            byval nPadAfter as long _
            ) as long

    dim as long core = 0

    select case nKind
    case TBR_KIND_SEPARATOR
        core = nSepWidth

    case TBR_KIND_CONTROL
        core = nControlWidth

    case else
        ' A zero width means "this part is absent", which is how icon-only, text-only and
        ' icon+text all come out of one formula instead of three cases.
        dim as boolean bIcon = (nIconWidth > 0)
        dim as boolean bText = (nTextWidth > 0)

        if bIcon then core += nIconWidth
        if bText then
            ' The gap is charged only when there is something on BOTH sides of it.
            if bIcon then core += nTextGap
            core += nTextWidth
        end if

        dim as boolean bContent = (bIcon orelse bText)
        select case nKind
        case TBR_KIND_SPLIT
            ' The divider separates the action zone from the chevron zone -- so it only
            ' exists when there IS an action zone to separate. The GAP goes on the content
            ' side of it, for the same reason a DROPDOWN charges one: without it the caption
            ' sits hard against the divider and the two zones read as one smudge.
            if bContent then core += nChevronGap + nDividerThick
            core += nChevronWidth
        case TBR_KIND_DROPDOWN
            if bContent then core += nChevronGap
            core += nChevronWidth
        end select
    end select

    return nPadBefore + core + nPadAfter
end function


sub PsToolbar_ComputeOverflowSplit( _
            nCellW() as long, _
            nGravity() as long, _
            nKind() as long, _
            byval nCount as long, _
            byval nClientW as long, _
            byval nOverflowCellW as long, _
            bOverflow() as boolean, _
            byref bNeedOverflow as boolean _
            )

    bNeedOverflow = false
    if nCount <= 0 then exit sub

    dim as long total = 0
    for i as long = 0 to nCount - 1
        bOverflow(i) = false
        total += nCellW(i)
    next

    ' Rung 1: it all fits, so there is no overflow button at all -- not an empty one.
    if total <= nClientW then exit sub

    bNeedOverflow = true
    dim as long avail = nClientW - nOverflowCellW
    if avail < 0 then avail = 0

    do while total > avail
        dim as long cand = -1

        ' (a) the TAIL of the LEFT run: the last left-gravity item still on the strip.
        for i as long = nCount - 1 to 0 step -1
            if bOverflow(i) then continue for
            if nKind(i) = TBR_KIND_CONTROL then continue for
            if nGravity(i) <> TBR_GRAVITY_LEFT then continue for
            cand = i
            exit for
        next

        ' (b) the FRONT of the RIGHT run: its leftmost item, so the outermost right-hand
        '     items survive longest.
        if cand = -1 then
            for i as long = 0 to nCount - 1
                if bOverflow(i) then continue for
                if nKind(i) = TBR_KIND_CONTROL then continue for
                if nGravity(i) <> TBR_GRAVITY_RIGHT then continue for
                cand = i
                exit for
            next
        end if

        ' Only pinned CONTROL cells are left. Stop: the tail clips, and rects stay honest
        ' rather than squeezed (PsIconPanel's rule -- clipping is the paint pass's job, and
        ' the cursor cannot reach past the client edge anyway, so hit-testing stays right).
        if cand = -1 then exit do

        bOverflow(cand) = true
        total -= nCellW(cand)
    loop
end sub


function PsToolbar_ResolveMood( _
            byval isEnabled as boolean, _
            byval isPressed as boolean, _
            byval isHot as boolean _
            ) as long

    if isEnabled = false then return TBR_MOOD_DISABLED
    if isPressed then return TBR_MOOD_PRESSED
    if isHot then return TBR_MOOD_HOT
    return TBR_MOOD_IDLE
end function


' ========================================================================================
' Assign every item its cell, decide what overflows, and place everything.
'
' This is the only place item geometry is produced; everything else (hit-testing, painting,
' invalidation, child positioning) consumes it.
'
' THE THING THAT MAKES THIS CONTROL DIFFERENT FROM PsIconPanel: this layout TAKES A DC AND
' MEASURES. PsIconPanel's header states, correctly for itself, that "nothing is measured --
' alone in the family this layout never takes a DC". Captions make that impossible here, and
' that single fact is most of why PsToolbar is a separate control rather than a wider
' PsIconPanel.
'
' ORDER OF OPERATIONS -- each step depends on the one before it:
'   1. MEASURE every visible caption once, with a deterministic font.
'   2. COMPUTE each cell width and the bar's ideal height. Neither depends on the client, so
'      GetIdealSize works before the control has ever been sized (the chicken-and-egg trap:
'      a host calls it to decide how big to make the bar in the first place).
'   3. SPLIT the run against the client width -- the pure function above.
'   4. TRIM separators that ended up leading or trailing the visible run. This runs AFTER
'      the split so it catches both the author's own trailing separator and one exposed by
'      overflow. Trimming only ever REMOVES width, so it cannot reintroduce overflow; it is
'      deliberately NOT re-run as a fixpoint, so a trimmed separator's width is not
'      reclaimed for another item. Stable and predictable beats optimal here.
'   5. PLACE: the LEFT run forward from the client left, the RIGHT run backward from the
'      client right, and the overflow chevron immediately inboard of the right run -- which
'      is the trailing edge of the flowing area, where a user looks for it.
'   6. DERIVE each cell's sub-rects, and position any CONTROL child.
' ========================================================================================
sub PSTOOLBAR.LayoutItems()
    this.bLayoutDirty  = false
    this.nTotalWidth   = 0
    this.nIdealHeight  = 0
    this.bNeedOverflow = false
    SetRectEmpty( @this.rcOverflow )

    if this.hWin = 0 then exit sub
    if this.itemCount = 0 then exit sub

    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            .bOverflowed = false
            .bTrimmed    = false
            .nTextWidth  = 0
            SetRectEmpty( @.rc )
            SetRectEmpty( @.rcIcon )
            SetRectEmpty( @.rcText )
            SetRectEmpty( @.rcChevron )
        end with
    next

    ' ---- 1. measure -----------------------------------------------------------------
    dim as HDC hDC = GetDC( this.hWin )
    if hDC = 0 then
        this.bLayoutDirty = true          ' couldn't measure; try again next paint
        exit sub
    end if

    ' Measure with a deterministic font: the caller's if set, else the stock GUI font --
    ' never "whatever the DC happens to hold". Stock objects must not be deleted.
    dim as HFONT hFontUse = this.hTextFont
    if hFontUse = 0 then hFontUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )
    dim as HFONT hOldFont = SelectObject( hDC, hFontUse )

    dim as TEXTMETRICW tm
    GetTextMetricsW( hDC, @tm )
    this.nTextHeight = tm.tmHeight

    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            if .isVisible = false then continue for
            if len(.wszText) = 0 then continue for
            dim as SIZE sz
            if GetTextExtentPoint32W( hDC, .wszText.vptr, len( .wszText ), @sz ) then
                .nTextWidth = sz.cx
            end if
        end with
    next

    SelectObject( hDC, hOldFont )
    ReleaseDC( this.hWin, hDC )

    ' ---- 2. cell widths, and the ideal height ---------------------------------------
    ' Resolve every per-item override ONCE, here, so no two passes below can disagree about
    ' what an item's padding or icon size is.
    dim padB( 0 to this.itemCount - 1 ) as long
    dim padA( 0 to this.itemCount - 1 ) as long
    dim iconW( 0 to this.itemCount - 1 ) as long
    dim iconH( 0 to this.itemCount - 1 ) as long

    dim as long contentH = 0
    dim as long totalW   = 0
    dim as boolean bAnyText = false

    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            padB(i)  = iif( .nPadBefore < 0, this.nPadBefore, .nPadBefore )
            padA(i)  = iif( .nPadAfter  < 0, this.nPadAfter,  .nPadAfter )
            iconH(i) = iif( .nIconHeight = 0, this.nIconHeight, .nIconHeight )
            ' An icon cell is charged when the item carries a glyph OR a valid image.
            dim as boolean bHasIcon = (len(.wszGlyph) > 0) orelse _
                                      ((.pImage <> 0) andalso (.pImage->IsValid() <> 0))
            if bHasIcon then
                iconW(i) = iif( .nIconWidth = 0, this.nIconWidth, .nIconWidth )
            else
                iconW(i) = 0
            end if

            .nCellWidth = PsToolbar_ComputeCellWidth( _
                              .itemKind, iconW(i), .nTextWidth, this.nTextGap, _
                              this.nSepWidth, .nControlWidth, this.nChevronWidth, _
                              this.nChevronGap, this.nDividerThick, padB(i), padA(i) )

            if .isVisible = false then continue for
            totalW += .nCellWidth

            ' The tallest thing any visible item wants to show.
            if len(.wszText) then bAnyText = true
            select case .itemKind
            case TBR_KIND_CONTROL
                if .nControlHeight > contentH then contentH = .nControlHeight
            case TBR_KIND_SEPARATOR
                ' contributes nothing: its rule is sized from the band, not the reverse
            case else
                dim as boolean bNeedsGlyphBox = bHasIcon
                if (.itemKind = TBR_KIND_SPLIT) orelse (.itemKind = TBR_KIND_DROPDOWN) then
                    bNeedsGlyphBox = true          ' the chevron occupies the same band
                end if
                if bNeedsGlyphBox then
                    if iconH(i) > contentH then contentH = iconH(i)
                end if
            end select
        end with
    next

    if bAnyText then
        if this.nTextHeight > contentH then contentH = this.nTextHeight
    end if

    this.nTotalWidth  = totalW
    this.nIdealHeight = this.nPadTop + contentH + this.nPadBottom

    ' No geometry yet (created 0x0, sized only once the host shows us). Placement is
    ' impossible, so leave the rects empty and ask to be run again -- but note this bail
    ' comes AFTER nTotalWidth and nIdealHeight are computed, because GetIdealSize is exactly
    ' what a host calls to decide how big to make the bar, and returning 0 until it had
    ' already been sized would be a chicken-and-egg trap.
    dim as RECT rcClient
    GetClientRect( this.hWin, @rcClient )
    dim as long clientW = rcClient.right - rcClient.left
    dim as long clientH = rcClient.bottom - rcClient.top
    if (clientW <= 0) orelse (clientH <= 0) then
        this.bLayoutDirty = true
        exit sub
    end if

    ' ---- 3. the overflow split ------------------------------------------------------
    ' Collapse to the VISIBLE items only, so the pure function never has to know what
    ' isVisible means.
    dim vmap( 0 to this.itemCount - 1 ) as long      ' visible slot -> model index
    dim vW  ( 0 to this.itemCount - 1 ) as long
    dim vG  ( 0 to this.itemCount - 1 ) as long
    dim vK  ( 0 to this.itemCount - 1 ) as long
    dim vOvr( 0 to this.itemCount - 1 ) as boolean
    dim as long vCount = 0

    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            if .isVisible = false then continue for
            vmap(vCount) = i
            vW(vCount)   = .nCellWidth
            vG(vCount)   = .nGravity
            vK(vCount)   = .itemKind
            vCount += 1
        end with
    next
    if vCount = 0 then exit sub

    dim as long nOverflowCellW = this.nPadBefore + this.nChevronWidth + this.nPadAfter
    dim as boolean bNeed = false
    PsToolbar_ComputeOverflowSplit( vW(), vG(), vK(), vCount, clientW, nOverflowCellW, vOvr(), bNeed )
    this.bNeedOverflow = bNeed

    for v as long = 0 to vCount - 1
        this.items( vmap(v) ).bOverflowed = vOvr(v)
    next

    ' ---- 4. trim leading / trailing separators from what is left on the strip --------
    ' Done per RUN, because the two runs have independent leading and trailing edges.
    for g as long = TBR_GRAVITY_LEFT to TBR_GRAVITY_RIGHT
        dim as long nFirst = -1
        dim as long nLast  = -1
        for v as long = 0 to vCount - 1
            dim as long i = vmap(v)
            if this.items(i).bOverflowed then continue for
            if this.items(i).nGravity <> g then continue for
            if nFirst = -1 then nFirst = i
            nLast = i
        next
        if nFirst <> -1 then
            if this.items(nFirst).itemKind = TBR_KIND_SEPARATOR then this.items(nFirst).bTrimmed = true
            if this.items(nLast).itemKind  = TBR_KIND_SEPARATOR then this.items(nLast).bTrimmed  = true
        end if
    next

    ' ---- 5. place the two runs ------------------------------------------------------
    dim as long x = rcClient.left
    for v as long = 0 to vCount - 1
        dim as long i = vmap(v)
        with this.items(i)
            if .bOverflowed orelse .bTrimmed then continue for
            if .nGravity <> TBR_GRAVITY_LEFT then continue for
            SetRect( @.rc, x, rcClient.top, x + .nCellWidth, rcClient.bottom )
            x += .nCellWidth
        end with
    next

    dim as long xr = rcClient.right
    for v as long = vCount - 1 to 0 step -1
        dim as long i = vmap(v)
        with this.items(i)
            if .bOverflowed orelse .bTrimmed then continue for
            if .nGravity <> TBR_GRAVITY_RIGHT then continue for
            xr -= .nCellWidth
            SetRect( @.rc, xr, rcClient.top, xr + .nCellWidth, rcClient.bottom )
        end with
    next

    ' The chevron sits immediately inboard of the right run -- the trailing edge of the
    ' flowing area, not the far edge of the bar, so it never displaces a pinned right item.
    if this.bNeedOverflow then
        SetRect( @this.rcOverflow, xr - nOverflowCellW, rcClient.top, xr, rcClient.bottom )
    end if

    ' ---- 6. sub-rects, and the CONTROL children -------------------------------------
    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            if IsRectEmpty( @.rc ) then
                ' Not on the strip. A CONTROL child that cannot be placed must be hidden --
                ' a child window draws itself and would otherwise sit there orphaned.
                if (.itemKind = TBR_KIND_CONTROL) andalso (.hChild <> 0) then
                    if IsWindow( .hChild ) then ShowWindow( .hChild, SW_HIDE )
                end if
                continue for
            end if

            dim as long cellH = .rc.bottom - .rc.top
            dim as long xc    = .rc.left + padB(i)

            select case .itemKind
            case TBR_KIND_SEPARATOR
                ' The rule stands as tall as the content band unless the host pinned it.
                dim as long sh = this.nSepHeight
                if sh <= 0 then sh = contentH
                if sh > cellH then sh = cellH
                dim as long ys = .rc.top + ((cellH - sh) \ 2)
                SetRect( @.rcIcon, xc, ys, xc + this.nSepWidth, ys + sh )

            case TBR_KIND_CONTROL
                dim as long ch = .nControlHeight
                if (ch <= 0) orelse (ch > cellH) then ch = cellH
                dim as long yc = .rc.top + ((cellH - ch) \ 2)
                if .hChild <> 0 then
                    if IsWindow( .hChild ) then
                        SetWindowPos( .hChild, 0, xc, yc, .nControlWidth, ch, _
                                      SWP_NOZORDER or SWP_NOACTIVATE or SWP_SHOWWINDOW )
                    end if
                end if

            case else
                ' The chevron is PINNED to the padding on the right, and the caption takes
                ' the span left over -- PsButton's and PsComboBox's model. So rcText is the
                ' SPAN, not the ink.
                dim as long xTextRight = .rc.right - padA(i)

                if (.itemKind = TBR_KIND_SPLIT) orelse (.itemKind = TBR_KIND_DROPDOWN) then
                    dim as long xChev = .rc.right - padA(i) - this.nChevronWidth
                    dim as long hChev = iconH(i)
                    if hChev > cellH then hChev = cellH
                    dim as long yChev = .rc.top + ((cellH - hChev) \ 2)
                    SetRect( @.rcChevron, xChev, yChev, xChev + this.nChevronWidth, yChev + hChev )
                    xTextRight = xChev
                    if (iconW(i) > 0) orelse (.nTextWidth > 0) then
                        if .itemKind = TBR_KIND_SPLIT then
                            xTextRight -= (this.nDividerThick + this.nChevronGap)
                        else
                            xTextRight -= this.nChevronGap
                        end if
                    end if
                end if

                if iconW(i) > 0 then
                    dim as long ih = iconH(i)
                    if ih > cellH then ih = cellH
                    dim as long yi = .rc.top + ((cellH - ih) \ 2)
                    SetRect( @.rcIcon, xc, yi, xc + iconW(i), yi + ih )
                    xc += iconW(i)
                    if .nTextWidth > 0 then xc += this.nTextGap
                end if

                if .nTextWidth > 0 then
                    dim as long yt = .rc.top + ((cellH - this.nTextHeight) \ 2)
                    ' Clamped to EMPTY rather than allowed to go negative: a caption span
                    ' squeezed past nothing is a useful failure, an inverted rect is not.
                    if xTextRight < xc then xTextRight = xc
                    SetRect( @.rcText, xc, yt, xTextRight, yt + this.nTextHeight )
                end if
            end select
        end with
    next
end sub


function PSTOOLBAR.GetCount() as long
    return this.itemCount
end function

function PSTOOLBAR.IsValidItem( byval idx as long ) as boolean
    return (idx >= 0) andalso (idx < this.itemCount)
end function

function PSTOOLBAR.GetItem( byval idx as long ) as PSTOOLBAR_ITEM ptr
    if this.IsValidItem(idx) = false then return null
    return @this.items(idx)
end function

' Can this item be hovered, pressed or clicked at all? Separators never can, invisible and
' overflowed items are not on the strip, and a disabled item is deliberately NOT excluded
' here -- it still occupies its cell, and the hover and click paths add that test themselves.
function PSTOOLBAR.IsInteractive( byval idx as long ) as boolean
    if this.IsValidItem(idx) = false then return false
    with this.items(idx)
        if .itemKind = TBR_KIND_SEPARATOR then return false
        if .itemKind = TBR_KIND_CONTROL then return false     ' the child owns its own mouse
        if .isVisible = false then return false
        if .bOverflowed orelse .bTrimmed then return false
    end with
    return true
end function

' Which item contains this client-coordinate point, and which ZONE of it.
'
' The chevron zone of a SPLIT item runs from rcChevron.left to the cell's right edge, so the
' right-hand padding belongs to the chevron rather than being dead space. A DROPDOWN reports
' ACTION for its whole cell: there is no second zone to distinguish.
function PSTOOLBAR.HitTestZone( byval x as long, byval y as long, byref nZone as long ) as long
    nZone = TBR_ZONE_NONE
    dim as POINT pt
    pt.x = x
    pt.y = y

    if this.bNeedOverflow then
        if PtInRect( @this.rcOverflow, pt ) then
            nZone = TBR_ZONE_OVERFLOW
            return -1
        end if
    end if

    for i as long = 0 to this.itemCount - 1
        if this.IsInteractive(i) = false then continue for
        if PtInRect( @this.items(i).rc, pt ) = 0 then continue for
        nZone = TBR_ZONE_ACTION
        if this.items(i).itemKind = TBR_KIND_SPLIT then
            if x >= this.items(i).rcChevron.left then nZone = TBR_ZONE_CHEVRON
        end if
        return i
    next
    return -1
end function

function PSTOOLBAR.HitTest( byval x as long, byval y as long ) as long
    dim as long nZone
    return this.HitTestZone( x, y, nZone )
end function

' Append a fresh (reset) item. Grows the backing store by doubling so building a bar is
' amortized O(1), not O(n^2).
function PSTOOLBAR.AddItemSlot() as PSTOOLBAR_ITEM ptr
    dim as long cap = ubound(this.items) + 1
    if this.itemCount >= cap then
        dim as long newcap = iif( cap = 0, 16, cap * 2 )
        redim preserve this.items( 0 to newcap - 1 )
    end if

    dim as long idx = this.itemCount

    ' Reset the new slot -- this also frees any DWSTRING left in a recycled capacity slot.
    with this.items(idx)
        .itemKind       = TBR_KIND_COMMAND
        .wszGlyph       = ""
        .wszText        = ""
        .wszTooltip     = ""
        .id             = 0
        .itemData       = 0
        .isSelected     = false
        .isEnabled      = true
        .isVisible      = true
        .nGroupID       = 0
        .nGravity       = TBR_GRAVITY_LEFT
        .hMenu          = 0
        .hChild         = 0
        .nControlWidth  = 0
        .nControlHeight = 0
        .nPadBefore     = -1
        .nPadAfter      = -1
        .nIconWidth     = 0
        .nIconHeight    = 0
        .ForeColorItem  = 0
        .bHasForeColor  = false
        .nTextWidth     = 0
        .nCellWidth     = 0
        .bOverflowed    = false
        .bTrimmed       = false
        SetRectEmpty( @.rc )
        SetRectEmpty( @.rcIcon )
        SetRectEmpty( @.rcText )
        SetRectEmpty( @.rcChevron )
    end with

    this.itemCount += 1
    this.Refresh()
    return @this.items(idx)
end function

' Insert a fresh slot at idx, shuffling the tail up. idx = itemCount appends.
'
' THE ITEM SET IS DYNAMIC HERE, unlike PsIconPanel -- so every stored index this control
' keeps has to move with the items. That is what FixupIndices exists for, and CLAUDE.md
' names stale stored indices the family's most repeated bug class.
function PSTOOLBAR.InsertItemSlot( byval idx as long ) as PSTOOLBAR_ITEM ptr
    if idx < 0 then return null
    if idx > this.itemCount then return null
    if idx = this.itemCount then return this.AddItemSlot()

    ' Grow by appending, then rotate the new empty slot down into place.
    if this.AddItemSlot() = null then return null
    for i as long = this.itemCount - 1 to idx + 1 step -1
        this.items(i) = this.items(i - 1)
    next

    with this.items(idx)
        .itemKind       = TBR_KIND_COMMAND
        .wszGlyph       = ""
        .wszText        = ""
        .wszTooltip     = ""
        .id             = 0
        .itemData       = 0
        .isSelected     = false
        .isEnabled      = true
        .isVisible      = true
        .nGroupID       = 0
        .nGravity       = TBR_GRAVITY_LEFT
        .hMenu          = 0
        .hChild         = 0
        .nControlWidth  = 0
        .nControlHeight = 0
        .nPadBefore     = -1
        .nPadAfter      = -1
        .nIconWidth     = 0
        .nIconHeight    = 0
        .ForeColorItem  = 0
        .bHasForeColor  = false
        .nTextWidth     = 0
        .nCellWidth     = 0
        .bOverflowed    = false
        .bTrimmed       = false
        SetRectEmpty( @.rc )
        SetRectEmpty( @.rcIcon )
        SetRectEmpty( @.rcText )
        SetRectEmpty( @.rcChevron )
    end with

    ' An insert at or before a stored index pushes it up by one.
    if this.nLastHotIdx >= idx then this.nLastHotIdx += 1
    if this.nPressedIdx >= idx then this.nPressedIdx += 1
    if this.nDroppedIdx >= idx then this.nDroppedIdx += 1

    this.Refresh()
    return @this.items(idx)
end function

' Remove one item, COMPACTING the tail down -- never swapping with the last entry, because
' author order IS layout order here (PsOptionButton's registry rule, same reasoning).
function PSTOOLBAR.RemoveItemAt( byval idx as long ) as boolean
    if this.IsValidItem(idx) = false then return false

    ' The toolbar owns any menu it created for this item; the HOST owns any child window,
    ' so that is hidden and forgotten, never destroyed.
    with this.items(idx)
        if .hMenu <> 0 then
            if IsWindow( .hMenu ) then DestroyWindow( .hMenu )
            .hMenu = 0
        end if
        if .hChild <> 0 then
            if IsWindow( .hChild ) then ShowWindow( .hChild, SW_HIDE )
        end if
        ' Free the image BEFORE the shift copies items(idx+1) over this slot, or the pointer
        ' leaks. Null it so the copy that follows cannot double-reference it.
        if .pImage <> 0 then delete .pImage : .pImage = 0
    end with

    for i as long = idx to this.itemCount - 2
        this.items(i) = this.items(i + 1)
    next

    ' Release the vacated tail slot's strings rather than leaving a stale copy in the
    ' capacity region, where AddItemSlot's reset would be the only thing freeing it. pImage
    ' is set to 0 (not deleted): after the shift the real pointer lives one slot down, and
    ' this slot holds only a shallow duplicate.
    with this.items( this.itemCount - 1 )
        .wszGlyph   = ""
        .wszText    = ""
        .wszTooltip = ""
        .hMenu      = 0
        .hChild     = 0
        .pImage     = 0
    end with

    this.itemCount -= 1
    this.FixupIndices( idx )
    this.Refresh()
    return true
end function

' The single place the dynamic item set is paid for. Called with the index that was just
' removed: a stored index pointing AT it is dropped, and one pointing PAST it slides down.
sub PSTOOLBAR.FixupIndices( byval idxRemoved as long )
    if this.nLastHotIdx = idxRemoved then
        this.nLastHotIdx = -1
        this.nHotZone    = TBR_ZONE_NONE
    elseif this.nLastHotIdx > idxRemoved then
        this.nLastHotIdx -= 1
    end if

    if this.nPressedIdx = idxRemoved then
        this.nPressedIdx  = -1
        this.nPressedZone = TBR_ZONE_NONE
    elseif this.nPressedIdx > idxRemoved then
        this.nPressedIdx -= 1
    end if

    if this.nDroppedIdx = idxRemoved then
        this.nDroppedIdx = -1
    elseif this.nDroppedIdx > idxRemoved then
        this.nDroppedIdx -= 1
    end if
end sub

' Forget any live press WITHOUT touching the capture. Called from the capture-loss path and
' from SetEnabled when the pressed item is disabled out from under the gesture. Releasing
' capture is the WndProc's job, and only on the up-message or WM_DESTROY -- doing it here
' would let a callback strand or double-release it.
sub PSTOOLBAR.CancelPress()
    this.nPressedIdx  = -1
    this.nPressedZone = TBR_ZONE_NONE
end sub

sub PSTOOLBAR.Clear()
    for i as long = 0 to this.itemCount - 1
        with this.items(i)
            if .hMenu <> 0 then
                if IsWindow( .hMenu ) then DestroyWindow( .hMenu )
                .hMenu = 0
            end if
            if .hChild <> 0 then
                if IsWindow( .hChild ) then ShowWindow( .hChild, SW_HIDE )
                .hChild = 0
            end if
            if .pImage <> 0 then delete .pImage : .pImage = 0
            .wszGlyph   = ""
            .wszText    = ""
            .wszTooltip = ""
        end with
    next
    this.itemCount    = 0
    this.nLastHotIdx  = -1
    this.nHotZone     = TBR_ZONE_NONE
    this.nDroppedIdx  = -1
    this.CancelPress()
    this.Refresh()
end sub

' Mark the layout stale and request a repaint. Every mutator routes through here, which is
' what makes layout lazy: a burst of Add calls costs one layout pass, not N.
sub PSTOOLBAR.Refresh()
    this.bLayoutDirty = true
    ' Repaint WITH background erase: overflow and the two gravities can move the whole run
    ' wholesale, so a vacated region must be cleared.
    if this.hWin then InvalidateRect( this.hWin, NULL, TRUE )
end sub


' ========================================================================================
' PUBLIC API
' ========================================================================================
'
' A COMMAND STRIP THAT MEASURES ITS CAPTIONS AND SHEDS WHAT DOES NOT FIT
'   Items carry an icon, a caption, or both. The control measures the captions, lays the
'   cells out in the order supplied, packs LEFT-gravity items from the left and
'   RIGHT-gravity items against the right, and -- when the total no longer fits -- moves
'   trailing items into an overflow menu behind a chevron.
'
' THE CONTROL HANDLE
'   Every PsToolbar_* function takes the handle returned by PsToolbar_Create().
'
'   The handle is a real HWND on purpose (not an opaque type): callers legitimately need to
'   treat the control as a window, e.g. SetWindowPos() to place and size it.
'
' MOUSE ONLY -- NO FOCUS, NO TAB STOP, NO KEYBOARD
'   The control takes no WS_TABSTOP and never calls SetFocus, so it cannot steal focus from
'   the host's editor. Toolbar commands are normally also reachable through the host's menus
'   and accelerators, and PsToolbar_Click is the door an accelerator uses.
'
'   Its window DOES carry WS_EX_CONTROLPARENT, and that is correct here rather than the
'   PsButton bug: this control genuinely is a container (a TBR_KIND_CONTROL cell holds a
'   host child), and since it takes no tab stop of its own it can never be the leaf the
'   dialog manager skips.
'
' TWO MANDATORY HOST OBLIGATIONS
'   1. PsToolbar_FilterMessage( hToolbar, @uMsg ) in the message pump. Without it a dropdown
'      has no keyboard navigation, never closes on an outside click, and cannot be closed by
'      clicking the chevron that opened it.
'   2. AfxGdipInit / AfxGdipShutdown around the message loop (PsBufferPaint's requirement),
'      and never name an identifier "ok" -- GDI+'s Status enum defines Ok = 0 in namespace
'      AfxNova. The family convention is bOK.
'
' LIFETIME
'   The control frees itself when its window is destroyed, and destroys every dropdown menu
'   it created. Fonts you pass in stay yours. A TBR_KIND_CONTROL child stays yours too: the
'   toolbar positions it and nothing else.
'
' ----------------------------------------------------------------------------------------
' Creation
'   CtrlID becomes the control window's id (GWLP_ID). The control is created zero-sized:
'   size it with PsToolbar_GetIdealSize + SetWindowPos().
' ----------------------------------------------------------------------------------------
declare function PsToolbar_Create( byval hWndParent as HWND, byval CtrlID as long ) as HWND

' ----------------------------------------------------------------------------------------
' Building the bar. Add* / Insert* return the new item's index, or -1.
'
'   THE ITEM SET IS DYNAMIC -- unlike PsIconPanel, items may be inserted, deleted and cleared
'   at any time. The control fixes up its own stored hot / pressed / dropped indices; a HOST
'   holding an index across a mutation must fix up its own.
'
'   AddControl takes a child HWND the HOST created and owns. The toolbar re-parents nothing
'   and destroys nothing: on Delete or Clear the child is simply hidden and forgotten. A
'   CONTROL item is never moved into the overflow menu (a window cannot live in one); if it
'   cannot be placed it is hidden with ShowWindow.
' ----------------------------------------------------------------------------------------
declare function PsToolbar_AddItem( byval hToolbar as HWND, byval itemKind as long, byval Glyph as DWSTRING, byval Text as DWSTRING = "", byval id as long = 0, byval itemData as integer = 0 ) as long
declare function PsToolbar_InsertItem( byval hToolbar as HWND, byval idx as long, byval itemKind as long, byval Glyph as DWSTRING, byval Text as DWSTRING = "", byval id as long = 0, byval itemData as integer = 0 ) as long
declare function PsToolbar_AddSeparator( byval hToolbar as HWND ) as long
declare function PsToolbar_AddControl( byval hToolbar as HWND, byval hChild as HWND, byval nControlWidth as long, byval nControlHeight as long = 0, byval id as long = 0 ) as long
declare function PsToolbar_DeleteItem( byval hToolbar as HWND, byval idx as long ) as boolean
declare sub      PsToolbar_Clear( byval hToolbar as HWND )
declare sub      PsToolbar_Refresh( byval hToolbar as HWND )

' ----------------------------------------------------------------------------------------
' Counts and lookup.
'   FindItemByID returns the FIRST item with that id, or -1. HitTest takes CLIENT
'   coordinates, forces a pending layout, and returns the item there (separators and
'   overflowed items excluded, disabled items included) or -1. HitTestEx additionally
'   reports WHICH ZONE was hit, which is the only way to tell a SPLIT button's action zone
'   from its chevron.
' ----------------------------------------------------------------------------------------
declare function PsToolbar_GetCount( byval hToolbar as HWND ) as long
declare function PsToolbar_IsValidItem( byval hToolbar as HWND, byval idx as long ) as boolean
declare function PsToolbar_FindItemByID( byval hToolbar as HWND, byval id as long ) as long
declare function PsToolbar_HitTest( byval hToolbar as HWND, byval x as long, byval y as long ) as long
declare function PsToolbar_HitTestEx( byval hToolbar as HWND, byval x as long, byval y as long, byref nZone as long ) as long

' ----------------------------------------------------------------------------------------
' Item contents and state.  Set* return FALSE for an invalid item index.
'
'   SetSelected is SILENT (no SelChangeCallback) but DOES enforce a radio group, so it is
'   both how a host restores latched state at startup and safe to call from inside the
'   change handler.
'
'   SetEnabled(false) greys the item, stops it going hot, and makes it swallow clicks;
'   disabling the item under a live press cancels the press. SetVisible(false) removes it
'   from layout, hit-testing and overflow entirely -- it is not merely hidden, it stops
'   occupying width.
'
'   SetItemGroupID makes a TOGGLE one of a mutually exclusive group. Group membership is
'   (this toolbar, this group id): every member lives in one array, so unlike PsOptionButton
'   no cross-window registry is needed. Nothing-checked is a legal state the user cannot
'   reach -- clicking the checked member is a silent no-op, as a radio button should be.
' ----------------------------------------------------------------------------------------
declare function PsToolbar_GetItemKind( byval hToolbar as HWND, byval idx as long ) as long
declare function PsToolbar_GetGlyph( byval hToolbar as HWND, byval idx as long ) as DWSTRING
declare function PsToolbar_SetGlyph( byval hToolbar as HWND, byval idx as long, byval Glyph as DWSTRING ) as boolean
' Load an image file (.ico/.png/.bmp/.jpg) into an item's icon cell, replacing its glyph.
' "" removes whatever image is there. Returns TRUE when the item ends up as you asked -- a
' successful load, OR a successful removal -- and FALSE only for a bad index or a file that
' would not decode. The image fits the item's icon cell, aspect preserved; SPLIT/DROPDOWN
' chevrons and the overflow button stay Fluent glyphs.
declare function PsToolbar_SetItemImage( byval hToolbar as HWND, byval idx as long, byval Path as DWSTRING ) as boolean
declare function PsToolbar_GetText( byval hToolbar as HWND, byval idx as long ) as DWSTRING
declare function PsToolbar_SetText( byval hToolbar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
declare function PsToolbar_GetItemID( byval hToolbar as HWND, byval idx as long ) as long
declare function PsToolbar_SetItemID( byval hToolbar as HWND, byval idx as long, byval id as long ) as boolean
declare function PsToolbar_GetItemData( byval hToolbar as HWND, byval idx as long ) as integer
declare function PsToolbar_SetItemData( byval hToolbar as HWND, byval idx as long, byval itemData as integer ) as boolean
declare function PsToolbar_GetSelected( byval hToolbar as HWND, byval idx as long ) as boolean
declare function PsToolbar_SetSelected( byval hToolbar as HWND, byval idx as long, byval isSelected as boolean ) as boolean
declare function PsToolbar_GetEnabled( byval hToolbar as HWND, byval idx as long ) as boolean
declare function PsToolbar_SetEnabled( byval hToolbar as HWND, byval idx as long, byval isEnabled as boolean ) as boolean
declare function PsToolbar_GetVisible( byval hToolbar as HWND, byval idx as long ) as boolean
declare function PsToolbar_SetVisible( byval hToolbar as HWND, byval idx as long, byval isVisible as boolean ) as boolean
declare function PsToolbar_GetItemGroupID( byval hToolbar as HWND, byval idx as long ) as long
declare function PsToolbar_SetItemGroupID( byval hToolbar as HWND, byval idx as long, byval nGroupID as long ) as boolean
declare function PsToolbar_FindCheckedInGroup( byval hToolbar as HWND, byval nGroupID as long ) as long
declare function PsToolbar_GetItemGravity( byval hToolbar as HWND, byval idx as long ) as long
declare function PsToolbar_SetItemGravity( byval hToolbar as HWND, byval idx as long, byval nGravity as long ) as boolean
declare function PsToolbar_GetItemChild( byval hToolbar as HWND, byval idx as long ) as HWND
' Map a child window back to the CONTROL item holding it, or -1. The reverse of GetItemChild,
' and what a notification handler uses to find out which cell spoke.
declare function PsToolbar_FindItemByChild( byval hToolbar as HWND, byval hChild as HWND ) as long

' An ACTION, not a setter, so it FIRES the click callback -- PsButton_Click's precedent, and
' with no mnemonics and no keyboard this is the only door a host accelerator has. Returns
' FALSE for an invalid, disabled or non-actionable item.
declare function PsToolbar_Click( byval hToolbar as HWND, byval idx as long ) as boolean
' Toggle a TOGGLE item as a user click would: flips it, enforces its radio group, and FIRES
' SelChangeCallback for both edges. Use SetSelected for the silent form.
declare function PsToolbar_Toggle( byval hToolbar as HWND, byval idx as long ) as boolean

' ----------------------------------------------------------------------------------------
' Layout.
'
'   Bar-level setters take RAW PIXELS -- the caller DPI-scales (the family rule; only the
'   Create-time defaults are scaled for you). The per-item setters override them: pass -1
'   for a padding or 0 for an icon dimension to go back to inheriting.
'
'   GetIdealSize returns the width the whole run wants (which is what a bar needs to avoid
'   overflowing) and the height the tallest content needs. The HEIGHT is valid before the
'   control has ever been sized; the WIDTH does not depend on the client either. Rects are
'   computed lazily; every query here forces a pending layout, so results are always current.
'
'   IsItemOverflowed / GetOverflowCount report the current split. Both force a layout, so
'   they are meaningful only once the control has a real client width.
' ----------------------------------------------------------------------------------------
declare sub      PsToolbar_GetIconSize( byval hToolbar as HWND, byref nIconWidth as long, byref nIconHeight as long )
declare sub      PsToolbar_SetIconSize( byval hToolbar as HWND, byval nIconWidth as long, byval nIconHeight as long )
declare sub      PsToolbar_GetPadding( byval hToolbar as HWND, byref nPadBefore as long, byref nPadAfter as long, byref nPadTop as long, byref nPadBottom as long )
declare sub      PsToolbar_SetPadding( byval hToolbar as HWND, byval nPadBefore as long, byval nPadAfter as long, byval nPadTop as long, byval nPadBottom as long )
declare function PsToolbar_GetTextGap( byval hToolbar as HWND ) as long
declare sub      PsToolbar_SetTextGap( byval hToolbar as HWND, byval nTextGap as long )
declare function PsToolbar_GetSeparatorWidth( byval hToolbar as HWND ) as long
declare sub      PsToolbar_SetSeparatorWidth( byval hToolbar as HWND, byval nSepWidth as long )
declare function PsToolbar_GetSeparatorHeight( byval hToolbar as HWND ) as long
declare sub      PsToolbar_SetSeparatorHeight( byval hToolbar as HWND, byval nSepHeight as long )
declare sub      PsToolbar_GetChevronSize( byval hToolbar as HWND, byref nChevronWidth as long, byref nChevronGap as long )
declare sub      PsToolbar_SetChevronSize( byval hToolbar as HWND, byval nChevronWidth as long, byval nChevronGap as long )
declare function PsToolbar_GetDividerThickness( byval hToolbar as HWND ) as long
declare sub      PsToolbar_SetDividerThickness( byval hToolbar as HWND, byval nThickness as long )
declare function PsToolbar_GetCornerCurvature( byval hToolbar as HWND ) as long
declare sub      PsToolbar_SetCornerCurvature( byval hToolbar as HWND, byval nCurvature as long )
declare function PsToolbar_SetItemPadding( byval hToolbar as HWND, byval idx as long, byval nPadBefore as long, byval nPadAfter as long ) as boolean
declare function PsToolbar_SetItemIconSize( byval hToolbar as HWND, byval idx as long, byval nIconWidth as long, byval nIconHeight as long ) as boolean
declare function PsToolbar_SetItemControlSize( byval hToolbar as HWND, byval idx as long, byval nControlWidth as long, byval nControlHeight as long ) as boolean
declare function PsToolbar_GetItemRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function PsToolbar_GetItemIconRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function PsToolbar_GetItemTextRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function PsToolbar_GetItemChevronRect( byval hToolbar as HWND, byval idx as long, byref rc as RECT ) as boolean
declare function PsToolbar_GetOverflowRect( byval hToolbar as HWND, byref rc as RECT ) as boolean
declare sub      PsToolbar_GetIdealSize( byval hToolbar as HWND, byref nWidth as long, byref nHeight as long )
declare function PsToolbar_GetIdealWidth( byval hToolbar as HWND ) as long
declare function PsToolbar_IsItemOverflowed( byval hToolbar as HWND, byval idx as long ) as boolean
declare function PsToolbar_GetOverflowCount( byval hToolbar as HWND ) as long

' ----------------------------------------------------------------------------------------
' Appearance.  Fonts are borrowed, never owned: keep them alive and destroy them yourself.
'
'   TWO fonts. The TEXT font is a LAYOUT input -- changing it re-measures every caption and
'   moves everything. The GLYPH font is a PAINT-time input only: the icon cell is DECLARED
'   (SetIconSize), never measured, so a glyph font too large for its cell simply clips.
'
'   SetColors copies the whole struct. GetColors fills one out, so the read-modify-write
'   "change one color" idiom is Get, assign, Set.
'
'   SetItemForeColor overrides the IDLE fore color of one item (a red record dot, a green run
'   arrow); hot, pressed, selected and disabled keep the bar's colors, so a hover always
'   speaks the bar's language.
' ----------------------------------------------------------------------------------------
declare sub      PsToolbar_GetColors( byval hToolbar as HWND, byval pColors as PSTOOLBAR_COLORS ptr )
declare sub      PsToolbar_SetColors( byval hToolbar as HWND, byval pColors as PSTOOLBAR_COLORS ptr )
declare function PsToolbar_SetItemForeColor( byval hToolbar as HWND, byval idx as long, byval clr as COLORREF ) as boolean
declare function PsToolbar_ClearItemForeColor( byval hToolbar as HWND, byval idx as long ) as boolean
declare function PsToolbar_GetTextFont( byval hToolbar as HWND ) as HFONT
declare function PsToolbar_SetTextFont( byval hToolbar as HWND, byval hTextFont as HFONT ) as boolean
declare function PsToolbar_GetGlyphFont( byval hToolbar as HWND ) as HFONT
declare function PsToolbar_SetGlyphFont( byval hToolbar as HWND, byval hGlyphFont as HFONT ) as boolean
declare sub      PsToolbar_GetGlyphs( byval hToolbar as HWND, byref wszChevron as DWSTRING, byref wszOverflow as DWSTRING )
declare sub      PsToolbar_SetGlyphs( byval hToolbar as HWND, byval wszChevron as DWSTRING, byval wszOverflow as DWSTRING )

' ----------------------------------------------------------------------------------------
' Dropdown menus.
'
'   GetItemMenu creates the item's PsPopupMenu on first call and returns it -- the TOOLBAR
'   owns it and destroys it with the control. Theme it and fill it like any PsPopupMenu; fill
'   it from DropDownCallback's opening edge if its contents are dynamic.
'
'   GetOverflowMenu returns the overflow menu, also toolbar-owned, but ITS CONTENTS ARE
'   REBUILT ON EVERY OPEN from the items that did not fit -- use it to theme, never to add
'   rows. Its row ids are (item index + 1), so a host command id never enters its command
'   stream (PsComboBox's rule).
'
'   DropDown / CloseUp are programmatic and still fire DropDownCallback -- it reports a
'   window-state transition, not a value change.
'
'   ONE LIVE CHAIN: PsPopupMenu allows only one open chain at a time, so opening menu B
'   closes menu A. That is a documented residual, not a bug to work around.
' ----------------------------------------------------------------------------------------
declare function PsToolbar_GetItemMenu( byval hToolbar as HWND, byval idx as long ) as HWND
declare function PsToolbar_GetOverflowMenu( byval hToolbar as HWND ) as HWND
' Claims the colors of EVERY menu this toolbar owns, present and future: from here on
' PsToolbar_SetColors will not re-derive them, so an explicit choice is never overwritten by a
' later theme change. Without this call the menus follow the bar's own palette.
declare sub      PsToolbar_SetMenuColors( byval hToolbar as HWND, byval pColors as PSPOPUPMENU_COLORS ptr )
declare function PsToolbar_IsDroppedDown( byval hToolbar as HWND ) as boolean
declare function PsToolbar_GetDroppedItem( byval hToolbar as HWND ) as long
declare function PsToolbar_DropDown( byval hToolbar as HWND, byval idx as long ) as boolean
declare sub      PsToolbar_CloseUp( byval hToolbar as HWND )

' ----------------------------------------------------------------------------------------
' Tooltips.  Per-item text wins; the callback is consulted only for items with none; an item
' with neither shows no tip.
' ----------------------------------------------------------------------------------------
declare function PsToolbar_GetTooltipText( byval hToolbar as HWND, byval idx as long ) as DWSTRING
declare function PsToolbar_SetTooltipText( byval hToolbar as HWND, byval idx as long, byval Text as DWSTRING ) as boolean
declare function PsToolbar_GetTooltipHandle( byval hToolbar as HWND ) as HWND
declare sub      PsToolbar_SetHoverTime( byval hToolbar as HWND, byval milliseconds as long )
declare function PsToolbar_SetTooltipMode( byval hToolbar as HWND, byval nMode as long ) as boolean
declare function PsToolbar_GetTooltipMode( byval hToolbar as HWND ) as long
' The PsTooltip window, or 0 on the system backend. The door to PsTooltip's own
' SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph -- deliberately not mirrored
' here, since thirteen controls x twenty setters is 260 wrappers to keep in step.
declare function PsToolbar_GetPsTooltipHandle( byval hToolbar as HWND ) as HWND
' Honoured by BOTH backends. A delay never set keeps the backend's own derivation from
' the system double-click time.
declare sub      PsToolbar_SetAutoPopTime( byval hToolbar as HWND, byval milliseconds as long )
declare sub      PsToolbar_SetReshowTime( byval hToolbar as HWND, byval milliseconds as long )

' ----------------------------------------------------------------------------------------
' Callbacks.  See the type declarations above for each signature and contract.
' ----------------------------------------------------------------------------------------
declare sub      PsToolbar_SetPaintItemCallback( byval hToolbar as HWND, byval usersub as TBR_PaintItemCallbackSub )
declare sub      PsToolbar_SetMessageCallback( byval hToolbar as HWND, byval userfunc as TBR_MessageCallbackFunc )
declare sub      PsToolbar_SetTooltipCallback( byval hToolbar as HWND, byval userfunc as TBR_TooltipCallbackFunc )
declare sub      PsToolbar_SetClickCallback( byval hToolbar as HWND, byval usersub as TBR_ClickCallbackSub )
declare sub      PsToolbar_SetSelChangeCallback( byval hToolbar as HWND, byval usersub as TBR_SelChangeCallbackSub )
declare sub      PsToolbar_SetDropDownCallback( byval hToolbar as HWND, byval usersub as TBR_DropDownCallbackSub )
declare sub      PsToolbar_SetControlNotifyCallback( byval hToolbar as HWND, byval userfunc as TBR_ControlNotifyFunc )

' ----------------------------------------------------------------------------------------
' THE message-pump hook. MANDATORY -- see the host obligations above.
'
'   Returns FALSE immediately when no menu of this toolbar's is open, so calling it for each
'   toolbar in a host's pump costs almost nothing.
' ----------------------------------------------------------------------------------------
declare function PsToolbar_FilterMessage( byval hToolbar as HWND, byval pMsg as MSG ptr ) as boolean

' ----------------------------------------------------------------------------------------
' Render probes. PUBLIC ON PURPOSE, PsButton's and PsCheckBox's precedent: a host that
' replaces the painter should be able to assert that its callback is not flooding the
' control and that a state change actually reaches the pixels.
'
'   CountRenderedTones renders the whole bar offscreen and counts DISTINCT COLOURS inside
'   one part rect. A frame stroked with PaintBorderRect (which fills before it strokes)
'   collapses this to a handful.
'
'   HashRenderedPart is an FNV-1a over the same rect. Compare two states; equal hashes mean
'   the change never reached the pixels.
'
'   DO NOT COPY A THRESHOLD FROM A SIBLING. PsOptionButton recorded PsButton's "> 1" test
'   PASSING on a completely destroyed render, because the healthy number depends entirely on
'   what else is inside the part rect. Measure your own healthy and flooded values.
' ----------------------------------------------------------------------------------------
declare function PsToolbar_CountRenderedTones( byval hToolbar as HWND, byval nPart as long, byval idx as long ) as long
declare function PsToolbar_HashRenderedPart( byval hToolbar as HWND, byval nPart as long, byval idx as long ) as ulong

' ----------------------------------------------------------------------------------------
' Env-gated self-test. Runs only when PSTOOLBAR_SELFTEST=1; call it from the host after the
' control exists. PSTOOLBAR_TRACE=1 additionally dumps live geometry.
' ----------------------------------------------------------------------------------------
declare sub PsToolbar_RunSelfTest( byval hWndParent as HWND )
