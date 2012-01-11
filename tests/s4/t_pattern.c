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

SETUP (Pattern) {
	return 0;
}

CLEANUP () {
	return 0;
}

static int match_str (s4_pattern_t *pat, const char *str)
{
	s4_val_t *val = s4_val_new_string (str);
	int ret = s4_pattern_match (pat, val);
	s4_val_free (val);
	return ret;
}

static int match_int (s4_pattern_t *pat, int32_t i)
{
	s4_val_t *val = s4_val_new_int (i);
	int ret = s4_pattern_match (pat, val);
	s4_val_free (val);
	return ret;
}

CASE (test_pattern) {
	s4_pattern_t *p;

	p = s4_pattern_create ("boring", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_TRUE (match_str (p, "boring"));
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "boringer"));
	CU_ASSERT_FALSE (match_str (p, "very boring"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_TRUE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "boring"));
	CU_ASSERT_FALSE (match_str (p, "boringer"));
	CU_ASSERT_FALSE (match_str (p, "very boring"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("boring*", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_TRUE (match_str (p, "boring"));
	CU_ASSERT_TRUE (match_str (p, "boringer"));
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "very boring"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("bo*ing", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_TRUE (match_str (p, "boring"));
	CU_ASSERT_TRUE (match_str (p, "booorrring"));
	CU_ASSERT_TRUE (match_str (p, "boasdfing"));
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "boringer"));
	CU_ASSERT_FALSE (match_str (p, "bori"));
	CU_ASSERT_FALSE (match_str (p, "very boring"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("*boring", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_TRUE (match_str (p, "boring"));
	CU_ASSERT_TRUE (match_str (p, "aaboring"));
	CU_ASSERT_TRUE (match_str (p, "asdfboring"));
	CU_ASSERT_TRUE (match_str (p, "very boring"));
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "boringer"));
	CU_ASSERT_FALSE (match_str (p, "bori"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("bo?ing", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_TRUE (match_str (p, "boring"));
	CU_ASSERT_TRUE (match_str (p, "boming"));
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "boringer"));
	CU_ASSERT_FALSE (match_str (p, "very boring"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("*a*", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_TRUE (match_str (p, "a"));
	CU_ASSERT_TRUE (match_str (p, "ab"));
	CU_ASSERT_TRUE (match_str (p, "ba"));
	CU_ASSERT_TRUE (match_str (p, "bbaabb"));
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "boring"));
	CU_ASSERT_FALSE (match_str (p, "bb"));
	CU_ASSERT_FALSE (match_str (p, "cc"));
	s4_pattern_free (p);

	p = s4_pattern_create ("12?4", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "boring"));
	CU_ASSERT_TRUE (match_str (p, "1234"));
	CU_ASSERT_TRUE (match_str (p, "1294"));
	CU_ASSERT_FALSE (match_str (p, "12345"));
	CU_ASSERT_FALSE (match_str (p, "01234"));
	CU_ASSERT_TRUE (match_int (p, 1234));
	CU_ASSERT_TRUE (match_int (p, 1294));
	CU_ASSERT_FALSE (match_int (p, 12345));
	CU_ASSERT_FALSE (match_int (p, 124));
	s4_pattern_free (p);

	p = s4_pattern_create ("*a*b?d*e*", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_TRUE (match_str (p, "abcde"));
	CU_ASSERT_FALSE (match_str (p, "boring"));
	CU_ASSERT_TRUE (match_str (p, "..abcde"));
	CU_ASSERT_TRUE (match_str (p, "abcde.."));
	CU_ASSERT_TRUE (match_str (p, "a..bcde"));
	CU_ASSERT_TRUE (match_str (p, "abcd..e"));
	CU_ASSERT_TRUE (match_str (p, "..a..bcd..e.."));
	CU_ASSERT_TRUE (match_str (p, "..a..b.d..e.."));
	CU_ASSERT_FALSE (match_str (p, "1234"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("123", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "1234"));
	CU_ASSERT_TRUE (match_str (p, "123"));
	CU_ASSERT_FALSE (match_str (p, "0123"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	CU_ASSERT_TRUE (match_int (p, 123));
	CU_ASSERT_FALSE (match_int (p, 1123));
	CU_ASSERT_FALSE (match_int (p, -123));
	s4_pattern_free (p);

	p = s4_pattern_create ("-123", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "1234"));
	CU_ASSERT_TRUE (match_str (p, "-123"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	CU_ASSERT_FALSE (match_int (p, 1123));
	CU_ASSERT_FALSE (match_int (p, 123));
	CU_ASSERT_TRUE (match_int (p, -123));
	CU_ASSERT_FALSE (match_int (p, -1234));
	s4_pattern_free (p);

	p = s4_pattern_create ("?123", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_str (p, "1234"));
	CU_ASSERT_TRUE (match_str (p, "0123"));
	CU_ASSERT_FALSE (match_int (p, 1234));
	CU_ASSERT_TRUE (match_int (p, 1123));
	CU_ASSERT_TRUE (match_int (p, -123));
	s4_pattern_free (p);

	p = s4_pattern_create ("12*34", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_TRUE (match_str (p, "1234"));
	CU_ASSERT_TRUE (match_str (p, "1287634"));
	CU_ASSERT_FALSE (match_str (p, "0123"));
	CU_ASSERT_TRUE (match_int (p, 1234));
	CU_ASSERT_TRUE (match_int (p, 1298734));
	CU_ASSERT_FALSE (match_int (p, 1123));
	CU_ASSERT_FALSE (match_int (p, -123));
	CU_ASSERT_FALSE (match_int (p, 123));
	CU_ASSERT_FALSE (match_int (p, -321));
	s4_pattern_free (p);

	p = s4_pattern_create ("*1*2*3*", 0);
	CU_ASSERT_PTR_NOT_NULL (p);
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_TRUE (match_str (p, "1234"));
	CU_ASSERT_TRUE (match_str (p, "0123"));
	CU_ASSERT_TRUE (match_int (p, 1234));
	CU_ASSERT_TRUE (match_int (p, 1123));
	CU_ASSERT_TRUE (match_int (p, -123));
	CU_ASSERT_TRUE (match_int (p, 123));
	CU_ASSERT_FALSE (match_str (p, ""));
	CU_ASSERT_FALSE (match_int (p, 1));
	CU_ASSERT_FALSE (match_int (p, 12));
	CU_ASSERT_FALSE (match_int (p, -321));
	s4_pattern_free (p);
}
