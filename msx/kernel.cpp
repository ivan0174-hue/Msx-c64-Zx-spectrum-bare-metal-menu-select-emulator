//
// kernel.cpp - Bare metal MSX (fMSX) for Raspberry Pi 3: menu + libretro frontend
//
// Part of rpi-retro. License: GPLv3 (frontend); fMSX has its own license, see fmsx/LICENSE
//
#include "kernel.h"
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/timer.h>
#include <circle/util.h>
#include <circle/startup.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "fmsx/libretro.h"

extern "C"
{
	// patched fMSX / fmsx-libretro hooks
	extern int BM_CPUMul;
	extern const char *BM_CartA, *BM_CartB, *BM_DiskA, *BM_DiskB, *BM_Tape;
	unsigned BM_GetFPS (void);
	void BM_FlushDisks (void);
	int BM_ChangeDisk (int drive, const char *path);
	unsigned char ChangeTape (const char *FileName);
	void RewindTape (void);
}

static const char FromMSX[] = "msx";

#define MSX_DIR		"/msx"
#define BIOS_DIR	MSX_DIR "/bios"
#define CBIOS_DIR	BIOS_DIR "/cbios"
#define SETTINGS_FILE	MSX_DIR "/msx.cfg"

#define SAMPLE_RATE	48000
#define AUDIO_CHUNK	(384 * 2)	// HDMI needs multiples of 384 frames

// option tables /////////////////////////////////////////////////////////////

static const char *ModelNames[]  = { "MSX1", "MSX2", "MSX2+" };
static const char *ModelValues[] = { "MSX1", "MSX2", "MSX2+" };

static const char *RAMNames[]  = { "Auto (MSX1 64K / MSX2 128K / 2+ 256K)", "64 KB", "128 KB", "256 KB",
				   "512 KB", "1 MB", "2 MB", "4 MB" };
static const char *RAMValues[] = { "Auto", "64KB", "128KB", "256KB", "512KB", "1MB", "2MB", "4MB" };

static const char *VRAMNames[]  = { "Auto (MSX1 16K / MSX2 128K)", "32 KB", "64 KB", "128 KB", "192 KB" };
static const char *VRAMValues[] = { "Auto", "32KB", "64KB", "128KB", "192KB" };

static const char *CPUNames[] = { "3.58 MHz (normale)", "7.16 MHz (x2)", "10.74 MHz (x3)",
				  "14.32 MHz (x4)", "21.48 MHz (x6)", "28.64 MHz (x8)" };
static const int   CPUMul[]   = { 1, 2, 3, 4, 6, 8 };

static const char *VideoNames[]  = { "NTSC 60 Hz (Giappone/USA)", "PAL 50 Hz (Europa)" };
static const char *VideoValues[] = { "NTSC", "PAL" };

static const char *MapperNames[]  = { "Automatico", "Generic 8 KB", "Generic 16 KB", "Konami SCC (Konami5)",
				      "Konami (Konami4)", "ASCII 8 KB", "ASCII 16 KB" };
static const char *MapperValues[] = { "Guess", "Generic 8kB", "Generic 16kB", "Konami5 8kB", "Konami4 8kB",
				      "ASCII 8kB", "ASCII 16kB" };

static const char *DiskSaveNames[]  = { "Mai (sola lettura)", "Immediato", "All'uscita" };
static const char *DiskSaveValues[] = { "Never", "Immediate", "On close" };

static const char *AudioNames[] = { "HDMI", "Jack 3.5 mm (PWM)" };
static const char *YesNo[]      = { "No", "Si" };
static const char *BIOSNames[]  = { "Auto (originale, se no C-BIOS)", "Originale (" BIOS_DIR ")", "C-BIOS libero (MSX1, solo cartucce)" };

#define COUNT(a)	(int) (sizeof (a) / sizeof (a)[0])

// HID usage -> RETROK ////////////////////////////////////////////////////////

static unsigned short HIDToRetro[256];

