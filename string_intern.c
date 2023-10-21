#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdint.h>

#include "string_intern.h"

typedef uint32_t u32;
typedef size_t usize;
typedef long long ssize;

#define STRARR_INITIAL_STR_COUNT 16

#ifndef STRARR_GROW_AMOUNT
#define STRARR_GROW_AMOUNT 2
#endif

#define ARRAY_INITIAL_CAP 25

struct array {
	usize cap;		/* in bytes */
	usize cursor;		/* counter in numbers */
	usize itemsize;	/* in bytes */
	unsigned char *data;	/* actualy data */
};

struct strarr {
	char *data;
	struct array offsets;

	usize data_cap;
	usize data_size;
};

/*
 * Array implementation
 */
int array_init(struct array *array, usize size, usize num_alloc)
{
	num_alloc = (num_alloc == 0) ? (ARRAY_INITIAL_CAP * size) : (num_alloc * size);
	array->cursor = 0;
	array->itemsize = size;
	array->cap = num_alloc;
	if ((array->data = malloc(num_alloc)) == NULL)
		return -1;

	return 0;
}

usize array_push(struct array *array, void *data)
{
	if (array->cap <= (array->cursor * array->itemsize)) {
		usize newcap = array->cap * 2;
		unsigned char *tmp = realloc(array->data, newcap);
		if (tmp == NULL) {
			tmp = malloc(newcap);
			if (tmp == NULL)
				return -1;
			memcpy(tmp, array->data, array->cursor * array->itemsize);
			free(array->data);
		}
		array->cap = newcap;
		array->data = tmp;
	}
	memcpy(array->data + (array->cursor * array->itemsize), data, array->itemsize);
	return array->cursor++;
}

void *array_get(struct array *array, usize cursor)
{
	assert((usize)array->cursor >= (usize)cursor);
	return array->data + (cursor * array->itemsize);
}

void array_free(struct array *array)
{
	free(array->data);
	array->cap = 0;
	array->itemsize = 0;
	array->cursor = 0;
}

void array_clear(struct array *array)
{
	memset(array->data, 0, array->cursor * array->itemsize);
	array->cursor = 0;
}

void array_overwrite(struct array *array)
{
	array->cursor = 0;
}

usize array_size(struct array *array)
{
	return array->cursor;
}

#ifndef STRARR_AVERAGE_STRING_SIZE
#define STRARR_AVERAGE_STRING_SIZE 32
#endif

/*
 * String Array implementation
 */
int strarr_init(struct strarr *arr, usize cap)
{
	arr->data_cap = STRARR_AVERAGE_STRING_SIZE * cap;
	arr->data_size = 0;
	arr->data = calloc(arr->data_cap , sizeof(char));

	if (arr->data == NULL)
		return -1;

	if (array_init(&arr->offsets, sizeof(usize), 0)) {
		free(arr->data);
		return -1;
	}

	return 0;
}

int strarr_push(struct strarr *arr, char* str, usize len)
{
	char *ptr;
	int index;
	len++; /* \0 character */

	if (arr->data_size >= arr->data_cap) {
		usize newcap = (arr->data_cap * STRARR_GROW_AMOUNT) + len;
		char *tmp = malloc(newcap);

		memcpy(tmp, arr->data, arr->data_size);
		if (tmp == NULL)
			return 0;

		arr->data_cap = newcap;
		arr->data = tmp;
	}

	ptr = arr->data + arr->data_size;
	memcpy(ptr, str, len);

	index = array_push(&arr->offsets, &arr->data_size);

	arr->data_size += len;
	return index;
}

void strarr_free(struct strarr *arr)
{
	free(arr->data);
}

char *strarr_get(struct strarr *arr, int index)
{
	if (index > array_size(&arr->offsets))
		return NULL;
	usize offset = *((usize*)array_get(&arr->offsets, index));
	return arr->data + offset;
}

#define MEMPOOL_SIZE 256

int mempool_init(mempool *pool, usize item_size, usize initial_cap)
{
	int i;
	pool->item_size = item_size;
	pool->cap = (initial_cap == 0) ? MEMPOOL_SIZE : initial_cap * pool->item_size;
	pool->data = malloc(pool->cap);

	if (pool->data == NULL)
		return -1;

	for (i = 0; i < pool->cap / sizeof(ssize); i++)
		*((ssize*)pool->data + i) = -1;

	return 0;
}

void mempool_push(mempool *pool, void *data, usize index)
{
	if (index * pool->item_size <= pool->cap) {
		usize newsize = pool->cap * 2;
		void *tmp = realloc(pool->data, newsize);
		int i;
		int base_address;
		if (tmp == NULL) {
			tmp = malloc(newsize);
			memcpy(tmp, pool->data, newsize);
		}
		pool->data = tmp;
		pool->cap = newsize;

		/* Initialize rest of the memory to -1 */
		for (i = newsize / 2; i < newsize; i++) {
			((ssize*)pool->data)[i] = -1;
		}
	}
	memcpy(pool->data + index * pool->item_size, data, pool->item_size);
}

