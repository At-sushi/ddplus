#include "stdafx.h"
#include ".\directdraw.h"

#define REL(a)	if (a) { a->Release(); a = NULL; }

CDirectDrawPlus::CDirectDrawPlus(void)
{
	pDD = NULL;
	primary = NULL;
	m_pClipper = NULL;
	back = NULL;
	m_pGammaControl = NULL;
	m_hwnd = NULL;
	ZeroMemory(&m_param, sizeof(m_param));
	bDialogMode = FALSE;

	// DirectDraw������
	DirectDrawCreateEx(NULL, (void**)&pDD, IID_IDirectDraw7, NULL);
}

CDirectDrawPlus::~CDirectDrawPlus(void)
{
	ReleaseDirectDraw();

	if (pDD != NULL)
		pDD->Release();
}

// ������
// �s���������Ȃ肷�����̂łǂ����ŕ������������
HRESULT CDirectDrawPlus::Initialize(HWND hwnd, DDP_PARAMETERS* pParam)
{
	HRESULT hRet;
	DDSURFACEDESC2 ddsd2;
	DDSCAPS2 ddscaps;

	ReleaseDirectDraw();

	if (pDD == NULL)
		return E_FAIL;

	try	
	{
		// �������[�h
		if ( FAILED(hRet = pDD->SetCooperativeLevel(hwnd,
			pParam->hDeviceWindow ? DDSCL_SETFOCUSWINDOW :		// �t�H�[�J�X�E�B���h�E�w��i�}���`���j�^�j
			( pParam->isWindowed ? DDSCL_NORMAL : (DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) ))) )
				throw hRet;

		// �f�o�C�X�E�B���h�E�̋������[�h�i�}���`���j�^�j
		if (pParam->hDeviceWindow &&
			FAILED(hRet = pDD->SetCooperativeLevel(pParam->hDeviceWindow,
			DDSCL_SETDEVICEWINDOW |
			( pParam->isWindowed ? DDSCL_NORMAL : (DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) ))) )
				throw hRet;

		// �f�B�X�v���C���[�h�i�t���X�N���[�����[�h�̂݁j
		if ( !pParam->isWindowed && FAILED(hRet = pDD->SetDisplayMode(pParam->Width, pParam->Height, pParam->PalletBits, pParam->RefreshRateHz, 0)) )
			throw hRet;

		// �v���C�}���T�[�t�F�X������
		ZeroMemory(&ddsd2, sizeof(ddsd2));
		ddsd2.dwSize = sizeof(ddsd2);
		if (pParam->isWindowed)
		{
			ddsd2.dwFlags = DDSD_CAPS;
			ddsd2.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		}
		else
		{
			ddsd2.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
			ddsd2.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
			ddsd2.dwBackBufferCount = pParam->BackBufferCount ? pParam->BackBufferCount : 1;
		}

		if (FAILED(hRet = pDD->CreateSurface(&ddsd2, &primary, NULL)))
		{
			if (pParam->BackBufferCount)
				pParam->BackBufferCount = ddsd2.dwBackBufferCount;	// �o�b�N�o�b�t�@�����炷�K�v������ꍇ�̒ʒm
			throw hRet;
		}

		// �N���b�p�[�ݒ�
		if ( FAILED(hRet = pDD->CreateClipper(0, &m_pClipper, NULL)) ||
			 FAILED(hRet = m_pClipper->SetHWnd(0, hwnd)) ||
			 FAILED(hRet = primary->SetClipper(m_pClipper)) )
				throw hRet;

		// �o�b�N�T�[�t�F�X������
		if (pParam->isWindowed)
		{
			RECT r = { 0, 0, 0, 0 };

			GetClientRect(hwnd, &r);
			if (pParam->Width == 0)
				pParam->Width = r.right;
			if (pParam->Height == 0)
				pParam->Height = r.bottom;

			ZeroMemory(&ddsd2, sizeof(ddsd2));
			ddsd2.dwSize = sizeof(ddsd2);
			ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
			ddsd2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
			ddsd2.dwWidth = pParam->Width;
			ddsd2.dwHeight = pParam->Height;
			if (FAILED(hRet = pDD->CreateSurface(&ddsd2, &back, NULL)))
				throw hRet;

		}
		else
		{
			ZeroMemory(&ddscaps, sizeof(ddscaps));
			ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
			if (FAILED(hRet = primary->GetAttachedSurface(&ddscaps, &back)))
				throw hRet;
		}

		// �K���}�R���g���[���ݒ�
		primary->QueryInterface(IID_IDirectDrawGammaControl, (void**)&m_pGammaControl);
	}
	catch (HRESULT e)	// �G���[����������
	{
		ReleaseDirectDraw();
		return e;
	}

	m_hwnd = hwnd;
	m_param = *pParam;

	return S_OK;
}

