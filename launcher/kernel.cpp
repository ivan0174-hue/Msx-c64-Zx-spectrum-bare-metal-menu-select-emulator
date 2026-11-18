//
// kernel.cpp - Boot launcher: choose which bare metal kernel to start (chain boot)
//
// The selected kernel image is read from the SD card into memory. Then Circle's
// chain boot copies it to 0x8000 (with MMU and caches off) and jumps to it, exactly
// as if the firmware had loaded it. Power-cycling the Pi brings back this menu.
//
// Part of rpi-retro. License: GPLv3
//
#include "kernel.h"
#include "../common/config.h"
#include <circle/chainboot.h>
#include <circle/memio.h>
#include <circle/bcm2835.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char FromLauncher[] = "launcher";

#define CONFIG_FILE	"/launcher.cfg"
#define LAST_FILE	"/launcher.last"

CKernel::CKernel (void)
:	CStdlibApp ("launcher"),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer),
	mUSBHCI (&mInterrupt, &mTimer, TRUE),
	mEMMC (&mInterrupt, &mTimer, &mActLED),
	mConsole (&mNullDevice, &mNullDevice),
	m_Input (&mUSBHCI),
	m_UI (&m_Video, &m_Input),
	m_nTimeout (10),
	m_nDefault (-1),
	m_nEntries (0)
{
	strcpy (m_Title, "Raspberry Pi 3 - Menu di avvio");
}

CKernel::~CKernel (void)
{
}

bool CKernel::Initialize (void)
{
	if (!CStdlibApp::Initialize ())
	{
		return false;
	}

	if (!mSerial.Initialize (115200))
	{
		return false;
	}

	CDevice *pTarget = mDeviceNameService.GetDevice (mOptions.GetLogDevice (), false);
	if (pTarget == 0)
	{
		pTarget = &mSerial;
	}
	if (!mLogger.Initialize (pTarget))
	{
		return false;
	}

	if (!mTimer.Initialize ())
	{
		return false;
	}

	if (!m_Video.Initialize ())
	{
		return false;
	}

	m_Video.Clear (COL_BG);
	m_Video.DrawTextCentered (CVideo::Height / 2, "Avvio...", COL_TEXT);
	m_Video.Present ();

	if (!mEMMC.Initialize ())
	{
		return false;
	}

	if (f_mount (&mFileSystem, "SD:", 1) != FR_OK)
	{
		mLogger.Write (FromLauncher, LogError, "Cannot mount SD card");
		return false;
	}

	if (!mUSBHCI.Initialize ())
	{
		return false;
	}

	if (!mConsole.Initialize ())
	{
		return false;
	}
	CGlueStdioInit (mConsole);

	return true;
}

static bool FileExists (const char *pPath)
{
	struct stat st;
	return stat (pPath, &st) == 0 && !(st.st_mode & S_IFDIR);
}

