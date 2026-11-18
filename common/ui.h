//
// ui.h - list menus and file browser on top of CVideo / CInput
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#ifndef _rpiretro_ui_h
#define _rpiretro_ui_h

#include "video.h"
#include "input.h"

struct TMenuItem
{
	const char *pLabel;		// left column text (0 = separator line)
	char Value[96];			// right column text (empty = none)
	bool bEnabled;
};

enum TMenuResult
{
	MenuResultSelect,		// Enter on an item
	MenuResultLeft,			// Left on an item (decrement value)
	MenuResultRight,		// Right on an item (increment value)
	MenuResultBack,			// Esc / B
	MenuResultTimeout,		// countdown expired
	MenuResultDelete		// Del on an item (clear value)
};

class CUserInterface
{
public:
	CUserInterface (CVideo *pVideo, CInput *pInput);

	// Show a menu. rSelected: in/out selected item index.
	// nTimeoutSecs > 0 shows a countdown; any key stops it.
	TMenuResult Menu (const char *pTitle, const char *pSubTitle,
			  TMenuItem *pItems, unsigned nCount, unsigned &rSelected,
			  const char *pHelp, unsigned nTimeoutSecs = 0);

	// Browse files. pExtensions: list like ".rom.mx1.mx2" (case insensitive), 0 = all.
	// pDirectory: in/out (last visited directory). Returns true and the full path in
	// pResult, or true with empty pResult if "(nessuno)" was chosen; false on cancel.
	bool BrowseFile (const char *pTitle, char *pDirectory, unsigned nDirSize,
			 const char *pExtensions, char *pResult, unsigned nResultSize,
			 bool bAllowNone = true);

	// Message box, waits for a key
	void MessageBox (const char *pTitle, const char *pLine1, const char *pLine2 = 0,
			 const char *pLine3 = 0);

	void DrawFrame (const char *pTitle, const char *pSubTitle, const char *pHelp);

	// Idle callback, called in every UI loop iteration (e.g. for sound/LED)
	typedef void TIdleHandler (void *pParam);
	void SetIdleHandler (TIdleHandler *pHandler, void *pParam)
	{
		m_pIdleHandler = pHandler;
		m_pIdleParam = pParam;
	}

private:
	void Idle (void);

	CVideo *m_pVideo;
	CInput *m_pInput;
	TIdleHandler *m_pIdleHandler;
	void *m_pIdleParam;
};

#endif
