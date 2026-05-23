# Linux kernel compressed RAM disk

A C Linux kernel module that provides a block device with RAM-backed storage and compression capabilities to reduce memory efficiency.

This project is implemented mainly for fun and educational purposes. No warranties of correctness or fitness for any particular purpose are given.

## Features

- RAM-backed block devices
- Optional compression
- Multiple devices
- Runtime device creation
- Per-device statistics via sysfs or convenience script
- Discard and write-zeroes support to reduce memory usage

## Current Limitations

Both are planned to be improved.

-- No runtime device removal
-- Only one I/O operation can happen at a time.

## Building

The project is expected to be built on **Fedora Server** with **Linux kernel 6.18**. Other distributions and Linux kernel versions are out of support, though they might work as well.

The Linux kernel must be configured to support 842 and deflate algorithms (it is by default on Fedora).

To build the project on Fedora Server, you need to install build dependencies:

```bash
sudo dnf install gcc make kernel-devel kernel-headers
```

After installing dependencies and cloning the repository, simply run make:

```bash
make all
```

This will produce the module ```.ko``` file that can be loaded.

## Loading the module and creating devices

Simply use ```insmod``` or ```modprobe``` to load the module you have just built. You can pass the following module parameters:

- ```initial_devices_count```: number of devices to create at module load (**uint, optional, defaults to 1**)
- ```default_capacity```: initial capacity of newly created devices in 4096-bytes long blocks (**ulong, optional, defaults to 4096 blocks (16 MiB)**)
- ```default_compression```: name of the compression algorithm to be used by newly created devices (**string, optional, defaults to ```deflate```**)

Recognized compression algorithm names are:

- ```nocomp```: to disable compression and store data uncompressed
- ```842```: 842 algorithm
- ```deflate```: deflate algorithm

Block devices created by the module receive ```/dev/foxramdiskN``` dev files in ```/dev```.

```default_capacity``` and ```default_compression``` parameters can be set after the module has loaded. Example:

```bash
echo -n "nocomp" > /sys/module/foxramdisk/parameters/default_compression
```

This would change neither capacity nor compression algorithm of devices already created.

You can hot-add new devices without restarting the module. They will be created with ```default_capacity``` and use ```default_compression```.

To do that, read from ```hot_add``` file:

```bash
cat /sys/class/foxramdisk-control/hot_add
```

If it succeeds at creating a new device, you will successfully read a device number attached to the new device. Device number is a number that's appended to foxramdisk in device file, for example, device number of ```/dev/foxramdisk7``` is ```7```.

## Using devices

You can use them like any other block devices, for example, you can format partition tables, filesystems, swaps, or use utilities such as ```dd```.

The module collects statistics on I/O to devices and their storage representation. You can manually read sysfs files for individual stats, for example:

```bash
sudo cat /sys/block/foxramdiskN/storage_stat/compressed_data_size
```

Additionally, you can use convenience script ```rd_display_stats.sh``` located in the ```script``` folder of this repository. This script will read all stats for you and format them for easier reading. Run script with no arguments to display its help page:

```bash
./script/rd_display_stats.sh
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

You can easily unload the module with the help of ```rmmod``` command. All existing devices and their data will be lost.

## Planned Updates

If you want to implement any of these, please contact me beforehand. If you have an idea about a new update not listed here, you can open an issue or contact me.

New updates ideas:

- hot-remove a device
- change device size and compression algorithm on the fly
- reduce memory footprint of metadata
- allocate per-CPU compressor contexts to allow concurrent I/O, switch to per-block locks instead of per-device lock
- support more compression algorithms
- repository: testing & CI

## How to Contribute

There are multiple ways to help the project.

You can:

- open an issue or contact the developer, if you find a bug
- write new code for planned updates and send a Pull Request on GitHub. See the next sections coding conventions and technical information.

## Coding Conventions

Here are conventions used in this project:

- Use [**conventional commit**](https://www.conventionalcommits.org/) messages.
- Write your code with Linux kernel coding style in mind. All code in this repository is checked for code style violations by kernel **```checkpatch.pl```** script. It would be nice to check your code as well.
- Ensure your code **builds and runs** as expected.
- Write kernel-doc comments for all public functions. These comments should be consumable by kernel doc tools. For functions, document invariants, argument boundaries, **thread safety, and whether or not a function may sleep**.

## Testing & CI

CI and testing infrastructure are planned, but not yet implemented.

## Project Architecture & Documentation

You can find and view doc comments directly in the source code, or generate docs using ```kernel-doc``` script found in Linux kernel repository. What follows is a general architecture overview.

The code is split across different files.

- ```src/ramdisk_store.h``` and ```src/ramdisk_store.c``` are responsible for actual storage logic. The data is organized into ```RD_BLOCK_SIZE``` blocks that can be independently accessed by ```rd_write```, ```rd_read``` and ```rd_write_zeroes``` function calls.
- ```src/ramdisk.c``` is responsible for all communication with the Linux kernel API, such as module parameters, sysfs entries, disk registration, etc. It also receives I/O requests, processes them and calls ```ramdisk_store.h``` to complete them.
- ```src/ramdisk_compressor.h``` and ```src/ramdisk_compressor.c``` provide a unified interface of compression algorithms for ```ramdisk_store```, as well as a registry to find these compressors by name.

A few important notes:

- For every supported compression algorithm, there is corresponding ```.c``` file that is included by ```ramdisk_compressor.c```.
- If compression is not desired, there is a ```"nocomp"``` compression algorithm. With every compress operation it returns an error indicating the block is incompressible, effectively forcing the storage to store data uncompressed.
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
