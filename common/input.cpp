//
// input.cpp
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#include "input.h"
#include <circle/devicenameservice.h>
#include <circle/timer.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/logger.h>

static const char FromInput[] = "input";

CInput *CInput::s_pThis = 0;

CInput::CInput (CUSBHCIDevice *pUSBHCI)
:	m_pUSBHCI (pUSBHCI),
	m_pKeyboard (0),
	m_nQueueIn (0),
	m_nQueueOut (0),
	m_usRepeatCode (0xFFFF),
	m_nRepeatStart (0),
	m_nRepeatLast (0),
	m_chLastChar (0),
	m_bMenuRequest (FALSE)
{
	s_pThis = this;
	for (unsigned i = 0; i < 256; i++)
	{
		m_KeyDown[i] = FALSE;
	}
	for (unsigned i = 0; i < MAX_PADS; i++)
	{
		m_pPad[i] = 0;
		m_PadProps[i] = 0;
		m_PadState[i] = 0;
	}
}

CInput::~CInput (void)
{
	s_pThis = 0;
}

unsigned CInput::GetNumPads (void) const
{
	unsigned n = 0;
	for (unsigned i = 0; i < MAX_PADS; i++)
	{
		if (m_pPad[i] != 0)
		{
			n++;
		}
	}
	return n;
}

void CInput::Update (void)
{
	boolean bUpdated = m_pUSBHCI->UpdatePlugAndPlay ();

	if (bUpdated)
	{
		if (m_pKeyboard == 0)
		{
			CUSBKeyboardDevice *pKeyboard =
				(CUSBKeyboardDevice *) CDeviceNameService::Get ()->GetDevice ("ukbd1", FALSE);
			if (pKeyboard != 0)
			{
				pKeyboard->RegisterRemovedHandler (KeyboardRemovedHandler);
				pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
				m_pKeyboard = pKeyboard;
				CLogger::Get ()->Write (FromInput, LogNotice, "Keyboard attached");
			}
		}

		for (unsigned i = 0; i < MAX_PADS; i++)
		{
			if (m_pPad[i] != 0)
			{
				continue;
			}

			CString Name;
			Name.Format ("upad%u", i + 1);
			CUSBGamePadDevice *pPad =
				(CUSBGamePadDevice *) CDeviceNameService::Get ()->GetDevice (Name, FALSE);
			if (pPad != 0)
			{
				m_PadProps[i] = pPad->GetProperties ();
				pPad->RegisterRemovedHandler (GamePadRemovedHandler, (void *) (uintptr) (i + 1));
				pPad->RegisterStatusHandler (GamePadStatusHandler);
				m_pPad[i] = pPad;
				CLogger::Get ()->Write (FromInput, LogNotice, "Gamepad %u attached", i + 1);
			}
		}
	}

	if (m_pKeyboard != 0)
	{
		// must not be called in interrupt context
		m_pKeyboard->UpdateLEDs ();
	}
}

void CInput::KeyboardRemovedHandler (CDevice *pDevice, void *pContext)
{
	if (s_pThis != 0)
	{
		s_pThis->m_pKeyboard = 0;
		for (unsigned i = 0; i < 256; i++)
		{
			s_pThis->m_KeyDown[i] = FALSE;
		}
	}
}

void CInput::GamePadRemovedHandler (CDevice *pDevice, void *pContext)
{
	unsigned i = (unsigned) (uintptr) pContext;
	if (s_pThis != 0 && i >= 1 && i <= MAX_PADS)
	{
		s_pThis->m_pPad[i - 1] = 0;
		s_pThis->m_PadState[i - 1] = 0;
	}
}

void CInput::KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	CInput *pThis = s_pThis;
	if (pThis == 0)
	{
		return;
	}

	boolean NewDown[256];
	memset (NewDown, 0, sizeof NewDown);

	for (unsigned i = 0; i < 6; i++)
	{
		u8 k = RawKeys[i];
		if (k >= 4)		// 0 = none, 1..3 = error codes
		{
			NewDown[k] = TRUE;
		}
	}
	for (unsigned i = 0; i < 8; i++)
	{
		if (ucModifiers & (1 << i))
		{
			NewDown[0xE0 + i] = TRUE;
		}
	}

	for (unsigned k = 0; k < 256; k++)
	{
		if (NewDown[k] && !pThis->m_KeyDown[k])
		{
			pThis->PushEvent (k);
		}
		pThis->m_KeyDown[k] = NewDown[k];
	}

	if (NewDown[HID_F12])
	{
		pThis->m_bMenuRequest = TRUE;
	}
}

