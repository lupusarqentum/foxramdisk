/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _RAMDISK_STORE_H_
#define _RAMDISK_STORE_H_

#include <linux/types.h>
#include <linux/blk_types.h>

struct rd_stats_snapshot {
	uint64_t total_bytes_discarded;
	uint64_t total_bytes_read;
	uint64_t total_bytes_written;
	uint64_t zeroed_blocks_count;
	uint64_t raw_blocks_count;
};

struct rd_store;

#define RD_BLOCK_SHIFT 12
#define RD_BLOCK_SIZE (1U << RD_BLOCK_SHIFT)
#define RD_BLOCK_SECTORS_SHIFT (RD_BLOCK_SHIFT - SECTOR_SHIFT)
#define RD_BLOCK_SECTORS (1U << RD_BLOCK_SECTORS_SHIFT)

// CAPACITY AND INDICES MEASURES IN RD_BLOCKS if not stated otherwise!

struct rd_store *rd_new(uint64_t blocks_count);
void rd_del(struct rd_store *store);
int rd_write(struct rd_store *store, uint64_t idx, const char *data);
int rd_read(struct rd_store *store, uint64_t idx, char *buffer);
int rd_write_zeroes(struct rd_store *store, uint64_t idx);
uint64_t rd_get_capacity_sectors(struct rd_store *store);
int rd_get_stats(struct rd_store *store, struct rd_stats_snapshot *out);

#endif // _RAMDISK_STORE_H_
