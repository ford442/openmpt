
ifeq ($(origin CC),default)
CC  = emcc
endif
ifeq ($(origin CXX),default)
CXX = em++
endif
ifeq ($(origin LD),default)
LD  = wasm-ld
endif
ifeq ($(origin AR),default)
AR  = emar
endif
LINK.cc = em++ $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)

EMSCRIPTEN_TARGET?=default
EMSCRIPTEN_THREADS?=0
EMSCRIPTEN_PORTS?=0

ifneq ($(STDCXX),)
CXXFLAGS_STDCXX = -std=$(STDCXX)
else ifeq ($(shell printf '\n' > bin/empty.cpp ; if $(CXX) -std=c++26 -c bin/empty.cpp -o bin/empty.out > /dev/null 2>&1 ; then echo 'c++26' ; fi ), c++26)
CXXFLAGS_STDCXX = -std=c++26
else ifeq ($(shell printf '\n' > bin/empty.cpp ; if $(CXX) -std=c++20 -c bin/empty.cpp -o bin/empty.out > /dev/null 2>&1 ; then echo 'c++20' ; fi ), c++20)
CXXFLAGS_STDCXX = -std=c++20
else
CXXFLAGS_STDCXX = -std=c++17
endif
ifneq ($(STDC),)
CFLAGS_STDC = -std=$(STDC)
else ifeq ($(shell printf '\n' > bin/empty.c ; if $(CC) -std=c26 -c bin/empty.c -o bin/empty.out > /dev/null 2>&1 ; then echo 'c26' ; fi ), c26)
CFLAGS_STDC = -std=c26
else ifeq ($(shell printf '\n' > bin/empty.c ; if $(CC) -std=c18 -c bin/empty.c -o bin/empty.out > /dev/null 2>&1 ; then echo 'c18' ; fi ), c18)
CFLAGS_STDC = -std=c18
else ifeq ($(shell printf '\n' > bin/empty.c ; if $(CC) -std=c17 -c bin/empty.c -o bin/empty.out > /dev/null 2>&1 ; then echo 'c17' ; fi ), c17)
CFLAGS_STDC = -std=c17
else
CFLAGS_STDC = -std=c11
endif
CXXFLAGS += $(CXXFLAGS_STDCXX)
CFLAGS += $(CFLAGS_STDC)

CPPFLAGS +=
CXXFLAGS += -fPIC
CFLAGS   += -fPIC
LDFLAGS  +=
LDLIBS   +=
ARFLAGS  := rcs

ifeq ($(EMSCRIPTEN_THREADS),1)
CXXFLAGS += -pthread
CFLAGS   += -pthread
LDFLAGS  += -pthread
endif

ifeq ($(EMSCRIPTEN_PORTS),1)
ifeq ($(ANCIENT),1)
CXXFLAGS += -s USE_ZLIB=1 -sUSE_MPG123=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -DMPT_WITH_ZLIB -DMPT_WITH_MPG123 -DMPT_WITH_VORBIS -DMPT_WITH_VORBISFILE -DMPT_WITH_OGG
CFLAGS   += -s USE_ZLIB=1 -sUSE_MPG123=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -DMPT_WITH_ZLIB -DMPT_WITH_MPG123 -DMPT_WITH_VORBIS -DMPT_WITH_VORBISFILE -DMPT_WITH_OGG
LDFLAGS  += -s USE_ZLIB=1 -sUSE_MPG123=1 -sUSE_OGG=1 -sUSE_VORBIS=1
else
CXXFLAGS += --use-port=zlib --use-port=mpg123 --use-port=vorbis --use-port=ogg -DMPT_WITH_ZLIB -DMPT_WITH_MPG123 -DMPT_WITH_VORBIS -DMPT_WITH_VORBISFILE -DMPT_WITH_OGG
CFLAGS   += --use-port=zlib --use-port=mpg123 --use-port=vorbis --use-port=ogg -DMPT_WITH_ZLIB -DMPT_WITH_MPG123 -DMPT_WITH_VORBIS -DMPT_WITH_VORBISFILE -DMPT_WITH_OGG
LDFLAGS  += --use-port=zlib --use-port=mpg123 --use-port=vorbis --use-port=ogg
endif
NO_MINIZ=1
NO_MINIMP3=1
NO_STBVORBIS=1
endif
NO_MINIZ=1
NO_MINIMP3=1
NO_STBVORBIS=1

CXXFLAGS += -O1
CFLAGS   += -O1
LDFLAGS  += -O1

# Enable LTO as recommended by Emscripten
#CXXFLAGS += -flto=thin
#CFLAGS   += -flto=thin
#LDFLAGS  += -flto=thin -Wl,--thinlto-jobs=all
# As per recommendation in <https://github.com/emscripten-core/emscripten/issues/15638#issuecomment-982772770>,
# thinLTO is not as well tested as full LTO. Stick to full LTO for now.
CXXFLAGS += -flto
CFLAGS   += -flto
LDFLAGS  += -flto