static void InitKeyMap (void)
{
	memset (HIDToRetro, 0, sizeof HIDToRetro);
	for (int i = 0; i < 26; i++) HIDToRetro[0x04 + i] = RETROK_a + i;
	for (int i = 0; i < 9; i++)  HIDToRetro[0x1E + i] = RETROK_1 + i;
	HIDToRetro[0x27] = RETROK_0;
	HIDToRetro[0x28] = RETROK_RETURN;
	HIDToRetro[0x29] = RETROK_ESCAPE;
	HIDToRetro[0x2A] = RETROK_BACKSPACE;
	HIDToRetro[0x2B] = RETROK_TAB;
	HIDToRetro[0x2C] = RETROK_SPACE;
	HIDToRetro[0x2D] = RETROK_MINUS;
	HIDToRetro[0x2E] = RETROK_EQUALS;
	HIDToRetro[0x2F] = RETROK_LEFTBRACKET;
	HIDToRetro[0x30] = RETROK_RIGHTBRACKET;
	HIDToRetro[0x31] = RETROK_BACKSLASH;
	HIDToRetro[0x32] = RETROK_BACKSLASH;
	HIDToRetro[0x33] = RETROK_SEMICOLON;
	HIDToRetro[0x34] = RETROK_QUOTE;
	HIDToRetro[0x35] = RETROK_BACKQUOTE;
	HIDToRetro[0x36] = RETROK_COMMA;
	HIDToRetro[0x37] = RETROK_PERIOD;
	HIDToRetro[0x38] = RETROK_SLASH;
	HIDToRetro[0x39] = RETROK_CAPSLOCK;
	for (int i = 0; i < 5; i++) HIDToRetro[0x3A + i] = RETROK_F1 + i;	// F1..F5
	HIDToRetro[0x3F] = RETROK_END;		// F6  -> SELECT
	HIDToRetro[0x40] = RETROK_PAUSE;	// F7  -> STOP
	HIDToRetro[0x48] = RETROK_PAUSE;	// Pause -> STOP
	HIDToRetro[0x49] = RETROK_INSERT;
	HIDToRetro[0x4A] = RETROK_HOME;
	HIDToRetro[0x4B] = RETROK_PAGEUP;	// DEAD key
	HIDToRetro[0x4C] = RETROK_DELETE;
	HIDToRetro[0x4D] = RETROK_END;		// SELECT
	HIDToRetro[0x4E] = RETROK_PAGEDOWN;	// CODE / country
	HIDToRetro[0x4F] = RETROK_RIGHT;
	HIDToRetro[0x50] = RETROK_LEFT;
	HIDToRetro[0x51] = RETROK_DOWN;
	HIDToRetro[0x52] = RETROK_UP;
	HIDToRetro[0x54] = RETROK_KP_DIVIDE;
	HIDToRetro[0x55] = RETROK_KP_MULTIPLY;
	HIDToRetro[0x56] = RETROK_KP_MINUS;
	HIDToRetro[0x57] = RETROK_KP_PLUS;
	HIDToRetro[0x58] = RETROK_KP_ENTER;
	for (int i = 0; i < 9; i++) HIDToRetro[0x59 + i] = RETROK_KP1 + i;
	HIDToRetro[0x62] = RETROK_KP0;
	HIDToRetro[0x63] = RETROK_KP_PERIOD;
	HIDToRetro[0x64] = RETROK_BACKSLASH;	// ISO key (< > on Italian keyboards)
	HIDToRetro[0xE0] = RETROK_LCTRL;
	HIDToRetro[0xE1] = RETROK_LSHIFT;
	HIDToRetro[0xE2] = RETROK_LALT;		// GRAPH
	HIDToRetro[0xE4] = RETROK_RCTRL;
	HIDToRetro[0xE5] = RETROK_RSHIFT;
	HIDToRetro[0xE6] = RETROK_PAGEDOWN;	// AltGr -> CODE
}

// static libretro callbacks ///////////////////////////////////////////////////

static bool EnvironmentCB (unsigned nCmd, void *pData)
{
	return CKernel::Get ()->Environment (nCmd, pData);
}

static void VideoRefreshCB (const void *pData, unsigned nWidth, unsigned nHeight, size_t nPitch)
{
	CKernel::Get ()->VideoRefresh (pData, nWidth, nHeight, nPitch);
}

static size_t AudioBatchCB (const int16_t *pData, size_t nFrames)
{
	return CKernel::Get ()->AudioBatch (pData, nFrames);
}

static void AudioSampleCB (int16_t nLeft, int16_t nRight)
{
}

static void InputPollCB (void)
{
	CKernel::Get ()->InputPoll ();
}

static int16_t InputStateCB (unsigned nPort, unsigned nDevice, unsigned nIndex, unsigned nID)
{
	return CKernel::Get ()->InputState (nPort, nDevice, nIndex, nID);
}

static void LogCB (enum retro_log_level Level, const char *pFormat, ...)
{
	char Buf[256];
	va_list Args;
	va_start (Args, pFormat);
	vsnprintf (Buf, sizeof Buf, pFormat, Args);
	va_end (Args);
	size_t n = strlen (Buf);
	while (n > 0 && (Buf[n - 1] == '\n' || Buf[n - 1] == '\r'))
	{
		Buf[--n] = 0;
	}
	CLogger::Get ()->Write ("fmsx", Level >= RETRO_LOG_WARN ? LogWarning : LogDebug, "%s", Buf);
}

// CKernel //////////////////////////////////////////////////////////////////////

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel (void)
:	CStdlibApp ("msx"),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer),
	mUSBHCI (&mInterrupt, &mTimer, TRUE),
	mEMMC (&mInterrupt, &mTimer, &mActLED),
	mConsole (&mNullDevice, &mNullDevice),
	m_CPUThrottle (CPUSpeedMaximum),
	m_Input (&mUSBHCI),
	m_UI (&m_Video, &m_Input),
	m_pSound (0),
	m_nSoundType (-1),
	m_bRunning (false),
	m_nOSDFrames (0)
{
	s_pThis = this;
	m_OSDMessage[0] = 0;
	strcpy (m_SystemDir, BIOS_DIR);
	m_bForceMSX1 = false;
	memset (m_RetroKeys, 0, sizeof m_RetroKeys);
	memset (&m_Settings, 0, sizeof m_Settings);
	strcpy (m_BrowseDir[0], MSX_DIR "/roms");
	strcpy (m_BrowseDir[1], MSX_DIR "/roms");
	strcpy (m_BrowseDir[2], MSX_DIR "/disks");
	strcpy (m_BrowseDir[3], MSX_DIR "/disks");
	strcpy (m_BrowseDir[4], MSX_DIR "/tapes");
}

