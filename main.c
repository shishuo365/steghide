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

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "main.h"
#include "io.h"
#include "crypto.h"
#include "hash.h"
#include "stegano.h"
#include "msg.h"
#include "support.h"

#ifdef DEBUG
#include "test.h"
#endif

/* arguments */
int args_action = 0 ;
int args_sthdrenc = 0 ;
unsigned int sthdr_dmtd = 0 ;
DMTDINFO sthdr_dmtdinfo ;
char *args_passphrase = NULL ;
char *args_fn_cvr = NULL ;
char *args_fn_stg = NULL ;
char *args_fn_pln = NULL ;

/* indicates which arguments have already been parsed */
int arggiven_method = 0 ;
int arggiven_passphrase = 0 ;

static void parsearguments (int argc, char *argv[]) ;
static void setdefaults (void) ;
static void usage (void) ;
static void license (void) ;
static void embedfile (const char *cvrfilename, const char *stgfilename, const char *plnfilename) ;
static void extractfile (const char *stgfilename, const char *plnfilename) ;
static void cleanup (void) ;

int main (int argc, char *argv[])
{
	/* 	the C "rand" generator is used if random numbers need not be reproduceable,
		the random number generator in support.c "rnd" is used if numbers must be reproduceable */
	srand ((unsigned int) time (NULL)) ;

	parsearguments (argc, argv) ;

	switch (args_action) {
		case ACTN_EMBED:
		embedfile (args_fn_cvr, args_fn_stg, args_fn_pln) ;
		break ;

		case ACTN_EXTRACT:
		extractfile (args_fn_stg, args_fn_pln) ;
		break ;

		case ACTN_VERSION:
		printf ("steghide version 0.4.1\n") ;
		break ;

		case ACTN_LICENSE:
		license () ;
		break ;

		case ACTN_HELP:
		usage () ;
		break ;

		default:
		assert (0) ;
		break ;
	}

	cleanup () ;

	exit (EXIT_SUCCESS) ;
}

