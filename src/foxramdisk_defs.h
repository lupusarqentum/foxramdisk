/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Grigoriy Loboda */

#ifndef _FOXRAMDISK_DEFS_H_
#define _FOXRAMDISK_DEFS_H_

// ramdisk is too common name for oot module;
// e.g. drivers/block/brd.c uses it for logging
#define DEV_FILE_NAME_PREFIX "foxramdisk"

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#endif // _FOXRAMDISK_DEFS_H_
