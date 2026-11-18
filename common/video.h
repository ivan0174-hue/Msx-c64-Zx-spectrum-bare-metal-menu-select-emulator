//
// video.h - 640x480 RGB565 double-buffered video output with text helpers
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#ifndef _rpiretro_video_h
#define _rpiretro_video_h

#include <circle/bcmframebuffer.h>
#include <circle/chargenerator.h>
#include <circle/types.h>

#define RGB565(r, g, b)	((u16) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

// UI palette
#define COL_BG		RGB565 (16, 20, 40)
#define COL_PANEL	RGB565 (32, 40, 80)
#define COL_TEXT	RGB565 (220, 220, 220)
#define COL_DIM		RGB565 (130, 130, 150)
#define COL_TITLE	RGB565 (255, 200, 60)
#define COL_SEL_BG	RGB565 (60, 110, 200)
#define COL_SEL_FG	RGB565 (255, 255, 255)
#define COL_VALUE	RGB565 (120, 220, 140)
#define COL_WARN	RGB565 (255, 110, 90)
#define COL_BLACK	RGB565 (0, 0, 0)

class CVideo
{
public:
	static const unsigned Width  = 640;
	static const unsigned Height = 480;
	static const unsigned CharW  = 8;
	static const unsigned CharH  = 16;
	static const unsigned Cols   = Width / CharW;	// 80
	static const unsigned Rows   = Height / CharH;	// 30

	CVideo (void);
	~CVideo (void);

	boolean Initialize (void);

	u16 *GetBackBuffer (void)	{ return m_pBack; }

	// copy back buffer to the hidden page and flip
	void Present (void);

	void Clear (u16 nColor);
	void FillRect (int x, int y, int w, int h, u16 nColor);
	void FrameRect (int x, int y, int w, int h, u16 nColor);

	// bTransparent: background is not drawn
	void DrawText (int x, int y, const char *pText, u16 nFG, u16 nBG = 0, boolean bTransparent = TRUE,
		       unsigned nMaxChars = 0xFFFF);
	void DrawTextCentered (int y, const char *pText, u16 nFG);

	// text cell coordinates (column/row on the 80x30 grid)
	void Print (unsigned nCol, unsigned nRow, const char *pText, u16 nFG, u16 nBG, boolean bTransparent = TRUE,
		    unsigned nMaxChars = 0xFFFF)
	{
		DrawText (nCol * CharW, nRow * CharH, pText, nFG, nBG, bTransparent, nMaxChars);
	}

	// Blit an RGB565 image into the back buffer, integer scaled and centered
	void BlitScaled (const u16 *pSrc, unsigned nWidth, unsigned nHeight, unsigned nPitchBytes);

private:
	CBcmFrameBuffer *m_pFrameBuffer;
	unsigned	 m_nFBWidth;	// frame buffer width (>= 640, matches the display aspect ratio)
	unsigned	 m_nOffsetX;	// where the 640x480 image starts in the frame buffer
	u16		*m_pBack;
	unsigned	 m_nPage;
	CCharGenerator	 m_Font;
};

#endif