/* parses command line arguments */
static void parsearguments (int argc, char* argv[])
{
	int i ;

	/* check for first argument -> action */
	if (argc == 1) {
		args_action = ACTN_HELP ;
		return ;
	}
	else if ((strncmp (argv[1], "embed\0", 6) == 0) || (strncmp (argv[1], "--embed\0", 8) == 0)) {
		args_action = ACTN_EMBED ;
		setdefaults () ;
	}
	else if ((strncmp (argv[1], "extract\0", 8) == 0) || (strncmp (argv[1], "--extract\0", 10) == 0)) {
		args_action = ACTN_EXTRACT ;
		setdefaults () ;
	}
	else if ((strncmp (argv[1], "version\0", 8) == 0) || (strncmp (argv[1], "--version\0", 10) == 0)) {
		args_action = ACTN_VERSION ;
		return ;
	}
	else if ((strncmp (argv[1], "license\0", 8) == 0) || (strncmp (argv[1], "--license\0", 10) == 0)) {
		args_action = ACTN_LICENSE ;
		return ;
	}
	else if ((strncmp (argv[1], "help\0", 5) == 0) || (strncmp (argv[1], "--help\0", 7) == 0)) {
		args_action = ACTN_HELP ;
		return ;
	}
#ifdef DEBUG
	else if (strncmp (argv[1], "test\0", 5) == 0) {
		test_all () ;
		exit (EXIT_SUCCESS) ;
	}
#endif
	else {
		perr (ERR_ARGUNKNOWN) ;
	}

	/* check rest of arguments */
	for (i = 2; i < argc; i++) {
		if ((strncmp (argv[i], "-d\0", 3) == 0) || (strncmp (argv[i], "--distribution\0", 15) == 0)) {
			unsigned int tmp = 0 ;

			if (args_action != ACTN_EMBED) {
				perr (ERR_ARGUNKNOWN) ;
			}

			if (arggiven_method) {
				perr (ERR_METHODTWICE) ;
			}
			else {
				arggiven_method = 1 ;
			}

			if (++i == argc) {
				perr (ERR_NOMETHOD) ;
			}

			if (strncmp (argv[i], "cnsti\0", 6) == 0) {
				sthdr.dmtd = DMTD_CNSTI ;
				
				if (++i == argc) {
					perr (ERR_NOMETHOD) ;
				}
				
				if ((tmp = readnum (argv[i])) > DMTD_CNSTI_MAX_ILEN) {
					perr (ERR_NUMTOOBIG) ;
				}
				sthdr.dmtdinfo.cnsti.interval_len = tmp ;
			}
			else if ((strncmp (argv[i], "prndi\0", 6) == 0)) {
				sthdr.dmtd = DMTD_PRNDI ;
				
				if (++i == argc) {
					perr (ERR_NOMETHOD) ;
				}
				
				if ((tmp = readnum (argv[i])) > DMTD_PRNDI_MAX_IMLEN) {
					perr (ERR_NUMTOOBIG) ;
				}
				sthdr.dmtdinfo.prndi.interval_maxlen = tmp ;
			}
			else {
				perr (ERR_METHODUNKNOWN) ;
			}
		}

		else if ((strncmp (argv[i], "-e\0", 3) == 0) || (strncmp (argv[i], "--encryption\0", 13) == 0)) {
			if (args_action != ACTN_EMBED) {
				perr (ERR_ARGUNKNOWN) ;
			}
			sthdr.encryption = ENC_MCRYPT ;
		}

		else if ((strncmp (argv[i], "-E\0", 3) == 0) || (strncmp (argv[i], "--noencryption\0", 15) == 0)) {
			if (args_action != ACTN_EMBED) {
				perr (ERR_ARGUNKNOWN) ;
			}
			sthdr.encryption = ENC_NONE ;
		}

		else if ((strncmp (argv[i], "-h\0", 3) == 0) || (strncmp (argv[i], "--sthdrencryption\0", 18) == 0)) {
			args_sthdrenc = ENC_MCRYPT ;
		}

		else if ((strncmp (argv[i], "-H\0", 3) == 0) || (strncmp (argv[i], "-nosthdrencryption\0", 20) == 0)) {
			args_sthdrenc = ENC_NONE ;
		}

		else if ((strncmp (argv[i], "-m\0", 3) == 0) || (strncmp (argv[i], "--mask\0", 7) == 0)) {
			unsigned int tmp = 0 ;

			if (++i == argc) {
				perr (ERR_NOMASK) ;
			}

			if (args_action != ACTN_EMBED) {
				perr (ERR_ARGUNKNOWN) ;
			}

			tmp = readnum (argv[i]) ;
			if (tmp > MAXMASK) {
				perr (ERR_NUMTOOBIG) ;
			}
			else if (tmp == 0) {
				perr (ERR_MASKZERO) ;
			}
			else {
				sthdr.mask = (unsigned char) tmp ;
			}
		}

		else if ((strncmp (argv[i], "-p\0", 3) == 0) || (strncmp (argv[i], "--passphrase\0", 12) == 0)) {
			int j = 0 ;

			if (arggiven_passphrase) {
				perr (ERR_DATATWOPASSPHRASES) ;
			}
			else {
				arggiven_passphrase = 1 ;
			}

			if (++i == argc) {
				perr (ERR_NOPASSPHRASE) ;
			}

			if (strlen (argv[i]) > PASSPHRASE_MAXLEN) {
				perr (ERR_PASSPHRASETOOLONG) ;
			}
			if ((args_passphrase = malloc (strlen (argv[i]) + 1)) == NULL) {
				perr (ERR_MEMALLOC) ;
			}
			strcpy (args_passphrase, argv[i]) ;

			for (j = 0 ; j < strlen (argv[i]) ; j++) {
				argv[i][j] = '*' ;
			}
		}

		else if ((strncmp (argv[i], "-cf\0", 4) == 0) || (strncmp (argv[i], "--coverfile\0", 16) == 0)) {
			if (args_action != ACTN_EMBED) {
				perr (ERR_ARGUNKNOWN) ;
			}

			if (++i == argc) {
				perr (ERR_NOCVRFILENAME) ;
			}
			if ((args_fn_cvr = malloc (strlen (argv[i]) + 1)) == NULL) {
				perr (ERR_MEMALLOC) ;
			}
			strcpy (args_fn_cvr, argv[i]) ;
		}

		else if ((strncmp (argv[i], "-sf\0", 4) == 0) || (strncmp (argv[i], "--stegofile\0", 12) == 0)) {
			if (++i == argc) {
				perr (ERR_NOSTGFILENAME) ;
			}
			if ((args_fn_stg = malloc (strlen (argv[i]) + 1)) == NULL) {
				perr (ERR_MEMALLOC) ;
			}
			strcpy (args_fn_stg, argv[i]) ;
		}

		else if ((strncmp (argv[i], "-pf\0", 4) == 0) || (strncmp (argv[i], "--plainfile\0", 12) == 0)) {
			if (++i == argc) {
				perr (ERR_NOPLNFILENAMECMDL) ;
			}
			if ((args_fn_pln = malloc (strlen (argv[i]) + 1)) == NULL) {
				perr (ERR_MEMALLOC) ;
			}
			strcpy (args_fn_pln, argv[i]) ;

			if (strcmp (argv[i], "-") == 0) {
				sthdr.plnfilename = NULL ;
			}
			else {
				if ((sthdr.plnfilename = malloc (strlen (argv[i]) + 1)) == NULL) {
					perr (ERR_MEMALLOC) ;
				}
				strcpy (sthdr.plnfilename, argv[i]) ;
			}
		}

		else {
			perr (ERR_ARGUNKNOWN) ;
		}
	}

	/* argument post-processing */
	if (arggiven_passphrase == 0) {
		perr (ERR_NOPASSPHRASE) ;
	}

	if (args_action == ACTN_EMBED) {
		if ((args_fn_cvr == NULL) && (args_fn_pln == NULL)) {
			perr (ERR_STDINTWICE) ;
		}
	}

	if (args_fn_cvr == NULL) {
		if (args_action == ACTN_EMBED) {
			if ((args_fn_cvr = malloc (2)) == NULL) {
				perr (ERR_MEMALLOC) ;
			}
			strcpy (args_fn_cvr, "-") ;
		}
	}

	if (args_fn_stg == NULL) {
		if ((args_fn_stg = malloc (2)) == NULL) {
			perr (ERR_MEMALLOC) ;
		}
		strcpy (args_fn_stg, "-") ;
	}

	if (args_fn_pln == NULL) {
		sthdr.plnfilename = NULL ;
	}

	sthdr_dmtd = DMTD_PRNDI ;
	sthdr_dmtdinfo.prndi.seed = get32hash (args_passphrase) ;
	sthdr_dmtdinfo.prndi.interval_maxlen = 2 * INTERVAL_DEFAULT ;
}

