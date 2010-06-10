#ifndef _QUERY_H
#define _QUERY_H

typedef struct s4_condition_St {
	s4_condition_type_t type;

	union {
		struct s4_condition_St **operands;
		struct {
			int32_t key;
			int (*func)(int32_t, void*);
			void *funcdata;
			GHashTable *sourcepref;
		} filter;
	} cond;
} s4_condition_t;

#endif
