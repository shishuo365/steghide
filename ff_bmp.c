/*
 * steghide 0.4.1 - a steganography program
 * Copyright (C) 2001 Stefan Hetzl <shetzl@teleweb.at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "main.h"
#include "io.h"
#include "bufmanag.h"
#include "support.h"
#include "msg.h"

/* reads the headers of a bmp file from disk */
void bmp_readheaders (CVRFILE *file)
{
	file->headers->bmp.bmfh.bfType = BMP_ID_BM ;
	file->headers->bmp.bmfh.bfSize = read32_le (file->stream) ;
	file->headers->bmp.bmfh.bfReserved1 = read16_le (file->stream) ;
	file->headers->bmp.bmfh.bfReserved2 = read16_le (file->stream) ;
	file->headers->bmp.bmfh.bfOffBits = read32_le (file->stream) ;
	
	file->headers->bmp.bmxh.Size = read32_le (file->stream) ;
	switch (file->headers->bmp.bmxh.Size) {
		case BMP_SIZE_BMINFOHEADER:
		file->headers->bmp.bmxh.Width = read32_le (file->stream) ;
		file->headers->bmp.bmxh.Height = read32_le (file->stream) ;
		file->headers->bmp.bmxh.Planes = read16_le (file->stream) ;
		file->headers->bmp.bmxh.BitCount = read16_le (file->stream) ;
		if ((file->headers->bmp.bmxh.Compression = read32_le (file->stream)) != BMP_BI_RGB) {
			if (args_action == ACTN_EMBED) {
				perr (ERR_CVRBMPWITHCOMPR) ;
			}
			else if (args_action == ACTN_EXTRACT) {
				perr (ERR_STGBMPWITHCOMPR) ;
			}
		}
		file->headers->bmp.bmxh.SizeImage = read32_le (file->stream) ;
		file->headers->bmp.bmxh.XPelsPerMeter = read32_le (file->stream) ;
		file->headers->bmp.bmxh.YPelsPerMeter = read32_le (file->stream) ;
		file->headers->bmp.bmxh.ClrUsed = read32_le (file->stream) ;
		file->headers->bmp.bmxh.ClrImportant = read32_le (file->stream) ;
		break ;

		case BMP_SIZE_BMCOREHEADER:
		file->headers->bmp.bmxh.Width = read16_le (file->stream) ;
		file->headers->bmp.bmxh.Height = read16_le (file->stream) ;
		file->headers->bmp.bmxh.Planes = read16_le (file->stream) ;
		file->headers->bmp.bmxh.BitCount = read16_le (file->stream) ;
		break ;

		default:
		if (args_action == ACTN_EMBED) {
			perr (ERR_CVRUNSUPBMPFILE) ;
		}
		else if (args_action == ACTN_EXTRACT) {
			perr (ERR_STGUNSUPBMPFILE) ;
		}
	}

	if (file->headers->bmp.bmxh.BitCount == 24) {
		file->unsupdata1 = NULL ;
		file->unsupdata1len = 0 ;
	}
	else {
		unsigned char *ptrunsupdata1 = NULL ;
		int i = 0 ;

		file->unsupdata1len = file->headers->bmp.bmfh.bfOffBits - (BMP_SIZE_BMFILEHEADER + file->headers->bmp.bmxh.Size) ;

		if ((file->unsupdata1 = malloc (file->unsupdata1len)) == NULL) {
			perr (ERR_MEMALLOC) ;
		}

		ptrunsupdata1 = file->unsupdata1 ;
		for (i = 0 ; i < file->unsupdata1len ; i++) {
			ptrunsupdata1[i] = (unsigned char) getc (file->stream) ;
		}
	}

	if (ferror (file->stream)) {
		if (args_action == ACTN_EMBED) {
			perr (ERR_CVRREAD) ;
		}
		else if (args_action == ACTN_EXTRACT) {
			perr (ERR_STGREAD) ;
		}
	}

	return ;
}