// ���
void CDirectDrawPlus::ReleaseDirectDraw(void)
{
	if (m_pGammaControl != NULL)
	{
		m_pGammaControl->Release();
		m_pGammaControl = NULL;
	}

	if (back != NULL)
	{
		if (m_param.isWindowed)		// �t���X�N���[���̏ꍇ��primary�ƈꏏ�ɏ�����
			back->Release();
		back = NULL;
	}

	if (m_pClipper != NULL)
	{
		m_pClipper->Release();
		m_pClipper = NULL;
	}

	if (primary != NULL)
	{
		primary->Release();
		primary = NULL;
	}

	if (pDD != NULL)
		pDD->SetCooperativeLevel(m_hwnd, DDSCL_NORMAL);

	m_hwnd = NULL;
	ZeroMemory(&m_param, sizeof(m_param));
}

// ��ʍX�V
HRESULT CDirectDrawPlus::Present(LPCRECT pSourceRect, LPCRECT pDestRect, BOOL isWait)
{
	HRESULT hRet;

	if (m_param.isWindowed || m_param.DoNotFlip || bDialogMode ||
		((DWORD)pSourceRect | (DWORD)pDestRect) != NULL)
	{
		RECT rSrc, rDest;
		POINT point;

		// �]������`�ݒ�
		if (pSourceRect)
			memcpy(&rSrc, pSourceRect, sizeof(RECT));
		else
		{
			rSrc.left = rSrc.top = 0;
			rSrc.right = m_param.Width;
			rSrc.bottom = m_param.Height;
		}

		// �E�B���h�E���Q�b�c
		point.x = point.y = 0;
		ClientToScreen(m_hwnd, &point);
		if (pDestRect)
			memcpy(&rDest, pDestRect, sizeof(RECT));
		else
			GetClientRect(m_hwnd, &rDest);

		// �]�����`���E�B���h�E�̈ʒu�ɍ��킹��
		rDest.left		= point.x;
		rDest.top		= point.y;
		rDest.right		+= point.x;
		rDest.bottom	+= point.y;

		if (!m_param.isWindowed)
			pDD->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);	// ��������

		// �����
		hRet = primary->Blt(&rDest, back, &rSrc, isWait ? DDBLT_WAIT : DDBLT_DONOTWAIT, NULL);
	}
	else
		hRet = primary->Flip(NULL, isWait ? DDFLIP_WAIT : DDFLIP_DONOTWAIT);

	return hRet;
}

