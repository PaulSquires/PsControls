'    PsTooltip - reusable owner-drawn tooltip control
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


' ========================================================================================
' Geometry and behaviour self-test, run with PSTOOLTIP_SELFTEST=1.
'
' WHAT THIS CAN AND CANNOT REACH. The four pure functions are asserted over their whole
' ranges, and the layout is asserted against a real tip with a real DC. What NO assertion here
' touches is the TICK: dwell timing, the fade actually running, click suppression and auto-pop
' all need a live message pump and real cursor movement, and a self-test that ran one would
' block the host that called it. Those are the interactive pass's, and that pass is therefore
' load-bearing evidence for this control rather than a finishing touch -- the same position
' PsMessageBox is in with its modal loop.
'
' Reports assertions RUN as well as failed: a harness that only counts failures cannot tell
' "all passed" from "never executed", and a suite that does not run reports zero failures
' (Learnings.md).
' ========================================================================================
dim shared as long gTipRun, gTipFailCount

' The parameter is bOK, never ok: GDI+'s Status enum defines Ok = 0 in namespace AfxNova and
' every host says `using AfxNova`.
private sub TipChk( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gTipRun += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gTipFailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub


private function TipRectStr( byval rc as RECT ) as string
    return "(" & rc.left & "," & rc.top & ")-(" & rc.right & "," & rc.bottom & ")"
end function


' --- state for the show-callback assertions below ---
dim shared as long gTipShowEdges, gTipHideEdges
dim shared as boolean gTipAllowShow

private function TipShowCb( byval hTooltip as HWND, byval hAttached as HWND, _
                            byval idx as long, byval bShowing as boolean ) as boolean
    if bShowing then
        gTipShowEdges += 1
        return gTipAllowShow
    end if
    gTipHideEdges += 1
    return true
end function


sub PsTooltip_RunSelfTest( byval hwnd as HWND )

    print ""
    print "=== PsTooltip self-test ==="

    gTipRun = 0 : gTipFailCount = 0

    if AfxCWindowPtr( hwnd ) = 0 then
        print "FAIL  precondition: hwnd has no CWindow   [nothing can be measured]"
        print "=== 0 of 1 assertions passed ==="
        gTipFailCount = 1
        exit sub
    end if

    ' A work area that is NOT at the origin, so a clamp that forgot rcWork.left/top and just
    ' clamped against 0 shows up. Deliberately not the real monitor: the placement law must be
    ' assertable without one.
    dim as RECT rcWork = type( 100, 50, 1100, 850 )
    const CURSOR_OFS = 20


    ' ====================================================================================
    ' 1. PsTooltip_ComputeOrigin -- placement, clamping and the flip
    ' ====================================================================================
    print ""
    print "-- placement --"

    scope
        ' Cursor mode: below-RIGHT of the hotspot, cleared by the cursor's own height so the
        ' pointer never sits on top of the tip.
        dim as RECT rcAnchor = type( 400, 300, 400, 300 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 60, rcWork, TIP_ALIGN_CURSOR, CURSOR_OFS )
        TipChk( "origin: cursor mode goes below-right, cleared by the cursor height", _
                (pt.x = 400) andalso (pt.y = 320), "got " & pt.x & "," & pt.y & " want 400,320" )
    end scope

    scope
        ' No room below -> FLIP ABOVE the anchor, not clamp up over it. This is the rule
        ' PsPopupMenu had to learn when tall dropdowns became impossible to open.
        dim as RECT rcAnchor = type( 400, 820, 400, 820 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 60, rcWork, TIP_ALIGN_CURSOR, CURSOR_OFS )
        TipChk( "origin: no room below -> flips ABOVE the anchor", _
                (pt.y = 760), "got y " & pt.y & " want 760 (anchor.top 820 - h 60)" )
        TipChk( "origin: the flipped tip clears the anchor entirely", _
                (pt.y + 60 <= 820), "bottom " & (pt.y + 60) & " vs anchor.top 820" )
    end scope

    scope
        ' Neither below nor above fits -> fall back to a clamp rather than going off-screen.
        dim as RECT rcAnchor = type( 400, 60, 400, 60 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 900, rcWork, TIP_ALIGN_CURSOR, CURSOR_OFS )
        TipChk( "origin: taller than the work area clamps to its top", _
                (pt.y = rcWork.top), "got y " & pt.y & " want " & rcWork.top )
    end scope

    scope
        dim as RECT rcAnchor = type( 300, 400, 500, 430 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 60, rcWork, TIP_ALIGN_BELOW, CURSOR_OFS )
        TipChk( "origin: BELOW sits under the anchor rect", _
                (pt.x = 300) andalso (pt.y = 430), "got " & pt.x & "," & pt.y & " want 300,430" )
    end scope

    scope
        dim as RECT rcAnchor = type( 300, 800, 500, 830 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 60, rcWork, TIP_ALIGN_BELOW, CURSOR_OFS )
        TipChk( "origin: BELOW with no room flips above the anchor rect", _
                (pt.y = 740), "got y " & pt.y & " want 740 (anchor.top 800 - h 60)" )
    end scope

    scope
        dim as RECT rcAnchor = type( 300, 400, 500, 430 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 60, rcWork, TIP_ALIGN_ABOVE, CURSOR_OFS )
        TipChk( "origin: ABOVE sits over the anchor rect", _
                (pt.y = 340), "got y " & pt.y & " want 340 (anchor.top 400 - h 60)" )
    end scope

    scope
        ' ABOVE flips the OTHER way. The mirror case exists because the two directions are
        ' separate branches, and a copy-paste between them would only show up here.
        dim as RECT rcAnchor = type( 300, 60, 500, 90 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 60, rcWork, TIP_ALIGN_ABOVE, CURSOR_OFS )
        TipChk( "origin: ABOVE with no room flips BELOW the anchor rect", _
                (pt.y = 90), "got y " & pt.y & " want 90 (anchor.bottom)" )
    end scope

    scope
        dim as RECT rcAnchor = type( 1050, 300, 1050, 300 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 60, rcWork, TIP_ALIGN_CURSOR, CURSOR_OFS )
        TipChk( "origin: clamps to the work area's RIGHT edge", _
                (pt.x = 900), "got x " & pt.x & " want 900 (right 1100 - w 200)" )
    end scope

    scope
        ' The left clamp must run AFTER the right one, or a tip wider than the work area ends
        ' up off the left edge. Deliberately wider than rcWork.
        dim as RECT rcAnchor = type( 110, 300, 110, 300 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 2000, 60, rcWork, TIP_ALIGN_CURSOR, CURSOR_OFS )
        TipChk( "origin: wider than the work area still clamps to its LEFT edge", _
                (pt.x = rcWork.left), "got x " & pt.x & " want " & rcWork.left )
    end scope

    scope
        ' THE ANCHOR IS NOT GUARANTEED TO BE INSIDE THE WORK AREA. In cursor mode the anchor
        ' IS the cursor, which sits happily over a taskbar or on another monitor -- and
        ' "flip above the anchor" then lands outside too. This is a REGRESSION assertion: the
        ' sweep below found it, at anchor y=855 against a work area ending at 850, where the
        ' flip produced 805 and the tip's bottom fell 5px past the edge.
        dim as RECT rcAnchor = type( 400, 855, 400, 855 )
        dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 200, 50, rcWork, TIP_ALIGN_CURSOR, CURSOR_OFS )
        TipChk( "origin: an anchor BELOW the work area still yields a tip inside it", _
                (pt.y + 50 <= rcWork.bottom), "origin y " & pt.y & " + 50 vs work bottom " & rcWork.bottom )
    end scope

    scope
        ' A sweep, because the individual cases above each test one branch and the interaction
        ' between the clamps is what actually has to hold. Anchors deliberately range OUTSIDE
        ' rcWork on every side -- which is what turned up the flip-outside-the-work-area bug.
        dim as long nBad = 0
        for ax as long = 0 to 1200 step 60
            for ay as long = 0 to 900 step 45
                dim as RECT rcAnchor = type( ax, ay, ax + 40, ay + 20 )
                for nAlign as long = TIP_ALIGN_CURSOR to TIP_ALIGN_ABOVE
                    dim as POINT pt = PsTooltip_ComputeOrigin( rcAnchor, 180, 50, rcWork, nAlign, CURSOR_OFS )
                    if pt.x < rcWork.left then nBad += 1
                    if pt.y < rcWork.top then nBad += 1
                    if pt.x + 180 > rcWork.right then nBad += 1
                    if pt.y + 50 > rcWork.bottom then nBad += 1
                next
            next
        next
        TipChk( "origin: sweep of 3 alignments x 441 anchors never leaves the work area", _
                (nBad = 0), "violations = " & nBad )
    end scope


    ' ====================================================================================
    ' 2. PsTooltip_StemOnTop -- one rule, six cases
    ' ====================================================================================
    print ""
    print "-- stem side --"

    scope
        dim as RECT rcAnchor = type( 400, 300, 400, 300 )
        dim as POINT ptBelow = type<POINT>( 400, 320 )
        dim as POINT ptAbove = type<POINT>( 400, 240 )
        TipChk( "stemOnTop: cursor mode, tip below -> stem on TOP", _
                PsTooltip_StemOnTop( rcAnchor, ptBelow, TIP_ALIGN_CURSOR ), "y 320 vs anchor.top 300" )
        TipChk( "stemOnTop: cursor mode, tip flipped above -> stem on BOTTOM", _
                (PsTooltip_StemOnTop( rcAnchor, ptAbove, TIP_ALIGN_CURSOR ) = false), "y 240 vs anchor.top 300" )
    end scope

    scope
        dim as RECT rcAnchor = type( 300, 400, 500, 430 )
        TipChk( "stemOnTop: BELOW, not flipped -> stem on TOP", _
                PsTooltip_StemOnTop( rcAnchor, type<POINT>(300, 430), TIP_ALIGN_BELOW ), "y 430 vs top 400" )
        TipChk( "stemOnTop: BELOW, flipped -> stem on BOTTOM", _
                (PsTooltip_StemOnTop( rcAnchor, type<POINT>(300, 340), TIP_ALIGN_BELOW ) = false), "y 340 vs top 400" )
        TipChk( "stemOnTop: ABOVE, not flipped -> stem on BOTTOM", _
                (PsTooltip_StemOnTop( rcAnchor, type<POINT>(300, 340), TIP_ALIGN_ABOVE ) = false), "y 340 vs top 400" )
        TipChk( "stemOnTop: ABOVE, flipped -> stem on TOP", _
                PsTooltip_StemOnTop( rcAnchor, type<POINT>(300, 430), TIP_ALIGN_ABOVE ), "y 430 vs top 400" )
    end scope


    ' ====================================================================================
    ' 3. PsTooltip_ComputeStem -- the geometry PaintPolygon is handed
    ' ====================================================================================
    print ""
    print "-- stem geometry --"

    scope
        dim as POINT pts(0 to 2)
        dim as boolean bGot = PsTooltip_ComputeStem( 200, 80, true, 14, 7, 100, 6, @pts(0) )
        TipChk( "stem: seats on a normal client", bGot, "returned " & bGot )
        TipChk( "stem on top: apex touches y = 0", (pts(1).y = 0), "apex y " & pts(1).y )
        TipChk( "stem on top: base sits at y = nStemH", _
                (pts(0).y = 7) andalso (pts(2).y = 7), "base y " & pts(0).y & "," & pts(2).y & " want 7" )
        TipChk( "stem: base is centred on the target", _
                (pts(1).x = 100) andalso (pts(0).x = 93) andalso (pts(2).x = 107), _
                "x " & pts(0).x & "," & pts(1).x & "," & pts(2).x & " want 93,100,107" )
        TipChk( "stem: base width is 2*(nStemW\2)", _
                ((pts(2).x - pts(0).x) = 14), "width " & (pts(2).x - pts(0).x) )
    end scope

    scope
        dim as POINT pts(0 to 2)
        PsTooltip_ComputeStem( 200, 80, false, 14, 7, 100, 6, @pts(0) )
        TipChk( "stem on bottom: apex touches y = cyClient", (pts(1).y = 80), "apex y " & pts(1).y )
        TipChk( "stem on bottom: base sits at cyClient - nStemH", _
                (pts(0).y = 73) andalso (pts(2).y = 73), "base y " & pts(0).y & " want 73" )
    end scope

    scope
        ' Clamped clear of the rounded corners at BOTH ends. Curvature is a DIAMETER, so the
        ' radius is half -- a stem allowed to slide into the curve has no straight edge to
        ' leave from and reads as detached.
        dim as POINT pts(0 to 2)
        PsTooltip_ComputeStem( 200, 80, true, 14, 7, 0, 20, @pts(0) )     ' target far left
        TipChk( "stem: clamped clear of the LEFT corner (radius 10 + half 7)", _
                (pts(1).x = 17), "apex x " & pts(1).x & " want 17" )
        PsTooltip_ComputeStem( 200, 80, true, 14, 7, 999, 20, @pts(0) )   ' target far right
        TipChk( "stem: clamped clear of the RIGHT corner", _
                (pts(1).x = 183), "apex x " & pts(1).x & " want 183 (200 - 10 - 7)" )
    end scope

    scope
        ' THE CURSOR-MODE INSET AND THE CORNER CLAMP MUST BE THE SAME NUMBER.
        '
        ' In cursor mode placement puts the tip's left edge at the cursor, so the stem's target
        ' is client x = 0 and the corner clamp shoves it to (radius + half) -- the stem parks in
        ' the corner and points at nothing. TIP_ShowCore compensates by shifting the ANCHOR left
        ' by exactly that distance, and the two formulas live in different functions. This
        ' asserts they agree: change the clamp without changing the inset and the stem silently
        ' stops pointing at the cursor again. Found by LOOKING at a capture; no other assertion
        ' in this suite could see it.
        dim as POINT pts(0 to 2)
        dim as long nCurv = 10
        dim as long nStemWide = 24
        dim as long nInset = (nCurv \ 2) + (nStemWide \ 2)
        PsTooltip_ComputeStem( 300, 80, true, nStemWide, 7, nInset, nCurv, @pts(0) )
        TipChk( "stem: the cursor-mode inset equals the corner clamp (the apex lands ON the target)", _
                (pts(1).x = nInset), "target " & nInset & " -> apex " & pts(1).x & " (unclamped)" )
    end scope

    scope
        dim as POINT pts(0 to 2)
        pts(0) = type<POINT>( -1, -1 )
        TipChk( "stem: too narrow to seat -> returns FALSE and touches nothing", _
                (PsTooltip_ComputeStem( 20, 80, true, 14, 7, 10, 20, @pts(0) ) = false) andalso (pts(0).x = -1), _
                "client 20 needs 2*(10+7)=34" )
        TipChk( "stem: degenerate size -> FALSE", _
                (PsTooltip_ComputeStem( 200, 80, true, 1, 7, 100, 6, @pts(0) ) = false) andalso _
                (PsTooltip_ComputeStem( 200, 80, true, 14, 0, 100, 6, @pts(0) ) = false), _
                "nStemW 1 and nStemH 0 both refused" )
        TipChk( "stem: null point array -> FALSE", _
                (PsTooltip_ComputeStem( 200, 80, true, 14, 7, 100, 6, 0 ) = false), "pts = 0" )
    end scope


    ' ====================================================================================
    ' 4. PsTooltip_ComputeAlpha -- the fade ramp
    ' ====================================================================================
    print ""
    print "-- fade ramp --"

    TipChk( "alpha: fade IN starts transparent", _
            (PsTooltip_ComputeAlpha( 0, 120, true ) = 0), "t=0 -> " & PsTooltip_ComputeAlpha(0,120,true) )
    TipChk( "alpha: fade IN ends opaque", _
            (PsTooltip_ComputeAlpha( 120, 120, true ) = 255), "t=120 -> " & PsTooltip_ComputeAlpha(120,120,true) )
    TipChk( "alpha: fade OUT starts opaque", _
            (PsTooltip_ComputeAlpha( 0, 120, false ) = 255), "t=0 -> " & PsTooltip_ComputeAlpha(0,120,false) )
    TipChk( "alpha: fade OUT ends transparent", _
            (PsTooltip_ComputeAlpha( 120, 120, false ) = 0), "t=120 -> " & PsTooltip_ComputeAlpha(120,120,false) )
    TipChk( "alpha: clamps past the end rather than overshooting", _
            (PsTooltip_ComputeAlpha( 5000, 120, true ) = 255) andalso _
            (PsTooltip_ComputeAlpha( 5000, 120, false ) = 0) andalso _
            (PsTooltip_ComputeAlpha( -50, 120, true ) = 0), "t=5000 and t=-50" )
    TipChk( "alpha: nFadeMs 0 is an instant transition, both directions", _
            (PsTooltip_ComputeAlpha( 0, 0, true ) = 255) andalso _
            (PsTooltip_ComputeAlpha( 0, 0, false ) = 0), "in -> 255, out -> 0" )

    scope
        ' Monotonic across the whole ramp, both ways, and the two are exact complements. A
        ' ramp that merely hits its endpoints could still do anything in between.
        dim as long nBad = 0, nNotComp = 0
        dim as long nPrevIn = -1, nPrevOut = 256
        for t as long = 0 to 120
            dim as long aIn  = PsTooltip_ComputeAlpha( t, 120, true )
            dim as long aOut = PsTooltip_ComputeAlpha( t, 120, false )
            if aIn < nPrevIn then nBad += 1
            if aOut > nPrevOut then nBad += 1
            if (aIn + aOut) <> 255 then nNotComp += 1
            nPrevIn = aIn : nPrevOut = aOut
        next
        TipChk( "alpha: monotonic over the whole ramp, both directions", (nBad = 0), "regressions = " & nBad )
        TipChk( "alpha: in and out are exact complements at every step", (nNotComp = 0), "mismatches = " & nNotComp )
    end scope


    ' ====================================================================================
    ' 5. PsTooltip_ShouldShow -- the whole veto table, one veto at a time
    ' ====================================================================================
    print ""
    print "-- show decision --"

    TipChk( "shouldShow: all conditions met -> TRUE", _
            PsTooltip_ShouldShow( true, true, true, false, false ), "over+resting+text, no button, not suppressed" )
    TipChk( "shouldShow: cursor not over the tool vetoes", _
            (PsTooltip_ShouldShow( false, true, true, false, false ) = false), "bOver = false" )
    TipChk( "shouldShow: still moving vetoes", _
            (PsTooltip_ShouldShow( true, false, true, false, false ) = false), "bResting = false" )
    TipChk( "shouldShow: no text vetoes", _
            (PsTooltip_ShouldShow( true, true, false, false, false ) = false), "bHasText = false" )
    TipChk( "shouldShow: a held mouse button vetoes", _
            (PsTooltip_ShouldShow( true, true, true, true, false ) = false), "bButtonDown = true" )
    TipChk( "shouldShow: post-click suppression vetoes", _
            (PsTooltip_ShouldShow( true, true, true, false, true ) = false), "bSuppressed = true" )


    ' ====================================================================================
    ' 6. LAYOUT -- against a real tip with a real DC
    ' ====================================================================================
    print ""
    print "-- layout --"

    dim as HWND hT = PsTooltip_Create( hwnd )
    if hT = 0 then
        TipChk( "layout: could not create a test tip", false, "PsTooltip_Create returned 0" )
    else
        PsTooltip_SetFonts( hT, ghFont(GUIFONT_9), ghFont(GUIFONTBOLD_10), ghFont(GUIFONT_9) )

        dim as long w0, h0, w1, h1
        PsTooltip_SetText( hT, "Hello" )
        PsTooltip_GetIdealSize( hT, w0, h0 )
        TipChk( "layout: a plain tip has a real size", (w0 > 0) andalso (h0 > 0), w0 & " x " & h0 )

        ' The balloon costs exactly nStemH in height and NOTHING in width. If the stem band
        ' ever leaked into the width this is what would catch it.
        PsTooltip_SetStemSize( hT, 14, 7 )
        PsTooltip_SetStyle( hT, TIP_STYLE_BALLOON )
        PsTooltip_GetIdealSize( hT, w1, h1 )
        TipChk( "layout: the balloon adds exactly nStemH to the height", _
                (h1 = h0 + 7), "rect " & h0 & " -> balloon " & h1 & " want +7" )
        TipChk( "layout: the balloon adds NOTHING to the width", (w1 = w0), w0 & " -> " & w1 )
        scope
            ' THE BODY AND THE STEM BAND MUST EXACTLY TILE THE CLIENT.
            '
            ' Anything neither of them covers is painted by nothing and shows as BLACK -- which
            ' is what a capture caught after the first build: the whole stem band, the full
            ' width of the tip, as a black bar above the balloon. Every probe read a part rect
            ' INSIDE the body, so the entire suite was blind to it. The window region added to
            ' fix it is built from these same two rects, so asserting they tile is asserting the
            ' region covers everything drawn.
            dim as PSTOOLTIP ptr pB = GetTooltipPointer( hT )
            dim as long wB, hB
            PsTooltip_GetIdealSize( hT, wB, hB )
            ' Reproduce what TIP_ShowCore does for a stem-on-top balloon.
            SetRect( @pB->rcBody, 0, 7, wB, hB )
            TipChk( "layout: body + stem band tile the client exactly (nothing unpainted)", _
                    (pB->rcBody.top = 7) andalso (pB->rcBody.bottom = hB) andalso _
                    (pB->rcBody.left = 0) andalso (pB->rcBody.right = wB), _
                    "client 0,0-" & wB & "," & hB & "  body " & TipRectStr(pB->rcBody) & "  band 7" )
        end scope

        PsTooltip_SetStyle( hT, TIP_STYLE_RECT )

        ' A title adds a band; a glyph adds a declared cell.
        PsTooltip_SetTitle( hT, "Title" )
        PsTooltip_GetIdealSize( hT, w1, h1 )
        TipChk( "layout: a title makes the tip taller", (h1 > h0), h0 & " -> " & h1 )
        dim as RECT rcTi = PsTooltip_GetPartRect( hT, TIP_PART_TITLE )
        TipChk( "layout: the title band is non-empty once there is a title", _
                (IsRectEmpty(@rcTi) = 0), TipRectStr(rcTi) )

        dim as RECT rcG = PsTooltip_GetPartRect( hT, TIP_PART_GLYPH )
        TipChk( "layout: no glyph -> an EMPTY glyph cell, not a zero-width one at the origin", _
                IsRectEmpty(@rcG), TipRectStr(rcG) )

        PsTooltip_SetGlyphCell( hT, 20, 8 )
        PsTooltip_SetGlyph( hT, "G" )
        rcG = PsTooltip_GetPartRect( hT, TIP_PART_GLYPH )
        TipChk( "layout: the glyph cell is DECLARED, not measured (PsIconPanel's rule)", _
                ((rcG.right - rcG.left) = 20) andalso ((rcG.bottom - rcG.top) = 20), _
                TipRectStr(rcG) & " want 20x20" )

        scope
            ' The gap is charged only when there is something on BOTH sides of it -- PsButton's
            ' rule. A glyph with no title must not carry the inset around for nothing.
            dim as long wBoth, hBoth, wGlyphOnly, hGlyphOnly
            PsTooltip_GetIdealSize( hT, wBoth, hBoth )
            PsTooltip_SetTitle( hT, "" )
            PsTooltip_GetIdealSize( hT, wGlyphOnly, hGlyphOnly )
            PsTooltip_SetTitle( hT, "Title" )
            TipChk( "layout: the glyph gap is charged only when a title is beside it", _
                    (wBoth > wGlyphOnly), "with title " & wBoth & ", glyph alone " & wGlyphOnly )
        end scope

        scope
            ' The wrap. This is the capability the whole control exists for, so it is measured
            ' both ways rather than assumed from the flag.
            dim as DWSTRING wszLong = "The quick brown fox jumps over the lazy dog and keeps " & _
                                      "going for quite a while after that as well."
            PsTooltip_SetTitle( hT, "" )
            PsTooltip_SetGlyph( hT, "" )
            PsTooltip_SetText( hT, wszLong )
            PsTooltip_SetMaxWidth( hT, 0 )
            dim as long wNo, hNo
            PsTooltip_GetIdealSize( hT, wNo, hNo )
            PsTooltip_SetMaxWidth( hT, 200 )
            dim as long wCap, hCap
            PsTooltip_GetIdealSize( hT, wCap, hCap )
            TipChk( "layout: SetMaxWidth makes a long message narrower", (wCap < wNo), _
                    "unwrapped " & wNo & " -> capped " & wCap )
            TipChk( "layout: ...and correspondingly taller", (hCap > hNo), _
                    "unwrapped " & hNo & " -> capped " & hCap )
            TipChk( "layout: the capped width honours the cap", (wCap <= 200), wCap & " <= 200" )

            ' A LONG TITLE STILL WIDENS THE TIP. SetMaxWidth caps the TEXT WRAP, not the
            ' window -- the width is a max() over the bands, not a clamp on the result. This
            ' is PsMessageBox's contract, and clamping here would clip the title instead of
            ' the thing the cap was aimed at.
            PsTooltip_SetTitle( hT, "A title considerably longer than two hundred pixels would allow" )
            dim as long wTitle, hTitle
            PsTooltip_GetIdealSize( hT, wTitle, hTitle )
            TipChk( "layout: SetMaxWidth caps the TEXT WRAP, not the window (a max, not a clamp)", _
                    (wTitle > 200), "with a long title " & wTitle & " > cap 200" )
            PsTooltip_SetTitle( hT, "" )
        end scope

        scope
            ' The flags the text was MEASURED with are the flags handed to the painter. A
            ' mismatch between the two is how text that fits in the arithmetic clips on screen
            ' -- the contract PsOptionButton's own demo broke by measuring regular and
            ' painting bold.
            dim as PSTOOLTIP ptr pT = GetTooltipPointer( hT )
            PsTooltip_SetMaxWidth( hT, 0 )
            dim as long wTmp, hTmp
            PsTooltip_GetIdealSize( hT, wTmp, hTmp )
            dim as boolean bNoWrap = ((pT->nTextFlags and DT_WORDBREAK) = 0)
            PsTooltip_SetMaxWidth( hT, 200 )
            PsTooltip_GetIdealSize( hT, wTmp, hTmp )
            dim as boolean bWrap = ((pT->nTextFlags and DT_WORDBREAK) <> 0)
            TipChk( "layout: DT_WORDBREAK is set if and only if a max width is", _
                    bNoWrap andalso bWrap, "maxWidth 0 -> no DT_WORDBREAK, 200 -> DT_WORDBREAK" )
        end scope

        scope
            ' The resolved-text buffer must follow its source. A stale buffer would have the
            ' tip measure and draw the PREVIOUS item's text while every layout number looked
            ' perfectly consistent -- invisible to any assertion that only reads geometry.
            PsTooltip_SetMaxWidth( hT, 0 )
            PsTooltip_SetText( hT, "iii" )
            dim as long wShort, hShort
            PsTooltip_GetIdealSize( hT, wShort, hShort )
            PsTooltip_SetText( hT, "WWWWWWWWWWWWWWWWWWWWWWWW" )
            dim as long wLong, hLong
            PsTooltip_GetIdealSize( hT, wLong, hLong )
            TipChk( "layout: changing the text re-resolves it (no stale wszTipBuf)", _
                    (wLong > wShort), "short " & wShort & " -> long " & wLong )
        end scope

        DestroyWindow( hT )
    end if


    ' ====================================================================================
    ' 6b. THE SHOW CALLBACK AND ITS VETO
    '
    ' The dwell that normally drives this cannot be asserted -- it needs a live pump and real
    ' cursor movement -- but PsTooltip_Show drives the identical TIP_ShowCore, so the veto path
    ' and the edge pairing ARE reachable. Worth having: the veto shipped honoured for exactly one
    ' 100ms tick and then retried, so a vetoed tip appeared anyway a tenth of a second late and
    ' the whole feature read as ignored.
    ' ====================================================================================
    print ""
    print "-- show callback --"

    scope
        dim as HWND hV = PsTooltip_Create( hwnd )
        if hV = 0 then
            TipChk( "show callback: could not create a test tip", false, "Create returned 0" )
        else
            PsTooltip_SetFonts( hV, ghFont(GUIFONT_9), 0, 0 )
            PsTooltip_SetText( hV, "Vetoable" )
            PsTooltip_SetShowCallback( hV, @TipShowCb )
            PsTooltip_SetFadeTime( hV, 0 )        ' no fade: hide must complete synchronously

            gTipShowEdges = 0 : gTipHideEdges = 0 : gTipAllowShow = false
            dim as boolean bShown = PsTooltip_Show( hV )
            TipChk( "show callback: returning FALSE suppresses the tip", _
                    (bShown = false) andalso (PsTooltip_IsVisible( hV ) = false), _
                    "Show returned " & bShown & ", visible " & PsTooltip_IsVisible( hV ) )
            TipChk( "show callback: the showing edge fired exactly once", _
                    (gTipShowEdges = 1), "edges = " & gTipShowEdges )
            TipChk( "show callback: a VETOED show fires no matching hide (edges stay paired)", _
                    (gTipHideEdges = 0), "hide edges = " & gTipHideEdges )

            gTipShowEdges = 0 : gTipHideEdges = 0 : gTipAllowShow = true
            bShown = PsTooltip_Show( hV )
            TipChk( "show callback: returning TRUE lets the tip through", _
                    bShown andalso PsTooltip_IsVisible( hV ), _
                    "Show returned " & bShown & ", visible " & PsTooltip_IsVisible( hV ) )
            PsTooltip_Hide( hV )
            TipChk( "show callback: hiding fires the closing edge and clears visibility", _
                    (gTipHideEdges = 1) andalso (PsTooltip_IsVisible( hV ) = false), _
                    "hide edges = " & gTipHideEdges & ", visible " & PsTooltip_IsVisible( hV ) )

            DestroyWindow( hV )
        end if
    end scope

    ' ====================================================================================
    ' 6c. THE TICK -- driven through PsTooltip_TickForTest
    '
    ' Everything below was UNREACHABLE by any assertion until the seam existed, because the
    ' tick needs a live pump and a real cursor. A defect duly shipped in it: a veto that held
    ' for one 100 ms tick and was then retried, so the tip appeared anyway. The suite passed
    ' 76/76 with that bug present.
    '
    ' Timestamps are offsets from a REAL GetTickCount base -- hide bookkeeping stamps itself
    ' from the real clock, and a synthetic dwNow near zero would wrap the unsigned arithmetic.
    ' ====================================================================================
    print ""
    print "-- the tick --"

    scope
        dim as HWND hK = PsTooltip_Create( hwnd )
        if hK = 0 then
            TipChk( "tick: could not create a test tip", false, "Create returned 0" )
        else
            PsTooltip_SetFonts( hK, ghFont(GUIFONT_9), 0, 0 )
            PsTooltip_SetText( hK, "Tick" )
            PsTooltip_SetFadeTime( hK, 0 )            ' hides complete synchronously
            PsTooltip_SetInitialDelay( hK, 500 )
            PsTooltip_SetAutoPopDelay( hK, 3000 )
            PsTooltip_SetMoveTolerance( hK, 3 )

            dim as POINT pt = type<POINT>( 400, 400 )
            dim as DWORD t0 = GetTickCount()

            ' --- the dwell ---
            PsTooltip_TickForTest( hK, pt, true, false, t0 )
            TipChk( "tick: arriving does not show a tip immediately", _
                    (PsTooltip_IsVisible( hK ) = false), "visible " & PsTooltip_IsVisible( hK ) )

            PsTooltip_TickForTest( hK, pt, true, false, t0 + 300 )
            TipChk( "tick: still nothing before the initial delay elapses", _
                    (PsTooltip_IsVisible( hK ) = false), "at +300ms of a 500ms delay" )

            PsTooltip_TickForTest( hK, pt, true, false, t0 + 600 )
            TipChk( "tick: the tip appears once the cursor has rested for the initial delay", _
                    PsTooltip_IsVisible( hK ), "at +600ms of a 500ms delay" )

            ' --- movement inside the tool neither moves nor dismisses a tip that is up ---
            PsTooltip_TickForTest( hK, type<POINT>( 460, 430 ), true, false, t0 + 700 )
            TipChk( "tick: moving within the control leaves a visible tip alone", _
                    PsTooltip_IsVisible( hK ), "moved 60,30 while shown" )

            ' --- auto-pop ---
            PsTooltip_TickForTest( hK, type<POINT>( 460, 430 ), true, false, t0 + 4000 )
            TipChk( "tick: auto-pop dismisses the tip after its delay", _
                    (PsTooltip_IsVisible( hK ) = false), "at +4000ms of a 3000ms auto-pop" )
            PsTooltip_TickForTest( hK, type<POINT>( 460, 430 ), true, false, t0 + 6000 )
            TipChk( "tick: after an auto-pop it stays down while the cursor remains inside", _
                    (PsTooltip_IsVisible( hK ) = false), "still over the control 2s later" )

            ' --- leaving clears the suppression ---
            PsTooltip_TickForTest( hK, type<POINT>( 900, 900 ), false, false, t0 + 6100 )
            dim as DWORD t1 = GetTickCount()
            PsTooltip_TickForTest( hK, pt, true, false, t1 )
            PsTooltip_TickForTest( hK, pt, true, false, t1 + 900 )
            TipChk( "tick: leaving and returning lets a tip show again", _
                    PsTooltip_IsVisible( hK ), "after a leave + a fresh dwell" )

            ' --- a held button hides it and suppresses until the cursor leaves ---
            PsTooltip_TickForTest( hK, pt, true, true, t1 + 1000 )
            TipChk( "tick: a held mouse button hides the tip", _
                    (PsTooltip_IsVisible( hK ) = false), "bButtonDown = true" )
            PsTooltip_TickForTest( hK, pt, true, false, t1 + 2000 )
            PsTooltip_TickForTest( hK, pt, true, false, t1 + 3000 )
            TipChk( "tick: after a click it stays down until the cursor leaves", _
                    (PsTooltip_IsVisible( hK ) = false), "button released, still inside" )

            DestroyWindow( hK )
        end if
    end scope

    scope
        ' THE REGRESSION. A refused show must suppress the REST OF THE HOVER, not one tick.
        ' Shipped broken: the veto held for exactly one 100 ms tick, the dwell conditions were
        ' all still true, and the attempt simply repeated -- so the tip appeared a tenth of a
        ' second late and the callback was consulted twice per hover. Both halves are asserted:
        ' the tip stays down, AND the host is asked only once.
        dim as HWND hV2 = PsTooltip_Create( hwnd )
        if hV2 = 0 then
            TipChk( "tick/veto: could not create a test tip", false, "Create returned 0" )
        else
            PsTooltip_SetFonts( hV2, ghFont(GUIFONT_9), 0, 0 )
            PsTooltip_SetText( hV2, "Vetoed" )
            PsTooltip_SetFadeTime( hV2, 0 )
            PsTooltip_SetInitialDelay( hV2, 500 )
            PsTooltip_SetShowCallback( hV2, @TipShowCb )

            dim as POINT pt = type<POINT>( 400, 400 )
            dim as DWORD t0 = GetTickCount()
            gTipShowEdges = 0 : gTipHideEdges = 0 : gTipAllowShow = false

            PsTooltip_TickForTest( hV2, pt, true, false, t0 )
            PsTooltip_TickForTest( hV2, pt, true, false, t0 + 600 )   ' first attempt -> vetoed
            TipChk( "tick/veto: a vetoed show leaves the tip down", _
                    (PsTooltip_IsVisible( hV2 ) = false), "visible " & PsTooltip_IsVisible( hV2 ) )

            for i as long = 1 to 8
                PsTooltip_TickForTest( hV2, pt, true, false, t0 + 700 + (i * 100) )
            next
            TipChk( "tick/veto: THE VETO COVERS THE HOVER, not one tick (no retry)", _
                    (PsTooltip_IsVisible( hV2 ) = false), _
                    "8 further ticks over 800ms, still down" )
            TipChk( "tick/veto: the host is consulted ONCE per hover, not once per tick", _
                    (gTipShowEdges = 1), "showing edges = " & gTipShowEdges & " over 9 ticks" )

            ' Leaving is what re-offers it.
            PsTooltip_TickForTest( hV2, type<POINT>( 900, 900 ), false, false, t0 + 1600 )
            gTipAllowShow = true
            dim as DWORD t2 = GetTickCount()
            PsTooltip_TickForTest( hV2, pt, true, false, t2 )
            PsTooltip_TickForTest( hV2, pt, true, false, t2 + 900 )
            TipChk( "tick/veto: leaving clears the suppression and the next hover is offered", _
                    PsTooltip_IsVisible( hV2 ) andalso (gTipShowEdges = 2), _
                    "visible " & PsTooltip_IsVisible( hV2 ) & ", edges " & gTipShowEdges )

            PsTooltip_Hide( hV2 )
            DestroyWindow( hV2 )
        end if
    end scope

    ' ====================================================================================
    ' 7. THE REGISTRY -- add, and remove a MIDDLE entry from FOUR
    ' ====================================================================================
    print ""
    print "-- registry --"

    scope
        ' FOUR entries, removing the SECOND. Three entries removing the middle one is the
        ' shape that tests NOTHING: the middle of three is the last-but-one, and for that
        ' entry compaction and swap-with-last produce an identical array. PsOptionButton
        ' shipped exactly that assertion and it passed under both implementations.
        dim as long nSaveCount = gTIP_Count
        dim as HWND hSave(0 to 63)
        for i as long = 0 to nSaveCount - 1
            if i > 63 then exit for
            hSave(i) = gTIP_Registry(i)
        next

        gTIP_Count = 0
        TIP_RegistryAdd( cast(HWND, 1001) )
        TIP_RegistryAdd( cast(HWND, 1002) )
        TIP_RegistryAdd( cast(HWND, 1003) )
        TIP_RegistryAdd( cast(HWND, 1004) )
        TipChk( "registry: four adds give four entries in order", _
                (gTIP_Count = 4) andalso (gTIP_Registry(0) = cast(HWND,1001)) andalso _
                (gTIP_Registry(3) = cast(HWND,1004)), "count " & gTIP_Count )

        TIP_RegistryRemove( cast(HWND, 1002) )
        dim as boolean bOrder = false
        if gTIP_Count = 3 then
            if gTIP_Registry(0) = cast(HWND,1001) then
                if gTIP_Registry(1) = cast(HWND,1003) then
                    if gTIP_Registry(2) = cast(HWND,1004) then bOrder = true
                end if
            end if
        end if
        TipChk( "registry: removing the SECOND of four COMPACTS (1,3,4), not swaps (1,4,3)", _
                bOrder, "count " & gTIP_Count & " -> " & cast(integer, gTIP_Registry(0)) & "," & _
                cast(integer, gTIP_Registry(1)) & "," & cast(integer, gTIP_Registry(2)) )

        TIP_RegistryRemove( cast(HWND, 9999) )
        TipChk( "registry: removing something that was never added is a no-op", _
                (gTIP_Count = 3), "count " & gTIP_Count )

        gTIP_Count = nSaveCount
        for i as long = 0 to nSaveCount - 1
            if i > 63 then exit for
            gTIP_Registry(i) = hSave(i)
        next
    end scope

    scope
        ' The clock is created with the first attach and released with the last. It is the
        ' only thing keeping the whole control alive, so "does it exist" is worth asserting
        ' rather than assuming.
        dim as HWND hA = PsTooltip_Create( hwnd )
        TipChk( "clock: exists once a tip is attached", (gTIP_hClock <> 0) andalso IsWindow(gTIP_hClock), _
                "hClock = " & cast(integer, gTIP_hClock) )
        TipChk( "clock: runs at the IDLE rate when nothing is fading", _
                (gTIP_ClockMs = PSTOOLTIP_CLOCK_IDLE_MS), gTIP_ClockMs & " ms" )
        TipChk( "attach: the tip knows what it is attached to", _
                (PsTooltip_GetAttached( hA ) = hwnd), "attached = " & cast(integer, PsTooltip_GetAttached(hA)) )
        PsTooltip_Detach( hA )
        TipChk( "detach: clears the attachment", (PsTooltip_GetAttached( hA ) = 0), "after Detach" )
        DestroyWindow( hA )
    end scope


    ' ====================================================================================
    ' 7b. PROCESS-WIDE DEFAULTS
    '
    ' The point of this block is that a control creates its own tip out of the host's sight,
    ' so what is being asserted is that PsTooltip_Create APPLIES the armed defaults -- not
    ' merely that the setters store them. Every assertion therefore reads a tip back through
    ' its public getter after Create, never the gTIP_Def* storage.
    '
    ' The second half is the one that matters and is easy to get wrong: an UNARMED default
    ' must leave Create's own derivation standing. A "defaults" mechanism built as one struct
    ' would zero the three GetDoubleClickTime()-derived delays for a host that only wanted to
    ' change the colours, and nothing would report it.
    ' ====================================================================================
    print ""
    print "-- process-wide defaults --"

    scope
        PsTooltip_ClearDefaults()

        ' Baseline: with nothing armed, Create derives the delays from the system.
        dim as long nDbl = GetDoubleClickTime()
        if nDbl <= 0 then nDbl = 500
        dim as HWND hD0 = PsTooltip_Create( hwnd )
        if hD0 = 0 then
            TipChk( "defaults: could not create a test tip", false, "PsTooltip_Create returned 0" )
        else
            dim as PSTOOLTIP ptr p0 = GetTooltipPointer( hD0 )
            TipChk( "defaults: unarmed -- Create still derives the initial delay from the system", _
                    (p0->nInitialDelay = nDbl), "initial = " & p0->nInitialDelay & " ms, GetDoubleClickTime = " & nDbl )
            TipChk( "defaults: unarmed -- style is TIP_STYLE_RECT", _
                    (PsTooltip_GetStyle( hD0 ) = TIP_STYLE_RECT), "style = " & PsTooltip_GetStyle( hD0 ) )
            TipChk( "defaults: unarmed -- max width is 0 (no wrap)", _
                    (PsTooltip_GetMaxWidth( hD0 ) = 0), "maxwidth = " & PsTooltip_GetMaxWidth( hD0 ) )
            DestroyWindow( hD0 )
        end if

        ' Arm colours, style, width and the INITIAL delay only. The other two delays are left
        ' unarmed on purpose -- that is the half of this that the one-struct design fails.
        dim as PSTOOLTIP_COLORS cDef
        cDef.BackColor   = BGR(  1,  2,  3)
        cDef.BorderColor = BGR(  4,  5,  6)
        cDef.ForeColor   = BGR(  7,  8,  9)
        cDef.TitleColor  = BGR( 10, 11, 12)
        cDef.GlyphColor  = BGR( 13, 14, 15)
        PsTooltip_SetDefaultColors( @cDef )
        PsTooltip_SetDefaultStyle( TIP_STYLE_BALLOON )
        PsTooltip_SetDefaultMaxWidth( 321 )
        PsTooltip_SetDefaultFonts( ghFont(GUIFONT_9), ghFont(GUIFONTBOLD_10), ghFont(GUIFONT_9) )
        PsTooltip_SetDefaultDelays( 777 )

        dim as HWND hD1 = PsTooltip_Create( hwnd )
        if hD1 = 0 then
            TipChk( "defaults: could not create a second test tip", false, "PsTooltip_Create returned 0" )
        else
            dim as PSTOOLTIP ptr p1 = GetTooltipPointer( hD1 )
            dim as PSTOOLTIP_COLORS cGot
            PsTooltip_GetColors( hD1, @cGot )

            ' Five separate fields, because a partial copy is exactly what a hand-written
            ' apply would leave behind.
            dim as boolean bAllFive = (cGot.BackColor   = cDef.BackColor)   andalso _
                                      (cGot.BorderColor = cDef.BorderColor) andalso _
                                      (cGot.ForeColor   = cDef.ForeColor)   andalso _
                                      (cGot.TitleColor  = cDef.TitleColor)  andalso _
                                      (cGot.GlyphColor  = cDef.GlyphColor)
            TipChk( "defaults: a NEW tip carries the armed colours, all five fields", _
                    bAllFive, "back " & cGot.BackColor & " border " & cGot.BorderColor & _
                              " fore " & cGot.ForeColor & " title " & cGot.TitleColor & _
                              " glyph " & cGot.GlyphColor )
            TipChk( "defaults: a NEW tip carries the armed style", _
                    (PsTooltip_GetStyle( hD1 ) = TIP_STYLE_BALLOON), "style = " & PsTooltip_GetStyle( hD1 ) )
            TipChk( "defaults: a NEW tip carries the armed max width", _
                    (PsTooltip_GetMaxWidth( hD1 ) = 321), "maxwidth = " & PsTooltip_GetMaxWidth( hD1 ) )
            TipChk( "defaults: a NEW tip carries the armed fonts", _
                    (p1->hTextFont = ghFont(GUIFONT_9)) andalso (p1->hTitleFont = ghFont(GUIFONTBOLD_10)), _
                    "text/title fonts installed" )
            TipChk( "defaults: a NEW tip carries the armed initial delay", _
                    (p1->nInitialDelay = 777), "initial = " & p1->nInitialDelay & " ms" )

            ' THE ASSERTION THIS BLOCK EXISTS FOR.
            TipChk( "defaults: an UNARMED delay keeps Create's system derivation (autopop)", _
                    (p1->nAutoPopDelay = nDbl * 10), "autopop = " & p1->nAutoPopDelay & " ms, expected " & (nDbl * 10) )
            TipChk( "defaults: an UNARMED delay keeps Create's system derivation (reshow)", _
                    (p1->nReshowDelay = nDbl \ 5), "reshow = " & p1->nReshowDelay & " ms, expected " & (nDbl \ 5) )
            DestroyWindow( hD1 )
        end if

        ' ApplyDefaults is the retro-fit door, so it has to work on a tip that ALREADY exists
        ' and was created before anything was armed.
        PsTooltip_ClearDefaults()
        dim as HWND hD2 = PsTooltip_Create( hwnd )
        if hD2 <> 0 then
            TipChk( "defaults: precondition -- the pre-existing tip does NOT have the armed width", _
                    (PsTooltip_GetMaxWidth( hD2 ) <> 654), "maxwidth = " & PsTooltip_GetMaxWidth( hD2 ) )
            PsTooltip_SetDefaultMaxWidth( 654 )
            PsTooltip_ApplyDefaults( hD2 )
            TipChk( "defaults: ApplyDefaults retro-fits a tip created before they were armed", _
                    (PsTooltip_GetMaxWidth( hD2 ) = 654), "maxwidth = " & PsTooltip_GetMaxWidth( hD2 ) )
            DestroyWindow( hD2 )
        end if

        ' Leave the process as it was found, or every tip the DEMO creates afterwards inherits
        ' a balloon with 1/2/3 colours and the run looks broken for a reason with nothing to
        ' do with the control.
        PsTooltip_ClearDefaults()
        dim as HWND hD3 = PsTooltip_Create( hwnd )
        TipChk( "defaults: ClearDefaults disarms -- a later tip is derived again", _
                (PsTooltip_GetMaxWidth( hD3 ) = 0), "a fresh tip is back to no-wrap" )
        if hD3 then DestroyWindow( hD3 )
    end scope


    ' ====================================================================================
    ' 8. RENDER PROBES
    '
    ' Both drive PsTooltip_Render, the same function WM_PAINT calls.
    '
    ' THE TONE FLOORS BELOW ARE MEASURED FOR THIS PAINTER, NOT INHERITED. PsButton's and
    ' PsCheckBox's `> 1` threshold passed on a completely destroyed render in both
    ' PsMessageBox and PsOptionButton, because the number depends on what else is inside the
    ' part rect. The values here were read off a healthy render and off a deliberately
    ' flooded one, and the floor put between them.
    ' ====================================================================================
    print ""
    print "-- render probes --"

    scope
        dim as HWND hR = PsTooltip_Create( hwnd )
        if hR = 0 then
            TipChk( "probes: could not create a test tip", false, "PsTooltip_Create returned 0" )
        else
            PsTooltip_SetFonts( hR, ghFont(GUIFONT_9), ghFont(GUIFONTBOLD_10), ghFont(GUIFONT_9) )
            PsTooltip_SetText( hR, "Rendered offscreen" )

            ' Give the body a real rect: nothing has been shown, so rcBody is still empty and
            ' the probe would have nothing to read. This is the lazy-layout trap -- a test
            ' that reads DERIVED state has to force the derivation first (Learnings.md).
            dim as long wR, hRt
            PsTooltip_GetIdealSize( hR, wR, hRt )
            dim as PSTOOLTIP ptr pR = GetTooltipPointer( hR )
            SetRect( @pR->rcBody, 0, 0, wR, hRt )

            dim as long nBody = PsTooltip_CountRenderedTones( hR, TIP_PART_BODY )
            dim as long nText = PsTooltip_CountRenderedTones( hR, TIP_PART_TEXT )

            ' THESE TWO FLOORS ARE MEASURED, AND THE BODY ONE IS DELIBERATELY COARSE.
            '
            ' A/B, drawing PaintRoundBorderRect over the finished render (the family's
            ' five-time flood bug): the body went 107 -> 50 and the text band 55 -> 1. The body
            ' keeps FIFTY tones on a completely destroyed render, because its rounded corners
            ' are antialiased against the surround and that survives any amount of flooding --
            ' so a floor of 8, which is what a sibling's number would have suggested, PASSES on
            ' a render with nothing left in it. It is calibrated between the two measurements
            ' instead. The text band is the assertion with real discriminating power.
            TipChk( "probe: the body render survives (frame + corners + content)", _
                    (nBody >= 80), "distinct tones in the body = " & nBody & "  (healthy 107, flooded 50)" )
            TipChk( "probe: the text band carries antialiased glyphs", _
                    (nText >= 8), "distinct tones in the text band = " & nText & "  (healthy 55, flooded 1)" )

            ' ISOLATED -- and the first version of this was NOT.
            '
            ' Written the obvious way (set a different string, re-measure, re-render, compare
            ' hashes) it PASSED with the text drawing removed entirely, because two different
            ' strings give two different ideal widths, so rcBody moved and the corner
            ' antialiasing inside the part rect moved with it. The hash changed for a reason
            ' that had nothing to do with the text. PsCheckBox hit the identical shape.
            '
            ' So: a fixed max width and a FIXED rcBody across both renders, and a PRECONDITION
            ' that the text part rect did not move between them. With the rect pinned, nothing
            ' but the glyphs can account for a different hash. (PsMessageBox's fix for its icon
            ' assertion was the same move -- assert the thing you are holding constant really
            ' is constant.)
            PsTooltip_SetMaxWidth( hR, 180 )
            PsTooltip_SetText( hR, "ABCDEFGH" )
            PsTooltip_GetIdealSize( hR, wR, hRt )
            dim as RECT rcFixed = type( 0, 0, wR, hRt )
            pR->rcBody = rcFixed
            dim as RECT rcTextA = PsTooltip_GetPartRect( hR, TIP_PART_TEXT )
            dim as ulong h1 = PsTooltip_HashRenderedPart( hR, TIP_PART_TEXT )

            PsTooltip_SetText( hR, "HGFEDCBA" )
            PsTooltip_GetIdealSize( hR, wR, hRt )
            pR->rcBody = rcFixed                     ' pinned: do NOT follow the new ideal size
            dim as RECT rcTextB = PsTooltip_GetPartRect( hR, TIP_PART_TEXT )
            dim as ulong h2 = PsTooltip_HashRenderedPart( hR, TIP_PART_TEXT )

            TipChk( "probe: precondition -- the text part rect did NOT move between the two renders", _
                    EqualRect( @rcTextA, @rcTextB ) <> 0, _
                    TipRectStr(rcTextA) & " vs " & TipRectStr(rcTextB) )
            TipChk( "probe: changing the text reaches the PIXELS (rect pinned, glyphs the only variable)", _
                    (h1 <> h2), "hash " & hex(h1,8) & " -> " & hex(h2,8) )

            DestroyWindow( hR )
        end if
    end scope


    ' ------------------------------------------------------------------------------------
    ' PsTooltip_GetSystemFont
    '
    ' The VALUES are the user's Windows appearance setting and cannot be asserted -- any
    ' expected face or size here would be a claim about the machine, not about this code. What
    ' IS assertable is the contract: a true return means a usable answer, and the point size is
    ' the pixel height converted against the real screen DPI. That conversion is the only
    ' arithmetic in the function and the only thing that can be wrong at 175% while looking
    ' right at 100%, so it is recomputed here independently rather than read back.
    scope
        dim as PSTOOLTIP_SYSTEMFONT sf
        dim as boolean bGot = PsTooltip_GetSystemFont( sf )

        dim as long dpiY = 0
        dim as HDC hDCm = GetDC( HWND_DESKTOP )
        if hDCm then
            dpiY = GetDeviceCaps( hDCm, LOGPIXELSY )
            ReleaseDC( HWND_DESKTOP, hDCm )
        end if

        TipChk( "sysfont: the system answered", bGot, _
                "face=" & str(sf.wszFaceName) & " " & sf.pointSize & "pt / " & sf.pixelHeight & "px" & _
                "  weight=" & sf.lfWeight & "  dpiY=" & dpiY )

        if bGot then
            TipChk( "sysfont: a true return means a usable face name", _
                    len(sf.wszFaceName) > 0, "len=" & len(sf.wszFaceName) )
            TipChk( "sysfont: a true return means a usable point size", _
                    sf.pointSize > 0, "pointSize=" & sf.pointSize )
            TipChk( "sysfont: pixelHeight is positive (abs of lfHeight, which is negative)", _
                    sf.pixelHeight > 0, "pixelHeight=" & sf.pixelHeight )
            ' THE ONE THAT MATTERS. Break the conversion -- hand back pixelHeight as the point
            ' size, the natural mistake -- and this goes red on any display above 100% while
            ' every other assertion here stays green.
            if dpiY > 0 then
                TipChk( "sysfont: pointSize = pixelHeight converted against the REAL screen DPI", _
                        sf.pointSize = MulDiv( sf.pixelHeight, 72, dpiY ), _
                        "got " & sf.pointSize & "  want " & MulDiv( sf.pixelHeight, 72, dpiY ) & _
                        "  (from " & sf.pixelHeight & "px at " & dpiY & "dpi)" )
                ' Stated separately because the equality above is satisfied trivially at 96dpi,
                ' where a point and a pixel-derived point happen to differ by only 25%.
                TipChk( "sysfont: pointSize is NOT the raw pixel height (vacuous at 100% DPI)", _
                        (dpiY <= 96) orelse (sf.pointSize <> sf.pixelHeight), _
                        "pt=" & sf.pointSize & " px=" & sf.pixelHeight & " dpi=" & dpiY )
            end if
            TipChk( "sysfont: isBold agrees with the raw weight it was derived from", _
                    sf.isBold = (sf.lfWeight >= FW_BOLD), _
                    "isBold=" & sf.isBold & " lfWeight=" & sf.lfWeight )
        end if
    end scope


    dim as string sSummary = "=== " & (gTipRun - gTipFailCount) & " of " & gTipRun & " assertions passed"
    if gTipFailCount > 0 then sSummary &= "  (" & gTipFailCount & " FAILED)"
    sSummary &= " ==="
    print ""
    print sSummary
end sub
