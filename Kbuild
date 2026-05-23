ccflags-y := 	-Wall					\
		-Wextra					\
		-Wformat					\
		-O2					\
		-std=gnu18				\
		-g					\
		-Werror=format-security			\
		-Werror=implicit-function-declaration

obj-m := foxramdisk.o

foxramdisk-y := src/ramdisk.o \
	src/ramdisk_store.o \
	src/ramdisk_compressor.o
