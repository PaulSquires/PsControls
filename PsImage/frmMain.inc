'    PsImage - demo harness
'
'    Copyright (C) 2016-2026 Paul Squires, PlanetSquires Software
'
'    This Source Code Form is subject to the terms of the Mozilla Public
'    License, v. 2.0. If a copy of the MPL was not distributed with this
'    file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include once "frmMain.bi"


' Three images, all shown side by side so the loaders can be compared by eye:
'   gImgBmp - built procedurally in memory (LoadFromBitmap)
'   gImgPng - that same bitmap saved to sample.png and RELOADED from disk (Load)
'   gImgBmpFile - saved to sample.bmp and reloaded (Load)
' A fourth, gImgArg, loads whatever path is on the command line -- point it at a real .ico or
' .png with transparency to judge those.
dim shared as PsImage ptr gImgBmp, gImgPng, gImgBmpFile, gImgArg


' ========================================================================================
' Build a NON-square opaque demo image: four coloured quadrants inside a white border, so
' letterbox / stretch / centre are all obvious at a glance. 96 x 48, top-down 32bpp.
' ========================================================================================
private function MakeDemoBitmap() as HBITMAP
    const W = 96, H = 48
    dim as BITMAPINFO bi
    bi.bmiHeader.biSize        = sizeof( BITMAPINFOHEADER )
    bi.bmiHeader.biWidth       = W
    bi.bmiHeader.biHeight      = -H
    bi.bmiHeader.biPlanes      = 1
    bi.bmiHeader.biBitCount    = 32
    bi.bmiHeader.biCompression = BI_RGB
    dim as ulong ptr p
    dim as HBITMAP hbm = CreateDIBSection( null, @bi, DIB_RGB_COLORS, cast(any ptr ptr, @p), null, 0 )
    if (hbm = 0) orelse (p = 0) then return hbm

    ' 0xAARRGGBB, opaque. (COLORREF's byte order is the reverse; these are chosen to read as
    ' distinct hues rather than to match any theme.)
    for y as long = 0 to H - 1
        for x as long = 0 to W - 1
            dim as ulong c
            if (x < 2) orelse (x > W - 3) orelse (y < 2) orelse (y > H - 3) then
                c = &hFFFFFFFF                         ' white border
            elseif (x < W \ 2) andalso (y < H \ 2) then
                c = &hFFE05656                         ' red-ish
            elseif (x >= W \ 2) andalso (y < H \ 2) then
                c = &hFF56A0E0                         ' blue-ish
            elseif (x < W \ 2) andalso (y >= H \ 2) then
                c = &hFF56C878                          ' green-ish
            else
                c = &hFFE0C24E                          ' amber
            end if
            p[y * W + x] = c
        next
    next
    function = hbm
end function


' ========================================================================================
' Draw one image three ways (aspect / stretch / centre) in a labelled row.
' ========================================================================================
private sub PaintImageRow( byval b as PsBufferPaint ptr, byref sLabel as string, _
                           byval pImg as PsImage ptr, byval y as long )
    dim as long x0 = 24
    dim as RECT rcLabel = type( x0, y, x0 + 560, y + 18 )
    b->SetForeColor( theme.ForeColor )
    b->PaintText( sLabel, @rcLabel, DT_LEFT )
    y += 22

    ' A cell background, so a transparent PNG shows its alpha against something.
    dim as long cellW = 120, cellH = 90, gap = 20
    dim as string sub_(0 to 2) = { "ASPECT", "STRETCH", "CENTER" }
    dim as long fit_(0 to 2) = { PS_IMGFIT_ASPECT, PS_IMGFIT_STRETCH, PS_IMGFIT_CENTER }

    for i as long = 0 to 2
        dim as long cx = x0 + i * (cellW + gap)
        dim as RECT rcCell = type( cx, y, cx + cellW, y + cellH )
        b->SetBackColor( theme.BackColorHot )
        b->SetPenColor( theme.Divider )
        b->PaintBorderRect( @rcCell, 1 )

        if pImg andalso pImg->IsValid() then
            dim as RECT rcInner = type( cx + 4, y + 4, cx + cellW - 4, y + cellH - 4 )
            b->PaintImage( pImg->Image(), @rcInner, fit_(i) )
        else
            dim as RECT rcT = rcCell
            b->SetForeColor( theme.ForeColorSelect )
            b->PaintText( "(not loaded)", @rcT, DT_CENTER )
        end if

        dim as RECT rcSub = type( cx, y + cellH + 2, cx + cellW, y + cellH + 18 )
        b->SetForeColor( theme.Divider )
        b->PaintText( sub_(i), @rcSub, DT_CENTER )
    next
