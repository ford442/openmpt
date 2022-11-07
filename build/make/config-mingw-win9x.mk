
ifeq ($(origin CC),default)
CC  = mingw32-gcc$(MINGW_FLAVOUR)
endif
ifeq ($(origin CXX),default)
CXX = mingw32-g++$(MINGW_FLAVOUR)
endif
ifeq ($(origin LD),default)
LD  = $(CXX)
endif
ifeq ($(origin AR),default)
AR  = mingw32-gcc-ar$(MINGW_FLAVOUR)
endif

CXXFLAGS_STDCXX = -std=gnu++17 -fexceptions -frtti
CFLAGS_STDC = -std=gnu17
CXXFLAGS += $(CXXFLAGS_STDCXX)
CFLAGS += $(CFLAGS_STDC)

CPPFLAGS += -DWINVER=0x0410 -D_WIN32_WINDOWS=0x0410 -DNOMINMAX -DMPT_BUILD_RETRO
CXXFLAGS += -mconsole -mthreads
CFLAGS   += -mconsole -mthreads
LDFLAGS  += 
LDLIBS   += -lm
ARFLAGS  := rcs

LDLIBS_LIBOPENMPTTEST += -lole32 -lrpcrt4
LDLIBS_OPENMPT123 += -lwinmm

LDFLAGS  += -static -static-libgcc -static-libstdc++

# enable gc-sections for all configurations in order to remove as much of the
# stdlib as possible
MPT_COMPILER_NOGCSECTIONS=1
CXXFLAGS += -ffunction-sections -fdata-sections
CFLAGS   += -ffunction-sections -fdata-sections
LDFLAGS  += -Wl,--gc-sections

CXXFLAGS += -march=i486 -m80387 -mtune=pentium
CFLAGS   += -march=i486 -m80387 -mtune=pentium

PC_LIBS_PRIVATE += -lole32 -lrpcrt4

include build/make/warnings-gcc.mk

EXESUFFIX=.exe
SOSUFFIX=.dll
SOSUFFIXWINDOWS=1

DYNLINK=0
SHARED_LIB=1
STATIC_LIB=0
SHARED_SONAME=0

OPTIMIZE=size

FORCE_UNIX_STYLE_COMMANDS=1

IN_OPENMPT=1
XMP_OPENMPT=1

IS_CROSS=1

NO_ZLIB=1
NO_MPG123=1
NO_OGG=1
NO_VORBIS=1
NO_VORBISFILE=1
NO_PORTAUDIO=1
NO_PORTAUDIOCPP=1
NO_PULSEAUDIO=1
NO_SDL2=1
NO_SNDFILE=1
NO_FLAC=1
