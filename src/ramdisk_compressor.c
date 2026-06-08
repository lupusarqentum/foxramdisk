// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Grigoriy Loboda

#include <linux/string.h>

#include "ramdisk_compressor.h"
#include "backend_nocomp.h"
#include "backend_842.h"
#include "backend_deflate.h"

struct rd_comp_entry {
	const char *name;
	const struct rd_comp_ops *ops;
};

static struct rd_comp_entry rd_comps_index[] = {
	{ .name = "nocomp",   .ops = &rd_nocomp_ops },
	{ .name = "842",      .ops = &rd_842_ops },
	{ .name = "deflate",  .ops = &rd_deflate_ops },
	{ .name = NULL,       .ops = NULL } // sentinel
};

const struct rd_comp_ops *rd_lookup_comp(const char *name)
{
	for (struct rd_comp_entry *i = rd_comps_index; i->name; i++) {
		if (!strcmp(i->name, name))
			return i->ops;
	}
	return NULL;
}

const char *rd_get_comp_name(const struct rd_comp_ops *ops)
{
	for (struct rd_comp_entry *i = rd_comps_index; i->name; i++) {
		if (i->ops == ops)
			return i->name;
	}
	return NULL;
}
