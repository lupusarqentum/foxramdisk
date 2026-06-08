ccflags-y := 	-Wall					\
		-Wextra					\
		-Wformat				\
		-Werror					\
		-O2					\
		-std=gnu18				\
		-g					\

obj-m := foxramdisk.o

foxramdisk-y := src/ramdisk.o 			\
		src/ramdisk_store.o 		\
		src/ramdisk_compressor.o	\
		src/backend_nocomp.o		\
		src/backend_deflate.o		\
		src/backend_842.o		\
