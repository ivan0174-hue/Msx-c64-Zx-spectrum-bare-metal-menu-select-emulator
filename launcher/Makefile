#
# Makefile - boot launcher for Raspberry Pi 3 (32-bit)
#
CIRCLESTDLIB ?= $(abspath ../build/circle-stdlib)

include $(CIRCLESTDLIB)/Config.mk

OBJS	= main.o kernel.o \
	  ../common/video.o ../common/input.o ../common/ui.o ../common/config.o

TARGET	= kernel-launcher

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

LIBS := $(CIRCLE_STDLIB_LIBS)

# circle-newlib's glue code is C++ with exceptions: link the C++ runtime support
EXTRALIBS += "$(shell $(CXX) $(ARCHCPU) -print-file-name=libsupc++.a)"

-include $(DEPS)
