// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bio.h>
#include <linux/module.h>
#include <linux/blkdev.h>

static const char *ramdisk_name = "foxramdisk"; // ramdisk is occupied

static struct block_device_operations ramdisk_ops;

struct ramdisk_dev {
	struct gendisk *gd;
	bool initialized;
};

static struct ramdisk_dev dev_a;
static struct ramdisk_dev dev_b;
static const sector_t capacity_a = 21;
static const sector_t capacity_b = 8*1024*1024*2;

static const char running_line[] = "Университетская Старый Петергоф Новый Петергоф Михайловская Дача Стрельна Сергиево Сосновая Поляна Лигово Ульянка Дачное Ленинский Проспект Броневая Электродепо Балтийский вокзал";

static void ramdisk_submit_bio(struct bio *bio)
{
	//struct ramdisk_dev *disk = bio->bi_bdev->bd_disk->private_data;
	if (bio_op(bio) == REQ_OP_READ) {
		struct bio_vec bvl;
		struct bvec_iter iter;

		bio_for_each_segment(bvl, bio, iter) {
			struct page *page = bvl.bv_page;
			unsigned int offset = bvl.bv_offset;
			unsigned int len = bvl.bv_len;
			char *mem = kmap_local_page(page);
			int pos;

			for (unsigned int i = 0; i < len; ++i) {
				pos = iter.bi_sector * 512 + i;
				mem[offset + i] = running_line[pos % (sizeof(running_line) - 1)];
			}
			kunmap_local(mem);
		}
		bio_endio(bio);
	} else {
		bio_endio(bio);
	}
}

static int ramdisk_add(struct ramdisk_dev *dev, int number, sector_t cap)
{
	int return_code;

	memset(dev, 0, sizeof(dev));
	dev->gd = blk_alloc_disk(NULL, NUMA_NO_NODE);
	if (IS_ERR(dev->gd)) {
		pr_err("Error allocating disk!\n");
		return_code = PTR_ERR(dev->gd);
		goto disk_allocation_error;
	}
	if (snprintf(dev->gd->disk_name, DISK_NAME_LEN, "%s%d", ramdisk_name, number) <= 0) {
		pr_err("Error disk name formatting!\n");
		goto disk_add_error;
	}
	dev->gd->fops = &ramdisk_ops;
	dev->gd->private_data = &dev;
	set_capacity(dev->gd, cap);
	return_code = add_disk(dev->gd);
	if (return_code) {
		pr_err("Error adding disk!\n");
		goto disk_add_error;
	}
	dev->initialized = true;
	return 0;
disk_add_error: del_gendisk(dev->gd);
disk_allocation_error: return return_code;
}

static void ramdisk_delete(struct ramdisk_dev *dev)
{
	if (dev->initialized == false)
		return;
	del_gendisk(dev->gd);
	put_disk(dev->gd);
}

static void cleanup(void)
{
	ramdisk_delete(&dev_a);
	ramdisk_delete(&dev_b);
}

static int __init ramdisk_init(void)
{
	memset(&ramdisk_ops, 0, sizeof(ramdisk_ops));
	ramdisk_ops.submit_bio = ramdisk_submit_bio;
	ramdisk_ops.owner = THIS_MODULE;

	int return_code;

	dev_a.initialized = false;
	dev_b.initialized = false;

	return_code = ramdisk_add(&dev_a, 0, capacity_a);
	if (return_code != 0) {
		pr_err("Error adding disk #%d!\n", 0);
		goto disk_add_error;
	}
	return_code = ramdisk_add(&dev_b, 1, capacity_b);
	if (return_code != 0) {
		pr_err("Error adding disk #%d!\n", 1);
		goto disk_add_error;
	}

	return 0;

disk_add_error:
	cleanup();
	return return_code;
}

static void __exit ramdisk_exit(void)
{
	cleanup();
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);

MODULE_AUTHOR("Loboda Grigoriy");
MODULE_DESCRIPTION("RAM backed block device driver");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL v2");
