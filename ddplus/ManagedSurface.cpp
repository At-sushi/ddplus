#include "stdafx.h"
#include ".\directdraw.h"

CManagedSurface::CManagedSurface(void)
{
	m_ssystem = NULL;
	m_svideo = NULL;
	m_width = 0;
	m_height = 0;
	m_pDDp = NULL;
}

CManagedSurface::~CManagedSurface(void)
{
	Release();
}

// 初期化
HRESULT CManagedSurface::Initialize(int width, int height, CDirectDrawPlus* pDDp)
{
	HRESULT ret;
	DDSURFACEDESC2 ddsd2;

	Release();

	// システム側初期化
	ZeroMemory(&ddsd2, sizeof(ddsd2));
	ddsd2.dwSize = sizeof(ddsd2);
	ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	ddsd2.dwWidth = width;
	ddsd2.dwHeight = height;

	if ( FAILED(ret = pDDp->GetDirectDraw7()->CreateSurface(&ddsd2, &m_ssystem, NULL)) )
		return ret;

	// ビデオ側初期化
	ZeroMemory(&ddsd2, sizeof(ddsd2));
	ddsd2.dwSize = sizeof(ddsd2);
	ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	ddsd2.dwWidth = width;
	ddsd2.dwHeight = height;

	pDDp->GetDirectDraw7()->CreateSurface(&ddsd2, &m_svideo, NULL);

	m_width = width;
	m_height = height;
	m_pDDp = pDDp;

	// ページロック
	if (!m_svideo)
	{
		DDCAPS ddc;

		ZeroMemory(&ddc, sizeof(ddc));
		ddc.dwSize = sizeof(ddc);
		if ( FAILED(pDDp->GetDirectDraw7()->GetCaps(&ddc, NULL)) ||
			(ddc.dwCaps & DDCAPS_CANBLTSYSMEM && !(ddc.dwCaps2 & DDCAPS2_NOPAGELOCKREQUIRED)) )
			m_ssystem->PageLock(0);
	}

	return ret;
}

// ビットマップで初期化
HRESULT CManagedSurface::Initialize(HINSTANCE hInstance, LPCTSTR lpszName, CDirectDrawPlus* pDDp)
{
	HRESULT ret;
	DDSURFACEDESC2 ddsd2;
	DDCAPS ddc;

	Release();

	// 関数を使ってシステムサーフェス生成
	if ( FAILED(ret = pDDp->CreateSurfaceFromBitmap(hInstance, lpszName, &m_ssystem, DDSCAPS_SYSTEMMEMORY)) )
		return ret;

	// ページロックができるならする
	ZeroMemory(&ddc, sizeof(ddc));
	ddc.dwSize = sizeof(ddc);
	if ( SUCCEEDED(pDDp->GetDirectDraw7()->GetCaps(&ddc, NULL)) &&
		(ddc.dwCaps & DDCAPS_CANBLTSYSMEM && !(ddc.dwCaps2 & DDCAPS2_NOPAGELOCKREQUIRED)) )
		m_ssystem->PageLock(0);

	// ビデオサーフェス生成、サイズはシステムメモリのを流用。
	ZeroMemory(&ddsd2, sizeof(ddsd2));
	ddsd2.dwSize = sizeof(ddsd2);
	m_ssystem->GetSurfaceDesc(&ddsd2);
	ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;

	pDDp->GetDirectDraw7()->CreateSurface(&ddsd2, &m_svideo, NULL);

	m_width = ddsd2.dwWidth;
	m_height = ddsd2.dwHeight;
	m_pDDp = pDDp;

	// ビデオサーフェス更新
	if ( SUCCEEDED(UpdateSurface()) )
		m_ssystem->PageUnlock(0);		// ビデオメモリを使うからロック解除

	return ret;
}

