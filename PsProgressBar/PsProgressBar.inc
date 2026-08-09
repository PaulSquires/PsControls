'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

''
''  PsProgressBar.inc  --  owner-drawn progress bar (track + fill, determinate or floating)
''

#include once "PsProgressBar.bi"
' The tooltip backend switch, pulled in here so a host needs no extra include line.
#include once "PsTipHost.inc"


function GetProgressBarPointer( byval hProgressBar as HWND ) as PSPROGRESSBAR ptr
    dim pWindow as CWindow ptr = AfxCWindowPtr(hProgressBar)
    if pWindow = 0 then return 0
    return cast( PSPROGRESSBAR ptr, pWindow->UserData(0) )
end function


' ========================================================================================
' THE PURE FUNCTIONS
'
' No window, no state, no timer. Everything about how the bar is sized and where the floating
' chunk sits lives in these three, precisely so the self-test can drive them exhaustively --
' a full marquee period, both styles, every degenerate range -- without a message pump.
' ========================================================================================

' Bar length for a determinate position.
'
' The longint cast is load-bearing rather than defensive: a host tracking bytes copied can
' legitimately set nMax to a few hundred million, and (nPos - nMin) * nTrackLen overflows a
' 32-bit long long before nPos gets anywhere near it.
function PsProgressBar_ComputeBarLength( byval nPos as long, _
                                         byval nMin as long, _
                                         byval nMax as long, _
                                         byval nTrackLen as long ) as long
    if nTrackLen <= 0 then return 0
    ' An empty or inverted range has no meaningful position. Answer 0 rather than dividing by
    ' zero -- SetRange clamps, but a host can reach the fields through GetRange/SetRange in
    ' either order and pass through this state legitimately.
    if nMax <= nMin then return 0

    dim as long p = nPos
    if p < nMin then p = nMin
    if p > nMax then p = nMax

    dim as longint num = cast( longint, p - nMin ) * cast( longint, nTrackLen )
    dim as long n = clng( num \ cast( longint, nMax - nMin ) )
    if n < 0 then n = 0
    if n > nTrackLen then n = nTrackLen
    return n
end function


' Leading edge of the indeterminate chunk at tick nTick, as an offset from the start of the
' track along the growth axis.
'
' THE TWO STYLES HAVE DIFFERENT RANGES, and that is deliberate rather than an oversight:
'
'   BOUNCE  returns a value in [0, nTrackLen - nChunkLen]. The chunk never leaves the track,
'           so nothing is ever clipped. The travel is out and back, so the PERIOD IS TWICE
'           the span -- a triangle wave. Getting that factor of two wrong is the classic bug
'           here (the chunk teleports from one end to the other instead of returning), which
'           is why the self-test walks a whole period rather than sampling it.
'
'   WRAP    returns a value in [-nChunkLen, nTrackLen), i.e. it goes NEGATIVE. That is what
'           makes the chunk genuinely slide in from one edge and out of the other, comctl32's
'           PBS_MARQUEE behaviour, instead of snapping back. THE CALLER MUST CLIP the
'           resulting rect to the track -- LayoutProgressBar does.
'
' Negative ticks are handled: the modulo is normalised, so a host that runs its own clock
' backwards gets the mirror animation rather than a chunk parked off-screen.
function PsProgressBar_ComputeMarqueeOffset( byval nTrackLen as long, _
                                             byval nChunkLen as long, _
                                             byval nStep as long, _
                                             byval nTick as long, _
                                             byval nStyle as long ) as long
    if nTrackLen <= 0 then return 0
    if nChunkLen <= 0 then return 0

    dim as long stp = nStep
    if stp < 1 then stp = 1

    dim as longint travel = cast( longint, nTick ) * cast( longint, stp )

    if nStyle = PRG_MARQUEE_WRAP then
        ' One period carries the chunk from entirely before the track to entirely past it.
        dim as longint period = cast( longint, nTrackLen ) + cast( longint, nChunkLen )
        dim as longint p = travel mod period
        if p < 0 then p += period
        return clng( p ) - nChunkLen
    end if

    ' BOUNCE. The chunk stays inside, so the span it can travel is what is left over.
    dim as long span = nTrackLen - nChunkLen
    if span <= 0 then return 0

    dim as longint period = cast( longint, span ) * 2
    dim as longint p = travel mod period
    if p < 0 then p += period
    ' Triangle wave: out on the first half of the period, back on the second.
    if p <= cast( longint, span ) then return clng( p )
    return clng( period - p )
end function


' How many whole blocks of nBlockLen, separated by nBlockGap, fit inside nLength.
'
' The +gap in the numerator is what makes the last block not need a trailing gap:
' n blocks occupy n*len + (n-1)*gap, and asking how many fit is the same as asking how many
' (len + gap) strides fit into (nLength + gap).
function PsProgressBar_ComputeBlockCount( byval nLength as long, _
                                          byval nBlockLen as long, _
                                          byval nBlockGap as long ) as long
    if nBlockLen <= 0 then return 0
    if nLength < nBlockLen then return 0
    dim as long gap = nBlockGap
    if gap < 0 then gap = 0
    dim as long stride = nBlockLen + gap
    if stride <= 0 then return 0
    return (nLength + gap) \ stride
end function


' ========================================================================================
' Hand a message to the host's message callback. Returns TRUE if the host claims it.
' ========================================================================================
private function PsProgressBar_CallMessage( _
            byval pBar   as PSPROGRESSBAR ptr, _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as boolean

    if pBar = 0 then return false
    if pBar->MessageCallback = 0 then return false

    dim as PSPROGRESSBAR_MESSAGEINFO m
    m.hProgressBar = hwnd
    m.uMsg         = uMsg
    m.wParam       = wParam
    m.lParam       = lParam
    return pBar->MessageCallback( @m )
end function


' ========================================================================================
' The position as a whole-number percentage of the range. 0 for an empty range.
' ========================================================================================
private function PRG_Percent( byval pBar as PSPROGRESSBAR ptr ) as long
    if pBar = 0 then return 0
    if pBar->nMax <= pBar->nMin then return 0
    dim as long p = pBar->nPos
    if p < pBar->nMin then p = pBar->nMin
    if p > pBar->nMax then p = pBar->nMax
    dim as longint num = cast( longint, p - pBar->nMin ) * 100
    return clng( num \ cast( longint, pBar->nMax - pBar->nMin ) )
end function


' ========================================================================================
' The text to draw, mode and callback already applied. "" means draw nothing.
' ========================================================================================
private function PRG_ResolveText( byval pBar as PSPROGRESSBAR ptr ) as DWSTRING
    dim as DWSTRING wszOut
    if pBar = 0 then return wszOut

    select case pBar->nTextMode
    case PRG_TEXT_PERCENT
        dim as string s = str( PRG_Percent( pBar ) ) & "%"
        wszOut = s
    case PRG_TEXT_VALUE
        dim as string s = str( pBar->nPos )
        wszOut = s
    case PRG_TEXT_CALLBACK
        if pBar->TextCallback then
            wszOut = pBar->TextCallback( pBar->hWin, pBar->nPos, pBar->nMin, pBar->nMax )
        end if
    end select

    return wszOut
end function


' ========================================================================================
' Where block idx sits inside the CURRENT bar rect.
'
' Blocks tile from the bar's OWN growth-start edge, not from the track's. For a determinate
' bar those are the same edge, so the blocks sit on a fixed grid and a rising position simply
' lights more of them. For an indeterminate one the chunk is what moves, and tiling from its
' edge makes the blocks travel WITH it instead of the chunk sliding over a stationary grid.
' One rule, both behaviours -- which is why there is no mode test in here.
' ========================================================================================
private function PRG_BlockRectAt( byval pBar as PSPROGRESSBAR ptr, _
                                  byval idx as long, _
                                  byref rc as RECT ) as boolean
    SetRect( @rc, 0, 0, 0, 0 )
    if pBar = 0 then return false
    if idx < 0 then return false

    dim as long blockLen = pBar->nBlockLength
    if blockLen <= 0 then return false
    dim as long gap = pBar->nBlockGap
    if gap < 0 then gap = 0
    dim as long stride = blockLen + gap
    dim as long ofs = idx * stride

    rc = pBar->rcBar
    if pBar->nOrientation = PRG_VERTICAL then
        if pBar->isReverse then
            rc.top    = pBar->rcBar.top + ofs
            rc.bottom = rc.top + blockLen
            if rc.bottom > pBar->rcBar.bottom then return false
        else
            rc.bottom = pBar->rcBar.bottom - ofs
            rc.top    = rc.bottom - blockLen
            if rc.top < pBar->rcBar.top then return false
        end if
    else
        if pBar->isReverse then
            rc.right = pBar->rcBar.right - ofs
            rc.left  = rc.right - blockLen
            if rc.left < pBar->rcBar.left then return false
        else
            rc.left  = pBar->rcBar.left + ofs
            rc.right = rc.left + blockLen
            if rc.right > pBar->rcBar.right then return false
        end if
    end if
    return true
end function


' The extent of the drawn bar along the growth axis. Used for the block count, so it has to
' agree with PRG_BlockRectAt's bounds test exactly.
private function PRG_BarLength( byval pBar as PSPROGRESSBAR ptr ) as long
    if pBar = 0 then return 0
    dim as long n
    if pBar->nOrientation = PRG_VERTICAL then
        n = pBar->rcBar.bottom - pBar->rcBar.top
    else
        n = pBar->rcBar.right - pBar->rcBar.left
    end if
    if n < 0 then n = 0
    return n
end function


' ========================================================================================
' Resolve the bar's two gradient stops from the state. Precedence: DISABLED beats everything,
' then the PBM_SETSTATE value. PRG_FILL_SOLID uses clr1 only.
'
' Split out and exposed (PsProgressBar_ResolveBarColors wraps it) because a precedence table
' is exactly the kind of thing that looks obviously right and is quietly wrong in one cell.
' ========================================================================================
private sub PRG_ResolveColors( byval pBar as PSPROGRESSBAR ptr, _
                               byref clr1 as COLORREF, _
                               byref clr2 as COLORREF )
    clr1 = 0 : clr2 = 0
    if pBar = 0 then exit sub

    with pBar->colors
        if pBar->isEnabled = false then
            clr1 = .BarColorDisabled : clr2 = .BarColorDisabledEnd
            exit sub
        end if
        select case pBar->nState
        case PRG_STATE_PAUSED
            clr1 = .BarColorPaused : clr2 = .BarColorPausedEnd
        case PRG_STATE_ERROR
            clr1 = .BarColorError  : clr2 = .BarColorErrorEnd
        case else
            clr1 = .BarColor       : clr2 = .BarColorEnd
        end select
    end with
end sub


' ========================================================================================
' Draw the track, the bar and the text.
' ========================================================================================
private sub PsProgressBar_RenderDefault( _
            byval pBar as PSPROGRESSBAR ptr, _
            byval b    as PsBufferPaint ptr, _
            byval p    as PSPROGRESSBAR_PAINTINFO ptr _
            )

    if (pBar = 0) orelse (b = 0) orelse (p = 0) then exit sub

    dim as long curv = p->nCurvature

    ' ---- the track ---------------------------------------------------------------------
    dim as RECT rcTrack = p->rcTrack
    if (rcTrack.right > rcTrack.left) andalso (rcTrack.bottom > rcTrack.top) then
        b->SetBackColor( pBar->colors.TrackColor )
        if pBar->nBorderThick > 0 then
            b->SetPenColor( pBar->colors.TrackBorderColor )
            b->PaintRoundBorderRect( @rcTrack, curv, pBar->nBorderThick )
        else
            b->PaintRoundRect( @rcTrack, curv )
        end if
    end if

    ' ---- the bar -------------------------------------------------------------------------
    dim as COLORREF clr1, clr2
    PRG_ResolveColors( pBar, clr1, clr2 )

    dim as RECT rcBar = p->rcBar
    dim as boolean bHasBar = (rcBar.right > rcBar.left) andalso (rcBar.bottom > rcBar.top)
    dim as boolean bGrad   = (p->nFillStyle = PRG_FILL_GRADIENT)

    if bHasBar then
        if p->isSegmented then
            dim as long n = PsProgressBar_ComputeBlockCount( PRG_BarLength( pBar ), _
                                                             pBar->nBlockLength, pBar->nBlockGap )
            for k as long = 0 to n - 1
                dim as RECT rcBlk
                if PRG_BlockRectAt( pBar, k, rcBlk ) = false then exit for
                if bGrad then
                    ' rcBar as the RAMP rect, the block as the fill: each block takes its own
                    ' slice of one gradient spanning the whole run, so the blocks read as a
                    ' single bar that happens to be perforated rather than as N identical chips.
                    b->PaintGradientRect( @rcBlk, clr1, clr2, pBar->nGradientMode, curv, @rcBar )
                else
                    b->SetBackColor( clr1 )
                    b->PaintRoundRect( @rcBlk, curv )
                end if
            next
        else
            if bGrad then
                b->PaintGradientRect( @rcBar, clr1, clr2, pBar->nGradientMode, curv )
            else
                b->SetBackColor( clr1 )
                b->PaintRoundRect( @rcBar, curv )
            end if
        end if
    end if

    ' ---- the text ------------------------------------------------------------------------
    ' Drawn LAST, after every GDI+ primitive above, and that ordering is required rather than
    ' tidy: the split below clips with a GDI region, and a GDI clip does not constrain GDI+
    ' (which keeps its own). Anything geometric drawn after this point would ignore it.
    if len( p->wszText ) = 0 then exit sub
    if (rcTrack.right <= rcTrack.left) orelse (rcTrack.bottom <= rcTrack.top) then exit sub

    ' SetFont(0) is a documented no-op in PsBufferPaint, so a host that never called SetFont
    ' gets the DC's own font rather than a crash or a blank.
    b->SetFont( pBar->hFont )

    if p->isEnabled = false then
        ' One colour: a disabled bar has no contrast problem to solve.
        b->SetForeColor( pBar->colors.TextColorDisabled )
        b->PaintText( p->wszText, @rcTrack, DT_CENTER )
        exit sub
    end if

    ' TWO CLIPPED PASSES OF THE SAME STRING INTO THE SAME RECT. Because the rect is identical
    ' both times, every glyph lands in exactly the same place and the only thing that changes
    ' at the fill boundary is the colour -- including mid-glyph, which is the whole point. A
    ' single-colour label washes out precisely where the fill edge crosses under it.
    '
    ' getMemDC() flushes the GDI+ batch on the way out, so the geometry above is on the
    ' surface before the first DrawText runs.
    dim as HDC hdc = b->getMemDC()
    if hdc = 0 then exit sub

    if bHasBar then
        dim as HRGN hrgnBar = CreateRectRgn( rcBar.left, rcBar.top, rcBar.right, rcBar.bottom )
        if hrgnBar then
            ' pass A -- the part of the string lying over the filled bar
            SelectClipRgn( hdc, hrgnBar )
            b->SetForeColor( pBar->colors.TextColorOnBar )
            b->PaintText( p->wszText, @rcTrack, DT_CENTER )

            ' pass B -- everything else. A DIFFERENCE region, not a second rect: in
            ' indeterminate mode the chunk floats in the MIDDLE of the track, so what is left
            ' over is two disjoint pieces and no single rect describes it.
            dim as HRGN hrgnTrack = CreateRectRgn( rcTrack.left, rcTrack.top, rcTrack.right, rcTrack.bottom )
            dim as HRGN hrgnOut   = CreateRectRgn( 0, 0, 1, 1 )
            if (hrgnTrack <> 0) andalso (hrgnOut <> 0) then
                CombineRgn( hrgnOut, hrgnTrack, hrgnBar, RGN_DIFF )
                SelectClipRgn( hdc, hrgnOut )
                b->SetForeColor( pBar->colors.TextColor )
                b->PaintText( p->wszText, @rcTrack, DT_CENTER )
            end if
            if hrgnTrack then DeleteObject( hrgnTrack )
            if hrgnOut   then DeleteObject( hrgnOut )
            DeleteObject( hrgnBar )
        end if
    else
        b->SetForeColor( pBar->colors.TextColor )
        b->PaintText( p->wszText, @rcTrack, DT_CENTER )
    end if

    ' Reset explicitly. The WM_PAINT path builds its own memDC and never SaveDC's it, so
    ' nothing else would put the clip back.
    SelectClipRgn( hdc, NULL )
end sub


' ========================================================================================
' Render the whole control into a buffer the CALLER owns.
'
' This split is the reason the offscreen probes are worth anything. PsDatePicker shipped a
' tone probe that passed at 59 tones while its popup was rendering as one flat rectangle,
' because the probe drove a single day cell through the host callback and never ran the real
' painter. Everything below WM_PAINT lives here instead, so a probe can drive the exact code
' path a repaint does.
' ========================================================================================
private sub PRG_Render( byval pBar as PSPROGRESSBAR ptr, byval b as PsBufferPaint ptr )
    if (pBar = 0) orelse (b = 0) then exit sub

    ' Consume any pending layout before painting -- this is what makes layout lazy.
    if pBar->bLayoutDirty then pBar->LayoutProgressBar()

    b->SetBackColor( pBar->colors.BackColor )
    b->PaintClientRect()

    dim p as PSPROGRESSBAR_PAINTINFO
    p.hProgressBar = pBar->hWin
    p.b            = b
    GetClientRect( pBar->hWin, @p.rcClient )
    p.rcTrack         = pBar->rcTrack
    p.rcFill          = pBar->rcFill
    p.rcBar           = pBar->rcBar
    p.nPos            = pBar->nPos
    p.nMin            = pBar->nMin
    p.nMax            = pBar->nMax
    p.nPercent        = PRG_Percent( pBar )
    p.isIndeterminate = (pBar->nMode = PRG_MODE_INDETERMINATE)
    p.isEnabled       = pBar->isEnabled
    p.isReverse       = pBar->isReverse
    p.isSegmented     = pBar->isSegmented
    p.nOrientation    = pBar->nOrientation
    p.nState          = pBar->nState
    p.nFillStyle      = pBar->nFillStyle
    ' Resolved here, so the stadium sentinel never reaches a paint callback. A host painter
    ' would otherwise have to know what -1 means and re-derive it from a rect we already have.
    p.nCurvature      = pBar->ResolvedCurvature()
    p.nBlockLength    = pBar->nBlockLength
    p.nBlockGap       = pBar->nBlockGap
    p.wszText         = PRG_ResolveText( pBar )

    if pBar->PaintCallback then
        pBar->PaintCallback( @p )
    else
        PsProgressBar_RenderDefault( pBar, b, @p )
    end if
end sub


' ========================================================================================
' Process WM_PAINT. A thin wrapper: it owns the buffer and nothing else.
' ========================================================================================
private function PsProgressBar_OnPaint( byval hwnd as HWND ) as LRESULT
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hwnd)
    if pBar = null then return 0

    dim as PsBufferPaint b
    b.BeginDoubleBuffer(hwnd)
    PRG_Render( pBar, @b )
    b.EndDoubleBuffer()
    function = 0