CKernel::~CKernel (void)
{
	s_pThis = 0;
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

	if (!mEMMC.Initialize ())
	{
		return false;
	}

	if (f_mount (&mFileSystem, "SD:", 1) != FR_OK)
	{
		mLogger.Write (FromMSX, LogError, "Cannot mount SD card");
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

	InitKeyMap ();

	return true;
}

// settings /////////////////////////////////////////////////////////////////

static int Clamp (int n, int nMax)
{
	return n < 0 ? 0 : (n >= nMax ? nMax - 1 : n);
}

void CKernel::LoadSettings (void)
{
	CConfigFile Cfg;
	Cfg.Load (SETTINGS_FILE);

	m_Settings.nModel      = Clamp (Cfg.GetInt ("model", 1), COUNT (ModelNames));
	m_Settings.nRAM        = Clamp (Cfg.GetInt ("ram", 0), COUNT (RAMNames));
	m_Settings.nVRAM       = Clamp (Cfg.GetInt ("vram", 0), COUNT (VRAMNames));
	m_Settings.nCPU        = Clamp (Cfg.GetInt ("cpu", 0), COUNT (CPUNames));
	m_Settings.nVideo      = Clamp (Cfg.GetInt ("video", 1), COUNT (VideoNames));
	m_Settings.nMapper     = Clamp (Cfg.GetInt ("mapper", 0), COUNT (MapperNames));
	m_Settings.nDiskSave   = Clamp (Cfg.GetInt ("disksave", 2), COUNT (DiskSaveNames));
	m_Settings.nAudio      = Clamp (Cfg.GetInt ("audio", 0), COUNT (AudioNames));
	m_Settings.nDOS2       = Clamp (Cfg.GetInt ("dos2", 0), 2);
	m_Settings.nAllSprites = Clamp (Cfg.GetInt ("allsprites", 0), 2);
	m_Settings.nBIOS       = Clamp (Cfg.GetInt ("bios", 0), COUNT (BIOSNames));

	strncpy (m_Settings.CartA, Cfg.Get ("carta"), MSX_PATH_SIZE - 1);
	strncpy (m_Settings.CartB, Cfg.Get ("cartb"), MSX_PATH_SIZE - 1);
	strncpy (m_Settings.DiskA, Cfg.Get ("diska"), MSX_PATH_SIZE - 1);
	strncpy (m_Settings.DiskB, Cfg.Get ("diskb"), MSX_PATH_SIZE - 1);
	strncpy (m_Settings.Tape,  Cfg.Get ("tape"),  MSX_PATH_SIZE - 1);

	const char *Keys[5] = { "dir_carta", "dir_cartb", "dir_diska", "dir_diskb", "dir_tape" };
	for (int i = 0; i < 5; i++)
	{
		const char *p = Cfg.Get (Keys[i], 0);
		if (p != 0 && *p)
		{
			strncpy (m_BrowseDir[i], p, MSX_PATH_SIZE - 1);
		}
	}
}

void CKernel::SaveSettings (void)
{
	CConfigFile Cfg;
	Cfg.SetInt ("model", m_Settings.nModel);
	Cfg.SetInt ("ram", m_Settings.nRAM);
	Cfg.SetInt ("vram", m_Settings.nVRAM);
	Cfg.SetInt ("cpu", m_Settings.nCPU);
	Cfg.SetInt ("video", m_Settings.nVideo);
	Cfg.SetInt ("mapper", m_Settings.nMapper);
	Cfg.SetInt ("disksave", m_Settings.nDiskSave);
	Cfg.SetInt ("audio", m_Settings.nAudio);
	Cfg.SetInt ("dos2", m_Settings.nDOS2);
	Cfg.SetInt ("allsprites", m_Settings.nAllSprites);
	Cfg.SetInt ("bios", m_Settings.nBIOS);
	Cfg.Set ("carta", m_Settings.CartA);
	Cfg.Set ("cartb", m_Settings.CartB);
	Cfg.Set ("diska", m_Settings.DiskA);
	Cfg.Set ("diskb", m_Settings.DiskB);
	Cfg.Set ("tape", m_Settings.Tape);
	const char *Keys[5] = { "dir_carta", "dir_cartb", "dir_diska", "dir_diskb", "dir_tape" };
	for (int i = 0; i < 5; i++)
	{
		Cfg.Set (Keys[i], m_BrowseDir[i]);
	}

	Cfg.Save (SETTINGS_FILE, "# Impostazioni MSX (scritte automaticamente dal menu)\n");
}

// main menu ////////////////////////////////////////////////////////////////

enum
{
	ItemStart,
	ItemSep1,
	ItemModel,
	ItemRAM,
	ItemVRAM,
	ItemCPU,
	ItemVideo,
	ItemBIOS,
	ItemSep2,
	ItemCartA,
	ItemCartB,
	ItemDiskA,
	ItemDiskB,
	ItemTape,
	ItemMapper,
	ItemSep3,
	ItemDiskSave,
	ItemDOS2,
	ItemAllSprites,
	ItemAudio,
	ItemSep4,
	ItemLauncher,
	ItemCount
};

static void SetMediaValue (char *pValue, const char *pPath)
{
	if (pPath[0] == 0)
	{
		strcpy (pValue, "(vuoto)");
	}
	else
	{
		strncpy (pValue, PathBaseName (pPath), 95);
		pValue[95] = 0;
	}
}

CKernel::TMainAction CKernel::MainMenu (void)
{
	static unsigned nSelected = ItemStart;

	TMenuItem Items[ItemCount];
	memset (Items, 0, sizeof Items);
	const char *Labels[ItemCount] =
	{
		">>> AVVIA MSX <<<", 0,
		"Modello", "Memoria RAM", "Memoria video (VRAM)", "Velocita' CPU (Z80)", "Video", "BIOS", 0,
		"Cartuccia slot 1 (ROM)", "Cartuccia slot 2 (ROM)", "Floppy A (DSK)", "Floppy B (DSK)",
		"Cassetta (CAS)", "Tipo mapper cartuccia", 0,
		"Salva modifiche ai floppy", "MSX-DOS 2", "Tutti gli sprite (no flicker)", "Uscita audio", 0,
		"<< Torna al menu di avvio"
	};

	while (1)
	{
		for (unsigned i = 0; i < ItemCount; i++)
		{
			Items[i].pLabel = Labels[i];
			Items[i].bEnabled = true;
			Items[i].Value[0] = 0;
		}

		TMSXSettings &S = m_Settings;
		strcpy (Items[ItemModel].Value, ModelNames[S.nModel]);
		strcpy (Items[ItemRAM].Value, RAMNames[S.nRAM]);
		strcpy (Items[ItemVRAM].Value, VRAMNames[S.nVRAM]);
		strcpy (Items[ItemCPU].Value, CPUNames[S.nCPU]);
		strcpy (Items[ItemVideo].Value, VideoNames[S.nVideo]);
		strcpy (Items[ItemBIOS].Value, BIOSNames[S.nBIOS]);
		SetMediaValue (Items[ItemCartA].Value, S.CartA);
		SetMediaValue (Items[ItemCartB].Value, S.CartB);
		SetMediaValue (Items[ItemDiskA].Value, S.DiskA);
		SetMediaValue (Items[ItemDiskB].Value, S.DiskB);
		SetMediaValue (Items[ItemTape].Value, S.Tape);
		strcpy (Items[ItemMapper].Value, MapperNames[S.nMapper]);
		strcpy (Items[ItemDiskSave].Value, DiskSaveNames[S.nDiskSave]);
		strcpy (Items[ItemDOS2].Value, YesNo[S.nDOS2]);
		strcpy (Items[ItemAllSprites].Value, YesNo[S.nAllSprites]);
		strcpy (Items[ItemAudio].Value, AudioNames[S.nAudio]);
		Items[ItemDOS2].bEnabled = S.nModel != 0;

		char SubTitle[96];
		snprintf (SubTitle, sizeof SubTitle, "Tastiera: %s   Joypad: %u   BIOS: " BIOS_DIR,
			  m_Input.HasKeyboard () ? "si" : "NO", m_Input.GetNumPads ());

		TMenuResult Result = m_UI.Menu ("fMSX bare metal - Raspberry Pi 3", SubTitle,
						Items, ItemCount, nSelected,
						"Su/Giu scegli  Sx/Dx cambia  INVIO apri/avvia  CANC svuota");

		int nDelta = Result == MenuResultLeft ? -1 : (Result == MenuResultRight ? 1 : 0);
		if (Result == MenuResultSelect)
		{
			nDelta = 1;
		}

		auto Cycle = [nDelta] (int &rValue, int nCount)
		{
			rValue = (rValue + nDelta + nCount) % nCount;
		};

		if (Result == MenuResultBack)
		{
			continue;
		}

		int nMedia = -1;
		char *pMedia = 0;
		const char *pExt = 0;
		const char *pTitle = 0;
		switch (nSelected)
		{
		case ItemCartA: nMedia = 0; pMedia = S.CartA; pExt = ".rom.mx1.mx2.ri"; pTitle = "Scegli cartuccia slot 1"; break;
		case ItemCartB: nMedia = 1; pMedia = S.CartB; pExt = ".rom.mx1.mx2.ri"; pTitle = "Scegli cartuccia slot 2"; break;
		case ItemDiskA: nMedia = 2; pMedia = S.DiskA; pExt = ".dsk.fdi"; pTitle = "Scegli floppy A"; break;
		case ItemDiskB: nMedia = 3; pMedia = S.DiskB; pExt = ".dsk.fdi"; pTitle = "Scegli floppy B"; break;
		case ItemTape:  nMedia = 4; pMedia = S.Tape;  pExt = ".cas"; pTitle = "Scegli cassetta"; break;
		default: break;
		}

		if (nMedia >= 0)
		{
			if (Result == MenuResultDelete || Result == MenuResultLeft)
			{
				pMedia[0] = 0;
			}
			else if (Result == MenuResultSelect || Result == MenuResultRight)
			{
				char Path[MSX_PATH_SIZE];
				if (m_UI.BrowseFile (pTitle, m_BrowseDir[nMedia], MSX_PATH_SIZE, pExt,
						     Path, sizeof Path))
				{
					strcpy (pMedia, Path);
				}
			}
			continue;
		}

		if (Result == MenuResultDelete)
		{
			continue;
		}

		switch (nSelected)
		{
		case ItemStart:
			if (Result == MenuResultSelect)
			{
				SaveSettings ();
				return ActionStart;
			}
			break;

		case ItemLauncher:
			if (Result == MenuResultSelect)
			{
				SaveSettings ();
				return ActionLauncher;
			}
			break;

		case ItemModel:		Cycle (S.nModel, COUNT (ModelNames));	break;
		case ItemRAM:		Cycle (S.nRAM, COUNT (RAMNames));	break;
		case ItemVRAM:		Cycle (S.nVRAM, COUNT (VRAMNames));	break;
		case ItemCPU:		Cycle (S.nCPU, COUNT (CPUNames));	break;
		case ItemVideo:		Cycle (S.nVideo, COUNT (VideoNames));	break;
		case ItemBIOS:		Cycle (S.nBIOS, COUNT (BIOSNames));	break;
		case ItemMapper:	Cycle (S.nMapper, COUNT (MapperNames));	break;
		case ItemDiskSave:	Cycle (S.nDiskSave, COUNT (DiskSaveNames)); break;
		case ItemDOS2:		Cycle (S.nDOS2, 2);			break;
		case ItemAllSprites:	Cycle (S.nAllSprites, 2);		break;
		case ItemAudio:		Cycle (S.nAudio, COUNT (AudioNames));	break;
		default:
			break;
		}
	}
}

// BIOS check ///////////////////////////////////////////////////////////////

static bool FileExists (const char *pPath)
{
	struct stat st;
	return stat (pPath, &st) == 0;
}

static bool HasBIOS (const char *pDir, int nModel)
{
	static const char *Needed[3][2] =
	{
		{ "MSX.ROM", 0 },
		{ "MSX2.ROM", "MSX2EXT.ROM" },
		{ "MSX2P.ROM", "MSX2PEXT.ROM" }
	};

	for (int i = 0; i < 2; i++)
	{
		const char *pName = Needed[nModel][i];
		if (pName == 0)
		{
			continue;
		}
		char Path[96];
		snprintf (Path, sizeof Path, "%s/%s", pDir, pName);
		if (!FileExists (Path))
		{
			return false;
		}
	}
	return true;
}

bool CKernel::CheckBIOS (void)
{
	m_bForceMSX1 = false;
	bool bOriginal = HasBIOS (BIOS_DIR, m_Settings.nModel);
	bool bCBIOS = HasBIOS (CBIOS_DIR, 0);	// C-BIOS is used as MSX1 only

	if (   m_Settings.nBIOS == 1
	    || (m_Settings.nBIOS == 0 && bOriginal))
	{
		if (!bOriginal)
		{
			m_UI.MessageBox ("BIOS originale mancante",
					 "Copia i file BIOS (es. MSX2.ROM, MSX2EXT.ROM, DISK.ROM)",
					 "nella cartella " BIOS_DIR " della scheda SD,",
					 "oppure scegli BIOS = C-BIOS nel menu.");
			return false;
		}
		strcpy (m_SystemDir, BIOS_DIR);
	}
	else
	{
		if (!bCBIOS)
		{
			m_UI.MessageBox ("BIOS mancante",
					 "Non trovo ne' il BIOS originale ne' C-BIOS.",
					 "Copia i file BIOS in " BIOS_DIR " (vedi LEGGIMI).");
			return false;
		}
		strcpy (m_SystemDir, CBIOS_DIR);
		m_bForceMSX1 = true;
		if (   m_Settings.nModel != 0
		    || m_Settings.DiskA[0] || m_Settings.DiskB[0] || m_Settings.Tape[0])
		{
			m_UI.MessageBox ("Avviso: uso C-BIOS (BIOS originale assente)",
					 "Con C-BIOS l'MSX parte come MSX1, senza BASIC ne' floppy:",
					 "funzionano solo le cartucce ROM MSX1.",
					 "Per MSX2/2+, DSK e CAS copia il BIOS originale in " BIOS_DIR ".");
		}
		return true;
	}

	if (   (m_Settings.DiskA[0] || m_Settings.DiskB[0])
	    && !FileExists (BIOS_DIR "/DISK.ROM"))
	{
		m_UI.MessageBox ("DISK.ROM mancante",
				 "Per usare i floppy serve " BIOS_DIR "/DISK.ROM",
				 "L'MSX partira' senza unita' floppy.");
	}

	return true;
}

// sound ////////////////////////////////////////////////////////////////////

bool CKernel::StartSound (void)
{
	if (m_pSound != 0 && m_nSoundType == m_Settings.nAudio)
	{
		return true;
	}

	StopSound ();

	if (m_Settings.nAudio == 0)
	{
		m_pSound = new CHDMISoundBaseDevice (&mInterrupt, SAMPLE_RATE, AUDIO_CHUNK);
	}
	else
	{
		m_pSound = new CPWMSoundBaseDevice (&mInterrupt, SAMPLE_RATE, 512);
	}

	if (   m_pSound == 0
	    || !m_pSound->AllocateQueue (100))
	{
		mLogger.Write (FromMSX, LogWarning, "Cannot allocate sound queue");
		delete m_pSound;
		m_pSound = 0;
		return false;
	}

	m_pSound->SetWriteFormat (SoundFormatSigned16, 2);

	if (!m_pSound->Start ())
	{
		mLogger.Write (FromMSX, LogWarning, "Cannot start sound device");
		delete m_pSound;
		m_pSound = 0;
		return false;
	}

	m_nSoundType = m_Settings.nAudio;
	return true;
}

void CKernel::StopSound (void)
{
	if (m_pSound != 0)
	{
		m_pSound->Cancel ();
		while (m_pSound->IsActive ())
		{
			CTimer::SimpleMsDelay (1);
		}
		delete m_pSound;
		m_pSound = 0;
	}
	m_nSoundType = -1;
}

// libretro glue ////////////////////////////////////////////////////////////

const char *CKernel::GetVariable (const char *pKey)
{
	const TMSXSettings &S = m_Settings;

	if (strcmp (pKey, "fmsx_mode") == 0)		return ModelValues[m_bForceMSX1 ? 0 : S.nModel];
	if (strcmp (pKey, "fmsx_video_mode") == 0)	return VideoValues[S.nVideo];
	if (strcmp (pKey, "fmsx_mapper_type_mode") == 0) return MapperValues[S.nMapper];
	if (strcmp (pKey, "fmsx_ram_pages") == 0)	return RAMValues[S.nRAM];
	if (strcmp (pKey, "fmsx_vram_pages") == 0)	return VRAMValues[S.nVRAM];
	if (strcmp (pKey, "fmsx_flush_disk") == 0)	return DiskSaveValues[S.nDiskSave];
	if (strcmp (pKey, "fmsx_dos2") == 0)		return S.nDOS2 ? "Yes" : "No";
	if (strcmp (pKey, "fmsx_allsprites") == 0)	return S.nAllSprites ? "Yes" : "No";
	if (strcmp (pKey, "fmsx_hires") == 0)		return "Off";
	if (strcmp (pKey, "fmsx_overscan") == 0)	return "No";
	if (strcmp (pKey, "fmsx_ym2413_core") == 0)	return "fMSX";
	if (strcmp (pKey, "fmsx_autospace") == 0)	return "No";
	if (strcmp (pKey, "fmsx_simbdos") == 0)		return "No";
	if (strcmp (pKey, "fmsx_phantom_disk") == 0)	return "No";
	if (strcmp (pKey, "fmsx_game_master") == 0)	return "No";

	return 0;
}

bool CKernel::Environment (unsigned nCmd, void *pData)
{
	switch (nCmd)
	{
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		*(const char **) pData = m_SystemDir;
		return true;

	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return *(const enum retro_pixel_format *) pData == RETRO_PIXEL_FORMAT_RGB565;

	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable *pVar = (struct retro_variable *) pData;
		pVar->value = GetVariable (pVar->key);
		return pVar->value != 0;
		}

	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
		*(bool *) pData = false;
		return true;

	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
		return true;

	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback *) pData)->log = LogCB;
		return true;

	case RETRO_ENVIRONMENT_SET_MESSAGE: {
		const struct retro_message *pMsg = (const struct retro_message *) pData;
		strncpy (m_OSDMessage, pMsg->msg, sizeof m_OSDMessage - 1);
		m_OSDMessage[sizeof m_OSDMessage - 1] = 0;
		m_nOSDFrames = pMsg->frames;
		return true;
		}

	default:
		return false;
	}
}

