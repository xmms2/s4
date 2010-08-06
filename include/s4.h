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

#ifndef _S4_H
#define _S4_H

#include <stdint.h>

/* Flags */
#define S4_RECOVER          1 << 0
#define S4_VERIFY           1 << 1
#define S4_VERIFY_THOROUGH  1 << 2
#define S4_VERIFY_REFCOUNT  1 << 3
#define S4_VERIFY_MASK      (S4_VERIFY_THOROUGH | S4_VERIFY_REFCOUNT)
#define S4_NEW              1 << 4
#define S4_EXISTS           1 << 5
#define S4_SYNC_THREAD      1 << 6

/**
 * Error codes
 */
typedef enum {
	S4E_EXISTS, /**< Tried to open a database with S4_NEW, but the file already exists */
	S4E_NOENT, /**< Tried to open a database with S4_EXISTS, but it did not exist */
	S4E_OPEN, /**< fopen failed when trying to open the database. errno has more details */
	S4E_MAGIC, /**< Magic number was not correct. Probably not an S4 database */
	S4E_VERSION, /**< Version number was incorrect */
	S4E_INCONS, /**< Database is inconsistent. */
	S4E_LOGOPEN, /**< Could not open log file. See errno for more details */
	S4E_LOGREDO /**< Could not redo changes in the log. Probably corrupted log */
} s4_errno_t;

typedef enum {
	S4_CMP_BINARY, /**< Compare the values byte by byte */
	S4_CMP_CASELESS, /**< Compare casefolded versions of the strings */
	S4_CMP_COLLATE /**< Compare collated keys (taking locale into account) */
} s4_cmp_mode_t;

typedef struct s4_St s4_t;

/* val.c */
typedef struct s4_val_St s4_val_t;

s4_val_t *s4_val_new_string (const char *str);
s4_val_t *s4_val_new_int (int32_t i);
s4_val_t *s4_val_copy (const s4_val_t *val);
void s4_val_free (s4_val_t *val);

int s4_val_is_str (const s4_val_t *val);
int s4_val_is_int (const s4_val_t *val);
int s4_val_get_str (const s4_val_t *val, const char **str);
int s4_val_get_int (const s4_val_t *val, int32_t *i);
int s4_val_get_collated_str (const s4_val_t *val, const char **str);
int s4_val_get_casefolded_str (const s4_val_t *val, const char **str);

int s4_val_cmp (const s4_val_t *v1, const s4_val_t *v2, s4_cmp_mode_t mode);

/* s4.c */
s4_t *s4_open (const char *name, const char **indices, int flags);
int s4_close (s4_t *s4);
int s4_verify (s4_t *s4, int flags);
int s4_recover (s4_t *s4, const char *name);
void s4_sync (s4_t *s4);
s4_errno_t s4_errno (void);

int s4_add (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src);
int s4_del (s4_t *s4, const char *key_a, const s4_val_t *val_a,
		const char *key_b, const s4_val_t *val_b, const char *src);
void s4_foreach (s4_t *s4, void (*func)(s4_t *s4, const char *key, const s4_val_t *val_a,
			const char *key_b, const s4_val_t *val_b, const char *src, void *data), void *data);

/* sourcepref.c */
typedef struct s4_sourcepref_St s4_sourcepref_t;

s4_sourcepref_t *s4_sourcepref_create (const char **sourcepref);
void s4_sourcepref_unref (s4_sourcepref_t *sourcepref);
s4_sourcepref_t *s4_sourcepref_ref (s4_sourcepref_t *sp);
int s4_sourcepref_get_priority (s4_sourcepref_t *sp, const char *src);

/* cond.c */
typedef enum {
	S4_FILTER_EQUAL,
	S4_FILTER_GREATER,
	S4_FILTER_SMALLER,
	S4_FILTER_MATCH,
	S4_FILTER_EXISTS
} s4_filter_type_t;

typedef enum {
	S4_COMBINE_AND,
	S4_COMBINE_OR,
	S4_COMBINE_NOT
} s4_combine_type_t;

#define S4_COND_PARENT 1

typedef struct s4_condition_St s4_condition_t;
typedef int (*check_function_t)(s4_condition_t *cond, void *data);
typedef int (*filter_function_t)(const s4_val_t *value, s4_condition_t* data);
typedef int (*combine_function_t)(s4_condition_t *cond, check_function_t func, void *check_data);
typedef void (*free_func_t)(void*);

s4_condition_t *s4_cond_new_combiner (s4_combine_type_t type);
s4_condition_t *s4_cond_new_custom_combiner (combine_function_t func);
void s4_cond_add_operand (s4_condition_t *cond, s4_condition_t *op);
s4_condition_t *s4_cond_get_operand (s4_condition_t *cond, int op);