void CKernel::LoadConfig (void)
{
	m_nEntries = 0;

	FILE *fp = fopen (CONFIG_FILE, "r");
	if (fp != 0)
	{
		char Line[256];
		TLaunchEntry *pEntry = 0;
		while (fgets (Line, sizeof Line, fp) != 0)
		{
			StrTrim (Line);
			if (Line[0] == 0 || Line[0] == '#' || Line[0] == ';')
			{
				continue;
			}

			if (Line[0] == '[')
			{
				char *pEnd = strchr (Line, ']');
				if (pEnd != 0)
				{
					*pEnd = 0;
				}
				if (m_nEntries < MAX_ENTRIES)
				{
					pEntry = &m_Entries[m_nEntries++];
					memset (pEntry, 0, sizeof *pEntry);
					strncpy (pEntry->Name, Line + 1, sizeof pEntry->Name - 1);
				}
				else
				{
					pEntry = 0;
				}
				continue;
			}

			char *pEq = strchr (Line, '=');
			if (pEq == 0)
			{
				continue;
			}
			*pEq = 0;
			char *pKey = Line;
			char *pValue = pEq + 1;
			StrTrim (pKey);
			StrTrim (pValue);
			// strip trailing comments
			char *pHash = strstr (pValue, " #");
			if (pHash != 0)
			{
				*pHash = 0;
				StrTrim (pValue);
			}

			if (pEntry == 0)
			{
				// global settings
				if (strcasecmp (pKey, "title") == 0)
				{
					strncpy (m_Title, pValue, sizeof m_Title - 1);
				}
				else if (strcasecmp (pKey, "timeout") == 0)
				{
					m_nTimeout = atoi (pValue);
				}
				else if (strcasecmp (pKey, "default") == 0)
				{
					m_nDefault = strcasecmp (pValue, "last") == 0 ? -1 : atoi (pValue) - 1;
				}
			}
			else
			{
				if (strcasecmp (pKey, "kernel") == 0)
				{
					strncpy (pEntry->Kernel, pValue, sizeof pEntry->Kernel - 1);
				}
				else if (strcasecmp (pKey, "info") == 0)
				{
					strncpy (pEntry->Info, pValue, sizeof pEntry->Info - 1);
				}
			}
		}
		fclose (fp);
	}

	if (m_nEntries == 0)
	{
		// built-in defaults
		static const char *Defaults[][2] =
		{
			{ "MSX (fMSX)", "/msx/kernel-msx.img" },
			{ "Commodore 64 (BMC64)", "/bmc64/kernel8-32.img|/bmc64-c64.img" },
			{ "ZX Spectrum (ZXMini)", "/zxmini/kernel8-32.img|/zxmini/kernel7.img" },
		};
		for (unsigned i = 0; i < sizeof Defaults / sizeof Defaults[0]; i++)
		{
			TLaunchEntry *pEntry = &m_Entries[m_nEntries++];
			memset (pEntry, 0, sizeof *pEntry);
			strcpy (pEntry->Name, Defaults[i][0]);
			strcpy (pEntry->Kernel, Defaults[i][1]);
		}
	}

	for (unsigned i = 0; i < m_nEntries; i++)
	{
		TLaunchEntry *pEntry = &m_Entries[i];
		if (   strcasecmp (pEntry->Kernel, "reboot") == 0
		    || strcasecmp (pEntry->Kernel, "halt") == 0)
		{
			pEntry->bAvailable = true;
		}
		else
		{
			// "kernel=a|b|c": use the first file that exists
			char List[sizeof pEntry->Kernel];
			strcpy (List, pEntry->Kernel);
			pEntry->bAvailable = false;
			char *pSave = 0;
			char *pFirst = 0;
			for (char *p = strtok_r (List, "|", &pSave); p != 0; p = strtok_r (0, "|", &pSave))
			{
				StrTrim (p);
				if (pFirst == 0)
				{
					pFirst = p;
				}
				if (*p && FileExists (p))
				{
					strcpy (pEntry->Kernel, p);
					pEntry->bAvailable = true;
					break;
				}
			}
			if (!pEntry->bAvailable && pFirst != 0)
			{
				strcpy (pEntry->Kernel, pFirst);
			}
		}
	}
}

// Stop the USB host controller (DWC OTG), so that it does not keep doing DMA into
// memory that will belong to the next kernel.
void CKernel::ResetUSBController (void)
{
	const uintptr Base = ARM_IO_BASE + 0x980000;
	const uintptr GAHBCFG = Base + 0x008;
	const uintptr GRSTCTL = Base + 0x010;

	write32 (GAHBCFG, read32 (GAHBCFG) & ~1U);	// global interrupt mask off

	// wait for AHB master idle
	for (unsigned i = 0; i < 100000 && !(read32 (GRSTCTL) & (1U << 31)); i++)
	{
		CTimer::SimpleusDelay (1);
	}

	write32 (GRSTCTL, read32 (GRSTCTL) | 1U);	// core soft reset
	for (unsigned i = 0; i < 100000 && (read32 (GRSTCTL) & 1U); i++)
	{
		CTimer::SimpleusDelay (1);
	}
	CTimer::SimpleMsDelay (10);
}

