'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsOptionButton.inc  --  owner-drawn option button / radio button (ring + dot + caption)
''

#include once "PsOptionButton.bi"
' The tooltip backend switch, pulled in here so a host needs no extra include line.
#include once "PsTipHost.inc"


' Forward declarations: the content setters re-apply auto-size, and the group engine is used by
' the WndProc well above where it is defined.
declare sub PsOptionButton_ApplyAutoSize( byval pOpt as PSOPTIONBUTTON ptr )
declare sub PsOptionButton_SyncTabStop( byval hParent as HWND, byval nGroupID as long )


' ========================================================================================
' THE REGISTRY
'
' Every live PsOptionButton is in this list, added by PsOptionButton_Create and removed by
' WM_NCDESTROY. Group membership is a scan of it filtered on (hParent, nGroupID).
'
' WHY A REGISTRY AND NOT A SIBLING WALK. Two facts about AfxNova make the obvious approach
' impossible rather than merely inelegant, and both were measured rather than assumed:
'
'   - CWindow.Create auto-registers a PER-INSTANCE window class named "FBWindowClass:N", N being
'     a process-global counter (AfxNova\src\CWindow.inc). Two PsOptionButtons therefore have
'     DIFFERENT class names, and the name is not stable across runs. There is no class constant
'     to compare a sibling against, and no Ps* control in this family contains one.
'   - AfxCWindowPtr is an unchecked GetWindowLongPtrW(hwnd, 0). On a window that is not a
'     CWindow it returns whatever that class put in slot 0. Probing an unknown sibling with it
'     is not a null return -- it is a dereference of someone else's memory.
'
' So the control only ever touches HWNDs it created itself, and this is the list of them.
'
' ORDER IS CREATION ORDER AND MUST STAY THAT WAY: it is what the arrow keys navigate along.
' PsOptionButton_RegistryRemove therefore COMPACTS the array rather than doing the cheaper
' swap-with-last, which would silently reorder a group every time a member was destroyed.
'
' Linear scans throughout, deliberately. A group is a handful of buttons and a form holds dozens;
' indexing this would be optimising the wrong thing and would add a second structure to keep in
' step with the first.
'
' SINGLE-THREADED, like every control in this family: touch it from the UI thread only.
' ========================================================================================
dim shared gOptRegistry() as PSOPTIONBUTTON ptr
dim shared gOptRegistryCount as long = 0
dim shared gOptRegistryCap   as long = 0


private sub PsOptionButton_RegistryAdd( byval pOpt as PSOPTIONBUTTON ptr )
    if pOpt = 0 then exit sub
    if gOptRegistryCount >= gOptRegistryCap then
        dim as long nNewCap = gOptRegistryCap + 16
        redim preserve gOptRegistry( 0 to nNewCap - 1 )
        gOptRegistryCap = nNewCap
    end if
    gOptRegistry( gOptRegistryCount ) = pOpt
    gOptRegistryCount += 1
end sub


' Remove and CLOSE THE GAP. See the ordering note above -- this is not a swap-with-last.
private sub PsOptionButton_RegistryRemove( byval pOpt as PSOPTIONBUTTON ptr )
    if pOpt = 0 then exit sub
    for i as long = 0 to gOptRegistryCount - 1
        if gOptRegistry(i) = pOpt then
            for j as long = i to gOptRegistryCount - 2
                gOptRegistry(j) = gOptRegistry(j + 1)
            next
            gOptRegistryCount -= 1
            gOptRegistry( gOptRegistryCount ) = 0
            exit sub
        end if
    next
end sub


' ========================================================================================
' Resolve a handle to its state block THROUGH THE REGISTRY, not through AfxCWindowPtr.
'
' This is a deliberate departure from the family idiom (every sibling reads UserData(0)), and
' the reason is the hazard described above: this is the first control that holds handles to
' OTHER controls, so "was this HWND really one of ours?" is a question that actually gets asked
' here. A registry hit answers it definitively; AfxCWindowPtr cannot answer it at all.
'
' The signature check is belt-and-braces on top of that -- by the time we have a registry hit the
' pointer is already known-good, so it can only catch memory that has been scribbled on.
' ========================================================================================
function GetOptionButtonPointer( byval hOptionButton as HWND ) as PSOPTIONBUTTON ptr
    if hOptionButton = 0 then return 0
    for i as long = 0 to gOptRegistryCount - 1
        dim as PSOPTIONBUTTON ptr p = gOptRegistry(i)
        if p then
            if p->hWin = hOptionButton then
                if p->dwSignature = PSOPTIONBUTTON_SIGNATURE then return p
                return 0
            end if
        end if
    next
    return 0
end function


' ========================================================================================
' GROUP ENUMERATION.  A group is the pair (hParent, nGroupID).
' ========================================================================================

private function PsOptionButton_GroupCountOf( byval hParent as HWND, byval nGroupID as long ) as long
    dim as long n = 0
    for i as long = 0 to gOptRegistryCount - 1
        dim as PSOPTIONBUTTON ptr p = gOptRegistry(i)
        if p then
            if (p->hParent = hParent) andalso (p->nGroupID = nGroupID) then n += 1
        end if
    next
    return n
end function


' The idx'th member of the group, in creation order. NULL when idx is out of range.
private function PsOptionButton_GroupAt( _
            byval hParent  as HWND, _
            byval nGroupID as long, _
            byval idx      as long _
            ) as PSOPTIONBUTTON ptr

    if idx < 0 then return 0
    dim as long n = 0
    for i as long = 0 to gOptRegistryCount - 1
        dim as PSOPTIONBUTTON ptr p = gOptRegistry(i)
        if p then
            if (p->hParent = hParent) andalso (p->nGroupID = nGroupID) then
                if n = idx then return p
                n += 1
            end if
        end if
    next
    return 0
end function


' Where does this button sit in its own group's order? -1 if it is not in the registry.
private function PsOptionButton_GroupIndexOf( byval pOpt as PSOPTIONBUTTON ptr ) as long
    if pOpt = 0 then return -1
    dim as long n = 0
    for i as long = 0 to gOptRegistryCount - 1
        dim as PSOPTIONBUTTON ptr p = gOptRegistry(i)
        if p then
            if (p->hParent = pOpt->hParent) andalso (p->nGroupID = pOpt->nGroupID) then
                if p = pOpt then return n
                n += 1
            end if
        end if
    next
    return -1
end function


' The checked member of a group, or NULL. There is at most one by construction -- every path
' that sets isChecked goes through PsOptionButton_ApplyCheck, which clears the previous holder
' before setting the new one.
private function PsOptionButton_GroupCheckedOf( byval hParent as HWND, byval nGroupID as long ) as PSOPTIONBUTTON ptr
    for i as long = 0 to gOptRegistryCount - 1
        dim as PSOPTIONBUTTON ptr p = gOptRegistry(i)
        if p then
            ' Hoisted to a boolean local rather than chained inline: "(a = b) andalso c" mixes a
            ' comparison result with a boolean field and trips fbc's warning 38, which is a real
            ' rule and not noise (C:\dev\Learnings.md).
            dim as boolean bInGroup = ((p->hParent = hParent) andalso (p->nGroupID = nGroupID))
            if bInGroup andalso p->isChecked then
                return p
            end if
        end if
    next
    return 0
end function


' ========================================================================================
' THE TAB-STOP INVARIANT
'
' Exactly one member of a group carries WS_TABSTOP: the checked one, or the first ENABLED one
' when none is checked, or -- if every member is disabled -- the first member, so the group
' never vanishes from the tab order entirely just because it is temporarily all-disabled.
'
' That is what makes the whole GROUP a single tab stop, which is what BS_AUTORADIOBUTTON does
' and is the reason the arrow keys exist here at all.
'
' THIS FUNCTION IS IDEMPOTENT AND RECOMPUTES FROM SCRATCH, which is why there is one of it
' rather than four. The invariant has four maintenance sites -- Create, a check moving,
' SetEnabled, and WM_NCDESTROY -- and hand-written "move the bit from A to B" logic at each
' would be four chances to get it wrong. The fourth site is the one that is easy to miss
' entirely: destroying the tabstop holder without handing the bit on drops the entire group out
' of the tab order, silently and permanently.
'
' Note this only ever writes WS_TABSTOP. It never touches focus: moving a style bit must not
' move the caret, and a SetFocus buried in a routine like this one is the "layout function that
' steals focus" trap recorded in C:\dev\Learnings.md.
' ========================================================================================
sub PsOptionButton_SyncTabStop( byval hParent as HWND, byval nGroupID as long )

    dim as PSOPTIONBUTTON ptr pWinner = PsOptionButton_GroupCheckedOf( hParent, nGroupID )

    if pWinner = 0 then
        ' Nothing checked: the first enabled member takes it.
        for i as long = 0 to gOptRegistryCount - 1
            dim as PSOPTIONBUTTON ptr p = gOptRegistry(i)
            if p then
                dim as boolean bInGroup = ((p->hParent = hParent) andalso (p->nGroupID = nGroupID))
                if bInGroup andalso p->isEnabled then
                    pWinner = p
                    exit for
                end if
            end if
        next
    end if

    if pWinner = 0 then
        ' Every member is disabled. Give it to the first one anyway rather than to nobody: a
        ' group that is disabled today may be enabled later, and a group with no tabstop at all
        ' is invisible to Tab in a way nothing would ever report.
        pWinner = PsOptionButton_GroupAt( hParent, nGroupID, 0 )
    end if

    for i as long = 0 to gOptRegistryCount - 1
        dim as PSOPTIONBUTTON ptr p = gOptRegistry(i)
        if p = 0 then continue for
        if (p->hParent <> hParent) orelse (p->nGroupID <> nGroupID) then continue for
        ' IsWindow, not "<> 0". A handle stored across time can be dead, and this routine runs
        ' from WM_NCDESTROY while a form is tearing down. See C:\dev\Learnings.md.
        if IsWindow( p->hWin ) = 0 then continue for

        dim as LONG_PTR nStyle = GetWindowLongPtr( p->hWin, GWL_STYLE )
        dim as LONG_PTR nWant  = nStyle
        if p = pWinner then
            nWant = nStyle or WS_TABSTOP
        else
            nWant = nStyle and (not cast(LONG_PTR, WS_TABSTOP))
        end if
        if nWant <> nStyle then SetWindowLongPtr( p->hWin, GWL_STYLE, nWant )
    next