s4_condition_t *s4_cond_new_filter (s4_filter_type_t type, const char *key,
		s4_val_t *value, s4_sourcepref_t *sourcepref, s4_cmp_mode_t mode, int flags);
s4_condition_t *s4_cond_new_custom_filter (filter_function_t func, void *userdata,
		free_func_t free, const char *key, s4_sourcepref_t *sourcepref,
		s4_cmp_mode_t cmp_mode, int flags);

int s4_cond_is_filter (s4_condition_t *cond);
int s4_cond_is_combiner (s4_condition_t *cond);

int s4_cond_get_flags (s4_condition_t *cond);
const char *s4_cond_get_key (s4_condition_t *cond);
s4_sourcepref_t *s4_cond_get_sourcepref (s4_condition_t *cond);
int s4_cond_is_monotonic (s4_condition_t *cond);
void *s4_cond_get_funcdata (s4_condition_t *cond);
void s4_cond_update_key (s4_condition_t *cond, s4_t *s4);
int s4_cond_get_cmp_mode (s4_condition_t *cond);

void s4_cond_free (s4_condition_t *cond);
s4_condition_t *s4_cond_ref (s4_condition_t *cond);
void s4_cond_unref (s4_condition_t *cond);
filter_function_t s4_cond_get_filter_function (s4_condition_t *cond);
combine_function_t s4_cond_get_combine_function (s4_condition_t *cond);

/* fetchspec.c */
#define S4_FETCH_PARENT 1
#define S4_FETCH_DATA 2
typedef struct s4_fetchspec_St s4_fetchspec_t;

s4_fetchspec_t *s4_fetchspec_create (void);
void s4_fetchspec_add (s4_fetchspec_t *spec, const char *key, s4_sourcepref_t *sourcepref, int flags);
void s4_fetchspec_free (s4_fetchspec_t *spec);
s4_fetchspec_t *s4_fetchspec_ref (s4_fetchspec_t *spec);
void s4_fetchspec_unref (s4_fetchspec_t *spec);
int s4_fetchspec_size (s4_fetchspec_t *spec);
const char *s4_fetchspec_get_key (s4_fetchspec_t *spec, int index);
s4_sourcepref_t *s4_fetchspec_get_sourcepref (s4_fetchspec_t *spec, int index);
int s4_fetchspec_get_flags (s4_fetchspec_t *spec, int index);
void s4_fetchspec_update_key (s4_t *s4, s4_fetchspec_t *spec);

/* result.c */
typedef struct s4_result_St s4_result_t;

const s4_result_t *s4_result_next (const s4_result_t *res);
const char *s4_result_get_key (const s4_result_t *res);
const char *s4_result_get_src (const s4_result_t *res);
const s4_val_t *s4_result_get_val (const s4_result_t *res);

/* resultset.c */
typedef struct s4_resultrow_St s4_resultrow_t;
void s4_resultrow_set_col (s4_resultrow_t *row, int col_no, s4_result_t *col);
int s4_resultrow_get_col (const s4_resultrow_t *row, int col_no, const s4_result_t **col);

typedef struct s4_resultset_St s4_resultset_t;

s4_resultset_t *s4_resultset_create (int col_count);
void s4_resultset_add_row (s4_resultset_t *set, const s4_resultrow_t *row);
int s4_resultset_get_row (const s4_resultset_t *set, int row_no, const s4_resultrow_t **row);
const s4_result_t *s4_resultset_get_result (const s4_resultset_t *set, int row, int col);
int s4_resultset_get_colcount (const s4_resultset_t *set);
int s4_resultset_get_rowcount (const s4_resultset_t *set);
void s4_resultset_free (s4_resultset_t *set);
s4_resultset_t *s4_resultset_ref (s4_resultset_t *set);
void s4_resultset_unref (s4_resultset_t *set);
void s4_resultset_sort (const s4_resultset_t *set, const int *order);
void s4_resultset_shuffle (const s4_resultset_t *set);

/* query.c */
s4_resultset_t *s4_query (s4_t *s4, s4_fetchspec_t *fs, s4_condition_t *cond);

/* pattern.c */
typedef struct s4_pattern_St s4_pattern_t;

s4_pattern_t *s4_pattern_create (const char *pattern, int normalize);
int s4_pattern_match (s4_pattern_t *p, const s4_val_t *val);
void s4_pattern_free (s4_pattern_t *pattern);

/* string.c */
char *s4_string_collate (const char *str);
char *s4_string_casefold (const char *str);

#endif /* _S4_H */
