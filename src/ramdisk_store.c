// SPDX-License-Identifier: GPL-2.0-only

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/err.h>

#include "ramdisk_store.h"

enum rd_page_state {
	RD_PAGE_ZEROED = 0,
	RD_PAGE_RAW,
};

struct rd_page {
	enum rd_page_state state;
	void *data;
};

struct rd_store {
	uint64_t capacity;
};

struct rd_store *rd_new(uint64_t capacity)
{
	struct rd_store *result;

	result = kmalloc(sizeof(*result), GFP_KERNEL);

	if (result) {
		result->capacity = capacity;
		return result;
	}
	return ERR_PTR(-ENOMEM);
}

void rd_del(struct rd_store *store)
{
	kfree(store);
}

int rd_write(struct rd_store *store, uint64_t idx, const char *data)
{
	return 0;
}

static const char running_line[] = "A quick brown fox jumps over the lazy dog\n";

int rd_read(struct rd_store *store, uint64_t idx, char *buffer)
{
	for (size_t i = 0; i < RD_BLOCK_SIZE; ++i)
		buffer[i] = running_line[i % (sizeof(running_line) - 1)];
	return 0;
}

int rd_write_zeroes(struct rd_store *store, uint64_t idx)
{
	return 0;
}

uint64_t rd_get_capacity_sectors(struct rd_store *store)
{
	return store->capacity * RD_BLOCK_SECTORS;
}
