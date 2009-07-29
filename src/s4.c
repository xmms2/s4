#include <s4.h>
#include "s4_be.h"
#include "log.h"
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

/**
 * Open an S4 database
 *
 * @param filename The name of the file containing the database
 * @param flags Define some options. S4_VERIFY will verify the file after
 * opening it, if it is corrupted and S4_RECOVER is set it will try to
 * recover it, otherwise it will return NULL. If S4_RECOVERY is set but
 * S4_VERIFY is not, it will try to recover the database without checking
 * if it is actually necessary. For the verify flags see s4_verify.
 * If S4_NEW makes sure it creates a new file, if he file already exists
 * it returns NULL.
 * @return A pointer to an s4_t, or NULL if something went wrong.
 */
s4_t *s4_open (const char *filename, int flags)
{
	s4be_t *be;
	s4_t *s4;
	int verified = 0;

	be = s4be_open (filename, flags);
	if (be == NULL) {
		return NULL;
	}

	if (flags & S4_VERIFY) {
		s4_t tmp;
		tmp.be = be;
		verified = s4_verify (&tmp, flags & S4_VERIFY_MASK);

		if (verified == 0 && !(flags & S4_RECOVER)) {
			s4be_close (be);
			return NULL;
		}
	}

	if (verified == 0 && (flags & S4_RECOVER)) {
		char buf[4096];
		s4_t tmp;
		strcpy (buf, filename);
		strcat (buf, ".rec");

		S4_INFO ("%s is corrupted, trying to recover..", filename);

		tmp.be = be;

		if (!s4_recover (&tmp, buf)) {
			s4be_close (be);
			return NULL;
		}

		s4be_close (be);

		g_unlink (filename);
		g_rename (buf, filename);

		be = s4be_open (filename, flags);

		if (be == NULL) {
			S4_ERROR ("Could not open the recovered database! This is bad..");
			return NULL;
		}
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

static int verify_refcount (s4_t *s4)
{
	return 1;
}

/**
 * Check the database for corruption
 *
 * @param s4 The database to check
 * @param flags Specifies what will be checked. Pass 0 to do a quick check,
 * add S4_VERIFY_THOROUGH if you want a thorough check of the backend and
 * add S4_VERFIY_REFCOUNT if you also want to check that the refcount in the
 * string-store is correct.
 *
 * @return 1 if everything is okay, 0 otherwise.
 */
int s4_verify (s4_t *s4, int flags) {
	int ret = s4be_verify (s4->be, flags & S4_VERIFY_THOROUGH);

	if (flags & S4_VERIFY_REFCOUNT) {
		ret = ret && verify_refcount (s4);
	}

	return ret;
}

int s4_recover (s4_t *s4, const char *name)
{
	s4be_t *rec;
	int ret = 1;

	if (g_file_test (name, G_FILE_TEST_EXISTS)) {
		S4_ERROR ("%s already exists, can't recover", name);
		return 0;
	}

	rec = s4be_open (name, S4_NEW);

	if (rec == NULL) {
		return 0;
	}

	ret = s4be_recover (s4->be, rec);
	s4be_close (rec);

	return ret;
}

/**
 * @}
 */
