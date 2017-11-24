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
#include <time.h>
#include <assert.h>

#include "stegano.h"
#include "bufmanag.h"
#include "support.h"
#include "crypto.h"
#include "msg.h"

STEGOHEADER sthdr = { 0, 0, { { 0 } }, '\0', 0, 0, 0, NULL } ;

static int setbits (int cvrbyte, int plnbits) ;
static int getbits (int stgbyte) ;
static int nstgbits (void) ;
#ifndef DEBUG
void dmtd_reset (unsigned int dmtd, DMTDINFO dmtdinfo, unsigned long resetpos) ;
unsigned long dmtd_nextpos (void) ;
#endif

static unsigned int curdmtd_dmtd ;
static DMTDINFO curdmtd_dmtdinfo ;
static unsigned long curdmtd_curpos ;

/* embed plain data in cover data (resulting in stego data) */
void embeddata (BUFFER *cvrbuflhead, unsigned long firstcvrpos, BUFFER *plnbuflhead)
{
	int cvrbyte = 0, plnbits = 0 ;
	int bit = 0 ;
	int i = 0 ;
	unsigned long plnpos_byte = 0, plnpos_bit = 0 ;
	unsigned long cvrpos_byte = firstcvrpos ;

	dmtd_reset (sthdr.dmtd, sthdr.dmtdinfo, cvrpos_byte) ;

	while (plnpos_byte < buflength (plnbuflhead)) {
		plnbits = 0 ;
		for (i = 0 ; i < nstgbits() ; i++) {
			if ((bit = bufgetbit (plnbuflhead, plnpos_byte, plnpos_bit)) != ENDOFBUF) {
				plnbits |= (bit << i) ;
				if (plnpos_bit == 7) {
					plnpos_byte++ ;
					plnpos_bit = 0 ;
				}
				else {
					plnpos_bit++ ;
				}
			}
		}

		cvrbyte = bufgetbyte (cvrbuflhead, cvrpos_byte) ;
		bufsetbyte (cvrbuflhead, cvrpos_byte, setbits (cvrbyte, plnbits)) ;
		
		cvrpos_byte = dmtd_nextpos () ;

		if ((cvrpos_byte >= buflength (cvrbuflhead)) && (plnpos_byte < buflength (plnbuflhead))) {
			perr (ERR_CVRTOOSHORT) ;
		}
	}

	return ;
}

/* extracts plain data (return value) from stego data */
BUFFER *extractdata (BUFFER *stgbuflhead, unsigned long firststgpos)
{
	BUFFER *plnbuflhead = NULL ;
	int plnbits = 0, i = 0 ;
	unsigned long plnpos_byte = 0, plnpos_bit = 0 ;
	unsigned long size = 0 ;
	unsigned long stgpos_byte = firststgpos ;

	dmtd_reset (sthdr.dmtd, sthdr.dmtdinfo, stgpos_byte) ;

	plnbuflhead = createbuflist() ;

	if (sthdr.encryption == ENC_MCRYPT) {
		if (sthdr.nbytesplain % BLOCKSIZE_BLOWFISH == 0) {
			size = sthdr.nbytesplain + BLOCKSIZE_BLOWFISH /* IV */ ;
		}
		else {
			size = (((sthdr.nbytesplain / BLOCKSIZE_BLOWFISH) + 1) * BLOCKSIZE_BLOWFISH) + BLOCKSIZE_BLOWFISH /* IV */ ;
		}
	}
	else {
		size = sthdr.nbytesplain ;
	}

	while (plnpos_byte < size) {
		plnbits = getbits (bufgetbyte (stgbuflhead, stgpos_byte)) ;
		for (i = 0 ; i < nstgbits() ; i++) {
			if (plnpos_byte < size) {
				bufsetbit (plnbuflhead, plnpos_byte, plnpos_bit, ((plnbits & (1 << i)) >> i)) ;
				if (plnpos_bit == 7) {
					plnpos_byte++ ;
					plnpos_bit = 0 ;
				}
				else {
					plnpos_bit++ ;
				}
			}
		}

		stgpos_byte = dmtd_nextpos() ;

		if ((stgpos_byte >= buflength (stgbuflhead)) && (plnpos_byte < size)) {
			perr (ERR_STGTOOSHORT) ;
		}
	}

	return plnbuflhead ;
}