end function


' ========================================================================================
' Start or stop the marquee timer to match the current state.
'
' Called from every place that can change the answer. The rule is deliberately NOT "is the
' window visible": IsWindowVisible walks the ancestor chain, so a control created on a
' parent that has not been shown yet reports hidden, and a bar set to indeterminate during
' form construction would never start (this is the trap PsScrollPanel hit, recorded in
' Learnings.md). Hiding is tracked from WM_SHOWWINDOW instead, which only ever fires on a
' real transition.
' ========================================================================================
private sub PRG_SyncMarqueeTimer( byval pBar as PSPROGRESSBAR ptr, byval hwnd as HWND )
    if pBar = 0 then exit sub

    dim as boolean bIndeterminate = (pBar->nMode = PRG_MODE_INDETERMINATE)
    dim as boolean bShown         = (pBar->bHidden = false)
    dim as boolean bWant          = bIndeterminate andalso pBar->isEnabled andalso bShown

    if bWant andalso (pBar->marqueeTimerOn = false) then
        dim as long ms = pBar->nMarqueeMs
        if ms < 1 then ms = 1
        SetTimer( hwnd, IDT_CPROGRESSBAR_MARQUEE, ms, NULL )
        pBar->marqueeTimerOn = true
    elseif (bWant = false) andalso pBar->marqueeTimerOn then
        KillTimer( hwnd, IDT_CPROGRESSBAR_MARQUEE )
        pBar->marqueeTimerOn = false
    end if
end sub


' ========================================================================================
' Resolve the tooltip text: the control's own authored text first, the callback only if there
' is none. THERE IS NO FALLBACK to the displayed percentage -- a tip that repeated the label
' already on screen would be noise. A bar with neither shows nothing.
' ========================================================================================
private function PsProgressBar_ResolveTooltipText( byval pBar as PSPROGRESSBAR ptr ) as DWSTRING
    dim as DWSTRING wszOut
    if pBar = 0 then return wszOut
    if len( pBar->wszTooltip ) then
        wszOut = pBar->wszTooltip
        return wszOut
    end if
    if pBar->TooltipCallback then return pBar->TooltipCallback( pBar->hWin )
    return wszOut
end function


' ========================================================================================
' Supply the tooltip text to the PsTooltip backend.
'
' This thunk is why PsTipHost never learns the text policy: PsProgressBar_ResolveTooltipText
' stays the ONE place that decides what a tip says, and the system backend reaches the same
' function by the other road (comctl32 asking through TTN_GETDISPINFOW).
' ========================================================================================
private function PsProgressBar_TipTextCallback( byval hTooltip as HWND, byval hAttached as HWND, _
                                                byval idx as long ) as DWSTRING
    dim as DWSTRING wszOut = PsProgressBar_ResolveTooltipText( GetProgressBarPointer(hAttached) )
    return wszOut
end function


