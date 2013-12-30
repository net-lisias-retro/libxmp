/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "period.h"

#define MAGIC_MGT	MAGIC4(0x00,'M','G','T')
#define MAGIC_MCS	MAGIC4(0xbd,'M','C','S')


static int mgt_test (HIO_HANDLE *, char *, const int);
static int mgt_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader mgt_loader = {
	"Megatracker",
	mgt_test,
	mgt_load
};

static int mgt_test(HIO_HANDLE *f, char *t, const int start)
{
	int sng_ptr;

	if (hio_read24b(f) != MAGIC_MGT)
		return -1;
	hio_read8(f);
	if (hio_read32b(f) != MAGIC_MCS)
		return -1;

	hio_seek(f, 18, SEEK_CUR);
	sng_ptr = hio_read32b(f);
	hio_seek(f, start + sng_ptr, SEEK_SET);

	read_title(f, t, 32);
	
	return 0;
}

static int mgt_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j;
	int ver;
	int sng_ptr, seq_ptr, ins_ptr, pat_ptr, trk_ptr, smp_ptr;
	int sdata[64];

	LOAD_INIT();

	hio_read24b(f);		/* MGT */
	ver = hio_read8(f);
	hio_read32b(f);		/* MCS */

	set_type(m, "Megatracker MGT v%d.%d", MSN(ver), LSN(ver));

	mod->chn = hio_read16b(f);
	hio_read16b(f);			/* number of songs */
	mod->len = hio_read16b(f);
	mod->pat = hio_read16b(f);
	mod->trk = hio_read16b(f);
	mod->ins = mod->smp = hio_read16b(f);
	hio_read16b(f);			/* reserved */
	hio_read32b(f);			/* reserved */

	sng_ptr = hio_read32b(f);
	seq_ptr = hio_read32b(f);
	ins_ptr = hio_read32b(f);
	pat_ptr = hio_read32b(f);
	trk_ptr = hio_read32b(f);
	smp_ptr = hio_read32b(f);
	hio_read32b(f);			/* total smp len */
	hio_read32b(f);			/* unpacked trk size */

	hio_seek(f, start + sng_ptr, SEEK_SET);

	hio_read(mod->name, 1, 32, f);
	seq_ptr = hio_read32b(f);
	mod->len = hio_read16b(f);
	mod->rst = hio_read16b(f);
	mod->bpm = hio_read8(f);
	mod->spd = hio_read8(f);
	hio_read16b(f);			/* global volume */
	hio_read8(f);			/* master L */
	hio_read8(f);			/* master R */

	for (i = 0; i < mod->chn; i++) {
		hio_read16b(f);		/* pan */
	}
	
	MODULE_INFO();

	/* Sequence */

	hio_seek(f, start + seq_ptr, SEEK_SET);
	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = hio_read16b(f);

	/* Instruments */

	if (instrument_init(mod) < 0)
		return -1;

	hio_seek(f, start + ins_ptr, SEEK_SET);

	for (i = 0; i < mod->ins; i++) {
		int c2spd, flags;

		if (subinstrument_alloc(mod, i , 1) < 0)
			return -1;

		hio_read(mod->xxi[i].name, 1, 32, f);
		sdata[i] = hio_read32b(f);
		mod->xxs[i].len = hio_read32b(f);
		mod->xxs[i].lps = hio_read32b(f);
		mod->xxs[i].lpe = mod->xxs[i].lps + hio_read32b(f);
		hio_read32b(f);
		hio_read32b(f);
		c2spd = hio_read32b(f);
		c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
		mod->xxi[i].sub[0].vol = hio_read16b(f) >> 4;
		hio_read8(f);		/* vol L */
		hio_read8(f);		/* vol R */
		mod->xxi[i].sub[0].pan = 0x80;
		flags = hio_read8(f);
		mod->xxs[i].flg = flags & 0x03 ? XMP_SAMPLE_LOOP : 0;
		mod->xxs[i].flg |= flags & 0x02 ? XMP_SAMPLE_LOOP_BIDIR : 0;
		mod->xxi[i].sub[0].fin += 0 * hio_read8(f);	// FIXME
		hio_read8(f);		/* unused */
		hio_read8(f);
		hio_read8(f);
		hio_read8(f);
		hio_read16b(f);
		hio_read32b(f);
		hio_read32b(f);

		mod->xxi[i].nsm = !!mod->xxs[i].len;
		mod->xxi[i].sub[0].sid = i;
		
		D_(D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x %5d\n",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, c2spd);
	}

	/* PATTERN_INIT - alloc extra track*/
	if (pattern_init(mod) < 0)
		return -1;

	D_(D_INFO "Stored tracks: %d", mod->trk);

	/* Tracks */

	for (i = 1; i < mod->trk; i++) {
		int offset, rows;
		uint8 b;

		hio_seek(f, start + trk_ptr + i * 4, SEEK_SET);
		offset = hio_read32b(f);
		hio_seek(f, start + offset, SEEK_SET);

		rows = hio_read16b(f);

		if (track_alloc(mod, i, rows) < 0)
			return -1;

		//printf("\n=== Track %d ===\n\n", i);
		for (j = 0; j < rows; j++) {
			uint8 note, f2p;

			b = hio_read8(f);
			j += b & 0x03;

			note = 0;
			event = &mod->xxt[i]->event[j];
			if (b & 0x04)
				note = hio_read8(f);
			if (b & 0x08)
				event->ins = hio_read8(f);
			if (b & 0x10)
				event->vol = hio_read8(f);
			if (b & 0x20)
				event->fxt = hio_read8(f);
			if (b & 0x40)
				event->fxp = hio_read8(f);
			if (b & 0x80)
				f2p = hio_read8(f);

			if (note == 1)
				event->note = XMP_KEY_OFF;
			else if (note > 11) /* adjusted to play codeine.mgt */
				event->note = note + 1;

			/* effects */
			if (event->fxt < 0x10)
				/* like amiga */ ;
			else switch (event->fxt) {
			case 0x13: 
			case 0x14: 
			case 0x15: 
			case 0x17: 
			case 0x1c: 
			case 0x1d: 
			case 0x1e: 
				event->fxt = FX_EXTENDED;
				event->fxp = ((event->fxt & 0x0f) << 4) |
							(event->fxp & 0x0f);
				break;
			default:
				event->fxt = event->fxp = 0;
			}

			/* volume and volume column effects */
			if ((event->vol >= 0x10) && (event->vol <= 0x50)) {
				event->vol -= 0x0f;
				continue;
			}

			switch (event->vol >> 4) {
			case 0x06:	/* Volume slide down */
				event->f2t = FX_VOLSLIDE_2;
				event->f2p = event->vol - 0x60;
				break;
			case 0x07:	/* Volume slide up */
				event->f2t = FX_VOLSLIDE_2;
				event->f2p = (event->vol - 0x70) << 4;
				break;
			case 0x08:	/* Fine volume slide down */
				event->f2t = FX_EXTENDED;
				event->f2p = (EX_F_VSLIDE_DN << 4) |
							(event->vol - 0x80);
				break;
			case 0x09:	/* Fine volume slide up */
				event->f2t = FX_EXTENDED;
				event->f2p = (EX_F_VSLIDE_UP << 4) |
							(event->vol - 0x90);
				break;
			case 0x0a:	/* Set vibrato speed */
				event->f2t = FX_VIBRATO;
				event->f2p = (event->vol - 0xa0) << 4;
				break;
			case 0x0b:	/* Vibrato */
				event->f2t = FX_VIBRATO;
				event->f2p = event->vol - 0xb0;
				break;
			case 0x0c:	/* Set panning */
				event->f2t = FX_SETPAN;
				event->f2p = ((event->vol - 0xc0) << 4) + 8;
				break;
			case 0x0d:	/* Pan slide left */
				event->f2t = FX_PANSLIDE;
				event->f2p = (event->vol - 0xd0) << 4;
				break;
			case 0x0e:	/* Pan slide right */
				event->f2t = FX_PANSLIDE;
				event->f2p = event->vol - 0xe0;
				break;
			case 0x0f:	/* Tone portamento */
				event->f2t = FX_TONEPORTA;
				event->f2p = (event->vol - 0xf0) << 4;
				break;
			}
	
			event->vol = 0;

			/*printf("%02x  %02x %02x %02x %02x %02x\n",
				j, event->note, event->ins, event->vol,
				event->fxt, event->fxp);*/
		}
	}

	/* Extra track */
	mod->xxt[0] = calloc(sizeof(struct xmp_track) +
			sizeof(struct xmp_event) * 64 - 1, 1);
	mod->xxt[0]->rows = 64;

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	hio_seek(f, start + pat_ptr, SEEK_SET);

	for (i = 0; i < mod->pat; i++) {
		if (pattern_alloc(mod, i) < 0)
			return -1;

		mod->xxp[i]->rows = hio_read16b(f);
		for (j = 0; j < mod->chn; j++) {
			mod->xxp[i]->index[j] = hio_read16b(f) - 1;
		}
	}

	/* Read samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxi[i].nsm == 0)
			continue;

		hio_seek(f, start + sdata[i], SEEK_SET);
		if (load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	return 0;
}
