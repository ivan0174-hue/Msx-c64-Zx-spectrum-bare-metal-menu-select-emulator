//
// video.cpp
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#include "video.h"
#include <circle/util.h>
#include <circle/synchronize.h>
#include <circle/logger.h>
#include <circle/bcmpropertytags.h>

static const char FromVideo[] = "video";

CVideo::CVideo (void)
:	m_pFrameBuffer (0),
	m_nFBWidth (Width),
	m_nOffsetX (0),
	m_pBack (0),
	m_nPage (0),
	m_Font (Font8x16)
{
}

CVideo::~CVideo (void)
{
	delete m_pFrameBuffer;
	delete [] m_pBack;
}

boolean CVideo::Initialize (void)
{
	// The GPU scales the frame buffer to the whole screen. To keep a 4:3 picture on
	// a 16:9 display, the frame buffer gets the display's aspect ratio (e.g. 856x480)
	// and the 640x480 image is centered in it.
	CBcmPropertyTags Tags;
	TPropertyTagDisplayDimensions Dimensions;
	if (   Tags.GetTag (PROPTAG_GET_DISPLAY_DIMENSIONS, &Dimensions, sizeof Dimensions)
	    && Dimensions.nWidth >= 320 && Dimensions.nHeight >= 200
	    && Dimensions.nWidth * 3 > Dimensions.nHeight * 4)
	{
		m_nFBWidth = (Height * Dimensions.nWidth / Dimensions.nHeight + 15) & ~15U;
		if (m_nFBWidth > 2 * Width)
		{
			m_nFBWidth = 2 * Width;
		}
	}
	m_nOffsetX = ((m_nFBWidth - Width) / 2) & ~1U;

	// virtual height doubled for page flipping
	m_pFrameBuffer = new CBcmFrameBuffer (m_nFBWidth, Height, 16, m_nFBWidth, Height * 2);
	if (   m_pFrameBuffer == 0
	    || !m_pFrameBuffer->Initialize ())
	{
		CLogger::Get ()->Write (FromVideo, LogError, "Cannot initialize frame buffer");
		return FALSE;
	}

	m_pBack = new u16[Width * Height];
	if (m_pBack == 0)
	{
		return FALSE;
	}

	// clear both pages including the side bars
	memset ((void *) (uintptr) m_pFrameBuffer->GetBuffer (), 0, m_pFrameBuffer->GetPitch () * Height * 2);

	Clear (COL_BLACK);
	Present ();
	Present ();

	return TRUE;
}

void CVideo::Present (void)
{
#ifdef CIRCLE_QEMU
	unsigned nNewPage = 0;		// QEMU ignores the virtual offset
#else
	unsigned nNewPage = m_nPage ^ 1;
#endif
	u32 nPitch = m_pFrameBuffer->GetPitch ();
	u8 *pDest = (u8 *) (uintptr) m_pFrameBuffer->GetBuffer () + nNewPage * Height * nPitch;

	pDest += m_nOffsetX * 2;
	if (nPitch == Width * 2)
	{
		memcpy (pDest, m_pBack, Width * Height * 2);
	}
	else
	{
		for (unsigned y = 0; y < Height; y++)
		{
			memcpy (pDest + y * nPitch, m_pBack + y * Width, Width * 2);
		}
	}

	DataSyncBarrier ();

	m_pFrameBuffer->SetVirtualOffset (0, nNewPage * Height);
	m_nPage = nNewPage;
}

void CVideo::Clear (u16 nColor)
{
	u32 n32 = nColor | ((u32) nColor << 16);
	u32 *p = (u32 *) m_pBack;
	for (unsigned i = 0; i < Width * Height / 2; i++)
	{
		p[i] = n32;
	}
}

void CVideo::FillRect (int x, int y, int w, int h, u16 nColor)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > (int) Width)  w = Width - x;
	if (y + h > (int) Height) h = Height - y;
	if (w <= 0 || h <= 0)
	{
		return;
	}

	for (int j = 0; j < h; j++)
	{
		u16 *p = m_pBack + (y + j) * Width + x;
		for (int i = 0; i < w; i++)
		{
			p[i] = nColor;
		}
	}
}

void CVideo::FrameRect (int x, int y, int w, int h, u16 nColor)
{
	FillRect (x, y, w, 1, nColor);
	FillRect (x, y + h - 1, w, 1, nColor);
	FillRect (x, y, 1, h, nColor);
	FillRect (x + w - 1, y, 1, h, nColor);
}

void CVideo::DrawText (int x, int y, const char *pText, u16 nFG, u16 nBG, boolean bTransparent,
		       unsigned nMaxChars)
{
	for (; *pText && nMaxChars > 0; pText++, nMaxChars--, x += CharW)
	{
		if (x < 0 || x + (int) CharW > (int) Width || y < 0 || y + (int) CharH > (int) Height)
		{
			continue;
		}

		char ch = *pText;
		for (unsigned cy = 0; cy < CharH; cy++)
		{
			CCharGenerator::TPixelLine Line = m_Font.GetPixelLine (ch, cy);
			u16 *p = m_pBack + (y + cy) * Width + x;
			for (unsigned cx = 0; cx < CharW; cx++)
			{
				if (m_Font.GetPixel (cx, Line))
				{
					p[cx] = nFG;
				}
				else if (!bTransparent)
				{
					p[cx] = nBG;
				}
			}
		}
	}
}

void CVideo::DrawTextCentered (int y, const char *pText, u16 nFG)
{
	int x = ((int) Width - (int) strlen (pText) * (int) CharW) / 2;
	DrawText (x, y, pText, nFG);
}

void CVideo::BlitScaled (const u16 *pSrc, unsigned nWidth, unsigned nHeight, unsigned nPitchBytes)
{
	static unsigned s_nLastW = 0, s_nLastH = 0;
	if (nWidth != s_nLastW || nHeight != s_nLastH)
	{
		Clear (COL_BLACK);
		s_nLastW = nWidth;
		s_nLastH = nHeight;
	}

	unsigned sx = nWidth * 2 <= Width ? 2 : 1;
	unsigned sy = nHeight * 2 <= Height ? 2 : 1;

	unsigned w = nWidth * sx;
	unsigned h = nHeight * sy;
	if (w > Width)  { nWidth = Width / sx;   w = nWidth * sx; }
	if (h > Height) { nHeight = Height / sy; h = nHeight * sy; }

	unsigned ox = ((Width - w) / 2) & ~1U;	// keep 32-bit alignment
	unsigned oy = (Height - h) / 2;

	unsigned nPitch = nPitchBytes / 2;

	for (unsigned y = 0; y < nHeight; y++)
	{
		const u16 *s = pSrc + y * nPitch;
		u16 *d = m_pBack + (oy + y * sy) * Width + ox;

		if (sx == 2)
		{
			u32 *d32 = (u32 *) d;	// ox is even, so d is 4-byte aligned
			for (unsigned x = 0; x < nWidth; x++)
			{
				u32 p = s[x];
				d32[x] = p | (p << 16);
			}
		}
		else
		{
			memcpy (d, s, nWidth * 2);
		}

		if (sy == 2)
		{
			memcpy (d + Width, d, w * 2);
		}
	}
}