' ========================================================================================
' PsProgressBar_WndProc
'
' NO CAPTURE, NO FOCUS, NO CS_DBLCLKS, NO HOT STATE. The family's capture test is "does
' something here consume the guaranteed WM_LBUTTONDOWN -> WM_LBUTTONUP pairing?", and for a
' control that cannot be acted on the answer is plainly no -- so there is no
' WM_CAPTURECHANGED handler, no CancelPress, and no hot-tracking poll timer either. A "no
' capture" note copied from a sibling is an assumption to re-test rather than a law to
' inherit; this one was re-tested.
'
' The one timer here is the marquee, and it only runs in indeterminate mode.
' ========================================================================================
function PsProgressBar_WndProc( _
            byval hwnd   as HWND, _
            byval uMsg   as UINT, _
            byval wParam as WPARAM, _
            byval lParam as LPARAM _
            ) as LRESULT

    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer( hwnd )

    ' Offer the message to the host FIRST, and honour the answer. The two destroy messages are
    ' deliberately never offered: they free the instance data and the tooltip window, and a
    ' callback suppressing either would leak both.
    select case uMsg
    case WM_DESTROY, WM_NCDESTROY
        ' not offered
    case else
        if PsProgressBar_CallMessage( pBar, hwnd, uMsg, wParam, lParam ) then return 0
    end select

    select case uMsg

    case WM_PAINT
        return PsProgressBar_OnPaint( hwnd )

    case WM_ERASEBKGND
        ' Never erase: WM_PAINT fills the whole client through the double buffer, so a
        ' background erase here would only add flicker.
        return true

    case WM_SIZE
        if pBar then pBar->Refresh()
        return 0

    case WM_TIMER
        if (pBar <> 0) andalso (wParam = IDT_CPROGRESSBAR_MARQUEE) then
            ' The tick is the animation's ONLY clock. Everything about where the chunk sits is
            ' derived from it by a pure function, so advancing it is the whole of the update.
            pBar->nTick += 1
            pBar->bLayoutDirty = true
            ' No background erase: the chunk moves within a track that is repainted whole
            ' anyway, and erasing at 30ms would flicker.
            InvalidateRect( hwnd, NULL, FALSE )
            return 0
        end if

    case WM_SHOWWINDOW
        if pBar then
            pBar->bHidden = (wParam = 0)
            PRG_SyncMarqueeTimer( pBar, hwnd )
        end if

    case WM_ENABLE
        ' Keeps our own flag true to the window's real state, so a host that reaches for
        ' EnableWindow directly instead of PsProgressBar_SetEnabled still gets the greyed
        ' rendering -- and still stops the animation.
        if pBar then
            pBar->isEnabled = (wParam <> 0)
            PRG_SyncMarqueeTimer( pBar, hwnd )
            InvalidateRect( hwnd, NULL, FALSE )
        end if

    case WM_NOTIFY
        ' The tooltip asks for its text on demand; supply it.
        dim as NMHDR ptr pnmh = cast(NMHDR ptr, lParam)
        if (pnmh <> 0) andalso (pnmh->code = TTN_GETDISPINFOW) then
            if pBar then
                dim as NMTTDISPINFOW ptr pdi = cast(NMTTDISPINFOW ptr, lParam)
                ' Store in an instance-lifetime buffer and point lpszText at it. NOT in
                ' wszTooltip: that field is the host's authored text, and a callback's answer
                ' written there would silently become stored state.
                pBar->wszTipBuf = PsProgressBar_ResolveTooltipText( pBar )
                pdi->lpszText = cast( LPWSTR, pBar->wszTipBuf.vptr )
                pdi->hinst    = NULL
            end if
            return 0
        end if

    case WM_DESTROY
        if pBar then
            ' Don't let the marquee timer outlive the window. There is no capture to release
            ' and no press to cancel -- this control never took either.
            if pBar->marqueeTimerOn then
                KillTimer( hwnd, IDT_CPROGRESSBAR_MARQUEE )
                pBar->marqueeTimerOn = false
            end if
        end if

    case WM_NCDESTROY
        ' Delete the stored data pointer. The font is the host's and is left alone; the
        ' tooltip window is ours and goes with us.
        if pBar then
            PsTipHost_Destroy( @pBar->tip )
            delete(pBar)
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
' PsProgressBar_Create
' ========================================================================================
function PsProgressBar_Create( byval hWndParent as HWND, _
                               byval CtrlID as long, _
                               byval nOrientation as long = PRG_HORIZONTAL ) as HWND

    dim pWindow as CWindow ptr = New CWindow

    ' NO WS_TABSTOP: this control cannot be acted on, so it must not sit in the tab order.
    '
    ' THE TRAILING 0 IS THE EXTENDED STYLE, AND IT IS NOT OPTIONAL. CWindow.Create defaults
    ' dwExStyle to WS_EX_CONTROLPARENT OR WS_EX_WINDOWEDGE, so passing only dwStyle silently
    ' declares the control a CONTAINER -- which cost PsButton, PsToggle and PsComboBox their
    ' Tab navigation for months before anyone measured it. It is harmless here (nothing in
    ' this control is focusable either way), but it is passed explicitly so the family stays
    ' uniform and so the next control seeded from this one inherits the correct habit.
    '
    ' DO NOT COPY THIS INTO A WRAP-A-REAL-CHILD CONTROL. PsListBox, PsTextBox, PsNumericUpDown
    ' and PsScrollPanel all NEED WS_EX_CONTROLPARENT, because their tabstop lives on an inner
    ' child. The test is "does this window have children the dialog manager should find?".
    dim as HWND hCtrl = _
            pWindow->Create( hWndParent, "", @PsProgressBar_WndProc, _
            0, 0, 0, 0, WS_CHILD or WS_CLIPSIBLINGS or WS_CLIPCHILDREN, _
            0 _
            )

    ' NOTE: CS_DBLCLKS is deliberately NOT enabled -- nothing here reacts to a click, so the
    ' extra message would only be one more thing for a host callback to have to ignore.

    dim as PSPROGRESSBAR ptr pBar = new PSPROGRESSBAR
    pBar->hWin            = hCtrl
    pBar->idc_ProgressBar = CtrlID

    ' Orientation is Create-time and immutable (PsSplitter's rule). Anything not recognised
    ' falls back to horizontal rather than leaving a control in an undrawable state.
    if nOrientation = PRG_VERTICAL then
        pBar->nOrientation = PRG_VERTICAL
    else
        pBar->nOrientation = PRG_HORIZONTAL
    end if

    ' Only the Create-time defaults are DPI-scaled for us; every setter afterwards takes raw
    ' pixels and the caller scales (the family rule). nBorderThick and nCurvature are NOT
    ' scaled at all -- a hairline should stay a hairline, which is PsMenuBar's
    ' nSeparatorThickness rule, and the curvature is either a sentinel or a corner size the
    ' caller chose in the same units as the rect it applies to.
    pBar->nThickness   = pWindow->ScaleY( PSPROGRESSBAR_DEFAULT_THICKNESS )
    pBar->nBarInset    = pWindow->ScaleY( PSPROGRESSBAR_DEFAULT_BARINSET )
    pBar->nBlockLength = pWindow->ScaleX( PSPROGRESSBAR_DEFAULT_BLOCKLEN )
    pBar->nBlockGap    = pWindow->ScaleX( PSPROGRESSBAR_DEFAULT_BLOCKGAP )
    ' The marquee step is scaled so the chunk crosses the bar in the same TIME at any DPI.
    ' Left unscaled it would crawl on a high-DPI display, where the track is physically wider.
    pBar->nMarqueeStep = pWindow->ScaleX( PSPROGRESSBAR_DEFAULT_MARQUEE_STEP )
    if pBar->nMarqueeStep < 1 then pBar->nMarqueeStep = 1


    SetWindowLongPtr( hCtrl, GWLP_ID, CtrlID )

    ' Store the new pointer into the user data area of the control window
    pWindow->UserData(0) = cast(LONG_PTR, pBar)

    ' AFTER UserData, not before: PsProgressBar_TipTextCallback resolves the control through
    ' GetProgressBarPointer(hAttached), which reads exactly that slot.
    PsTipHost_Ensure( @pBar->tip, hCtrl, @PsProgressBar_TipTextCallback )

    function = hCtrl

end function


sub PsProgressBar_Refresh( byval hProgressBar as HWND )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->Refresh()
end sub

