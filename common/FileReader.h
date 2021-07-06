
#pragma once
#include "openmpt/all/BuildSettings.hpp"
#include "mpt/io_read/filecursor.hpp"
#include "mpt/io_read/filecursor_filename_traits.hpp"
#include "mpt/io_read/filecursor_traits_filedata.hpp"
#include "mpt/io_read/filecursor_traits_memory.hpp"
#include "mpt/io_read/filereader.hpp"
#include "openmpt/base/Types.hpp"
#include "mptPathString.h"
#include "mptStringBuffer.h"
#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <cstring>
#include "FileReaderFwd.h"
OPENMPT_NAMESPACE_BEGIN
namespace FileReaderExt {
// The file cursor is advanced by "srcSize" bytes.
template<mpt::String::ReadWriteMode mode, size_t destSize, typename TFileCursor>
bool ReadString(TFileCursor &f, char (&destBuffer)[destSize], const typename TFileCursor::pos_type srcSize) {
typename TFileCursor::PinnedView source = f.ReadPinnedView(srcSize); // Make sure the string is cached properly.
typename TFileCursor::pos_type realSrcSize = source.size();    // In case fewer bytes are available
mpt::String::WriteAutoBuf(destBuffer) = mpt::String::ReadBuf(mode, mpt::byte_cast<const char *>(source.data()),
                                                             realSrcSize);
return (realSrcSize > 0 || srcSize == 0);
}
// The file cursor is advanced by "srcSize" bytes.
template<mpt::String::ReadWriteMode mode, typename TFileCursor>
bool ReadString(TFileCursor &f, std::string &dest, const typename TFileCursor::pos_type srcSize) {
dest.clear();
typename TFileCursor::PinnedView source = f.ReadPinnedView(srcSize);    // Make sure the string is cached properly.
typename TFileCursor::pos_type realSrcSize = source.size();    // In case fewer bytes are available
dest = mpt::String::ReadBuf(mode, mpt::byte_cast<const char *>(source.data()), realSrcSize);
return (realSrcSize > 0 || srcSize == 0);
}
// The file cursor is advanced by "srcSize" bytes.
template<mpt::String::ReadWriteMode mode, std::size_t len, typename TFileCursor>
bool ReadString(TFileCursor &f, mpt::charbuf<len> &dest, const typename TFileCursor::pos_type srcSize) {
typename TFileCursor::PinnedView source = f.ReadPinnedView(srcSize);    // Make sure the string is cached properly.
typename TFileCursor::pos_type realSrcSize = source.size();    // In case fewer bytes are available
dest = mpt::String::ReadBuf(mode, mpt::byte_cast<const char *>(source.data()), realSrcSize);
return (realSrcSize > 0 || srcSize == 0);
}
// The file cursor is advanced by "srcSize" bytes.
template<mpt::String::ReadWriteMode mode, typename TFileCursor>
bool
ReadString(TFileCursor &f, mpt::ustring &dest, mpt::Charset charset, const typename TFileCursor::pos_type srcSize) {
dest.clear();
typename TFileCursor::PinnedView source = f.ReadPinnedView(srcSize);    // Make sure the string is cached properly.
typename TFileCursor::pos_type realSrcSize = source.size();    // In case fewer bytes are available
dest = mpt::ToUnicode(charset, mpt::String::ReadBuf(mode, mpt::byte_cast<const char *>(source.data()), realSrcSize));
return (realSrcSize > 0 || srcSize == 0);
}
// The file cursor is advanced by the string length.
template<typename Tsize, mpt::String::ReadWriteMode mode, size_t destSize, typename TFileCursor>
bool ReadSizedString(TFileCursor &f, char (&destBuffer)[destSize],
                     const typename TFileCursor::pos_type maxLength = std::numeric_limits<typename TFileCursor::pos_type>::max()) {
mpt::packed<typename Tsize::base_type, typename Tsize::endian_type> srcSize;    // Enforce usage of a packed type by ensuring that the passed type has the required typedefs
if(!mpt::IO::FileReader::Read(f, srcSize)) {
return false;
}
return FileReaderExt::ReadString<mode>(f, destBuffer,
                                       std::min(static_cast<typename TFileCursor::pos_type>(srcSize), maxLength));
}
// The file cursor is advanced by the string length.
template<typename Tsize, mpt::String::ReadWriteMode mode, typename TFileCursor>
bool ReadSizedString(TFileCursor &f, std::string &dest,
                     const typename TFileCursor::pos_type maxLength = std::numeric_limits<typename TFileCursor::pos_type>::max()) {
mpt::packed<typename Tsize::base_type, typename Tsize::endian_type> srcSize;    // Enforce usage of a packed type by ensuring that the passed type has the required typedefs
if(!mpt::IO::FileReader::Read(f, srcSize)) {
return false;
}
return FileReaderExt::ReadString<mode>(f, dest,
                                       std::min(static_cast<typename TFileCursor::pos_type>(srcSize), maxLength));
}
// The file cursor is advanced by the string length.
template<typename Tsize, mpt::String::ReadWriteMode mode, std::size_t len, typename TFileCursor>
bool ReadSizedString(TFileCursor &f, mpt::charbuf<len> &dest,
                     const typename TFileCursor::pos_type maxLength = std::numeric_limits<typename TFileCursor::pos_type>::max()) {
mpt::packed<typename Tsize::base_type, typename Tsize::endian_type> srcSize;    // Enforce usage of a packed type by ensuring that the passed type has the required typedefs
if(!mpt::IO::FileReader::Read(f, srcSize)) {
return false;
}
return FileReaderExt::ReadString<mode>(f, dest,
                                       std::min(static_cast<typename TFileCursor::pos_type>(srcSize), maxLength));
}
} // namespace FileReaderExt

