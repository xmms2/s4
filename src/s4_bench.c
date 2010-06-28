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

#include "s4.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gstdio.h>

#define ENTRIES 100000

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

	g_thread_init (NULL);
	log_init(G_LOG_LEVEL_MASK & ~G_LOG_LEVEL_DEBUG);
	g_get_current_time (&prev);

	s4 = s4_open (filename, NULL, S4_NEW);

	if (s4 == NULL) {
		fprintf (stderr, "Could not open %s\n", argv[1]);
		exit (1);
	}

	take_time ("s4_open took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_val_t *val;
		val = s4_val_new_int (i);
		s4_add (s4, "a", val, "b", val, "src");
		s4_val_free (val);
	}

	take_time ("s4be_ip_add took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_val_t *val;
		val = s4_val_new_int (i);
		s4_del (s4, "a", val, "b", val, "src");
		s4_val_free (val);
	}

	take_time ("s4be_ip_del took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_val_t *val;
		val = s4_val_new_int (i);
		s4_add (s4, "a", val, "b", val, "src");
		s4_val_free (val);
	}

	take_time ("s4be_ip_add took", &prev, &cur);

	for (i = 0; i < ENTRIES; i++) {
		s4_val_t *val;
		val = s4_val_new_int (i);
		s4_del (s4, "a", val, "b", val, "src");
		s4_val_free (val);
	}

	take_time ("s4be_ip_del took", &prev, &cur);

	for (i = ENTRIES; i > 0; i--) {
		s4_val_t *val;
		val = s4_val_new_int (i);
		s4_add (s4, "a", val, "b", val, "src");
		s4_val_free (val);
	}

	take_time ("s4be_ip_add (backwards) took", &prev, &cur);

	for (i = ENTRIES; i > 0; i--) {
		s4_val_t *val;
		val = s4_val_new_int (i);
		s4_del (s4, "a", val, "b", val, "src");
		s4_val_free (val);
	}

	take_time ("s4be_ip_del (backwards) took", &prev, &cur);

	s4_close (s4);

	take_time ("s4_close took", &prev, &cur);

	g_unlink (filename);
	g_unlink (g_strconcat (filename, ".log", NULL));

	take_time ("g_unlink took", &prev, &cur);

	return 0;
}
