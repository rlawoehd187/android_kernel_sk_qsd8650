#
# linux/arch/arm/panic_log/Makefile
#
# Copyright (C) 2001 ARM Limited
#

# EXTRA_CFLAGS := -DDEBUG
# EXTRA_AFLAGS := -DDEBUG

#KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=softvfp+vfp)
#LDFLAGS		+=--no-warn-mismatch
KBUILD_CFLAGS	+= -O0
obj-$(CONFIG_PANIC_LOG_SAVE)+= panic_nand.o