void CKernel::VideoRefresh (const void *pData, unsigned nWidth, unsigned nHeight, size_t nPitch)
{
	if (pData == 0)
	{
		return;
	}

	m_Video.BlitScaled ((const u16 *) pData, nWidth, nHeight, nPitch);

	if (m_nOSDFrames > 0)
	{
		m_nOSDFrames--;
		m_Video.FillRect (0, CVideo::Height - 20, CVideo::Width, 20, COL_BLACK);
		m_Video.DrawText (8, CVideo::Height - 18, m_OSDMessage, COL_TITLE);
		if (m_nOSDFrames == 0)
		{
			m_Video.FillRect (0, CVideo::Height - 20, CVideo::Width, 20, COL_BLACK);
		}
	}

	m_Video.Present ();
}

size_t CKernel::AudioBatch (const s16 *pData, size_t nFrames)
{
	if (m_pSound != 0)
	{
		m_pSound->Write (pData, nFrames * 2 * sizeof (s16));
	}
	return nFrames;
}

void CKernel::InputPoll (void)
{
	memset (m_RetroKeys, 0, sizeof m_RetroKeys);
	const volatile boolean *pKeys = m_Input.GetKeyArray ();
	for (unsigned i = 0; i < 256; i++)
	{
		if (pKeys[i] && HIDToRetro[i] != 0 && HIDToRetro[i] < 512)
		{
			m_RetroKeys[HIDToRetro[i]] = TRUE;
		}
	}
}

