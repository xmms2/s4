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

#include <glib.h>
#include <stdio.h>

GLogLevelFlags log_level;

void log_handler (const gchar *log_domain,
		GLogLevelFlags log_lev,
		const gchar *message,
		gpointer user_data)
{
	if (!(log_level & log_lev))
		return;

	printf ("%s\n", message);
}

void log_init (GLogLevelFlags log_lev)
{
	log_level = log_lev;
	g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_handler, NULL);
}
