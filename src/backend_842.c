// SPDX-License-Identifier: GPL-2.0-only

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/sw842.h>

#include "ramdisk_compressor.h"

static void *c842_create(size_t max_data_to_compress_size)
{
	void *workmem = kmalloc(SW842_MEM_COMPRESS, GFP_KERNEL);

	if (!workmem)
		return ERR_PTR(-ENOMEM);
	return workmem;
}

static void c842_del(void *private_data)
{
	kfree(private_data);
}

static ssize_t c842_compress(void *private_data,
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

static ssize_t c842_decompress(void *private_data,
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

static struct rd_comp_ops c842_ops = {
	.create = c842_create,
	.del = c842_del,
	.compress = c842_compress,
	.decompress = c842_decompress
};
