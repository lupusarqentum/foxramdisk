// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Grigoriy Loboda

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/sw842.h>

#include "ramdisk_compressor.h"

static void *rd_842_create(size_t max_data_to_compress_size)
{
	void *workmem = kmalloc(SW842_MEM_COMPRESS, GFP_KERNEL);

	if (!workmem)
		return ERR_PTR(-ENOMEM);
	return workmem;
}

static void rd_842_del(void *private_data)
{
	kfree(private_data);
}

static ssize_t rd_842_compress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	int ret;
	unsigned int comp_size = dlen;

	ret = sw842_compress(src, slen, dst, &comp_size, private_data);
	if (ret == 0)
		ret = comp_size;
	return ret;
}

static ssize_t rd_842_decompress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	int ret;
	unsigned int decomp_size = dlen;

	ret = sw842_decompress(src, slen, dst, &decomp_size);
	if (ret == 0)
		ret = decomp_size;
	return ret;
}

static const struct rd_comp_ops rd_842_ops = {
	.create = rd_842_create,
	.del = rd_842_del,
	.compress = rd_842_compress,
	.decompress = rd_842_decompress
};
