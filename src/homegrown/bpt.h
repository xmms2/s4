#ifndef _BPT_H
#define _BPT_H

#include <s4.h>

typedef struct bpt_record_St {
	int32_t key_a, val_a;
	int32_t key_b, val_b;
	int32_t src;
} bpt_record_t;

typedef struct bpt_St {
	int32_t root;
	int32_t leaves;
} bpt_t;


int bpt_insert (s4be_t *be, int32_t bpt, bpt_record_t record);
int bpt_remove (s4be_t *be, int32_t bpt, bpt_record_t record);
void bpt_foreach (s4be_t *be, int32_t bpt,
		void (*func)(bpt_record_t, void *userdata),
		void *userdata);
s4_set_t *bpt_find (s4be_t *be, int32_t bpt, bpt_record_t start, bpt_record_t stop);
int bpt_verify (s4be_t *be, int32_t bpt);

#endif /* _BPT_H */
