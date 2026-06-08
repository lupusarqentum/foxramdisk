/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2026 Grigoriy Loboda */

#ifndef _RAMDISK_COMPRESSOR_H_
#define _RAMDISK_COMPRESSOR_H_

#include <linux/types.h>

/**
 * struct rd_comp_ops - compressor operations
 * @create: allocates and inits private compressor data that is to be passed to all other methods
 * @del: properly free resources and memory allocated by create
 * @compress: compress data
 * @decompress: decompress data
 *
 * The compressor implementation might need to store additional data to perform
 * compression and decompression. It is initialized with create call and later
 * freed with del call. A compressor implementation might need to know in advance
 * the max possible source buffer to compress. It is passed to create call.
 *
 * Compress and decompress operations are not thread-safe. Private data should
 * not be accessed by compressor concurrently, and should not be passed
 * to different compressor.
 *
 * On failure, or when data is incompressible, compress and decompress operations return -errno.
 * On success, they return (de)compressed data size.
 */
struct rd_comp_ops {
	void *(*create)(size_t max_data_to_compress_size);

	void (*del)(void *private_data);

	ssize_t (*compress)(void *private_data, const char *src, size_t slen,
			    char *dst, size_t dlen);

	ssize_t (*decompress)(void *private_data, const char *src, size_t slen,
			      char *dst, size_t dlen);
};

/**
 * rd_lookup_comp - find a compressor by its string identifier
 * @name: compressor string identifier
 *
 * Returned pointer points to static duration object, and, possibly, readonly memory.
 * It should be neither freed nor modified.
 * Name parameter is not consumed by rd_lookup_comp,
 * it would never try to free it or use it after rd_lookup_comp returns.
 *
 * Return:
 * pointer to operation struct, if name is a valid id. NULL, otherwise.
 */
const struct rd_comp_ops *rd_lookup_comp(const char *name);

/**
 * rd_get_comp_name - find a compressor's name
 * @ops: compressor previously obtained by rd_lookup_comp
 *
 * Returned name is of static duration and might be in read-only memory.
 * It should be neither freed nor modified.
 *
 * Return:
 * NULL on error, otherwise
 * name used to obtain this compressor in rd_lookup_comp (compressor id).
 */
const char *rd_get_comp_name(const struct rd_comp_ops *ops);

#endif