void CInput::GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState)
{
	CInput *pThis = s_pThis;
	if (pThis == 0 || nDeviceIndex >= MAX_PADS || pState == 0)
	{
		return;
	}

	unsigned nBits = 0;
	unsigned b = pState->buttons;

	if (pThis->m_PadProps[nDeviceIndex] & GamePadPropertyIsKnown)
	{
		if (b & GamePadButtonUp)	nBits |= PAD_UP;
		if (b & GamePadButtonDown)	nBits |= PAD_DOWN;
		if (b & GamePadButtonLeft)	nBits |= PAD_LEFT;
		if (b & GamePadButtonRight)	nBits |= PAD_RIGHT;
		if (b & GamePadButtonA)		nBits |= PAD_A;
		if (b & GamePadButtonB)		nBits |= PAD_B;
		if (b & GamePadButtonX)		nBits |= PAD_X;
		if (b & GamePadButtonY)		nBits |= PAD_Y;
		if (b & (GamePadButtonStart | GamePadButtonPlus))	nBits |= PAD_START;
		if (b & (GamePadButtonSelect | GamePadButtonMinus))	nBits |= PAD_SELECT;
		if (b & GamePadButtonL1)	nBits |= PAD_L;
		if (b & GamePadButtonR1)	nBits |= PAD_R;
		if (b & GamePadButtonGuide)	nBits |= PAD_HOME;
	}
	else
	{
		// generic HID gamepad: guess a common layout
		if (b & (1 << 0))	nBits |= PAD_A;
		if (b & (1 << 1))	nBits |= PAD_B;
		if (b & (1 << 2))	nBits |= PAD_X;
		if (b & (1 << 3))	nBits |= PAD_Y;
		if (b & (1 << 4))	nBits |= PAD_L;
		if (b & (1 << 5))	nBits |= PAD_R;
		if (b & (1 << 8))	nBits |= PAD_SELECT;
		if (b & (1 << 9))	nBits |= PAD_START;
		if (b & (1 << 12))	nBits |= PAD_HOME;
	}

	// left stick / first two axes
	for (int a = 0; a < 2 && a < pState->naxes; a++)
	{
		int nMin = pState->axes[a].minimum;
		int nMax = pState->axes[a].maximum;
		int nRange = nMax - nMin;
		if (nRange <= 0)
		{
			continue;
		}
		int nLow  = nMin + nRange / 4;
		int nHigh = nMax - nRange / 4;
		int v = pState->axes[a].value;
		if (v < nLow)  nBits |= a == 0 ? PAD_LEFT : PAD_UP;
		if (v > nHigh) nBits |= a == 0 ? PAD_RIGHT : PAD_DOWN;
	}

	// first hat (0 = up, clockwise, 8 directions; other values = centered)
	if (pState->nhats > 0)
	{
		static const unsigned HatMap[8] =
		{
			PAD_UP, PAD_UP | PAD_RIGHT, PAD_RIGHT, PAD_DOWN | PAD_RIGHT,
			PAD_DOWN, PAD_DOWN | PAD_LEFT, PAD_LEFT, PAD_UP | PAD_LEFT
		};
		int h = pState->hats[0];
		if (h >= 0 && h < 8)
		{
			nBits |= HatMap[h];
		}
	}

	unsigned nOld = pThis->m_PadState[nDeviceIndex];
	unsigned nNew = nBits & ~nOld;
	for (unsigned i = 0; i < 16; i++)
	{
		if (nNew & (1 << i))
		{
			pThis->PushEvent (0x100 + nDeviceIndex * 0x20 + i);
		}
	}
	pThis->m_PadState[nDeviceIndex] = nBits;

	if (   (nNew & PAD_HOME)
	    || ((nBits & (PAD_SELECT | PAD_START)) == (PAD_SELECT | PAD_START) && (nNew & (PAD_SELECT | PAD_START))))
	{
		pThis->m_bMenuRequest = TRUE;
	}
}

void CInput::PushEvent (u16 usCode)
{
	unsigned nNext = (m_nQueueIn + 1) % QueueSize;
	if (nNext != m_nQueueOut)
	{
		m_Queue[m_nQueueIn] = usCode;
		m_nQueueIn = nNext;
	}
}

void CInput::FlushEvents (void)
{
	m_nQueueOut = m_nQueueIn;
	m_usRepeatCode = 0xFFFF;
	m_bMenuRequest = FALSE;
}

