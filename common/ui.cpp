//
// ui.cpp
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#include "ui.h"
#include <circle/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wrap_fatfs.h>

#define LIST_FIRST_ROW	4
#define LIST_ROWS	22
#define HELP_ROW	28
#define INFO_ROW	27

CUserInterface::CUserInterface (CVideo *pVideo, CInput *pInput)
:	m_pVideo (pVideo),
	m_pInput (pInput),
	m_pIdleHandler (0),
	m_pIdleParam (0)
{
}

void CUserInterface::Idle (void)
{
	m_pInput->Update ();
	if (m_pIdleHandler != 0)
	{
		(*m_pIdleHandler) (m_pIdleParam);
	}
	CTimer::SimpleMsDelay (5);
}

void CUserInterface::DrawFrame (const char *pTitle, const char *pSubTitle, const char *pHelp)
{
	m_pVideo->Clear (COL_BG);
	m_pVideo->FillRect (0, 0, CVideo::Width, CVideo::CharH * 2 + 8, COL_PANEL);
	m_pVideo->DrawTextCentered (12, pTitle, COL_TITLE);
	if (pSubTitle != 0)
	{
		m_pVideo->DrawTextCentered (CVideo::CharH * 2 + 14, pSubTitle, COL_DIM);
	}
	m_pVideo->FillRect (0, HELP_ROW * CVideo::CharH - 4, CVideo::Width, CVideo::CharH * 2 + 4, COL_PANEL);
	if (pHelp != 0)
	{
		m_pVideo->DrawTextCentered (HELP_ROW * CVideo::CharH + 4, pHelp, COL_TEXT);
	}
}

TMenuResult CUserInterface::Menu (const char *pTitle, const char *pSubTitle,
				  TMenuItem *pItems, unsigned nCount, unsigned &rSelected,
				  const char *pHelp, unsigned nTimeoutSecs)
{
	if (nCount == 0)
	{
		return MenuResultBack;
	}
	if (rSelected >= nCount)
	{
		rSelected = 0;
	}
	// make sure the selection is on an enabled item
	for (unsigned n = 0; n < nCount && (pItems[rSelected].pLabel == 0 || !pItems[rSelected].bEnabled); n++)
	{
		rSelected = (rSelected + 1) % nCount;
	}

	unsigned nTop = 0;
	bool bRedraw = true;
	unsigned nStart = CTimer::GetClockTicks ();
	unsigned nLastSecs = 0xFFFFFFFF;

	while (1)
	{
		unsigned nRemaining = 0;
		if (nTimeoutSecs > 0)
		{
			unsigned nElapsed = (CTimer::GetClockTicks () - nStart) / 1000000;
			if (nElapsed >= nTimeoutSecs)
			{
				return MenuResultTimeout;
			}
			nRemaining = nTimeoutSecs - nElapsed;
			if (nRemaining != nLastSecs)
			{
				nLastSecs = nRemaining;
				bRedraw = true;
			}
		}

		if (bRedraw)
		{
			bRedraw = false;

			if (rSelected < nTop)
			{
				nTop = rSelected;
			}
			if (rSelected >= nTop + LIST_ROWS)
			{
				nTop = rSelected - LIST_ROWS + 1;
			}

			DrawFrame (pTitle, pSubTitle, pHelp);

			for (unsigned i = 0; i < LIST_ROWS && nTop + i < nCount; i++)
			{
				const TMenuItem &Item = pItems[nTop + i];
				unsigned nRow = LIST_FIRST_ROW + i;
				int y = nRow * CVideo::CharH;

				if (Item.pLabel == 0)
				{
					m_pVideo->FillRect (4 * CVideo::CharW, y + CVideo::CharH / 2,
							    72 * CVideo::CharW, 1, COL_DIM);
					continue;
				}

				bool bSel = nTop + i == rSelected;
				u16 nFG = !Item.bEnabled ? COL_DIM : (bSel ? COL_SEL_FG : COL_TEXT);
				if (bSel)
				{
					m_pVideo->FillRect (3 * CVideo::CharW, y, 74 * CVideo::CharW, CVideo::CharH,
							    COL_SEL_BG);
				}

				bool bHasValue = Item.Value[0] != 0;
				m_pVideo->Print (4, nRow, Item.pLabel, nFG, 0, TRUE, bHasValue ? 30 : 72);
				if (bHasValue)
				{
					m_pVideo->Print (35, nRow, Item.Value, bSel ? COL_SEL_FG : COL_VALUE, 0, TRUE, 41);
				}
			}

			if (nTop > 0)
			{
				m_pVideo->Print (76, LIST_FIRST_ROW - 1, "^", COL_DIM, 0);
			}
			if (nTop + LIST_ROWS < nCount)
			{
				m_pVideo->Print (76, LIST_FIRST_ROW + LIST_ROWS, "v", COL_DIM, 0);
			}

			if (nTimeoutSecs > 0)
			{
				char Buf[80];
				snprintf (Buf, sizeof Buf, "Avvio automatico tra %u s (premi un tasto per fermare)",
					  nRemaining);
				m_pVideo->DrawTextCentered (INFO_ROW * CVideo::CharH - 8, Buf, COL_TITLE);
			}

			m_pVideo->Present ();
		}

		Idle ();

		TMenuKey Key = m_pInput->GetMenuKey ();
		if (Key == MenuKeyNone)
		{
			continue;
		}

		if (nTimeoutSecs > 0)
		{
			nTimeoutSecs = 0;	// any key stops the countdown
			bRedraw = true;
		}

		switch (Key)
		{
		case MenuKeyUp:
		case MenuKeyDown:
		case MenuKeyPageUp:
		case MenuKeyPageDown:
		case MenuKeyHome:
		case MenuKeyEnd: {
			int nDir = (Key == MenuKeyUp || Key == MenuKeyPageUp || Key == MenuKeyEnd) ? -1 : 1;
			int nSteps = (Key == MenuKeyPageUp || Key == MenuKeyPageDown) ? LIST_ROWS : 1;
			int nSel = (int) rSelected;
			if (Key == MenuKeyHome) { nSel = -1; nSteps = 1; nDir = 1; }
			if (Key == MenuKeyEnd)  { nSel = (int) nCount; nSteps = 1; nDir = -1; }
			for (int s = 0; s < nSteps; s++)
			{
				int nNew = nSel;
				do
				{
					nNew += nDir;
				}
				while (   nNew >= 0 && nNew < (int) nCount
				       && (pItems[nNew].pLabel == 0 || !pItems[nNew].bEnabled));

				if (nNew < 0 || nNew >= (int) nCount)
				{
					if (nSteps == 1 && Key != MenuKeyHome && Key != MenuKeyEnd)
					{
						// wrap around
						nNew = nDir > 0 ? -1 : (int) nCount;
						do
						{
							nNew += nDir;
						}
						while (   nNew >= 0 && nNew < (int) nCount
						       && (pItems[nNew].pLabel == 0 || !pItems[nNew].bEnabled));
						if (nNew >= 0 && nNew < (int) nCount)
						{
							nSel = nNew;
						}
					}
					break;
				}
				nSel = nNew;
			}
			rSelected = (unsigned) nSel;
			bRedraw = true;
			} break;

		case MenuKeyLeft:
			return MenuResultLeft;

		case MenuKeyRight:
			return MenuResultRight;

		case MenuKeyEnter:
			return MenuResultSelect;

		case MenuKeyBack:
		case MenuKeyMenu:
			return MenuResultBack;

		case MenuKeyDelete:
			return MenuResultDelete;

		case MenuKeyChar: {
			// jump to the next item starting with this character
			char ch = tolower ((unsigned char) m_pInput->GetLastChar ());
			for (unsigned n = 1; n <= nCount; n++)
			{
				unsigned i = (rSelected + n) % nCount;
				if (   pItems[i].pLabel != 0 && pItems[i].bEnabled
				    && tolower ((unsigned char) pItems[i].pLabel[0]) == ch)
				{
					rSelected = i;
					bRedraw = true;
					break;
				}
			}
			} break;

		default:
			break;
		}
	}
}

