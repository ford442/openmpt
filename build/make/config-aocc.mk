
ifeq ($(origin CC),default)
CC  = $(TOOLCHAIN_PREFIX)clang$(TOOLCHAIN_SUFFIX)
endif
ifeq ($(origin CXX),default)
CXX = $(TOOLCHAIN_PREFIX)clang++$(TOOLCHAIN_SUFFIX)
endif
ifeq ($(origin LD),default)
LD  = $(CXX)
endif
ifeq ($(origin AR),default)
AR  = $(TOOLCHAIN_PREFIX)ar$(TOOLCHAIN_SUFFIX)
endif

STDCXX?=c++20

ifneq ($(STDCXX),)
CXXFLAGS_STDCXX = -std=$(STDCXX) -fexceptions -frtti -pthread
else ifeq ($(shell printf '\n' > bin/empty.cpp ; if $(CXX) -std=c++23 -c bin/empty.cpp -o bin/empty.out > /dev/null 2>&1 ; then echo 'c++23' ; fi ), c++23)
CXXFLAGS_STDCXX = -std=c++23 -fexceptions -frtti -pthread
else ifeq ($(shell printf '\n' > bin/empty.cpp ; if $(CXX) -std=c++20 -c bin/empty.cpp -o bin/empty.out > /dev/null 2>&1 ; then echo 'c++20' ; fi ), c++20)
CXXFLAGS_STDCXX = -std=c++20 -fexceptions -frtti -pthread
else
CXXFLAGS_STDCXX = -std=c++17 -fexceptions -frtti -pthread
endif
ifneq ($(STDC),)
CFLAGS_STDC = -std=$(STDC) -pthread
else ifeq ($(shell printf '\n' > bin/empty.c ; if $(CC) -std=c23 -c bin/empty.c -o bin/empty.out > /dev/null 2>&1 ; then echo 'c23' ; fi ), c23)
CFLAGS_STDC = -std=c23 -pthread
else ifeq ($(shell printf '\n' > bin/empty.c ; if $(CC) -std=c18 -c bin/empty.c -o bin/empty.out > /dev/null 2>&1 ; then echo 'c18' ; fi ), c18)
CFLAGS_STDC = -std=c18 -pthread
else ifeq ($(shell printf '\n' > bin/empty.c ; if $(CC) -std=c17 -c bin/empty.c -o bin/empty.out > /dev/null 2>&1 ; then echo 'c17' ; fi ), c17)
CFLAGS_STDC = -std=c17 -pthread
else
CFLAGS_STDC = -std=c11 -pthread
endif
CXXFLAGS += $(CXXFLAGS_STDCXX)
CFLAGS += $(CFLAGS_STDC)
LDFLAGS += -pthread

CPPFLAGS +=
CXXFLAGS += -fPIC
CFLAGS   += -fPIC
LDFLAGS  += 
LDLIBS   += -lm
ARFLAGS  := rcs

MODERN=1
NATIVE=1
OPTIMIZE=vectorize
OPTIMIZE_LTO=1

ifeq ($(NATIVE),1)
CXXFLAGS += -march=native
CFLAGS   += -march=native
endif

ifeq ($(OPTIMIZE_LTO),1)
CXXFLAGS += -flto
CFLAGS   += -flto
LDFLAGS  += -flto
endif

ifeq ($(CHECKED_ADDRESS),1)
CXXFLAGS += -fsanitize=address
CFLAGS   += -fsanitize=address
endif

ifeq ($(CHECKED_UNDEFINED),1)
CXXFLAGS += -fsanitize=undefined
CFLAGS   += -fsanitize=undefined
endif

include build/make/warnings-clang.mk

EXESUFFIX=
