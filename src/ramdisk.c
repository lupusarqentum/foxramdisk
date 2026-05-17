// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/module.h>
#include <linux/types.h>

#include "linux/device.h"
#include "ramdisk_store.h"

#define MAX_RAMDISK_DEVICES_COUNT 64

// ramdisk is already taken by drivers/block/brd.c
static const char *ramdisk_name = "foxramdisk";

static struct block_device_operations ramdisk_ops;
static struct queue_limits limits;

struct ramdisk_dev {
	struct gendisk *gd;
	struct rd_store *store;
	atomic64_t failed_reads;
	atomic64_t failed_writes;
	atomic64_t failed_discards;
	bool initialized;
};

static struct ramdisk_dev devices[MAX_RAMDISK_DEVICES_COUNT];
static uint32_t devices_added;

static ssize_t ramdisk_rd_stats_show(struct device *dev,
	struct device_attribute *attr,
	char *buf);

static ssize_t ramdisk_stats_show(struct device *dev,
	struct device_attribute *attr,
	char *buf);

static DEVICE_ATTR(total_bytes_written, 0444, ramdisk_rd_stats_show, NULL);
static DEVICE_ATTR(total_bytes_discarded, 0444, ramdisk_rd_stats_show, NULL);
static DEVICE_ATTR(total_bytes_read, 0444, ramdisk_rd_stats_show, NULL);
static DEVICE_ATTR(raw_blocks_count, 0444, ramdisk_rd_stats_show, NULL);
static DEVICE_ATTR(zeroed_blocks_count, 0444, ramdisk_rd_stats_show, NULL);

static DEVICE_ATTR(failed_reads, 0444, ramdisk_stats_show, NULL);
static DEVICE_ATTR(failed_writes, 0444, ramdisk_stats_show, NULL);
static DEVICE_ATTR(failed_discards, 0444, ramdisk_stats_show, NULL);

static struct attribute *ramdisk_attrs[] = {
	&dev_attr_total_bytes_written.attr,
	&dev_attr_total_bytes_discarded.attr,
	&dev_attr_total_bytes_read.attr,
	&dev_attr_raw_blocks_count.attr,
	&dev_attr_zeroed_blocks_count.attr,
	&dev_attr_failed_reads.attr,
	&dev_attr_failed_writes.attr,
	&dev_attr_failed_discards.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ramdisk);

static ssize_t ramdisk_rd_stats_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct ramdisk_dev *rd_dev = dev_to_disk(dev)->private_data;
	struct rd_stats_snapshot stats_snapshot;
	uint64_t stat;
	int err;

	err = rd_get_stats(rd_dev->store, &stats_snapshot);
	if (err != 0)
		return err;

	if (attr == &dev_attr_total_bytes_written)
		stat = stats_snapshot.total_bytes_written;
	else if (attr == &dev_attr_total_bytes_read)
		stat = stats_snapshot.total_bytes_read;
	else if (attr == &dev_attr_total_bytes_discarded)
		stat = stats_snapshot.total_bytes_discarded;
	else if (attr == &dev_attr_raw_blocks_count)
		stat = stats_snapshot.raw_blocks_count;
	else if (attr == &dev_attr_zeroed_blocks_count)
		stat = stats_snapshot.zeroed_blocks_count;
	else
		return -EINVAL;
	return sysfs_emit(buf, "%llu\n", (unsigned long long)stat);
}

static ssize_t ramdisk_stats_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct ramdisk_dev *rd_dev = dev_to_disk(dev)->private_data;
	uint64_t stat;

	if (attr == &dev_attr_failed_reads)
		stat = atomic64_read(&rd_dev->failed_reads);
	else if (attr == &dev_attr_failed_writes)
		stat = atomic64_read(&rd_dev->failed_writes);
	else if (attr == &dev_attr_failed_discards)
		stat = atomic64_read(&rd_dev->failed_discards);
	else
		return -EINVAL;
	return sysfs_emit(buf, "%llu\n", (unsigned long long)stat);
}

