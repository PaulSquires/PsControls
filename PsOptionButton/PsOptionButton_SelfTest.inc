'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsOptionButton_SelfTest.inc  --  gated on PSOPTIONBUTTON_SELFTEST=1
''
''  Included at the end of PsOptionButton.inc, so it is the SAME module and can reach the
''  private internals (PsOptionButton_ClearHot, PsOptionButton_ApplyCheck, the registry).
''  That is deliberate: several of the invariants worth asserting here are not reachable
''  through the public API at all.
''
''  THREE RULES THE ASSERTIONS THEMSELVES FOLLOW, each from a recorded false-confidence
''  incident in C:\dev\Learnings.md:
''
''    1. ASSERT PRECONDITIONS, DO NOT SKIP SILENTLY. A skipped assertion reads as a pass in the
''       failure count. Group assertions are naturally conditional ("if the group has 2+
''       members"), so the precondition is asserted rather than tested. THE ASSERTION COUNT IS
''       THE ORACLE, not the failure count -- compare it against the recorded baseline.
''    2. FORCE LAZY DERIVATION BEFORE ASSERTING IT. Reading a derived rect without calling
''       LayoutOptionButton() first asserts the value from before the change under test. If a
''       case only passes WITHOUT the forced layout, the derivation is broken.
''    3. ISOLATE WHAT THE CASE NAMES. PsCheckBox shipped an assertion called "the checked state
''       reaches the pixels" that survived freezing glyph resolution outright, because the state
''       also moved a colour. The assertion was true; its NAME was not. The render section below
''       therefore runs its state probe twice, the second time with the ring colours flattened.
''

dim shared as long gOPT_TestCount
dim shared as long gOPT_FailCount

' Notification recorder. Every test button shares one callback, so the ORDER of the callbacks
' across DIFFERENT controls is observable -- which is the whole point, since "the loser fires
' first" is a cross-control claim and no per-control counter could see it.
const OPT_MAXNOTIFY = 32
dim shared as long    gOPT_NotifyCount
dim shared as HWND    gOPT_NotifyWnd(0 to OPT_MAXNOTIFY - 1)
dim shared as boolean gOPT_NotifyVal(0 to OPT_MAXNOTIFY - 1)

private sub OPTTest_OnCheckChanged( byval hOptionButton as HWND, byval isChecked as boolean )
    if gOPT_NotifyCount < OPT_MAXNOTIFY then
        gOPT_NotifyWnd( gOPT_NotifyCount ) = hOptionButton
        gOPT_NotifyVal( gOPT_NotifyCount ) = isChecked
    end if
    gOPT_NotifyCount += 1
end sub

private sub OPTTest_ResetNotify()
    gOPT_NotifyCount = 0
    for i as long = 0 to OPT_MAXNOTIFY - 1
        gOPT_NotifyWnd(i) = 0
        gOPT_NotifyVal(i) = false
    next
end sub

' bOK, never "ok": GDI+'s Status enum defines Ok = 0 in namespace AfxNova, and every host says
' "using AfxNova", so an identifier called ok is a duplicate definition.
private sub OPTTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gOPT_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gOPT_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

private function OPTTest_RectStr( byref rc as RECT ) as string
    return "(" & str(rc.left) & "," & str(rc.top) & ")-(" & str(rc.right) & "," & str(rc.bottom) & ")"
end function

' Does this window carry WS_TABSTOP right now?
private function OPTTest_HasTabStop( byval h as HWND ) as boolean
    if IsWindow(h) = 0 then return false
    return ((GetWindowLongPtr( h, GWL_STYLE ) and WS_TABSTOP) <> 0)
end function

' How many members of a group carry WS_TABSTOP? The invariant says exactly one, always.
private function OPTTest_TabStopCount( byval hParent as HWND, byval nGroupID as long ) as long
    dim as long n = 0
    dim as long nCount = PsOptionButton_GroupCountOf( hParent, nGroupID )
    for i as long = 0 to nCount - 1
        dim as PSOPTIONBUTTON ptr p = PsOptionButton_GroupAt( hParent, nGroupID, i )
        if p then
            if OPTTest_HasTabStop( p->hWin ) then n += 1
        end if
    next
    return n
end function

' Ask the control what it would tell IsDialogMessage about one key.
private function OPTTest_DlgCodeFor( byval h as HWND, byval vk as long ) as LRESULT
    dim as MSG m
    m.hwnd    = h
    m.message = WM_KEYDOWN
    m.wParam  = vk
    m.lParam  = 0
    return SendMessageW( h, WM_GETDLGCODE, vk, cast(LPARAM, @m) )
end function

' Create a test button, size it, pin every layout input so the arithmetic below is
' DPI-independent, and give it the shared notify callback.
private function OPTTest_MakeButton( _
            byval hParent  as HWND, _
            byval nID      as long, _
            byval nGroupID as long, _
            byval sCaption as string, _
            byval cx       as long = 240, _
            byval cy       as long = 40 _
            ) as HWND

    dim as HWND h = PsOptionButton_Create( hParent, nID, nGroupID )
    if h = 0 then return 0
    SetWindowPos( h, 0, 0, 0, cx, cy, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    PsOptionButton_SetPadding( h, 10, 5, 10, 5 )
    PsOptionButton_SetBoxGap( h, 8 )
    PsOptionButton_SetBoxSize( h, 16, 16 )
    PsOptionButton_SetFocusRing( h, 2, 1 )          ' ringPad = 3
    PsOptionButton_SetFocusCurvature( h, 8 )
    PsOptionButton_SetRingThickness( h, 1 )
    PsOptionButton_SetDotSize( h, 6 )
    PsOptionButton_SetText( h, sCaption )
    PsOptionButton_SetCheckChangedCallback( h, @OPTTest_OnCheckChanged )
    return h
end function


' ========================================================================================
sub PsOptionButton_RunSelfTest( byval hWndParent as HWND )

    if environ("PSOPTIONBUTTON_SELFTEST") <> "1" then exit sub

    print "=== PsOptionButton self-test ==="
    gOPT_TestCount = 0
    gOPT_FailCount = 0

    const CAPTION = "Test Caption"
    const RINGPAD = 3                               ' nFocusGap 2 + nFocusThick 1

    ' The lone button every geometry case runs against. Group 9000 so nothing here can collide
    ' with the demo's own groups.
    dim as HWND h = OPTTest_MakeButton( hWndParent, 9901, 9000, CAPTION )
    if h = 0 then
        OPTTest_Check( "self-test could create a control", false, "PsOptionButton_Create returned 0" )
        print "=== PsOptionButton: ABORTED ==="
        exit sub
    end if
    dim as PSOPTIONBUTTON ptr pC = GetOptionButtonPointer( h )
    OPTTest_Check( "the registry resolves the handle we just created", pC <> 0, "" )
    if pC = 0 then
        print "=== PsOptionButton: ABORTED ==="
        exit sub
    end if
    OPTTest_Check( "the signature survived construction", _
                   pC->dwSignature = PSOPTIONBUTTON_SIGNATURE, "&h" & hex(pC->dwSignature) )
    OPTTest_Check( "a foreign HWND resolves to NULL, not to garbage", _
                   GetOptionButtonPointer( hWndParent ) = 0, "asked for the parent form" )

    dim as string sD


    ' ---- 1. ideal size is valid before the control has ever been sized -------------------
    scope
        dim as HWND h0 = PsOptionButton_Create( hWndParent, 9902, 9001 )
        PsOptionButton_SetText( h0, CAPTION )
        dim as long w0, hh0
        PsOptionButton_GetIdealSize( h0, w0, hh0 )
        OPTTest_Check( "ideal size is non-zero before first sizing", _
                       (w0 > 0) andalso (hh0 > 0), "ideal=" & str(w0) & "x" & str(hh0) )
        DestroyWindow( h0 )
    end scope


    ' ---- 2. the ideal size decomposes exactly ---------------------------------------------
    scope
        dim as long w, hh
        PsOptionButton_GetIdealSize( h, w, hh )
        dim as long expectW = (2 * RINGPAD) + 10 + 16 + 8 + pC->nTextWidth + 10
        sD = "ideal=" & str(w) & " expect=" & str(expectW) & " textW=" & str(pC->nTextWidth)
        OPTTest_Check( "ideal width = 2*ring + padL + box + gap + text + padR", (w = expectW), sD )

        dim as long contentH = 16
        if pC->nTextHeight > contentH then contentH = pC->nTextHeight
        dim as long expectH = (2 * RINGPAD) + 5 + contentH + 5
        sD = "ideal=" & str(hh) & " expect=" & str(expectH) & " textH=" & str(pC->nTextHeight)
        OPTTest_Check( "ideal height = 2*ring + padT + max(text,box) + padB", (hh = expectH), sD )
    end scope


    ' ---- 3. the gap is charged ONLY when there is a caption -------------------------------
    scope
        dim as long wWith, hWith, wNone, hNone
        PsOptionButton_GetIdealSize( h, wWith, hWith )
        PsOptionButton_SetText( h, "" )
        PsOptionButton_GetIdealSize( h, wNone, hNone )
        dim as long expectNone = (2 * RINGPAD) + 10 + 16 + 10
        sD = "none=" & str(wNone) & " expect=" & str(expectNone)
        OPTTest_Check( "no caption: width charges no gap and no text", (wNone = expectNone), sD )

        PsOptionButton_SetText( h, CAPTION )
        dim as long wBack, hBack
        PsOptionButton_GetIdealSize( h, wBack, hBack )
        sD = "before=" & str(wWith) & " after=" & str(wBack)
        OPTTest_Check( "restoring the caption restores the width", (wBack = wWith), sD )
    end scope


    ' ---- 4. contentH is a real max(), both branches ---------------------------------------
    scope
        ' Box SHORTER than the text -> the text wins.
        PsOptionButton_SetBoxSize( h, 16, 8 )
        dim as long w1, h1
        PsOptionButton_GetIdealSize( h, w1, h1 )
        dim as long expect1 = (2 * RINGPAD) + 5 + pC->nTextHeight + 5
        sD = "h=" & str(h1) & " expect=" & str(expect1) & " textH=" & str(pC->nTextHeight)
        OPTTest_Check( "contentH: a short box loses to the text height", (h1 = expect1), sD )

        ' Box TALLER than the text -> the box wins.
        dim as long nTall = pC->nTextHeight + 20
        PsOptionButton_SetBoxSize( h, 16, nTall )
        dim as long w2, h2
        PsOptionButton_GetIdealSize( h, w2, h2 )
        dim as long expect2 = (2 * RINGPAD) + 5 + nTall + 5
        sD = "h=" & str(h2) & " expect=" & str(expect2) & " boxH=" & str(nTall)
        OPTTest_Check( "contentH: a tall box beats the text height", (h2 = expect2), sD )

        PsOptionButton_SetBoxSize( h, 16, 16 )
    end scope


    ' ---- 5. flipping the box side does NOT change the ideal size --------------------------
    scope
        dim as long wL, hL, wR, hR
        PsOptionButton_SetBoxAlign( h, OPT_BOXALIGN_LEFT )
        PsOptionButton_GetIdealSize( h, wL, hL )
        PsOptionButton_SetBoxAlign( h, OPT_BOXALIGN_RIGHT )
        PsOptionButton_GetIdealSize( h, wR, hR )
        sD = "left=" & str(wL) & "x" & str(hL) & " right=" & str(wR) & "x" & str(hR)
        OPTTest_Check( "the mirror is exact: ideal size is side-independent", _
                       (wL = wR) andalso (hL = hR), sD )
        PsOptionButton_SetBoxAlign( h, OPT_BOXALIGN_LEFT )
    end scope


    ' ---- 6. BOXALIGN_LEFT placement -------------------------------------------------------
    scope
        dim as RECT rcCon, rcBox, rcTxt
        PsOptionButton_GetContentRect( h, rcCon )
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetTextRect( h, rcTxt )

        sD = "content=" & OPTTest_RectStr(rcCon) & " box=" & OPTTest_RectStr(rcBox)
        OPTTest_Check( "LEFT: box is pinned one padL from the content edge", _
                       (rcBox.left - rcCon.left) = 10, sD )
        OPTTest_Check( "LEFT: box keeps its declared width", _
                       (rcBox.right - rcBox.left) = 16, "w=" & str(rcBox.right - rcBox.left) )
        sD = "box=" & OPTTest_RectStr(rcBox) & " text=" & OPTTest_RectStr(rcTxt)
        OPTTest_Check( "LEFT: the span starts one gap after the box", _
                       (rcTxt.left - rcBox.right) = 8, sD )
        OPTTest_Check( "LEFT: the span runs to the far padding", _
                       (rcCon.right - rcTxt.right) = 10, sD )
    end scope


    ' ---- 7. BOXALIGN_RIGHT is the exact mirror ---------------------------------------------
    scope
        PsOptionButton_SetBoxAlign( h, OPT_BOXALIGN_RIGHT )
        dim as RECT rcCon, rcBox, rcTxt
        PsOptionButton_GetContentRect( h, rcCon )
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetTextRect( h, rcTxt )

        sD = "content=" & OPTTest_RectStr(rcCon) & " box=" & OPTTest_RectStr(rcBox)
        OPTTest_Check( "RIGHT: box is pinned one padR from the far content edge", _
                       (rcCon.right - rcBox.right) = 10, sD )
        OPTTest_Check( "RIGHT: box keeps its declared width", _
                       (rcBox.right - rcBox.left) = 16, "w=" & str(rcBox.right - rcBox.left) )
        sD = "box=" & OPTTest_RectStr(rcBox) & " text=" & OPTTest_RectStr(rcTxt)
        OPTTest_Check( "RIGHT: the span ends one gap before the box", _
                       (rcBox.left - rcTxt.right) = 8, sD )
        OPTTest_Check( "RIGHT: the span starts at the near padding", _
                       (rcTxt.left - rcCon.left) = 10, sD )
        PsOptionButton_SetBoxAlign( h, OPT_BOXALIGN_LEFT )
    end scope


    ' ---- 8. a long caption never moves the box, in EITHER alignment ------------------------
    scope
        dim as RECT rcBoxShort, rcBoxLong
        PsOptionButton_GetBoxRect( h, rcBoxShort )
        PsOptionButton_SetText( h, "A very much longer caption than the original one" )
        PsOptionButton_GetBoxRect( h, rcBoxLong )
        sD = "short=" & OPTTest_RectStr(rcBoxShort) & " long=" & OPTTest_RectStr(rcBoxLong)
        OPTTest_Check( "LEFT: a longer caption does not move the box", _
                       EqualRect( @rcBoxShort, @rcBoxLong ) <> 0, sD )

        PsOptionButton_SetBoxAlign( h, OPT_BOXALIGN_RIGHT )
        PsOptionButton_SetText( h, CAPTION )
        PsOptionButton_GetBoxRect( h, rcBoxShort )
        PsOptionButton_SetText( h, "A very much longer caption than the original one" )
        PsOptionButton_GetBoxRect( h, rcBoxLong )
        sD = "short=" & OPTTest_RectStr(rcBoxShort) & " long=" & OPTTest_RectStr(rcBoxLong)
        OPTTest_Check( "RIGHT: a longer caption does not move the box", _
                       EqualRect( @rcBoxShort, @rcBoxLong ) <> 0, sD )

        PsOptionButton_SetBoxAlign( h, OPT_BOXALIGN_LEFT )
        PsOptionButton_SetText( h, CAPTION )
    end scope


    ' ---- 9. vertical centring --------------------------------------------------------------
    scope
        dim as RECT rcCon, rcBox, rcTxt
        PsOptionButton_GetContentRect( h, rcCon )
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetTextRect( h, rcTxt )
        dim as long padAbove = rcBox.top - rcCon.top
        dim as long padBelow = rcCon.bottom - rcBox.bottom
        sD = "above=" & str(padAbove) & " below=" & str(padBelow)
        OPTTest_Check( "the box is v-centred in the content (within rounding)", _
                       abs(padAbove - padBelow) <= 1, sD )
        dim as long tAbove = rcTxt.top - rcCon.top
        dim as long tBelow = rcCon.bottom - rcTxt.bottom
        sD = "above=" & str(tAbove) & " below=" & str(tBelow)
        OPTTest_Check( "the caption is v-centred in the content (within rounding)", _
                       abs(tAbove - tBelow) <= 1, sD )
    end scope


    ' ---- 10. an absent caption gives an EMPTY span; the circle never vanishes ---------------
    scope
        PsOptionButton_SetText( h, "" )
        dim as RECT rcTxt, rcBox, rcCirc
        dim as boolean bTextOK = PsOptionButton_GetTextRect( h, rcTxt )
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetCircleRect( h, rcCirc )
        sD = "text=" & OPTTest_RectStr(rcTxt)
        OPTTest_Check( "no caption: rcText is EMPTY, not zero-width at an edge", _
                       (bTextOK = false) andalso (IsRectEmpty(@rcTxt) <> 0), sD )
        OPTTest_Check( "no caption: rcBox is still non-empty", IsRectEmpty(@rcBox) = 0, OPTTest_RectStr(rcBox) )
        OPTTest_Check( "no caption: rcCircle is still non-empty", IsRectEmpty(@rcCirc) = 0, OPTTest_RectStr(rcCirc) )
        PsOptionButton_SetText( h, CAPTION )
    end scope


    ' ---- 11. visual = content +/- ringPad, and focus moves nothing --------------------------
    scope
        dim as RECT rcCon, rcVis
        PsOptionButton_GetContentRect( h, rcCon )
        PsOptionButton_GetVisualRect( h, rcVis )
        sD = "content=" & OPTTest_RectStr(rcCon) & " visual=" & OPTTest_RectStr(rcVis)
        OPTTest_Check( "visual = content inflated by the ring band", _
                       ((rcCon.left - rcVis.left) = RINGPAD) andalso _
                       ((rcVis.right - rcCon.right) = RINGPAD), sD )

        dim as RECT rcBoxUnfocused, rcBoxFocused
        PsOptionButton_GetBoxRect( h, rcBoxUnfocused )
        pC->isFocused = true
        pC->bLayoutDirty = true
        PsOptionButton_GetBoxRect( h, rcBoxFocused )
        pC->isFocused = false
        pC->bLayoutDirty = true
        sD = "unfocused=" & OPTTest_RectStr(rcBoxUnfocused) & " focused=" & OPTTest_RectStr(rcBoxFocused)
        OPTTest_Check( "the ring band is reserved always, so focus moves nothing", _
                       EqualRect( @rcBoxUnfocused, @rcBoxFocused ) <> 0, sD )
    end scope


    ' ---- 12. overflow: the span degenerates, the circle keeps its size ----------------------
    scope
        dim as long wIdeal, hIdeal
        PsOptionButton_GetIdealSize( h, wIdeal, hIdeal )
        ' Narrower than padding + box + padding: there is no span left at all.
        SetWindowPos( h, 0, 0, 0, (2 * RINGPAD) + 10 + 16 + 10, hIdeal, _
                      SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
        dim as RECT rcBox, rcTxt
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetTextRect( h, rcTxt )
        sD = "box=" & OPTTest_RectStr(rcBox) & " text=" & OPTTest_RectStr(rcTxt)
        OPTTest_Check( "overflow: the span degenerates rather than going negative", _
                       (rcTxt.right >= rcTxt.left), sD )
        OPTTest_Check( "overflow: the box keeps its declared size", _
                       (rcBox.right - rcBox.left) = 16, "w=" & str(rcBox.right - rcBox.left) )

        dim as long wAfter, hAfter
        PsOptionButton_GetIdealSize( h, wAfter, hAfter )
        sD = "before=" & str(wIdeal) & " after=" & str(wAfter)
        OPTTest_Check( "overflow: the IDEAL size is unaffected by a starved client", _
                       (wAfter = wIdeal), sD )
        SetWindowPos( h, 0, 0, 0, 240, 40, SWP_NOZORDER or SWP_NOMOVE or SWP_NOACTIVATE )
    end scope


    ' ---- 13. THE CIRCLE IS INSCRIBED, and stays round in a non-square cell ------------------
    scope
        PsOptionButton_SetBoxSize( h, 16, 16 )
        dim as RECT rcBox, rcCirc
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetCircleRect( h, rcCirc )
        OPTTest_Check( "square cell: circle == cell", _
                       EqualRect( @rcBox, @rcCirc ) <> 0, _
                       "box=" & OPTTest_RectStr(rcBox) & " circ=" & OPTTest_RectStr(rcCirc) )

        ' A WIDE cell. This is the case that separates a radio button from a stadium: a painter
        ' that stroked rcBox would draw a rounded rectangle here.
        PsOptionButton_SetBoxSize( h, 40, 16 )
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetCircleRect( h, rcCirc )
        dim as long cwc = rcCirc.right - rcCirc.left
        dim as long chc = rcCirc.bottom - rcCirc.top
        sD = "box=" & OPTTest_RectStr(rcBox) & " circ=" & OPTTest_RectStr(rcCirc)
        OPTTest_Check( "wide cell: the circle is SQUARE", (cwc = chc), sD & " " & str(cwc) & "x" & str(chc) )
        OPTTest_Check( "wide cell: the circle takes the SHORTER side", (cwc = 16), "side=" & str(cwc) )
        OPTTest_Check( "wide cell: the circle is h-centred in the cell", _
                       abs( (rcCirc.left - rcBox.left) - (rcBox.right - rcCirc.right) ) <= 1, sD )

        ' A TALL cell, the mirrored case. Learnings.md: a single-sided test of a mirrored pair
        ' passes with the mirror inverted.
        PsOptionButton_SetBoxSize( h, 16, 40 )
        PsOptionButton_GetBoxRect( h, rcBox )
        PsOptionButton_GetCircleRect( h, rcCirc )
        cwc = rcCirc.right - rcCirc.left
        chc = rcCirc.bottom - rcCirc.top
        sD = "box=" & OPTTest_RectStr(rcBox) & " circ=" & OPTTest_RectStr(rcCirc)
        OPTTest_Check( "tall cell: the circle is SQUARE", (cwc = chc), sD & " " & str(cwc) & "x" & str(chc) )
        OPTTest_Check( "tall cell: the circle takes the SHORTER side", (cwc = 16), "side=" & str(cwc) )
        OPTTest_Check( "tall cell: the circle is v-centred in the cell", _
                       abs( (rcCirc.top - rcBox.top) - (rcBox.bottom - rcCirc.bottom) ) <= 1, sD )

        PsOptionButton_SetBoxSize( h, 16, 16 )
    end scope


    ' ---- 14. the dot: both branches, and it never escapes the disc --------------------------
    scope
        ' Declared branch.
        PsOptionButton_SetDotSize( h, 6 )
        dim as RECT rcCirc, rcDot
        PsOptionButton_GetCircleRect( h, rcCirc )
        PsOptionButton_GetDotRect( h, rcDot )
        OPTTest_Check( "declared dot: diameter is exactly what was asked for", _
                       (rcDot.right - rcDot.left) = 6, "d=" & str(rcDot.right - rcDot.left) )
        sD = "circ=" & OPTTest_RectStr(rcCirc) & " dot=" & OPTTest_RectStr(rcDot)
        OPTTest_Check( "declared dot: centred in the circle", _
                       (abs( (rcDot.left - rcCirc.left) - (rcCirc.right - rcDot.right) ) <= 1) andalso _
                       (abs( (rcDot.top - rcCirc.top) - (rcCirc.bottom - rcDot.bottom) ) <= 1), sD )

        ' Derived branch: 0 means "a percentage of the circle".
        PsOptionButton_SetDotSize( h, 0 )
        PsOptionButton_GetCircleRect( h, rcCirc )
        PsOptionButton_GetDotRect( h, rcDot )
        dim as long nSide = rcCirc.right - rcCirc.left
        dim as long expectDot = (nSide * PSOPTIONBUTTON_DOTRATIO_PCT) \ 100
        if expectDot < 2 then expectDot = 2
        sD = "side=" & str(nSide) & " dot=" & str(rcDot.right - rcDot.left) & " expect=" & str(expectDot)
        OPTTest_Check( "derived dot: diameter is the ratio of the circle", _
                       (rcDot.right - rcDot.left) = expectDot, sD )

        ' AND IT FOLLOWS THE CELL. This is the reason 0 is the default: a declared dot silently
        ' decouples when a host scales the box up.
        PsOptionButton_SetBoxSize( h, 32, 32 )
        dim as RECT rcCirc2, rcDot2
        PsOptionButton_GetCircleRect( h, rcCirc2 )
        PsOptionButton_GetDotRect( h, rcDot2 )
        sD = "16px dot=" & str(rcDot.right - rcDot.left) & "  32px dot=" & str(rcDot2.right - rcDot2.left)
        OPTTest_Check( "derived dot: grows with the box, which is why 0 is the default", _
                       (rcDot2.right - rcDot2.left) > (rcDot.right - rcDot.left), sD )

        ' Clamped: a dot larger than its disc would paint outside the circle.
        PsOptionButton_SetDotSize( h, 999 )
        dim as RECT rcCirc3, rcDot3
        PsOptionButton_GetCircleRect( h, rcCirc3 )
        PsOptionButton_GetDotRect( h, rcDot3 )
        sD = "circ=" & OPTTest_RectStr(rcCirc3) & " dot=" & OPTTest_RectStr(rcDot3)
        OPTTest_Check( "an over-large dot is clamped to the disc", _
                       (rcDot3.right - rcDot3.left) <= (rcCirc3.right - rcCirc3.left), sD )

        PsOptionButton_SetBoxSize( h, 16, 16 )
        PsOptionButton_SetDotSize( h, 6 )
    end scope


    ' ---- 15. neither pen is pre-scaled at Create --------------------------------------------
    scope
        ' PaintRoundOutline DPI-scales the pen it is handed. If Create scaled these too they
        ' would be doubled at any DPI above 100%, and at 100% nothing would show it -- so this
        ' asserts the RAW value survives, which is true at every DPI.
        dim as HWND hRaw = PsOptionButton_Create( hWndParent, 9903, 9002 )
        dim as PSOPTIONBUTTON ptr pRaw = GetOptionButtonPointer( hRaw )
        OPTTest_Check( "Create leaves nRingThick RAW (the painter scales it)", _
                       pRaw->nRingThick = PSOPTIONBUTTON_DEFAULT_RINGTHICK, _
                       "ring=" & str(pRaw->nRingThick) )
        OPTTest_Check( "Create leaves nFocusThick RAW (the painter scales it)", _
                       pRaw->nFocusThick = PSOPTIONBUTTON_DEFAULT_FOCUSTHICK, _
                       "focus=" & str(pRaw->nFocusThick) )
        DestroyWindow( hRaw )
    end scope


    ' ---- 16. the mood truth table, all eight combinations ------------------------------------
    scope
        OPTTest_Check( "mood: disabled beats pressed", _
                       PsOptionButton_ResolveMood( false, true, false ) = OPT_MOOD_DISABLED, "" )
        OPTTest_Check( "mood: disabled beats hot", _
                       PsOptionButton_ResolveMood( false, false, true ) = OPT_MOOD_DISABLED, "" )
        OPTTest_Check( "mood: disabled beats both", _
                       PsOptionButton_ResolveMood( false, true, true ) = OPT_MOOD_DISABLED, "" )
        OPTTest_Check( "mood: disabled alone", _
                       PsOptionButton_ResolveMood( false, false, false ) = OPT_MOOD_DISABLED, "" )
        OPTTest_Check( "mood: pressed beats hot", _
                       PsOptionButton_ResolveMood( true, true, true ) = OPT_MOOD_PRESSED, "" )
        OPTTest_Check( "mood: pressed alone", _
                       PsOptionButton_ResolveMood( true, true, false ) = OPT_MOOD_PRESSED, "" )
        OPTTest_Check( "mood: hot alone", _
                       PsOptionButton_ResolveMood( true, false, true ) = OPT_MOOD_HOT, "" )
        OPTTest_Check( "mood: idle", _
                       PsOptionButton_ResolveMood( true, false, false ) = OPT_MOOD_IDLE, "" )
    end scope


    ' ---- 17. isChecked moves the RING colour and nothing else, in all four moods --------------
    scope
        ' Distinguishable sentinels rather than the shipped defaults, so a case cannot pass by
        ' two different fields happening to hold the same colour.
        dim as PSOPTIONBUTTON_COLORS c
        c.BackColor  = BGR(0,0,10) : c.BackColorHot = BGR(0,0,11)
        c.BackColorPressed = BGR(0,0,12) : c.BackColorDisabled = BGR(0,0,13)
        c.ForeColor  = BGR(0,0,20) : c.ForeColorHot = BGR(0,0,21)
        c.ForeColorPressed = BGR(0,0,22) : c.ForeColorDisabled = BGR(0,0,23)
        c.RingColor  = BGR(0,0,30) : c.RingColorHot = BGR(0,0,31)
        c.RingColorPressed = BGR(0,0,32) : c.RingColorDisabled = BGR(0,0,33)
        c.RingColorChecked = BGR(0,0,40) : c.RingColorCheckedHot = BGR(0,0,41)
        c.RingColorCheckedPressed = BGR(0,0,42) : c.RingColorCheckedDisabled = BGR(0,0,43)
        c.DotColor = BGR(0,0,50) : c.DotColorHot = BGR(0,0,51)
        c.DotColorPressed = BGR(0,0,52) : c.DotColorDisabled = BGR(0,0,53)

        for m as long = OPT_MOOD_IDLE to OPT_MOOD_DISABLED
            dim as COLORREF bkU, fgU, rgU, dtU
            dim as COLORREF bkC, fgC, rgC, dtC
            PsOptionButton_ResolveColors( @c, m, false, bkU, fgU, rgU, dtU )
            PsOptionButton_ResolveColors( @c, m, true,  bkC, fgC, rgC, dtC )
            sD = "mood=" & str(m) & " ring " & str(rgU) & "->" & str(rgC)
            OPTTest_Check( "checked changes the ring colour in this mood", (rgU <> rgC), sD )
            OPTTest_Check( "checked leaves the BACK colour alone in this mood", (bkU = bkC), sD )
            OPTTest_Check( "checked leaves the FORE colour alone in this mood", (fgU = fgC), sD )
            OPTTest_Check( "the dot colour is resolved even when unchecked", (dtU = dtC), sD )
        next
    end scope


    ' ================================================================================
    ' GROUPS.  Everything below here is what makes this control an OPTION button rather
    ' than a round checkbox, and none of it exists in any sibling.
    ' ================================================================================

    ' Group 100: three members. Group 200: two members, same parent, must stay untouched.
    dim as HWND a1 = OPTTest_MakeButton( hWndParent, 9911, 100, "A1" )
    dim as HWND a2 = OPTTest_MakeButton( hWndParent, 9912, 100, "A2" )
    dim as HWND a3 = OPTTest_MakeButton( hWndParent, 9913, 100, "A3" )
    dim as HWND b1 = OPTTest_MakeButton( hWndParent, 9921, 200, "B1" )
    dim as HWND b2 = OPTTest_MakeButton( hWndParent, 9922, 200, "B2" )

    ' PRECONDITION, ASSERTED RATHER THAN ASSUMED. Every case below is conditional on the groups
    ' having been built; a silent skip here would read as a pass in the failure count.
    OPTTest_Check( "precondition: group 100 has three live members", _
                   PsOptionButton_GetGroupCount( a1 ) = 3, _
                   "count=" & str( PsOptionButton_GetGroupCount( a1 ) ) )
    OPTTest_Check( "precondition: group 200 has two live members", _
                   PsOptionButton_GetGroupCount( b1 ) = 2, _
                   "count=" & str( PsOptionButton_GetGroupCount( b1 ) ) )
    OPTTest_Check( "precondition: the two groups are disjoint", _
                   PsOptionButton_GetGroupCount( a1 ) + PsOptionButton_GetGroupCount( b1 ) = 5, "" )


    ' ---- 18. a new group starts with NOTHING checked, which is legal --------------------------
    scope
        OPTTest_Check( "a fresh group has nothing checked", _
                       PsOptionButton_FindChecked( hWndParent, 100 ) = 0, "" )
        OPTTest_Check( "FindCheckedID reports -1 for an unanswered group", _
                       PsOptionButton_FindCheckedID( hWndParent, 100 ) = -1, _
                       "id=" & str( PsOptionButton_FindCheckedID( hWndParent, 100 ) ) )
    end scope


    ' ---- 19. exclusivity, and the second group is untouched -----------------------------------
    scope
        PsOptionButton_SetChecked( a1, true )
        PsOptionButton_SetChecked( b1, true )
        OPTTest_Check( "checking a1 checks a1", PsOptionButton_GetChecked( a1 ), "" )
        OPTTest_Check( "checking b1 does not disturb group 100", _
                       PsOptionButton_FindChecked( hWndParent, 100 ) = a1, "" )

        PsOptionButton_SetChecked( a2, true )
        OPTTest_Check( "checking a2 UNCHECKED a1", PsOptionButton_GetChecked( a1 ) = false, "" )
        OPTTest_Check( "checking a2 checked a2", PsOptionButton_GetChecked( a2 ), "" )
        OPTTest_Check( "a3 was never involved", PsOptionButton_GetChecked( a3 ) = false, "" )
        OPTTest_Check( "GROUP 200 IS UNTOUCHED by anything group 100 did", _
                       PsOptionButton_GetChecked( b1 ), "" )
        OPTTest_Check( "FindChecked agrees with GetChecked", _
                       PsOptionButton_FindChecked( hWndParent, 100 ) = a2, "" )
        OPTTest_Check( "FindCheckedID reports the control id", _
                       PsOptionButton_FindCheckedID( hWndParent, 100 ) = 9912, _
                       "id=" & str( PsOptionButton_FindCheckedID( hWndParent, 100 ) ) )
    end scope


    ' ---- 20. programmatic is SILENT, user action NOTIFIES -------------------------------------
    scope
        OPTTest_ResetNotify()
        PsOptionButton_SetChecked( a3, true )
        OPTTest_Check( "SetChecked is silent even when it moves the check", _
                       gOPT_NotifyCount = 0, "notifies=" & str(gOPT_NotifyCount) )

        OPTTest_ResetNotify()
        PsOptionButton_ClearGroup( a1 )
        OPTTest_Check( "ClearGroup is silent", gOPT_NotifyCount = 0, "notifies=" & str(gOPT_NotifyCount) )
        OPTTest_Check( "ClearGroup leaves nothing checked", _
                       PsOptionButton_FindChecked( hWndParent, 100 ) = 0, "" )
        OPTTest_Check( "ClearGroup did not touch group 200", PsOptionButton_GetChecked( b1 ), "" )
    end scope


    ' ---- 21. BOTH EDGES FIRE, AND THE LOSER FIRES FIRST ---------------------------------------
    scope
        PsOptionButton_SetChecked( a1, true )         ' silent setup
        OPTTest_ResetNotify()
        PsOptionButton_Click( a2 )                    ' the user path

        OPTTest_Check( "a user pick fires exactly twice when it displaces a peer", _
                       gOPT_NotifyCount = 2, "notifies=" & str(gOPT_NotifyCount) )
        if gOPT_NotifyCount = 2 then
            sD = "first=" & iif( gOPT_NotifyWnd(0) = a1, "a1", "a2" ) & _
                 " val=" & str(gOPT_NotifyVal(0)) & _
                 "  second=" & iif( gOPT_NotifyWnd(1) = a2, "a2", "a1" ) & _
                 " val=" & str(gOPT_NotifyVal(1))
            OPTTest_Check( "THE LOSER FIRES FIRST, with isChecked = false", _
                           (gOPT_NotifyWnd(0) = a1) andalso (gOPT_NotifyVal(0) = false), sD )
            OPTTest_Check( "the winner fires second, with isChecked = true", _
                           (gOPT_NotifyWnd(1) = a2) andalso (gOPT_NotifyVal(1) = true), sD )
        else
            OPTTest_Check( "THE LOSER FIRES FIRST, with isChecked = false", false, "wrong notify count" )
            OPTTest_Check( "the winner fires second, with isChecked = true", false, "wrong notify count" )
        end if

        ' The claim the ordering exists to support: the group is never observably double-checked
        ' or empty from inside a callback. Both writes land before the first callback runs, so
        ' asserting the FINAL state is coherent is the observable half of it.
        dim as long nChecked = 0
        for i as long = 0 to PsOptionButton_GetGroupCount( a1 ) - 1
            if PsOptionButton_GetChecked( PsOptionButton_GetGroupMember( a1, i ) ) then nChecked += 1
        next
        OPTTest_Check( "exactly one member is checked after the swap", nChecked = 1, _
                       "checked=" & str(nChecked) )
    end scope


    ' ---- 22. clicking the ALREADY-CHECKED member is a silent no-op -----------------------------
    scope
        ' a2 is checked from the case above.
        OPTTest_ResetNotify()
        PsOptionButton_Click( a2 )
        OPTTest_Check( "clicking the checked member notifies nothing", _
                       gOPT_NotifyCount = 0, "notifies=" & str(gOPT_NotifyCount) )
        OPTTest_Check( "clicking the checked member leaves it checked", _
                       PsOptionButton_GetChecked( a2 ), "" )
        OPTTest_Check( "THE USER CANNOT EMPTY A GROUP", _
                       PsOptionButton_FindChecked( hWndParent, 100 ) = a2, "" )
    end scope


    ' ---- 23. a disabled member refuses the USER but not the HOST --------------------------------
    scope
        PsOptionButton_SetEnabled( a3, false )
        OPTTest_ResetNotify()
        PsOptionButton_Click( a3 )
        OPTTest_Check( "a disabled member refuses a click", _
                       PsOptionButton_GetChecked( a3 ) = false, "" )
        OPTTest_Check( "a refused click notifies nothing", gOPT_NotifyCount = 0, _
                       "notifies=" & str(gOPT_NotifyCount) )

        ' But a host restoring saved settings onto a conditionally-greyed form must still be able
        ' to say which option is selected.
        PsOptionButton_SetChecked( a3, true )
        OPTTest_Check( "SetChecked still works on a disabled member (settings restore)", _
                       PsOptionButton_GetChecked( a3 ), "" )
        PsOptionButton_SetEnabled( a3, true )
        PsOptionButton_SetChecked( a2, true )
    end scope


    ' ---- 24. THE TAB-STOP INVARIANT ------------------------------------------------------------
    scope
        OPTTest_Check( "exactly one member of group 100 carries WS_TABSTOP", _
                       OPTTest_TabStopCount( hWndParent, 100 ) = 1, _
                       "count=" & str( OPTTest_TabStopCount( hWndParent, 100 ) ) )
        OPTTest_Check( "exactly one member of group 200 carries WS_TABSTOP", _
                       OPTTest_TabStopCount( hWndParent, 200 ) = 1, _
                       "count=" & str( OPTTest_TabStopCount( hWndParent, 200 ) ) )
        OPTTest_Check( "the CHECKED member is the one that carries it", _
                       OPTTest_HasTabStop( a2 ), "a2 is checked" )

        ' Site 2: the tab stop follows the check.
        PsOptionButton_SetChecked( a1, true )
        OPTTest_Check( "the tab stop FOLLOWS the check to a1", OPTTest_HasTabStop( a1 ), "" )
        OPTTest_Check( "...and left a2", OPTTest_HasTabStop( a2 ) = false, "" )
        OPTTest_Check( "...and there is still exactly one", _
                       OPTTest_TabStopCount( hWndParent, 100 ) = 1, "" )

        ' Site 3: disabling the holder hands it on.
        PsOptionButton_ClearGroup( a1 )
        OPTTest_Check( "nothing checked: the FIRST member holds the tab stop", _
                       OPTTest_HasTabStop( a1 ), "" )
        PsOptionButton_SetEnabled( a1, false )
        OPTTest_Check( "disabling the holder hands the tab stop on", _
                       OPTTest_HasTabStop( a1 ) = false, "" )
        OPTTest_Check( "...to the first ENABLED member", OPTTest_HasTabStop( a2 ), "" )
        OPTTest_Check( "...and there is still exactly one", _
                       OPTTest_TabStopCount( hWndParent, 100 ) = 1, "" )
        PsOptionButton_SetEnabled( a1, true )
    end scope


    ' ---- 25. ARROW NAVIGATION: both directions, mirrored, wrapping at both ends ------------------
    scope
        ' Learnings.md: a single-sided test of a mirrored pair passes with the mirror inverted.
        ' So every claim below is made twice, with the input mirrored too.
        PsOptionButton_SetChecked( a1, true )

        SendMessageW( a1, WM_KEYDOWN, VK_DOWN, 0 )
        OPTTest_Check( "Down moves the selection to the next member", _
                       PsOptionButton_GetChecked( a2 ), "" )
        OPTTest_Check( "...and unchecked the previous one", _
                       PsOptionButton_GetChecked( a1 ) = false, "" )

        SendMessageW( a2, WM_KEYDOWN, VK_RIGHT, 0 )
        OPTTest_Check( "Right behaves as Down", PsOptionButton_GetChecked( a3 ), "" )

        ' WRAP FORWARD: a3 is the last member.
        SendMessageW( a3, WM_KEYDOWN, VK_DOWN, 0 )
        OPTTest_Check( "Down from the LAST member wraps to the first", _
                       PsOptionButton_GetChecked( a1 ), "" )

        SendMessageW( a1, WM_KEYDOWN, VK_UP, 0 )
        OPTTest_Check( "Up from the FIRST member wraps to the last", _
                       PsOptionButton_GetChecked( a3 ), "" )

        SendMessageW( a3, WM_KEYDOWN, VK_LEFT, 0 )
        OPTTest_Check( "Left behaves as Up", PsOptionButton_GetChecked( a2 ), "" )

        ' Arrow navigation is USER action, so it notifies -- through the same funnel a click uses.
        OPTTest_ResetNotify()
        SendMessageW( a2, WM_KEYDOWN, VK_DOWN, 0 )
        OPTTest_Check( "arrow navigation notifies both edges, like a click", _
                       gOPT_NotifyCount = 2, "notifies=" & str(gOPT_NotifyCount) )
        if gOPT_NotifyCount = 2 then
            OPTTest_Check( "arrow navigation: the loser still fires first", _
                           (gOPT_NotifyWnd(0) = a2) andalso (gOPT_NotifyVal(0) = false), "" )
        else
            OPTTest_Check( "arrow navigation: the loser still fires first", false, "wrong notify count" )
        end if

        ' The arrows must not escape the group.
        OPTTest_Check( "arrow navigation never leaves the group", _
                       PsOptionButton_GetChecked( b1 ) andalso (PsOptionButton_GetChecked( b2 ) = false), _
                       "group 200 unchanged" )
    end scope


    ' ---- 26. arrow navigation STEPS OVER disabled members ----------------------------------------
    scope
        PsOptionButton_SetChecked( a1, true )
        PsOptionButton_SetEnabled( a2, false )

        SendMessageW( a1, WM_KEYDOWN, VK_DOWN, 0 )
        OPTTest_Check( "Down steps OVER a disabled member", PsOptionButton_GetChecked( a3 ), "" )
        OPTTest_Check( "...and did not check the disabled one", _
                       PsOptionButton_GetChecked( a2 ) = false, "" )

        ' Mirrored.
        SendMessageW( a3, WM_KEYDOWN, VK_UP, 0 )
        OPTTest_Check( "Up steps OVER a disabled member too", PsOptionButton_GetChecked( a1 ), "" )

        PsOptionButton_SetEnabled( a2, true )
    end scope


    ' ---- 27. a group of one has nowhere to navigate ----------------------------------------------
    scope
        dim as HWND lone = OPTTest_MakeButton( hWndParent, 9931, 300, "Lone" )
        PsOptionButton_SetChecked( lone, true )
        OPTTest_ResetNotify()
        SendMessageW( lone, WM_KEYDOWN, VK_DOWN, 0 )
        OPTTest_Check( "a single-member group: Down is a silent no-op", _
                       (gOPT_NotifyCount = 0) andalso PsOptionButton_GetChecked( lone ), _
                       "notifies=" & str(gOPT_NotifyCount) )
        DestroyWindow( lone )
    end scope


    ' ---- 28. Space checks the focused member, and is a no-op on the checked one --------------------
    scope
        PsOptionButton_SetChecked( a1, true )
        OPTTest_ResetNotify()
        SendMessageW( a3, WM_KEYDOWN, VK_SPACE, 0 )
        OPTTest_Check( "Space checks the member that got it", PsOptionButton_GetChecked( a3 ), "" )
        OPTTest_Check( "Space fires both edges", gOPT_NotifyCount = 2, _
                       "notifies=" & str(gOPT_NotifyCount) )

        OPTTest_ResetNotify()
        SendMessageW( a3, WM_KEYDOWN, VK_SPACE, 0 )
        OPTTest_Check( "Space on the CHECKED member is a silent no-op", _
                       gOPT_NotifyCount = 0, "notifies=" & str(gOPT_NotifyCount) )
    end scope


    ' ---- 29. THE HOVER SAFETY NET MUST NOT ERASE KEYBOARD STATE ------------------------------------
    scope
        ' Learnings.md (PsMenuBar): the family's hover pattern assumes hover is the only writer of
        ' the highlight, and a control that grows keyboard selection on top of it gets that
        ' selection erased ~100ms after every keypress by the poll. This control is exactly that
        ' sibling, so the separation is asserted rather than assumed.
        dim as PSOPTIONBUTTON ptr p3 = GetOptionButtonPointer( a3 )
        p3->isHot     = true
        p3->isFocused = true
        dim as boolean bWasChecked = p3->isChecked

        PsOptionButton_ClearHot( p3, a3 )            ' what the poll and WM_MOUSELEAVE both call

        OPTTest_Check( "the hover net clears isHot", p3->isHot = false, "" )
        OPTTest_Check( "THE HOVER NET DOES NOT TOUCH isChecked", _
                       p3->isChecked = bWasChecked, "checked=" & str(p3->isChecked) )
        OPTTest_Check( "THE HOVER NET DOES NOT TOUCH isFocused", p3->isFocused, "" )
    end scope


    ' ---- 30. the registry preserves CREATION ORDER across a removal ---------------------------------
    scope
        ' Arrow navigation walks registry order, so a removal that reordered the array (the
        ' cheaper swap-with-last) would silently scramble a group's arrow order.
        '
        ' FOUR MEMBERS, AND THE ONE REMOVED IS THE SECOND. That is not arbitrary: the first
        ' version of this case used three and removed the middle one, which is the LAST-BUT-ONE
        ' entry -- and for that entry swap-with-last and compaction produce the IDENTICAL result.
        ' The case passed under both implementations and therefore tested nothing. Caught by
        ' deliberately breaking the removal and finding the assertion still green.
        dim as HWND c1 = OPTTest_MakeButton( hWndParent, 9941, 400, "C1" )
        dim as HWND c2 = OPTTest_MakeButton( hWndParent, 9942, 400, "C2" )
        dim as HWND c3 = OPTTest_MakeButton( hWndParent, 9943, 400, "C3" )
        dim as HWND c4 = OPTTest_MakeButton( hWndParent, 9944, 400, "C4" )
        OPTTest_Check( "precondition: group 400 built in order", _
                       (PsOptionButton_GetGroupMember( c1, 0 ) = c1) andalso _
                       (PsOptionButton_GetGroupMember( c1, 1 ) = c2) andalso _
                       (PsOptionButton_GetGroupMember( c1, 2 ) = c3) andalso _
                       (PsOptionButton_GetGroupMember( c1, 3 ) = c4), "" )

        DestroyWindow( c2 )                          ' NOT the last-but-one; see above
        OPTTest_Check( "the group shrank by one", PsOptionButton_GetGroupCount( c1 ) = 3, _
                       "count=" & str( PsOptionButton_GetGroupCount( c1 ) ) )
        OPTTest_Check( "REMOVAL COMPACTS, it does not swap-with-last", _
                       (PsOptionButton_GetGroupMember( c1, 0 ) = c1) andalso _
                       (PsOptionButton_GetGroupMember( c1, 1 ) = c3) andalso _
                       (PsOptionButton_GetGroupMember( c1, 2 ) = c4), "order preserved" )

        DestroyWindow( c1 )
        DestroyWindow( c3 )
        DestroyWindow( c4 )
    end scope


    ' ---- 31. DESTROYING THE CHECKED MEMBER: the fourth tab-stop site --------------------------------
    scope
        dim as HWND d1 = OPTTest_MakeButton( hWndParent, 9951, 500, "D1" )
        dim as HWND d2 = OPTTest_MakeButton( hWndParent, 9952, 500, "D2" )
        dim as HWND d3 = OPTTest_MakeButton( hWndParent, 9953, 500, "D3" )

        PsOptionButton_SetChecked( d2, true )
        OPTTest_Check( "precondition: d2 is checked and holds the tab stop", _
                       PsOptionButton_GetChecked( d2 ) andalso OPTTest_HasTabStop( d2 ), "" )

        DestroyWindow( d2 )

        OPTTest_Check( "the destroyed member left the group", _
                       PsOptionButton_GetGroupCount( d1 ) = 2, _
                       "count=" & str( PsOptionButton_GetGroupCount( d1 ) ) )
        OPTTest_Check( "the group is left with NOTHING checked (the legal empty state)", _
                       PsOptionButton_FindChecked( hWndParent, 500 ) = 0, "" )
        OPTTest_Check( "the check was NOT reassigned to an arbitrary survivor", _
                       (PsOptionButton_GetChecked( d1 ) = false) andalso _
                       (PsOptionButton_GetChecked( d3 ) = false), "" )
        OPTTest_Check( "THE TAB STOP MIGRATED -- the group is still reachable by Tab", _
                       OPTTest_TabStopCount( hWndParent, 500 ) = 1, _
                       "count=" & str( OPTTest_TabStopCount( hWndParent, 500 ) ) )
        OPTTest_Check( "...and it landed on the first survivor", OPTTest_HasTabStop( d1 ), "" )

        DestroyWindow( d1 )
        DestroyWindow( d3 )
        OPTTest_Check( "destroying every member empties the group cleanly", _
                       PsOptionButton_GroupCountOf( hWndParent, 500 ) = 0, "" )
    end scope


    ' ---- 32. SetGroupID moves a button and fixes up BOTH groups -------------------------------------
    scope
        PsOptionButton_SetChecked( a1, true )
        dim as long nBefore100 = PsOptionButton_GetGroupCount( a1 )
        dim as long nBefore200 = PsOptionButton_GetGroupCount( b1 )

        PsOptionButton_SetGroupID( a3, 200 )

        OPTTest_Check( "the old group shrank", PsOptionButton_GetGroupCount( a1 ) = nBefore100 - 1, _
                       "was=" & str(nBefore100) & " now=" & str( PsOptionButton_GetGroupCount( a1 ) ) )
        OPTTest_Check( "the new group grew", PsOptionButton_GetGroupCount( b1 ) = nBefore200 + 1, _
                       "was=" & str(nBefore200) & " now=" & str( PsOptionButton_GetGroupCount( b1 ) ) )
        OPTTest_Check( "the moved button reports its new group", _
                       PsOptionButton_GetGroupID( a3 ) = 200, _
                       "gid=" & str( PsOptionButton_GetGroupID( a3 ) ) )
        OPTTest_Check( "the OLD group still has exactly one tab stop", _
                       OPTTest_TabStopCount( hWndParent, 100 ) = 1, "" )
        OPTTest_Check( "the NEW group still has exactly one tab stop", _
                       OPTTest_TabStopCount( hWndParent, 200 ) = 1, "" )
        OPTTest_Check( "the new group did not end up with two checked members", _
                       PsOptionButton_GetChecked( a3 ) = false, "b1 was already checked" )

        PsOptionButton_SetGroupID( a3, 100 )
        OPTTest_Check( "moving it back restores both counts", _
                       (PsOptionButton_GetGroupCount( a1 ) = nBefore100) andalso _
                       (PsOptionButton_GetGroupCount( b1 ) = nBefore200), "" )
    end scope


    ' ---- 33. TAB PLUMBING.  Copied wholesale from PsCheckBox -- the flag is invisible at the
    '          call site that causes it, so it is asserted rather than remembered.
    scope
        dim as LONG_PTR exStyle = GetWindowLongPtr( h, GWL_EXSTYLE )
        sD = "exstyle=&h" & hex(exStyle)
        OPTTest_Check( "NOT a control parent (or Tab skips this control entirely)", _
                       (exStyle and WS_EX_CONTROLPARENT) = 0, sD )
        OPTTest_Check( "no WS_EX_WINDOWEDGE either", (exStyle and WS_EX_WINDOWEDGE) = 0, sD )

        ' The lone group-9000 control is the only member of its group, so it must hold the stop.
        dim as LONG_PTR wsStyle = GetWindowLongPtr( h, GWL_STYLE )
        OPTTest_Check( "the sole member of a group IS a tab stop", _
                       (wsStyle and WS_TABSTOP) <> 0, "style=&h" & hex(wsStyle) )

        ' End-to-end: can the dialog manager actually reach it? WS_VISIBLE is set DIRECTLY via
        ' SetWindowLongPtr rather than with ShowWindow -- no paint, no flash, and no dependence
        ' on when the host happens to run this test.
        dim as LONG_PTR savedStyle = wsStyle
        SetWindowLongPtr( h, GWL_STYLE, wsStyle or WS_VISIBLE )
        dim as boolean bReached = false
        dim as HWND hScan = GetNextDlgTabItem( hWndParent, NULL, FALSE )
        dim as HWND hFirst = hScan
        for i as long = 1 to 64
            if hScan = 0 then exit for
            if hScan = h then bReached = true : exit for
            hScan = GetNextDlgTabItem( hWndParent, hScan, FALSE )
            if hScan = hFirst then exit for            ' wrapped
        next
        SetWindowLongPtr( h, GWL_STYLE, savedStyle )
        OPTTest_Check( "GetNextDlgTabItem actually REACHES this control", bReached, _
                       "walked the parent's tab ring" )
    end scope


    ' ---- 34. WM_GETDLGCODE claims exactly the right keys, and no others -----------------------------
    scope
        OPTTest_Check( "claims Space", OPTTest_DlgCodeFor( h, VK_SPACE ) = DLGC_WANTALLKEYS, "" )
        OPTTest_Check( "claims Up",    OPTTest_DlgCodeFor( h, VK_UP )    = DLGC_WANTALLKEYS, "" )
        OPTTest_Check( "claims Down",  OPTTest_DlgCodeFor( h, VK_DOWN )  = DLGC_WANTALLKEYS, "" )
        OPTTest_Check( "claims Left",  OPTTest_DlgCodeFor( h, VK_LEFT )  = DLGC_WANTALLKEYS, "" )
        OPTTest_Check( "claims Right", OPTTest_DlgCodeFor( h, VK_RIGHT ) = DLGC_WANTALLKEYS, "" )

        ' NEGATIVE ASSERTIONS, and they are the point of this section. Enter is left for the
        ' dialog's default button -- a departure from PsCheckBox/PsToggle/PsButton, made because
        ' this control commits to Win32 group semantics. A future edit that "restores family
        ' consistency" turns this red instead of changing behaviour quietly.
        OPTTest_Check( "DOES NOT claim Enter (it belongs to the default button)", _
                       OPTTest_DlgCodeFor( h, VK_RETURN ) = 0, _
                       "code=" & str( OPTTest_DlgCodeFor( h, VK_RETURN ) ) )
        OPTTest_Check( "DOES NOT claim Tab (it would break the navigation it opted into)", _
                       OPTTest_DlgCodeFor( h, VK_TAB ) = 0, _
                       "code=" & str( OPTTest_DlgCodeFor( h, VK_TAB ) ) )
        OPTTest_Check( "claims nothing when asked without a MSG", _
                       SendMessageW( h, WM_GETDLGCODE, 0, 0 ) = 0, "" )

        ' And Enter must not activate either -- claiming nothing would be pointless if the
        ' WM_KEYDOWN handler acted on it anyway.
        PsOptionButton_SetChecked( a1, true )
        OPTTest_ResetNotify()
        SendMessageW( a3, WM_KEYDOWN, VK_RETURN, 0 )
        OPTTest_Check( "Enter does not check the focused member either", _
                       (PsOptionButton_GetChecked( a3 ) = false) andalso (gOPT_NotifyCount = 0), _
                       "notifies=" & str(gOPT_NotifyCount) )
    end scope


    ' ---- 35. window text: one store, two doors -------------------------------------------------------
    scope
        dim as wstring * 64 wszSet = "Window Text"
        SendMessageW( h, WM_SETTEXT, 0, cast(LPARAM, @wszSet) )
        OPTTest_Check( "WM_SETTEXT reaches the caption store", _
                       PsOptionButton_GetText( h ) = "Window Text", "" )
        OPTTest_Check( "WM_GETTEXTLENGTH agrees", _
                       SendMessageW( h, WM_GETTEXTLENGTH, 0, 0 ) = 11, _
                       "len=" & str( SendMessageW( h, WM_GETTEXTLENGTH, 0, 0 ) ) )

        dim as wstring * 64 wszGot
        dim as long nGot = SendMessageW( h, WM_GETTEXT, 64, cast(LPARAM, @wszGot) )
        OPTTest_Check( "WM_GETTEXT round-trips", (nGot = 11) andalso (wszGot = "Window Text"), _
                       "got=" & str(nGot) )

        ' Truncation must honour the caller's buffer size, terminator included.
        dim as wstring * 8 wszSmall
        dim as long nSmall = SendMessageW( h, WM_GETTEXT, 5, cast(LPARAM, @wszSmall) )
        OPTTest_Check( "WM_GETTEXT truncates to the caller's buffer", _
                       (nSmall = 4) andalso (wszSmall = "Wind"), _
                       "n=" & str(nSmall) & " s=" & str(wszSmall) )

        PsOptionButton_SetText( h, CAPTION )
    end scope


    ' ---- 36. RENDER PROBES ----------------------------------------------------------------------------
    scope
        ' THE THRESHOLD IS CALIBRATED, NOT INHERITED, AND THAT MATTERS.
        '
        ' PsButton and PsCheckBox both use "> 1" here, on the reasoning that a part wiped by a
        ' PaintBorderRect flood is literally one tone. THAT THRESHOLD WAS MEASURED AGAINST THIS
        ' CONTROL AND IT DOES NOT DISCRIMINATE: swapping the ring's PaintRoundOutline for
        ' PaintBorderRect destroys the render completely, and the box still reports TWO tones,
        ' because PaintBorderRect FILLS and then strokes a border -- fill colour plus pen colour.
        ' "> 1" passes on that. PsMessageBox hit the identical thing for the identical reason and
        ' its REFACTOR_PLAN says so; inheriting the number rather than re-measuring it is the
        ' trap, and this control walked into it before the A/B pass pulled it back out.
        '
        ' Measured, both states, this control:
        '     part      healthy   flooded with PaintBorderRect
        '     CONTROL      64            47   (the caption survives, so this one is coarse)
        '     BOX          33             2
        '     TEXT         46            46   (unaffected -- it is a different rect)
        '
        ' So the floor sits at 8: far above every flooded reading, far below every healthy one.
        const TONE_FLOOR = 8

        dim as long tCtl = PsOptionButton_CountRenderedTones( h, OPT_PART_CONTROL )
        dim as long tBox = PsOptionButton_CountRenderedTones( h, OPT_PART_BOX )
        dim as long tTxt = PsOptionButton_CountRenderedTones( h, OPT_PART_TEXT )
        OPTTest_Check( "the CONTROL is not flooded", tCtl >= TONE_FLOOR, "tones=" & str(tCtl) )
        OPTTest_Check( "the BOX is not flooded", tBox >= TONE_FLOOR, "tones=" & str(tBox) )
        OPTTest_Check( "the TEXT is not flooded", tTxt >= TONE_FLOOR, "tones=" & str(tTxt) )

        ' The ring is ANTIALIASED, which is the whole reason it is geometry rather than a font
        ' glyph, and it is the assertion that actually caught the flood above. Plain aliased GDI
        ' would give 2 tones for an outline; a smoothed one gives many.
        OPTTest_Check( "the ring is antialiased, not a two-tone aliased circle", tBox >= 12, _
                       "tones=" & str(tBox) )

        ' Determinism: the same state twice must hash identically, or every comparison below is
        ' measuring noise.
        dim as ulong hA = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )
        dim as ulong hB = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )
        OPTTest_Check( "the probe is deterministic", (hA = hB) andalso (hA <> 0), _
                       "&h" & hex(hA) & " vs &h" & hex(hB) )

        ' The state must reach the PIXELS, not just a field.
        PsOptionButton_SetChecked( h, false )
        dim as ulong hOff = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )
        PsOptionButton_SetChecked( h, true )
        dim as ulong hOn = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )
        sD = "unchecked=&h" & hex(hOff) & " checked=&h" & hex(hOn)
        OPTTest_Check( "the checked state reaches the pixels", _
                       (hOff <> hOn) andalso (hOff <> 0) andalso (hOn <> 0), sD )

        ' ISOLATED RE-RUN.  The case above is true for two reasons at once -- checked changes the
        ' ring COLOUR and also swaps an outline for a filled disc -- so it would still pass with
        ' the dot never drawn at all. PsCheckBox shipped exactly that shape of assertion and it
        ' survived freezing the thing it was named for.
        '
        ' So: hold the state at CHECKED and make the dot the only variable, by giving it the same
        ' colour as the disc it sits on. If the dot is really being drawn, the pixels move.
        scope
            dim as PSOPTIONBUTTON_COLORS saved
            PsOptionButton_GetColors( h, @saved )

            dim as PSOPTIONBUTTON_COLORS cTest = saved
            cTest.RingColorChecked = BGR(86,156,214)
            cTest.DotColor         = BGR(255,255,255)
            PsOptionButton_SetColors( h, @cTest )
            dim as ulong hDotVisible = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )

            ' Dot painted in the disc's own colour: geometry unchanged, dot invisible.
            cTest.DotColor = cTest.RingColorChecked
            PsOptionButton_SetColors( h, @cTest )
            dim as ulong hDotHidden = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )

            sD = "visible=&h" & hex(hDotVisible) & " hidden=&h" & hex(hDotHidden)
            OPTTest_Check( "THE DOT IS ACTUALLY DRAWN (isolated: only its colour changed)", _
                           (hDotVisible <> hDotHidden) andalso (hDotVisible <> 0), sD )

            PsOptionButton_SetColors( h, @saved )
        end scope

        ' And the mirrored half: with the dot held constant, the RING colour must also reach the
        ' pixels. Together these two say the checked rendering is not one undifferentiated blob.
        scope
            dim as PSOPTIONBUTTON_COLORS saved
            PsOptionButton_GetColors( h, @saved )
            dim as PSOPTIONBUTTON_COLORS cTest = saved

            cTest.RingColorChecked = BGR(200,60,60)
            PsOptionButton_SetColors( h, @cTest )
            dim as ulong hRedRing = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )

            cTest.RingColorChecked = BGR(60,200,60)
            PsOptionButton_SetColors( h, @cTest )
            dim as ulong hGreenRing = PsOptionButton_HashRenderedPart( h, OPT_PART_BOX )

            sD = "red=&h" & hex(hRedRing) & " green=&h" & hex(hGreenRing)
            OPTTest_Check( "the ring colour reaches the pixels (isolated)", _
                           (hRedRing <> hGreenRing) andalso (hRedRing <> 0), sD )

            PsOptionButton_SetColors( h, @saved )
        end scope

        PsOptionButton_SetChecked( h, false )
    end scope


    ' ---- teardown ---------------------------------------------------------------------------------
    DestroyWindow( a1 )
    DestroyWindow( a2 )
    DestroyWindow( a3 )
    DestroyWindow( b1 )
    DestroyWindow( b2 )
    DestroyWindow( h )

    OPTTest_Check( "every test control left the registry on destroy", _
                   PsOptionButton_GroupCountOf( hWndParent, 100 ) = 0 andalso _
                   PsOptionButton_GroupCountOf( hWndParent, 200 ) = 0 andalso _
                   PsOptionButton_GroupCountOf( hWndParent, 9000 ) = 0, _
                   "no leaked registry entries" )

    ' THE COUNT IS THE ORACLE, not the failure total: a suite that silently did not run reports
    ' zero failures. Compare this number against the recorded baseline.
    ' ------------------------------------------------------------------------------------
    ' The Enter/Space BEEP.
    '
    ' Claiming a key with DLGC_WANTALLKEYS makes IsDialogMessage TranslateMessage it, which
    ' manufactures a matching WM_CHAR. Nothing consuming that character means it reaches
    ' DefWindowProc, and the system beeps -- while the keystroke itself works perfectly. The
    ' beep is the ONLY symptom, so it cannot be heard by any assertion; what is asserted here
    ' is the mechanism that suppresses it, entirely through messages.
    '
    ' The control is DISABLED on purpose: the arm happens before the enabled check, so the
    ' mechanism is identical while nothing fires a callback or opens a dropdown -- and a
    ' disabled control that swallows the key owes the character too.
    scope
        dim as HWND hBeep = PsOptionButton_Create( hWndParent, 7804 )
        PsOptionButton_SetEnabled( hBeep, false )

        dim as MSG mBeep
        mBeep.hwnd = hBeep : mBeep.message = WM_CHAR : mBeep.wParam = 32

        SendMessageW( hBeep, WM_KEYDOWN, VK_SPACE, 0 )
        dim as LRESULT dc1 = SendMessageW( hBeep, WM_GETDLGCODE, 32, cast(LPARAM, @mBeep) )
        OPTTest_Check( "Space arms the WM_CHAR swallow that stops the system beep", _
            (dc1 and DLGC_WANTALLKEYS) <> 0, "dlgcode=&h" & hex(dc1) )

        SendMessageW( hBeep, WM_CHAR, 32, 0 )
        dim as LRESULT dc2 = SendMessageW( hBeep, WM_GETDLGCODE, 32, cast(LPARAM, @mBeep) )
        ' The disarm is the proof the WM_CHAR was consumed. The return value cannot be:
        ' DefWindowProc also returns 0 for WM_CHAR, so it reads the same when it fell through.
        OPTTest_Check( "...and the WM_CHAR is then swallowed instead of reaching DefWindowProc", _
            (dc2 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc2) )

        ' A key we never claimed must leave ordinary characters alone, or a host accelerator
        ' on a printable key would stop working.
        SendMessageW( hBeep, WM_KEYDOWN, VK_TAB, 0 )
        mBeep.wParam = asc("a")
        dim as LRESULT dc3 = SendMessageW( hBeep, WM_GETDLGCODE, asc("a"), cast(LPARAM, @mBeep) )
        OPTTest_Check( "an unclaimed key leaves ordinary characters alone", _
            (dc3 and DLGC_WANTALLKEYS) = 0, "dlgcode=&h" & hex(dc3) )

        DestroyWindow( hBeep )
    end scope


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
        dim as HWND hTipT = PsOptionButton_Create( hWndParent, 9807, 77 )
        if hTipT = 0 then
            OPTTest_Check( "tooltip: could not create a test control", false, "Create returned 0" )
        else
            OPTTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsOptionButton_GetTooltipMode( hTipT ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsOptionButton_GetTooltipMode( hTipT )) )
            OPTTest_Check( "tooltip: a comctl32 window exists, and no PsTooltip does", _
                (PsOptionButton_GetTooltipHandle( hTipT ) <> 0) andalso _
                (IsWindow(PsOptionButton_GetTooltipHandle( hTipT )) <> 0) andalso _
                (PsOptionButton_GetPsTooltipHandle( hTipT ) = 0), _
                "hSys = " & str(cast(integer, PsOptionButton_GetTooltipHandle( hTipT ))) )

            PsOptionButton_SetHoverTime( hTipT, 777 )
            OPTTest_Check( "tooltip: SetHoverTime reaches the comctl32 tip", _
                (SendMessageW( PsOptionButton_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "TTM_GETDELAYTIME = " & str(SendMessageW( PsOptionButton_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )

            dim as HWND hSysBeforeT = PsOptionButton_GetTooltipHandle( hTipT )
            ' The switch is made on its own line, NOT inside the assertion call. fbc does not
            ' evaluate call arguments left to right, so a detail string that reads the mode in
            ' the same call reports the mode from BEFORE the switch -- a green assertion with a
            ' trace saying it failed, which is worse than either.
            dim as boolean bSwitchedT = PsOptionButton_SetTooltipMode( hTipT, PSTIP_MODE_PS )
            OPTTest_Check( "tooltip: switching to PsTooltip succeeds", bSwitchedT, _
                "mode = " & str(PsOptionButton_GetTooltipMode( hTipT )) )
            OPTTest_Check( "tooltip: the outgoing comctl32 window is DESTROYED, not orphaned", _
                (IsWindow(hSysBeforeT) = 0) andalso (PsOptionButton_GetTooltipHandle( hTipT ) = 0), _
                "old hSys alive = " & str(IsWindow(hSysBeforeT)) )

            dim as HWND hPsT = PsOptionButton_GetPsTooltipHandle( hTipT )
            OPTTest_Check( "tooltip: a PsTooltip window exists and is attached to the control", _
                (hPsT <> 0) andalso (IsWindow(hPsT) <> 0) andalso _
                (PsTooltip_GetAttached( hPsT ) = hTipT), _
                "hPs = " & str(cast(integer, hPsT)) )

            dim as PSTOOLTIP ptr pTipT = GetTooltipPointer( hPsT )
            OPTTest_Check( "tooltip: the hover time set on the OLD backend is honoured by the new one", _
                (pTipT <> 0) andalso (pTipT->nInitialDelay = 777), _
                "PsTooltip initial delay = " & str(iif(pTipT, pTipT->nInitialDelay, -1)) & " ms" )
            OPTTest_Check( "tooltip: the PsTooltip backend is WIRED to the control's text policy", _
                (pTipT <> 0) andalso (pTipT->TooltipCallback <> 0), _
                "callback installed = " & str(iif(pTipT andalso pTipT->TooltipCallback, 1, 0)) )

            PsOptionButton_SetTooltipMode( hTipT, PSTIP_MODE_SYSTEM )
            OPTTest_Check( "tooltip: switching back rebuilds the comctl32 tip, hover time and all", _
                (PsOptionButton_GetTooltipHandle( hTipT ) <> 0) andalso _
                (PsOptionButton_GetPsTooltipHandle( hTipT ) = 0) andalso _
                (SendMessageW( PsOptionButton_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "initial = " & str(SendMessageW( PsOptionButton_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )
            OPTTest_Check( "tooltip: an unset delay is left at the backend's own default", _
                (SendMessageW( PsOptionButton_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 ) > 0), _
                "autopop = " & str(SendMessageW( PsOptionButton_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 )) & " ms" )

            DestroyWindow( hTipT )
        end if
    end scope

    dim as string sSummary = "=== PsOptionButton: " & str(gOPT_TestCount - gOPT_FailCount) & _
                             "/" & str(gOPT_TestCount) & " assertions passed"
    if gOPT_FailCount > 0 then sSummary &= "  (" & str(gOPT_FailCount) & " FAILED)"
    print sSummary

end sub