void embedsthdr (BUFFER *cvrbuflhead, int dmtd, DMTDINFO dmtdinfo, int enc, char *passphrase, unsigned long *firstplnpos)
{
	int hdrbuflen = STHDR_NBYTES_BLOWFISH ;
	unsigned char *hdrbuf = NULL ;
	unsigned int bit = 0, sthdrbuflen = 0 ;
	unsigned int bitval = 0 ;
	unsigned long cvrbytepos = 0 ;

	if ((hdrbuf = calloc (STHDR_NBYTES_BLOWFISH, 1)) == NULL) {
		perr (ERR_MEMALLOC) ;
	}

	/* assemble bits that make up sthdr in a buffer */
	bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) nbits (sthdr.nbytesplain), SIZE_NBITS_NBYTESPLAIN) ;
	bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) sthdr.nbytesplain, nbits (sthdr.nbytesplain)) ;
	
	bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) sthdr.dmtd, SIZE_DMTD) ;
	switch (sthdr.dmtd) {
		case DMTD_CNSTI:
			bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) nbits (sthdr.dmtdinfo.cnsti.interval_len), SIZE_DMTDINFO_CNSTI_NBITS_ILEN) ;
			bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) sthdr.dmtdinfo.cnsti.interval_len, nbits (sthdr.dmtdinfo.cnsti.interval_len)) ;
		break ;

		case DMTD_PRNDI:
			bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) sthdr.dmtdinfo.prndi.seed, SIZE_DMTDINFO_PRNDI_SEED) ;
			bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) nbits (sthdr.dmtdinfo.prndi.interval_maxlen), SIZE_DMTDINFO_PRNDI_NBITS_IMLEN) ;
			bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) sthdr.dmtdinfo.prndi.interval_maxlen, nbits (sthdr.dmtdinfo.prndi.interval_maxlen)) ;
		break ;

		default:
		assert (0) ;
		break ;
	}

	if (sthdr.mask == DEFAULTMASK) {
		bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) 0, SIZE_MASKUSED) ;
	}
	else {
		bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) 1, SIZE_MASKUSED) ;
		bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) sthdr.mask, SIZE_MASK) ;
	}

	bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) sthdr.encryption, SIZE_ENCRYPTION) ;

	/* compression and checksum are not yet implemented */
	bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) COMPR_NONE, SIZE_COMPRESSION) ;
	bit = cp_bits_to_buf_le (hdrbuf, bit, (unsigned long) CKSUM_NONE, SIZE_CHECKSUM) ;

	/* eventually encrypt the buffer */
	if (enc) {
		/* pad with random bits */
		while (bit < hdrbuflen * 8) {
			bit = cp_bits_to_buf_le (hdrbuf, bit, (2.0 * rand() / (RAND_MAX + 1.0)), 1) ;
		}
		assert (bit == hdrbuflen * 8) ;

		encrypt_sthdr (hdrbuf, hdrbuflen, passphrase) ;
	}

	/* embed the buffer */
	sthdrbuflen = bit ;
	dmtd_reset (dmtd, dmtdinfo, 0) ;
	cvrbytepos = 0 ;
	for (bit = 0 ; bit < sthdrbuflen ; bit++) {
		bitval = (hdrbuf[bit / 8] & (1 << (bit % 8))) >> (bit % 8) ;
		bufsetbit (cvrbuflhead, cvrbytepos, 0, bitval) ;
		if (((cvrbytepos = dmtd_nextpos()) >= buflength (cvrbuflhead)) &&
		   bit < sthdrbuflen) {
			perr (ERR_CVRTOOSHORT) ;
		}
	}

	free (hdrbuf) ;
	
	*firstplnpos = cvrbytepos ;
}	