void CUserInterface::MessageBox (const char *pTitle, const char *pLine1, const char *pLine2,
				 const char *pLine3)
{
	DrawFrame (pTitle, 0, "Premi INVIO per continuare");
	int y = 10 * CVideo::CharH;
	m_pVideo->DrawTextCentered (y, pLine1, COL_TEXT);
	if (pLine2) m_pVideo->DrawTextCentered (y + 2 * CVideo::CharH, pLine2, COL_TEXT);
	if (pLine3) m_pVideo->DrawTextCentered (y + 4 * CVideo::CharH, pLine3, COL_TEXT);
	m_pVideo->Present ();

	m_pInput->FlushEvents ();
	while (1)
	{
		Idle ();
		TMenuKey Key = m_pInput->GetMenuKey ();
		if (Key == MenuKeyEnter || Key == MenuKeyBack || Key == MenuKeyMenu)
		{
			break;
		}
	}
}

// File browser /////////////////////////////////////////////////////////////

struct TFileEntry
{
	char *pName;
	bool bDir;
};

static int CompareEntries (const void *p1, const void *p2)
{
	const TFileEntry *e1 = (const TFileEntry *) p1;
	const TFileEntry *e2 = (const TFileEntry *) p2;
	if (e1->bDir != e2->bDir)
	{
		return e1->bDir ? -1 : 1;
	}
	return strcasecmp (e1->pName, e2->pName);
}

static bool MatchExtension (const char *pName, const char *pExtensions)
{
	if (pExtensions == 0 || *pExtensions == 0)
	{
		return true;
	}

	const char *pDot = strrchr (pName, '.');
	if (pDot == 0)
	{
		return false;
	}

	char Ext[16];
	unsigned n = 0;
	for (const char *p = pDot; *p && n < sizeof Ext - 1; p++)
	{
		Ext[n++] = tolower ((unsigned char) *p);
	}
	Ext[n] = 0;

	const char *p = pExtensions;
	while ((p = strstr (p, Ext)) != 0)
	{
		char chNext = p[n];
		if (chNext == 0 || chNext == '.')
		{
			return true;
		}
		p++;
	}
	return false;
}

