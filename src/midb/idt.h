#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>

#define IDT_TYPE int32_t
#define IDT_TYPE_BITS 32

typedef struct idt_St idt_t;


idt_t*   idt_create (void);
void     idt_destroy (idt_t *idt);
IDT_TYPE idt_insert (idt_t *idt, void *data);
void*    idt_replace (idt_t *idt, IDT_TYPE id, void *new_data);
void*    idt_get (idt_t *idt, IDT_TYPE id);

#endif