namespace detail {
template<typename Ttraits, typename Tfilenametraits>
using FileCursor = mpt::IO::FileCursor<Ttraits, Tfilenametraits>;
template<typename Ttraits, typename Tfilenametraits>
class FileReader
        : public FileCursor<Ttraits, Tfilenametraits> {
private:
using traits_type = Ttraits;
using filename_traits_type = Tfilenametraits;
public:
using pos_type = typename traits_type::pos_type;
using off_t = pos_type;

using data_type = typename traits_type::data_type;
using ref_data_type = typename traits_type::ref_data_type;
using shared_data_type = typename traits_type::shared_data_type;
using value_data_type = typename traits_type::value_data_type;

using shared_filename_type = typename filename_traits_type::shared_filename_type;
public:
FileReader() {
return;
}
FileReader(const FileCursor<Ttraits, Tfilenametraits> &other)
        : FileCursor<Ttraits, Tfilenametraits>(other) {
return;
}
FileReader(FileCursor<Ttraits, Tfilenametraits> &&other)
        : FileCursor<Ttraits, Tfilenametraits>(std::move(other)) {
return;
}
template<typename Tbyte>
explicit FileReader(mpt::span<Tbyte> bytedata, shared_filename_type filename = shared_filename_type{})
        : FileCursor<Ttraits, Tfilenametraits>(bytedata, std::move(filename)) {
return;
}
explicit FileReader(value_data_type other, shared_filename_type filename = shared_filename_type{})
        : FileCursor<Ttraits, Tfilenametraits>(std::move(other), std::move(filename)) {
return;
}
public:
template<typename T>
bool Read(T &target) {
return mpt::IO::FileReader::Read(*this, target);
}
template<typename T>
T ReadIntLE() {
return mpt::IO::FileReader::ReadIntLE<T>(*this);
}
template<typename T>
T ReadIntBE() {
return mpt::IO::FileReader::ReadIntLE<T>(*this);
}
template<typename T>
T ReadTruncatedIntLE(pos_type size) {
return mpt::IO::FileReader::ReadTruncatedIntLE<T>(*this, size);
}
template<typename T>
T ReadSizedIntLE(pos_type size) {
return mpt::IO::FileReader::ReadSizedIntLE<T>(*this, size);
}
uint32 ReadUint32LE() {
return mpt::IO::FileReader::ReadUint32LE(*this);
}
uint32 ReadUint32BE() {
return mpt::IO::FileReader::ReadUint32BE(*this);
}
int32 ReadInt32LE() {
return mpt::IO::FileReader::ReadInt32LE(*this);
}
int32 ReadInt32BE() {
return mpt::IO::FileReader::ReadInt32BE(*this);
}
uint32 ReadUint24LE() {
return mpt::IO::FileReader::ReadUint24LE(*this);
}
uint32 ReadUint24BE() {
return mpt::IO::FileReader::ReadUint24BE(*this);
}
uint16 ReadUint16LE() {
return mpt::IO::FileReader::ReadUint16LE(*this);
}
uint16 ReadUint16BE() {
return mpt::IO::FileReader::ReadUint16BE(*this);
}
int16 ReadInt16LE() {
return mpt::IO::FileReader::ReadInt16LE(*this);
}
int16 ReadInt16BE() {
return mpt::IO::FileReader::ReadInt16BE(*this);
}
char ReadChar() {
return mpt::IO::FileReader::ReadChar(*this);
}
uint8 ReadUint8() {
return mpt::IO::FileReader::ReadUint8(*this);
}
int8 ReadInt8() {
return mpt::IO::FileReader::ReadInt8(*this);
}
float ReadFloatLE() {
return mpt::IO::FileReader::ReadFloatLE(*this);
}
float ReadFloatBE() {
return mpt::IO::FileReader::ReadFloatBE(*this);
}
double ReadDoubleLE() {
return mpt::IO::FileReader::ReadDoubleLE(*this);
}
double ReadDoubleBE() {
return mpt::IO::FileReader::ReadDoubleBE(*this);
}
template<typename T>
bool ReadStruct(T &target) {
return mpt::IO::FileReader::ReadStruct(*this, target);
}
template<typename T>
size_t ReadStructPartial(T &target, size_t partialSize = sizeof(T)) {
return mpt::IO::FileReader::ReadStructPartial(*this, target, partialSize);
}
bool ReadNullString(std::string &dest, const pos_type maxLength = std::numeric_limits<pos_type>::max()) {
return mpt::IO::FileReader::ReadNullString(*this, dest, maxLength);
}
bool ReadLine(std::string &dest, const pos_type maxLength = std::numeric_limits<pos_type>::max()) {
return mpt::IO::FileReader::ReadLine(*this, dest, maxLength);
}
template<typename T, std::size_t destSize>
bool ReadArray(T (&destArray)[destSize]) {
return mpt::IO::FileReader::ReadArray(*this, destArray);
}
template<typename T, std::size_t destSize>
bool ReadArray(std::array <T, destSize> &destArray) {
return mpt::IO::FileReader::ReadArray(*this, destArray);
}
template<typename T, std::size_t destSize>
std::array <T, destSize> ReadArray() {
return mpt::IO::FileReader::ReadArray<T, destSize>(*this);
}
template<typename T>
bool ReadVector(std::vector <T> &destVector, size_t destSize) {
return mpt::IO::FileReader::ReadVector(*this, destVector, destSize);
}
template<size_t N>
bool ReadMagic(const char (&magic)[N]) {
return mpt::IO::FileReader::ReadMagic(*this, magic);
}
template<typename T>
bool ReadVarInt(T &target) {
return mpt::IO::FileReader::ReadVarInt(*this, target);
}
template<mpt::String::ReadWriteMode mode, size_t destSize>
bool ReadString(char (&destBuffer)[destSize], const pos_type srcSize) {
return FileReaderExt::ReadString<mode>(*this, destBuffer, srcSize);
}
template<mpt::String::ReadWriteMode mode>
bool ReadString(std::string &dest, const pos_type srcSize) {
return FileReaderExt::ReadString<mode>(*this, dest, srcSize);
}
template<mpt::String::ReadWriteMode mode, std::size_t len>
bool ReadString(mpt::charbuf<len> &dest, const pos_type srcSize) {
return FileReaderExt::ReadString<mode>(*this, dest, srcSize);
}
template<mpt::String::ReadWriteMode mode>
bool ReadString(mpt::ustring &dest, mpt::Charset charset, const pos_type srcSize) {
return FileReaderExt::ReadString<mode>(*this, dest, charset, srcSize);
}
template<typename Tsize, mpt::String::ReadWriteMode mode, size_t destSize>
bool ReadSizedString(char (&destBuffer)[destSize], const pos_type maxLength = std::numeric_limits<pos_type>::max()) {
return FileReaderExt::ReadSizedString<Tsize, mode>(*this, destBuffer, maxLength);
}
template<typename Tsize, mpt::String::ReadWriteMode mode>
bool ReadSizedString(std::string &dest, const pos_type maxLength = std::numeric_limits<pos_type>::max()) {
return FileReaderExt::ReadSizedString<Tsize, mode>(*this, dest, maxLength);
}
template<typename Tsize, mpt::String::ReadWriteMode mode, std::size_t len>
bool ReadSizedString(mpt::charbuf<len> &dest, const pos_type maxLength = std::numeric_limits<pos_type>::max()) {
return FileReaderExt::ReadSizedString<Tsize, mode, len>(*this, dest, maxLength);
}
};
} // namespace detail

using FileCursor = detail::FileCursor<mpt::IO::FileCursorTraitsFileData, mpt::IO::FileCursorFilenameTraits<mpt::PathString>>;
using FileReader = detail::FileReader<mpt::IO::FileCursorTraitsFileData, mpt::IO::FileCursorFilenameTraits<mpt::PathString>>;

using MemoryFileCursor = detail::FileCursor<mpt::IO::FileCursorTraitsMemory, mpt::IO::FileCursorFilenameTraitsNone>;
using MemoryFileReader = detail::FileReader<mpt::IO::FileCursorTraitsMemory, mpt::IO::FileCursorFilenameTraitsNone>;


OPENMPT_NAMESPACE_END