bool CKernel::LoadKernel (const TLaunchEntry &rEntry)
{
	FIL File;
	if (f_open (&File, rEntry.Kernel, FA_READ) != FR_OK)
	{
		return false;
	}

	FSIZE_t nSize = f_size (&File);
	if (nSize == 0 || nSize > 64 * 1024 * 1024)
	{
		f_close (&File);
		return false;
	}

	u8 *pBuffer = (u8 *) malloc (nSize + 64);
	if (pBuffer == 0)
	{
		f_close (&File);
		return false;
	}

	UINT nRead = 0;
	FRESULT Result = f_read (&File, pBuffer, nSize, &nRead);
	f_close (&File);
	if (Result != FR_OK || nRead != nSize)
	{
		free (pBuffer);
		return false;
	}

	mLogger.Write (FromLauncher, LogNotice, "Chain booting %s (%u bytes)", rEntry.Kernel, (unsigned) nSize);

	EnableChainBoot (pBuffer, nSize);

	return true;
}

CStdlibApp::TShutdownMode CKernel::Run (void)
{
	LoadConfig ();

	// remember last choice
	unsigned nSelected = 0;
	{
		CConfigFile Last;
		if (Last.Load (LAST_FILE))
		{
			nSelected = (unsigned) Last.GetInt ("last", 0);
		}
	}
	if (m_nDefault >= 0)
	{
		nSelected = (unsigned) m_nDefault;
	}
	if (nSelected >= m_nEntries)
	{
		nSelected = 0;
	}

	// give USB devices a moment to show up
	for (unsigned i = 0; i < 50; i++)
	{
		m_Input.Update ();
		CTimer::SimpleMsDelay (10);
	}

	TMenuItem *pItems = new TMenuItem[m_nEntries];
	unsigned nTimeout = m_nTimeout;

	while (1)
	{
		for (unsigned i = 0; i < m_nEntries; i++)
		{
			pItems[i].pLabel = m_Entries[i].Name;
			pItems[i].bEnabled = true;
			if (!m_Entries[i].bAvailable)
			{
				snprintf (pItems[i].Value, sizeof pItems[i].Value, "manca %s", m_Entries[i].Kernel);
			}
			else
			{
				strncpy (pItems[i].Value, m_Entries[i].Info, sizeof pItems[i].Value - 1);
				pItems[i].Value[sizeof pItems[i].Value - 1] = 0;
			}
		}

		// no auto boot if the default entry is missing
		if (!m_Entries[nSelected].bAvailable)
		{
			nTimeout = 0;
		}

		TMenuResult Result = m_UI.Menu (m_Title, "Scegli cosa avviare (spegni e riaccendi per tornare qui)",
						pItems, m_nEntries, nSelected,
						"Su/Giu scegli   INVIO avvia", nTimeout);
		nTimeout = 0;

		if (Result != MenuResultSelect && Result != MenuResultTimeout)
		{
			continue;
		}

		const TLaunchEntry &Entry = m_Entries[nSelected];
		if (!Entry.bAvailable)
		{
			m_UI.MessageBox ("File mancante", "Non trovo il kernel:", Entry.Kernel,
					 "Controlla launcher.cfg e i file sulla scheda SD.");
			continue;
		}

		if (strcasecmp (Entry.Kernel, "reboot") == 0)
		{
			return ShutdownReboot;
		}
		if (strcasecmp (Entry.Kernel, "halt") == 0)
		{
			m_Video.Clear (COL_BLACK);
			m_Video.DrawTextCentered (CVideo::Height / 2, "Ora puoi spegnere il Raspberry Pi.", COL_TEXT);
			m_Video.Present ();
			return ShutdownHalt;
		}

		CConfigFile Last;
		Last.SetInt ("last", (int) nSelected);
		Last.Save (LAST_FILE);

		m_Video.Clear (COL_BLACK);
		char Msg[128];
		snprintf (Msg, sizeof Msg, "Avvio di %s ...", Entry.Name);
		m_Video.DrawTextCentered (CVideo::Height / 2, Msg, COL_TEXT);
		m_Video.Present ();

		if (!LoadKernel (Entry))
		{
			m_UI.MessageBox ("Errore", "Impossibile leggere il kernel:", Entry.Kernel);
			continue;
		}

		f_mount (0, "SD:", 0);
		ResetUSBController ();

		return ShutdownReboot;		// sysinit sees chain boot enabled and starts the new kernel
	}
}
