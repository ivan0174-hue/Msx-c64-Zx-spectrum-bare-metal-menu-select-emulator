//
// input.h - USB keyboard (raw HID codes) and USB gamepads, with menu key events
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#ifndef _rpiretro_input_h
#define _rpiretro_input_h

#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/usb/usbgamepad.h>
#include <circle/types.h>

// gamepad buttons (normalized)
#define PAD_UP		(1 << 0)
#define PAD_DOWN	(1 << 1)
#define PAD_LEFT	(1 << 2)
#define PAD_RIGHT	(1 << 3)
#define PAD_A		(1 << 4)	// south / primary fire
#define PAD_B		(1 << 5)	// east  / secondary fire
#define PAD_X		(1 << 6)
#define PAD_Y		(1 << 7)
#define PAD_START	(1 << 8)
#define PAD_SELECT	(1 << 9)
#define PAD_L		(1 << 10)
#define PAD_R		(1 << 11)
#define PAD_HOME	(1 << 12)

// USB HID usage codes used by the menus
#define HID_A		0x04
#define HID_Z		0x1D
#define HID_1		0x1E
#define HID_0		0x27
#define HID_ENTER	0x28
#define HID_ESC		0x29
#define HID_BACKSPACE	0x2A
#define HID_TAB		0x2B
#define HID_SPACE	0x2C
#define HID_F1		0x3A
#define HID_F10		0x43
#define HID_F11		0x44
#define HID_F12		0x45
#define HID_HOME	0x4A
#define HID_PAGEUP	0x4B
#define HID_DELETE	0x4C
#define HID_END		0x4D
#define HID_PAGEDOWN	0x4E
#define HID_RIGHT	0x4F
#define HID_LEFT	0x50
#define HID_DOWN	0x51
#define HID_UP		0x52
#define HID_KPENTER	0x58
#define HID_LCTRL	0xE0
#define HID_LSHIFT	0xE1
#define HID_LALT	0xE2
#define HID_LGUI	0xE3
#define HID_RCTRL	0xE4
#define HID_RSHIFT	0xE5
#define HID_RALT	0xE6
#define HID_RGUI	0xE7

enum TMenuKey
{
	MenuKeyNone,
	MenuKeyUp,
	MenuKeyDown,
	MenuKeyLeft,
	MenuKeyRight,
	MenuKeyEnter,
	MenuKeyBack,
	MenuKeyPageUp,
	MenuKeyPageDown,
	MenuKeyHome,
	MenuKeyEnd,
	MenuKeyDelete,
	MenuKeyMenu,		// F12 / gamepad HOME or SELECT+START
	MenuKeyChar		// printable character, see GetLastChar()
};

#define MAX_PADS	2

class CInput
{
public:
	CInput (CUSBHCIDevice *pUSBHCI);
	~CInput (void);

	// call frequently from the main loop (handles USB plug-and-play)
	void Update (void);

	boolean HasKeyboard (void) const	{ return m_pKeyboard != 0; }
	unsigned GetNumPads (void) const;

	// raw state
	boolean IsKeyDown (u8 ucHIDCode) const	{ return m_KeyDown[ucHIDCode]; }
	const volatile boolean *GetKeyArray (void) const { return m_KeyDown; }
	unsigned GetPad (unsigned nPad) const	{ return nPad < MAX_PADS ? m_PadState[nPad] : 0; }

	// menu events (with auto repeat)
	TMenuKey GetMenuKey (void);
	char GetLastChar (void) const		{ return m_chLastChar; }

	// was the "open menu" combo pressed since last call? (used while emulating)
	boolean MenuRequested (void);

	void FlushEvents (void);

	static CInput *Get (void)		{ return s_pThis; }

private:
	void PushEvent (u16 usCode);
	TMenuKey Translate (u16 usCode, char *pChar);
	boolean IsCodeHeld (u16 usCode) const;

	static void KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6]);
	static void KeyboardRemovedHandler (CDevice *pDevice, void *pContext);
	static void GamePadStatusHandler (unsigned nDeviceIndex, const TGamePadState *pState);
	static void GamePadRemovedHandler (CDevice *pDevice, void *pContext);

private:
	CUSBHCIDevice *m_pUSBHCI;
	CUSBKeyboardDevice * volatile m_pKeyboard;
	CUSBGamePadDevice * volatile m_pPad[MAX_PADS];
	unsigned m_PadProps[MAX_PADS];

	volatile boolean m_KeyDown[256];
	volatile unsigned m_PadState[MAX_PADS];

	// event ring buffer: HID codes (0..255) or 0x100 + pad button bit index (+ 0x80 * pad)
	static const unsigned QueueSize = 64;
	volatile u16 m_Queue[QueueSize];
	volatile unsigned m_nQueueIn;
	volatile unsigned m_nQueueOut;

	// auto repeat
	u16 m_usRepeatCode;
	unsigned m_nRepeatStart;
	unsigned m_nRepeatLast;

	char m_chLastChar;
	volatile boolean m_bMenuRequest;

	static CInput *s_pThis;
};

#endif
