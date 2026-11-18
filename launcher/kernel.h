//
// kernel.h - Boot launcher: choose which bare metal kernel to start (chain boot)
//
// Part of rpi-retro. License: GPLv3
//
#ifndef _launcher_kernel_h
#define _launcher_kernel_h

#include <circle_stdlib_app.h>
#include "../common/video.h"
#include "../common/input.h"
#include "../common/ui.h"

#define MAX_ENTRIES	24

struct TLaunchEntry
{
	char Name[64];
	char Kernel[128];	// path on the SD card, or "reboot" / "halt"
	char Info[96];		// optional description
	bool bAvailable;
};

class CKernel : public CStdlibApp
{
public:
	CKernel (void);
	~CKernel (void);

	bool Initialize (void);
	TShutdownMode Run (void);

private:
	void LoadConfig (void);
	bool LoadKernel (const TLaunchEntry &rEntry);
	static void ResetUSBController (void);

private:
	CTimer			mTimer;
	CLogger			mLogger;
	CSerialDevice		mSerial;
	CUSBHCIDevice		mUSBHCI;
	CEMMCDevice		mEMMC;
	FATFS			mFileSystem;
	CConsole		mConsole;

	CVideo			m_Video;
	CInput			m_Input;
	CUserInterface		m_UI;

	char			m_Title[80];
	unsigned		m_nTimeout;
	int			m_nDefault;	// -1 = last used
	TLaunchEntry		m_Entries[MAX_ENTRIES];
	unsigned		m_nEntries;
};

#endif