static void ramdisk_read(struct bio *bio, struct ramdisk_dev *dev)
{
	struct rd_store *rd = dev->store;
	char *buf = NULL;
	char *data = NULL;
	uint32_t buf_pos = 0;
	uint32_t buf_left_bytes = RD_BLOCK_SIZE;
	uint64_t block_idx = bio->bi_iter.bi_sector / RD_BLOCK_SECTORS;
	struct bio_vec bvl;
	struct bvec_iter iter;
	int err_code;

	buf = kmalloc(RD_BLOCK_SIZE, GFP_KERNEL);
	if (!buf)
		goto read_fail;

	bio_for_each_segment(bvl, bio, iter) {
		unsigned int len = bvl.bv_len;
		unsigned int offset = bvl.bv_offset;

		data = kmap_local_page(bvl.bv_page);

		while (len >= buf_left_bytes) {
			if (buf_pos == 0) {
				err_code = rd_read(rd, block_idx, buf);
				if (err_code != 0) {
					pr_err("ramdisk: read error %d\n", err_code);
					goto read_fail;
				}
			}
			memcpy(data + offset, buf + buf_pos, buf_left_bytes);
			len -= buf_left_bytes;
			offset += buf_left_bytes;
			buf_pos = 0;
			buf_left_bytes = RD_BLOCK_SIZE;
			block_idx++;
		}
		if (len > 0) {
			if (buf_pos == 0) {
				err_code = rd_read(rd, block_idx, buf);
				if (err_code != 0) {
					pr_err("ramdisk: read error %d\n", err_code);
					goto read_fail;
				}
			}
			memcpy(data + offset, buf + buf_pos, len);
			buf_pos += len;
			buf_left_bytes -= len;
		}

		kunmap_local(data);
		data = NULL;
	}
	bio_endio(bio);
	kfree(buf);
	return;
read_fail:
	atomic64_inc(&dev->failed_reads);
	if (data)
		kunmap_local(data);
	kfree(buf);
	bio_io_error(bio);
}

static void ramdisk_write(struct bio *bio, struct ramdisk_dev *dev)
{
	struct rd_store *rd = dev->store;
	char *buf = NULL;
	char *data = NULL;
	uint32_t buf_pos = 0;
	uint32_t buf_left_bytes = RD_BLOCK_SIZE;
	uint64_t block_idx = bio->bi_iter.bi_sector / RD_BLOCK_SECTORS;
	struct bio_vec bvl;
	struct bvec_iter iter;
	int err_code;

	buf = kmalloc(RD_BLOCK_SIZE, GFP_KERNEL);
	if (!buf)
		goto write_fail;

	bio_for_each_segment(bvl, bio, iter) {
		unsigned int len = bvl.bv_len;
		unsigned int offset = bvl.bv_offset;

		data = kmap_local_page(bvl.bv_page);

		while (len >= buf_left_bytes) {
			memcpy(buf + buf_pos, data + offset, buf_left_bytes);
			err_code = rd_write(rd, block_idx, buf);
			if (err_code != 0) {
				pr_err("ramdisk: write error %d\n", err_code);
				goto write_fail;
			}
			offset += buf_left_bytes;
			len -= buf_left_bytes;
			buf_pos = 0;
			buf_left_bytes = RD_BLOCK_SIZE;
			block_idx++;
		}
		if (len > 0) {
			memcpy(buf + buf_pos, data + offset, len);
			buf_pos += len;
			buf_left_bytes -= len;
		}

		kunmap_local(data);
		data = NULL;
	}
	bio_endio(bio);
	kfree(buf);
	return;
write_fail:
	atomic64_inc(&dev->failed_writes);
	if (data)
		kunmap_local(data);
	kfree(buf);
	bio_io_error(bio);
}

static void ramdisk_write_zeroes(struct bio *bio, struct ramdisk_dev *dev)
{
	struct rd_store *rd = dev->store;
	uint64_t blocks_left = bio->bi_iter.bi_size / RD_BLOCK_SIZE;
	uint64_t block_idx = bio->bi_iter.bi_sector / RD_BLOCK_SECTORS;

	while (blocks_left) {
		int err_code = rd_write_zeroes(rd, block_idx);

		if (err_code != 0) {
			pr_err("ramdisk: error writing zeroes\n");
			atomic64_inc(&dev->failed_discards);
			bio_io_error(bio);
			return;
		}
		block_idx++;
		blocks_left--;
	}

	bio_endio(bio);
}

