/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Grigoriy Loboda */

#ifndef _RAMDISK_STORE_H_
#define _RAMDISK_STORE_H_

#include <linux/types.h>
#include <linux/blk_types.h>

#include "ramdisk_compressor.h"

/**
 * struct rd_stats_snapshot - snapshot of storage statistics
 * @zeroed_blocks_count: number of fully zeroed blocks
 * @raw_blocks_count: number of blocks stored in an uncompressed form
 * @compressed_blocks_count: number of blocks stored in a compressed form
 * @compressed_data_size: total size of memory used for compressed blocks in bytes
 */
struct rd_stats_snapshot {
	uint64_t zeroed_blocks_count;
	uint64_t raw_blocks_count;
	uint64_t compressed_blocks_count;
	uint64_t compressed_data_size;
};

struct rd_store;

#define RD_BLOCK_SHIFT 12
#define RD_BLOCK_SIZE (1U << RD_BLOCK_SHIFT)
#define RD_BLOCK_SECTORS_SHIFT (RD_BLOCK_SHIFT - SECTOR_SHIFT)
#define RD_BLOCK_SECTORS (1U << RD_BLOCK_SECTORS_SHIFT)

/**
 * rd_new - allocate and initialize new compressed ramdisk storage
 * @blocks_count: initial capacity of storage in RD_BLOCK_SIZE blocks
 * @comp: compressor used to compress/decompress data
 *
 * Allocates memory and initializes new compressed RAM storage.
 * This function may sleep. If succeeds, returned pointer should be passed
 * to rd_del, when storage is no longer needed, to free memory and resources.
 *
 * Compressed ramdisk storage organizes its internal content into RD_BLOCK_SIZE blocks.
 * All future I/O requests operating on its data must be of RD_BLOCK_SIZE and aligned
 * to it. These blocks are indexed continiously from 0.
 *
 * Context: May sleep, allocates memory.
 *
 * Return:
 * error pointer on failure, or a pointer to dynamically allocated storage struct.
 */
struct rd_store *rd_new(uint64_t blocks_count, const struct rd_comp_ops *comp);

/**
 * rd_del - delete compressed ramdisk storage
 * @store: ramdisk storage previously created with rd_new
 *
 * Frees all memory and resources associated with the store.
 * Is NOT thread-safe; if there are any I/O requests going, behavior is undefined.
 *
 * Context: May sleep, NOT thread-safe.
 */
void rd_del(struct rd_store *store);

/**
 * rd_write - write to compressed ramdisk storage
 * @store: storage to write to
 * @idx: index of RD_BLOCK_SIZE block in the storage
 * @data: data to write, at least RD_BLOCK_SIZE bytes following the pointer must be accesible
 *
 * Context: May sleep. Thread-safe.
 *
 * Return:
 * 0 on success, -errno on error
 */
int rd_write(struct rd_store *store, uint64_t idx, const char *data);

/**
 * rd_read - read from compressed ramdisk storage
 * @store: storage to read from
 * @idx: index of RD_BLOCK_SIZE block in the storage
 * @buffer: buffer where read data would be written. Must be at least RD_BLOCK_SIZE length
 *
 * Context: May sleep. Thread-safe.
 *
 * Return:
 * 0 on success, -errno on error
 */
int rd_read(struct rd_store *store, uint64_t idx, char *buffer);

/**
 * rd_write_zeroes - write zeroes (discard) a block of compressed ramdisk storage
 * @store: storage where a block must be zeroed (discarded)
 * @idx: index of a block to write zeroes to (to discard)
 *
 * All reads of idx block after this request would read all 0's to buffer,
 * until a new write request to the block is performed.
 *
 * Context: May sleep. Thread-safe.
 *
 * Return:
 * 0 on success, -errno on error
 */
int rd_write_zeroes(struct rd_store *store, uint64_t idx);

/**
 * rd_get_capacity_sectors - return capacity of ramdisk storage in 512-byte sectors
 * @store: storage that is a subject of sectors capacity calculation
 *
 * Context: May sleep. Thread-safe.
 *
 * Return:
 * capacity of the storage in 512-byte sectors.
 */
uint64_t rd_get_capacity_sectors(struct rd_store *store);

/**
 * rd_get_stats - snapshot current stats
 * @store: storage to save stats from
 * @out: where stats should be saved to
 *
 * It is thread safe in the sense that it won't cause undefined behavior in
 * concurrent context. However, in this context, stats saved could be a little
 * wrong or even contradictionary. Usually, it is not a problem.
 * This function must not sleep.
 *
 * Return:
 * 0 on success, -errno on error
 */
int rd_get_stats(struct rd_store *store, struct rd_stats_snapshot *out);

#endif // _RAMDISK_STORE_H_
