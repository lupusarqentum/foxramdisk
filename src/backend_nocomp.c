// SPDX-License-Identifier: GPL-2.0-only

#include <linux/err.h>
#include <linux/printk.h>

#include "ramdisk_compressor.h"

static void *nocomp_create(size_t max_data_to_compress_size)
{
	return NULL;
}

static void nocomp_del(void *private_data)
{
}

static ssize_t nocomp_compress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	return -ENOMEM;
}

static ssize_t nocomp_decompress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	pr_err("ramdisk: %s should not have been called\n", __func__);
	return -EINVAL;
}

static struct rd_comp_ops nocomp_ops = {
	.create = nocomp_create,
	.del = nocomp_del,
	.compress = nocomp_compress,
	.decompress = nocomp_decompress
};
