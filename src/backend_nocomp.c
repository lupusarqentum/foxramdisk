// SPDX-License-Identifier: GPL-2.0-only

#include <linux/err.h>
#include <linux/printk.h>

#include "ramdisk_compressor.h"

static void *rd_nocomp_create(size_t max_data_to_compress_size)
{
	return NULL;
}

static void rd_nocomp_del(void *private_data)
{
}

static ssize_t rd_nocomp_compress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	return -ENOMEM;
}

static ssize_t rd_nocomp_decompress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	pr_err("ramdisk: %s should not have been called\n", __func__);
	return -EINVAL;
}

static const struct rd_comp_ops rd_nocomp_ops = {
	.create = rd_nocomp_create,
	.del = rd_nocomp_del,
	.compress = rd_nocomp_compress,
	.decompress = rd_nocomp_decompress
};
