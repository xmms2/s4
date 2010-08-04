/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009, 2010 Sivert Berg
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include "s4_priv.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>

typedef enum {
	LOG_ENTRY_ADD = 0xaddadd,
	LOG_ENTRY_DEL = 0xde1e7e,
	LOG_ENTRY_WRAP = 0x123123
} log_type_t;

#define LOG_SIZE (2*1024*1024)

struct log_header {
	log_type_t type;
	log_number_t num;
	int32_t ka_len;
	int32_t va_len;
	int32_t kb_len;
	int32_t vb_len;
	int32_t s_len;
};

/**
 * Calculate the size of a log entry from the header
 */
static int _get_size (struct log_header *hdr)
{
	int ret = sizeof (struct log_header);

	ret += hdr->ka_len + hdr->kb_len + hdr->s_len;

	if (hdr->va_len == -1)
		ret += sizeof (int32_t);
	else
		ret += hdr->va_len;

	if (hdr->vb_len == -1)
		ret += sizeof (int32_t);
	else
		ret += hdr->vb_len;

	return ret;
}

static void _write_str (const char *str, int len, FILE *file)
{
	fwrite (str, 1, len, file);
}

static void _write_val (const s4_val_t *val, int len, FILE *file)
{
	const char *s;
	int32_t i;

	if (len == -1) {
		s4_val_get_int (val, &i);
		fwrite (&i, sizeof (int32_t), 1, file);
	} else {
		s4_val_get_str (val, &s);
		_write_str (s, len, file);
	}
}

static int _get_val_len (const s4_val_t *val)
{
	const char *s;

	if (s4_val_get_str (val, &s)) {
		return strlen (s);
	}
	return -1;
}

/**
 * Appends a log entry to the log
 */
static void _log (s4_t *s4, log_type_t type, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src)
{
	struct log_header header;
	int size;
	log_number_t pos, end, round;

	if (s4->logfile == NULL)
		return;

	header.type = type;
	header.ka_len = strlen (key_a);
	header.kb_len = strlen (key_b);
	header.s_len = strlen (src);
	header.va_len = _get_val_len (val_a);
	header.vb_len = _get_val_len (val_b);

	g_mutex_lock (s4->log_lock);
	size = _get_size (&header);
	pos = ftell (s4->logfile);
	round = s4->last_logpoint / LOG_SIZE;

	/* Wrap around if we're at the end */
	if ((pos + size) > (LOG_SIZE - sizeof (struct log_header))) {
		struct log_header hdr;
		hdr.num = pos + round * LOG_SIZE;
		hdr.type = LOG_ENTRY_WRAP;
		fwrite (&hdr, sizeof (struct log_header), 1, s4->logfile);
		pos = 0;
		round++;
		rewind (s4->logfile);
	}

	header.num = pos + round * LOG_SIZE;
	end = header.num + size;

	/* If writing this would overwrite log entries containing
	 * data not commited yet we have to sync
	 */
	while ((end - s4->last_checkpoint) > LOG_SIZE) {
		_sync (s4);
	}

	fwrite (&header, sizeof (struct log_header), 1, s4->logfile);

	_write_str (key_a, header.ka_len, s4->logfile);
	_write_val (val_a, header.va_len, s4->logfile);
	_write_str (key_b, header.kb_len, s4->logfile);
	_write_val (val_b, header.vb_len, s4->logfile);
	_write_str (src, header.s_len, s4->logfile);

	/* If we have written more than half the file since we last
	 * synced we start the sync thread in the background.
	 * Hopefully the sync thread will sync everything before
	 * we run out of space in the log file, and we have to stall.
	 */
	if ((end - s4->last_synced) > (LOG_SIZE / 2)) {
		_start_sync (s4);
	}

	s4->last_logpoint = header.num;

	g_mutex_unlock (s4->log_lock);
	fflush (s4->logfile);
}

void _log_add (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src)
{
	_log (s4, LOG_ENTRY_ADD, key_a, val_a, key_b, val_b, src);
}

void _log_del (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src)
{
	_log (s4, LOG_ENTRY_DEL, key_a, val_a, key_b, val_b, src);
}

static char *_read_str (FILE *file, int len)
{
	char *str = malloc (len + 1);
	fread (str, 1, len, file);
	str[len] = '\0';

	return str;
}

static s4_val_t *_read_val (FILE *file, int len)
{
	s4_val_t *ret;
	if (len == -1) {
		int32_t i;
		fread (&i, sizeof (int32_t), 1, file);
		ret = s4_val_new_int (i);
	} else {
		char *str = _read_str (file, len);
		ret = s4_val_new_string (str);
		free (str);
	}
	return ret;
}

/**
 * Redoes everything that happened since the last checkpoint
 *
 * @param s4 The database to add changes to
 * @param logfile The logfile to redo changes from
 * @return -1 on error, 0 otherwise
 */
static int _log_redo (s4_t *s4, FILE *logfile)
{
	struct log_header hdr;
	log_number_t pos, round, expected;

	pos = s4->last_checkpoint % LOG_SIZE;
	round = s4->last_checkpoint / LOG_SIZE;

	s4->last_synced = s4->last_checkpoint;

	if (fseek (logfile, pos, SEEK_SET) != 0) {
		return -1;
	}

	while (fread (&hdr, sizeof (struct log_header), 1, logfile) == 1
			&& (hdr.type == LOG_ENTRY_ADD
				|| hdr.type == LOG_ENTRY_DEL
				|| hdr.type != LOG_ENTRY_WRAP)
			&& hdr.num == (expected = pos + round * LOG_SIZE)) {

		if (hdr.type == LOG_ENTRY_WRAP) {
			round++;
			pos = 0;
			rewind (logfile);
		} else {
			char *key_a, *key_b, *src;
			s4_val_t *val_a, *val_b;

			key_a = _read_str (logfile, hdr.ka_len);
			val_a = _read_val (logfile, hdr.va_len);
			key_b = _read_str (logfile, hdr.kb_len);
			val_b = _read_val (logfile, hdr.vb_len);
			src = _read_str (logfile, hdr.s_len);

			if (hdr.type == LOG_ENTRY_ADD) {
				s4_add (s4, key_a, val_a, key_b, val_b, src);
			} else if (hdr.type == LOG_ENTRY_DEL) {
				s4_del (s4, key_a, val_a, key_b, val_b, src);
			}

			free (key_a);
			free (key_b);
			free (src);
			s4_val_free (val_a);
			s4_val_free (val_b);

			pos = ftell (logfile);
		}
	}

	s4->last_logpoint = expected;

	return 0;
}

/**
 * Opens a log file. It will redo every change written to the log
 * after the last checkpoint.
 *
 * @param s4 The database to open the logfile for
 * @return -1 on error, 0 otherwise
 */
int _log_open (s4_t *s4)
{
	char *log_name = g_strconcat (s4->filename, ".log", NULL);
	FILE *file = fopen (log_name, "r+");

	if (file == NULL) {
		file = fopen (log_name, "w");
		if (file == NULL) {
			s4_set_errno (S4E_LOGOPEN);
			return -1;
		}
	}
	g_free (log_name);

	if (_log_redo (s4, file)) {
		fclose (file);
		s4_set_errno (S4E_LOGREDO);
		return -1;
	}

	s4->logfile = file;
	return 0;
}

int _log_close (s4_t *s4)
{
	fclose (s4->logfile);
	return 0;
}