// ��������
HRESULT CDirectDrawPlus::Blt(LPDIRECTDRAWSURFACE7 pDest, RECT rDest, LPDIRECTDRAWSURFACE7 pSrc, RECT rSrc, DWORD Flags,
						 DWORD FlagsFast, LPDDBLTFX pBltFx)
{
	HRESULT ret = 0;
	DDSURFACEDESC2 sDesc;

	if (!pSrc || !pDest)
		return E_INVALIDARG;

	// �T�[�t�F�X��񏊓�
	ZeroMemory(&sDesc, sizeof(sDesc));
	sDesc.dwSize = sizeof(sDesc);
	if ( FAILED(ret = pSrc->GetSurfaceDesc(&sDesc)) )
		return ret;

	if (rSrc.left >= (int)sDesc.dwWidth ||
		rSrc.top >= (int)sDesc.dwHeight ||
		rSrc.right < 0 ||
		rSrc.bottom < 0)
		return DDERR_INVALIDRECT; // RECT���T�[�t�F�X�̊O

	if (rDest.left >= (int)m_param.Width ||
		rDest.top >= (int)m_param.Height ||
		rDest.right < 0 ||
		rDest.bottom < 0)
		return DDERR_INVALIDRECT; // ��ʊO

	// ��ʂ���͂ݏo�����������킹��
	if (rSrc.left < 0)
	{
		rDest.left -= rSrc.left;
		rSrc.left = 0;
	}
	if (rSrc.top < 0)
	{
		rDest.top -= rSrc.top;
		rSrc.top = 0;
	}
	if ( rSrc.right > (int)sDesc.dwWidth )
	{
		rDest.right -= rSrc.right - sDesc.dwWidth;
		rSrc.right = sDesc.dwWidth;
	}
	if ( rSrc.bottom > (int)sDesc.dwHeight )
	{
		rDest.bottom -= rSrc.bottom - sDesc.dwHeight;
		rSrc.bottom = sDesc.dwHeight;
	}

	// ����A�ƌ����������������C���B
	int src_width = rSrc.right - rSrc.left,
		src_height = rSrc.bottom - rSrc.top,
		dest_width = rDest.right - rDest.left,
		dest_height = rDest.bottom - rDest.top;		// �]�����Ɠ]����̕��ƍ���

	if (rDest.left < 0)
	{
		rSrc.left -= rDest.left * src_width / dest_width;
		rDest.left = 0;
	}
	if (rDest.top < 0)
	{
		rSrc.top -=rDest.top * src_height / dest_height;
		rDest.top = 0;
	}
	if ( rDest.right >= (int)m_param.Width )
	{
		rSrc.right -= (rDest.right - m_param.Width) * src_width / dest_width;
		rDest.right = m_param.Width;
	}
	if ( rDest.bottom >= (int)m_param.Height )
	{
		rSrc.bottom -= (rDest.bottom - m_param.Height) * src_height / dest_height;
		rDest.bottom = m_param.Height;
	}

	// �g��k�����Ȃ���΍����]���������Ă݂�
	if ( pBltFx ||
		((rSrc.right - rSrc.left) != (rDest.right - rDest.left)) ||
		((rSrc.bottom - rSrc.top) != (rDest.bottom - rDest.top)) ||
		FAILED(ret = pDest->BltFast(rDest.left, rDest.top, pSrc, &rSrc, FlagsFast)) )
	ret = pDest->Blt(&rDest, pSrc, &rSrc, Flags, pBltFx); // �_���Ȃ畁�ʂ̓]��

	return ret;
}

// ���W���ڎw��Blt
HRESULT CDirectDrawPlus::Blt(LPDIRECTDRAWSURFACE7 pDest, LPDIRECTDRAWSURFACE7 pSrc, int x, int y, int vx, int vy,
						 int xDest, int yDest, DWORD Flags, DWORD FlagsFast, double ScaleX, double ScaleY,
						 LPDDBLTFX pBltFx)
{
	RECT rSrc, rDest;

	// ��`��`
	rSrc.left = x;
	rSrc.top = y;
	rSrc.right = x + vx;
	rSrc.bottom = y + vy;

	// ���M���`��`
	rDest.left = xDest;
	rDest.top = yDest;
	rDest.right = xDest + (int)(vx * ScaleX);
	rDest.bottom = yDest + (int)(vy * ScaleY);

	return Blt(pDest, rDest, pSrc, rSrc, Flags, FlagsFast, pBltFx);
}

