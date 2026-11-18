//
// kernel.h - Bare metal MSX (fMSX) for Raspberry Pi 3
//
// Part of rpi-retro. License: GPLv3 (frontend); fMSX has its own license, see fmsx/LICENSE
//
#ifndef _msx_kernel_h
#define _msx_kernel_h

#include <circle_stdlib_app.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/cputhrottle.h>
#include "../common/video.h"
#include "../common/input.h"
#include "../common/ui.h"
#include "../common/config.h"

#define MSX_PATH_SIZE	256

struct TMSXSettings
{
	int nModel;		// 0 MSX1, 1 MSX2, 2 MSX2+
	int nRAM;		// index into RAM table
	int nVRAM;		// index into VRAM table
	int nCPU;		// index into CPU speed table
	int nVideo;		// 0 NTSC 60 Hz, 1 PAL 50 Hz
	int nMapper;		// index into mapper table
	int nDiskSave;		// 0 never, 1 immediate, 2 on close
	int nAudio;		// 0 HDMI, 1 jack (PWM)
	int nDOS2;		// 0/1
	int nAllSprites;	// 0/1
	int nBIOS;		// 0 auto, 1 original, 2 C-BIOS
	char CartA[MSX_PATH_SIZE];
	char CartB[MSX_PATH_SIZE];
	char DiskA[MSX_PATH_SIZE];
	char DiskB[MSX_PATH_SIZE];
	char Tape[MSX_PATH_SIZE];
};

class CKernel : public CStdlibApp
{
public:
	CKernel (void);
	~CKernel (void);

	bool Initialize (void);
	TShutdownMode Run (void);

	static CKernel *Get (void)	{ return s_pThis; }

	// libretro glue (called from static callbacks)
	bool Environment (unsigned nCmd, void *pData);
	void VideoRefresh (const void *pData, unsigned nWidth, unsigned nHeight, size_t nPitch);
	size_t AudioBatch (const s16 *pData, size_t nFrames);
	s16 InputState (unsigned nPort, unsigned nDevice, unsigned nIndex, unsigned nID);
	void InputPoll (void);

private:
	enum TMainAction { ActionStart, ActionLauncher };
	enum TPauseAction { PauseResume, PauseReset, PauseMainMenu, PauseLauncher };

	void LoadSettings (void);
	void SaveSettings (void);

	TMainAction MainMenu (void);
	void RunEmulation (void);
	TPauseAction PauseMenu (void);

	bool CheckBIOS (void);
	bool StartSound (void);
	void StopSound (void);

	const char *GetVariable (const char *pKey);

private:
	CTimer			mTimer;
	CLogger			mLogger;
	CSerialDevice		mSerial;
	CUSBHCIDevice		mUSBHCI;
	CEMMCDevice		mEMMC;
	FATFS			mFileSystem;
	CConsole		mConsole;
	CCPUThrottle		m_CPUThrottle;

	CVideo			m_Video;
	CInput			m_Input;
	CUserInterface		m_UI;

	CSoundBaseDevice	*m_pSound;
	int			 m_nSoundType;

	TMSXSettings		 m_Settings;
	char			 m_BrowseDir[5][MSX_PATH_SIZE];

	// emulation state
	char			 m_SystemDir[64];	// BIOS folder in use
	bool			 m_bForceMSX1;		// C-BIOS: only MSX1 works with fMSX
	bool			 m_bRunning;
	char			 m_OSDMessage[80];
	unsigned		 m_nOSDFrames;
	boolean			 m_RetroKeys[512];

	static CKernel *s_pThis;
};

#endif
