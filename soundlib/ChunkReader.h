
#pragma once
#include "openmpt/all/BuildSettings.hpp"
#include "../common/FileReader.h"
#include <vector>
OPENMPT_NAMESPACE_BEGIN


class ChunkReader : public FileReader {
public:
template<typename Tbyte>
ChunkReader(mpt::span<Tbyte> bytedata) : FileReader(bytedata) {}
ChunkReader(const FileCursor &other) : FileReader(other) {}
ChunkReader(FileCursor &&other) : FileReader(std::move(other)) {}
template<typename T>
class Item {
private:
T chunkHeader;
FileReader chunkData;
public:
Item(const T &header, FileReader &&data) : chunkHeader(header), chunkData(std::move(data)) {}
Item(const Item<T> &) = default;
Item(Item<T> &&)
noexcept =
default;
const T &GetHeader() const { return chunkHeader; }
const FileReader &GetData() const { return chunkData; }
};
template<typename T>
class ChunkList : public std::vector<Item<T>> {
public:
typedef decltype(T()
.
GetID()
)
id_type;
bool ChunkExists(id_type id) const {
return std::find_if(this->cbegin(), this->cend(),
                    [&id](const Item<T> &item) { return item.GetHeader().GetID() == id; }) != this->cend();
}
FileReader GetChunk(id_type id) const {
auto item = std::find_if(this->cbegin(), this->cend(),
                         [&id](const Item<T> &item) { return item.GetHeader().GetID() == id; });
if(item != this->cend())
return item->GetData();
return FileReader();
}
std::vector <FileReader> GetAllChunks(id_type id) const {
std::vector <FileReader> result;
for(const auto &item : *this) {
if(item.GetHeader().GetID() == id) {
result.push_back(item.GetData());
}
}
return result;
}
};
template<typename T>
Item<T> GetNextChunk(off_t padding) {
T chunkHeader;
off_t dataSize = 0;
if(Read(chunkHeader)) {
dataSize = chunkHeader.GetLength();
}
Item<T> resultItem(chunkHeader, ReadChunk(dataSize));
if(padding != 0 && dataSize % padding != 0) {
Skip(padding - (dataSize % padding));
}
return resultItem;
}
template<typename T>
ChunkList<T> ReadChunks(off_t padding) {
ChunkList<T> result;
while (CanRead(sizeof(T))) {
result.push_back(GetNextChunk<T>(padding));
}
return result;
}
template<typename T>
ChunkList<T> ReadChunksUntil(off_t padding, decltype(T()
.
GetID()
) stopAtID)
{
ChunkList<T> result;
while (CanRead(sizeof(T))) {
result.push_back(GetNextChunk<T>(padding));
if(result.back().GetHeader().GetID() == stopAtID) {
break;
}
}
return result;
}
};
OPENMPT_NAMESPACE_END