// �r�b�g�}�b�v����DirectDraw�p���b�g����
// ��ɂ����MS�̃T���v��
HRESULT CDirectDrawPlus::CreatePaletteFromBitmap( LPDIRECTDRAWPALETTE* ppPalette,
										   HINSTANCE hInst,
										   const TCHAR* strBMP )
{
	HRSRC             hResource      = NULL;
	RGBQUAD*          pRGB           = NULL;
	BITMAPINFOHEADER* pbi = NULL;
	PALETTEENTRY      aPalette[256];
	HANDLE            hFile = NULL;
	DWORD             iColor;
	DWORD             dwColors;
	BITMAPFILEHEADER  bf;
	BITMAPINFOHEADER  bi;
	DWORD             dwBytesRead;

	if( pDD == NULL)
		return E_FAIL;
	if(strBMP == NULL || ppPalette == NULL )
		return E_INVALIDARG;

	*ppPalette = NULL;

	//  Try to load the bitmap as a resource, if that fails, try it as a file
	hResource = FindResource( hInst, strBMP, RT_BITMAP );
	if( hResource )
	{
		pbi = (LPBITMAPINFOHEADER) LockResource( LoadResource( hInst, hResource ) );       
		if( NULL == pbi )
			return E_FAIL;

		pRGB = (RGBQUAD*) ( (BYTE*) pbi + pbi->biSize );

		// Figure out how many colors there are
		if( pbi == NULL || pbi->biSize < sizeof(BITMAPINFOHEADER) )
			dwColors = 0;
		else if( pbi->biBitCount > 8 )
			dwColors = 0;
		else if( pbi->biClrUsed == 0 )
			dwColors = 1 << pbi->biBitCount;
		else
			dwColors = pbi->biClrUsed;

		// �ǉ��F�p���b�g�̃r�b�g���Ƃ�
		DWORD pCaps;
		if (dwColors > 16) pCaps = DDPCAPS_8BIT;
		else pCaps = DDPCAPS_4BIT;

		//  A DIB color table has its colors stored BGR not RGB
		//  so flip them around.
		for( iColor = 0; iColor < dwColors; iColor++ )
		{
			aPalette[iColor].peRed   = pRGB[iColor].rgbRed;
			aPalette[iColor].peGreen = pRGB[iColor].rgbGreen;
			aPalette[iColor].peBlue  = pRGB[iColor].rgbBlue;
			aPalette[iColor].peFlags = PC_NOCOLLAPSE;
		}

		return pDD->CreatePalette( pCaps, aPalette, ppPalette, NULL );
	}

	// Attempt to load bitmap as a file
	hFile = CreateFile( strBMP, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
	if( NULL == hFile )
		return E_FAIL;

	// Read the BITMAPFILEHEADER
	ReadFile( hFile, &bf, sizeof(bf), &dwBytesRead, NULL );
	if( dwBytesRead != sizeof(bf) )
	{
		CloseHandle( hFile );
		return E_FAIL;
	}

	// Read the BITMAPINFOHEADER
	ReadFile( hFile, &bi, sizeof(bi), &dwBytesRead, NULL );
	if( dwBytesRead != sizeof(bi) )
	{
		CloseHandle( hFile );
		return E_FAIL;
	}

	// Read the PALETTEENTRY 
	ReadFile( hFile, aPalette, sizeof(aPalette), &dwBytesRead, NULL );
	if( dwBytesRead != sizeof(aPalette) )
	{
		CloseHandle( hFile );
		return E_FAIL;
	}

	CloseHandle( hFile );

	// Figure out how many colors there are
	if( bi.biSize != sizeof(BITMAPINFOHEADER) )
		dwColors = 0;
	else if (bi.biBitCount > 8)
		dwColors = 0;
	else if (bi.biClrUsed == 0)
		dwColors = 1 << bi.biBitCount;
	else
		dwColors = bi.biClrUsed;

	//  A DIB color table has its colors stored BGR not RGB
	//  so flip them around since DirectDraw uses RGB
	for( iColor = 0; iColor < dwColors; iColor++ )
	{
		BYTE r = aPalette[iColor].peRed;
		aPalette[iColor].peRed  = aPalette[iColor].peBlue;
		aPalette[iColor].peBlue = r;
		aPalette[iColor].peFlags = PC_NOCOLLAPSE;
	}

	return pDD->CreatePalette( DDPCAPS_8BIT, aPalette, ppPalette, NULL );
}

// �T�[�t�F�X�Ƀr�b�g�}�b�v�̃f�[�^�������ʂ�
// �m��MS�̃T���v����������������Ă������A���[�h���Ɏg���Ă܂��B
HRESULT CDirectDrawPlus::DrawBitmap( LPDIRECTDRAWSURFACE7 m_pdds, HBITMAP hBMP, 
					DWORD dwBMPOriginX, DWORD dwBMPOriginY, 
					DWORD dwBMPWidth, DWORD dwBMPHeight )
{
	HDC            hDC, hDCImage;
	BITMAP         bmp;
	DDSURFACEDESC2 ddsd;
	HRESULT        hr;

	if( hBMP == NULL || m_pdds == NULL )
		return E_INVALIDARG;

	// Make sure this surface is restored.
	if( FAILED( hr = m_pdds->Restore() ) )
		return hr;

	// Get the surface.description
	ddsd.dwSize  = sizeof(ddsd);
	m_pdds->GetSurfaceDesc( &ddsd );

	if( ddsd.ddpfPixelFormat.dwFlags == DDPF_FOURCC )
		return E_NOTIMPL;

	// Select bitmap into a memoryDC so we can use it.
	hDCImage = CreateCompatibleDC( NULL );
	if( NULL == hDCImage )
		return E_FAIL;

	SelectObject( hDCImage, hBMP );

	// Get size of the bitmap
	GetObject( hBMP, sizeof(bmp), &bmp );

	// Use the passed size, unless zero
	dwBMPWidth  = ( dwBMPWidth  == 0 ) ? bmp.bmWidth  : dwBMPWidth;     
	dwBMPHeight = ( dwBMPHeight == 0 ) ? bmp.bmHeight : dwBMPHeight;

	// Stretch the bitmap to cover this surface
	if( FAILED( hr = m_pdds->GetDC( &hDC ) ) )
		return hr;

	StretchBlt( hDC, 0, 0, 
				ddsd.dwWidth, ddsd.dwHeight, 
				hDCImage, dwBMPOriginX, dwBMPOriginY,
				dwBMPWidth, dwBMPHeight, SRCCOPY );

	if( FAILED( hr = m_pdds->ReleaseDC( hDC ) ) )
		return hr;

	DeleteDC( hDCImage );

	return S_OK;
}

// �J���[�L�[�ݒ�
HRESULT CDirectDrawPlus::DDSetColorKey( const LPDIRECTDRAWSURFACE7 m_pdds, DWORD dwColorKey )
{
	if( NULL == m_pdds )
		return E_POINTER;

//    m_bColorKeyed = TRUE;

	DDCOLORKEY ddck;
	ddck.dwColorSpaceLowValue  = ConvertGDIColor( m_pdds, dwColorKey );
	ddck.dwColorSpaceHighValue = ConvertGDIColor( m_pdds, dwColorKey );
	
	return m_pdds->SetColorKey( DDCKEY_SRCBLT, &ddck );
}

// DDSetColorKey�̓����Ŏg���Ă�֐�
DWORD CDirectDrawPlus::ConvertGDIColor( const LPDIRECTDRAWSURFACE7 m_pdds,
								COLORREF dwGDIColor )
{
	if( m_pdds == NULL )
		return 0x00000000;

	COLORREF       rgbT;
	HDC            hdc;
	DWORD          dw = CLR_INVALID;
	DDSURFACEDESC2 ddsd;
	HRESULT        hr;

	//  Use GDI SetPixel to color match for us
	if( dwGDIColor != CLR_INVALID && m_pdds->GetDC(&hdc) == DD_OK)
	{
		rgbT = GetPixel(hdc, 0, 0);     // Save current pixel value
		SetPixel(hdc, 0, 0, dwGDIColor);       // Set our value
		m_pdds->ReleaseDC(hdc);
	}

	// Now lock the surface so we can read back the converted color
	ddsd.dwSize = sizeof(ddsd);
	hr = m_pdds->Lock( NULL, &ddsd, DDLOCK_WAIT, NULL );
	if( hr == DD_OK)
	{
		dw = *(DWORD *) ddsd.lpSurface; 
		if( ddsd.ddpfPixelFormat.dwRGBBitCount < 32 ) // Mask it to bpp
			dw &= ( 1 << ddsd.ddpfPixelFormat.dwRGBBitCount ) - 1;  
		m_pdds->Unlock(NULL);
	}

	//  Now put the color that was there back.
	if( dwGDIColor != CLR_INVALID && m_pdds->GetDC(&hdc) == DD_OK )
	{
		SetPixel( hdc, 0, 0, rgbT );
		m_pdds->ReleaseDC(hdc);
	}
	
	return dw;    
}

// �r�b�g�}�b�v����I�t�X�N���[���T�[�t�F�X�쐬
// MS�̃T���v��������
HRESULT CDirectDrawPlus::CreateSurfaceFromBitmap(HINSTANCE hInstance, LPCTSTR bmp_name, LPDIRECTDRAWSURFACE7* dest,
											 DWORD FlagsAdd /*=0*/)
{
	HRESULT hRet;
	DDSURFACEDESC2 ddsd2;
	HRSRC             hResource;
	BITMAPINFOHEADER  bi;

	//  Try to load the bitmap as a resource, if that fails, try it as a file
	hResource = FindResource( hInstance, bmp_name, RT_BITMAP );
	if( hResource )
	{
		BITMAPINFOHEADER* pbi;

		pbi = (LPBITMAPINFOHEADER) LockResource( LoadResource( hInstance, hResource ) );       
		if( NULL == pbi )
			return E_FAIL;

		bi = *pbi;
	}
	else
	{
		HANDLE            hFile;
		DWORD             dwBytesRead = 0;

		// Attempt to load bitmap as a file
		hFile = CreateFile( bmp_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
		if( NULL == hFile )
			return E_FAIL;

		// Read the BITMAPFILEHEADER
		// �p�X
		SetFilePointer( hFile, sizeof(BITMAPFILEHEADER), NULL, FILE_CURRENT );

		// Read the BITMAPINFOHEADER
		ReadFile( hFile, &bi, sizeof(bi), &dwBytesRead, NULL );
		if( dwBytesRead != sizeof(bi) )
		{
			CloseHandle( hFile );
			return E_FAIL;
		}

		CloseHandle( hFile );
	}

	ZeroMemory(&ddsd2, sizeof(ddsd2));
	ddsd2.dwSize = sizeof(ddsd2);
	ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | FlagsAdd;
	ddsd2.dwWidth = bi.biWidth;
	ddsd2.dwHeight = bi.biHeight;
	if (FAILED(hRet = pDD->CreateSurface(&ddsd2, dest, NULL)))
		return hRet;

	// �r�b�g�}�b�v�ǂݍ���
	// ���\�[�X����ǂݍ��݁A���s������t�@�C������B
	HBITMAP hBMP = (HBITMAP)LoadImage(hInstance, bmp_name, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	if (!hBMP)
		hBMP = (HBITMAP)LoadImage(NULL, bmp_name, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE|LR_CREATEDIBSECTION);

	// �r�b�g�}�b�v�̒��g�������ʂ�
	if (hBMP) {
		DrawBitmap(*dest, hBMP, 0, 0, 0, 0);
		DeleteObject(hBMP);
	}

	return S_OK;
}

// ��ʏ���
HRESULT CDirectDrawPlus::Clear(DWORD color, BOOL isWait)
{
	DDBLTFX ddbltfx;

	ZeroMemory(&ddbltfx, sizeof(DDBLTFX));
	ddbltfx.dwSize = sizeof(DDBLTFX);
	ddbltfx.dwFillColor = ConvertGDIColor(primary, color);

	return back->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | (isWait ? DDBLT_WAIT : DDBLT_DONOTWAIT), &ddbltfx);
}

// ��v�ȃT�[�t�F�X�������X�g�A�i�K���j
HRESULT CDirectDrawPlus::Restore(void)
{
	HRESULT ret;

	ret = primary->Restore();
	if (m_param.isWindowed)
		back->Restore();

	return ret;
}

HRESULT CDirectDrawPlus::SaveSurfaceToBitmap(LPCTSTR pDestFile, LPDIRECTDRAWSURFACE7 pSrcSurface, const PALETTEENTRY* pSrcPalette, const RECT* pSrcRect)
{
	HRESULT hRet;
	HANDLE hFile = NULL, hMap = NULL;
	HDC hDC, memDC = NULL;
	HBITMAP hBMP = NULL;
	DDSURFACEDESC2 ddsd;
	BITMAPFILEHEADER bf;
	union {
		BYTE byte[sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*256];
		BITMAPINFO bi;
	} bi;

	if (pDestFile == NULL || pSrcSurface == NULL)
		return E_INVALIDARG;

	// desc����
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(DDSURFACEDESC2);
	if (FAILED(hRet = pSrcSurface->GetSurfaceDesc(&ddsd)))
		return hRet;
	if (ddsd.ddpfPixelFormat.dwRGBBitCount == 32)
		ddsd.ddpfPixelFormat.dwRGBBitCount = 24;	// ���F
	DWORD num_colors = 1 << ddsd.ddpfPixelFormat.dwRGBBitCount;

	ZeroMemory(&bf, sizeof(bf));
	bf.bfOffBits = sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
	bf.bfOffBits = (bf.bfOffBits + 3) & ~3;		// ���񂷂��
	if (ddsd.ddpfPixelFormat.dwRGBBitCount <= 8)
		bf.bfOffBits += sizeof(RGBQUAD) * num_colors;
	bf.bfSize = bf.bfOffBits + ((ddsd.lPitch+3)&~3) * ddsd.dwHeight;
	bf.bfType = 'MB';

	bi.bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bi.bmiHeader.biWidth = ddsd.dwWidth;
	bi.bi.bmiHeader.biHeight = ddsd.dwHeight;
	bi.bi.bmiHeader.biPlanes = 1;
	bi.bi.bmiHeader.biBitCount = (WORD)ddsd.ddpfPixelFormat.dwRGBBitCount;
	bi.bi.bmiHeader.biCompression = BI_RGB;
	bi.bi.bmiHeader.biSizeImage = ((ddsd.lPitch+3)&~3) * ddsd.dwHeight;
	bi.bi.bmiHeader.biXPelsPerMeter = 72;
	bi.bi.bmiHeader.biYPelsPerMeter = 72;
	bi.bi.bmiHeader.biClrUsed = num_colors;
	bi.bi.bmiHeader.biClrImportant = num_colors;

	try
	{
		hRet = E_FAIL;
		hFile = CreateFile(pDestFile, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			throw S_FALSE;

		// �t�@�C���Ƀ}�b�s���O���ꂽ�r�b�g�}�b�v�����
		void* dummy;
		hMap = CreateFileMapping(hFile, NULL, PAGE_READWRITE,
			0, bf.bfSize,
			NULL);
		if (!hMap)
			throw S_FALSE;
		PBYTE mview = (PBYTE)MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, bf.bfOffBits);		// �w�b�_���������߂̃r���[
		hDC = GetDC(NULL);
		hBMP = CreateDIBSection(hDC, &bi.bi,
			bi.bi.bmiHeader.biBitCount > 8 ? DIB_RGB_COLORS : DIB_PAL_COLORS,
			&dummy, hMap, bf.bfOffBits);
		ReleaseDC(NULL, hDC);
		if (!hMap || !hBMP)
			throw S_FALSE;

		memDC = CreateCompatibleDC(NULL);
		if (!memDC)
			throw S_FALSE;
		SelectObject(memDC, hBMP);

		// �w�b�_����������
		memcpy(mview, &bf, sizeof(bf));											// �t�@�C���w�b�_
		memcpy(mview+sizeof(bf), &bi.bi.bmiHeader, sizeof(bi.bi.bmiHeader));	// ���w�b�_
		if (bi.bi.bmiHeader.biBitCount <= 8)
		{
			GetDIBColorTable(memDC, 0, num_colors, bi.bi.bmiColors);
			memcpy(mview+sizeof(bf)+sizeof(bi.bi.bmiHeader), bi.bi.bmiColors, sizeof(RGBQUAD) * num_colors);	// �p���b�g
		}
		UnmapViewOfFile(mview);
		mview = NULL;

		if (SUCCEEDED(hRet = pSrcSurface->GetDC(&hDC)))
		{
			// �T�[�t�F�X�̒��g���r�b�g�}�b�v�ɃR�s�[
			BitBlt(memDC, 0, 0, ddsd.dwWidth, ddsd.dwHeight, hDC, 0, 0, SRCCOPY);
			pSrcSurface->ReleaseDC(hDC);
		}
	}
	catch (HRESULT e)
	{
		DeleteDC(memDC);
		DeleteObject(hBMP);
		CloseHandle(hMap);
		CloseHandle(hFile);

		return e;
	}

	DeleteDC(memDC);
	DeleteObject(hBMP);
	CloseHandle(hMap);
	CloseHandle(hFile);

	return hRet;
}

// �񋓖��߂őS�T�[�t�F�X�ɕ`��
static HRESULT CALLBACK _BltToEnumSurfaces(LPDIRECTDRAWSURFACE7 lpDDSurface, LPDDSURFACEDESC2 lpDesc, LPVOID lpContext)
{
	CDirectDrawPlus* pDDp = (CDirectDrawPlus*)lpContext;
	LPDIRECTDRAWSURFACE7 pSource = pDDp->GetPrimaryBuffer();

	if (lpDDSurface != pSource)
		pDDp->Blt(lpDDSurface, pSource, 0, 0, lpDesc->dwWidth, lpDesc->dwHeight, 0, 0, DDBLT_WAIT, DDBLTFAST_WAIT);

	return DDENUMRET_OK;
}

// �_�C�A���O�{�b�N�X�g�p���[�h�ݒ�
HRESULT CDirectDrawPlus::SetDialogBoxMode(BOOL bEnableDialogs)
{
	HRESULT hr;

	// GDI�T�[�t�F�X��\��
	if (bEnableDialogs)
	{
		primary->EnumAttachedSurfaces(this, _BltToEnumSurfaces);	// ��ʂ�������̂�h��
		if ( FAILED(hr = pDD->FlipToGDISurface()) )
			return hr;
	}

	bDialogMode = bEnableDialogs;

	return S_OK;
}
