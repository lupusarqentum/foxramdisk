// SPDX-License-Identifier: GPL-2.0-only

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
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

struct rd_stats {
	atomic64_t total_bytes_discarded;
	atomic64_t total_bytes_read;
	atomic64_t total_bytes_written;
	atomic64_t zeroed_blocks_count;
	atomic64_t raw_blocks_count;
};

struct rd_store {
	uint64_t blocks_count;
	struct rd_block *blocks;
	struct rd_stats stats;
};

struct rd_store *rd_new(uint64_t blocks_count)
{
	if (blocks_count == 0)
		return ERR_PTR(-EINVAL);

	struct rd_store *result;

	result = kmalloc(sizeof(*result), GFP_KERNEL);

	if (!result)
		return ERR_PTR(-ENOMEM);

	result->blocks_count = blocks_count;
	result->blocks = vmalloc_array(blocks_count, sizeof(struct rd_block));
	if (!result->blocks) {
		kfree(result);
		return ERR_PTR(-ENOMEM);
	}

	for (uint64_t i = 0; i < blocks_count; ++i) {
		result->blocks[i].data = NULL;
		rwlock_init(&result->blocks[i].lock);
		result->blocks[i].state = RD_BLOCK_ZEROED;
	}

	atomic64_set(&result->stats.total_bytes_discarded, 0);
	atomic64_set(&result->stats.total_bytes_read, 0);
	atomic64_set(&result->stats.total_bytes_written, 0);
	atomic64_set(&result->stats.raw_blocks_count, 0);
	atomic64_set(&result->stats.zeroed_blocks_count, (int64_t)blocks_count);

	return result;
}

void rd_del(struct rd_store *store)
{
	if (unlikely(!store))
		return;

	for (uint64_t i = 0; i < store->blocks_count; ++i) {
		if (store->blocks[i].state == RD_BLOCK_RAW)
			kfree(store->blocks[i].data);
	}
	vfree(store->blocks);
	kfree(store);
}

int rd_write(struct rd_store *store, uint64_t idx, const char *data)
{
	if (unlikely(!store))
		return -EINVAL;

	if (unlikely(idx >= store->blocks_count)) {
		pr_err("ramdisk: block index out of range");
		return -EINVAL;
	}

	char *old_data = NULL;
	char *ndata = kmalloc(RD_BLOCK_SIZE, GFP_KERNEL);
	enum rd_block_state old_state;

	if (!ndata)
		return -ENOMEM;

	memcpy(ndata, data, RD_BLOCK_SIZE);

	write_lock(&store->blocks[idx].lock);
	if (store->blocks[idx].state == RD_BLOCK_ZEROED) {
		old_state = RD_BLOCK_ZEROED;
		store->blocks[idx].state = RD_BLOCK_RAW;
		store->blocks[idx].data = ndata;
	} else if (store->blocks[idx].state == RD_BLOCK_RAW) {
		old_state = RD_BLOCK_RAW;
		old_data = store->blocks[idx].data;
		store->blocks[idx].data = ndata;
	}
	write_unlock(&store->blocks[idx].lock);
	atomic64_add(RD_BLOCK_SIZE, &store->stats.total_bytes_written);
	if (old_state == RD_BLOCK_ZEROED) {
		atomic64_dec(&store->stats.zeroed_blocks_count);
		atomic64_inc(&store->stats.raw_blocks_count);
	}
	if (!old_data)
		kfree(old_data);
	return 0;
}

int rd_read(struct rd_store *store, uint64_t idx, char *buffer)
{
	if (unlikely(!store))
		return -EINVAL;

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
	atomic64_add(RD_BLOCK_SIZE, &store->stats.total_bytes_read);
	return 0;
}

int rd_write_zeroes(struct rd_store *store, uint64_t idx)
{
	if (unlikely(!store))
		return -EINVAL;

	if (unlikely(idx >= store->blocks_count)) {
		pr_err("ramdisk: block index out of range");
		return -EINVAL;
	}

	char *old_data = NULL;
	enum rd_block_state old_state;

	write_lock(&store->blocks[idx].lock);
	if (store->blocks[idx].state == RD_BLOCK_RAW) {
		old_data = store->blocks[idx].data;
		old_state = RD_BLOCK_RAW;
		store->blocks[idx].data = NULL;
		store->blocks[idx].state = RD_BLOCK_ZEROED;
	} else {
		old_state = RD_BLOCK_ZEROED;
	}
	write_unlock(&store->blocks[idx].lock);
	atomic64_add(RD_BLOCK_SIZE, &store->stats.total_bytes_discarded);
	if (old_state == RD_BLOCK_RAW) {
		atomic64_inc(&store->stats.zeroed_blocks_count);
		atomic64_dec(&store->stats.raw_blocks_count);
	}
	kfree(old_data);
	return 0;
}

uint64_t rd_get_capacity_sectors(struct rd_store *store)
{
	return store->blocks_count * RD_BLOCK_SECTORS;
}

int rd_get_stats(struct rd_store *store, struct rd_stats_snapshot *out)
{
	if (unlikely(!store))
		return -EINVAL;

	out->raw_blocks_count =
		atomic64_read(&store->stats.raw_blocks_count);
	out->total_bytes_read =
		atomic64_read(&store->stats.total_bytes_read);
	out->zeroed_blocks_count =
		atomic64_read(&store->stats.zeroed_blocks_count);
	out->total_bytes_written =
		atomic64_read(&store->stats.total_bytes_written);
	out->total_bytes_discarded =
		atomic64_read(&store->stats.total_bytes_discarded);

	return 0;
}
