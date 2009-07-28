#include "s4_be.h"
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
#define BIGGEST_CHUNK 12 /* 2^12 = 4096 */
#define SMALLEST_CHUNK 4 /* 2^4  = 16 */

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

	int32_t free_lists[BIGGEST_CHUNK - SMALLEST_CHUNK + 1];
	int free;
} header_t;

typedef struct chunk_St {
	int32_t next;
} chunk_t;


static inline long pagesize ()
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
void sync_db (s4be_t *s4)
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
	n = S4_ALIGN (n, pagesize());

	map_unmap (s4);
	s4->size += n;
	map_file (s4);
}


/* Initilize a new database */
static void init_db (s4be_t *s4)
{
	header_t *header;

	s4->size = pagesize();
	map_file (s4);

	header = s4->map;

	memset (header, -1, sizeof (header_t));
	header->sync_state = DIRTY;
	header->free = 0;
	sync_db(s4);
}


static int32_t make_chunks (s4be_t *be, int exp)
{
	int size = 2 << (exp + SMALLEST_CHUNK - 1);
	int32_t ret = be->size;
	int i;
	chunk_t *chunk;

	grow_db (be, 1);

	for (i = 0; i < pagesize(); i += size) {
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
		map_sync (s4, pagesize());
	}
}


/* The loop for the sync thread */
static gpointer sync_thread (gpointer be)
{
	s4be_t *s4 = be;
	GTimeVal tv;

	g_get_current_time (&tv);
	g_time_val_add (&tv, 60*1000);

	g_mutex_lock (s4->cond_mutex);

	while (!g_cond_timed_wait (s4->cond, s4->cond_mutex, &tv)) {
		sync_db (s4);
		g_get_current_time (&tv);
		g_time_val_add (&tv, 60*1000);
	}

	g_mutex_unlock (s4->cond_mutex);

	return NULL;
}


s4be_t *be_open (const char *filename, int recover)
{
	s4be_t* s4 = malloc (sizeof(s4be_t));
	memset (s4, 0, sizeof (s4be_t));
#ifdef _WIN32
	s4->fd = CreateFile (filename, GENERIC_READ | GENERIC_WRITE,
			0, NULL, (recover)?OPEN_EXISTING:OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);

	if (s4->fd == INVALID_HANDLE_VALUE) {
		free (s4);
		fprintf (stderr, "Could not open %s\n", filename);
		return NULL;
	}
#else
	struct stat stat_buf;
	int flags = O_RDWR | O_CREAT;

	if (recover) {
		flags |= O_EXCL;
	}

	s4->fd = open (filename, flags, 0644);
	if (s4->fd == -1) {
		free (s4);
		fprintf (stderr, "Could not open %s: %s\n", filename, strerror (errno));
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
			fprintf (stderr, "Could not map %s\n",
					filename);
			return NULL;
		}
	}

	return s4;
}


/**
 * Open an s4 database
 *
 * @param filename The file to open
 * @return A pointer to an s4 structure, or NULL on error
 */
s4be_t *s4be_open (const char* filename)
{
	s4be_t *ret, *rec;
	header_t *header;

	ret = be_open (filename, 0);

	if (ret == NULL)
		return NULL;

	header = ret->map;

	if (header->sync_state != CLEAN) {
		char buf[4096];
		strcpy (buf, filename);
		strcat (buf, ".rec");

		rec = be_open (buf, 1);

		if (rec == NULL)
			return NULL;

		_st_recover (ret, rec);
		_ip_recover (ret, rec);

		s4be_close (ret);
		s4be_close (rec);

		g_unlink (filename);
		g_rename (buf, filename);

		ret = be_open (filename, 0);
	}

	ret->cond = g_cond_new ();
	ret->cond_mutex = g_mutex_new ();

	ret->s_thread = g_thread_create (sync_thread, ret, TRUE, NULL);
	return ret;
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

	printf ("free %i\n", header->free);

	if (s4->cond_mutex != NULL) {
		g_mutex_lock (s4->cond_mutex);
		g_cond_signal (s4->cond);
		g_mutex_unlock (s4->cond_mutex);

		g_thread_join (s4->s_thread);

		g_mutex_free (s4->cond_mutex);
		g_cond_free (s4->cond);
	}

	g_static_rw_lock_free (&s4->rwlock);

	sync_db (s4);
	map_unmap (s4);

#ifdef _WIN32
	CloseHandle (s4->fd);
#else
	close (s4->fd);
#endif
	free (s4);

	return 0;
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

	if (l >= BIGGEST_CHUNK) {
		printf ("trying to allocate a bigger chunk than the biggest we allow!\n");
		return -1;
	}

	if (header->free_lists[l] == -1) {
		ret = make_chunks (s4, l);
		header = s4->map;
		header->free_lists[l] = ret;
		header->free += pagesize();
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
