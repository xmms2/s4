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

#include "xcu.h"
#include "s4.h"
#include <glib.h>

SETUP (Sourcepref) {
	if (!g_thread_get_initialized ())
		g_thread_init (NULL);

	return 0;
}

CLEANUP () {
	return 0;
}

CASE (test_sourcepref) {
	const char *sources[] = {"a*", "b*", "c*", "d*", NULL};
	s4_sourcepref_t *sp = s4_sourcepref_create (sources);

	CU_ASSERT_EQUAL (s4_sourcepref_get_priority (sp, "asdf"), 0);
	CU_ASSERT_EQUAL (s4_sourcepref_get_priority (sp, "buhu"), 1);
	CU_ASSERT_EQUAL (s4_sourcepref_get_priority (sp, "asdf"), 0);
	CU_ASSERT_EQUAL (s4_sourcepref_get_priority (sp, "cake"), 2);
	CU_ASSERT_EQUAL (s4_sourcepref_get_priority (sp, "dazzle"), 3);
	CU_ASSERT_EQUAL (s4_sourcepref_get_priority (sp, "something else"), INT_MAX);
	CU_ASSERT_EQUAL (s4_sourcepref_get_priority (NULL, "something else"), 0);

	s4_sourcepref_free (sp);
}