s16 CKernel::InputState (unsigned nPort, unsigned nDevice, unsigned nIndex, unsigned nID)
{
	if (nDevice == RETRO_DEVICE_KEYBOARD)
	{
		return nID < 512 && m_RetroKeys[nID] ? 1 : 0;
	}

	if (nDevice == RETRO_DEVICE_JOYPAD && nPort < MAX_PADS)
	{
		unsigned nPad = m_Input.GetPad (nPort);
		unsigned nMask = 0;
		if (nPad & PAD_UP)	nMask |= 1 << RETRO_DEVICE_ID_JOYPAD_UP;
		if (nPad & PAD_DOWN)	nMask |= 1 << RETRO_DEVICE_ID_JOYPAD_DOWN;
		if (nPad & PAD_LEFT)	nMask |= 1 << RETRO_DEVICE_ID_JOYPAD_LEFT;
		if (nPad & PAD_RIGHT)	nMask |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT;
		if (nPad & (PAD_A | PAD_Y))	nMask |= 1 << RETRO_DEVICE_ID_JOYPAD_A;	// fire A
		if (nPad & (PAD_B | PAD_X))	nMask |= 1 << RETRO_DEVICE_ID_JOYPAD_B;	// fire B

		if (nID == RETRO_DEVICE_ID_JOYPAD_MASK)
		{
			return (s16) nMask;
		}
		return nID < 16 && (nMask & (1 << nID)) ? 1 : 0;
	}

	return 0;
}

