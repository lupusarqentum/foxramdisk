// SPDX-License-Identifier: GPL-2.0-only

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "ramdisk_store.h"
#include "ramdisk_compressor.h"

enum rd_op {
	RD_OP_READ,
	RD_OP_WRITE,
	RD_OP_WRITE_ZEROES,
};

enum rd_block_state {
	RD_BLOCK_ZEROED = 0,
	RD_BLOCK_RAW,
	RD_BLOCK_COMPRESSED,
};

struct rd_block {
	void *data;
	size_t size;
	enum rd_block_state state;
};

struct rd_stats {
	atomic64_t zeroed_blocks_count;
	atomic64_t raw_blocks_count;
	atomic64_t compressed_blocks_count;
	atomic64_t compressed_data_size;
};

struct comp_ctx {
	const struct rd_comp_ops *ops;
	void                     *priv_data;
	void                     *tmp_buf;
};

struct rd_store {
	uint64_t blocks_count;
	struct rd_block *blocks;
	struct rd_stats stats;

	struct semaphore lock;

	struct comp_ctx comp_ctx;
};

static int rd_comp_ctx_create(struct rd_store *store, const char *comp_name)
{
	int err;

	struct comp_ctx *ctx = &store->comp_ctx;

	ctx->ops = rd_lookup_comp(comp_name);
	if (!ctx->ops) {
		err = -EINVAL;
		pr_err("ramdisk: %s compression method not found\n", comp_name);
		goto lookup_fail;
	}

	store->comp_ctx.tmp_buf = kmalloc(RD_BLOCK_SIZE, GFP_KERNEL);
	if (!ctx->tmp_buf) {
		err = -ENOMEM;
		goto tmp_buf_alloc_fail;
	}

	ctx->priv_data = ctx->ops->create(RD_BLOCK_SIZE);
	if (IS_ERR(ctx->priv_data)) {
		err = PTR_ERR(ctx->priv_data);
		goto priv_data_alloc_fail;
	}
	return 0;

priv_data_alloc_fail:
	kfree(ctx->tmp_buf);
tmp_buf_alloc_fail:
lookup_fail:
	return err;
}

static void rd_comp_ctx_del(struct rd_store *store)
{
	store->comp_ctx.ops->del(store->comp_ctx.priv_data);
	kfree(store->comp_ctx.tmp_buf);
}

struct rd_store *rd_new(uint64_t blocks_count, const char *comp)
{
	if (blocks_count >= U64_MAX / RD_BLOCK_SECTORS)
		return ERR_PTR(-EINVAL);

	struct rd_store *result = kmalloc(sizeof(*result), GFP_KERNEL);
	int ret;

	if (!result) {
		ret = -ENOMEM;
		goto return_error;
	}

	sema_init(&result->lock, 1);
	result->blocks_count = blocks_count;
	ret = rd_comp_ctx_create(result, comp);
	if (ret < 0)
		goto comp_ctx_create_fail;

	result->blocks = vmalloc_array(blocks_count, sizeof(result->blocks[0]));
	if (!result->blocks) {
		ret = -ENOMEM;
		goto block_index_alloc_fail;
	}

	for (uint64_t i = 0; i < blocks_count; ++i) {
		result->blocks[i].data = NULL;
		result->blocks[i].state = RD_BLOCK_ZEROED;
		result->blocks[i].size = 0;
	}

	atomic64_set(&result->stats.raw_blocks_count, 0);
	atomic64_set(&result->stats.compressed_blocks_count, 0);
	atomic64_set(&result->stats.zeroed_blocks_count, (int64_t)blocks_count);
	atomic64_set(&result->stats.compressed_data_size, 0);

	return result;
block_index_alloc_fail:
	rd_comp_ctx_del(result);
comp_ctx_create_fail:
	kfree(result);
return_error:
	return ERR_PTR(ret);
}

void rd_del(struct rd_store *store)
{
	if (unlikely(!store))
		return;
	rd_comp_ctx_del(store);
	for (uint64_t i = 0; i < store->blocks_count; ++i) {
		if (store->blocks[i].state != RD_BLOCK_ZEROED)
			kfree(store->blocks[i].data);
	}
	vfree(store->blocks);
	kfree(store);
}

static void update_block_state_counters(struct rd_store *store,
	enum rd_block_state old,
	enum rd_block_state new)
{
	switch (old) {
	case RD_BLOCK_ZEROED:
		atomic64_dec(&store->stats.zeroed_blocks_count);
		break;
	case RD_BLOCK_RAW:
		atomic64_dec(&store->stats.raw_blocks_count);
		break;
	case RD_BLOCK_COMPRESSED:
		atomic64_dec(&store->stats.compressed_blocks_count);
		break;
	}
	switch (new) {
	case RD_BLOCK_ZEROED:
		atomic64_inc(&store->stats.zeroed_blocks_count);
		break;
	case RD_BLOCK_RAW:
		atomic64_inc(&store->stats.raw_blocks_count);
		break;
	case RD_BLOCK_COMPRESSED:
		atomic64_inc(&store->stats.compressed_blocks_count);
		break;
	}
}

