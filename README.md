# 3-rd party Linux kernel compressed RAM disk module

A third-party Linux kernel module written in C to provide a block device with RAM-backed storage and compression capabilities for improving memory efficiency.

This project is implemented mainly for fun and educational purposes. No warranties of correctness or fitness for any particular purpose are given.

## Features

- RAM-backed block devices
- Optional compression
- Multiple devices
- Runtime device creation
- Per-device statistics via sysfs or convenience script
- Discard and write-zeroes support to reduce memory usage

## Building the module

The project is expected to be built on **Fedora Server 43** with **Linux kernel 6.18.5-200.fc43.x86_64**. Other distributions and Linux kernel versions are not supported, though they might work as well.

To build the project, you need to install build dependencies:

```bash
sudo dnf install gcc make kernel-devel kernel-headers
```

After installing dependencies and cloning the repository, simply run make:

```bash
make
```

This will produce the module ```.ko``` file that can be loaded. Try also:

```bash
make help
```

for additional information.

## Loading the module

Use ```insmod``` or ```modprobe``` to load the module you have built, for example:

```bash
sudo insmod ./foxramdisk.ko
```

Alternatively, you can install the module to insert it by its name:

```bash
sudo make install
sudo modprobe foxramdisk
```

Please note that there is no ```uninstall``` target.

If ```modprobe``` fails after successful module installation, try running ```depmod -a``` command.

## Module parameters

. You can pass the following module parameters:

- ```initial_devices_count```: number of devices to create at module load (**uint, optional, defaults to 1**)
- ```default_capacity```: initial capacity of newly created devices in 4096-bytes long blocks (**ulong, optional, defaults to 4096 blocks (16 MiB)**)
- ```default_compression```: name of the compression algorithm to be used by newly created devices (**string, optional, defaults to ```deflate```**)

Recognized compression algorithm names are:

- ```nocomp```: to disable compression and store data uncompressed
- ```842```: 842 algorithm
- ```deflate```: deflate algorithm

Block devices created by the module receive ```/dev/foxramdiskN``` dev files in ```/dev```, where ```N``` is the device number.

```default_capacity``` and ```default_compression``` parameters can be set after the module has loaded. Example:

```bash
echo -n "nocomp" > /sys/module/foxramdisk/parameters/default_compression
```

This would change neither capacity nor compression algorithm of devices already created.

## Creating devices

In addition to devices created at module load, you can hot-add new devices. They will be created with ```default_capacity``` and use ```default_compression``` (see the previous section on how to set these parameters after module has been loaded).

To do that, read from ```hot_add``` file:

```bash
cat /sys/class/foxramdisk-control/hot_add
```

If it succeeds at creating a new device, you will successfully read a device number attached to the new device. Device number is a number that's appended to foxramdisk in device file, e.g., ```/dev/foxramdisk7```.

## Using devices

You can use them like any other block devices, for example, you can format partition tables, filesystems, swaps, or use utilities such as ```dd```.

All data will be stored in RAM, possibly in compressed form.

Probably use cases include:

- swap
- temporary files
- maybe more!

## Viewing device usage statistics

The module collects statistics on I/O to devices and their storage representation. You can manually read sysfs files for individual stats. For example, the following command will show total size of compressed blocks:

```bash
sudo cat /sys/block/foxramdiskN/storage_stat/compressed_data_size
```

Additionally, you can use convenience script ```rd_display_stats.sh``` located in the ```script``` folder of this repository. This script will read all stats for you and format them for easier reading. Run script with no arguments or with a ```--help``` option to display its help page:

```bash
./script/rd_display_stats.sh --help
```

An example of using the script:

