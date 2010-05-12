/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009 Sivert Berg
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

#include "s4_be.h"
#include "log.h"
#include "be.h"
#include "pat.h"
#include "bpt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>

#ifndef _WIN32
	#include <errno.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
	#include <sys/mman.h>
	#include <fcntl.h>
#endif


#define CLEAN 0
#define DIRTY 1

/* Define the biggest and smallest chunk size (exponents) */
#define BIGGEST_CHUNK 16 /* 2^16 = 65536 */
#define SMALLEST_CHUNK 4 /* 2^4  = 16 */

#define S4_MAGIC 0x54DABA5E
#define S4_VERSION 1

/**
 *
 * @defgroup Homegrown Homegrown
 * @ingroup S4
 * @brief A homegrown backend for S4
 *
 * @{
 */

typedef struct header_St {
	pat_trie_t string_store;
	bpt_t int_store, int_rev;
	int32_t sync_state;
	int32_t magic;
	int32_t version;

	int32_t free_lists[BIGGEST_CHUNK - SMALLEST_CHUNK + 1];
	int free;
} header_t;

typedef struct chunk_St {
	int32_t next;
} chunk_t;


static inline long pagesize (void)
{
	static long ps = 0;

	if (ps == 0) {
#ifdef _WIN32
		SYSTEM_INFO info;
		GetSystemInfo (&info);
		ps = info.dwPageSize;
#else
		ps = sysconf (_SC_PAGESIZE);
#endif
	}

	return ps;
}


static int log2 (unsigned int x)
{
	int ret = 31;

	if (!x) return 0;

	while (!(x & (1 << 31))) {
		x <<= 1;
		ret--;
	}

	if (x > (1 << 31)) ret++;

	return ret;
}

static void map_unmap (s4be_t *s4)
{
#ifdef _WIN32
	UnmapViewOfFile (s4->map);
	CloseHandle (s4->fw);
#else
	munmap (s4->map, s4->size);
#endif
}

/* Resize and map s4->fd and save the mapped address in s4->map.
 * s4->map is NULL if something went wrong.
 */
static void map_file (s4be_t *s4)
{
#ifdef _WIN32
	s4->fw = CreateFileMapping (s4->fd, NULL, PAGE_READWRITE, 0, s4->size, NULL);
	s4->map = MapViewOfFile (s4->fw, FILE_MAP_WRITE, 0, 0, s4->size);
#else
	ftruncate(s4->fd, s4->size);
	s4->map = mmap (NULL, s4->size, PROT_READ | PROT_WRITE, MAP_SHARED, s4->fd, 0);
	if (s4->map == MAP_FAILED)
		s4->map = NULL;
#endif
}

/* Make sure all the data in the memory mapped file is written to disc. */
static void map_sync (s4be_t *s4, size_t len)
{
#ifdef _WIN32
	FlushViewOfFile (s4->map, len);
	FlushFileBuffers (s4->fd);
#else
	msync (s4->map, len, MS_SYNC);
#endif
}


/* Sync the database if it is dirty */
void s4be_sync (s4be_t *s4)
{
	header_t *header;

	/* We only need a read lock, we're not modifying any data, just
	 * flushing it to disk, so we can have other threads reading.
	 */
	be_rlock (s4);
	header = s4->map;

	if (header->sync_state != CLEAN) {
		map_sync (s4, s4->size);
		header->sync_state = CLEAN;
		map_sync (s4, pagesize());
	}
	be_runlock (s4);
}


/* Grow the database by the number of bytes given.
 * It will align to a pagesize boundry.
 */
static void grow_db (s4be_t *s4, int n)
{
	n = S4_ALIGN (n, pagesize ());

	map_unmap (s4);
	s4->size += n;
	map_file (s4);
}


/* Initilize a new database */
static void init_db (s4be_t *s4)
{
	header_t *header;

	s4->size = pagesize ();
	map_file (s4);

	header = s4->map;

	memset (header, -1, sizeof (header_t));
	header->sync_state = DIRTY;
	header->magic = S4_MAGIC;
	header->version = S4_VERSION;
	header->free = 0;
	s4be_sync (s4);
}


static int32_t make_chunks (s4be_t *be, int exp)
{
	int size = 2 << (exp + SMALLEST_CHUNK - 1);
	int32_t ret = be->size;
	int i;
	chunk_t *chunk;

	grow_db (be, size);

	for (i = 0; i < S4_ALIGN(size, pagesize ()); i += size) {
		chunk = S4_PNT (be, ret + i, chunk_t);
		chunk->next = ret + i + size;
	}

	chunk->next = -1;

	return ret;
}


/* Marks the database as dirty, it should be
 * marked as dirty BEFORE anything is written
 */
static void mark_dirty (s4be_t *s4)
{
	header_t *header = s4->map;

	/* This should be fairly cheap, a 4 byte write */
	if (header->sync_state != DIRTY) {
		header->sync_state = DIRTY;
		map_sync (s4, pagesize ());
	}
}

static void close_file (s4be_t *s4)
{
#ifdef _WIN32
	CloseHandle (s4->fd);
#else
	close (s4->fd);
#endif
}

/**
 * Open an S4 backend database.
 *
 * @param filename The file to open
 * @param open_flags Flags to use
 * @return A pointer to an s4be_t, or NULL on error.
 *
 */