boolean CInput::MenuRequested (void)
{
	if (m_bMenuRequest)
	{
		m_bMenuRequest = FALSE;
		return TRUE;
	}
	return FALSE;
}

boolean CInput::IsCodeHeld (u16 usCode) const
{
	if (usCode < 0x100)
	{
		return m_KeyDown[usCode];
	}

	unsigned nPad = (usCode - 0x100) / 0x20;
	unsigned nBit = (usCode - 0x100) % 0x20;
	return nPad < MAX_PADS && (m_PadState[nPad] & (1 << nBit));
}

TMenuKey CInput::Translate (u16 usCode, char *pChar)
{
	*pChar = 0;

	if (usCode >= 0x100)
	{
		unsigned nBit = 1 << ((usCode - 0x100) % 0x20);
		switch (nBit)
		{
		case PAD_UP:		return MenuKeyUp;
		case PAD_DOWN:		return MenuKeyDown;
		case PAD_LEFT:		return MenuKeyLeft;
		case PAD_RIGHT:		return MenuKeyRight;
		case PAD_A:		return MenuKeyEnter;
		case PAD_START:		return MenuKeyEnter;
		case PAD_B:		return MenuKeyBack;
		case PAD_L:		return MenuKeyPageUp;
		case PAD_R:		return MenuKeyPageDown;
		case PAD_HOME:		return MenuKeyMenu;
		default:		return MenuKeyNone;
		}
	}

	boolean bShift = m_KeyDown[HID_LSHIFT] || m_KeyDown[HID_RSHIFT];

	switch (usCode)
	{
	case HID_UP:		return MenuKeyUp;
	case HID_DOWN:		return MenuKeyDown;
	case HID_LEFT:		return MenuKeyLeft;
	case HID_RIGHT:		return MenuKeyRight;
	case HID_ENTER:
	case HID_KPENTER:	return MenuKeyEnter;
	case HID_ESC:
	case HID_BACKSPACE:	return MenuKeyBack;
	case HID_PAGEUP:	return MenuKeyPageUp;
	case HID_PAGEDOWN:	return MenuKeyPageDown;
	case HID_HOME:		return MenuKeyHome;
	case HID_END:		return MenuKeyEnd;
	case HID_DELETE:	return MenuKeyDelete;
	case HID_F12:		return MenuKeyMenu;
	case HID_SPACE:		*pChar = ' '; return MenuKeyChar;
	default:
		break;
	}

	if (usCode >= HID_A && usCode <= HID_Z)
	{
		*pChar = (bShift ? 'A' : 'a') + (usCode - HID_A);
		return MenuKeyChar;
	}
	if (usCode >= HID_1 && usCode <= HID_0)
	{
		*pChar = usCode == HID_0 ? '0' : '1' + (usCode - HID_1);
		return MenuKeyChar;
	}

	return MenuKeyNone;
}

TMenuKey CInput::GetMenuKey (void)
{
	unsigned nNow = CTimer::GetClockTicks ();	// microseconds

	while (m_nQueueOut != m_nQueueIn)
	{
		u16 usCode = m_Queue[m_nQueueOut];
		m_nQueueOut = (m_nQueueOut + 1) % QueueSize;

		char ch;
		TMenuKey Key = Translate (usCode, &ch);
		if (Key == MenuKeyNone)
		{
			continue;
		}

		m_chLastChar = ch;

		// navigation keys auto-repeat
		if (Key >= MenuKeyUp && Key <= MenuKeyRight)
		{
			m_usRepeatCode = usCode;
			m_nRepeatStart = nNow;
			m_nRepeatLast = nNow;
		}
		else if (Key == MenuKeyPageUp || Key == MenuKeyPageDown)
		{
			m_usRepeatCode = usCode;
			m_nRepeatStart = nNow;
			m_nRepeatLast = nNow;
		}
		else
		{
			m_usRepeatCode = 0xFFFF;
		}

		if (Key == MenuKeyMenu)
		{
			m_bMenuRequest = FALSE;
		}

		return Key;
	}

	if (m_usRepeatCode != 0xFFFF)
	{
		if (!IsCodeHeld (m_usRepeatCode))
		{
			m_usRepeatCode = 0xFFFF;
		}
		else if (   nNow - m_nRepeatStart > 400000
			 && nNow - m_nRepeatLast > 70000)
		{
			m_nRepeatLast = nNow;
			char ch;
			return Translate (m_usRepeatCode, &ch);
		}
	}

	return MenuKeyNone;
}
