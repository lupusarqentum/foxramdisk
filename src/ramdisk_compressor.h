/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _RAMDISK_COMPRESSOR_H_
#define _RAMDISK_COMPRESSOR_H_

#include <linux/types.h>

struct rd_comp_ops {
	void* (*const create) (size_t max_data_to_compress_size);

	void (*const del) (void *private_data);

	ssize_t (*const compress) (
		void *private_data,
		const char *src,
		size_t slen,
		char *dst,
		size_t dlen
	);

	ssize_t (*const decompress) (
		void *private_data,
		const char *src,
		size_t slen,
		char *dst,
		size_t dlen
	);
};

struct rd_comp_ops *rd_lookup_comp(const char *name);

#endif
