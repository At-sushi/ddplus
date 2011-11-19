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

// ������
HRESULT CManagedSurface::Initialize(int width, int height, CDirectDrawPlus* pDDp)
{
	HRESULT ret;
	DDSURFACEDESC2 ddsd2;

	Release();

	// �V�X�e����������
	ZeroMemory(&ddsd2, sizeof(ddsd2));
	ddsd2.dwSize = sizeof(ddsd2);
	ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	ddsd2.dwWidth = width;
	ddsd2.dwHeight = height;

	if ( FAILED(ret = pDDp->GetDirectDraw7()->CreateSurface(&ddsd2, &m_ssystem, NULL)) )
		return ret;

	// �r�f�I��������
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

	// �y�[�W���b�N
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

// �r�b�g�}�b�v�ŏ�����
HRESULT CManagedSurface::Initialize(HINSTANCE hInstance, LPCTSTR lpszName, CDirectDrawPlus* pDDp)
{
	HRESULT ret;
	DDSURFACEDESC2 ddsd2;
	DDCAPS ddc;

	Release();

	// �֐����g���ăV�X�e���T�[�t�F�X����
	if ( FAILED(ret = pDDp->CreateSurfaceFromBitmap(hInstance, lpszName, &m_ssystem, DDSCAPS_SYSTEMMEMORY)) )
		return ret;

	// �y�[�W���b�N���ł���Ȃ炷��
	ZeroMemory(&ddc, sizeof(ddc));
	ddc.dwSize = sizeof(ddc);
	if ( SUCCEEDED(pDDp->GetDirectDraw7()->GetCaps(&ddc, NULL)) &&
		(ddc.dwCaps & DDCAPS_CANBLTSYSMEM && !(ddc.dwCaps2 & DDCAPS2_NOPAGELOCKREQUIRED)) )
		m_ssystem->PageLock(0);

	// �r�f�I�T�[�t�F�X�����A�T�C�Y�̓V�X�e���������̂𗬗p�B
	ZeroMemory(&ddsd2, sizeof(ddsd2));
	ddsd2.dwSize = sizeof(ddsd2);
	m_ssystem->GetSurfaceDesc(&ddsd2);
	ddsd2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddsd2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;

	pDDp->GetDirectDraw7()->CreateSurface(&ddsd2, &m_svideo, NULL);

	m_width = ddsd2.dwWidth;
	m_height = ddsd2.dwHeight;
	m_pDDp = pDDp;

	// �r�f�I�T�[�t�F�X�X�V
	if ( SUCCEEDED(UpdateSurface()) )
		m_ssystem->PageUnlock(0);		// �r�f�I���������g�����烍�b�N����

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

		// �r�f�I�������𗧂Ē����Ă����B
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
		 // �]���悪�V�X�e���������Ȃ��߂Ƃ�
		 !(SUCCEEDED(pDest->GetCaps(&ddscaps)) &&
		   ddscaps.dwCaps & DDSCAPS_SYSTEMMEMORY) )
	{
		// �r�f�I����������`��
		HRESULT hr = m_pDDp->Blt(pDest, *rDest, m_svideo, *rSrc, Flags, FlagsFast, pBltFx);

		if (SUCCEEDED(hr) || hr == DDERR_WASSTILLDRAWING)
			return hr;

		// �r�f�I�������������Ȃ��Ă����蒼��
		if ( hr == DDERR_SURFACELOST && SUCCEEDED(UpdateSurface()) )
			return m_pDDp->Blt(pDest, *rDest, m_svideo, *rSrc, Flags, FlagsFast, pBltFx);	// �������
	}
	
	// ���s������V�X�e���������𒼏���
	return m_pDDp->Blt(pDest, *rDest, m_ssystem, *rSrc, Flags, FlagsFast);
}

HRESULT CManagedSurface::Blt(LPDIRECTDRAWSURFACE7 pDest, int x, int y, int vx, int vy, int xDest, int yDest,
							 DWORD Flags, DWORD FlagsFast/* = 0*/, double ScaleX, double ScaleY, LPDDBLTFX pBltFx/* = NULL*/)
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

	return Blt(pDest, &rDest, &rSrc, Flags, FlagsFast, pBltFx);
}

// ���
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

// �J���[�L�[�ݒ�
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
	if (m_svideo)	// �r�f�I�T�[�t�F�X�ɂ��ݒ�
		m_svideo->SetColorKey( DDCKEY_SRCBLT, &ddck );
	
	return hr;
}

HRESULT CManagedSurface::Unlock(LPRECT lpRect)
{
	HRESULT result = m_ssystem->Unlock(lpRect);

	UpdateSurface(lpRect);	// �b��
	return result;
}