```bash
$ ./script/rd_display_stats.sh -h /dev/foxramdiskN

Bytes written:                   282.21 MiB
Bytes read:                      4.00 KiB
Bytes discarded:                 0.00 B
Failed reads:                    0
Failed writes:                   0
Failed discards:                 0

Block size in bytes:             4096
Incompressible blocks:           8120
Incompressible blocks data size: 31.72 MiB
Zeroed blocks:                   990081
Zeroed blocks logical size:      3.78 GiB
Compressed blocks:               50375
Compressed blocks logical size:  196.78 MiB
Compressed data size:            36.82 MiB

Compression algorithm:           deflate

Compression ratio:               5.344
    Incompressible blocks are not considered.
Effective compression ratio:     3.334
    Incompressible blocks are treated as compression ratio 1:1.
```

## Removing devices and unloading the module

Currently, there is no way to hot-remove a device. As a workaround, you can discard all of its content to free the memory it uses for storage (though it will still use some memory for metadata):

```bash
sudo blkdiscard /dev/foxramdiskN
```

You can also unload the module with  the help of ```rmmod``` command. All existing devices and their data will be lost.

## Planned Updates

New updates ideas:

- hot-remove a device
- change device size and compression algorithm on the fly
- reduce memory footprint of metadata
- allocate per-CPU compressor contexts to allow concurrent I/O, switch to per-block locks instead of per-device lock
- support more compression algorithms

TODO: add static analyzers to CI
TODO: add testing to CI (already in slow progress locally)

## Testing & CI

There are make targets to run clang-format to check or fix sources. There is a make target to run checkpatch on git diff as well. Please, see the ```make help``` page to view exact target names.

clang-format and checkpatch stylechecks are run in CI on pushes and pull requests. Additionally, CI checks that the source is buildable.

## Project Architecture & Documentation

You can find and view doc comments directly in the source code, or generate docs using ```kernel-doc``` script found in the Linux kernel repository. What follows is a general architecture overview.

The code is split across different files.

- ```src/ramdisk_store.h``` and ```src/ramdisk_store.c``` are responsible for actual storage logic. The data is organized into ```RD_BLOCK_SIZE``` blocks that can be independently accessed by ```rd_write```, ```rd_read``` and ```rd_write_zeroes``` function calls.
- ```src/ramdisk.c``` is responsible for all communication with the Linux kernel API, such as module parameters, sysfs entries, disk registration, etc. It also receives I/O requests, processes them and calls ```ramdisk_store.h``` to complete them.
- ```src/ramdisk_compressor.h``` and ```src/ramdisk_compressor.c``` provide a unified interface of compression algorithms for ```ramdisk_store```, as well as a registry to find these compressors by their name.

Notes:

- For every supported compression algorithm, there are corresponding ```.c``` and ```.h``` files..
- If compression is not desired, there is a ```"nocomp"``` compression algorithm. For every compress call it returns an error indicating the block is incompressible, effectively forcing the storage to store data uncompressed.
- ```ramdisk_store``` implementation: all I/O requests (function calls read, write, write zeroes/discard) are passed to ```rd_io_high``` function. This function will switch on the operation code and call one of the "low" functions: ```rd_write_low```, ```rd_read_low```, or ```rd_write_zeroes_low```. "low" functions focus on I/O logic only, they don't do user arguments validation, mutual exclusion, and statistics update. ```rd_io_high``` function can be thought as a decorator that adds this functionality to more focused I/O functions.
- Each block is represented by a ```struct rd_block``` that holds pointer to the data buffer (if applicable), the data buffer size and *state*.

A block might be in one of the following *states*:

- **zeroed**: the block is filled with zero bytes only. For efficiency, no data buffer is allocated for this block. This is the initial state of all blocks and also their state after discard operations.
- **compressed**: the data buffer contains compressed data that needs to be decompressed to be read.
- **raw**: reader can read block data directly without decompression. It could have happened because the block data is incompressible (e.g. random data) or the compression is disabled. The size field must be set to ```RD_BLOCK_SIZE```.

Discard and write zeroes operations are implemented identically and both transition blocks to zeroed state.

## License, Authors and Contact Information

[GPL-2.0](./LICENSE.md)

Copyright (C) 2026 - Grigoriy Loboda

See my GitHub page for contact information.
