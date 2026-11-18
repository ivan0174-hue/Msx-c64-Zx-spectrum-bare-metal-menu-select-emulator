//
// config.cpp
//
// Part of rpi-retro (bare metal launcher + MSX emulator for Raspberry Pi 3)
// License: GPLv3
//
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void StrTrim (char *pStr)
{
	char *p = pStr;
	while (*p && isspace ((unsigned char) *p))
	{
		p++;
	}
	if (p != pStr)
	{
		memmove (pStr, p, strlen (p) + 1);
	}
	size_t n = strlen (pStr);
	while (n > 0 && isspace ((unsigned char) pStr[n - 1]))
	{
		pStr[--n] = 0;
	}
}

const char *PathBaseName (const char *pPath)
{
	const char *p = strrchr (pPath, '/');
	return p ? p + 1 : pPath;
}

CConfigFile::CConfigFile (void)
:	m_nCount (0)
{
}

bool CConfigFile::Load (const char *pPath)
{
	FILE *fp = fopen (pPath, "r");
	if (fp == 0)
	{
		return false;
	}

	char Line[MaxKey + MaxValue + 16];
	while (fgets (Line, sizeof Line, fp) != 0)
	{
		StrTrim (Line);
		if (Line[0] == 0 || Line[0] == '#' || Line[0] == ';')
		{
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
		Set (pKey, pValue);
	}

	fclose (fp);
	return true;
}

bool CConfigFile::Save (const char *pPath, const char *pHeader) const
{
	FILE *fp = fopen (pPath, "w");
	if (fp == 0)
	{
		return false;
	}

	if (pHeader != 0)
	{
		fputs (pHeader, fp);
	}
	for (unsigned i = 0; i < m_nCount; i++)
	{
		fprintf (fp, "%s=%s\n", m_Entries[i].Key, m_Entries[i].Value);
	}

	fclose (fp);
	return true;
}

int CConfigFile::Find (const char *pKey) const
{
	for (unsigned i = 0; i < m_nCount; i++)
	{
		if (strcasecmp (m_Entries[i].Key, pKey) == 0)
		{
			return (int) i;
		}
	}
	return -1;
}

const char *CConfigFile::Get (const char *pKey, const char *pDefault) const
{
	int i = Find (pKey);
	return i >= 0 ? m_Entries[i].Value : pDefault;
}

int CConfigFile::GetInt (const char *pKey, int nDefault) const
{
	int i = Find (pKey);
	return i >= 0 ? atoi (m_Entries[i].Value) : nDefault;
}

void CConfigFile::Set (const char *pKey, const char *pValue)
{
	int i = Find (pKey);
	if (i < 0)
	{
		if (m_nCount >= MaxEntries)
		{
			return;
		}
		i = (int) m_nCount++;
		strncpy (m_Entries[i].Key, pKey, MaxKey - 1);
		m_Entries[i].Key[MaxKey - 1] = 0;
	}
	strncpy (m_Entries[i].Value, pValue, MaxValue - 1);
	m_Entries[i].Value[MaxValue - 1] = 0;
}

void CConfigFile::SetInt (const char *pKey, int nValue)
{
	char Buf[16];
	snprintf (Buf, sizeof Buf, "%d", nValue);
	Set (pKey, Buf);
}
