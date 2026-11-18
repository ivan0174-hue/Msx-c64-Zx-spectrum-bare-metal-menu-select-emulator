//
// config.h - tiny key=value configuration file
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#ifndef _rpiretro_config_h
#define _rpiretro_config_h

class CConfigFile
{
public:
	static const unsigned MaxEntries = 64;
	static const unsigned MaxKey = 32;
	static const unsigned MaxValue = 256;

	CConfigFile (void);

	bool Load (const char *pPath);
	bool Save (const char *pPath, const char *pHeader = 0) const;

	const char *Get (const char *pKey, const char *pDefault = "") const;
	int GetInt (const char *pKey, int nDefault) const;
	void Set (const char *pKey, const char *pValue);
	void SetInt (const char *pKey, int nValue);

private:
	int Find (const char *pKey) const;

	unsigned m_nCount;
	struct
	{
		char Key[MaxKey];
		char Value[MaxValue];
	}
	m_Entries[MaxEntries];
};

// helpers
void StrTrim (char *pStr);
const char *PathBaseName (const char *pPath);

#endif