ifeq ($(EMSCRIPTEN_TARGET),default)
# emits whatever is emscripten's default, currently (13.1.51) this is the same as "wasm" below.
CPPFLAGS += 
CXXFLAGS += 
CFLAGS   += 
LDFLAGS  += 

LDFLAGS += -s ALLOW_MEMORY_GROWTH=1

else ifeq ($(EMSCRIPTEN_TARGET),all)
# emits native wasm AND javascript with full wasm optimizations.
CPPFLAGS += 
CXXFLAGS += 
CFLAGS   += 
LDFLAGS  += -s WASM=2 -s LEGACY_VM_SUPPORT=1

# work-around <https://github.com/emscripten-core/emscripten/issues/17897>.
CXXFLAGS += -fno-inline-functions
CFLAGS   += -fno-inline-functions
LDFLAGS  += -fno-inline-functions

LDFLAGS += -s ALLOW_MEMORY_GROWTH=1

else ifeq ($(EMSCRIPTEN_TARGET),audioworkletprocessor)
# emits an es6 module in a single file suitable for use in an AudioWorkletProcessor
CPPFLAGS += -DMPT_BUILD_AUDIOWORKLETPROCESSOR
CXXFLAGS += 
CFLAGS   += 
LDFLAGS  += -s WASM=1 -s WASM_ASYNC_COMPILATION=0 -s MODULARIZE=1 -s EXPORT_ES6=1 -s SINGLE_FILE=1

LDFLAGS += -s ALLOW_MEMORY_GROWTH=1

else ifeq ($(EMSCRIPTEN_TARGET),wasm)
# emits native wasm.
CPPFLAGS += 
CXXFLAGS += 
CFLAGS   += 
LDFLAGS  += -s WASM=1

LDFLAGS += -s ALLOW_MEMORY_GROWTH=1


else ifeq ($(EMSCRIPTEN_TARGET),1it1-new2)
LINK_SIMD_FLAGS = --enable-simd -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -msimd128 -mavx2 -mrelaxed-simd -fopenmp-simd
SIMD_FLAGS = -DSIMD=AVX -msse4.2 -msimd128 -mavx2 -mrelaxed-simd -fopenmp-simd
CPPFLAGS += -fno-fast-math -ffp-contract=off -ffp-model=strict -fno-math-errno -mextended-const -mbulk-memory -matomics -mmutable-globals -msign-ext -fmerge-all-constants
CXXFLAGS += -fno-fast-math -ffp-contract=off -ffp-model=strict -fno-math-errno -mextended-const -mbulk-memory -matomics -mmutable-globals -msign-ext -fmerge-all-constants
CFLAGS   += -fno-fast-math -ffp-contract=off -ffp-model=strict -fno-math-errno -mextended-const -mbulk-memory -matomics -mmutable-globals -msign-ext -fmerge-all-constants
LDFLAGS  += -DNDEBUG=1 \
-sTRUSTED_TYPES=1 -pipe -dead-strip -mtune=wasm32 -polly -polly-position=before-vectorizer \
-ffp-contract=off -ffp-model=strict -stdlib=libc++ -sALLOW_UNIMPLEMENTED_SYSCALLS=1 \
-fno-fast-math -mextended-const -mbulk-memory --typed-function-references --enable-reference-types \
-matomics -mmutable-globals -msign-ext -fmerge-all-constants -fno-math-errno \
-sWASM=0 -sFORCE_FILESYSTEM=1 -sALLOW_MEMORY_GROWTH=0 -sINITIAL_HEAP=512mb \
-rtlib=compiler-rt -sENVIRONMENT=web -sASYNCIFY=0 -sMALLOC='emmalloc' \
--output_eol linux --use-preload-plugins --closure 0 --closureFriendly -sSTRICT_JS=0 -sASSERTIONS=0


else ifeq ($(EMSCRIPTEN_TARGET),1it1-new)
LINK_SIMD_FLAGS = -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -msimd128 -openmp-simd
SIMD_FLAGS = -DSIMD=AVX -msimd128 -mavx -openmp-simd
CPPFLAGS += -fno-fast-math -ffp-contract=off -fexcess-precision=standard 
CXXFLAGS += -fno-fast-math -ffp-contract=off -fexcess-precision=standard 
CFLAGS   += -fno-fast-math -ffp-contract=off -fexcess-precision=standard 
LDFLAGS  += -DNDEBUG=1 \
-sTRUSTED_TYPES=1 -pipe -dead-strip -fno-fast-math -mtune=wasm32 -polly -polly-position=before-vectorizer \
-ffp-contract=off -fexcess-precision=standard -stdlib=libc++ -sALLOW_UNIMPLEMENTED_SYSCALLS=1 \
-mextended-const -mbulk-memory --typed-function-references --enable-reference-types \
-matomics -mmutable-globals -msign-ext -fmerge-all-constants -fno-omit-frame-pointer \
-sWASM=0 -sFORCE_FILESYSTEM=1 -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=700mb \
-march=haswell -rtlib=compiler-rt -sENVIRONMENT=web -sASYNCIFY=0 \
--output_eol linux --use-preload-plugins --closure 0 --closureFriendly -sSTRICT_JS=0