static void setdefaults (void)
{
	unsigned char tmp[4] ;

	sthdr.dmtd = DMTD_PRNDI ;

	tmp[0] = (unsigned char) (256.0 * rand() / (RAND_MAX + 1.0)) ;
	tmp[1] = (unsigned char) (256.0 * rand() / (RAND_MAX + 1.0)) ;
	tmp[2] = (unsigned char) (256.0 * rand() / (RAND_MAX + 1.0)) ;
	tmp[3] = (unsigned char) (256.0 * rand() / (RAND_MAX + 1.0)) ;
	cp32uc2ul_be (&sthdr.dmtdinfo.prndi.seed, tmp) ; 
	sthdr.dmtdinfo.prndi.interval_maxlen = 2 * INTERVAL_DEFAULT ;

	sthdr.mask = 1 ;
	sthdr.encryption = ENC_MCRYPT ;

	/* compression and checksum are not yet implemented but included
	   to enable 0.4.1 to read not compressed post 0.4.1 files */
	sthdr.compression = COMPR_NONE ;
	sthdr.checksum = CKSUM_NONE ;

	args_sthdrenc = 1 ;
}

static void usage (void)
{
	printf ("steghide version 0.4.1\n\n") ;

	printf ("the first argument must be one of the following:\n") ;
	printf (" embed, --embed          embed plain data in cover data\n") ;
	printf (" extract, --extract      extract plain data from stego data\n") ;
	printf (" version, --version      display version information\n") ;
	printf (" license, --license      display steghide's license\n") ;
	printf (" help, --help            display this usage information\n") ;

	printf ("\noptions for data embedding only:\n") ;

	printf (" -d, --distribution      distribution method for secret bits in cover bits\n") ;
	printf ("   -d cnsti <n>          constant intervals, length: <n>\n") ;
	printf ("   -d prndi <n>          pseudo-random intervals, maximum interval length: <n>\n") ;
	printf (" -cf, --coverfile        select cover file\n") ;
	printf ("   -cf <filename>        use <filename> as cover file\n") ;
	printf (" -e, --encryption        encrypt plain data before embedding (default)\n") ;
	printf (" -E, --noencryption      do not encrypt plain data before embedding\n") ;
	printf (" -m, --mask              specify mask used to embed secret bits in cover bits\n") ;
	printf ("   -m <n>                use <n> as mask\n") ;

	printf ("\noptions for embedding and extracting:\n") ;

	printf (" -p, --passphrase        specify passphrase (mandatory)\n") ;
	printf ("   -p <passphrase>       use <passphrase> as passphrase\n") ;
	printf (" -sf, --stegofile        select stego file\n") ;
	printf ("   -sf <filename>        use <filename> as stego file\n") ;
	printf (" -pf, --plainfile        select plain file\n") ;
	printf ("   -pf <filename>        use <filename> as plain file\n") ;
	printf (" -h, --sthdrencryption   encrypt stego header before embedding (default)\n") ;
	printf (" -H, --nosthdrencryption do not encrypt stego header before embedding\n") ;

	printf ("\nIf a <filename> is \"-\", stdin or stdout is used.\n\n") ;

	printf ("The arguments default to reasonable values.\n") ;
	printf ("For embedding the following line is enough:\n") ;
	printf ("steghide embed -cf cvr.bmp -sf stg.bmp -pf pln.txt -p \"my passphrase\"\n\n") ;

	printf ("For extracting it is sufficient to use:\n") ;
	printf ("steghide extract -sf stg.bmp -p \"my passphrase\"\n\n") ;

}

