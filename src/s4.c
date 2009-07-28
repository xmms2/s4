#include <s4.h>
#include "s4_be.h"
#include <stdlib.h>


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
