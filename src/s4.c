#include <s4.h>
#include "s4_be.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>


/**
 *
 * @defgroup S4 S4
 * @brief A database backend for XMMS2
 *
 * @{
 */

s4_t *s4_open (const char *filename)
{
	s4be_t *be;
	s4_t *s4;

	be = s4be_open (filename);
	if (be == NULL)
		return NULL;

	if (s4be_verify (be, 0) == 0) {
		char buf[4096];
		s4be_t *rec;
		strcpy (buf, filename);
		strcat (buf, ".rec");

		if (g_file_test (buf, G_FILE_TEST_EXISTS)) {
			return NULL;
		}

		rec = s4be_open (buf);

		if (rec == NULL)
			return NULL;

		s4be_recover (be, rec);

		s4be_close (be);
		s4be_close (rec);

		g_unlink (filename);
		g_rename (buf, filename);

		be = s4be_open (filename);
	}

	s4 = malloc (sizeof (s4_t));
	s4->be = be;

	return s4;
}


int s4_close (s4_t *s4)
{
	int ret = s4be_close (s4->be);

	free (s4);

	return ret;
}

/**
 * @}
 */
