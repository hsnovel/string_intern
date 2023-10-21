#ifndef STRING_INTERN_H
#define STRING_INTERN_H

#include <stddef.h>
typedef struct
{
	void *data;
	size_t cap;
	size_t item_size;
} mempool;

typedef struct {
	struct strarr *string_table;
	mempool *dispatch_table;
	int num_table;
	int num_used_table;
} intern_table;

int intern_init(intern_table *table);
char *intern_str(intern_table *table, char *ptr, size_t len);

#endif