// emulation ////////////////////////////////////////////////////////////////

void CKernel::RunEmulation (void)
{
	if (!CheckBIOS ())
	{
		return;
	}

	m_Video.Clear (COL_BLACK);
	m_Video.DrawTextCentered (CVideo::Height / 2, "Avvio MSX...", COL_TEXT);
	m_Video.Present ();

	bool bSound = StartSound ();

	BM_CPUMul = CPUMul[m_Settings.nCPU];
	BM_CartA = m_Settings.CartA;
	BM_CartB = m_Settings.CartB;
	BM_DiskA = m_Settings.DiskA;
	BM_DiskB = m_Settings.DiskB;
	BM_Tape  = m_Settings.Tape;

	retro_set_environment (EnvironmentCB);
	retro_set_video_refresh (VideoRefreshCB);
	retro_set_audio_sample (AudioSampleCB);
	retro_set_audio_sample_batch (AudioBatchCB);
	retro_set_input_poll (InputPollCB);
	retro_set_input_state (InputStateCB);
	retro_init ();
	retro_set_controller_port_device (0, RETRO_DEVICE_KEYBOARD);
	retro_set_controller_port_device (1, RETRO_DEVICE_JOYPAD);

	if (!retro_load_game (0))
	{
		retro_deinit ();
		m_UI.MessageBox ("Errore", "Impossibile avviare l'emulatore MSX");
		return;
	}

	m_Video.Clear (COL_BLACK);
	m_Input.FlushEvents ();
	m_bRunning = true;

	unsigned nNextFrame = CTimer::GetClockTicks ();
	const unsigned nTargetFrames = SAMPLE_RATE * 50 / 1000;	// keep ~50 ms of audio queued

	while (m_bRunning)
	{
		m_Input.Update ();

		if (m_Input.MenuRequested ())
		{
			if (m_pSound != 0)
			{
				m_pSound->Flush ();
			}

			m_Input.FlushEvents ();		// drop keys typed while emulating (and F12 itself)
			TPauseAction Action = PauseMenu ();
			m_Video.Clear (COL_BLACK);
			m_Input.FlushEvents ();
			nNextFrame = CTimer::GetClockTicks ();

			switch (Action)
			{
			case PauseResume:
				break;

			case PauseReset:
				retro_reset ();
				break;

			case PauseMainMenu:
				m_bRunning = false;
				break;

			case PauseLauncher:
				BM_FlushDisks ();
				retro_unload_game ();
				retro_deinit ();
				SaveSettings ();
				StopSound ();
				reboot ();
				break;
			}

			if (!m_bRunning)
			{
				break;
			}
		}

		// pacing: frame timer, slightly corrected by the audio queue level
		unsigned nFPS = BM_GetFPS ();
		unsigned nPeriod = 1000000 / nFPS;
		if (bSound && m_pSound != 0)
		{
			unsigned nAvail = m_pSound->GetQueueFramesAvail ();
			if (nAvail > nTargetFrames + SAMPLE_RATE / 100)		// > +10 ms
			{
				nPeriod += nPeriod / 50;
			}
			else if (nAvail < nTargetFrames - SAMPLE_RATE / 100)	// < -10 ms
			{
				nPeriod -= nPeriod / 50;
			}
		}

		while ((int) (CTimer::GetClockTicks () - nNextFrame) < 0)
		{
			// busy wait (timer resolution 1 us)
		}
		nNextFrame += nPeriod;

		// don't try to catch up after long stalls
		if ((int) (CTimer::GetClockTicks () - nNextFrame) > 100000)
		{
			nNextFrame = CTimer::GetClockTicks () + nPeriod;
		}

		retro_run ();

		// keep the SoC temperature under control (every ~3 s)
		static unsigned s_nThrottleCount = 0;
		if (++s_nThrottleCount >= 150)
		{
			s_nThrottleCount = 0;
			m_CPUThrottle.Update ();
		}
	}

	BM_FlushDisks ();
	retro_unload_game ();
	retro_deinit ();
}

