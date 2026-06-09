KVER ?= $(shell uname -r)
LK_SRC_DIR ?= /lib/modules/$(KVER)/build

KBUILD_OPTIONS := -j$(shell nproc) -C $(LK_SRC_DIR) M=$(CURDIR)

STATIC_ANALYZERS_SOURCES := 	$(wildcard src/*.h) \
				$(wildcard src/*.c)

CHECKPATCH_PATH := $(LK_SRC_DIR)/scripts/checkpatch.pl
CHECKPATCH_OPTIONS := --no-tree --ignore=FILE_PATH_CHANGES
GIT_DIFF := git diff
GIT_DIFF_OPTIONS := origin/main

CLANG_FORMAT := clang-format
CLANG_FORMAT_OPTIONS := --Werror
CLANG_FORMAT_FIX_OPTIONS := $(CLANG_FORMAT_OPTIONS) -i
CLANG_FORMAT_CHECK_OPTIONS := $(CLANG_FORMAT_OPTIONS) --dry-run
CLANG_FORMAT_SOURCES := $(STATIC_ANALYZERS_SOURCES)

PHONY += all
all: build

PHONY += build
build:
	$(MAKE) $(KBUILD_OPTIONS) modules

PHONY += clean
clean:
	$(MAKE) $(KBUILD_OPTIONS) clean

PHONY += install
install:
	$(MAKE) $(KBUILD_OPTIONS) modules_install

PHONY += checkpatch
checkpatch:
	$(GIT_DIFF) $(GIT_DIFF_OPTIONS) | $(CHECKPATCH_PATH) $(CHECKPATCH_OPTIONS)

PHONY += clang-format-check
clang-format-check:
	$(CLANG_FORMAT) $(CLANG_FORMAT_CHECK_OPTIONS) $(CLANG_FORMAT_SOURCES)

PHONY += clang-format-fix
clang-format-fix:
	$(CLANG_FORMAT) $(CLANG_FORMAT_FIX_OPTIONS) $(CLANG_FORMAT_SOURCES)

PHONY += check
check: clang-format-check checkpatch

PHONY += help
help:
	@echo "  Syntax: make [TARGET] [VARIABLE=value ...]"
	@echo
	@echo "  all                - default target, same as build"
	@echo "  clean              - remove build artifacts"
	@echo "  build              - build modules"
	@echo "  install            - install modules"
	@echo "  clang-format-fix   - use clang-format to fix styling issues"
	@echo "  clang-format-check - use clang-format to check for styling issues"
	@echo "  checkpatch         - run checkpatch on git diff main"
	@echo "  check              - combines checkpatch and clang-format-check"
	@echo "  help               - display this help message"
	@echo
	@echo "  KVER             - kernel version to build against"
	@echo "                       defaulted to: $(KVER)"
	@echo "  LK_SRC_DIR       - kernel source directory"
	@echo "                       defaulted to: $(LK_SRC_DIR)"
	@echo "  INSTALL_MOD_PATH - optional installation prefix"
	@echo "                       defaulted to: $(INSTALL_MOD_PATH)"

.PHONY: $(PHONY)
