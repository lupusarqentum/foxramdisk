// SPDX-License-Identifier: GPL-2.0-only

#include <linux/string.h>

#include "ramdisk_compressor.h"
#include "backend_nocomp.c"
#include "backend_842.c"

struct rd_comp_entry {
	const char *name;
	struct rd_comp_ops *ops;
};

static struct rd_comp_entry rd_comps_index[] = {
	{ .name = "nocomp",   .ops = &nocomp_ops },
	{ .name = "842",      .ops = &c842_ops },
	{ .name = NULL,       .ops = NULL } // sentinel
};

struct rd_comp_ops *rd_lookup_comp(const char *name)
{
	for (struct rd_comp_entry *i = rd_comps_index; i->name; i++) {
		if (!strcmp(i->name, name))
			return i->ops;
	}
	return NULL;
}