bool CUserInterface::BrowseFile (const char *pTitle, char *pDirectory, unsigned nDirSize,
				 const char *pExtensions, char *pResult, unsigned nResultSize,
				 bool bAllowNone)
{
	static const unsigned MaxEntries = 2048;
	TFileEntry *pEntries = new TFileEntry[MaxEntries];
	unsigned nSelected = 0;

	while (1)
	{
		// read directory
		unsigned nCount = 0;
		bool bRoot = strcmp (pDirectory, "/") == 0 || pDirectory[0] == 0;
		if (pDirectory[0] == 0)
		{
			strcpy (pDirectory, "/");
		}

		if (bAllowNone)
		{
			pEntries[nCount].pName = strdup ("(nessuno / espelli)");
			pEntries[nCount++].bDir = false;
		}
		if (!bRoot)
		{
			pEntries[nCount].pName = strdup ("..");
			pEntries[nCount++].bDir = true;
		}
		unsigned nFixed = nCount;

		FATFS_DIR Dir;
		FILINFO Info;
		if (f_opendir (&Dir, pDirectory) == FR_OK)
		{
			while (   nCount < MaxEntries
			       && f_readdir (&Dir, &Info) == FR_OK
			       && Info.fname[0] != 0)
			{
				if (Info.fattrib & (AM_HID | AM_SYS))
				{
					continue;
				}
				if (Info.fname[0] == '.')
				{
					continue;
				}
				bool bDir = (Info.fattrib & AM_DIR) != 0;
				if (!bDir && !MatchExtension (Info.fname, pExtensions))
				{
					continue;
				}
				pEntries[nCount].pName = strdup (Info.fname);
				pEntries[nCount++].bDir = bDir;
			}
			f_closedir (&Dir);
		}
		else if (!bRoot)
		{
			// directory vanished: go to root
			strcpy (pDirectory, "/");
			for (unsigned i = 0; i < nCount; i++)
			{
				free (pEntries[i].pName);
			}
			continue;
		}

		qsort (pEntries + nFixed, nCount - nFixed, sizeof (TFileEntry), CompareEntries);

		// build menu items
		TMenuItem *pItems = new TMenuItem[nCount];
		for (unsigned i = 0; i < nCount; i++)
		{
			pItems[i].pLabel = pEntries[i].pName;
			pItems[i].Value[0] = 0;
			pItems[i].bEnabled = true;
			if (pEntries[i].bDir)
			{
				strcpy (pItems[i].Value, "<DIR>");
			}
		}

		char SubTitle[128];
		snprintf (SubTitle, sizeof SubTitle, "Cartella: %s  (%u elementi)", pDirectory, nCount - nFixed);

		int nAction = -1;	// -1 = continue, 0 = cancel, 1 = chosen
		while (nAction < 0)
		{
			TMenuResult Result = Menu (pTitle, SubTitle, pItems, nCount, nSelected,
						   "INVIO apri/scegli   <- cartella su   ESC annulla   lettera = salta");
			if (Result == MenuResultBack)
			{
				nAction = 0;
			}
			else if (Result == MenuResultLeft && !bRoot)
			{
				// go to parent
				char *p = strrchr (pDirectory, '/');
				if (p != 0 && p != pDirectory) *p = 0; else strcpy (pDirectory, "/");
				nSelected = 0;
				nAction = 2;
			}
			else if (Result == MenuResultSelect || Result == MenuResultRight)
			{
				TFileEntry &E = pEntries[nSelected];
				if (bAllowNone && nSelected == 0)
				{
					if (Result == MenuResultSelect)
					{
						pResult[0] = 0;
						nAction = 1;
					}
				}
				else if (E.bDir)
				{
					if (strcmp (E.pName, "..") == 0)
					{
						char *p = strrchr (pDirectory, '/');
						if (p != 0 && p != pDirectory) *p = 0; else strcpy (pDirectory, "/");
					}
					else
					{
						size_t n = strlen (pDirectory);
						if (n + strlen (E.pName) + 2 < nDirSize)
						{
							if (n > 1) strcat (pDirectory, "/");
							strcat (pDirectory, E.pName);
						}
					}
					nSelected = 0;
					nAction = 2;
				}
				else if (Result == MenuResultSelect)
				{
					snprintf (pResult, nResultSize, "%s%s%s", pDirectory,
						  strcmp (pDirectory, "/") == 0 ? "" : "/", E.pName);
					nAction = 1;
				}
			}

		}

		for (unsigned i = 0; i < nCount; i++)
		{
			free (pEntries[i].pName);
		}
		delete [] pItems;

		if (nAction == 0 || nAction == 1)
		{
			delete [] pEntries;
			return nAction == 1;
		}
	}
}
