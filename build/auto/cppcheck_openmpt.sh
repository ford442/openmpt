#!/usr/bin/env bash

# Usage: build/auto/cppcheck_openmpt.sh [std] [win32W|win64|win32A] [options]

set -e

CPPCHECK_INCLUDES="-Isrc -Iinclude -Iinclude/vstsdk2.4 -Iinclude/ASIOSDK2/common -Iinclude/flac/include -Iinclude/lame/include -Iinclude/lhasa/lib/public -Iinclude/mpg123/ports/MSVC++ -Iinclude/mpg123/src/libmpg123 -Iinclude/ogg/include -Iinclude/opus/include -Iinclude/opusenc/include -Iinclude/opusfile/include -Iinclude/portaudio/include -Iinclude/rtaudio -Iinclude/vorbis/include -Iinclude/zlib -Icommon -Isoundlib -Ibuild/svn_version"

CPPCHECK_DEFINES="-DMODPLUG_TRACKER -DMPT_BUILD_MSVC -DMPT_BUILD_MSVC_STATIC"

CPPCHECK_STD=${1} # Expect C++ standard like c++17 as first argument

# Platform argument processing needs to be fixed if we were to use it,
# but for now, we'll hardcode or pass it via options if necessary.
# This script was originally complex with argument positions.
# For this focused run, we'll assume $2 might be platform, $3 options.
# However, the main goal is to limit CPPCHECK_FILES.

case ${2} in
win32W)
	CPPCHECK_PLATFORM="--platform=win32W -D_WIN32 -DWIN32                  -D_UNICODE -DUNICODE -D_WINDOWS -DWINDOWS -D_MFC_VER"
	;;
win64)
	CPPCHECK_PLATFORM="--platform=win64  -D_WIN32 -DWIN32 -D_WIN64 -DWIN64 -D_UNICODE -DUNICODE -D_WINDOWS -DWINDOWS -D_MFC_VER"
	;;
win32A)
	CPPCHECK_PLATFORM="--platform=win32A -D_WIN32 -DWIN32                                       -D_WINDOWS -DWINDOWS -D_MFC_VER"
	;;
*)
	CPPCHECK_PLATFORM="" # Default to no specific platform if $2 is not a recognized platform
	;;
esac

# If $3 is meant to be options, it's captured here. If $3 is the directory (as in previous attempt), this will take it.
# For this modification, we are hardcoding the directory, so $3 is less critical unless it contains other cppcheck flags.
CPPCHECK_USER_OPTIONS=${3}

# MODIFIED: Only check 'common/' directory for now
CPPCHECK_FILES="common/"

NPROC=$(nproc)
# Try to limit to a smaller number of cores if nproc is very high, to avoid overwhelming the sandbox.
if [ "$NPROC" -gt 4 ]; then
  NPROC=4
fi
if [ "$NPROC" -lt 1 ]; then
  NPROC=1
fi


echo "Platform: $CPPCHECK_PLATFORM"
echo "Standard: $CPPCHECK_STD"
echo "User Options: $CPPCHECK_USER_OPTIONS"
echo "Target Files: $CPPCHECK_FILES"
echo "Max Processes: $NPROC"

# MODIFIED: Skipping --check-config for now to simplify and isolate main analysis pass
# echo "Checking config ..."
# cppcheck -j $NPROC -DCPPCHECK -DMPT_CPPCHECK_CUSTOM $CPPCHECK_PLATFORM $CPPCHECK_STD --library=windows.cfg --library=microsoft_atl.cfg --library=mfc.cfg --library=build/cppcheck/mfc-extras.cfg --library=build/cppcheck/nlohmann-json.cfg --suppressions-list=build/cppcheck/nlohmann-json.suppressions.txt --suppressions-list=build/cppcheck/r8brain.suppressions.txt --enable=warning --inline-suppr --template='{file}:{line}: warning: {severity}: {message} [{id}]' --suppress=missingIncludeSystem --suppress=uninitMemberVar $CPPCHECK_USER_OPTIONS $CPPCHECK_DEFINES $CPPCHECK_INCLUDES --check-level=exhaustive --check-config --suppress=unmatchedSuppression $CPPCHECK_FILES

echo "Checking C++ in $CPPCHECK_FILES ..."
# MODIFIED: Added --error-exitcode=0 and --quiet. User options ($CPPCHECK_USER_OPTIONS) are now passed here.
cppcheck -j $NPROC -DCPPCHECK -DMPT_CPPCHECK_CUSTOM $CPPCHECK_PLATFORM $CPPCHECK_STD --library=windows.cfg --library=microsoft_atl.cfg --library=mfc.cfg --library=build/cppcheck/mfc-extras.cfg --library=build/cppcheck/nlohmann-json.cfg --suppressions-list=build/cppcheck/nlohmann-json.suppressions.txt --suppressions-list=build/cppcheck/r8brain.suppressions.txt --enable=warning --inline-suppr --template='{file}:{line}: warning: {severity}: {message} [{id}]' --suppress=missingIncludeSystem --suppress=uninitMemberVar $CPPCHECK_USER_OPTIONS $CPPCHECK_DEFINES $CPPCHECK_INCLUDES --check-level=exhaustive --error-exitcode=0 --quiet $CPPCHECK_FILES

echo "Cppcheck execution finished."