function PsProgressBar_GetOrientation( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return PRG_HORIZONTAL
    return pBar->nOrientation
end function


' ========================================================================================
' Model. EVERY setter here is silent -- see the PSPROGRESSBAR type header for why there is
' no change callback at all.
' ========================================================================================
sub PsProgressBar_GetRange( byval hProgressBar as HWND, byref nMin as long, byref nMax as long )
    nMin = 0 : nMax = 0
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    nMin = pBar->nMin
    nMax = pBar->nMax
end sub

' An inverted range is normalised rather than rejected, and the position is re-clamped into
' the new range -- otherwise SetRange could leave a bar reading past its own end.
sub PsProgressBar_SetRange( byval hProgressBar as HWND, byval nMin as long, byval nMax as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long lo = nMin, hi = nMax
    if hi < lo then swap lo, hi
    pBar->nMin = lo
    pBar->nMax = hi
    if pBar->nPos < lo then pBar->nPos = lo
    if pBar->nPos > hi then pBar->nPos = hi
    pBar->Refresh()
end sub

function PsProgressBar_GetPos( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->nPos
end function

sub PsProgressBar_SetPos( byval hProgressBar as HWND, byval nPos as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long p = nPos
    if p < pBar->nMin then p = pBar->nMin
    if p > pBar->nMax then p = pBar->nMax
    if p = pBar->nPos then exit sub          ' nothing moved; do not repaint
    pBar->nPos = p
    pBar->Refresh()
end sub

function PsProgressBar_GetStep( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->nStep
end function

sub PsProgressBar_SetStep( byval hProgressBar as HWND, byval nStep as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->nStep = nStep
end sub

function PsProgressBar_StepIt( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    PsProgressBar_SetPos( hProgressBar, pBar->nPos + pBar->nStep )
    return pBar->nPos
end function

function PsProgressBar_DeltaPos( byval hProgressBar as HWND, byval nDelta as long ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    PsProgressBar_SetPos( hProgressBar, pBar->nPos + nDelta )
    return pBar->nPos
end function

function PsProgressBar_GetPercent( byval hProgressBar as HWND ) as long
    return PRG_Percent( GetProgressBarPointer(hProgressBar) )
end function


' ========================================================================================
' Mode / state
' ========================================================================================
function PsProgressBar_GetMode( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return PRG_MODE_DETERMINATE
    return pBar->nMode
end function

sub PsProgressBar_SetMode( byval hProgressBar as HWND, byval nMode as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long m = PRG_MODE_DETERMINATE
    if nMode = PRG_MODE_INDETERMINATE then m = PRG_MODE_INDETERMINATE
    if m = pBar->nMode then exit sub
    pBar->nMode = m
    ' Restart the animation from the beginning rather than resuming wherever the last
    ' indeterminate spell happened to stop -- a chunk that reappears mid-track reads as a
    ' glitch.
    pBar->nTick = 0
    PRG_SyncMarqueeTimer( pBar, pBar->hWin )
    pBar->Refresh()
end sub

function PsProgressBar_GetState( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return PRG_STATE_NORMAL
    return pBar->nState
end function

sub PsProgressBar_SetState( byval hProgressBar as HWND, byval nState as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long s = nState
    if (s < PRG_STATE_NORMAL) orelse (s > PRG_STATE_ERROR) then s = PRG_STATE_NORMAL
    if s = pBar->nState then exit sub
    pBar->nState = s
    ' Colours only -- no geometry changed, so this does not have to go through Refresh().
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetEnabled( byval hProgressBar as HWND ) as boolean
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    return pBar->isEnabled
end function

' Goes through EnableWindow rather than setting a cosmetic flag, so the disable is enforced
' by the system and WM_ENABLE syncs our own field back (PsToggle's rule).
sub PsProgressBar_SetEnabled( byval hProgressBar as HWND, byval isEnabled as boolean )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    if pBar->isEnabled = isEnabled then exit sub
    EnableWindow( hProgressBar, iif( isEnabled, TRUE, FALSE ) )
    ' Belt and braces: if the window is already gone WM_ENABLE never arrives.
    pBar->isEnabled = isEnabled
    PRG_SyncMarqueeTimer( pBar, hProgressBar )
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub


' ========================================================================================
' Marquee
' ========================================================================================
function PsProgressBar_GetMarqueeStyle( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return PRG_MARQUEE_BOUNCE
    return pBar->nMarqueeStyle
end function

sub PsProgressBar_SetMarqueeStyle( byval hProgressBar as HWND, byval nStyle as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long s = PRG_MARQUEE_BOUNCE
    if nStyle = PRG_MARQUEE_WRAP then s = PRG_MARQUEE_WRAP
    if s = pBar->nMarqueeStyle then exit sub
    pBar->nMarqueeStyle = s
    pBar->nTick = 0
    pBar->Refresh()
end sub

sub PsProgressBar_GetMarqueeSpeed( byval hProgressBar as HWND, byref nIntervalMs as long, byref nStepPx as long )
    nIntervalMs = 0 : nStepPx = 0
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    nIntervalMs = pBar->nMarqueeMs
    nStepPx     = pBar->nMarqueeStep
end sub

sub PsProgressBar_SetMarqueeSpeed( byval hProgressBar as HWND, byval nIntervalMs as long, byval nStepPx as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    if nIntervalMs > 0 then pBar->nMarqueeMs = nIntervalMs
    if nStepPx > 0 then pBar->nMarqueeStep = nStepPx
    ' The interval only takes effect when the timer is (re)created, so bounce it if it is
    ' already running.
    if pBar->marqueeTimerOn then
        KillTimer( hProgressBar, IDT_CPROGRESSBAR_MARQUEE )
        pBar->marqueeTimerOn = false
        PRG_SyncMarqueeTimer( pBar, hProgressBar )
    end if
    pBar->Refresh()
end sub

function PsProgressBar_GetMarqueeChunkPercent( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->nChunkPercent
end function

sub PsProgressBar_SetMarqueeChunkPercent( byval hProgressBar as HWND, byval nPercent as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long n = nPercent
    if n < 1 then n = 1
    if n > 100 then n = 100
    pBar->nChunkPercent = n
    pBar->Refresh()
end sub


' ========================================================================================
' Fill style and segmentation
' ========================================================================================
function PsProgressBar_GetFillStyle( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return PRG_FILL_SOLID
    return pBar->nFillStyle
end function

sub PsProgressBar_SetFillStyle( byval hProgressBar as HWND, byval nFillStyle as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long f = PRG_FILL_SOLID
    if nFillStyle = PRG_FILL_GRADIENT then f = PRG_FILL_GRADIENT
    if f = pBar->nFillStyle then exit sub
    pBar->nFillStyle = f
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetGradientMode( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return LinearGradientModeVertical
    return pBar->nGradientMode
end function

sub PsProgressBar_SetGradientMode( byval hProgressBar as HWND, byval nMode as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long m = nMode
    if (m < LinearGradientModeHorizontal) orelse (m > LinearGradientModeBackwardDiagonal) then
        m = LinearGradientModeVertical
    end if
    pBar->nGradientMode = m
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetSegmented( byval hProgressBar as HWND ) as boolean
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    return pBar->isSegmented
end function

sub PsProgressBar_SetSegmented( byval hProgressBar as HWND, byval isSegmented as boolean )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    if pBar->isSegmented = isSegmented then exit sub
    pBar->isSegmented = isSegmented
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

sub PsProgressBar_GetBlockMetrics( byval hProgressBar as HWND, byref nBlockLen as long, byref nBlockGap as long )
    nBlockLen = 0 : nBlockGap = 0
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    nBlockLen = pBar->nBlockLength
    nBlockGap = pBar->nBlockGap
end sub

sub PsProgressBar_SetBlockMetrics( byval hProgressBar as HWND, byval nBlockLen as long, byval nBlockGap as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    if nBlockLen > 0 then pBar->nBlockLength = nBlockLen
    if nBlockGap >= 0 then pBar->nBlockGap = nBlockGap
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetBlockCount( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    if pBar->isSegmented = false then return 0
    if pBar->bLayoutDirty then pBar->LayoutProgressBar()
    return PsProgressBar_ComputeBlockCount( PRG_BarLength( pBar ), pBar->nBlockLength, pBar->nBlockGap )
end function

function PsProgressBar_GetBlockRect( byval hProgressBar as HWND, byval idx as long, byref rc as RECT ) as boolean
    SetRect( @rc, 0, 0, 0, 0 )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    if pBar->isSegmented = false then return false
    if pBar->bLayoutDirty then pBar->LayoutProgressBar()
    return PRG_BlockRectAt( pBar, idx, rc )
end function


' ========================================================================================
' Geometry / chrome
' ========================================================================================
function PsProgressBar_GetThickness( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->nThickness
end function

sub PsProgressBar_SetThickness( byval hProgressBar as HWND, byval nThickness as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long n = nThickness
    if n < 0 then n = 0
    pBar->nThickness = n
    pBar->Refresh()
end sub

function PsProgressBar_GetBarInset( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->nBarInset
end function

sub PsProgressBar_SetBarInset( byval hProgressBar as HWND, byval nBarInset as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long n = nBarInset
    if n < 0 then n = 0
    pBar->nBarInset = n
    pBar->Refresh()
end sub

function PsProgressBar_GetBorderThickness( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->nBorderThick
end function

sub PsProgressBar_SetBorderThickness( byval hProgressBar as HWND, byval nThickness as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long n = nThickness
    if n < 0 then n = 0
    pBar->nBorderThick = n
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetCurvature( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->nCurvature
end function

' Any negative value is normalised to the stadium sentinel, so a host cannot store -3 and
' then wonder why GetCurvature does not read back what it wrote.
sub PsProgressBar_SetCurvature( byval hProgressBar as HWND, byval nCurvature as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long n = nCurvature
    if n < 0 then n = PSPROGRESSBAR_CURVATURE_STADIUM
    pBar->nCurvature = n
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetReverse( byval hProgressBar as HWND ) as boolean
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    return pBar->isReverse
end function

sub PsProgressBar_SetReverse( byval hProgressBar as HWND, byval isReverse as boolean )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    if pBar->isReverse = isReverse then exit sub
    pBar->isReverse = isReverse
    pBar->Refresh()
end sub

function PsProgressBar_GetTrackRect( byval hProgressBar as HWND, byref rc as RECT ) as boolean
    SetRect( @rc, 0, 0, 0, 0 )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    if pBar->bLayoutDirty then pBar->LayoutProgressBar()
    rc = pBar->rcTrack
    return true
end function

function PsProgressBar_GetFillRect( byval hProgressBar as HWND, byref rc as RECT ) as boolean
    SetRect( @rc, 0, 0, 0, 0 )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    if pBar->bLayoutDirty then pBar->LayoutProgressBar()
    rc = pBar->rcFill
    return true
end function

function PsProgressBar_GetBarRect( byval hProgressBar as HWND, byref rc as RECT ) as boolean
    SetRect( @rc, 0, 0, 0, 0 )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    if pBar->bLayoutDirty then pBar->LayoutProgressBar()
    rc = pBar->rcBar
    return true
end function


' ========================================================================================
' Text
' ========================================================================================
function PsProgressBar_GetTextMode( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return PRG_TEXT_NONE
    return pBar->nTextMode
end function

sub PsProgressBar_SetTextMode( byval hProgressBar as HWND, byval nTextMode as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    dim as long m = nTextMode
    if (m < PRG_TEXT_NONE) orelse (m > PRG_TEXT_CALLBACK) then m = PRG_TEXT_NONE
    if m = pBar->nTextMode then exit sub
    pBar->nTextMode = m
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetFont( byval hProgressBar as HWND ) as HFONT
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return pBar->hFont
end function

' BORROWED, not owned. The caller keeps the handle and must outlive the control -- the family
' rule, and the reason no control here ever calls CreateFont.
sub PsProgressBar_SetFont( byval hProgressBar as HWND, byval hFont as HFONT )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->hFont = hFont
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

function PsProgressBar_GetDisplayText( byval hProgressBar as HWND ) as DWSTRING
    return PRG_ResolveText( GetProgressBarPointer(hProgressBar) )
end function


' ========================================================================================
' Colors
' ========================================================================================
sub PsProgressBar_GetColors( byval hProgressBar as HWND, byval pColors as PSPROGRESSBAR_COLORS ptr )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if (pBar = 0) orelse (pColors = 0) then exit sub
    *pColors = pBar->colors
end sub

sub PsProgressBar_SetColors( byval hProgressBar as HWND, byval pColors as PSPROGRESSBAR_COLORS ptr )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if (pBar = 0) orelse (pColors = 0) then exit sub
    pBar->colors = *pColors
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

sub PsProgressBar_ResolveBarColors( byval hProgressBar as HWND, byref clr1 as COLORREF, byref clr2 as COLORREF )
    PRG_ResolveColors( GetProgressBarPointer(hProgressBar), clr1, clr2 )
end sub


' ========================================================================================
' Tooltips
' ========================================================================================
function PsProgressBar_GetTooltipText( byval hProgressBar as HWND ) as DWSTRING
    dim as DWSTRING wszOut
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return wszOut
    wszOut = pBar->wszTooltip
    return wszOut
end function

sub PsProgressBar_SetTooltipText( byval hProgressBar as HWND, byval Text as DWSTRING )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->wszTooltip = Text
end sub

function PsProgressBar_GetTooltipHandle( byval hProgressBar as HWND ) as HWND
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return PsTipHost_SysHandle( @pBar->tip )
end function

function PsProgressBar_GetPsTooltipHandle( byval hProgressBar as HWND ) as HWND
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return 0
    return PsTipHost_PsHandle( @pBar->tip )
end function

function PsProgressBar_SetTooltipMode( byval hProgressBar as HWND, byval nMode as long ) as boolean
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    return PsTipHost_SetMode( @pBar->tip, nMode )
end function

function PsProgressBar_GetTooltipMode( byval hProgressBar as HWND ) as long
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return PSTIP_MODE_SYSTEM
    return PsTipHost_GetMode( @pBar->tip )
end function

' The three delays go through PsTipHost, which STORES them as well as pushing them, so one set
' before a mode switch is still in force after it.
sub PsProgressBar_SetHoverTime( byval hProgressBar as HWND, byval milliseconds as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    PsTipHost_SetDelay( @pBar->tip, TTDT_INITIAL, milliseconds )
end sub

sub PsProgressBar_SetAutoPopTime( byval hProgressBar as HWND, byval milliseconds as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    PsTipHost_SetDelay( @pBar->tip, TTDT_AUTOPOP, milliseconds )
end sub

sub PsProgressBar_SetReshowTime( byval hProgressBar as HWND, byval milliseconds as long )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    PsTipHost_SetDelay( @pBar->tip, TTDT_RESHOW, milliseconds )
end sub


' ========================================================================================
' Callbacks
' ========================================================================================
sub PsProgressBar_SetPaintCallback( byval hProgressBar as HWND, byval usersub as PRG_PaintCallbackSub )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->PaintCallback = usersub
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub

sub PsProgressBar_SetMessageCallback( byval hProgressBar as HWND, byval userfunc as PRG_MessageCallbackFunc )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->MessageCallback = userfunc
end sub

sub PsProgressBar_SetTooltipCallback( byval hProgressBar as HWND, byval userfunc as PRG_TooltipCallbackFunc )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->TooltipCallback = userfunc
end sub

sub PsProgressBar_SetTextCallback( byval hProgressBar as HWND, byval userfunc as PRG_TextCallbackFunc )
    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then exit sub
    pBar->TextCallback = userfunc
    if pBar->hWin then InvalidateRect( pBar->hWin, NULL, FALSE )
end sub


' ========================================================================================
' OFFSCREEN PROBES
'
' Render the control into a DIB we own and measure the pixels. Two things make this worth
' anything rather than merely present:
'
'   1. IT DRIVES PRG_Render, THE EXACT PATH WM_PAINT USES. PsDatePicker shipped a probe that
'      passed at 59 tones while its popup was rendering as one flat rectangle, because the
'      probe drove a single day cell through the host callback and never ran the real painter.
'      Splitting the renderer away from the buffer ownership is what prevents that here.
'
'   2. Alpha is masked off. GDI text leaves it at 0 and GDI+ shapes leave it at 255, so an
'      unmasked compare reports two tones for one colour -- and would make the hash depend on
'      which renderer last touched a pixel rather than on what colour it is.
'
' Returns FALSE (and leaves both outputs at 0) if the control has no geometry yet or the part
' rect is empty.
' ========================================================================================
private function PRG_PartRect( byval pBar as PSPROGRESSBAR ptr, byval nPart as long ) as RECT
    dim as RECT rc
    if pBar = 0 then return rc
    select case nPart
    case PRG_PART_TRACK
        rc = pBar->rcTrack
    case PRG_PART_BAR
        rc = pBar->rcBar
    case PRG_PART_TEXT
        ' The middle third of the track, which is where a short centred string lands. NOT the
        ' whole track: a hash over the whole track would move whenever the BAR moved, and an
        ' assertion about the text would then pass for the wrong reason.
        rc = pBar->rcTrack
        dim as long w = rc.right - rc.left
        rc.left  += w \ 3
        rc.right -= w \ 3
    case else
        if pBar->hWin then GetClientRect( pBar->hWin, @rc )
    end select
    return rc
end function


private function PsProgressBar_ProbePart( _
            byval hProgressBar as HWND, _
            byval nPart        as long, _
            byref nTones       as long, _
            byref nHash        as ulong _
            ) as boolean

    nTones = 0
    nHash  = 0

    dim as PSPROGRESSBAR ptr pBar = GetProgressBarPointer(hProgressBar)
    if pBar = 0 then return false
    if pBar->bLayoutDirty then pBar->LayoutProgressBar()
    if pBar->bLayoutDirty then return false          ' no client area yet

    dim as RECT rcClient
    GetClientRect( hProgressBar, @rcClient )
    dim as long cw = rcClient.right - rcClient.left
    dim as long ch = rcClient.bottom - rcClient.top
    if (cw <= 0) orelse (ch <= 0) then return false

    dim as RECT rcPart = PRG_PartRect( pBar, nPart )
    if IsRectEmpty( @rcPart ) then return false

    ' Clamp to the client: the DIB only covers that.
    if rcPart.left   < 0  then rcPart.left   = 0
    if rcPart.top    < 0  then rcPart.top    = 0
    if rcPart.right  > cw then rcPart.right  = cw
    if rcPart.bottom > ch then rcPart.bottom = ch
    if (rcPart.right <= rcPart.left) orelse (rcPart.bottom <= rcPart.top) then return false

    dim as HDC hRef = GetDC( hProgressBar )
    if hRef = 0 then return false
    dim as HDC hMem = CreateCompatibleDC( hRef )
    if hMem = 0 then
        ReleaseDC( hProgressBar, hRef )
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
    dim as any ptr pBits
    dim as HBITMAP hbm = CreateDIBSection( hRef, @bmi, DIB_RGB_COLORS, @pBits, null, 0 )
    ReleaseDC( hProgressBar, hRef )
    if (hbm = 0) orelse (pBits = 0) then
        if hbm then DeleteObject( hbm )
        DeleteDC( hMem )
        return false
    end if
    dim as HGDIOBJ hOldBmp = SelectObject( hMem, hbm )

    scope
        dim as PsBufferPaint b
        ' Four-arg form: hMem is BOTH the blit target and the cached back buffer, so the final
        ' BitBlt is a self-copy and the bitmap survives EndDoubleBuffer for us to read.
        b.BeginDoubleBuffer( hProgressBar, hMem, rcClient, hMem )
        PRG_Render( pBar, @b )
        b.EndDoubleBuffer()
    end scope

    ' Count distinct tones and hash the same pixels in one pass.
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


' See the header in PsProgressBar.bi. A part wiped by a PaintBorderRect fill is literally ONE
' tone, so the useful signal is the count LEAVING 1 -- not the count being large.
function PsProgressBar_CountRenderedTones( byval hProgressBar as HWND, byval nPart as long ) as long
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsProgressBar_ProbePart( hProgressBar, nPart, nTones, nHash ) = false then return 0
    return nTones
end function

' Compare two hashes of the SAME part taken in two different states: equal means the state
' change never reached the surface. It can only prove difference, never correctness -- and it
' can prove difference for a reason other than the one the assertion is named for, if more
' than one variable moved. Isolate before trusting one. (0 is also the failure return; an
' FNV-1a of a non-empty region landing exactly on 0 is possible in principle and has been
' accepted rather than worked around.)
function PsProgressBar_HashRenderedPart( byval hProgressBar as HWND, byval nPart as long ) as ulong
    dim as long nTones = 0
    dim as ulong nHash = 0
    if PsProgressBar_ProbePart( hProgressBar, nPart, nTones, nHash ) = false then return 0
    return nHash
end function


' ========================================================================================
' Self-test.  Gated on PSPROGRESSBAR_SELFTEST=1.  Geometry dump on PSPROGRESSBAR_TRACE=1.
'
' Reports assertions RUN as well as failed: a harness that only counts failures cannot tell
' "all passed" from "barely executed" (Learnings.md).
'
' NOTE what the bulk of this exercises. Better than a third of it drives the three PURE
' functions with no window in sight -- a full marquee period for both styles, every degenerate
' range. That is deliberate: the motion law and the fill arithmetic are the two things most
' likely to be subtly wrong, and asserting them through a live control would have meant
' asserting them through a timer.
' ========================================================================================
dim shared as long gPRG_TestCount
dim shared as long gPRG_FailCount

' The throwaway controls the self-test creates get their ids from HERE, not from the demo's
' frmMain.bi. Naming a host constant would make this .inc compile only at a site that happened
' to define it -- i.e. only inside this repo -- and anyone who copied the four files into their
' own project would get "Variable not declared" from a file they never edited.
#define PSPROGRESSBAR_SELFTEST_IDC   32700

private sub PRGTest_Check( byval sName as string, byval bOK as boolean, byval sDetail as string )
    gPRG_TestCount += 1
    dim as string s
    if bOK then
        s = "PASS  "
    else
        s = "FAIL  "
        gPRG_FailCount += 1
    end if
    s &= sName & "   [" & sDetail & "]"
    print s
end sub

' Trace output goes through a "dim as string" first. print on a DWSTRING, or on a
' string & wstring concat, interleaves nulls (Learnings.md).
private sub PRGTest_TraceRect( byval sName as string, byref rc as RECT )
    if environ("PSPROGRESSBAR_TRACE") <> "1" then exit sub
    dim as string s = "  TRACE " & sName & " = (" & str(rc.left) & "," & str(rc.top) & _
                      ")-(" & str(rc.right) & "," & str(rc.bottom) & ")  " & _
                      str(rc.right - rc.left) & "x" & str(rc.bottom - rc.top)
    print s
end sub

private function PRGTest_TextCallback( byval hBar as HWND, byval nPos as long, _
                                       byval nMin as long, byval nMax as long ) as DWSTRING
    dim as DWSTRING wszOut
    dim as string s = "CB:" & str(nPos)
    wszOut = s
    return wszOut
end function


sub PsProgressBar_RunSelfTest( byval hWndParent as HWND )
    if environ("PSPROGRESSBAR_SELFTEST") <> "1" then exit sub

    print ""
    print "=== PsProgressBar self-test ==="

    gPRG_TestCount = 0
    gPRG_FailCount = 0

    ' ------------------------------------------------------------------------------------
    ' 1. ComputeBarLength -- pure, no window.
    ' ------------------------------------------------------------------------------------
    scope
        dim as long n

        n = PsProgressBar_ComputeBarLength( 0, 0, 100, 200 )
        PRGTest_Check( "BarLength: at minimum is empty", (n = 0), "len = " & str(n) )

        n = PsProgressBar_ComputeBarLength( 100, 0, 100, 200 )
        PRGTest_Check( "BarLength: at maximum fills the track", (n = 200), "len = " & str(n) )

        n = PsProgressBar_ComputeBarLength( 50, 0, 100, 200 )
        PRGTest_Check( "BarLength: halfway is half", (n = 100), "len = " & str(n) )

        ' A non-zero minimum is the case a host hits when it tracks bytes from an offset.
        n = PsProgressBar_ComputeBarLength( 150, 100, 200, 200 )
        PRGTest_Check( "BarLength: honours a non-zero minimum", (n = 100), "len = " & str(n) )

        n = PsProgressBar_ComputeBarLength( 50, 100, 100, 200 )
        PRGTest_Check( "BarLength: empty range answers 0, does not divide by zero", (n = 0), _
                       "len = " & str(n) )

        n = PsProgressBar_ComputeBarLength( 50, 100, 0, 200 )
        PRGTest_Check( "BarLength: inverted range answers 0", (n = 0), "len = " & str(n) )

        n = PsProgressBar_ComputeBarLength( -50, 0, 100, 200 )
        PRGTest_Check( "BarLength: below the range clamps to empty", (n = 0), "len = " & str(n) )

        n = PsProgressBar_ComputeBarLength( 5000, 0, 100, 200 )
        PRGTest_Check( "BarLength: above the range clamps to full", (n = 200), "len = " & str(n) )

        ' THE OVERFLOW CASE, and the reason the implementation casts to longint. Without it
        ' (pos - min) * trackLen is 2e11 in a 32-bit long and the bar reads garbage.
        n = PsProgressBar_ComputeBarLength( 1000000000, 0, 2000000000, 200 )
        PRGTest_Check( "BarLength: no 32-bit overflow on a huge range", (n = 100), _
                       "len = " & str(n) & " want 100" )

        n = PsProgressBar_ComputeBarLength( 50, 0, 100, 0 )
        PRGTest_Check( "BarLength: zero-length track answers 0", (n = 0), "len = " & str(n) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 2. ComputeMarqueeOffset, BOUNCE -- walked over a FULL PERIOD, not sampled.
    '    The classic bug here is a period of span rather than 2*span, which makes the chunk
    '    teleport back instead of returning. Sampling three ticks would not see it.
    ' ------------------------------------------------------------------------------------
    scope
        const TRACKLEN = 200
        const CHUNKLEN = 50
        const STEPPX   = 3
        dim as long span = TRACKLEN - CHUNKLEN                  ' 150
        dim as long period = (span * 2) \ STEPPX                ' ticks per period

        dim as long o = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, 0, PRG_MARQUEE_BOUNCE )
        PRGTest_Check( "Bounce: starts at the beginning", (o = 0), "offset = " & str(o) )

        dim as boolean bInRange = true
        dim as long nMaxSeen = -1
        dim as long nMinSeen = 999999
        dim as long nBiggestJump = 0
        dim as long prev = 0
        for t as long = 0 to (period * 2) + 8
            dim as long v = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t, PRG_MARQUEE_BOUNCE )
            if (v < 0) orelse (v > span) then bInRange = false
            if v > nMaxSeen then nMaxSeen = v
            if v < nMinSeen then nMinSeen = v
            if t > 0 then
                dim as long d = abs( v - prev )
                if d > nBiggestJump then nBiggestJump = d
            end if
            prev = v
        next
        PRGTest_Check( "Bounce: never leaves [0, span] over two periods", bInRange, _
                       "span = " & str(span) & " seen " & str(nMinSeen) & ".." & str(nMaxSeen) )
        PRGTest_Check( "Bounce: reaches the far end", (nMaxSeen = span), _
                       "max = " & str(nMaxSeen) & " want " & str(span) )
        PRGTest_Check( "Bounce: returns to the near end", (nMinSeen = 0), _
                       "min = " & str(nMinSeen) )

        ' THIS is the assertion that separates a bounce from a sawtooth, and the shape of it
        ' matters. The obvious version -- "some tick moved backwards" -- is USELESS: a sawtooth
        ' moves backwards too, once per period, when it wraps. It was written that way first and
        ' passed cleanly against a deliberately-broken sawtooth implementation.
        ' What actually distinguishes them is the SIZE of the largest single-tick move: a
        ' triangle wave never moves more than one step in either direction, while a sawtooth
        ' has exactly one jump of nearly the whole span.
        PRGTest_Check( "Bounce: reverses smoothly -- no single-tick jump, so it is not a wrap", _
                       (nBiggestJump <= STEPPX), _
                       "largest single-tick move = " & str(nBiggestJump) & " step = " & str(STEPPX) )

        ' Periodicity, and the same trap in the other direction. Asserting only
        ' "offset(t) = offset(t + 2*span/step)" is VACUOUS against the period-is-span bug,
        ' because span/step divides 2*span/step -- a function with the wrong, shorter period
        ' still repeats at the longer one. So the half-period is asserted to DIFFER as well.
        dim as long tp     = (span * 2) \ STEPPX      ' 100 ticks: the real period
        dim as long tpHalf = span \ STEPPX            '  50 ticks: what the classic bug gives
        dim as boolean bPeriodic = true
        dim as boolean bHalfDiffers = false
        for t as long = 0 to 40
            dim as long a = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t, PRG_MARQUEE_BOUNCE )
            dim as long b2 = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t + tp, PRG_MARQUEE_BOUNCE )
            dim as long c2 = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t + tpHalf, PRG_MARQUEE_BOUNCE )
            if a <> b2 then bPeriodic = false
            if a <> c2 then bHalfDiffers = true
        next
        PRGTest_Check( "Bounce: repeats after 2*span of travel", bPeriodic, _
                       "ticks per period = " & str(tp) )
        PRGTest_Check( "Bounce: does NOT repeat after only span -- out and back are different", _
                       bHalfDiffers, "half-period = " & str(tpHalf) & " ticks" )

        ' A chunk that fills or overfills the track has nowhere to go.
        o = PsProgressBar_ComputeMarqueeOffset( 100, 100, 3, 17, PRG_MARQUEE_BOUNCE )
        PRGTest_Check( "Bounce: a chunk as wide as the track cannot move", (o = 0), "offset = " & str(o) )
        o = PsProgressBar_ComputeMarqueeOffset( 100, 400, 3, 17, PRG_MARQUEE_BOUNCE )
        PRGTest_Check( "Bounce: an oversized chunk cannot move", (o = 0), "offset = " & str(o) )

        ' A host running its own clock backwards must get the mirror animation, not a chunk
        ' parked off-screen by a negative modulo.
        dim as boolean bNegOK = true
        for t as long = -60 to -1
            dim as long v = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t, PRG_MARQUEE_BOUNCE )
            if (v < 0) orelse (v > span) then bNegOK = false
        next
        PRGTest_Check( "Bounce: negative ticks stay in range", bNegOK, "ticks -60..-1" )

        ' A zero or negative step must not divide by zero or freeze.
        o = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, 0, 5, PRG_MARQUEE_BOUNCE )
        PRGTest_Check( "Bounce: a zero step is treated as 1", (o = 5), "offset = " & str(o) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 3. ComputeMarqueeOffset, WRAP.
    '    Its range is NOT the same as BOUNCE's -- it goes negative on purpose, so the chunk
    '    genuinely slides in from the edge rather than snapping back. That asymmetry is the
    '    thing most likely to be "corrected" by a later reader, so it is asserted explicitly.
    ' ------------------------------------------------------------------------------------
    scope
        const TRACKLEN = 200
        const CHUNKLEN = 40
        const STEPPX   = 4
        dim as long tp = (TRACKLEN + CHUNKLEN) \ STEPPX     ' 60 ticks

        dim as long nMinSeen = 999999
        dim as long nMaxSeen = -999999
        dim as boolean bMonotonic = true
        dim as long prev = -999999
        for t as long = 0 to tp - 1
            dim as long v = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t, PRG_MARQUEE_WRAP )
            if v < nMinSeen then nMinSeen = v
            if v > nMaxSeen then nMaxSeen = v
            dim as boolean bLater = (t > 0)
            dim as boolean bNotUp = (v <= prev)
            if bLater andalso bNotUp then bMonotonic = false
            prev = v
        next
        PRGTest_Check( "Wrap: starts fully OFF the track (negative offset)", (nMinSeen = -CHUNKLEN), _
                       "min = " & str(nMinSeen) & " want " & str(-CHUNKLEN) )
        PRGTest_Check( "Wrap: ends past the far edge", (nMaxSeen >= TRACKLEN - STEPPX), _
                       "max = " & str(nMaxSeen) & " tracklen = " & str(TRACKLEN) )
        PRGTest_Check( "Wrap: is a sawtooth -- rises without reversing inside one period", bMonotonic, _
                       "ticks 0.." & str(tp - 1) )

        dim as boolean bPeriodic = true
        for t as long = 0 to 30
            dim as long a = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t, PRG_MARQUEE_WRAP )
            dim as long b2 = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, t + tp, PRG_MARQUEE_WRAP )
            if a <> b2 then bPeriodic = false
        next
        PRGTest_Check( "Wrap: period is track + chunk", bPeriodic, "ticks per period = " & str(tp) )

        ' The two styles must genuinely differ, or one of them is not wired up.
        dim as long ob = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, 3, PRG_MARQUEE_BOUNCE )
        dim as long ow = PsProgressBar_ComputeMarqueeOffset( TRACKLEN, CHUNKLEN, STEPPX, 3, PRG_MARQUEE_WRAP )
        PRGTest_Check( "Wrap and Bounce are different laws", (ob <> ow), _
                       "bounce = " & str(ob) & " wrap = " & str(ow) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 4. ComputeBlockCount -- the one function that answers both "how many slots" and
    '    "how many are lit", so the two can never disagree.
    ' ------------------------------------------------------------------------------------
    scope
        dim as long n

        ' 7 blocks = 7*10 + 6*3 = 88, and an 8th would need 101.
        n = PsProgressBar_ComputeBlockCount( 100, 10, 3 )
        PRGTest_Check( "BlockCount: 100px of 10+3 holds 7", (n = 7), "count = " & str(n) )

        n = PsProgressBar_ComputeBlockCount( 88, 10, 3 )
        PRGTest_Check( "BlockCount: an exact fit is not off by one", (n = 7), "count = " & str(n) )

        n = PsProgressBar_ComputeBlockCount( 87, 10, 3 )
        PRGTest_Check( "BlockCount: one pixel short drops a block", (n = 6), "count = " & str(n) )

        n = PsProgressBar_ComputeBlockCount( 100, 10, 0 )
        PRGTest_Check( "BlockCount: a zero gap is legal", (n = 10), "count = " & str(n) )

        n = PsProgressBar_ComputeBlockCount( 10, 10, 3 )
        PRGTest_Check( "BlockCount: the last block needs no trailing gap", (n = 1), "count = " & str(n) )

        n = PsProgressBar_ComputeBlockCount( 9, 10, 3 )
        PRGTest_Check( "BlockCount: shorter than one block is none", (n = 0), "count = " & str(n) )

        n = PsProgressBar_ComputeBlockCount( 100, 0, 3 )
        PRGTest_Check( "BlockCount: a zero block length is none, not a hang", (n = 0), "count = " & str(n) )

        n = PsProgressBar_ComputeBlockCount( 0, 10, 3 )
        PRGTest_Check( "BlockCount: an empty bar lights nothing", (n = 0), "count = " & str(n) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 5. Live geometry -- a real control, HORIZONTAL.
    ' ------------------------------------------------------------------------------------
    dim as HWND h = PsProgressBar_Create( hWndParent, PSPROGRESSBAR_SELFTEST_IDC, PRG_HORIZONTAL )
    if h = 0 then
        print "FAIL  precondition: could not create the test control"
        print "=== 0 of 1 assertions passed ==="
        exit sub
    end if
    ' Sized but never shown: every assertion below reads geometry or drives the renderer
    ' offscreen, and neither needs the control on screen.
    MoveWindow( h, 0, 0, 200, 24, FALSE )

    scope
        dim as RECT rcT, rcF, rcB
        PsProgressBar_SetRange( h, 0, 100 )
        PsProgressBar_SetPos( h, 50 )
        PsProgressBar_GetTrackRect( h, rcT )
        PsProgressBar_GetFillRect( h, rcF )
        PsProgressBar_GetBarRect( h, rcB )
        PRGTest_TraceRect( "H rcTrack", rcT )
        PRGTest_TraceRect( "H rcFill ", rcF )
        PRGTest_TraceRect( "H rcBar  ", rcB )

        PRGTest_Check( "H geometry: track spans the full client width", _
                       (rcT.left = 0) andalso (rcT.right = 200), _
                       "track x " & str(rcT.left) & ".." & str(rcT.right) )
        PRGTest_Check( "H geometry: thickness 0 fills the client height", _
                       (rcT.top = 0) andalso (rcT.bottom = 24), _
                       "track y " & str(rcT.top) & ".." & str(rcT.bottom) )
        PRGTest_Check( "H geometry: no inset means fill = track", _
                       (rcF.left = rcT.left) andalso (rcF.right = rcT.right), _
                       "fill x " & str(rcF.left) & ".." & str(rcF.right) )
        PRGTest_Check( "H geometry: the bar starts at the fill's left edge", _
                       (rcB.left = rcF.left), _
                       "bar.left = " & str(rcB.left) & " fill.left = " & str(rcF.left) )
        PRGTest_Check( "H geometry: 50% is half the track", _
                       ((rcB.right - rcB.left) = 100), _
                       "bar width = " & str(rcB.right - rcB.left) )
        PRGTest_Check( "H geometry: the bar spans the fill's full height", _
                       (rcB.top = rcF.top) andalso (rcB.bottom = rcF.bottom), _
                       "bar y " & str(rcB.top) & ".." & str(rcB.bottom) )

        ' The two ends of the range.
        PsProgressBar_SetPos( h, 0 )
        PsProgressBar_GetBarRect( h, rcB )
        PRGTest_Check( "H geometry: an empty bar has zero width", ((rcB.right - rcB.left) = 0), _
                       "bar width = " & str(rcB.right - rcB.left) )
        PsProgressBar_SetPos( h, 100 )
        PsProgressBar_GetBarRect( h, rcB )
        PRGTest_Check( "H geometry: a full bar exactly equals the fill", _
                       (rcB.left = rcF.left) andalso (rcB.right = rcF.right), _
                       "bar x " & str(rcB.left) & ".." & str(rcB.right) )
    end scope

    ' Thickness and inset.
    scope
        dim as RECT rcT, rcF
        PsProgressBar_SetThickness( h, 10 )
        PsProgressBar_SetBarInset( h, 2 )
        PsProgressBar_GetTrackRect( h, rcT )
        PsProgressBar_GetFillRect( h, rcF )
        PRGTest_TraceRect( "H thick rcTrack", rcT )
        PRGTest_TraceRect( "H thick rcFill ", rcF )
        PRGTest_Check( "H thickness: the track is nThickness deep", _
                       ((rcT.bottom - rcT.top) = 10), "depth = " & str(rcT.bottom - rcT.top) )
        PRGTest_Check( "H thickness: and vertically centred", (rcT.top = 7), _
                       "top = " & str(rcT.top) & " want (24-10)\2 = 7" )
        dim as boolean bL = (rcF.left = rcT.left + 2)
        dim as boolean bT = (rcF.top = rcT.top + 2)
        dim as boolean bR = (rcF.right = rcT.right - 2)
        dim as boolean bB = (rcF.bottom = rcT.bottom - 2)
        PRGTest_Check( "H inset: fill is the track deflated on all four sides", _
                       bL andalso bT andalso bR andalso bB, _
                       "fill = (" & str(rcF.left) & "," & str(rcF.top) & ")-(" & _
                       str(rcF.right) & "," & str(rcF.bottom) & ")" )
        ' An inset larger than half the control must collapse, not invert.
        PsProgressBar_SetBarInset( h, 40 )
        PsProgressBar_GetFillRect( h, rcF )
        PRGTest_Check( "H inset: an oversized inset collapses rather than inverting", _
                       (rcF.right >= rcF.left) andalso (rcF.bottom >= rcF.top), _
                       "fill = (" & str(rcF.left) & "," & str(rcF.top) & ")-(" & _
                       str(rcF.right) & "," & str(rcF.bottom) & ")" )
        PsProgressBar_SetBarInset( h, 0 )
        PsProgressBar_SetThickness( h, 0 )
    end scope

    ' Reverse.
    scope
        dim as RECT rcF, rcB
        PsProgressBar_SetReverse( h, true )
        PsProgressBar_SetPos( h, 25 )
        PsProgressBar_GetFillRect( h, rcF )
        PsProgressBar_GetBarRect( h, rcB )
        PRGTest_TraceRect( "H reverse rcBar", rcB )
        PRGTest_Check( "H reverse: the bar is anchored to the RIGHT edge", (rcB.right = rcF.right), _
                       "bar.right = " & str(rcB.right) & " fill.right = " & str(rcF.right) )
        PRGTest_Check( "H reverse: and is still 25% long", ((rcB.right - rcB.left) = 50), _
                       "bar width = " & str(rcB.right - rcB.left) )
        PsProgressBar_SetReverse( h, false )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 6. Live geometry -- VERTICAL. A separate control, because orientation is Create-time.
    ' ------------------------------------------------------------------------------------
    dim as HWND hv = PsProgressBar_Create( hWndParent, PSPROGRESSBAR_SELFTEST_IDC + 1, PRG_VERTICAL )
    MoveWindow( hv, 0, 0, 24, 200, FALSE )
    scope
        dim as RECT rcT, rcF, rcB
        PsProgressBar_SetRange( hv, 0, 100 )
        PsProgressBar_SetPos( hv, 25 )
        PsProgressBar_GetTrackRect( hv, rcT )
        PsProgressBar_GetFillRect( hv, rcF )
        PsProgressBar_GetBarRect( hv, rcB )
        PRGTest_TraceRect( "V rcTrack", rcT )
        PRGTest_TraceRect( "V rcBar  ", rcB )

        PRGTest_Check( "V geometry: the track spans the full client height", _
                       (rcT.top = 0) andalso (rcT.bottom = 200), _
                       "track y " & str(rcT.top) & ".." & str(rcT.bottom) )
        ' THE DEFAULT FOR A VERTICAL GAUGE IS BOTTOM-UP. Getting this backwards is the whole
        ' risk of transposing the axis, and it is invisible in a screenshot of a 50% bar.
        PRGTest_Check( "V geometry: fills BOTTOM-UP by default", (rcB.bottom = rcF.bottom), _
                       "bar.bottom = " & str(rcB.bottom) & " fill.bottom = " & str(rcF.bottom) )
        PRGTest_Check( "V geometry: 25% is a quarter of the height", ((rcB.bottom - rcB.top) = 50), _
                       "bar height = " & str(rcB.bottom - rcB.top) )
        PRGTest_Check( "V geometry: the bar spans the fill's full width", _
                       (rcB.left = rcF.left) andalso (rcB.right = rcF.right), _
                       "bar x " & str(rcB.left) & ".." & str(rcB.right) )

        PsProgressBar_SetReverse( hv, true )
        PsProgressBar_GetBarRect( hv, rcB )
        PRGTest_Check( "V reverse: fills TOP-DOWN", (rcB.top = rcF.top), _
                       "bar.top = " & str(rcB.top) & " fill.top = " & str(rcF.top) )
        PsProgressBar_SetReverse( hv, false )

        PsProgressBar_SetThickness( hv, 8 )
        PsProgressBar_GetTrackRect( hv, rcT )
        PRGTest_Check( "V thickness: the track is nThickness WIDE and h-centred", _
                       ((rcT.right - rcT.left) = 8) andalso (rcT.left = 8), _
                       "track x " & str(rcT.left) & ".." & str(rcT.right) )
        PsProgressBar_SetThickness( hv, 0 )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 7. Block tiling -- must not drift, and must not overhang.
    ' ------------------------------------------------------------------------------------
    scope
        PsProgressBar_SetSegmented( h, true )
        PsProgressBar_SetBlockMetrics( h, 10, 3 )
        PsProgressBar_SetPos( h, 50 )                      ' bar = 100px of a 200px track

        dim as RECT rcB
        PsProgressBar_GetBarRect( h, rcB )
        dim as long n = PsProgressBar_GetBlockCount( h )
        dim as long want = PsProgressBar_ComputeBlockCount( rcB.right - rcB.left, 10, 3 )
        PRGTest_Check( "Blocks: the live count agrees with the pure function", (n = want), _
                       "live = " & str(n) & " pure = " & str(want) )

        dim as boolean bNoDrift = true
        dim as boolean bInside  = true
        for k as long = 0 to n - 1
            dim as RECT rcK
            if PsProgressBar_GetBlockRect( h, k, rcK ) = false then
                bInside = false
                exit for
            end if
            ' Every block sits exactly k strides from the bar's start. Asserted against the
            ' formula rather than against its predecessor, so a per-block rounding error
            ' cannot hide by being consistent.
            if rcK.left <> (rcB.left + (k * 13)) then bNoDrift = false
            if (rcK.right - rcK.left) <> 10 then bNoDrift = false
            if rcK.right > rcB.right then bInside = false
            if k = 0 then PRGTest_TraceRect( "block 0", rcK )
        next
        PRGTest_Check( "Blocks: tile on an exact stride, no cumulative drift", bNoDrift, _
                       str(n) & " blocks, stride 13 from x=" & str(rcB.left) )
        PRGTest_Check( "Blocks: the last one does not overhang the bar", bInside, _
                       "bar.right = " & str(rcB.right) )

        dim as RECT rcOver
        PRGTest_Check( "Blocks: asking past the end answers FALSE", _
                       (PsProgressBar_GetBlockRect( h, n, rcOver ) = false), _
                       "asked for index " & str(n) & " of " & str(n) )
        PRGTest_Check( "Blocks: a negative index answers FALSE", _
                       (PsProgressBar_GetBlockRect( h, -1, rcOver ) = false), "index -1" )

        ' Not segmented means no blocks at all -- not "blocks the painter happens to skip".
        PsProgressBar_SetSegmented( h, false )
        PRGTest_Check( "Blocks: a continuous bar reports none", (PsProgressBar_GetBlockCount( h ) = 0), _
                       "count = " & str(PsProgressBar_GetBlockCount( h )) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 8. Colour precedence -- the full state x enabled table.
    ' ------------------------------------------------------------------------------------
    scope
        PsProgressBar_SetSegmented( h, false )
        dim as PSPROGRESSBAR_COLORS clrs
        PsProgressBar_GetColors( h, @clrs )
        dim as COLORREF c1, c2

        PsProgressBar_SetState( h, PRG_STATE_NORMAL )
        PsProgressBar_ResolveBarColors( h, c1, c2 )
        PRGTest_Check( "Colours: NORMAL uses the normal pair", _
                       (c1 = clrs.BarColor) andalso (c2 = clrs.BarColorEnd), _
                       "got " & hex(c1,6) & "/" & hex(c2,6) )

        PsProgressBar_SetState( h, PRG_STATE_PAUSED )
        PsProgressBar_ResolveBarColors( h, c1, c2 )
        PRGTest_Check( "Colours: PAUSED uses the paused pair", _
                       (c1 = clrs.BarColorPaused) andalso (c2 = clrs.BarColorPausedEnd), _
                       "got " & hex(c1,6) & "/" & hex(c2,6) )

        PsProgressBar_SetState( h, PRG_STATE_ERROR )
        PsProgressBar_ResolveBarColors( h, c1, c2 )
        PRGTest_Check( "Colours: ERROR uses the error pair", _
                       (c1 = clrs.BarColorError) andalso (c2 = clrs.BarColorErrorEnd), _
                       "got " & hex(c1,6) & "/" & hex(c2,6) )

        ' THE ONE CELL WORTH ASSERTING: disabled has to win over a state, or a paused-and-
        ' disabled bar comes up yellow and reads as live.
        PsProgressBar_SetEnabled( h, false )
        PsProgressBar_ResolveBarColors( h, c1, c2 )
        PRGTest_Check( "Colours: DISABLED beats an active state", _
                       (c1 = clrs.BarColorDisabled) andalso (c2 = clrs.BarColorDisabledEnd), _
                       "state was ERROR, got " & hex(c1,6) & "/" & hex(c2,6) )
        PsProgressBar_SetEnabled( h, true )
        PsProgressBar_SetState( h, PRG_STATE_NORMAL )

        ' An unrecognised state falls back rather than indexing off the end.
        PsProgressBar_SetState( h, 99 )
        PRGTest_Check( "Colours: an unknown state falls back to NORMAL", _
                       (PsProgressBar_GetState( h ) = PRG_STATE_NORMAL), _
                       "state = " & str(PsProgressBar_GetState( h )) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 9. Text modes -- assertable without rendering anything.
    ' ------------------------------------------------------------------------------------
    scope
        PsProgressBar_SetRange( h, 0, 200 )
        PsProgressBar_SetPos( h, 50 )

        PsProgressBar_SetTextMode( h, PRG_TEXT_NONE )
        PRGTest_Check( "Text: NONE draws nothing", (len( PsProgressBar_GetDisplayText( h ) ) = 0), _
                       "len = " & str(len( PsProgressBar_GetDisplayText( h ) )) )

        PsProgressBar_SetTextMode( h, PRG_TEXT_PERCENT )
        dim as DWSTRING w1 = PsProgressBar_GetDisplayText( h )
        dim as string s1 = trim(w1)
        PRGTest_Check( "Text: PERCENT is a percentage of the RANGE, not the position", (s1 = "25%"), _
                       "pos 50 of 0..200 gave [" & s1 & "]" )

        PsProgressBar_SetTextMode( h, PRG_TEXT_VALUE )
        dim as DWSTRING w2 = PsProgressBar_GetDisplayText( h )
        dim as string s2 = trim(w2)
        PRGTest_Check( "Text: VALUE is the raw position", (s2 = "50"), "got [" & s2 & "]" )

        PsProgressBar_SetTextCallback( h, @PRGTest_TextCallback )
        PsProgressBar_SetTextMode( h, PRG_TEXT_CALLBACK )
        dim as DWSTRING w3 = PsProgressBar_GetDisplayText( h )
        dim as string s3 = trim(w3)
        PRGTest_Check( "Text: CALLBACK owns the whole string", (left(s3,3) = "CB:"), _
                       "got [" & s3 & "]" )

        ' A callback mode with no callback installed must be silent, not a crash.
        PsProgressBar_SetTextCallback( h, 0 )
        PRGTest_Check( "Text: CALLBACK with no callback draws nothing", _
                       (len( PsProgressBar_GetDisplayText( h ) ) = 0), "no callback set" )

        ' An empty range would divide by zero in the percentage.
        PsProgressBar_SetTextMode( h, PRG_TEXT_PERCENT )
        PsProgressBar_SetRange( h, 5, 5 )
        dim as DWSTRING w4 = PsProgressBar_GetDisplayText( h )
        dim as string s4 = trim(w4)
        PRGTest_Check( "Text: PERCENT of an empty range is 0%, not a crash", (s4 = "0%"), _
                       "got [" & s4 & "]" )

        PsProgressBar_SetRange( h, 0, 100 )
        PsProgressBar_SetTextMode( h, PRG_TEXT_NONE )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 10. The model. Every setter here is silent, and every one clamps.
    ' ------------------------------------------------------------------------------------
    scope
        PsProgressBar_SetRange( h, 0, 100 )

        PsProgressBar_SetPos( h, 500 )
        PRGTest_Check( "Model: SetPos clamps at the maximum", (PsProgressBar_GetPos( h ) = 100), _
                       "pos = " & str(PsProgressBar_GetPos( h )) )
        PsProgressBar_SetPos( h, -500 )
        PRGTest_Check( "Model: SetPos clamps at the minimum", (PsProgressBar_GetPos( h ) = 0), _
                       "pos = " & str(PsProgressBar_GetPos( h )) )

        ' comctl32's PBM_STEPIT WRAPS to nMin at the top. This deliberately does not: a
        ' finished job that suddenly reads 0% looks restarted.
        PsProgressBar_SetPos( h, 95 )
        PsProgressBar_SetStep( h, 10 )
        dim as long after = PsProgressBar_StepIt( h )
        PRGTest_Check( "Model: StepIt CLAMPS at the maximum, it does not wrap", (after = 100), _
                       "95 + 10 gave " & str(after) )

        PsProgressBar_SetPos( h, 40 )
        after = PsProgressBar_DeltaPos( h, -15 )
        PRGTest_Check( "Model: DeltaPos accepts a negative delta", (after = 25), _
                       "40 - 15 gave " & str(after) )

        ' An inverted range is normalised rather than left to poison every later calculation.
        PsProgressBar_SetRange( h, 300, 100 )
        dim as long lo, hi
        PsProgressBar_GetRange( h, lo, hi )
        PRGTest_Check( "Model: an inverted SetRange is normalised", (lo = 100) andalso (hi = 300), _
                       "got " & str(lo) & ".." & str(hi) )
        PRGTest_Check( "Model: and the position is pulled into the new range", _
                       (PsProgressBar_GetPos( h ) >= 100), "pos = " & str(PsProgressBar_GetPos( h )) )
        PsProgressBar_SetRange( h, 0, 100 )

        dim as boolean bH = (PsProgressBar_GetOrientation( h ) = PRG_HORIZONTAL)
        dim as boolean bV = (PsProgressBar_GetOrientation( hv ) = PRG_VERTICAL)
        PRGTest_Check( "Model: orientation reads back what Create was given", bH andalso bV, _
                       "h = " & str(PsProgressBar_GetOrientation( h )) & _
                       " v = " & str(PsProgressBar_GetOrientation( hv )) )

        ' Any negative curvature is the stadium sentinel, so GetCurvature reads back what
        ' SetCurvature was given rather than an arbitrary negative number.
        PsProgressBar_SetCurvature( h, -7 )
        PRGTest_Check( "Model: a negative curvature normalises to the stadium sentinel", _
                       (PsProgressBar_GetCurvature( h ) = PSPROGRESSBAR_CURVATURE_STADIUM), _
                       "got " & str(PsProgressBar_GetCurvature( h )) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 11. The marquee, driven through the LIVE control -- specifically that the WRAP style's
    '     deliberately-negative offsets never escape rcFill once layout has clipped them.
    ' ------------------------------------------------------------------------------------
    scope
        dim as PSPROGRESSBAR ptr pTest = GetProgressBarPointer( h )
        PsProgressBar_SetSegmented( h, false )
        PsProgressBar_SetMarqueeStyle( h, PRG_MARQUEE_WRAP )
        PsProgressBar_SetMarqueeChunkPercent( h, 20 )
        PsProgressBar_SetMode( h, PRG_MODE_INDETERMINATE )

        dim as RECT rcF
        PsProgressBar_GetFillRect( h, rcF )
        dim as boolean bInside = true
        dim as boolean bClipped = false
        dim as long widest = 0
        for t as long = 0 to 200
            pTest->nTick = t
            pTest->bLayoutDirty = true
            dim as RECT rcB
            PsProgressBar_GetBarRect( h, rcB )
            if rcB.left < rcF.left then bInside = false
            if rcB.right > rcF.right then bInside = false
            if rcB.right < rcB.left then bInside = false
            dim as long w = rcB.right - rcB.left
            if w > widest then widest = w
        next
        for t as long = 0 to 200
            pTest->nTick = t
            pTest->bLayoutDirty = true
            dim as RECT rcB
            PsProgressBar_GetBarRect( h, rcB )
            if (rcB.right - rcB.left) < widest then bClipped = true
        next
        PRGTest_Check( "Marquee: WRAP's negative offsets are clipped to the fill area", bInside, _
                       "200 ticks, fill x " & str(rcF.left) & ".." & str(rcF.right) )
        ' The clipping is what MAKES it slide in: the visible chunk has to be narrower while
        ' it is half off the edge. A constant width would mean it was snapping instead.
        PRGTest_Check( "Marquee: WRAP genuinely slides in, the chunk narrows at the edges", bClipped, _
                       "widest visible chunk = " & str(widest) )

        PsProgressBar_SetMarqueeStyle( h, PRG_MARQUEE_BOUNCE )
        dim as boolean bFullWidth = true
        dim as long chunkW = -1
        for t as long = 0 to 200
            pTest->nTick = t
            pTest->bLayoutDirty = true
            dim as RECT rcB
            PsProgressBar_GetBarRect( h, rcB )
            if chunkW = -1 then chunkW = rcB.right - rcB.left
            if (rcB.right - rcB.left) <> chunkW then bFullWidth = false
        next
        PRGTest_Check( "Marquee: BOUNCE never clips -- the chunk keeps one width throughout", _
                       bFullWidth, "chunk width = " & str(chunkW) )

        PsProgressBar_SetMode( h, PRG_MODE_DETERMINATE )
        PRGTest_Check( "Marquee: leaving indeterminate mode resets the tick", (pTest->nTick = 0), _
                       "tick = " & str(pTest->nTick) )
    end scope

    ' ------------------------------------------------------------------------------------
    ' 12. The renderer, measured offscreen.
    '
    ' EVERY FLOOR HERE WAS CALIBRATED BY DELIBERATELY BREAKING THE RENDERER AND MEASURING
    ' WHAT THE BROKEN VERSION SCORED -- not by picking a number that looked safe. PsButton's
    ' "> 1" threshold is NOT copied blind: it passes on a completely destroyed render in
    ' PsMessageBox, where a window border kept every band at 2 tones.
    ' ------------------------------------------------------------------------------------
    scope
        PsProgressBar_SetMode( h, PRG_MODE_DETERMINATE )
        PsProgressBar_SetSegmented( h, false )
        PsProgressBar_SetFillStyle( h, PRG_FILL_SOLID )
        PsProgressBar_SetTextMode( h, PRG_TEXT_NONE )
        PsProgressBar_SetCurvature( h, PSPROGRESSBAR_CURVATURE_STADIUM )
        PsProgressBar_SetPos( h, 50 )

        dim as long tCtl = PsProgressBar_CountRenderedTones( h, PRG_PART_CONTROL )
        PRGTest_Check( "Render: the control is not flooded with one colour", (tCtl > 3), _
                       "distinct tones = " & str(tCtl) )

        ' A half-full solid bar must show BOTH the bar colour and the track behind it.
        dim as long tTrack = PsProgressBar_CountRenderedTones( h, PRG_PART_TRACK )
        PRGTest_Check( "Render: track and bar are both on the surface", (tTrack > 3), _
                       "distinct tones over the track = " & str(tTrack) )

        ' The gradient has to produce a genuine ramp, not two flat halves.
        '
        ' MEASURED AT CURVATURE 0, AND THAT IS THE POINT OF THE TWO LINES BELOW. At the default
        ' stadium curvature the rounded end caps are 24px semicircles whose antialiasing alone
        ' scores 42 tones on a SOLID bar -- so "gradient beats solid" passed by 64 vs 42 while
        ' measuring mostly AA, and would have kept passing with the gradient wired to a flat
        ' fill. Square corners remove the only other source of blended pixels, so what is left
        ' is the ramp and nothing else.
        PsProgressBar_SetCurvature( h, 0 )
        PsProgressBar_SetFillStyle( h, PRG_FILL_GRADIENT )
        dim as long tGrad = PsProgressBar_CountRenderedTones( h, PRG_PART_BAR )
        PsProgressBar_SetFillStyle( h, PRG_FILL_SOLID )
        dim as long tSolid = PsProgressBar_CountRenderedTones( h, PRG_PART_BAR )
        PsProgressBar_SetCurvature( h, PSPROGRESSBAR_CURVATURE_STADIUM )
        PRGTest_Check( "Render: a gradient bar ramps", (tGrad > 6), _
                       "gradient tones = " & str(tGrad) & "  (square corners, so no AA)" )
        ' AT MOST 3, NOT 1, AND THE DIFFERENCE IS A REAL BEHAVIOUR WORTH KNOWING.
        ' PaintRectFactory fills [left, right-1) x [top, bottom-1) -- the -1 is calibrated
        ' family-wide to match what GDI's RoundRect actually covers, and PsBufferPaint's own
        ' self-test pins it. So a square-cornered fill leaves a ONE-PIXEL SEAM down its right
        ' edge and along its bottom, and inside rcBar that seam shows the track underneath and,
        ' on the bottom row, the control background. Three tones: the fill and those two.
        ' Invisible at any curvature above 0 (the caps cover it) and covered by a track border
        ' when one is set, which is why no sibling has ever had to care. Documented in the
        ' README rather than worked around -- deviating from the convention in one control
        ' would be worse than the seam.
        PRGTest_Check( "Render: a solid bar is flat (no ramp)", (tSolid <= 3), _
                       "solid tones = " & str(tSolid) & "  gradient = " & str(tGrad) )

        ' Blocks must let the track show through the gaps, or they are not blocks.
        dim as ulong hCont = PsProgressBar_HashRenderedPart( h, PRG_PART_BAR )
        PsProgressBar_SetSegmented( h, true )
        dim as ulong hSeg = PsProgressBar_HashRenderedPart( h, PRG_PART_BAR )
        PRGTest_Check( "Render: segmenting changes the pixels", (hCont <> hSeg), _
                       "continuous " & hex(hCont,8) & "  blocks " & hex(hSeg,8) )
        PsProgressBar_SetSegmented( h, false )

        ' TEXT. Isolated: the ONLY thing that differs between these two renders is the text
        ' mode -- same position, same colours, same geometry -- so a difference cannot be
        ' explained by anything else. (PsCheckBox shipped a probe of this shape whose first
        ' version passed for the wrong reason; that is why this note is here.)
        PsProgressBar_SetTextMode( h, PRG_TEXT_NONE )
        dim as ulong hNoText = PsProgressBar_HashRenderedPart( h, PRG_PART_TEXT )
        PsProgressBar_SetTextMode( h, PRG_TEXT_PERCENT )
        dim as ulong hText = PsProgressBar_HashRenderedPart( h, PRG_PART_TEXT )
        PRGTest_Check( "Render: the text actually reaches the pixels", (hNoText <> hText), _
                       "without " & hex(hNoText,8) & "  with " & hex(hText,8) )
        PsProgressBar_SetTextMode( h, PRG_TEXT_NONE )

        ' STATE. Also isolated: only the colour pair differs.
        PsProgressBar_SetState( h, PRG_STATE_NORMAL )
        dim as ulong hNormal = PsProgressBar_HashRenderedPart( h, PRG_PART_BAR )
        PsProgressBar_SetState( h, PRG_STATE_ERROR )
        dim as ulong hError = PsProgressBar_HashRenderedPart( h, PRG_PART_BAR )
        PRGTest_Check( "Render: a state change reaches the pixels", (hNormal <> hError), _
                       "normal " & hex(hNormal,8) & "  error " & hex(hError,8) )
        PsProgressBar_SetState( h, PRG_STATE_NORMAL )

        ' And moving the position must move the render. Trivially true if anything works at
        ' all, which is exactly why it is the canary for the probe itself being broken.
        PsProgressBar_SetPos( h, 20 )
        dim as ulong hA = PsProgressBar_HashRenderedPart( h, PRG_PART_TRACK )
        PsProgressBar_SetPos( h, 80 )
        dim as ulong hB = PsProgressBar_HashRenderedPart( h, PRG_PART_TRACK )
        PRGTest_Check( "Render: moving the position moves the render", (hA <> hB), _
                       "20% " & hex(hA,8) & "  80% " & hex(hB,8) )

        ' A vertical bar renders at all. The transposed layout is asserted numerically above;
        ' this only proves the painter is reached on that path too.
        dim as long tV = PsProgressBar_CountRenderedTones( hv, PRG_PART_CONTROL )
        PRGTest_Check( "Render: a vertical bar renders", (tV > 3), "distinct tones = " & str(tV) )
    end scope

    DestroyWindow( h )
    DestroyWindow( hv )


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
        dim as HWND hTipT = PsProgressBar_Create( hWndParent, 9807 )
        if hTipT = 0 then
            PRGTest_Check( "tooltip: could not create a test control", false, "Create returned 0" )
        else
            PRGTest_Check( "tooltip: still ships on the SYSTEM backend", _
                (PsProgressBar_GetTooltipMode( hTipT ) = PSTIP_MODE_SYSTEM), _
                "mode = " & str(PsProgressBar_GetTooltipMode( hTipT )) )
            PRGTest_Check( "tooltip: a comctl32 window exists, and no PsTooltip does", _
                (PsProgressBar_GetTooltipHandle( hTipT ) <> 0) andalso _
                (IsWindow(PsProgressBar_GetTooltipHandle( hTipT )) <> 0) andalso _
                (PsProgressBar_GetPsTooltipHandle( hTipT ) = 0), _
                "hSys = " & str(cast(integer, PsProgressBar_GetTooltipHandle( hTipT ))) )

            PsProgressBar_SetHoverTime( hTipT, 777 )
            PRGTest_Check( "tooltip: SetHoverTime reaches the comctl32 tip", _
                (SendMessageW( PsProgressBar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "TTM_GETDELAYTIME = " & str(SendMessageW( PsProgressBar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )

            dim as HWND hSysBeforeT = PsProgressBar_GetTooltipHandle( hTipT )
            ' The switch is made on its own line, NOT inside the assertion call. fbc does not
            ' evaluate call arguments left to right, so a detail string that reads the mode in
            ' the same call reports the mode from BEFORE the switch -- a green assertion with a
            ' trace saying it failed, which is worse than either.
            dim as boolean bSwitchedT = PsProgressBar_SetTooltipMode( hTipT, PSTIP_MODE_PS )
            PRGTest_Check( "tooltip: switching to PsTooltip succeeds", bSwitchedT, _
                "mode = " & str(PsProgressBar_GetTooltipMode( hTipT )) )
            PRGTest_Check( "tooltip: the outgoing comctl32 window is DESTROYED, not orphaned", _
                (IsWindow(hSysBeforeT) = 0) andalso (PsProgressBar_GetTooltipHandle( hTipT ) = 0), _
                "old hSys alive = " & str(IsWindow(hSysBeforeT)) )

            dim as HWND hPsT = PsProgressBar_GetPsTooltipHandle( hTipT )
            PRGTest_Check( "tooltip: a PsTooltip window exists and is attached to the control", _
                (hPsT <> 0) andalso (IsWindow(hPsT) <> 0) andalso _
                (PsTooltip_GetAttached( hPsT ) = hTipT), _
                "hPs = " & str(cast(integer, hPsT)) )

            dim as PSTOOLTIP ptr pTipT = GetTooltipPointer( hPsT )
            PRGTest_Check( "tooltip: the hover time set on the OLD backend is honoured by the new one", _
                (pTipT <> 0) andalso (pTipT->nInitialDelay = 777), _
                "PsTooltip initial delay = " & str(iif(pTipT, pTipT->nInitialDelay, -1)) & " ms" )
            PRGTest_Check( "tooltip: the PsTooltip backend is WIRED to the control's text policy", _
                (pTipT <> 0) andalso (pTipT->TooltipCallback <> 0), _
                "callback installed = " & str(iif(pTipT andalso pTipT->TooltipCallback, 1, 0)) )

            PsProgressBar_SetTooltipMode( hTipT, PSTIP_MODE_SYSTEM )
            PRGTest_Check( "tooltip: switching back rebuilds the comctl32 tip, hover time and all", _
                (PsProgressBar_GetTooltipHandle( hTipT ) <> 0) andalso _
                (PsProgressBar_GetPsTooltipHandle( hTipT ) = 0) andalso _
                (SendMessageW( PsProgressBar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 ) = 777), _
                "initial = " & str(SendMessageW( PsProgressBar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_INITIAL, 0 )) & " ms" )
            PRGTest_Check( "tooltip: an unset delay is left at the backend's own default", _
                (SendMessageW( PsProgressBar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 ) > 0), _
                "autopop = " & str(SendMessageW( PsProgressBar_GetTooltipHandle( hTipT ), TTM_GETDELAYTIME, TTDT_AUTOPOP, 0 )) & " ms" )

            DestroyWindow( hTipT )
        end if
    end scope

    dim as string sSummary = "=== " & str(gPRG_TestCount - gPRG_FailCount) & " of " & _
                             str(gPRG_TestCount) & " assertions passed"
    if gPRG_FailCount > 0 then sSummary &= "  (" & str(gPRG_FailCount) & " FAILED)"
    sSummary &= " ==="
    print sSummary

end sub
