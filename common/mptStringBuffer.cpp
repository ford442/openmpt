
#include "stdafx.h"
#include "mptStringBuffer.h"
OPENMPT_NAMESPACE_BEGIN
namespace mpt {
namespace String {
namespace detail {
std::string ReadStringBuffer(String::ReadWriteMode mode, const char *srcBuffer, std::size_t srcSize) {
std::string dest;
const char *src = srcBuffer;
if(mode == nullTerminated || mode == spacePaddedNull) {
if(srcSize > 0) {
srcSize -= 1;
}
}
if(mode == nullTerminated || mode == maybeNullTerminated) {
dest.assign(src, std::find(src, src + srcSize, '\0'));
} else if(mode == spacePadded || mode == spacePaddedNull) {
dest.assign(src, src + srcSize);
std::transform(dest.begin(), dest.end(), dest.begin(), [](char c) -> char { return (c != '\0') ? c : ' '; });
dest = mpt::trim_right(dest, std::string(" "));
}
return dest;
}
void WriteStringBuffer(String::ReadWriteMode mode, char *destBuffer, const std::size_t destSize, const char *srcBuffer,
                       const std::size_t srcSize) {
MPT_ASSERT(destSize > 0);
const size_t maxSize = std::min(destSize, srcSize);
char *dst = destBuffer;
const char *src = srcBuffer;
size_t pos = maxSize;
while (pos > 0) {
if((*dst = *src) == '\0') {
break;
}
pos--;
dst++;
src++;
}
if(mode == nullTerminated || mode == maybeNullTerminated) {
std::fill(dst, dst + destSize - maxSize + pos, '\0');
} else if(mode == spacePadded || mode == spacePaddedNull) {
std::fill(dst, dst + destSize - maxSize + pos, ' ');
}
if(mode == nullTerminated || mode == spacePaddedNull) {
SetNullTerminator(destBuffer, destSize);
}
}
} // namespace detail

} // namespace String

} // namespace mpt



OPENMPT_NAMESPACE_END
