#pragma once

#include <ddraw.h>

// 初期化時パラメータ
typedef struct _DDP_PARAMETERS
{
	DWORD Width, Height;
	DWORD PalletBits;
	BOOL isWindowed;
	DWORD BackBufferCount;
	DWORD RefreshRateHz;
	HWND hDeviceWindow;
	BOOL DoNotFlip;
} DDP_PARAMETERS;

class CDirectDrawPlus
{
public:
	CDirectDrawPlus(void);
	virtual ~CDirectDrawPlus(void);

	// 初期化
	HRESULT Initialize(HWND hwnd, DDP_PARAMETERS* pParam);
	void ReleaseDirectDraw(void);
	const LPDIRECTDRAWSURFACE7 GetBackBuffer() const { return back; }
	const LPDIRECTDRAWSURFACE7 GetPrimaryBuffer() const { return primary; }
	const LPDIRECTDRAW7 GetDirectDraw7() const { return pDD; }
	HRESULT SetPalette(LPDIRECTDRAWPALETTE pal) { return primary->SetPalette(pal); }
	HRESULT GetPalette(LPDIRECTDRAWPALETTE* pPal) { return primary->GetPalette(pPal); }
	HRESULT Present(LPCRECT pSourceRect, LPCRECT pDestRect, BOOL isWait);
	// ダイアログボックス使用モード設定
	HRESULT SetDialogBoxMode(BOOL bEnableDialogs);

	// ガンマ処理
	HRESULT SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData)
		{ return m_pGammaControl ? m_pGammaControl->SetGammaRamp(dwFlags, lpRampData) : E_FAIL; }
	HRESULT GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData)
		{ return m_pGammaControl ? m_pGammaControl->GetGammaRamp(dwFlags, lpRampData) : E_FAIL; }

	// サーフェス間描画
	HRESULT Blt(LPDIRECTDRAWSURFACE7 pDest, RECT rDest, const LPDIRECTDRAWSURFACE7 pSrc, RECT rSrc, DWORD Flags,
				DWORD FlagsFast = 0, const LPDDBLTFX pBltFx = NULL);
	HRESULT Blt(LPDIRECTDRAWSURFACE7 pDest, const LPDIRECTDRAWSURFACE7 pSrc, int x, int y, int vx, int vy,
				int xDest, int yDest, DWORD Flags, DWORD FlagsFast = 0, double ScaleX = 1.0, double ScaleY = 1.0,
				const LPDDBLTFX pBltFx = NULL);

	// ビットマップからオフスクリーンサーフェス作成
	HRESULT CreateSurfaceFromBitmap(HINSTANCE hInstance, LPCTSTR bmp_name, LPDIRECTDRAWSURFACE7* dest, DWORD FlagsAdd = 0);
	// ビットマップからDirectDrawパレット生成
	HRESULT CreatePaletteFromBitmap( LPDIRECTDRAWPALETTE* ppPalette,
									HINSTANCE hInst,
									const TCHAR* strBMP );
	// 画面消去
	HRESULT Clear(DWORD color, BOOL isWait);
	// 主要なサーフェスだけリストア（適当）
	HRESULT Restore(void);

	static HRESULT DrawBitmap( LPDIRECTDRAWSURFACE7 m_pdds, HBITMAP hBMP, 
						DWORD dwBMPOriginX, DWORD dwBMPOriginY, 
						DWORD dwBMPWidth, DWORD dwBMPHeight );
	static HRESULT DDSetColorKey( const LPDIRECTDRAWSURFACE7 m_pdds,
						DWORD dwColorKey );
	static DWORD ConvertGDIColor( const LPDIRECTDRAWSURFACE7 m_pdds,
									COLORREF dwGDIColor );
	// スクリーンショットを保存
	static HRESULT SaveSurfaceToBitmap(LPCTSTR pDestFile, LPDIRECTDRAWSURFACE7 pSrcSurface,
						const PALETTEENTRY* pSrcPalette, const RECT* pSrcRect);

protected:
	LPDIRECTDRAW7 pDD;
	LPDIRECTDRAWSURFACE7 primary;
	LPDIRECTDRAWCLIPPER m_pClipper;
	LPDIRECTDRAWSURFACE7 back;
	LPDIRECTDRAWGAMMACONTROL m_pGammaControl;
	HWND m_hwnd;
	DDP_PARAMETERS m_param;
	BOOL bDialogMode;
public:
};

class CManagedSurface
{
public:
	CManagedSurface(void);
	virtual ~CManagedSurface(void);

	// 初期化
	HRESULT Initialize(int width, int height, CDirectDrawPlus* pDDp);
	// ビットマップで初期化
	HRESULT Initialize(HINSTANCE hInstance, LPCTSTR lpszName, CDirectDrawPlus* pDDp);
	// 解放
	void Release(void);

	const LPDIRECTDRAWSURFACE7 GetSurface() const { return m_ssystem; }
	HRESULT UpdateSurface(LPRECT lpRect = NULL);
	HRESULT Blt(LPDIRECTDRAWSURFACE7 pDest, RECT* rDest, RECT* rSrc, DWORD Flags,
				DWORD FlagsFast = 0, LPDDBLTFX pBltFx = NULL);
	HRESULT Blt(LPDIRECTDRAWSURFACE7 pDest, int x, int y, int vx, int vy, int xDest, int yDest, DWORD Flags,
				 DWORD FlagsFast = 0, double ScaleX = 1.0, double ScaleY = 1.0,LPDDBLTFX pBltFx = NULL);
	HRESULT DDSetColorKey( DWORD dwColorKey );

	// ロック
	HRESULT Lock(
		LPRECT lpDestRect,
		LPDDSURFACEDESC2 lpDDSurfaceDesc,
		DWORD dwFlags,
		HANDLE hEvent = NULL
		)
	{ return m_ssystem->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent); }

	HRESULT Unlock(LPRECT lpRect);

protected:
	LPDIRECTDRAWSURFACE7 m_ssystem, m_svideo;
	int m_width, m_height;
	CDirectDrawPlus* m_pDDp;
};
