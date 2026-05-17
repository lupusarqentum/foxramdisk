// SPDX-License-Identifier: GPL-2.0-only

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "ramdisk_store.h"

enum rd_block_state {
	RD_BLOCK_ZEROED = 0,
	RD_BLOCK_RAW,
};

struct rd_block {
	void *data;
	rwlock_t lock;
	enum rd_block_state state;
};

struct rd_store {
	uint64_t blocks_count;
	struct rd_block *blocks;
};

struct rd_store *rd_new(uint64_t blocks_count)
{
	if (blocks_count == 0)
		return ERR_PTR(-EINVAL);

	uint64_t blocks_size = sizeof(struct rd_block) * blocks_count;

	struct rd_store *result;

	result = kmalloc(sizeof(*result), GFP_KERNEL);

	if (!result)
		return ERR_PTR(-ENOMEM);

	result->blocks_count = blocks_count;
	result->blocks = vmalloc(blocks_size);
	if (!result->blocks) {
		kfree(result);
		return ERR_PTR(-ENOMEM);
	}

	memset(result->blocks, 0, blocks_size);
	for (uint64_t i = 0; i < blocks_count; ++i) {
		result->blocks[i].data = NULL;
		rwlock_init(&result->blocks[i].lock);
		result->blocks[i].state = RD_BLOCK_ZEROED;
	}

	return result;
}

void rd_del(struct rd_store *store)
{
	for (uint64_t i = 0; i < store->blocks_count; ++i) {
		if (store->blocks[i].state == RD_BLOCK_RAW)
			kfree(store->blocks[i].data);
	}
	vfree(store->blocks);
	kfree(store);
}

int rd_write(struct rd_store *store, uint64_t idx, const char *data)
{
	if (unlikely(idx >= store->blocks_count)) {
		pr_err("ramdisk: block index out of range");
		return -EINVAL;
	}

	char *old_data = NULL;
	char *ndata = kmalloc(RD_BLOCK_SIZE, GFP_KERNEL);

	if (!ndata)
		return -ENOMEM;

	memcpy(ndata, data, RD_BLOCK_SIZE);

	write_lock(&store->blocks[idx].lock);
	if (store->blocks[idx].state == RD_BLOCK_ZEROED) {
		store->blocks[idx].state = RD_BLOCK_RAW;
		store->blocks[idx].data = ndata;
	} else if (store->blocks[idx].state == RD_BLOCK_RAW) {
		old_data = store->blocks[idx].data;
		store->blocks[idx].data = ndata;
	}
	write_unlock(&store->blocks[idx].lock);
	kfree(old_data);
	return 0;
}

int rd_read(struct rd_store *store, uint64_t idx, char *buffer)
{
	if (unlikely(idx >= store->blocks_count)) {
		pr_err("ramdisk: block index out of range");
		return -EINVAL;
	}

	read_lock(&store->blocks[idx].lock);
	if (store->blocks[idx].state == RD_BLOCK_ZEROED)
		memset(buffer, 0, RD_BLOCK_SIZE);
	else if (store->blocks[idx].state == RD_BLOCK_RAW)
		memcpy(buffer, store->blocks[idx].data, RD_BLOCK_SIZE);
	read_unlock(&store->blocks[idx].lock);
	return 0;
}

int rd_write_zeroes(struct rd_store *store, uint64_t idx)
{
	if (unlikely(idx >= store->blocks_count)) {
		pr_err("ramdisk: block index out of range");
		return -EINVAL;
	}

	char *old_data = NULL;

	write_lock(&store->blocks[idx].lock);
	if (store->blocks[idx].state == RD_BLOCK_RAW) {
		old_data = store->blocks[idx].data;
		store->blocks[idx].data = NULL;
		store->blocks[idx].state = RD_BLOCK_ZEROED;
	}
	write_unlock(&store->blocks[idx].lock);
	kfree(old_data);
	return 0;
}

uint64_t rd_get_capacity_sectors(struct rd_store *store)
{
	return store->blocks_count * RD_BLOCK_SECTORS;
}