end sub


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
'
' Callers decide whether to honor that answer -- three messages deliberately do not.
' WM_LBUTTONUP is one of them here, because this control HOLDS MOUSE CAPTURE across a press and
' the up-message is what releases it. (PsComboBox, which takes no capture, honours its
' callback's answer for that message; a note copied in from there would be a lie here.)
' ========================================================================================
private function PsOptionButton_CallMessage( _
            byval pOpt   as PSOPTIONBUTTON ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pOpt = 0 then return false
    if pOpt->MessageCallback = 0 then return false

    dim as PSOPTIONBUTTON_MESSAGEINFO m
    m.hOptionButton = hwnd
    m.uMsg          = uMsg
    m.wParam        = wParam
    m.lParam        = lParam
    return pOpt->MessageCallback( @m )
end function


' Resolve the tooltip text: the control's own text if it has any, else the host callback.
' NO CAPTION FALLBACK -- an option button's caption is already on screen, so falling back to it
' would pop a tip that repeats what the user is looking at.
private function PsOptionButton_ResolveTooltipText( byval pOpt as PSOPTIONBUTTON ptr ) as DWSTRING
    if pOpt = 0 then return ""
    if len( pOpt->wszTooltip ) then
        dim as DWSTRING wszOut = pOpt->wszTooltip
        return wszOut
    end if
    if pOpt->TooltipCallback then return pOpt->TooltipCallback( pOpt->hWin )
    return ""
end function


' ========================================================================================
' Draw the option button: circle, caption, focus ring.
'
' The BACKGROUND is not drawn here -- PsOptionButton_OnPaint has already flooded the client with
' the resolved mood's BackColor, which is what lets a host paint callback that only wants to add
' a foreground still sit on the right colour. There is no chrome: this control draws no frame
' and no body.
'
' Nothing in here touches a GDI object or a device context -- PsBufferPaint owns rendering and
' this control owns geometry and state, which is the family contract.
'
' TWO PRIMITIVES, AND THEY ARE NOT INTERCHANGEABLE:
'   PaintRoundOutline  strokes WITHOUT filling. This is the unchecked ring.
'   PaintEllipse       ALWAYS fills, whatever its pen argument says ("nPenWidth 0 = fill only"
'                      describes the pen, not the fill). This is the checked disc and the dot.
' Using PaintBorderRect for either would flood the control -- the mistake this family has made
' four separate times, most spectacularly in PsDatePicker where it rendered an entire popup
' calendar as one flat blue rectangle. PsOptionButton_CountRenderedTones exists to assert it.
'
' NEITHER PEN IS PRE-SCALED. PaintRoundOutline DPI-scales the pen width it is handed, so
' nRingThick and nFocusThick are stored raw and scaled exactly once, by the painter. Scaling
' them at Create as well would double them at any DPI above 100%.
'
' PRESENCE IS TESTED ON THE RECT, not on the string. LayoutOptionButton leaves rcText EMPTY when
' there is no caption, so IsRectEmpty is the single source of truth -- and a degenerate caption
' span (the client too narrow to leave any room) drops out of the same test.
' ========================================================================================
private sub PsOptionButton_RenderDefault( _
            byval pOpt as PSOPTIONBUTTON ptr, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSOPTIONBUTTON_PAINTINFO ptr _
            )

    if (pOpt = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long cw = p->rcContent.right - p->rcContent.left
    dim as long ch = p->rcContent.bottom - p->rcContent.top
    if (cw <= 0) orelse (ch <= 0) then exit sub

    ' ---- which four colors --------------------------------------------------------------
    dim as long nMood = PsOptionButton_ResolveMood( p->isEnabled, p->isPressed, p->isHot )
    dim as COLORREF clrBack, clrFore, clrRing, clrDot
    PsOptionButton_ResolveColors( @pOpt->colors, nMood, p->isChecked, clrBack, clrFore, clrRing, clrDot )

    ' ---- the circle ---------------------------------------------------------------------
    ' rcCircle, not rcBox: the cell is what the layout charges for, the circle is what is round.
    ' They are equal for a square cell and differ for any other, at which point stroking rcBox
    ' would draw a stadium instead of a radio button.
    dim as RECT rcC = p->rcCircle
    dim as long nSide = rcC.right - rcC.left
    if nSide > 0 then
        if p->isChecked then
            ' Accent-FILLED disc, then the contrasting dot on top of it. This is the look
            ' PsCheckBox structurally cannot produce -- its box is one composite glyph, so its
            ' outline and tick are one piece of ink in one colour.
            b->SetBackColor( clrRing )
            b->PaintEllipse( @rcC, 0 )

            dim as RECT rcD = p->rcDot
            if IsRectEmpty( @rcD ) = 0 then
                b->SetBackColor( clrDot )
                b->PaintEllipse( @rcD, 0 )
            end if
        else
            ' OUTLINE ONLY. The curvature is the full side, which turns the rounded rect into a
            ' circle -- PsToggle's focus ring uses the identical trick.
            b->SetPenColor( clrRing )
            b->PaintRoundOutline( @rcC, nSide, p->nRingThick )
        end if
    end if

    ' ---- the caption --------------------------------------------------------------------
    ' rcText is the SPAN between the box cell and the far padding, which is exactly what
    ' DT_END_ELLIPSIS needs: handed a tight box the ellipsis would have nowhere to happen and the
    ' text would simply clip. DT_NOPREFIX / DT_VCENTER / DT_SINGLELINE are added by PaintText
    ' itself -- and the first of those is what makes "&Highest" draw a literal ampersand, so this
    ' control's no-mnemonics rule is enforced by the renderer rather than merely intended.
    if IsRectEmpty( @p->rcText ) = 0 then
        dim as HFONT hTextUse = pOpt->hTextFont
        if hTextUse = 0 then hTextUse = cast( HFONT, GetStockObject( DEFAULT_GUI_FONT ) )

        dim as DWORD dtAlign = DT_LEFT
        select case p->nTextAlign
        case OPT_TEXTALIGN_CENTER
            dtAlign = DT_CENTER
        case OPT_TEXTALIGN_RIGHT
            dtAlign = DT_RIGHT
        end select

        dim as RECT rcT = p->rcText
        b->SetFont( hTextUse )
        b->SetColors( clrFore, clrBack )
        b->PaintText( p->wszText, @rcT, dtAlign or DT_END_ELLIPSIS )
    end if

    ' ---- the focus ring -----------------------------------------------------------------
    ' Outline, never filled: it is drawn OVER the control, and a filled round rect here would
    ' erase everything above it. Its band is reserved in the layout whether or not we are
    ' focused, which is what stops the content jumping when focus arrives.
    if p->isFocused then
        if pOpt->nFocusThick > 0 then
            dim as RECT rcRing = p->rcVisual
            b->SetPenColor( pOpt->colors.FocusRingColor )
            b->PaintRoundOutline( @rcRing, pOpt->nFocusCurvature, pOpt->nFocusThick )
        end if
    end if
end sub


' ========================================================================================
' Fill a PAINTINFO from the control's current state and geometry. One place, so the WM_PAINT
' path and the two offscreen probes cannot describe the control differently.
' ========================================================================================
private sub PsOptionButton_FillPaintInfo( _
            byval pOpt as PSOPTIONBUTTON ptr, _
            byval hwnd as HWND, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSOPTIONBUTTON_PAINTINFO ptr _
            )

    if (pOpt = 0) orelse (p = 0) then exit sub

    p->hOptionButton = hwnd
    p->b             = b
    GetClientRect( hwnd, @p->rcClient )
    p->rcContent  = pOpt->rcContent
    p->rcBox      = pOpt->rcBox
    p->rcCircle   = pOpt->rcCircle
    p->rcDot      = pOpt->rcDot
    p->rcText     = pOpt->rcText
    p->rcVisual   = pOpt->rcVisual
    p->isChecked  = pOpt->isChecked
    p->isHot      = pOpt->isHot
    p->isPressed  = pOpt->isPressed
    p->isEnabled  = pOpt->isEnabled
    p->isFocused  = pOpt->isFocused
    p->wszText    = pOpt->wszText
    p->nRingThick = pOpt->nRingThick
    p->nBoxAlign  = pOpt->nBoxAlign
    p->nTextAlign = pOpt->nTextAlign
end sub


' ========================================================================================
' Process WM_PAINT.
' ========================================================================================
private function PsOptionButton_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hwnd)
    if pOpt = null then return 0

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pOpt->bLayoutDirty then pOpt->LayoutOptionButton()

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)

    ' The client is filled with the resolved mood's own background, not a fixed one, so a paint
    ' callback that draws only a foreground still sits on the right colour. Resolved through the
    ' same two functions the painter uses, so the fill can never disagree with the rest.
    dim as long nMood = PsOptionButton_ResolveMood( pOpt->isEnabled, pOpt->isPressed, pOpt->isHot )
    dim as COLORREF clrFill, clrFore, clrRing, clrDot
    PsOptionButton_ResolveColors( @pOpt->colors, nMood, pOpt->isChecked, clrFill, clrFore, clrRing, clrDot )
    b.SetBackColor( clrFill )
    b.PaintClientRect()

    dim p as PSOPTIONBUTTON_PAINTINFO
    PsOptionButton_FillPaintInfo( pOpt, hwnd, @b, @p )

    if pOpt->PaintCallback then
        pOpt->PaintCallback( @p )
    else
        PsOptionButton_RenderDefault( pOpt, @b, @p )
    end if

    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Clear the hover state and stop the hover poll timer. Repaints only if the state actually