else ifeq ($(EMSCRIPTEN_TARGET),1it1-dbg)
LINK_SIMD_FLAGS = -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx -msimd128
SIMD_FLAGS = -DSIMD=AVX -msimd128 -mavx
CPPFLAGS += -fno-fast-math -ffp-contract=off -fexcess-precision=standard 
CXXFLAGS += -fno-fast-math -ffp-contract=off -fexcess-precision=standard 
CFLAGS   += -fno-fast-math -ffp-contract=off -fexcess-precision=standard 
LDFLAGS  += -DNDEBUG=1 \
-sTRUSTED_TYPES=1 -pipe -dead-strip -fno-fast-math -mtune=wasm32 -polly -polly-position=before-vectorizer \
-ffp-contract=off -fexcess-precision=standard -stdlib=libc++ -sALLOW_UNIMPLEMENTED_SYSCALLS=1 \
-mextended-const -mbulk-memory --typed-function-references --enable-reference-types \
-matomics -mmutable-globals -msign-ext -fmerge-all-constants -fno-omit-frame-pointer \
-sWASM=0 -sFORCE_FILESYSTEM=1 -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=700mb -sMALLOC='mimalloc' \
-march=haswell -rtlib=compiler-rt -sENVIRONMENT=web -sASYNCIFY=0 -sEXIT_RUNTIME=0 \
--output_eol linux --use-preload-plugins --closure 0 --closureFriendly -sSTRICT_JS=0 -sASSERTIONS=0


else ifeq ($(EMSCRIPTEN_TARGET),js)
# emits only plain javascript with plain javascript focused optimizations.
CPPFLAGS += 
CXXFLAGS += 
CFLAGS   += 
LDFLAGS  += -s WASM=0 -s LEGACY_VM_SUPPORT=1

# work-around <https://github.com/emscripten-core/emscripten/issues/17897>.
CXXFLAGS += -fno-inline-functions
CFLAGS   += -fno-inline-functions
LDFLAGS  += -fno-inline-functions

LDFLAGS += -s ALLOW_MEMORY_GROWTH=1

endif

CXXFLAGS += -s DISABLE_EXCEPTION_CATCHING=0
CFLAGS   += -s DISABLE_EXCEPTION_CATCHING=0 -fno-strict-aliasing
LDFLAGS  += -s DISABLE_EXCEPTION_CATCHING=0 -s ERROR_ON_UNDEFINED_SYMBOLS=1 -s ERROR_ON_MISSING_LIBRARIES=1 -s EXPORT_NAME="'libopenmpt'"
SO_LDFLAGS += -sEXPORTED_FUNCTIONS="['_malloc','_free']" -sEXPORTED_RUNTIME_METHODS="['HEAPU8','HEAPF32']" \


NO_NO_UNDEFINED_LINKER_FLAG=1

include build/make/warnings-clang.mk

REQUIRES_RUNPREFIX=1

EXESUFFIX=.js
SOSUFFIX=.js
RUNPREFIX=node 
TEST_LDFLAGS= -lnodefs.js 

ifeq ($(EMSCRIPTEN_THREADS),1)
RUNPREFIX+=--experimental-wasm-threads --experimental-wasm-bulk-memory 
endif

DYNLINK=0
SHARED_LIB=1
STATIC_LIB=0
EXAMPLES=0
OPENMPT123=0
SHARED_SONAME=0
NO_SHARED_LINKER_FLAG=1

# Disable the generic compiler optimization flags as emscripten is sufficiently different.
# Optimization flags are hard-coded for emscripten in this file.
DEBUG=0
OPTIMIZE=none

IS_CROSS=1

ifeq ($(ALLOW_LGPL),1)
LOCAL_ZLIB=1
LOCAL_MPG123=1
LOCAL_OGG=1
LOCAL_VORBIS=1
else
NO_ZLIB=1
NO_MPG123=1
NO_OGG=1
NO_VORBIS=1
NO_VORBISFILE=1
endif
NO_PORTAUDIO=1
NO_PORTAUDIOCPP=1
NO_PULSEAUDIO=1
NO_SDL2=1
NO_FLAC=1
NO_SNDFILE=1

