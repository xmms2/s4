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

#include <glib.h>
#include <stdlib.h>
#include <s4.h>
#include "s4_priv.h"

/**
 *
 * @defgroup UUID UUID
 * @ingroup S4
 * @brief Functions dealing with UUID
 *
 * Every S4 database is assigned a random UUID. This can be used
 * to identify a database without using the filename.
 *
 * @{
 */

/**
 * Creates a new random UUID and saves it in the array passed
 *
 * @param uuid The array to save the new UUID to
 */
void s4_create_uuid (unsigned char uuid[16])
{
	int i;
	uint32_t *rnd = (uint32_t *)uuid;

	for (i = 0; i < 4; i++) {
		rnd[i] = g_random_int ();
	}

	/* Set type to random */
	uuid[6] &= 0xf;
	uuid[6] |= 0x40;
	/* Set variant */
	uuid[8] &= 0x3f;
	uuid[8] |= 0x80;
}

/**
 * Gets the UUID of a S4 database
 *
 * @param s4 The database to find the UUID of
 * @param uuid An array to save the UUID in
 */
void s4_get_uuid (s4_t *s4, unsigned char uuid[16])
{
	int i;
	for (i = 0; i < 16; i++) {
		uuid[i] = s4->uuid[i];
	}
}

#define HEX_TO_CHAR(c) ((c)<10?(c) + '0':(c) + 'a' - 10)

/**
 * Gets the UUID-string of a S4 database
 *
 * @param s4 The database to find the UUID of
 * @return A NUL-terminated string with the UUID. Must be freed
 */
char *s4_get_uuid_string (s4_t* s4)
{
	int i, j;
	char *uuid_str = malloc (37);

	for (i = 0, j = 0; i < 16; i++) {
		uuid_str[j++] = HEX_TO_CHAR (s4->uuid[i] >> 4);
		uuid_str[j++] = HEX_TO_CHAR (s4->uuid[i] & 0xf);

		if (i == 3 || i == 5 || i == 7 || i == 9) {
			uuid_str[j++] = '-';
		}
	}
	uuid_str[j] = '\0';

	return uuid_str;
}

/**
 * @}
 */