static void ramdisk_submit_bio(struct bio *bio)
{
	struct ramdisk_dev *dev = bio->bi_bdev->bd_disk->private_data;
	enum req_op op = bio_op(bio);

	if (bio->bi_iter.bi_sector % RD_BLOCK_SECTORS != 0) {
		pr_err("ramdisk: assumption fail: first sector start is misaligned\n");
		bio_io_error(bio);
		return;
	}
	if (bio->bi_iter.bi_size % RD_BLOCK_SIZE != 0) {
		pr_err("ramdisk: assumption fail: bio len is misaligned\n");
		bio_io_error(bio);
		return;
	}

	switch (op) {
	case REQ_OP_READ:
		ramdisk_read(bio, dev);
		return;
	case REQ_OP_WRITE:
		ramdisk_write(bio, dev);
		return;
	case REQ_OP_DISCARD:
	case REQ_OP_WRITE_ZEROES:
		ramdisk_write_zeroes(bio, dev);
		return;
	case REQ_OP_FLUSH:
		// nothing to flush
		bio_endio(bio);
		return;
	default:
		pr_err("ramdisk: unsupported operation enum req_op %d\n", op);
		bio->bi_status = BLK_STS_NOTSUPP;
		bio_endio(bio);
		return;
	}
}

static int ramdisk_add(uint64_t capacity)
{
	int return_code;
	uint32_t device_index;
	struct gendisk *gd;
	struct rd_store *rd_store;
	char *buf;
	size_t size;

	if (devices_added >= MAX_RAMDISK_DEVICES_COUNT) {
		pr_err("ramdisk: ramdisk count limit exceeded\n");
		return_code = -ENOSPC;
		goto return_with_error;
	}
	device_index = devices_added;
	devices_added++;

	atomic64_set(&devices[device_index].failed_discards, 0);
	atomic64_set(&devices[device_index].failed_writes, 0);
	atomic64_set(&devices[device_index].failed_reads, 0);

	rd_store = rd_new(capacity);
	if (IS_ERR(rd_store)) {
		pr_err("ramdisk: failed to allocate storage for new ramdisk\n");
		return_code = PTR_ERR(rd_store);
		goto fail_allocating_rd_store;
	}

	gd = blk_alloc_disk(&limits, NUMA_NO_NODE);
	if (IS_ERR(gd)) {
		pr_err("ramdisk: disk allocation error\n");
		return_code = PTR_ERR(gd);
		goto disk_allocation_error;
	}

	buf = gd->disk_name;
	size = DISK_NAME_LEN;

	if (snprintf(buf, size, "%s%d", ramdisk_name, device_index) >= size) {
		pr_err("ramdisk: formatted disk name would be too long\n");
		return_code = -EINVAL;
		goto disk_name_formatting_error;
	}
	gd->fops = &ramdisk_ops;
	gd->private_data = &devices[device_index];
	set_capacity(gd, rd_get_capacity_sectors(rd_store));
	devices[device_index].gd = gd;
	devices[device_index].store = rd_store;
	devices[device_index].initialized = true;
	return_code = device_add_disk(NULL, gd, ramdisk_groups);
	if (return_code) {
		pr_err("ramdisk: disk_add_error\n");
		goto disk_add_error;
	}

	return device_index;
disk_add_error:
disk_name_formatting_error:
	put_disk(gd);
disk_allocation_error:
	rd_del(rd_store);
fail_allocating_rd_store:
	devices_added--;
return_with_error:
	return return_code;
}

static void ramdisk_delete(uint32_t device_index)
{
	struct ramdisk_dev *dev = &devices[device_index];

	if (dev->initialized == false)
		return;
	dev->initialized = false;
	del_gendisk(dev->gd);
	put_disk(dev->gd);
	rd_del(dev->store);
}

static int __init ramdisk_init(void)
{
	memset(&ramdisk_ops, 0, sizeof(ramdisk_ops));
	ramdisk_ops.submit_bio = ramdisk_submit_bio;
	ramdisk_ops.owner = THIS_MODULE;
	memset(&limits, 0, sizeof(limits));
	limits.physical_block_size = RD_BLOCK_SIZE;
	limits.logical_block_size = RD_BLOCK_SIZE;
	limits.discard_granularity = RD_BLOCK_SIZE;
	limits.features = BLK_FEAT_SYNCHRONOUS;
	limits.max_hw_discard_sectors = UINT_MAX;
	limits.max_write_zeroes_sectors = UINT_MAX;

	ramdisk_add(1024 * 1024);

	return 0;
}

static void __exit ramdisk_exit(void)
{
	for (uint32_t i = 0; i < devices_added; ++i)
		ramdisk_delete(i);
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);

MODULE_AUTHOR("Grigoriy Loboda");
MODULE_DESCRIPTION("RAM backed block device driver");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL v2");
