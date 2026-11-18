#
# Makefile - bare metal MSX (fMSX) for Raspberry Pi 3
#
# CIRCLESTDLIB must point to a configured and built circle-stdlib tree
# (./configure -r 3 -n && make), see ../build.sh
#
CIRCLESTDLIB ?= $(abspath ../build/circle-stdlib)

include $(CIRCLESTDLIB)/Config.mk

FMSX	= fmsx

OBJS	= main.o kernel.o \
	  ../common/video.o ../common/input.o ../common/ui.o ../common/config.o \
	  $(FMSX)/libretro.o \
	  $(FMSX)/fMSX/MSX.o $(FMSX)/fMSX/V9938.o \
	  $(FMSX)/EMULib/Sound.o $(FMSX)/EMULib/SHA1.o $(FMSX)/EMULib/Floppy.o \
	  $(FMSX)/EMULib/FDIDisk.o $(FMSX)/EMULib/MCF.o $(FMSX)/EMULib/I8255.o \
	  $(FMSX)/EMULib/YM2413.o $(FMSX)/EMULib/AY8910.o $(FMSX)/EMULib/SCC.o \
	  $(FMSX)/EMULib/WD1793.o \
	  $(FMSX)/Z80/Z80.o \
	  $(FMSX)/NukeYKT/opll.o $(FMSX)/NukeYKT/WrapNukeYKT.o \
	  $(FMSX)/libretro-common/file/retro_dirent.o \
	  $(FMSX)/libretro-common/file/file_path.o \
	  $(FMSX)/libretro-common/file/file_path_io.o \
	  $(FMSX)/libretro-common/compat/compat_posix_string.o \
	  $(FMSX)/libretro-common/compat/compat_strl.o \
	  $(FMSX)/libretro-common/compat/compat_snprintf.o \
	  $(FMSX)/libretro-common/compat/fopen_utf8.o \
	  $(FMSX)/libretro-common/compat/compat_strcasestr.o \
	  $(FMSX)/libretro-common/encodings/encoding_utf.o \
	  $(FMSX)/libretro-common/streams/file_stream.o \
	  $(FMSX)/libretro-common/streams/file_stream_transforms.o \
	  $(FMSX)/libretro-common/time/rtime.o \
	  $(FMSX)/libretro-common/string/stdstring.o \
	  $(FMSX)/libretro-common/vfs/vfs_implementation.o

TARGET	= kernel-msx

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE) \
	   -I $(FMSX) -I $(FMSX)/libretro-common/include -I $(FMSX)/EMULib \
	   -I $(FMSX)/Z80 -I $(FMSX)/fMSX -I $(FMSX)/NukeYKT

DEFINE	+= -DSKIP_STDIO_REDEFINES -DLSB_FIRST -DFRONTEND_SUPPORTS_RGB565 -DGIT_VERSION=\"bm\"

CFLAGS	+= -Wno-unused-variable -Wno-unused-function -Wno-sign-compare -Wno-pointer-sign \
	   -Wno-unused-but-set-variable -Wno-implicit-fallthrough

LIBS := $(CIRCLE_STDLIB_LIBS) $(CIRCLEHOME)/lib/sound/libsound.a

# circle-newlib's glue code is C++ with exceptions: link the C++ runtime support
EXTRALIBS += "$(shell $(CXX) $(ARCHCPU) -print-file-name=libsupc++.a)"

-include $(DEPS)