void *mempool_get(mempool *pool, usize index)
{
	assert(index < pool->cap / pool->item_size);
	return pool->data + index * pool->item_size;
}

usize mempool_cap(mempool *pool)
{
	return pool-> cap;
}

void mempool_free(mempool *pool)
{
	free(pool->data);
	pool->cap = 0;
	pool->item_size = 0;
}

uint32_t fnv1a_hash(uint8_t* key, usize len) {
	uint32_t FNV_offset_basis = 2166136261;
	uint32_t FNV_prime = 16777619;
	usize i;
	uint32_t hash_value = FNV_offset_basis;

	for (i = 0; i < len; ++i) {
		hash_value ^= key[i];
		hash_value *= FNV_prime;
	}

	return hash_value;
}

#define DISPATCH_TABLE_INITIAL_SIZE 32

/*
 * Having a pointer to multiple string tables
 * are almost as stupid as using linked lists
 * for collusion handling. What should happen
 * instead is to have "virtual" tables combined
 * into a single malloc call. Then going between
 * tables is as simple as adding or substracting
 * table size to the base address of the previous
 * table.
 *
 * It would be even better to have virtual tables
 * being indexed from the base address. Having an
 * array of of indexes from the base address to
 * prevent multiple calls to malloc would increase
 * the stability of the program even further.
 *
 * The problem is that it would require it's own
 * engineering problems. Threashold would be either
 * in favor of the memory usage of cpu performance.
 *
 * It is still unclear if such thing would be done,
 * it would be fast.
 * I hate doing benchmarks and I don't want to spend
 * time on something that I don't know if it's worth
 * working on..
 *
 * Anyways why are you still reading this?
 */

#define INTERN_TABLE_ENTRY_SIZE 32
#define INTERN_TABLE_DEFAULT_NUMBER_TABLE_SIZE 8

int intern_init(intern_table *table)
{
	int i;

	table->num_used_table = 1;
	table->num_table = INTERN_TABLE_DEFAULT_NUMBER_TABLE_SIZE;
	table->dispatch_table = malloc(table->num_table * sizeof(mempool));

	for (i = 0; i < table->num_table ; i++) {
		if (mempool_init(&table->dispatch_table[i], sizeof(ssize), INTERN_TABLE_ENTRY_SIZE)) {
			for  (; i > 0; --i) {
				mempool_free(&table->dispatch_table[i]);
			}
		}
	}

	table->string_table = malloc(table->num_table * sizeof(struct strarr));
	for (i = 0; i < table->num_table; i++) {
		if (strarr_init(&table->string_table[i], INTERN_TABLE_ENTRY_SIZE)) {
			for (; i > 0; --i) {
				strarr_free(&table->string_table[i]);
				return -1;
			}
		}
	}

	return 0;
}

/*
 * There are definitely better ways of doing this.
 * I cannot say which way is the fastest untill I clean
 * up this code and benchmark it. I don't know if I ever
 * will implement some other methods. I'm sure there is a
 * faster one on the internet tough.
 *
 * Also if there is too many hash collusions for a single
 * hash this will eat a lot of memory. You should use
 * a collusion safe hash :)
 */
char *intern_str(intern_table *table, char *ptr, usize len)
{
	int i;
	char *result;
	for (i = 0; i < table->num_used_table; i++) {
		int index_into_string_array;
		usize array_index;
		usize hash_index;
		usize array_cap;
		char *tmp;
		usize index;

		if (ptr == NULL) return NULL;
		if (*ptr == 0)   return NULL;

		array_cap = mempool_cap(&table->dispatch_table[i]) / table->dispatch_table[i].item_size;
		hash_index = fnv1a_hash(ptr, len);

		/* Handle floating point exception */
		if (hash_index != 0 && array_cap != 0)
			hash_index %= array_cap;

		/* NOTE: Do not forget to -1 initialize whenever you grow mempool */
		index = *(ssize*)mempool_get(&table->dispatch_table[i], hash_index);


		/* Push it for the first time? */
		if (index == -1) {
			usize real_index;
			real_index = strarr_push(&table->string_table[i], ptr, len);
			mempool_push(&table->dispatch_table[i], &real_index, hash_index);
			result = strarr_get(&table->string_table[i], real_index);
			break;
		}
		/* Either strings are the same or they have hash collusion*/
		else {
			index_into_string_array = *(usize*)mempool_get(&table->dispatch_table[i], hash_index);

			tmp = strarr_get(&table->string_table[i], index_into_string_array);

			/* No collusion, string are equal*/
			if (strcmp(tmp, ptr) == 0) {
				result = strarr_get(&table->string_table[i], index_into_string_array);
				break;
			} else {
				if (i == table->num_used_table - 1)
					table->num_used_table++;
				continue;
			}
		}
	}

	return result;
}
