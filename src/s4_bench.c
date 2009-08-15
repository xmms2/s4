#include "s4.h"
#include "s4_be.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

#define ENTRIES 1000000

long timediff (GTimeVal *prev, GTimeVal *cur)
{
	long ret;

   	ret = (cur->tv_sec - prev->tv_sec) * G_USEC_PER_SEC;
	ret += cur->tv_usec - prev->tv_usec;

	return ret;
}

void take_time (const char *message, GTimeVal *prev, GTimeVal *cur)
{
	g_get_current_time (cur);

	printf ("%s %li.%.6li sec\n", message,
			timediff (prev, cur) / G_USEC_PER_SEC,
			timediff (prev, cur) % G_USEC_PER_SEC);

	g_get_current_time (prev);
}

int main (int argc, char *argv[])
{
	s4_t *s4;
	int i;
	char *filename = tmpnam (NULL);
	GTimeVal cur, prev;

	log_init(G_LOG_LEVEL_MASK & ~G_LOG_LEVEL_DEBUG);
	g_get_current_time (&prev);

	s4 = s4_open (filename, S4_NEW);

	if (s4 == NULL) {
		fprintf (stderr, "Could not open %s\n", argv[1]);
		exit (1);
	}

	take_time ("s4_open took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_entry_t entry;
		entry.key_i = entry.val_i = entry.src_i = i;

		s4be_ip_add (s4->be, &entry, &entry);
	}

	take_time ("s4be_ip_add took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_entry_t entry;
		entry.key_i = entry.val_i = entry.src_i = i;

		s4be_ip_del (s4->be, &entry, &entry);
	}

	take_time ("s4be_ip_del took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_entry_t entry;
		entry.key_i = entry.val_i = entry.src_i = i;

		s4be_ip_add (s4->be, &entry, &entry);
	}

	take_time ("s4be_ip_add took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_entry_t entry;
		entry.key_i = entry.val_i = entry.src_i = i;

		s4be_ip_del (s4->be, &entry, &entry);
	}

	take_time ("s4be_ip_del took", &prev, &cur);

	for (i = ENTRIES; i > 0; i--) {
		s4_entry_t entry;
		entry.key_i = entry.val_i = entry.src_i = i;

		s4be_ip_add (s4->be, &entry, &entry);
	}

	take_time ("s4be_ip_add (backwards) took", &prev, &cur);

	for (i = ENTRIES; i > 0; i--) {
		s4_entry_t entry;
		entry.key_i = entry.val_i = entry.src_i = i;

		s4be_ip_del (s4->be, &entry, &entry);
	}

	take_time ("s4be_ip_del (backwards) took", &prev, &cur);

	s4_close (s4);

	take_time ("s4_close took", &prev, &cur);

	g_unlink (filename);

	take_time ("g_unlink took", &prev, &cur);

	return 0;
}