void extractsthdr (BUFFER *stgbuflhead, int dmtd, DMTDINFO dmtdinfo, int enc, char *passphrase, unsigned long *firstplnpos)
{
	int hdrbuflen = STHDR_NBYTES_BLOWFISH ;
	unsigned char hdrbuf[STHDR_NBYTES_BLOWFISH] ;
	unsigned long oldcvrbytepos[(8 * STHDR_NBYTES_BLOWFISH) + 1] ;
	unsigned long cvrbytepos = 0 ;
	int bitval = 0 ;
	unsigned long nbits = 0 ;
	unsigned long tmp = 0 ;
	unsigned int bit = 0 ;
	int i = 0 ;

	dmtd_reset (dmtd, dmtdinfo, 0) ;

	for (i = 0 ; i < BLOCKSIZE_BLOWFISH * STHDR_NBLOCKS_BLOWFISH ; i++) {
		hdrbuf[i] = 0 ;
	}

	/* this is a pretty dirty hack, but it prevents duplicating a lot of the code:
	   we read hdrbuflen bits even if the sthdr is not encrypted, thus
	   having read bits at the end of hdrbuf that have nothing to do with the sthdr.
	   After that we set the dmtd position generator back if sthdr is not encrypted */

	for (bit = 0 ; bit < hdrbuflen * 8 ; bit++) {
		oldcvrbytepos[bit] = cvrbytepos ;
		bitval = bufgetbit (stgbuflhead, cvrbytepos, 0) ;
		hdrbuf[bit / 8] |= bitval << (bit % 8) ;
		if (((cvrbytepos = dmtd_nextpos()) >= buflength (stgbuflhead)) &&
		   bit < hdrbuflen * 8) {
			perr (ERR_STGTOOSHORT) ;
		}
	}
	oldcvrbytepos[bit] = cvrbytepos ;

	if (enc) {
		decrypt_sthdr (hdrbuf, hdrbuflen, passphrase) ;
	}

	/* copy the values from the buffer into the sthdr structure */
	bit = 0 ;
	bit = cp_bits_from_buf_le (hdrbuf, bit, &nbits, SIZE_NBITS_NBYTESPLAIN) ;
	bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, nbits) ;
	sthdr.nbytesplain = (unsigned long) tmp ;

	bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, SIZE_DMTD) ;
	sthdr.dmtd = (unsigned int) tmp ;
	switch (sthdr.dmtd) {
		case DMTD_CNSTI:
			bit = cp_bits_from_buf_le (hdrbuf, bit, &nbits, SIZE_DMTDINFO_CNSTI_NBITS_ILEN) ;
			bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, nbits) ;
			sthdr.dmtdinfo.cnsti.interval_len = (unsigned int) tmp ;
		break ;

		case DMTD_PRNDI:
			bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, SIZE_DMTDINFO_PRNDI_SEED) ;
			sthdr.dmtdinfo.prndi.seed = (unsigned long) tmp ;
			bit = cp_bits_from_buf_le (hdrbuf, bit, &nbits, SIZE_DMTDINFO_PRNDI_NBITS_IMLEN) ;
			bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, nbits) ;
			sthdr.dmtdinfo.prndi.interval_maxlen = (unsigned int) tmp ;
		break ;

		default:
			perr (ERR_STHDRDMTDUNKNOWN) ;
		break ;
	}

	bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, SIZE_MASKUSED) ;
	if (tmp) {
		bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, SIZE_MASK) ;
		if (tmp == 0) {
			perr (ERR_MASKZERO) ;
		}
		sthdr.mask = (unsigned int) tmp ;
	}

	bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, SIZE_ENCRYPTION) ;
	sthdr.encryption = (unsigned int) tmp ;

	bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, SIZE_COMPRESSION) ;
	sthdr.compression = (unsigned int) tmp ;
	if (sthdr.compression != COMPR_NONE) {
		perr (ERR_COMPRNOTIMPL) ;
	}

	bit = cp_bits_from_buf_le (hdrbuf, bit, &tmp, SIZE_CHECKSUM) ;
	sthdr.checksum = (unsigned int) tmp ;
	if (sthdr.checksum != CKSUM_NONE) {
		perr (ERR_CKSUMNOTIMPL) ;
	}

	/* set *firstplnpos */
	if (enc) {
		*firstplnpos = oldcvrbytepos[hdrbuflen * 8] ;
	}
	else {
		*firstplnpos = oldcvrbytepos[bit] ;
	}

	return ;
}

/* writes plnbits to cvrbyte as determined by sthdr.mask resulting in return value stgbyte */
static int setbits (int cvrbyte, int plnbits)
{
	int maskbitpos = 0, plnbitpos = 0 ;
	int stgbyte = 0, bit = 0 ;

	stgbyte = cvrbyte & (255 - sthdr.mask) ;

	while (maskbitpos < 8) {
		while ((!(sthdr.mask & (1 << maskbitpos))) && (maskbitpos < 8))
			maskbitpos++ ;

		if (maskbitpos == 8)
			break ;

		bit = (plnbits & (1 << plnbitpos)) >> plnbitpos ;
		stgbyte |= bit << maskbitpos ;
		plnbitpos++ ;
		maskbitpos++ ;
	}

	return stgbyte ;
}

/* reads plain bits (return value) from stgbyte as determined by sthdr.mask */
static int getbits (int stgbyte)
{
	int maskbitpos = 0, plnbitpos = 0 ;
	int plnbits = 0, bit ;

	while (maskbitpos < 8) {
		while ((!(sthdr.mask & (1 << maskbitpos))) && (maskbitpos < 8)) {
			maskbitpos++ ;
		}

		if (maskbitpos == 8) {
			break ;
		}

		bit = (stgbyte & (1 << maskbitpos)) >> maskbitpos ;
		plnbits |= bit << plnbitpos ;
		plnbitpos++ ;
		maskbitpos++ ;
	}

	return plnbits ;
}

/* returns the number of set bits in sthdr.mask */
static int nstgbits (void)
{
	int i = 0, n = 0 ;

	for (i = 0; i < 8; i++)
		if (sthdr.mask & (1 << i))
			n++ ;

	return n ;
}

void dmtd_reset (unsigned int dmtd, DMTDINFO dmtdinfo, unsigned long resetpos)
{
	switch (dmtd) {
		case DMTD_CNSTI:
		/* no initialization code necessary */
		break ;

		case DMTD_PRNDI:
		srnd (dmtdinfo.prndi.seed) ;
		break ;

		default:
		assert (0) ;
		break ;
	}

	curdmtd_dmtd = dmtd ;
	curdmtd_dmtdinfo = dmtdinfo ;
	curdmtd_curpos = resetpos ;

	return ;
}

unsigned long dmtd_nextpos (void)
{
	switch (curdmtd_dmtd) {
		case DMTD_CNSTI:
		curdmtd_curpos += curdmtd_dmtdinfo.cnsti.interval_len + 1 ;
		break ;

		case DMTD_PRNDI:
		curdmtd_curpos += rnd (curdmtd_dmtdinfo.prndi.interval_maxlen) + 1 ;
		break ;

		default:
		assert (0) ;
		break ;
	}

	return curdmtd_curpos ;
}
