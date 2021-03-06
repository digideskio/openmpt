/*
 * ungzip.h
 * --------
 * Purpose: Header file for .gz loader
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 */

#pragma once

#ifndef ZLIB_WINAPI
#define ZLIB_WINAPI
#endif // ZLIB_WINAPI
#include <cstdint>
#include "../zlib-1.2.5/zlib.h"

#pragma pack(1)

struct GZheader
{
	uint8_t  magic1;	// 0x1F
	uint8_t  magic2;	// 0x8B
	uint8_t  method;	// 0-7 = reserved, 8 = deflate
	uint8_t  flags;	// See GZ_F* constants
	uint32_t mtime;	// UNIX time
	uint8_t  xflags;	// Available for use by specific compression methods. We ignore this.
	uint8_t  os;		// Which OS was used to compress the file? We also ignore this.
};

struct GZtrailer
{
	uint32_t crc32;	// CRC32 of decompressed data
	uint32_t isize;	// Size of decompressed data
};

#pragma pack()

// magic bytes
#define GZ_HMAGIC1		0x1F
#define GZ_HMAGIC2		0x8B
#define GZ_HMDEFLATE	0x08
// header flags
#define GZ_FTEXT		0x01	// File is probably ASCII text (who cares)
#define GZ_FHCRC		0x02	// CRC16 present
#define GZ_FEXTRA		0x04	// Extra fields present
#define GZ_FNAME		0x08	// Original filename present
#define GZ_FCOMMENT		0x10	// Comment is present
#define GZ_FRESERVED	(~(GZ_FTEXT | GZ_FHCRC | GZ_FEXTRA | GZ_FNAME | GZ_FCOMMENT))


//================
class CGzipArchive
//================
{
public:

	LPBYTE GetOutputFile() const { return m_pOutputFile; }
	DWORD GetOutputFileLength() const { return m_dwOutputLen; }
	bool IsArchive() const;
	bool ExtractFile();

	CGzipArchive(LPBYTE lpStream, DWORD dwMemLength);
	~CGzipArchive();

protected:
	// in
	LPBYTE m_lpStream;
	DWORD m_dwStreamLen;
	// out
	Bytef *m_pOutputFile;
	DWORD m_dwOutputLen;
};