s4be_t *s4be_open (const char *filename, int open_flags)
{
	s4be_t* s4 = malloc (sizeof(s4be_t));
	memset (s4, 0, sizeof (s4be_t));
#ifdef _WIN32
	int flags = OPEN_ALWAYS;
	if (open_flags & S4_NEW)
		flags = CREATE_NEW;
	if (open_flags & S4_EXISTS)
		flags = OPEN_EXISTING;

	s4->fd = CreateFile (filename, GENERIC_READ | GENERIC_WRITE,
			0, NULL, flags,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);

	if (s4->fd == INVALID_HANDLE_VALUE) {
		S4_ERROR ("Could not open %s", filename);
		free (s4);
		return NULL;
	}
#else
	struct stat stat_buf;
	int flags = O_RDWR;

	if (!(open_flags & S4_EXISTS))
		flags |= O_CREAT;
	if (open_flags & S4_NEW)
		flags |= O_EXCL;

	s4->fd = open (filename, flags, 0644);
	if (s4->fd == -1) {
		int err = -1;
		/* Try to convert some of the libc error codes to S4 error codes */
		switch (errno) {
			case EEXIST: err = S4E_EXISTS; break;
			case ENOENT: err = S4E_NOENT; break;
			default: err = S4E_OPEN; break;
		}
		s4_set_errno (err);
		S4_ERROR ("Could not open %s : %s", filename, strerror (errno));
		free (s4);
		return NULL;
	}
#endif

	g_static_rw_lock_init (&s4->rwlock);

#ifdef _WIN32
	s4->size = GetFileSize (s4->fd, NULL);
#else
	fstat(s4->fd, &stat_buf);
	s4->size = stat_buf.st_size;
#endif
	if (s4->size == 0) {
		init_db(s4);
	}
	else {
		map_file (s4);
		if (s4->map == NULL) {
			S4_ERROR ("Could not map %s", filename);
			close_file (s4);
			free (s4);
			return NULL;
		}

		header_t *hdr = s4->map;
		if (hdr->magic != S4_MAGIC) {
			S4_ERROR ("Wrong magic number in %s", filename);
			s4_set_errno (S4E_MAGIC);
			close_file (s4);
			free (s4);
			return NULL;
		}
	}

	return s4;
}

/**
 * Close an open s4 database
 *
 * @param s4 The database to close
 * @return 0 on success, anything else on error
 */
int s4be_close (s4be_t* s4)
{
	header_t *header = s4->map;

	S4_DBG ("Free %i", header->free);

	g_static_rw_lock_free (&s4->rwlock);
	s4be_sync (s4);
	map_unmap (s4);

	close_file (s4);
	free (s4);

	return 0;
}

int s4be_recover (s4be_t *old, s4be_t *rec)
{
	_st_recover (old, rec);
	_ip_recover (old, rec);

	return 1;
}

/**
 * Check that the database is consistent
 *
 * @param be The database to check
 * @param thorough Set to 0 to just do a quick check, 1 to do a full check.
 * @return 1 if the database is good, 0 otherwise
 */
int s4be_verify (s4be_t *be, int thorough)
{
	int ret = 1;
	header_t *hdr;

	if (thorough) {
		ret = _st_verify (be) & _ip_verify (be);
	}

	hdr = be->map;

	ret = ret && hdr != NULL && hdr->sync_state == CLEAN;

	return ret;
}


/**
 * Allocate atleast n bytes in the database and return the offset
 *
 * @param s4 The database handle
 * @param n The number of bytes needed
 * @return The offset into the database
 */
int32_t be_alloc (s4be_t* s4, int n)
{
	header_t* header = s4->map;
	chunk_t *chunk;
	int32_t ret;
	int l = log2 (n) - SMALLEST_CHUNK;

	if (l < 0)
		l = 0;

	if (l > (BIGGEST_CHUNK - SMALLEST_CHUNK)) {
		S4_ERROR ("Trying to allocate a bigger chunk than we allow!");
		return -1;
	}

	if (header->free_lists[l] == -1) {
		ret = make_chunks (s4, l);
		header = s4->map;
		header->free_lists[l] = ret;
		header->free += S4_ALIGN(n, pagesize ());
	}

	ret = header->free_lists[l];
	chunk = S4_PNT (s4, ret, chunk_t);
	header->free_lists[l] = chunk->next;

	header->free -= 2 << (l + SMALLEST_CHUNK - 1);

	return ret;
}


/**
 * Free the allocation at offset off
 *
 * @param s4 The database handle
 * @param off The allocation to free
 * @param size The size of the block
 */
void be_free(s4be_t* s4, int32_t off, int size)
{
	int32_t l = log2 (size) - SMALLEST_CHUNK;
	header_t *header = s4->map;
	chunk_t *chunk = S4_PNT (s4, off, chunk_t);

	if (l < 0)
		l = 0;

	chunk->next = header->free_lists[l];
	header->free_lists[l] = off;
	header->free += 2 << (l + SMALLEST_CHUNK - 1);
}

/* Locking routines
 * The database is protected by a multiple readers,
 * single writer lock. Use these functions to lock/unlock
 * the database.
 */
void be_rlock (s4be_t *s4)
{
	g_static_rw_lock_reader_lock (&s4->rwlock);
}

void be_runlock (s4be_t *s4)
{
	g_static_rw_lock_reader_unlock (&s4->rwlock);
}

void be_wlock (s4be_t *s4)
{
	g_static_rw_lock_writer_lock (&s4->rwlock);
	mark_dirty (s4);
}

void be_wunlock (s4be_t *s4)
{
	g_static_rw_lock_writer_unlock (&s4->rwlock);
}

/**
 * @}
 */
