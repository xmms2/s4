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

#ifdef _WIN32
#include <io.h>      /* For _chsize */
#include <Windows.h> /* For (Un)LockFile */
#else
#include <unistd.h>  /* For ftruncate */
#include <fcntl.h>
#endif

typedef enum {
	LOG_ENTRY_ADD = 0xaddadd,
	LOG_ENTRY_DEL = 0xde1e7e,
	LOG_ENTRY_WRAP = 0x123123,
	LOG_ENTRY_INIT = 0x87654321,
	LOG_ENTRY_BEGIN = 0x1,
	LOG_ENTRY_END = 0x2,
	LOG_ENTRY_WRITING = 0x3,
	LOG_ENTRY_CHECKPOINT = 0x4
} log_type_t;

#define LOG_SIZE (2*1024*1024)

struct log_header {
	log_type_t type;
	log_number_t num;
};

struct mod_header {
	int32_t ka_len;
	int32_t va_len;
	int32_t kb_len;
	int32_t vb_len;
	int32_t s_len;
};

/**
 * Calculate the size of a log entry from the header
 */
static int _get_size (struct mod_header *hdr)
{
	int ret = sizeof (struct mod_header);

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

static int _estimate_size (oplist_t *list, int *writing) {
	int ret = 0, largest = 0;
	_oplist_first (list);

	while (_oplist_next (list)) {
		const char *key_a, *key_b, *src;
		const s4_val_t *val_a, *val_b;
		int size = sizeof (struct log_header);

		if (_oplist_get_add (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			size += sizeof (struct mod_header);
			size += strlen (key_a) + strlen (key_a) + strlen (src);
			size += _get_val_len (val_a) + _get_val_len (val_b);
		} else if (_oplist_get_del (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			size += sizeof (struct mod_header);
			size += strlen (key_a) + strlen (key_a) + strlen (src);
			size += _get_val_len (val_a) + _get_val_len (val_b);
		} else if (_oplist_get_writing (list)) {
			/* A write is only the size of a log header */
			*writing = 1;
		}

		largest = MAX (size, largest);
		ret += size;
	}

	if (ret == 0) {
		return 0;
	}

	/* Add the size of begin, end and a warp-around header */
	ret += 3 * sizeof (struct log_header) + largest;
	return ret;
}


static void _log_lock (s4_t *s4)
{
	g_mutex_lock (s4->log_lock);
}

static void _log_unlock (s4_t *s4)
{
	g_mutex_unlock (s4->log_lock);
}


static void _log_write_header (s4_t *s4, struct log_header hdr, int size)
{
	log_number_t pos, round;

	if (s4->logfile == NULL)
		return;

	pos = s4->next_logpoint % LOG_SIZE;
	round = s4->next_logpoint / LOG_SIZE;

	/* Wrap around if we're at the end */
	if ((pos + size) > (LOG_SIZE - sizeof (struct log_header) * 2)) {
		struct log_header hdr;

		hdr.num = pos + round * LOG_SIZE;
		hdr.type = LOG_ENTRY_WRAP;
		fwrite (&hdr, sizeof (struct log_header), 1, s4->logfile);
		pos = 0;
		round++;
		rewind (s4->logfile);
	}

	hdr.num = pos + round * LOG_SIZE;
	fwrite (&hdr, sizeof (struct log_header), 1, s4->logfile);

	s4->last_logpoint = s4->next_logpoint;
	s4->next_logpoint = ftell (s4->logfile) + round * LOG_SIZE + size;
}

/**
 * Appends a log entry to the log
 */
static void _log_mod (s4_t *s4, log_type_t type, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src)
{
	struct log_header lhdr;
	struct mod_header mhdr;
	int size;

	if (s4->logfile == NULL)
		return;

	lhdr.type = type;
	mhdr.ka_len = strlen (key_a);
	mhdr.kb_len = strlen (key_b);
	mhdr.s_len = strlen (src);
	mhdr.va_len = _get_val_len (val_a);
	mhdr.vb_len = _get_val_len (val_b);

	size = _get_size (&mhdr);

	_log_write_header (s4, lhdr, size);

	fwrite (&mhdr, sizeof (struct mod_header), 1, s4->logfile);

	_write_str (key_a, mhdr.ka_len, s4->logfile);
	_write_val (val_a, mhdr.va_len, s4->logfile);
	_write_str (key_b, mhdr.kb_len, s4->logfile);
	_write_val (val_b, mhdr.vb_len, s4->logfile);
	_write_str (src, mhdr.s_len, s4->logfile);
}

static void _log_simple (s4_t *s4, log_type_t type)
{
	struct log_header hdr;

	hdr.type = type;

	_log_write_header (s4, hdr, 0);
}

void _log_checkpoint (s4_t *s4)
{
	struct log_header hdr;
	hdr.type = LOG_ENTRY_CHECKPOINT;

	_log_lock (s4);
	_log_simple (s4, LOG_ENTRY_BEGIN);
	_log_write_header (s4, hdr, sizeof (int32_t));
	fwrite (&s4->last_synced, sizeof (log_number_t), 1, s4->logfile);
	s4->last_checkpoint = s4->last_synced;
	_log_simple (s4, LOG_ENTRY_END);
	_log_unlock (s4);
}

static void _log_flush (s4_t *s4)
{
	fflush (s4->logfile);
	fsync (fileno (s4->logfile));
}

int _log_write (oplist_t *list)
{
	s4_t *s4 = _oplist_get_db (list);
	int writing = 0;
	int size = _estimate_size (list, &writing);

	if (s4->logfile == NULL || size == 0)
		return 1;

	_log_lock (s4);
	if (writing) {
		s4->last_synced = s4->last_logpoint;
	}

	if ((s4->next_logpoint + size) > (s4->last_checkpoint + LOG_SIZE)) {
		_log_unlock (s4);
		return 0;
	}

	_log_simple (s4, LOG_ENTRY_BEGIN);

	_oplist_first (list);
	while (_oplist_next (list)) {
		const char *key_a, *key_b, *src;
		const s4_val_t *val_a, *val_b;

		if (_oplist_get_add (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			_log_mod (s4, LOG_ENTRY_ADD, key_a, val_a, key_b, val_b, src);
		} else if (_oplist_get_del (list, &key_a, &val_a, &key_b, &val_b, &src)) {
			_log_mod (s4, LOG_ENTRY_DEL, key_a, val_a, key_b, val_b, src);
		} else if (_oplist_get_writing (list)) {
			_log_simple (s4, LOG_ENTRY_WRITING);
		}
	}

	_log_simple (s4, LOG_ENTRY_END);

	if (s4->last_synced > (s4->last_checkpoint + LOG_SIZE / 2))
		_start_sync (s4);

	_log_flush (s4);
	_log_unlock (s4);
	return 1;
}

static const char *_read_str (s4_t *s4, int len)
{
	const char *ret;
	char *str = malloc (len + 1);
	fread (str, 1, len, s4->logfile);
	str[len] = '\0';

	ret = _string_lookup (s4, str);
	free (str);

	return ret;
}

static const s4_val_t *_read_val (s4_t *s4, int len)
{
	const s4_val_t *ret;
	if (len == -1) {
		int32_t i;
		fread (&i, sizeof (int32_t), 1, s4->logfile);
		ret = _int_lookup_val (s4, i);
		ret = s4_val_new_int (i);
	} else {
		const char *str = _read_str (s4, len);
		ret = _string_lookup_val (s4, str);
	}
	return ret;
}

static int _read_mod (s4_t *s4, oplist_t *list, log_type_t type)
{
	const char *key_a, *key_b, *src;
	const s4_val_t *val_a, *val_b;
	struct mod_header mhdr;

	if (list == NULL)
		return 0;

	fread (&mhdr, sizeof (struct mod_header), 1, s4->logfile);

	key_a = _read_str (s4, mhdr.ka_len);
	val_a = _read_val (s4, mhdr.va_len);
	key_b = _read_str (s4, mhdr.kb_len);
	val_b = _read_val (s4, mhdr.vb_len);
	src = _read_str (s4, mhdr.s_len);

	if (type == LOG_ENTRY_ADD) {
		_oplist_insert_add (list, key_a, val_a, key_b, val_b, src);
	} else if (type == LOG_ENTRY_DEL) {
		_oplist_insert_del (list, key_a, val_a, key_b, val_b, src);
	}

	return 1;
}

/**
 * Redoes everything that happened since the last checkpoint
 *
 * @param s4 The database to add changes to
 * @param logfile The logfile to redo changes from
 * @return 0 on error, non-zero otherwise
 */
static int _log_redo (s4_t *s4)
{
	struct log_header hdr;
	log_number_t pos, round, new_checkpoint = -1, new_synced = -1;
	log_number_t last_valid_logpoint;
	oplist_t *oplist = NULL;

	fflush (s4->logfile);

	/* Check if the log wrapped around since our last write */
	pos = s4->last_logpoint % LOG_SIZE;
	if (fseek (s4->logfile, pos, SEEK_SET) != 0 ||
			fread (&hdr, sizeof (struct log_header), 1, s4->logfile) != 1) {
		return 0;
	}

	/* If it did, we have to read in everything */
	if (hdr.num != s4->last_logpoint) {
		_reread_file (s4);
	}

	last_valid_logpoint = s4->last_logpoint;
	s4->next_logpoint = s4->last_logpoint + sizeof (struct log_header);

	pos = s4->next_logpoint % LOG_SIZE;
	round = s4->next_logpoint / LOG_SIZE;
	if (fseek (s4->logfile, pos, SEEK_SET) != 0) {
		return 0;
	}

	while (fread (&hdr, sizeof (struct log_header), 1, s4->logfile) == 1
			&& hdr.num == (pos + round * LOG_SIZE)) {

		s4->last_logpoint = s4->next_logpoint;

		if (hdr.type == LOG_ENTRY_WRAP) {
			round++;
			rewind (s4->logfile);
		} else if (hdr.type == LOG_ENTRY_DEL || hdr.type == LOG_ENTRY_ADD) {
			if (!_read_mod (s4, oplist, hdr.type))
				break;
		} else if (hdr.type == LOG_ENTRY_CHECKPOINT) {
			fread (&new_checkpoint, sizeof (log_number_t), 1, s4->logfile);
		} else if (hdr.type == LOG_ENTRY_WRITING) {
			new_synced = s4->last_logpoint;
		} else if (hdr.type == LOG_ENTRY_BEGIN) {
			oplist = _oplist_new (s4);
			new_checkpoint = -1;
			new_synced = -1;
		} else if (hdr.type == LOG_ENTRY_END) {
			if (oplist == NULL) {
				break;
			}

			_oplist_execute (oplist, 0);
			_oplist_free (oplist);
			oplist = NULL;

			if (new_checkpoint != -1) {
				s4->last_synced = s4->last_checkpoint = new_checkpoint;
			} else if (new_synced != -1) {
				s4->last_synced = new_synced;
			}
			last_valid_logpoint = s4->last_logpoint;
		} else if (hdr.type == LOG_ENTRY_INIT) {
			/* Ignore */
		} else {
			/* Unknown header type */
			break;
		}

		pos = ftell (s4->logfile);
		s4->next_logpoint = pos + round * LOG_SIZE;
	}

	if (oplist != NULL)
		_oplist_free (oplist);

	s4->last_logpoint = last_valid_logpoint;
	s4->next_logpoint = last_valid_logpoint + sizeof (struct log_header);
	pos = s4->next_logpoint % LOG_SIZE;
	fseek (s4->logfile, pos, SEEK_SET);

	return 1;
}

void _log_truncate (s4_t *s4)
{
#ifdef _WIN32
	_chsize (fileno (s4->logfile, LOG_SIZE));
#else
	ftruncate (fileno (s4->logfile), LOG_SIZE);
#endif
}

void _log_lockf (s4_t *s4, int offset)
{
#ifdef _WIN32
	while (!LockFile (fileno (s4->logfile), offset, 0, 1, 0));
#else
	struct flock lock;
	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = offset;
	lock.l_len = 1;

	while (fcntl (fileno (s4->logfile), F_SETLKW, &lock) == -1);
#endif
}

void _log_unlockf (s4_t *s4, int offset)
{
#ifdef _WIN32
	while (!UnlockFile (fileno (s4->logfile), offset, 0, 1, 0));
#else
	struct flock lock;
	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = offset;
	lock.l_len = 1;

	while (fcntl (fileno (s4->logfile), F_SETLKW, &lock) == -1);
#endif
}

/**
 * Opens a log file. It will redo every change written to the log
 * after the last checkpoint.
 *
 * @param s4 The database to open the logfile for
 * @return 0 on error, non-zero otherwise
 */
int _log_open (s4_t *s4)
{
	char *log_name = g_strconcat (s4->filename, ".log", NULL);

	s4->logfile = fopen (log_name, "r+");

	if (s4->logfile == NULL) {
		s4->logfile = fopen (log_name, "w+");
		if (s4->logfile == NULL) {
			s4_set_errno (S4E_LOGOPEN);
			return 0;
		}
		_log_truncate (s4);
		_log_simple (s4, LOG_ENTRY_INIT);
	}
	g_free (log_name);

	return 1;
}

int _log_close (s4_t *s4)
{
	fclose (s4->logfile);
	return 0;
}

void _log_lock_file (s4_t *s4)
{
	if (s4->logfile == NULL)
		return;

	_log_lock (s4);
	if (s4->log_users == 0) {
		_log_lockf (s4, 0);
		_log_redo (s4);
	}

	s4->log_users++;
	_log_unlock (s4);
}

void _log_unlock_file (s4_t *s4)
{
	if (s4->logfile == NULL)
		return;

	_log_lock (s4);
	s4->log_users--;

	if (s4->log_users < 0) {
		S4_ERROR ("_log_unlock_file called more time than _log_lock_file!");
		s4->log_users = 0;
	}
	if (s4->log_users == 0) {
		_log_unlockf (s4, 0);
	}
	_log_unlock (s4);
}

void _log_lock_db (s4_t *s4)
{
	_log_lockf (s4, 1);
}

void _log_unlock_db (s4_t *s4)
{
	_log_unlockf (s4, 1);
}
