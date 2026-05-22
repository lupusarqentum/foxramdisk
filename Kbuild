ccflags-y := 	-Wall					\
		-Wextra					\
		-Wformat					\
		-O2					\
		-std=gnu18				\
		-g					\
		-Werror=format-security			\
		-Werror=implicit-function-declaration

obj-m := ramdisk.o

ramdisk-y := src/ramdisk.o src/ramdisk_store.o src/ramdisk_compressor.o