CKernel::TPauseAction CKernel::PauseMenu (void)
{
	enum
	{
		PItemResume, PItemReset, PItemSep1,
		PItemDiskA, PItemDiskB, PItemTape, PItemRewind, PItemSep2,
		PItemMenu, PItemLauncher, PItemCount
	};

	const char *Labels[PItemCount] =
	{
		"Riprendi", "Reset MSX", 0,
		"Cambia floppy A", "Cambia floppy B", "Cambia cassetta", "Riavvolgi cassetta", 0,
		"Torna al menu MSX (configurazione)", "Torna al menu di avvio (riavvia)"
	};

	unsigned nSelected = 0;
	while (1)
	{
		TMenuItem Items[PItemCount];
		memset (Items, 0, sizeof Items);
		for (unsigned i = 0; i < PItemCount; i++)
		{
			Items[i].pLabel = Labels[i];
			Items[i].bEnabled = true;
		}
		SetMediaValue (Items[PItemDiskA].Value, m_Settings.DiskA);
		SetMediaValue (Items[PItemDiskB].Value, m_Settings.DiskB);
		SetMediaValue (Items[PItemTape].Value, m_Settings.Tape);

		TMenuResult Result = m_UI.Menu ("Pausa", "F12 / HOME / SELECT+START = questo menu",
						Items, PItemCount, nSelected,
						"INVIO conferma   ESC riprendi");
		if (Result == MenuResultBack)
		{
			return PauseResume;
		}
		if (Result != MenuResultSelect)
		{
			continue;
		}

		switch (nSelected)
		{
		case PItemResume:	return PauseResume;
		case PItemReset:	return PauseReset;
		case PItemMenu:		return PauseMainMenu;
		case PItemLauncher:	return PauseLauncher;

		case PItemDiskA:
		case PItemDiskB: {
			int nDrive = nSelected == PItemDiskA ? 0 : 1;
			char *pDisk = nDrive ? m_Settings.DiskB : m_Settings.DiskA;
			char Path[MSX_PATH_SIZE];
			if (m_UI.BrowseFile (nDrive ? "Inserisci floppy B" : "Inserisci floppy A",
					     m_BrowseDir[2 + nDrive], MSX_PATH_SIZE, ".dsk.fdi", Path, sizeof Path))
			{
				strcpy (pDisk, Path);
				BM_ChangeDisk (nDrive, pDisk);
				SaveSettings ();
			}
			} break;

		case PItemTape: {
			char Path[MSX_PATH_SIZE];
			if (m_UI.BrowseFile ("Inserisci cassetta", m_BrowseDir[4], MSX_PATH_SIZE, ".cas",
					     Path, sizeof Path))
			{
				strcpy (m_Settings.Tape, Path);
				ChangeTape (Path[0] ? m_Settings.Tape : 0);
				SaveSettings ();
			}
			} break;

		case PItemRewind:
			RewindTape ();
			return PauseResume;

		default:
			break;
		}
	}
}

// main loop ////////////////////////////////////////////////////////////////

CStdlibApp::TShutdownMode CKernel::Run (void)
{
	mLogger.Write (FromMSX, LogNotice, "fMSX bare metal starting");

	// make sure the data folders exist
	mkdir (MSX_DIR, 0777);
	mkdir (BIOS_DIR, 0777);
	mkdir (MSX_DIR "/save", 0777);
	mkdir (MSX_DIR "/roms", 0777);
	mkdir (MSX_DIR "/disks", 0777);
	mkdir (MSX_DIR "/tapes", 0777);

	LoadSettings ();

	// give USB devices a moment to show up
	for (unsigned i = 0; i < 100; i++)
	{
		m_Input.Update ();
		CTimer::SimpleMsDelay (10);
	}

	while (1)
	{
		TMainAction Action = MainMenu ();
		if (Action == ActionLauncher)
		{
			break;
		}

		RunEmulation ();
	}

	StopSound ();

	// rebooting starts the launcher again (it is the kernel in config.txt)
	return ShutdownReboot;
}