end sub


' ========================================================================================
' frmMain_WndProc
' ========================================================================================
function frmMain_WndProc( byval hwnd as HWND, byval uMsg as UINT, _
                          byval wParam as WPARAM, byval lParam as LPARAM ) as LRESULT

    select case uMsg
        case WM_ERASEBKGND
            return true

        case WM_PAINT
            dim b as PsBufferPaint
            b.BeginDoubleBuffer( hwnd )
            b.SetBackColor( theme.BackColor )
            b.PaintClientRect()
            b.SetFont( ghFont(GUIFONT_9) )

            dim as long y = 20
            PaintImageRow( @b, "LoadFromBitmap (built in memory)", gImgBmp, y ) : y += 150
            PaintImageRow( @b, "Load .png (saved & reloaded from disk)", gImgPng, y ) : y += 150
            PaintImageRow( @b, "Load .bmp (saved & reloaded from disk)", gImgBmpFile, y ) : y += 150
            if gImgArg then PaintImageRow( @b, "Load from command-line path", gImgArg, y )

            b.EndDoubleBuffer()
            return 0

        case WM_KEYDOWN
            if wParam = VK_ESCAPE then PostMessage( hwnd, WM_CLOSE, 0, 0 )
            return 0

        case WM_DESTROY
            PostQuitMessage(0)
            return 0
    end select

    return DefWindowProc( hwnd, uMsg, wParam, lParam )
end function


' ========================================================================================
' frmMain_Show
' ========================================================================================
function frmMain_Show( byval hWndParent as HWND ) as LRESULT

    dim pWindow as CWindow ptr = new CWindow( APPCLASSNAME )

    ghFont(GUIFONT_9)  = pWindow->CreateFont( GUIFONT, 9,  FW_NORMAL )
    ghFont(GUIFONT_10) = pWindow->CreateFont( GUIFONT, 10, FW_NORMAL )

    ' --- Build the images. Save the procedural one out, reload the copies from disk. ------
    gImgBmp = new PsImage
    scope
        dim as HBITMAP hbm = MakeDemoBitmap()
        gImgBmp->LoadFromBitmap( hbm )
        if gImgBmp->IsValid() then
            AfxGdipSaveImageToPng( gImgBmp->Image()->m_pImage, "sample.png" )
            AfxGdipSaveImageToBmp( gImgBmp->Image()->m_pImage, "sample.bmp" )
        end if
        if hbm then DeleteObject( hbm )
    end scope

    gImgPng = new PsImage : gImgPng->Load( "sample.png" )
    gImgBmpFile = new PsImage : gImgBmpFile->Load( "sample.bmp" )

    ' Optional: a real file from the command line (a .ico or transparent .png to eyeball).
    dim as string sArg = command(1)
    if len(sArg) then
        gImgArg = new PsImage
        gImgArg->Load( sArg )
    end if

    HWND_FRMMAIN = pWindow->Create( hWndParent, APPNAME, @frmMain_WndProc )
    pWindow->SetClientSize( 560, iif(gImgArg, 640, 490) )
    pWindow->Center()

    if environ("PSIMAGE_SELFTEST") = "1" then PsImage_RunSelfTest()

    ShowWindow( HWND_FRMMAIN, SW_SHOW )
    UpdateWindow( HWND_FRMMAIN )

    dim as MSG uMsg
    while GetMessage( @uMsg, null, 0, 0 )
        TranslateMessage( @uMsg )
        DispatchMessage( @uMsg )
    wend

    if gImgBmp     then delete gImgBmp     : gImgBmp = 0
    if gImgPng     then delete gImgPng     : gImgPng = 0
    if gImgBmpFile then delete gImgBmpFile : gImgBmpFile = 0
    if gImgArg     then delete gImgArg     : gImgArg = 0

    for i as long = 0 to MAXFONTS - 1
        if ghFont(i) then DeleteObject( ghFont(i) )
    next

    delete pWindow
    return uMsg.wParam
end function