' changes, so this is cheap to call repeatedly.
'
' NOTE WHAT IT DOES NOT TOUCH: isChecked and isFocused. The family's hover safety net assumes
' hover is the only writer of the highlight, and a control that grows KEYBOARD-driven selection
' on top of that pattern gets its selection erased ~100ms after every keypress -- the trap
' recorded in Learnings.md against PsMenuBar. This control is exactly that sibling. It is safe
' because the arrow keys write isChecked and the system writes isFocused, and neither is hover
' state; keeping it that way is a live constraint, not an accident, and the self-test asserts it.
' ========================================================================================
private sub PsOptionButton_ClearHot( byval pOpt as PSOPTIONBUTTON ptr, byval hwnd as HWND )
    if pOpt = 0 then exit sub
    if pOpt->hotTimerOn then
        KillTimer( hwnd, IDT_PSOPTIONBUTTON_HOTTRACK )
        pOpt->hotTimerOn = false
    end if
    if pOpt->isHot then
        pOpt->isHot = false
        InvalidateRect( hwnd, NULL, FALSE )
    end if
end sub


' ========================================================================================
' Apply the ideal size to ourselves, preserving the top-left. Only runs when the host has opted
' in with PsOptionButton_SetAutoSize -- the family default is that a control never moves itself
' and the host measures with GetIdealSize.
'
' NOTE WHICH MUTATORS ARE ABSENT FROM ITS CALLERS: SetChecked, SetBoxAlign, SetRingThickness and
' SetDotSize. None of them can move the ideal size -- the circle lives inside a DECLARED cell
' whatever it looks like, and flipping the alignment mirrors the layout without changing what it
' costs.
' ========================================================================================
sub PsOptionButton_ApplyAutoSize( byval pOpt as PSOPTIONBUTTON ptr )
    if pOpt = 0 then exit sub
    if pOpt->bAutoSize = false then exit sub
    if pOpt->hWin = 0 then exit sub

    if pOpt->bLayoutDirty then pOpt->LayoutOptionButton()
    if (pOpt->nIdealW <= 0) orelse (pOpt->nIdealH <= 0) then exit sub

    SetWindowPos( pOpt->hWin, 0, 0, 0, pOpt->nIdealW, pOpt->nIdealH, _
                  SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
end sub


' ========================================================================================
' THE ONE PLACE isChecked IS EVER WRITTEN.  Mouse, keyboard and every host setter come through
' here, which is what makes mutual exclusion a property of the control rather than of each call
' site.
'
' BOTH STATE UPDATES HAPPEN BEFORE ANY CALLBACK FIRES, and then the LOSER is notified first:
'
'       pOld->isChecked = false
'       pOpt->isChecked = true          <-- the group is now coherent
'       OnChanged( hOld, false )        <-- and only now does anyone get told
'       OnChanged( hNew, true  )
'
' That ordering is the whole reason this is one function. Notifying as we go would mean a host
' reading PsOptionButton_FindChecked() from inside the first callback saw either two checked
' members or none, depending on which order the writes happened to be in -- and it would be
' whichever order, in whichever build, that nobody had thought about.
'
' bNotify is FALSE for every programmatic setter (Win32's BM_SETCHECK precedent) and TRUE only
' for user action. That is also what makes it safe for a host to call SetChecked from inside its
' own CheckChangedCallback: the recursive call cannot notify.
'
' UNCHECKING IS ALLOWED (bChecked = false) and leaves the group with nothing checked. That is a
' legal state -- it is the one a group starts in -- and the user cannot reach it, because no
' gesture routes here with bChecked false.
' ========================================================================================
private sub PsOptionButton_ApplyCheck( _
            byval pOpt     as PSOPTIONBUTTON ptr, _
            byval bChecked as boolean, _
            byval bNotify  as boolean _
            )

    if pOpt = 0 then exit sub
    if pOpt->isChecked = bChecked then exit sub      ' no-op, and therefore silent

    dim as PSOPTIONBUTTON ptr pOld = 0

    if bChecked then
        ' Find the outgoing holder BEFORE writing anything, so the search cannot see our own
        ' half-applied state.
        pOld = PsOptionButton_GroupCheckedOf( pOpt->hParent, pOpt->nGroupID )
        if pOld = pOpt then pOld = 0                 ' cannot happen (we early-outed), belt only
    end if

    ' ---- writes ------------------------------------------------------------------------
    if pOld then
        pOld->isChecked = false
        if IsWindow( pOld->hWin ) then InvalidateRect( pOld->hWin, NULL, FALSE )
    end if

    pOpt->isChecked = bChecked
    if IsWindow( pOpt->hWin ) then InvalidateRect( pOpt->hWin, NULL, FALSE )

    ' The tab stop follows the check. Recomputed rather than moved, so it is right even when the
    ' group had no checked member to begin with.
    PsOptionButton_SyncTabStop( pOpt->hParent, pOpt->nGroupID )

    ' ---- notifications, loser first ------------------------------------------------------
    if bNotify then
        if pOld then
            if pOld->CheckChangedCallback then pOld->CheckChangedCallback( pOld->hWin, false )
        end if
        if pOpt->CheckChangedCallback then pOpt->CheckChangedCallback( pOpt->hWin, pOpt->isChecked )
    end if
end sub


' ========================================================================================
' The user-action funnel. A completed mouse click, Space, an arrow-key move and the programmatic
' PsOptionButton_Click all come through here, so keyboard, mouse and host can never drift apart.
'
' It re-reads isEnabled rather than trusting the caller: a message callback may have disabled the
' control mid-gesture.
'
' CLICKING THE ALREADY-CHECKED MEMBER IS A SILENT NO-OP -- not an error, and not a re-notify.
' That is what makes this an option button rather than a checkbox: there is no gesture that
' unchecks, so the user can never empty a group.
' ========================================================================================
private sub PsOptionButton_FireCheck( byval pOpt as PSOPTIONBUTTON ptr )
    if pOpt = 0 then exit sub
    if pOpt->isEnabled = false then exit sub
    if pOpt->isChecked then exit sub
    PsOptionButton_ApplyCheck( pOpt, true, true )
end sub


' ========================================================================================
' ARROW-KEY NAVIGATION.  Move to the next/previous ENABLED member of the group, wrapping at both
' ends, and check it -- which is what BS_AUTORADIOBUTTON does: in a radio group the arrows do not
' merely move focus, they move the selection.
'
' nDelta is +1 (Down/Right) or -1 (Up/Left). The step is taken modulo the group size and retried
' up to count-1 times so a run of disabled members is stepped over rather than landed on; if
' every other member is disabled the search exhausts and we stay put, silently.
'
' FOCUS MOVES FIRST, THEN THE CHECK. A host reading GetFocus() from inside the change callback
' therefore sees the new button, which is the same order a real click produces (WM_LBUTTONDOWN
' focuses, WM_LBUTTONUP checks).
'
' This is the ONE place in the family where a control moves focus to a window it does not own.
' The only precedent is PsTextBox's VK_TAB handler, which delegates to GetNextDlgTabItem; here
' the group order is ours, so the walk is ours too.
' ========================================================================================
private sub PsOptionButton_NavigateGroup( byval pOpt as PSOPTIONBUTTON ptr, byval nDelta as long )

    if pOpt = 0 then exit sub
    if pOpt->isEnabled = false then exit sub

    dim as long nCount = PsOptionButton_GroupCountOf( pOpt->hParent, pOpt->nGroupID )
    if nCount <= 1 then exit sub

    dim as long idx = PsOptionButton_GroupIndexOf( pOpt )
    if idx < 0 then exit sub

    ' nStep, not "step": step is a FreeBASIC reserved word (the FOR clause) and using it as a
    ' loop variable fails with "Duplicated definition", which does not name the real problem.
    for nStep as long = 1 to nCount - 1
        ' Wrap in BOTH directions. The "+ nCount" is what makes a negative delta wrap rather
        ' than produce a negative index -- FreeBASIC's mod follows the sign of the dividend.
        dim as long nNext = ((idx + (nDelta * nStep)) mod nCount + nCount) mod nCount
        dim as PSOPTIONBUTTON ptr pNext = PsOptionButton_GroupAt( pOpt->hParent, pOpt->nGroupID, nNext )
        if pNext = 0 then continue for
        if pNext->isEnabled = false then continue for        ' step OVER disabled members
        if IsWindow( pNext->hWin ) = 0 then continue for

        SetFocus( pNext->hWin )
        PsOptionButton_FireCheck( pNext )
        exit sub
    next
end sub