HRESULT CManagedSurface::UpdateSurface(LPRECT lpRect/* = NULL*/)
{
	if (m_pDDp == NULL)
		return E_FAIL;

	if (m_ssystem && m_svideo)
	{
		const RECT rect = { 0, 0, m_width, m_height };
		DDCOLORKEY hoge;
		HRESULT hr;

		// ビデオメモリを立て直しておく。
		if ( FAILED(hr = m_svideo->Restore()) )
			return hr;

		m_ssystem->GetColorKey(DDCKEY_SRCBLT, &hoge);
		m_svideo->SetColorKey(DDCKEY_SRCBLT, &hoge);
		return m_pDDp->Blt(m_svideo, lpRect ? *lpRect : rect, m_ssystem, lpRect ? *lpRect : rect, DDBLT_WAIT, DDBLTFAST_NOCOLORKEY | DDBLTFAST_WAIT);
	}

	return E_FAIL;
}

HRESULT CManagedSurface::Blt(LPDIRECTDRAWSURFACE7 pDest, RECT* rDest, RECT* rSrc, DWORD Flags,
							 DWORD FlagsFast/* = 0*/, LPDDBLTFX pBltFx/* = NULL*/)
{
	DDSCAPS2 ddscaps;

	if (m_pDDp == NULL)
		return E_FAIL;

	if ( m_svideo &&
		 // 転送先がシステムメモリならやめとく
		 !(SUCCEEDED(pDest->GetCaps(&ddscaps)) &&
		   ddscaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
	{
		// ビデオメモリから描画
		HRESULT hr = m_pDDp->Blt(pDest, *rDest, m_svideo, *rSrc, Flags, FlagsFast, pBltFx);

		if (SUCCEEDED(hr) || hr == DDERR_WASSTILLDRAWING)
			return hr;

		// ビデオメモリが無くなってたら作り直す
		if ( hr == DDERR_SURFACELOST && SUCCEEDED(UpdateSurface()) )
			return m_pDDp->Blt(pDest, *rDest, m_svideo, *rSrc, Flags, FlagsFast, pBltFx);	// もう一回
	}
	
	// 失敗したらシステムメモリを直書き
	return m_pDDp->Blt(pDest, *rDest, m_ssystem, *rSrc, Flags, FlagsFast);
}

HRESULT CManagedSurface::Blt(LPDIRECTDRAWSURFACE7 pDest, int x, int y, int vx, int vy, int xDest, int yDest,
							 DWORD Flags, DWORD FlagsFast/* = 0*/, double ScaleX, double ScaleY, LPDDBLTFX pBltFx/* = NULL*/)
{
	RECT rSrc, rDest;

	// 矩形定義
	rSrc.left = x;
	rSrc.top = y;
	rSrc.right = x + vx;
	rSrc.bottom = y + vy;

	// 送信先矩形定義
	rDest.left = xDest;
	rDest.top = yDest;
	rDest.right = xDest + (int)(vx * ScaleX);
	rDest.bottom = yDest + (int)(vy * ScaleY);

	return Blt(pDest, &rDest, &rSrc, Flags, FlagsFast, pBltFx);
}

// 解放
void CManagedSurface::Release(void)
{
	if (m_svideo)
	{
		m_svideo->Release();
		m_svideo = NULL;
	}

	if (m_ssystem)
	{
		m_ssystem->Release();
		m_ssystem = NULL;
	}
}

// カラーキー設定
HRESULT CManagedSurface::DDSetColorKey( DWORD dwColorKey )
{
    HRESULT hr;
	
	if( NULL == m_ssystem )
        return E_POINTER;

//    m_bColorKeyed = TRUE;

    DDCOLORKEY ddck;
	ddck.dwColorSpaceLowValue  = CDirectDrawPlus::ConvertGDIColor( m_ssystem, dwColorKey );
    ddck.dwColorSpaceHighValue = CDirectDrawPlus::ConvertGDIColor( m_ssystem, dwColorKey );
    
    hr = m_ssystem->SetColorKey( DDCKEY_SRCBLT, &ddck );
	if (m_svideo)	// ビデオサーフェスにも設定
		m_svideo->SetColorKey( DDCKEY_SRCBLT, &ddck );
	
	return hr;
}

HRESULT CManagedSurface::Unlock(LPRECT lpRect)
{
	HRESULT result = m_ssystem->Unlock(lpRect);

	UpdateSurface(lpRect);	// 暫定
	return result;
}