static int rd_write_low(struct rd_store *store, uint64_t idx, const char *data)
{
	ssize_t new_size;
	enum rd_block_state new_state;
	char *new_data = NULL;
	struct comp_ctx *ctx = &store->comp_ctx;
	const void *copy_source;

	new_size = ctx->ops->compress(
		ctx->priv_data,
		data,
		RD_BLOCK_SIZE,
		ctx->tmp_buf,
		RD_BLOCK_SIZE
	);

	if (new_size < 0) {
		new_size = RD_BLOCK_SIZE;
		new_state = RD_BLOCK_RAW;
		copy_source = data;
	} else {
		new_state = RD_BLOCK_COMPRESSED;
		copy_source = ctx->tmp_buf;
	}

	new_data = kmalloc(new_size, GFP_KERNEL);
	if (!new_data)
		return -ENOMEM;
	memcpy(new_data, copy_source, new_size);

	kfree(store->blocks[idx].data);

	store->blocks[idx].data = new_data;
	store->blocks[idx].state = new_state;
	store->blocks[idx].size = new_size;

	return 0;
}

static int rd_write_zeroes_low(struct rd_store *store, uint64_t idx)
{
	char *old_data = store->blocks[idx].data;

	store->blocks[idx].data = NULL;
	store->blocks[idx].size = 0;
	store->blocks[idx].state = RD_BLOCK_ZEROED;
	kfree(old_data);
	return 0;
}

static int rd_read_low(struct rd_store *store, uint64_t idx, char *buffer)
{
	enum rd_block_state state = store->blocks[idx].state;
	size_t size = store->blocks[idx].size;
	void *data = store->blocks[idx].data;

	if (state == RD_BLOCK_ZEROED) {
		memset(buffer, 0, RD_BLOCK_SIZE);
		return 0;
	}
	if (state == RD_BLOCK_RAW) {
		memcpy(buffer, data, RD_BLOCK_SIZE);
		return 0;
	}

	struct comp_ctx *ctx = &store->comp_ctx;
	ssize_t err;

	err = ctx->ops->decompress(
		ctx->priv_data,
		data,
		size,
		buffer,
		RD_BLOCK_SIZE
	);

	return err < 0 ? (int)err : 0;
}

static int rd_io_high(struct rd_store *store,
	enum rd_op op,
	uint64_t idx,
	const char *data,
	char *buffer)
{
	int ret = -1;

	if (unlikely(!store))
		return -EINVAL;
	down(&store->lock);
	// lock above required to prevent concurrent blocks_count update
	if (unlikely(idx >= store->blocks_count)) {
		ret = -EINVAL;
		goto exit;
	}

	struct rd_block *block = &store->blocks[idx];
	size_t new_size, old_size;
	enum rd_block_state new_state, old_state;
	int64_t comp_size_delta = 0;

	old_size = block->size;
	old_state = block->state;

	switch (op) {
	case RD_OP_READ:
		ret = rd_read_low(store, idx, buffer);
		break;
	case RD_OP_WRITE:
		ret = rd_write_low(store, idx, data);
		break;
	case RD_OP_WRITE_ZEROES:
		ret = rd_write_zeroes_low(store, idx);
		break;
	}

	if (ret)
		goto exit;

	new_size = block->size;
	new_state = block->state;

	update_block_state_counters(store, old_state, new_state);
	if (old_state == RD_BLOCK_COMPRESSED)
		comp_size_delta -= (int64_t)old_size;
	if (new_state == RD_BLOCK_COMPRESSED)
		comp_size_delta += (int64_t)new_size;
	atomic64_add(comp_size_delta, &store->stats.compressed_data_size);
exit:
	up(&store->lock);
	return ret;
}

int rd_write(struct rd_store *store, uint64_t idx, const char *data)
{
	return rd_io_high(store, RD_OP_WRITE, idx, data, NULL);
}

int rd_write_zeroes(struct rd_store *store, uint64_t idx)
{
	return rd_io_high(store, RD_OP_WRITE_ZEROES, idx, NULL, NULL);
}

int rd_read(struct rd_store *store, uint64_t idx, char *buffer)
{
	return rd_io_high(store, RD_OP_READ, idx, NULL, buffer);
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
	out->zeroed_blocks_count =
		atomic64_read(&store->stats.zeroed_blocks_count);
	out->compressed_blocks_count =
		atomic64_read(&store->stats.compressed_blocks_count);

	out->compressed_data_size =
		atomic64_read(&store->stats.compressed_data_size);

	return 0;
}