static void license ()
{
 	printf ("Copyright (C) 2001 Stefan Hetzl <shetzl@teleweb.at>\n\n") ;

 	printf ("This program is free software; you can redistribute it and/or\n") ;
 	printf ("modify it under the terms of the GNU General Public License\n") ;
 	printf ("as published by the Free Software Foundation; either version 2\n") ;
 	printf ("of the License, or (at your option) any later version.\n\n") ;

 	printf ("This program is distributed in the hope that it will be useful,\n") ;
 	printf ("but WITHOUT ANY WARRANTY; without even the implied warranty of\n") ;
 	printf ("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n") ;
 	printf ("GNU General Public License for more details.\n\n") ;

 	printf ("You should have received a copy of the GNU General Public License\n") ;
 	printf ("along with this program; if not, write to the Free Software\n") ;
 	printf ("Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.\n") ;
}

/* calls functions to embed plain data in cover data and save as stego data */
static void embedfile (const char *cvrfilename, const char *stgfilename, const char *plnfilename)
{
	CVRFILE *cvrfile = NULL, *stgfile = NULL ;
	PLNFILE *plnfile = NULL ;
	unsigned long firstplnpos = 0 ;

	cvrfile = readcvrfile (cvrfilename) ;

	plnfile = readplnfile (plnfilename) ;

	if (sthdr.encryption) {
		encrypt_plnfile (plnfile, args_passphrase) ;
	}

	embedsthdr (cvrfile->cvrbuflhead, sthdr_dmtd, sthdr_dmtdinfo, args_sthdrenc, args_passphrase, &firstplnpos) ;

	embeddata (cvrfile->cvrbuflhead, firstplnpos, plnfile->plnbuflhead) ;

	stgfile = createstgfile (cvrfile, stgfilename) ;

	writecvrfile (stgfile) ;

	cleanupcvrfile (cvrfile, FSS_NO) ;
	cleanupcvrfile (stgfile, FSS_YES) ;
	cleanupplnfile (plnfile) ;

	return ;
}

/* calls functions to extract (and save) plain data from stego data */
static void extractfile (const char *stgfilename, const char *plnfilename)
{
	CVRFILE *stgfile = NULL ;
	PLNFILE *plnfile = NULL ;
	unsigned long firstplnpos = 0 ;

	stgfile = readcvrfile (stgfilename) ;

	extractsthdr (stgfile->cvrbuflhead, sthdr_dmtd, sthdr_dmtdinfo, args_sthdrenc, args_passphrase, &firstplnpos) ;

	plnfile = createplnfile () ;

	plnfile->plnbuflhead = extractdata (stgfile->cvrbuflhead, firstplnpos) ;

	if (sthdr.encryption) {
		decrypt_plnfile (plnfile, args_passphrase) ;
	}

	writeplnfile (plnfile) ;

	cleanupcvrfile (stgfile, FSS_YES) ;
	cleanupplnfile (plnfile) ;

	return ;
}

static void cleanup (void)
{
	if (args_fn_cvr != NULL) {
		free (args_fn_cvr) ;
	}
	if (args_fn_pln != NULL) {
		free (args_fn_pln) ;
	}
	if (args_fn_stg != NULL) {
		free (args_fn_stg) ;
	}
	if (args_passphrase != NULL) {
		free (args_passphrase) ;
	}
}