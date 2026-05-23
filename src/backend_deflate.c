// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Grigoriy Loboda

#include <linux/slab.h>
#include <linux/zlib.h>

struct rd_deflate_ctx {
	struct z_stream_s cctx;
	struct z_stream_s dctx;
};

static void rd_deflate_del(void *private_data)
{
	struct rd_deflate_ctx *ctx = private_data;

	if (!ctx)
		return;

	if (ctx->cctx.workspace) {
		zlib_deflateEnd(&ctx->cctx);
		kvfree(ctx->cctx.workspace);
	}
	if (ctx->dctx.workspace) {
		zlib_inflateEnd(&ctx->dctx);
		kvfree(ctx->dctx.workspace);
	}

	kfree(ctx);
}

static void *rd_deflate_create(size_t max_data_to_compress_size)
{
	int ret;
	struct rd_deflate_ctx *ctx = NULL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto error;
	}

	int wbits = MAX_WBITS;
	int memlevel = MAX_MEM_LEVEL;

	int deflate_workmem_size = zlib_deflate_workspacesize(wbits, memlevel);

	ctx->cctx.workspace = kvzalloc(deflate_workmem_size, GFP_KERNEL);
	if (!ctx->cctx.workspace) {
		ret = -ENOMEM;
		goto error;
	}

	ret = zlib_deflateInit2(&ctx->cctx,
		Z_BEST_COMPRESSION,
		Z_DEFLATED,
		wbits,
		memlevel,
		Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		ret = -EINVAL;
		goto error;
	}

	int inflate_workmem_size = zlib_inflate_workspacesize();

	ctx->dctx.workspace = kvzalloc(inflate_workmem_size, GFP_KERNEL);
	if (!ctx->dctx.workspace) {
		ret = -ENOMEM;
		goto error;
	}

	ret = zlib_inflateInit2(&ctx->dctx, wbits);
	if (ret != Z_OK) {
		ret = -EINVAL;
		goto error;
	}

	return ctx;
error:
	rd_deflate_del(ctx);
	return ERR_PTR(ret);
}

static ssize_t rd_deflate_compress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	int ret;
	struct rd_deflate_ctx *ctx = private_data;
	struct z_stream_s *deflate = &ctx->cctx;

	ret = zlib_deflateReset(deflate);
	if (ret != Z_OK)
		return -EINVAL;

	deflate->next_in = src;
	deflate->avail_in = slen;
	deflate->next_out = dst;
	deflate->avail_out = dlen;

	ret = zlib_deflate(deflate, Z_FINISH);
	if (ret != Z_STREAM_END)
		return -EINVAL;
	return (ssize_t)deflate->total_out;
}

static ssize_t rd_deflate_decompress(void *private_data,
	const char *src,
	size_t slen,
	char *dst,
	size_t dlen)
{
	int ret;
	struct rd_deflate_ctx *ctx = private_data;
	struct z_stream_s *inflate = &ctx->dctx;

	ret = zlib_inflateReset(inflate);
	if (ret != Z_OK)
		return -EINVAL;

	inflate->next_in = src;
	inflate->avail_in = slen;
	inflate->next_out = dst;
	inflate->avail_out = dlen;

	ret = zlib_inflate(inflate, Z_FINISH);
	if (ret != Z_STREAM_END)
		return -EINVAL;
	return (ssize_t)inflate->total_in;
}

static const struct rd_comp_ops rd_deflate_ops = {
	.create = rd_deflate_create,
	.del = rd_deflate_del,
	.compress = rd_deflate_compress,
	.decompress = rd_deflate_decompress
};