' ========================================================================================
' PsOptionButton Window procedure
'
' This control has no child windows, so there is nothing to subclass: every message is handled
' here and anything unhandled goes to DefWindowProc.
'
' MOUSE CAPTURE IS TAKEN. The family's test is "take capture only if something consumes the
' guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing", and here it comes out positive: the
' press/cancel gesture (press the option button, slide off it, release, nothing gets checked) is
' exactly that. So the full price is paid -- snapshot the press before releasing, a
' WM_CAPTURECHANGED handler that cancels it, a release in WM_DESTROY, and a callback result that
' is ignored for the up-message.
'
' THE WHOLE CLIENT IS THE HIT AREA, caption included, exactly as a system radio button behaves.
' There is no hit test and no notion of clicking "the circle" specifically.
'
' CS_DBLCLKS IS DELIBERATELY *NOT* SET. With it, the second of two rapid clicks arrives as
' WM_LBUTTONDBLCLK instead of a second WM_LBUTTONDOWN. Here that would be harmless (a second
' click on the checked member is a no-op anyway) but it is also pointless, and a double-click
' never spans two windows so it cannot help a rapid A->B->A either.
'
' FOCUS: the sixth focusable control in the family, after PsToggle, PsComboBox, PsButton,
' PsCheckBox and PsDatePicker. See WM_GETDLGCODE for the two things that needed care.
' ========================================================================================
function PsOptionButton_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer( hwnd )

    select case uMsg

    case WM_PAINT
        return PsOptionButton_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a background
        ' erase here would only add flicker.
        return true

    case WM_SIZE
        if pOpt then
            pOpt->bLayoutDirty = true
            InvalidateRect( hwnd, NULL, TRUE )
        end if
        return 0

    case WM_TIMER
        if (pOpt <> 0) andalso (wParam = IDT_PSOPTIONBUTTON_HOTTRACK) then
            ' The cursor may have left without us getting a WM_MOUSELEAVE. If it is no longer
            ' over our client area, clear the hot state ourselves.
            '
            ' NOT while a press is live: during capture the cursor is legitimately allowed to
            ' wander off and come back, and the WM_MOUSEMOVE handler is already maintaining isHot
            ' for exactly that.
            if pOpt->isPressed = false then
                dim as RECT rcClient
                GetClientRect( hwnd, @rcClient )
                if isMouseOverRECT( hwnd, rcClient ) = false then
                    PsOptionButton_ClearHot( pOpt, hwnd )
                end if
            end if
            return 0
        end if

    case WM_MOUSEMOVE
        if pOpt = 0 then exit select
        if pOpt->isEnabled = false then exit select

        dim as RECT rcClient
        GetClientRect( hwnd, @rcClient )
        dim as boolean bOver = isMouseOverRECT( hwnd, rcClient )

        if bOver then
            ' Track so we get the eventual WM_MOUSELEAVE.
            dim tme as TrackMouseEvent
            tme.cbSize    = sizeof(TrackMouseEvent)
            tme.dwFlags   = TME_LEAVE
            tme.hwndTrack = hwnd
            TrackMouseEvent(@tme)

            ' Safety net: WM_MOUSELEAVE is not guaranteed to arrive when the mouse exits quickly
            ' (or onto another control), which would strand the control painted hot.
            if pOpt->hotTimerOn = false then
                SetTimer( hwnd, IDT_PSOPTIONBUTTON_HOTTRACK, PSOPTIONBUTTON_HOTTRACK_MS, NULL )
                pOpt->hotTimerOn = true
            end if
        end if

        ' This is also what drives the press-cancel FEEDBACK: with capture held, moves outside
        ' the client still reach us, so sliding off a pressed button clears isHot -- which drops
        ' the mood from PRESSED back to IDLE, before the release decides that nothing happened.
        if bOver <> pOpt->isHot then
            pOpt->isHot = bOver
            InvalidateRect( hwnd, NULL, FALSE )
        end if

        if PsOptionButton_CallMessage( pOpt, hwnd, uMsg, wParam, lParam ) then return 0

    case WM_MOUSELEAVE
        if pOpt then
            if pOpt->isPressed = false then PsOptionButton_ClearHot( pOpt, hwnd )
            if PsOptionButton_CallMessage( pOpt, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_LBUTTONDOWN
        if pOpt = 0 then return 0
        ' A disabled window does not receive mouse input at all (PsOptionButton_SetEnabled calls
        ' EnableWindow), so this is belt-and-braces against a host that disabled us by some other
        ' route.
        if pOpt->isEnabled = false then return 0

        ' Focus first, and unconditionally: clicking a control focuses it, and that is true even
        ' if the host goes on to suppress the press. This runs WM_SETFOCUS synchronously, so
        ' isFocused is already correct by the time we paint below.
        '
        ' NOTE FOR HOSTS ON A PsScrollPanel: focusing on the DOWN is what fights
        ' PsScrollPanel_SetAutoScrollToFocus -- the reveal tick scrolls the control out from
        ' under a still-held cursor and re-targets the click. tiko's Options dialog turns that
        ' setting off for exactly this reason (see Learnings.md). It is recorded here because
        ' the symptom appears in the host, never in this file.
        SetFocus( hwnd )

        ' Suppressing the press is allowed: no capture has been taken yet, so there is no
        ' bookkeeping to strand.
        if PsOptionButton_CallMessage( pOpt, hwnd, uMsg, wParam, lParam ) then return 0

        SetCapture( hwnd )
        pOpt->isPressed = true
        pOpt->isHot     = true
        InvalidateRect( hwnd, NULL, FALSE )
        return 0

    case WM_LBUTTONUP
        if pOpt = 0 then return 0

        ' SNAPSHOT BEFORE RELEASING. ReleaseCapture sends WM_CAPTURECHANGED to the window losing
        ' capture SYNCHRONOUSLY, and our handler for it cancels the press -- so reading isPressed
        ' after the release would always find FALSE and nothing would ever get checked.
        dim as boolean bWasPressed = pOpt->isPressed
        ' Release capture unconditionally, before any callback runs. Nothing below -- not a
        ' callback, not a host handler -- gets the chance to leave it held.
        if GetCapture() = hwnd then ReleaseCapture()

        dim as RECT rcClient
        GetClientRect( hwnd, @rcClient )
        dim as boolean bInside = isMouseOverRECT( hwnd, rcClient )

        pOpt->CancelPress()
        InvalidateRect( hwnd, NULL, FALSE )

        ' The result is deliberately IGNORED here (see OPT_MessageCallbackFunc).
        PsOptionButton_CallMessage( pOpt, hwnd, uMsg, wParam, lParam )

        ' Only a matched press+release with the cursor still inside checks. Press, slide off,
        ' release = nothing happened, which is the whole reason for capture.
        if (bWasPressed andalso bInside) then PsOptionButton_FireCheck( pOpt )
        return 0

    case WM_CAPTURECHANGED
        ' Capture can be stolen at any time (a modal dialog, another control, the shell). The
        ' gesture is over; forget it and repaint.
        if pOpt then
            pOpt->CancelPress()
            InvalidateRect( hwnd, NULL, FALSE )
            return 0
        end if

    case WM_RBUTTONDOWN, WM_RBUTTONUP
        ' Reported, never acted on: a context menu is the host's business (PsPopupMenu). No
        ' capture is taken for the right button, so an up can arrive without a down.
        if pOpt then
            if PsOptionButton_CallMessage( pOpt, hwnd, uMsg, wParam, lParam ) then return 0
        end if

    case WM_SETFOCUS, WM_KILLFOCUS
        ' A pending swallow must not survive a focus change: the WM_CHAR it was armed for is
        ' not coming, and a stale flag would eat the next character instead.
        if pOpt then pOpt->bAteKeyDown = false
        ' Focus is a FACT reported by the system, not an action a host can veto, so the state is
        ' updated first and the callback's result is IGNORED.
        if pOpt then
            dim as boolean bFocused = (uMsg = WM_SETFOCUS)
            if bFocused <> pOpt->isFocused then
                pOpt->isFocused = bFocused
                InvalidateRect( hwnd, NULL, FALSE )
            end if
            PsOptionButton_CallMessage( pOpt, hwnd, uMsg, wParam, lParam )
            return 0
        end if

    case WM_GETDLGCODE
        ' Claim ONLY the keys we actually consume, and only when we are being asked about a
        ' specific message (lParam points at the MSG when it does).
        '
        ' Claiming unconditionally would return DLGC_WANTALLKEYS for Tab as well, and the control
        ' would swallow the very navigation it just opted into by being a tabstop. Claiming
        ' nothing would let a host's IsDialogMessage pump route the arrows somewhere else before
        ' the control ever saw them. Per-message is the only answer that gets both right.
        '
        ' VK_RETURN IS ABSENT, AND THAT IS THE DECISION, NOT AN OMISSION. PsCheckBox, PsToggle
        ' and PsButton all claim Enter; a Win32 radio button does not, and lets it reach the
        ' dialog's default button instead. This control commits to Win32 group semantics
        ' everywhere else -- one tab stop per group, arrows that move the selection -- so taking
        ' Enter would contradict that in the one place a user would notice: pressing Enter to
        ' accept a dialog after picking an option. Asserted NEGATIVELY in the self-test, so a
        ' future edit that "restores family consistency" turns the suite red rather than changing
        ' behaviour quietly.
        '
        ' VK_TAB is absent for the more obvious reason: claiming it would break the navigation
        ' the tabstop exists for.
        if lParam then
            dim as MSG ptr pMsg = cast( MSG ptr, lParam )
            ' Claim the CHARACTER produced by a key we already consumed. Claiming a key with
            ' DLGC_WANTALLKEYS makes IsDialogMessage TranslateMessage it, which manufactures a
            ' matching WM_CHAR; if nothing claims that, it reaches DefWindowProc and beeps.
            if pMsg->message = WM_CHAR then
                if pOpt then
                    if pOpt->bAteKeyDown then return DLGC_WANTALLKEYS
                end if
            end if
            if pMsg->message = WM_KEYDOWN then
                select case pMsg->wParam
                case VK_SPACE, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT
                    return DLGC_WANTALLKEYS
                end select
            end if
        end if
        return 0

    case WM_CHAR
        ' Swallow the character generated by a key WM_KEYDOWN already acted on, so it never
        ' reaches DefWindowProc -- which beeps at a character no control wants. The keystroke
        ' works either way, so the beep is the ONLY symptom and no assertion can hear it.
        '
        ' Anything we did not consume falls through untouched. WM_SYSCHAR is deliberately not
        ' touched either: swallowing it would break Alt+F4 and the host's menu mnemonics.
        if pOpt = 0 then exit select
        if pOpt->bAteKeyDown then
            pOpt->bAteKeyDown = false
            return 0
        end if

    case WM_KEYDOWN
        if pOpt = 0 then exit select
        ' Arm the character swallow (see WM_CHAR). Every path below that handles one of these
        ' keys consumes it, so the key alone decides -- and only keys that actually produce a
        ' WM_CHAR are armed.
        pOpt->bAteKeyDown = (wParam = VK_SPACE)
        if pOpt->isEnabled = false then return 0
        ' The host sees the key BEFORE it moves the group selection, so navigation can be vetoed.
        if PsOptionButton_CallMessage( pOpt, hwnd, uMsg, wParam, lParam ) then return 0
        select case wParam
        case VK_SPACE
            ' Same funnel a completed click takes, so keyboard and mouse cannot drift apart:
            ' both are user action, both notify, and both are a no-op on the checked member.
            PsOptionButton_FireCheck( pOpt )
            return 0
        case VK_DOWN, VK_RIGHT
            PsOptionButton_NavigateGroup( pOpt, 1 )
            return 0
        case VK_UP, VK_LEFT
            PsOptionButton_NavigateGroup( pOpt, -1 )
            return 0
        end select

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of PsOptionButton_SetEnabled still gets the greyed
        ' rendering rather than a control that looks live but eats input.
        if pOpt then
            pOpt->isEnabled = (wParam <> 0)
            if pOpt->isEnabled = false then
                pOpt->isHot = false
                pOpt->CancelPress()
            end if
            ' A disabled member must not keep the group's tab stop, and a re-enabled one may now
            ' be the best holder. Recomputed either way.
            PsOptionButton_SyncTabStop( pOpt->hParent, pOpt->nGroupID )
            InvalidateRect( hwnd, NULL, FALSE )
        end if

    case WM_SETTEXT
        ' The caption has ONE store, reached through two doors: PsOptionButton_SetText and this
        ' message. DefWindowProc is deliberately not called -- letting it keep a second copy in
        ' the window's own text would give GetWindowText an answer that could drift from what is
        ' painted.
        if pOpt = 0 then exit select
        if lParam = 0 then
            pOpt->wszText = ""
        else
            pOpt->wszText = *cast( wstring ptr, lParam )
        end if
        pOpt->Refresh()
        PsOptionButton_ApplyAutoSize( pOpt )
        return TRUE

    case WM_GETTEXT
        ' wParam = buffer size in CHARACTERS including the terminator; return value excludes it.
        ' Copied by hand rather than through a wstring assignment, because the caller's buffer is
        ' not ours to overrun.
        if pOpt = 0 then exit select
        dim as long nMax = cast( long, wParam )
        if nMax <= 0 then return 0
        dim as ushort ptr pDst = cast( ushort ptr, lParam )
        if pDst = 0 then return 0
        dim as long nLen = len( pOpt->wszText )
        if nLen > (nMax - 1) then nLen = nMax - 1
        if nLen > 0 then
            dim as ushort ptr pSrc = cast( ushort ptr, pOpt->wszText.vptr )
            if pSrc then
                for i as long = 0 to nLen - 1
                    pDst[i] = pSrc[i]
                next
            else
                nLen = 0
            end if
        end if
        pDst[nLen] = 0
        return nLen

    case WM_GETTEXTLENGTH
        ' Required, not optional: GetWindowText asks this first and allocates from the answer, so
        ' without it a caller gets an empty string no matter what WM_GETTEXT would have said.
        if pOpt = 0 then exit select
        return len( pOpt->wszText )

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply it.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pOpt then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' Store in an instance-lifetime buffer and point lpszText at it. NOT in
                ' wszTooltip: that field is the host's authored text, and a callback's answer
                ' written there would silently become stored state.
                pOpt->wszTipBuf = PsOptionButton_ResolveTooltipText( pOpt )
                pdi->lpszText = cast( LPWSTR, pOpt->wszTipBuf.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        if pOpt then
            ' Don't let the hot-tracking timer outlive the window...
            if pOpt->hotTimerOn then
                KillTimer( hwnd, IDT_PSOPTIONBUTTON_HOTTRACK )
                pOpt->hotTimerOn = false
            end if
            ' ...nor the capture, if we are being destroyed mid-press.
            if GetCapture() = hwnd then ReleaseCapture()
            pOpt->CancelPress()
        end if

    case WM_NCDESTROY
        if pOpt then
            ' THE FOURTH TAB-STOP MAINTENANCE SITE, and the one that is easy to miss entirely.
            ' Leave the registry FIRST so the re-sync cannot see us, then hand the tab stop to a
            ' survivor. Without this, destroying the member that happened to hold WS_TABSTOP
            ' takes the whole remaining group out of the tab order -- silently, permanently, and
            ' with nothing in a build or an assertion to say so.
            '
            ' If we were the CHECKED member the group is simply left with nothing checked. That
            ' is the legal empty state, and it is deliberately not reassigned to an arbitrary
            ' survivor: the host asked for this button to go, not for a different answer.
            dim as HWND hOldParent  = pOpt->hParent
            dim as long nOldGroupID = pOpt->nGroupID
            PsOptionButton_RegistryRemove( pOpt )
            PsOptionButton_SyncTabStop( hOldParent, nOldGroupID )

            ' The font is the host's and is left alone; the tooltip window is ours and goes with
            ' us.
            PsTipHost_Destroy( @pOpt->tip )
            delete(pOpt)
        end if

        ' delete the allocated memory for window
        dim as CWindow ptr pWindow = AfxCWindowPtr(hwnd)
        if pWindow then delete(pWindow)
        return 0

    end select

    ' for messages that we don't deal with
    function = DefWindowProc( hwnd, uMsg, wParam, lParam )

end function


' ========================================================================================
' Supply the tooltip text to the PsTooltip backend.
'
' This thunk is why PsTipHost never learns the text policy: PsOptionButton_ResolveTooltipText
' stays the ONE place that decides what a tip says, and the system backend reaches the
' same function by the other road (comctl32 asking through TTN_GETDISPINFOW), which is
' why that arm stays in the WndProc.
' ========================================================================================
private function PsOptionButton_TipTextCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                        byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut = PsOptionButton_ResolveTooltipText( GetOptionButtonPointer(hAttached) )
    return wszOut
end function


' ========================================================================================
' PsOptionButton_Create
' ========================================================================================
function PsOptionButton_Create( _
            byval hWndParent as HWND, _
            byval CtrlID     as long, _
            byval nGroupID   as long = PSOPTIONBUTTON_DEFAULT_GROUPID _
            ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' WS_TABSTOP: this control takes focus. Note that PsOptionButton_SyncTabStop will CLEAR this
    ' bit again a few lines below if the group already has a better holder -- exactly one member
    ' of a group carries it, which is what makes the group a single tab stop.
    '
    ' THE EXTENDED STYLE IS PASSED EXPLICITLY, AND IT MUST BE. CWindow.Create defaults its
    ' dwExStyle parameter to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so a control that supplies
    ' only dwStyle silently declares itself a CONTAINER. The dialog manager then descends into
    ' it hunting for tabstops, finds no children, and SKIPS IT: the control becomes unreachable
    ' by Tab. That shipped in PsButton's first commit and was then found in PsToggle and
    ' PsComboBox too; see C:\dev\Learnings.md. It is ASSERTED in the self-test below, not merely
    ' remembered.
    '
    ' A leaf control must be a tab ITEM, not a tab container. WS_EX_WINDOWEDGE goes with it -- it
    ' is a raised-border hint that means nothing to a window drawing its own chrome.
    '
    ' DO NOT COPY THIS LINE INTO A WRAP-A-REAL-CHILD CONTROL. PsListBox, PsTextBox and
    ' PsNumericUpDown all NEED WS_EX_CONTROLPARENT, because their tabstop lives on an inner child
    ' and their container is a real container; passing 0 there would make them unreachable. The
    ' test is "does this window have children the dialog manager should find?", not "is this a
    ' control?".
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsOptionButton_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_TABSTOP or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled here -- see the WndProc header.

    dim as PSOPTIONBUTTON ptr pOpt = new PSOPTIONBUTTON
    pOpt->hWin             = hCtrl
    pOpt->idc_OptionButton = CtrlID
    pOpt->id               = CtrlID     ' sensible default; PsOptionButton_SetID overrides it
    pOpt->nGroupID         = nGroupID
    ' Cached rather than re-read with GetParent on every group scan: the scans run on every click
    ' and every arrow key, and it is also what lets WM_NCDESTROY re-sync the group it is leaving.
    pOpt->hParent          = hWndParent

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule).
    '
    ' nRingThick AND nFocusThick ARE DELIBERATELY ABSENT FROM THIS LIST. Both are handed to
    ' PsBufferPaint.PaintRoundOutline, which DPI-scales the pen itself; scaling them here as well
    ' would scale them twice and give a two-pixel ring at 175% where one was asked for. They do
    ' end up DPI-aware -- the painter is simply the thing that does it. (nDotSize is scaled, and
    ' correctly so: nothing downstream scales a distance.)
    pOpt->nPadLeft        = pWindow->ScaleX( PSOPTIONBUTTON_DEFAULT_PADLEFT )
    pOpt->nPadTop         = pWindow->ScaleY( PSOPTIONBUTTON_DEFAULT_PADTOP )
    pOpt->nPadRight       = pWindow->ScaleX( PSOPTIONBUTTON_DEFAULT_PADRIGHT )
    pOpt->nPadBottom      = pWindow->ScaleY( PSOPTIONBUTTON_DEFAULT_PADBOTTOM )
    pOpt->nBoxGap         = pWindow->ScaleX( PSOPTIONBUTTON_DEFAULT_BOXGAP )
    pOpt->nBoxWidth       = pWindow->ScaleX( PSOPTIONBUTTON_DEFAULT_BOXWIDTH )
    pOpt->nBoxHeight      = pWindow->ScaleY( PSOPTIONBUTTON_DEFAULT_BOXHEIGHT )
    pOpt->nFocusGap       = pWindow->ScaleY( PSOPTIONBUTTON_DEFAULT_FOCUSGAP )
    pOpt->nFocusCurvature = pWindow->ScaleY( PSOPTIONBUTTON_DEFAULT_FOCUSCURV )


    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the pointer in the CWindow user data area as well. NOTHING LOOKS IT UP THERE --
    ' GetOptionButtonPointer resolves through the registry, for the reasons in its header -- but
    ' keeping it costs nothing and preserves the family's shape for anyone reading this next to a
    ' sibling.
    pWindow->UserData(0) = cast(LONG_PTR, pOpt)

    ' AFTER UserData, not before: PsOptionButton_TipTextCallback resolves the control through
    ' GetOptionButtonPointer(hAttached), which reads exactly that slot.
    PsTipHost_Ensure( @pOpt->tip, hCtrl, @PsOptionButton_TipTextCallback )

    ' Join the registry BEFORE the tab-stop sync, or the sync cannot see us.
    PsOptionButton_RegistryAdd( pOpt )
    PsOptionButton_SyncTabStop( hWndParent, nGroupID )

    function = hCtrl

end function


' ========================================================================================
' Group.  All SILENT (see PsOptionButton_ApplyCheck).
' ========================================================================================
function PsOptionButton_GetGroupID( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->nGroupID
end function


' Moving a button between groups re-establishes the invariant in BOTH groups: the one it left
' may have lost its tab-stop holder, and the one it joined may now have two candidates.
'
' If it was the checked member of its old group, that group is left with nothing checked. The
' check is NOT reassigned to a survivor -- that would be the control inventing an answer the host
' never gave it.
sub PsOptionButton_SetGroupID( byval hOptionButton as HWND, byval nGroupID as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if pOpt->nGroupID = nGroupID then exit sub

    dim as long nOldGroupID = pOpt->nGroupID

    ' Arriving in a group that already has a checked member cannot make two: the arrival is
    ' unchecked, or it is checked and the newcomer loses. Losing rather than winning is the
    ' conservative choice -- a re-group is a layout operation, not a user answering a question.
    if pOpt->isChecked then
        if PsOptionButton_GroupCheckedOf( pOpt->hParent, nGroupID ) <> 0 then
            pOpt->isChecked = false
            if IsWindow( pOpt->hWin ) then InvalidateRect( pOpt->hWin, NULL, FALSE )
        end if
    end if

    pOpt->nGroupID = nGroupID

    PsOptionButton_SyncTabStop( pOpt->hParent, nOldGroupID )
    PsOptionButton_SyncTabStop( pOpt->hParent, nGroupID )
end sub


function PsOptionButton_GetGroupCount( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return PsOptionButton_GroupCountOf( pOpt->hParent, pOpt->nGroupID )
end function


function PsOptionButton_GetGroupMember( byval hOptionButton as HWND, byval idx as long ) as HWND
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    dim as PSOPTIONBUTTON ptr pMember = PsOptionButton_GroupAt( pOpt->hParent, pOpt->nGroupID, idx )
    if pMember = 0 then return 0
    return pMember->hWin
end function


function PsOptionButton_GetGroupChecked( byval hOptionButton as HWND ) as HWND
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    dim as PSOPTIONBUTTON ptr pChecked = PsOptionButton_GroupCheckedOf( pOpt->hParent, pOpt->nGroupID )
    if pChecked = 0 then return 0
    return pChecked->hWin
end function


function PsOptionButton_FindChecked( byval hWndParent as HWND, byval nGroupID as long ) as HWND
    dim as PSOPTIONBUTTON ptr pChecked = PsOptionButton_GroupCheckedOf( hWndParent, nGroupID )
    if pChecked = 0 then return 0
    return pChecked->hWin
end function


' -1 for "nothing checked", which is a legal answer and not an error. A host storing a setting
' should treat it as "unanswered" rather than as a failure.
function PsOptionButton_FindCheckedID( byval hWndParent as HWND, byval nGroupID as long ) as long
    dim as PSOPTIONBUTTON ptr pChecked = PsOptionButton_GroupCheckedOf( hWndParent, nGroupID )
    if pChecked = 0 then return -1
    return pChecked->id
end function


sub PsOptionButton_ClearGroup( byval hOptionButton as HWND )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    dim as PSOPTIONBUTTON ptr pChecked = PsOptionButton_GroupCheckedOf( pOpt->hParent, pOpt->nGroupID )
    if pChecked = 0 then exit sub                     ' already empty; silent no-op
    PsOptionButton_ApplyCheck( pChecked, false, false )
end sub


' ========================================================================================
' Content.  All SILENT -- they change what is drawn, never what was clicked.
' ========================================================================================
function PsOptionButton_GetText( byval hOptionButton as HWND ) as DWSTRING
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    ' ONE return style for the whole function. A function returning an object with constructors
    ' cannot mix "return x" with "function = x" / "exit function" (C:\dev\Learnings.md).
    dim as DWSTRING wszOut
    if pOpt then wszOut = pOpt->wszText
    return wszOut
end function

sub PsOptionButton_SetText( byval hOptionButton as HWND, byval Text as DWSTRING )
    ' byval, never "byref as const". A const-qualified DWSTRING reference that is then COPIED
    ' corrupts the process heap -- measured at 0xC0000374 in a 30-line program with no window
    ' (C:\dev\Learnings.md). Every DWSTRING parameter in this family is byval.
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->wszText = Text
    pOpt->Refresh()
    PsOptionButton_ApplyAutoSize( pOpt )
end sub

function PsOptionButton_GetID( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->id
end function

sub PsOptionButton_SetID( byval hOptionButton as HWND, byval id as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->id = id
end sub

function PsOptionButton_GetItemData( byval hOptionButton as HWND ) as integer
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->itemData
end function

sub PsOptionButton_SetItemData( byval hOptionButton as HWND, byval itemData as integer )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->itemData = itemData
end sub


' ========================================================================================
' State.
' ========================================================================================
function PsOptionButton_GetChecked( byval hOptionButton as HWND ) as boolean
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return false
    return pOpt->isChecked
end function

sub PsOptionButton_SetChecked( byval hOptionButton as HWND, byval isChecked as boolean )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    ' SILENT: bNotify = false. Unlike the user paths this does NOT refuse a disabled control --
    ' a host restoring saved settings onto a form whose options are conditionally greyed must
    ' still be able to say which one is selected.
    PsOptionButton_ApplyCheck( pOpt, isChecked, false )
end sub

function PsOptionButton_GetEnabled( byval hOptionButton as HWND ) as boolean
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return false
    return pOpt->isEnabled
end function

sub PsOptionButton_SetEnabled( byval hOptionButton as HWND, byval isEnabled as boolean )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if pOpt->hWin = 0 then exit sub
    ' EnableWindow, not a cosmetic flag: the disable is then enforced by the system. It sends
    ' WM_ENABLE synchronously, and THAT is what updates isEnabled and re-syncs the group's tab
    ' stop -- so a host calling EnableWindow directly gets the same bookkeeping.
    EnableWindow( pOpt->hWin, iif( isEnabled, TRUE, FALSE ) )
    PsOptionButton_ClearHot( pOpt, pOpt->hWin )
    InvalidateRect( pOpt->hWin, NULL, FALSE )
end sub

function PsOptionButton_GetFocused( byval hOptionButton as HWND ) as boolean
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return false
    return pOpt->isFocused
end function

sub PsOptionButton_Refresh( byval hOptionButton as HWND )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->Refresh()
end sub


' ========================================================================================
' Action.  The one public entry point that NOTIFIES.
' ========================================================================================
sub PsOptionButton_Click( byval hOptionButton as HWND )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    PsOptionButton_FireCheck( pOpt )
end sub


' ========================================================================================
' Layout.
' ========================================================================================
function PsOptionButton_GetBoxAlign( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return OPT_BOXALIGN_LEFT
    return pOpt->nBoxAlign
end function

' No ApplyAutoSize: the mirror is exact, so flipping the side cannot change the ideal size.
sub PsOptionButton_SetBoxAlign( byval hOptionButton as HWND, byval nBoxAlign as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->nBoxAlign = nBoxAlign
    pOpt->Refresh()
end sub

function PsOptionButton_GetTextAlign( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return OPT_TEXTALIGN_LEFT
    return pOpt->nTextAlign
end function

sub PsOptionButton_SetTextAlign( byval hOptionButton as HWND, byval nTextAlign as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->nTextAlign = nTextAlign
    pOpt->Refresh()
end sub

sub PsOptionButton_GetPadding( byval hOptionButton as HWND, byref nLeft as long, byref nTop as long, byref nRight as long, byref nBottom as long )
    nLeft = 0 : nTop = 0 : nRight = 0 : nBottom = 0
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    nLeft   = pOpt->nPadLeft
    nTop    = pOpt->nPadTop
    nRight  = pOpt->nPadRight
    nBottom = pOpt->nPadBottom
end sub

sub PsOptionButton_SetPadding( byval hOptionButton as HWND, byval nLeft as long, byval nTop as long, byval nRight as long, byval nBottom as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->nPadLeft   = nLeft
    pOpt->nPadTop    = nTop
    pOpt->nPadRight  = nRight
    pOpt->nPadBottom = nBottom
    pOpt->Refresh()
    PsOptionButton_ApplyAutoSize( pOpt )
end sub

function PsOptionButton_GetBoxGap( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->nBoxGap
end function

sub PsOptionButton_SetBoxGap( byval hOptionButton as HWND, byval nBoxGap as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->nBoxGap = nBoxGap
    pOpt->Refresh()
    PsOptionButton_ApplyAutoSize( pOpt )
end sub

sub PsOptionButton_GetBoxSize( byval hOptionButton as HWND, byref nBoxWidth as long, byref nBoxHeight as long )
    nBoxWidth = 0 : nBoxHeight = 0
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    nBoxWidth  = pOpt->nBoxWidth
    nBoxHeight = pOpt->nBoxHeight
end sub

sub PsOptionButton_SetBoxSize( byval hOptionButton as HWND, byval nBoxWidth as long, byval nBoxHeight as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if nBoxWidth  < 0 then nBoxWidth  = 0
    if nBoxHeight < 0 then nBoxHeight = 0
    pOpt->nBoxWidth  = nBoxWidth
    pOpt->nBoxHeight = nBoxHeight
    pOpt->Refresh()
    PsOptionButton_ApplyAutoSize( pOpt )
end sub

function PsOptionButton_GetRingThickness( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->nRingThick
end function

' RAW pixels. PaintRoundOutline DPI-scales the pen it is handed, so pre-scaling here would
' double it. No re-layout: the ring is drawn inside a declared cell and cannot move a rect.
sub PsOptionButton_SetRingThickness( byval hOptionButton as HWND, byval nThickness as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if nThickness < 0 then nThickness = 0
    pOpt->nRingThick = nThickness
    if pOpt->hWin then InvalidateRect( pOpt->hWin, NULL, FALSE )
end sub

function PsOptionButton_GetDotSize( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->nDotSize
end function

' Marks the layout dirty rather than only invalidating, because rcDot is a DERIVED rect and the
' next paint has to recompute it. It cannot change the ideal size, so no auto-size.
sub PsOptionButton_SetDotSize( byval hOptionButton as HWND, byval nDotSize as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if nDotSize < 0 then nDotSize = 0
    pOpt->nDotSize = nDotSize
    pOpt->Refresh()
end sub

sub PsOptionButton_GetFocusRing( byval hOptionButton as HWND, byref nGap as long, byref nThickness as long )
    nGap = 0 : nThickness = 0
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    nGap       = pOpt->nFocusGap
    nThickness = pOpt->nFocusThick
end sub

' Both feed RingPad(), which is reserved unconditionally and therefore contributes to the ideal
' size -- so unlike the ring thickness this one does re-measure.
sub PsOptionButton_SetFocusRing( byval hOptionButton as HWND, byval nGap as long, byval nThickness as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if nGap < 0 then nGap = 0
    if nThickness < 0 then nThickness = 0
    pOpt->nFocusGap   = nGap
    pOpt->nFocusThick = nThickness
    pOpt->Refresh()
    PsOptionButton_ApplyAutoSize( pOpt )
end sub

function PsOptionButton_GetFocusCurvature( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->nFocusCurvature
end function

sub PsOptionButton_SetFocusCurvature( byval hOptionButton as HWND, byval nCurvature as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if nCurvature < 0 then nCurvature = 0
    pOpt->nFocusCurvature = nCurvature
    if pOpt->hWin then InvalidateRect( pOpt->hWin, NULL, FALSE )
end sub

function PsOptionButton_GetAutoSize( byval hOptionButton as HWND ) as boolean
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return false
    return pOpt->bAutoSize
end function

sub PsOptionButton_SetAutoSize( byval hOptionButton as HWND, byval bAutoSize as boolean )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->bAutoSize = bAutoSize
    PsOptionButton_ApplyAutoSize( pOpt )
end sub

' Valid BEFORE the control has ever been sized -- the measuring pass runs ahead of the
' zero-client bail in LayoutOptionButton precisely so a host can call this to decide how big to
' make the control in the first place.
sub PsOptionButton_GetIdealSize( byval hOptionButton as HWND, byref nWidth as long, byref nHeight as long )
    nWidth = 0 : nHeight = 0
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    if pOpt->bLayoutDirty then pOpt->LayoutOptionButton()
    nWidth  = pOpt->nIdealW
    nHeight = pOpt->nIdealH
end sub

' The six rect getters share one shape: force the pending layout, then hand back a copy. FALSE
' means the control has no geometry yet (created but never sized) or the rect is legitimately
' empty -- rcText with no caption being the normal case of the second.
private function PsOptionButton_GetRectCommon( byval hOptionButton as HWND, byref rc as RECT, byval nWhich as long ) as boolean
    SetRectEmpty( @rc )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return false
    ' Force the derivation before reading it. An assertion that reads a lazily-derived rect
    ' without this measures the value from BEFORE the change it is testing (C:\dev\Learnings.md).
    if pOpt->bLayoutDirty then pOpt->LayoutOptionButton()
    select case nWhich
    case 0 : rc = pOpt->rcContent
    case 1 : rc = pOpt->rcBox
    case 2 : rc = pOpt->rcCircle
    case 3 : rc = pOpt->rcDot
    case 4 : rc = pOpt->rcText
    case else : rc = pOpt->rcVisual
    end select
    return (IsRectEmpty( @rc ) = 0)
end function

function PsOptionButton_GetContentRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
    return PsOptionButton_GetRectCommon( hOptionButton, rc, 0 )
end function
function PsOptionButton_GetBoxRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
    return PsOptionButton_GetRectCommon( hOptionButton, rc, 1 )
end function
function PsOptionButton_GetCircleRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
    return PsOptionButton_GetRectCommon( hOptionButton, rc, 2 )
end function
function PsOptionButton_GetDotRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
    return PsOptionButton_GetRectCommon( hOptionButton, rc, 3 )
end function
function PsOptionButton_GetTextRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
    return PsOptionButton_GetRectCommon( hOptionButton, rc, 4 )
end function
function PsOptionButton_GetVisualRect( byval hOptionButton as HWND, byref rc as RECT ) as boolean
    return PsOptionButton_GetRectCommon( hOptionButton, rc, 5 )
end function


' ========================================================================================
' Appearance.
' ========================================================================================
sub PsOptionButton_GetColors( byval hOptionButton as HWND, byval pColors as PSOPTIONBUTTON_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    *pColors = pOpt->colors
end sub

sub PsOptionButton_SetColors( byval hOptionButton as HWND, byval pColors as PSOPTIONBUTTON_COLORS ptr )
    if pColors = 0 then exit sub
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->colors = *pColors
    if pOpt->hWin then InvalidateRect( pOpt->hWin, NULL, FALSE )
end sub

function PsOptionButton_GetFont( byval hOptionButton as HWND ) as HFONT
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return pOpt->hTextFont
end function

' The MEASURING font: it drives the ideal width, so this re-measures and re-auto-sizes.
sub PsOptionButton_SetFont( byval hOptionButton as HWND, byval hTextFont as HFONT )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->hTextFont = hTextFont
    pOpt->Refresh()
    PsOptionButton_ApplyAutoSize( pOpt )
end sub


' ========================================================================================
' Tooltips.
' ========================================================================================
function PsOptionButton_GetTooltipText( byval hOptionButton as HWND ) as DWSTRING
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    dim as DWSTRING wszOut
    if pOpt then wszOut = pOpt->wszTooltip
    return wszOut
end function

sub PsOptionButton_SetTooltipText( byval hOptionButton as HWND, byval Text as DWSTRING )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->wszTooltip = Text
end sub

' The comctl32 handle, and 0 while on PsTooltip -- the honest answer, since a TTM_* sent
' to a PsTooltip window is silently ignored.
function PsOptionButton_GetTooltipHandle( byval hOptionButton as HWND ) as HWND
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return PsTipHost_SysHandle( @pOpt->tip )
end function

' The PsTooltip window, and 0 while on the system backend. This is the door to PsTooltip's
' own SetColors/SetFonts/SetStyle/SetMaxWidth/SetTitle/SetGlyph, none of which is mirrored
' on this control.
function PsOptionButton_GetPsTooltipHandle( byval hOptionButton as HWND ) as HWND
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return 0
    return PsTipHost_PsHandle( @pOpt->tip )
end function

function PsOptionButton_SetTooltipMode( byval hOptionButton as HWND, byval nMode as long ) as boolean
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return false
    return PsTipHost_SetMode( @pOpt->tip, nMode )
end function

function PsOptionButton_GetTooltipMode( byval hOptionButton as HWND ) as long
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pOpt->tip )
end function

' The three delays go through PsTipHost, which STORES them as well as pushing them, so a
' hover time set before a mode switch is still in force after it. Pushing straight at the
' live tooltip loses it at the switch -- and the tip still appears, just on a different
' beat, with nothing reporting the change.
sub PsOptionButton_SetHoverTime( byval hOptionButton as HWND, byval milliseconds as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    PsTipHost_SetDelay( @pOpt->tip, TTDT_INITIAL, milliseconds )
end sub

sub PsOptionButton_SetAutoPopTime( byval hOptionButton as HWND, byval milliseconds as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    PsTipHost_SetDelay( @pOpt->tip, TTDT_AUTOPOP, milliseconds )
end sub

sub PsOptionButton_SetReshowTime( byval hOptionButton as HWND, byval milliseconds as long )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    PsTipHost_SetDelay( @pOpt->tip, TTDT_RESHOW, milliseconds )
end sub


' ========================================================================================
' Callbacks.
' ========================================================================================
sub PsOptionButton_SetPaintCallback( byval hOptionButton as HWND, byval usersub as OPT_PaintCallbackSub )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->PaintCallback = usersub
    if pOpt->hWin then InvalidateRect( pOpt->hWin, NULL, TRUE )
end sub

sub PsOptionButton_SetMessageCallback( byval hOptionButton as HWND, byval userfunc as OPT_MessageCallbackFunc )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->MessageCallback = userfunc
end sub

sub PsOptionButton_SetTooltipCallback( byval hOptionButton as HWND, byval userfunc as OPT_TooltipCallbackFunc )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->TooltipCallback = userfunc
end sub

sub PsOptionButton_SetCheckChangedCallback( byval hOptionButton as HWND, byval usersub as OPT_CheckChangedCallbackSub )
    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then exit sub
    pOpt->CheckChangedCallback = usersub
end sub


' ========================================================================================
' Which rect does this part name? Shared by the two probes and nothing else, but factored out so
' their bodies stay about pixels rather than about rect selection.
'
' OPT_PART_BOX gives the declared CELL, not the inscribed circle -- it is the region a host
' painter is entitled to draw in, so it is the region worth measuring for a flood.
' ========================================================================================
private function PsOptionButton_PartRect( byval pOpt as PSOPTIONBUTTON ptr, byval nPart as long ) as RECT
    dim as RECT rc
    SetRectEmpty( @rc )
    if pOpt = 0 then return rc
    select case nPart
    case OPT_PART_BOX
        rc = pOpt->rcBox
    case OPT_PART_TEXT
        rc = pOpt->rcText
    case else
        rc = pOpt->rcContent
    end select
    return rc
end function


' ========================================================================================
' Render the control OFFSCREEN with its current painter and measure one of its rects two ways:
' the number of distinct colours in it (capped at 64) and an FNV-1a hash of its pixels. Both
' public probes are thin wrappers over this, so they cannot disagree about what was rendered.
'
' THREE THINGS HERE ARE LOAD-BEARING, and all three come from write-ups in Learnings.md:
'
'   1. The PAINTINFO is filled by PsOptionButton_FillPaintInfo -- the SAME routine WM_PAINT
'      uses. A version of this probe elsewhere in the family hand-built the struct and left
'      rcVisual zeroed; PaintRectFactory bails on a negative extent, so the focus-ring call
'      became a no-op and the A/B came back identical. A probe that skips the code path it
'      exists to measure is worse than no probe.
'   2. isFocused is forced TRUE. The ring is the step the flood defect lives on -- it is drawn
'      last, over the largest rect -- so a probe run on an unfocused control measures nothing.
'   3. The buffer is given a CACHED memDC (the four-argument BeginDoubleBuffer), so
'      EndDoubleBuffer flushes GDI+ and blits WITHOUT deleting the bitmap underneath us. GDI+
'      batches: reading the bits before that flush would sample a surface the shapes have not
'      reached yet.
'
' Returns FALSE (and leaves both outputs at 0) if the control has no geometry yet or the part
' rect is empty.
' ========================================================================================
private function PsOptionButton_ProbePart( _
            byval hOptionButton as HWND, _
            byval nPart         as long, _
            byref nTones        as long, _
            byref nHash         as ulong _
            ) as boolean

    nTones = 0
    nHash  = 0

    dim as PSOPTIONBUTTON ptr pOpt = GetOptionButtonPointer(hOptionButton)
    if pOpt = 0 then return false
    if pOpt->bLayoutDirty then pOpt->LayoutOptionButton()
    if pOpt->bLayoutDirty then return false          ' no client area yet

    dim as RECT rcClient
    GetClientRect( hOptionButton, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return false

    dim as RECT rcPart = PsOptionButton_PartRect( pOpt, nPart )
    if IsRectEmpty( @rcPart ) then return false

    ' Clamp to the client: rcVisual-derived rects can legitimately extend past it when the host
    ' sized the control smaller than ideal, and the DIB only covers the client.
    if rcPart.left   < 0  then rcPart.left   = 0
    if rcPart.top    < 0  then rcPart.top    = 0
    if rcPart.right  > cw then rcPart.right  = cw
    if rcPart.bottom > ch then rcPart.bottom = ch
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return false

    dim as HDC hRef = GetDC( hOptionButton )
    if hRef = 0 then return false
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( hOptionButton, hRef )
        return false
    end if

    ' 32bpp top-down, matching what PsBufferPaint.SetupBitmap builds, so the pixels we read are
    ' laid out the way the renderer wrote them.
    dim as BITMAPINFO bmi
    with bmi.bmiHeader
        .biSize        = sizeof(BITMAPINFOHEADER)
        .biWidth       = cw
        .biHeight      = -ch
        .biPlanes      = 1
        .biBitCount    = 32
        .biCompression = BI_RGB
    end with
    dim as any ptr pBits
    dim as HBITMAP hbm = CreateDIBSection( hRef, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )
    ReleaseDC( hOptionButton, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return false
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    dim as boolean bSavedFocus = pOpt->isFocused
    pOpt->isFocused = true

    scope
        dim as PsBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( hOptionButton, hMem, rcClient, hMem )

        dim as long nMood = PsOptionButton_ResolveMood( pOpt->isEnabled, pOpt->isPressed, pOpt->isHot )
        dim as COLORREF clrFill, clrFore, clrRing, clrDot
        PsOptionButton_ResolveColors( @pOpt->colors, nMood, pOpt->isChecked, clrFill, clrFore, clrRing, clrDot )
        b.SetBackColor( clrFill )
        b.PaintClientRect()

        dim p as PSOPTIONBUTTON_PAINTINFO
        PsOptionButton_FillPaintInfo( pOpt, hOptionButton, @b, @p )

        if pOpt->PaintCallback then
            pOpt->PaintCallback( @p )
        else
            PsOptionButton_RenderDefault( pOpt, @b, @p )
        end if

        b.EndDoubleBuffer()
    end scope

    pOpt->isFocused = bSavedFocus

    ' Count distinct tones and hash the same pixels in one pass. The alpha byte is masked off:
    ' GDI text leaves it at 0 and GDI+ shapes leave it at 255, so an unmasked compare would
    ' report two tones for one colour -- and would make the hash depend on which renderer last
    ' touched a pixel rather than on what colour it is.
    dim tones(0 to 63) as ulong
    dim as long nSeen = 0
    dim as ulong h = &h811C9DC5          ' FNV-1a 32-bit offset basis
    const FNV_PRIME as ulong = &h01000193
    dim as ulong ptr px = cast( ulong ptr, pBits )

    for y as long = rcPart.top to rcPart.bottom - 1
        for x as long = rcPart.left to rcPart.right - 1
            dim as ulong c = px[(y * cw) + x] and &h00FFFFFF

            dim as boolean bFound = false
            for t as long = 0 to nSeen - 1
                if tones(t) = c then bFound = true : exit for
            next
            if bFound = false then
                if nSeen < 64 then
                    tones(nSeen) = c
                    nSeen += 1
                end if
            end if

            h = (h xor (c and &hFF)) * FNV_PRIME
            h = (h xor ((c shr 8) and &hFF)) * FNV_PRIME
            h = (h xor ((c shr 16) and &hFF)) * FNV_PRIME
        next
    next

    SelectObject( hMem, hOldBmp )
    DeleteObject( hbm )
    DeleteDC( hMem )

    nTones = nSeen
    nHash  = h
    return true
end function


' See the header in PsOptionButton.bi. A part wiped by a PaintBorderRect fill is literally ONE
' tone, so the useful signal is the count LEAVING 1 -- not the count being large.
function PsOptionButton_CountRenderedTones( byval hOptionButton as HWND, byval nPart as long ) as long
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsOptionButton_ProbePart( hOptionButton, nPart, nTones, nHash ) = false then return 0
    return nTones
end function

' See the header in PsOptionButton.bi. Compare two hashes of the SAME part taken in two different
' states: equal means the state change never reached the surface. It can only prove difference,
' never correctness. (0 is also the failure return; an FNV-1a of a non-empty region landing
' exactly on 0 is possible in principle and has been accepted rather than worked around.)
function PsOptionButton_HashRenderedPart( byval hOptionButton as HWND, byval nPart as long ) as ulong
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsOptionButton_ProbePart( hOptionButton, nPart, nTones, nHash ) = false then return 0
    return nHash
end function


#include once "PsOptionButton_SelfTest.inc"