/* writes the headers of a bmp file to disk */
void bmp_writeheaders (CVRFILE *file)
{
	write16_le (file->stream, file->headers->bmp.bmfh.bfType) ;
	write32_le (file->stream, file->headers->bmp.bmfh.bfSize) ;
	write16_le (file->stream, file->headers->bmp.bmfh.bfReserved1) ;
	write16_le (file->stream, file->headers->bmp.bmfh.bfReserved2) ;
	write32_le (file->stream, file->headers->bmp.bmfh.bfOffBits) ;

	switch (file->headers->bmp.bmxh.Size) {
		case BMP_SIZE_BMINFOHEADER:
		write32_le (file->stream, file->headers->bmp.bmxh.Size) ;
		write32_le (file->stream, file->headers->bmp.bmxh.Width) ;
		write32_le (file->stream, file->headers->bmp.bmxh.Height) ;
		write16_le (file->stream, file->headers->bmp.bmxh.Planes) ;
		write16_le (file->stream, file->headers->bmp.bmxh.BitCount) ;
		write32_le (file->stream, file->headers->bmp.bmxh.Compression) ;
		write32_le (file->stream, file->headers->bmp.bmxh.SizeImage) ;
		write32_le (file->stream, file->headers->bmp.bmxh.XPelsPerMeter) ;
		write32_le (file->stream, file->headers->bmp.bmxh.YPelsPerMeter) ;
		write32_le (file->stream, file->headers->bmp.bmxh.ClrUsed) ;
		write32_le (file->stream, file->headers->bmp.bmxh.ClrImportant) ;
		break ;

		case BMP_SIZE_BMCOREHEADER:
		write32_le (file->stream, file->headers->bmp.bmxh.Size) ;
		write16_le (file->stream, file->headers->bmp.bmxh.Width) ;
		write16_le (file->stream, file->headers->bmp.bmxh.Height) ;
		write16_le (file->stream, file->headers->bmp.bmxh.Planes) ;
		write16_le (file->stream, file->headers->bmp.bmxh.BitCount) ;
		break ;
	}

	if (file->unsupdata1len != 0) {
		unsigned char *ptrunsupdata1 = file->unsupdata1 ;
		int i = 0 ;

		for (i = 0 ; i < file->unsupdata1len ; i++) {
			putc ((int) ptrunsupdata1[i], file->stream) ;
		}
	}

	if (ferror (file->stream)) {
		if (args_action == ACTN_EMBED) {
			perr (ERR_STGWRITE) ;
		}
		else {
			assert (0) ;
		}
	}

	return ;
}

/* reads a bmp file from disk into a CVRFILE structure */
void bmp_readfile (CVRFILE *file)
{
	unsigned long posinline = 0, line = 0 ;
	unsigned long bufpos = 0 ;

	while (line < file->headers->bmp.bmxh.Height) {
		while (posinline < file->headers->bmp.bmxh.BitCount * file->headers->bmp.bmxh.Width / 8) {
			bufsetbyte (file->cvrbuflhead, bufpos, getc (file->stream)) ;
			bufpos++ ;
			posinline++ ;
		}

		/* read padding bytes */
		while (posinline++ % 4 != 0) {
			getc (file->stream) ;
		}

		line++ ;
		posinline = 0 ;
	}

	if (ferror (file->stream)) {
		if (args_action == ACTN_EMBED) {
			perr (ERR_CVRREAD) ;
		}
		else if (args_action == ACTN_EXTRACT) {
			perr (ERR_STGREAD) ;
		}
	}
	
	return ;
}

/* writes a bmp file from a CVRFILE structure to disk */
void bmp_writefile (CVRFILE *file)
{
	unsigned long posinline = 0, line = 0 ;
	unsigned long bufpos = 0 ;

	bmp_writeheaders (file) ;

	while (line < file->headers->bmp.bmxh.Height) {
		while (posinline < file->headers->bmp.bmxh.BitCount * file->headers->bmp.bmxh.Width / 8) {
			putc (bufgetbyte (file->cvrbuflhead, bufpos), file->stream) ;
			bufpos++ ;
			posinline++ ;
		}

		/* write padding bytes */
		while (posinline++ % 4 != 0) {
			putc (0, file->stream) ;
		}

		line++ ;
		posinline = 0 ;
	}

	if (ferror (file->stream)) {
		if (args_action == ACTN_EMBED) {
			perr (ERR_STGWRITE) ;
		}
		else {
			assert (0) ;
		}
	}

	return ;
}
