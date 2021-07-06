
#include "stdafx.h"
#include "Loaders.h"
#include "../common/version.h"
#include "XMTools.h"
#include "mod_specifications.h"
#ifndef MODPLUG_NO_FILESAVE
#include "mpt/io/base.hpp"
#include "mpt/io/io.hpp"
#include "mpt/io/io_stdstream.hpp"
#include "../common/mptFileIO.h"
#endif
#include "OggStream.h"
#include <algorithm>
#ifdef MODPLUG_TRACKER
#include "../mptrack/TrackerSettings.h"	// For super smooth ramping option
#endif // MODPLUG_TRACKER
#include "mpt/audio/span.hpp"
#if defined(MPT_WITH_VORBIS) && defined(MPT_WITH_VORBISFILE)
#include <sstream>
#endif
#if defined(MPT_WITH_VORBIS)
#if MPT_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif // MPT_COMPILER_CLANG
#include <vorbis/codec.h>
#if MPT_COMPILER_CLANG
#pragma clang diagnostic pop
#endif // MPT_COMPILER_CLANG
#endif
#if defined(MPT_WITH_VORBISFILE)
#if MPT_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif // MPT_COMPILER_CLANG
#include <vorbis/vorbisfile.h>
#if MPT_COMPILER_CLANG
#pragma clang diagnostic pop
#endif // MPT_COMPILER_CLANG
#include "openmpt/soundbase/Copy.hpp"
#endif
#ifdef MPT_WITH_STBVORBIS
#include <stb_vorbis/stb_vorbis.c>
#include "openmpt/soundbase/Copy.hpp"
#endif // MPT_WITH_STBVORBIS
OPENMPT_NAMESPACE_BEGIN
#if defined(MPT_WITH_VORBIS) && defined(MPT_WITH_VORBISFILE)

static size_t VorbisfileFilereaderRead(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    FileReader &file = *reinterpret_cast<FileReader*>(datasource);
    return file.ReadRaw(mpt::span(mpt::void_cast<std::byte*>(ptr), size * nmemb)).size() / size;
}

static int VorbisfileFilereaderSeek(void *datasource, ogg_int64_t offset, int whence)
{
    FileReader &file = *reinterpret_cast<FileReader*>(datasource);
    switch(whence)
    {
    case SEEK_SET:
        {
            if(!mpt::in_range<FileReader::off_t>(offset))
            {
                return -1;
            }
            return file.Seek(mpt::saturate_cast<FileReader::off_t>(offset)) ? 0 : -1;
        }
        break;
    case SEEK_CUR:
        {
            if(offset < 0)
            {
                if(offset == std::numeric_limits<ogg_int64_t>::min())
                {
                    return -1;
                }
                if(!mpt::in_range<FileReader::off_t>(0-offset))
                {
                    return -1;
                }
                return file.SkipBack(mpt::saturate_cast<FileReader::off_t>(0 - offset)) ? 0 : -1;
            } else
            {
                if(!mpt::in_range<FileReader::off_t>(offset))
                {
                    return -1;
                }
                return file.Skip(mpt::saturate_cast<FileReader::off_t>(offset)) ? 0 : -1;
            }
        }
        break;
    case SEEK_END:
        {
            if(!mpt::in_range<FileReader::off_t>(offset))
            {
                return -1;
            }
            if(!mpt::in_range<FileReader::off_t>(file.GetLength() + offset))
            {
                return -1;
            }
            return file.Seek(mpt::saturate_cast<FileReader::off_t>(file.GetLength() + offset)) ? 0 : -1;
        }
        break;
    default:
        return -1;
    }
}

static long VorbisfileFilereaderTell(void *datasource)
{
    FileReader &file = *reinterpret_cast<FileReader*>(datasource);
    FileReader::off_t result = file.GetPosition();
    if(!mpt::in_range<long>(result))
    {
        return -1;
    }
    return static_cast<long>(result);
}

#endif // MPT_WITH_VORBIS && MPT_WITH_VORBISFILE
static std::vector<SAMPLEINDEX>
AllocateXMSamples(CSoundFile
&sndFile,
SAMPLEINDEX numSamples
)
{
LimitMax(numSamples, SAMPLEINDEX(32)
);
std::vector <SAMPLEINDEX> foundSlots;
foundSlots.
reserve(numSamples);
for(
SAMPLEINDEX i = 0;
i<numSamples;
i++)
{
SAMPLEINDEX candidateSlot = sndFile.GetNumSamples() + 1;
if(candidateSlot >= MAX_SAMPLES)
{
for(
SAMPLEINDEX j = 1;
j <= sndFile.
GetNumSamples();
j++)
{
if(sndFile.
GetSample(j)
.
HasSampleData()
)
{
continue;
}

if(!
mpt::contains(foundSlots, j
))
{
candidateSlot = j;
for(
INSTRUMENTINDEX ins = 1;
ins <= sndFile.
GetNumInstruments();
ins++)
{
if(sndFile.Instruments[ins] == nullptr)
{
continue;
}
for(
auto &sample
: sndFile.Instruments[ins]->Keyboard)
{
if(sample == candidateSlot)
{
sample = 0;
}
}
}
break;
}
}
}

if(candidateSlot >= MAX_SAMPLES)
{
std::vector<bool> usedSamples;
SAMPLEINDEX unusedSampleCount = sndFile.DetectUnusedSamples(usedSamples);
if(unusedSampleCount > 0)
{
sndFile.
RemoveSelectedSamples(usedSamples);
for(
INSTRUMENTINDEX ins = 1;
ins <= sndFile.
GetNumInstruments();
ins++)
{
if(sndFile.Instruments[ins] == nullptr)
{
continue;
}
for(
auto &sample
: sndFile.Instruments[ins]->Keyboard)
{
if(sample<usedSamples.
size() &&
!usedSamples[sample])
{
sample = 0;
}
}
}
candidateSlot = static_cast<SAMPLEINDEX>(std::find(usedSamples.begin() + 1, usedSamples.end(), false) -
                                         usedSamples.begin());
} else
{
break;
}
}

if(candidateSlot<MAX_SAMPLES)
{
foundSlots.
push_back(candidateSlot);
if(candidateSlot > sndFile.
GetNumSamples()
)
{
sndFile.
m_nSamples = candidateSlot;
}
}
}

return
foundSlots;
}
static void ReadXMPatterns(FileReader &file, const XMFileHeader &fileHeader, CSoundFile &sndFile) {
sndFile.Patterns.ResizeArray(fileHeader.patterns);
for(PATTERNINDEX pat = 0; pat < fileHeader.patterns; pat++) {
FileReader::off_t curPos = file.GetPosition();
uint32 headerSize = file.ReadUint32LE();
file.Skip(1);    // Pack method (= 0)

ROWINDEX numRows = 64;
if(fileHeader.version == 0x0102) {
numRows = file.ReadUint8() + 1;
} else {
numRows = file.ReadUint16LE();
}
const uint16 packedSize = file.ReadUint16LE();
if(numRows == 0)
numRows = 64;
else if(numRows > MAX_PATTERN_ROWS)
numRows = MAX_PATTERN_ROWS;
file.Seek(curPos + headerSize);
FileReader patternChunk = file.ReadChunk(packedSize);
if(!sndFile.Patterns.Insert(pat, numRows) || packedSize == 0) {
continue;
}
enum PatternFlags {
isPackByte = 0x80,
allFlags = 0xFF,
notePresent = 0x01,
instrPresent = 0x02,
volPresent = 0x04,
commandPresent = 0x08,
paramPresent = 0x10,
};
for(auto &m : sndFile.Patterns[pat]) {
uint8 info = patternChunk.ReadUint8();
uint8 vol = 0;
if(info & isPackByte) {
if(info & notePresent) m.note = patternChunk.ReadUint8();
} else {
m.note = info;
info = allFlags;
}
if(info & instrPresent) m.instr = patternChunk.ReadUint8();
if(info & volPresent) vol = patternChunk.ReadUint8();
if(info & commandPresent) m.command = patternChunk.ReadUint8();
if(info & paramPresent) m.param = patternChunk.ReadUint8();
if(m.note == 97) {
m.note = NOTE_KEYOFF;
} else if(m.note > 0 && m.note < 97) {
m.note += 12;
} else {
m.note = NOTE_NONE;
}
if(m.command | m.param) {
CSoundFile::ConvertModCommand(m);
} else {
m.command = CMD_NONE;
}
if(m.instr == 0xFF) {
m.instr = 0;
}
if(vol >= 0x10 && vol <= 0x50) {
m.volcmd = VOLCMD_VOLUME;
m.vol = vol - 0x10;
} else if(vol >= 0x60) {
static constexpr ModCommand::VOLCMD
volEffTrans[] =
        {
                VOLCMD_VOLSLIDEDOWN, VOLCMD_VOLSLIDEUP, VOLCMD_FINEVOLDOWN, VOLCMD_FINEVOLUP,
                VOLCMD_VIBRATOSPEED, VOLCMD_VIBRATODEPTH, VOLCMD_PANNING, VOLCMD_PANSLIDELEFT,
                VOLCMD_PANSLIDERIGHT, VOLCMD_TONEPORTAMENTO,
        };
m.volcmd = volEffTrans[(vol - 0x60) >> 4];
m.vol = vol & 0x0F;
if(m.volcmd == VOLCMD_PANNING) {
m.vol *= 4;    // FT2 does indeed not scale panning symmetrically.
}
}
}
}
}
enum TrackerVersions {
verUnknown = 0x00,        // Probably not made with MPT
verOldModPlug = 0x01,        // Made with MPT Alpha / Beta
verNewModPlug = 0x02,        // Made with MPT (not Alpha / Beta)
verModPlug1_09 = 0x04,        // Made with MPT 1.09 or possibly other version
verOpenMPT = 0x08,        // Made with OpenMPT
verConfirmed = 0x10,        // We are very sure that we found the correct tracker version.

verFT2Generic = 0x20,        // "FastTracker v2.00", but FastTracker has NOT been ruled out
verOther = 0x40,        // Something we don't know, testing for DigiTrakker.
verFT2Clone = 0x80,        // NOT FT2: itype changed between instruments, or \0 found in song title
verDigiTrakker = 0x100,    // Probably DigiTrakker
verUNMO3 = 0x200,    // TODO: UNMO3-ed XMs are detected as MPT 1.16
verEmptyOrders = 0x400,    // Allow empty order list like in OpenMPT (FT2 just plays pattern 0 if the order list is empty according to the header)
};
DECLARE_FLAGSET(TrackerVersions)
static bool ValidateHeader(const XMFileHeader &fileHeader) {
if(fileHeader.channels == 0
   || fileHeader.channels > MAX_BASECHANNELS
   || std::memcmp(fileHeader.signature, "Extended Module: ", 17)
        ) {
return false;
}
return true;
}
static uint64 GetHeaderMinimumAdditionalSize(const XMFileHeader &fileHeader) {
return fileHeader.orders + 4 * (fileHeader.patterns + fileHeader.instruments);
}
CSoundFile::ProbeResult CSoundFile::ProbeFileHeaderXM(MemoryFileReader file, const uint64 *pfilesize) {
XMFileHeader fileHeader;
if(!file.ReadStruct(fileHeader)) {
return ProbeWantMoreData;
}
if(!ValidateHeader(fileHeader)) {
return ProbeFailure;
}
return ProbeAdditionalSize(file, pfilesize, GetHeaderMinimumAdditionalSize(fileHeader));
}
static bool ReadSampleData(ModSample &sample, SampleIO sampleFlags, FileReader &sampleChunk, bool &isOXM) {
bool unsupportedSample = false;
bool isOGG = false;
if(sampleChunk.CanRead(8)) {
isOGG = true;
sampleChunk.Skip(4);
// we parse and verify the complete sample data and only assume OggVorbis,
// The fast-path for regular PCM will only check "OggS" magic and do no other work after failing that check.
while (!sampleChunk.EndOfFile()) {
if(!Ogg::ReadPage(sampleChunk)) {
isOGG = false;
break;
}
}
}
isOXM = isOXM || isOGG;
sampleChunk.Rewind();
if(isOGG) {
uint32 originalSize = sampleChunk.ReadInt32LE();
FileReader sampleData = sampleChunk.ReadChunk(sampleChunk.BytesLeft());
sample.uFlags.set(CHN_16BIT, sampleFlags.GetBitDepth() >= 16);
sample.uFlags.set(CHN_STEREO, sampleFlags.GetChannelFormat() != SampleIO::mono);
sample.nLength = originalSize / (sample.uFlags[CHN_16BIT] ? 2 : 1) / (sample.uFlags[CHN_STEREO] ? 2 : 1);
#if defined(MPT_WITH_VORBIS) && defined(MPT_WITH_VORBISFILE)

ov_callbacks callbacks = {
            &VorbisfileFilereaderRead,
            &VorbisfileFilereaderSeek,
            NULL,
            &VorbisfileFilereaderTell
        };
        OggVorbis_File vf;
        MemsetZero(vf);
        if(ov_open_callbacks(&sampleData, &vf, nullptr, 0, callbacks) == 0)
        {
            if(ov_streams(&vf) == 1)
            { // we do not support chained vorbis samples
                vorbis_info *vi = ov_info(&vf, -1);
                if(vi && vi->rate > 0 && vi->channels > 0)
                {
                    sample.AllocateSample();
                    SmpLength offset = 0;
                    int channels = vi->channels;
                    int current_section = 0;
                    long decodedSamples = 0;
                    bool eof = false;
                    while(!eof && offset < sample.nLength && sample.HasSampleData())
                    {
                        float **output = nullptr;
                        long ret = ov_read_float(&vf, &output, 1024, &current_section);
                        if(ret == 0)
                        {
                            eof = true;
                        } else if(ret < 0)
                        {
                        } else
                        {
                            decodedSamples = ret;
                            LimitMax(decodedSamples, mpt::saturate_cast<long>(sample.nLength - offset));
                            if(decodedSamples > 0 && channels == sample.GetNumChannels())
                            {
                                if(sample.uFlags[CHN_16BIT])
                                {
                                    CopyAudio(mpt::audio_span_interleaved(sample.sample16() + (offset * sample.GetNumChannels()), sample.GetNumChannels(), decodedSamples), mpt::audio_span_planar(output, channels, decodedSamples));
                                } else
                                {
                                    CopyAudio(mpt::audio_span_interleaved(sample.sample8() + (offset * sample.GetNumChannels()), sample.GetNumChannels(), decodedSamples), mpt::audio_span_planar(output, channels, decodedSamples));
                                }
                            }
                            offset += decodedSamples;
                        }
                    }
                } else
                {
                    unsupportedSample = true;
                }
            } else
            {
                unsupportedSample = true;
            }
            ov_clear(&vf);
        } else
        {
            unsupportedSample = true;
        }

#elif defined(MPT_WITH_STBVORBIS)
// position at stream start. (See
        // This means that, for remuxed and re-aligned/cutted (at stream start)
        // beginning. OXM files with this property are yet to be spotted in the

        int consumed = 0, error = 0;
        stb_vorbis *vorb = nullptr;
        FileReader::PinnedView sampleDataView = sampleData.GetPinnedView();
        const std::byte* data = sampleDataView.data();
        std::size_t dataLeft = sampleDataView.size();
        vorb = stb_vorbis_open_pushdata(mpt::byte_cast<const unsigned char*>(data), mpt::saturate_cast<int>(dataLeft), &consumed, &error, nullptr);
        sampleData.Skip(consumed);
        data += consumed;
        dataLeft -= consumed;
        if(vorb)
        {
            sample.AllocateSample();
            SmpLength offset = 0;
            while((error == VORBIS__no_error || (error == VORBIS_need_more_data && dataLeft > 0))
                && offset < sample.nLength && sample.HasSampleData())
            {
                int channels = 0, decodedSamples = 0;
                float **output;
                consumed = stb_vorbis_decode_frame_pushdata(vorb, mpt::byte_cast<const unsigned char*>(data), mpt::saturate_cast<int>(dataLeft), &channels, &output, &decodedSamples);
                sampleData.Skip(consumed);
                data += consumed;
                dataLeft -= consumed;
                LimitMax(decodedSamples, mpt::saturate_cast<int>(sample.nLength - offset));
                if(decodedSamples > 0 && channels == sample.GetNumChannels())
                {
                    if(sample.uFlags[CHN_16BIT])
                    {
                        CopyAudio(mpt::audio_span_interleaved(sample.sample16() + (offset * sample.GetNumChannels()), sample.GetNumChannels(), decodedSamples), mpt::audio_span_planar(output, channels, decodedSamples));
                    } else
                    {
                        CopyAudio(mpt::audio_span_interleaved(sample.sample8() + (offset * sample.GetNumChannels()), sample.GetNumChannels(), decodedSamples), mpt::audio_span_planar(output, channels, decodedSamples));
                    }
                }
                offset += decodedSamples;
                error = stb_vorbis_get_error(vorb);
            }
            stb_vorbis_close(vorb);
        } else
        {
            unsupportedSample = true;
        }

#else // !VORBIS
unsupportedSample = true;
#endif // VORBIS
} else {
sampleFlags.ReadSample(sample, sampleChunk);
}
return !unsupportedSample;
}
bool CSoundFile::ReadXM(FileReader &file, ModLoadingFlags loadFlags) {
file.Rewind();
XMFileHeader fileHeader;
if(!file.ReadStruct(fileHeader)) {
return false;
}
if(!ValidateHeader(fileHeader)) {
return false;
}
if(!file.CanRead(mpt::saturate_cast<FileReader::off_t>(GetHeaderMinimumAdditionalSize(fileHeader)))) {
return false;
} else if(loadFlags == onlyVerifyHeader) {
return true;
}
InitializeGlobals(MOD_TYPE_XM);
InitializeChannels();
m_nMixLevels = MixLevels::Compatible;
FlagSet <TrackerVersions> madeWith(verUnknown);
mpt::ustring madeWithTracker;
if(!memcmp(fileHeader.trackerName, "FastTracker v2.00   ", 20) && fileHeader.size == 276) {
if(fileHeader.version < 0x0104)
madeWith = verFT2Generic | verConfirmed;
else if(memchr(fileHeader.songName, '\0', 20) != nullptr)
madeWith = verFT2Clone | verNewModPlug | verEmptyOrders;
else
madeWith = verFT2Generic | verNewModPlug;
} else if(!memcmp(fileHeader.trackerName, "FastTracker v 2.00  ", 20)) {
madeWith = verOldModPlug;
} else {
madeWith = verUnknown | verConfirmed;
madeWithTracker = mpt::ToUnicode(mpt::Charset::CP437,
                                 mpt::String::ReadBuf(mpt::String::spacePadded, fileHeader.trackerName));
if(!memcmp(fileHeader.trackerName, "OpenMPT ", 8)) {
madeWith = verOpenMPT | verConfirmed | verEmptyOrders;
} else if(!memcmp(fileHeader.trackerName, "MilkyTracker ", 12)) {
// Luckily, starting with v0.90.87, MilkyTracker also implements the FT2 panning scheme.
if(memcmp(fileHeader.trackerName + 12, "        ", 8)) {
m_nMixLevels = MixLevels::CompatibleFT2;
}
} else if(!memcmp(fileHeader.trackerName, "Fasttracker II clone", 20)) {
madeWith = verFT2Generic | verConfirmed;
} else if(!memcmp(fileHeader.trackerName, "MadTracker 2.0\0", 15)) {
m_playBehaviour.reset(kFT2PortaNoNote);
m_playBehaviour.reset(kFT2Arpeggio);
} else if(!memcmp(fileHeader.trackerName, "Skale Tracker\0", 14)) {
m_playBehaviour.reset(kFT2ST3OffsetOutOfRange);
} else if(!memcmp(fileHeader.trackerName, "*Converted ", 11)) {
madeWith = verDigiTrakker;
}
}
m_songName = mpt::String::ReadBuf(mpt::String::spacePadded, fileHeader.songName);
m_nMinPeriod = 1;
m_nMaxPeriod = 31999;
Order().SetRestartPos(fileHeader.restartPos);
m_nChannels = fileHeader.channels;
m_nInstruments = std::min(static_cast<uint16>(fileHeader.instruments), static_cast<uint16>(MAX_INSTRUMENTS - 1));
if(fileHeader.speed)
m_nDefaultSpeed = fileHeader.speed;
if(fileHeader.tempo)
m_nDefaultTempo = Clamp(TEMPO(fileHeader.tempo, 0), ModSpecs::xmEx.GetTempoMin(), ModSpecs::xmEx.GetTempoMax());
m_SongFlags.reset();
m_SongFlags.set(SONG_LINEARSLIDES, (fileHeader.flags & XMFileHeader::linearSlides) != 0);
m_SongFlags.set(SONG_EXFILTERRANGE, (fileHeader.flags & XMFileHeader::extendedFilterRange) != 0);
if(m_SongFlags[SONG_EXFILTERRANGE] && madeWith == (verFT2Generic | verNewModPlug)) {
madeWith = verFT2Clone | verNewModPlug | verConfirmed;
}
ReadOrderFromFile<uint8>(Order(), file, fileHeader.orders);
if(fileHeader.orders == 0 && !madeWith[verEmptyOrders]) {
Order().assign(1, 0);
}
file.Seek(fileHeader.size + 60);
if(fileHeader.version >= 0x0104) {
ReadXMPatterns(file, fileHeader, *this);
}
bool isOXM = false;
std::vector <SampleIO> sampleFlags;
uint8 sampleReserved = 0;
int instrType = -1;
bool unsupportedSamples = false;
for(INSTRUMENTINDEX instr = 1; instr <= m_nInstruments; instr++) {
uint32 headerSize = file.ReadUint32LE();
if(headerSize == 0) {
headerSize = sizeof(XMInstrumentHeader);
}
file.SkipBack(4);
XMInstrumentHeader instrHeader;
file.ReadStructPartial(instrHeader, headerSize);
if(madeWith == verOldModPlug) {
madeWith.set(verConfirmed);
if(instrHeader.size == 245) {
m_dwLastSavedWithVersion = MPT_V("1.00.00.A5");
madeWithTracker = U_("ModPlug Tracker 1.0 alpha");
} else if(instrHeader.size == 263) {
m_dwLastSavedWithVersion = MPT_V("1.00.00.B3");
madeWithTracker = U_("ModPlug Tracker 1.0 beta");
} else {
madeWith = (verUnknown | verConfirmed);
}
} else if(instrHeader.numSamples == 0) {
if(instrHeader.size == 263 && instrHeader.sampleHeaderSize == 0 && madeWith[verNewModPlug])
madeWith.set(verConfirmed);
else if(instrHeader.size != 29 && madeWith[verDigiTrakker])
madeWith.reset(verDigiTrakker);
else if(madeWith[verFT2Clone | verFT2Generic] && instrHeader.size != 33) {
// Note: FT2 NORMALLY writes shdr=40 for all samples, but sometimes it
// Note: 4-mat's eternity.xm has an instrument header size of 29.
madeWith = verUnknown;
}
}
if(AllocateInstrument(instr) == nullptr) {
continue;
}
instrHeader.ConvertToMPT(*Instruments[instr]);
if(instrType == -1) {
instrType = instrHeader.type;
} else if(instrType != instrHeader.type && madeWith[verFT2Generic]) {
// but it's always the SAME junk for every instrument saved.
madeWith.reset(verFT2Generic);
madeWith.set(verFT2Clone);
}
if(instrHeader.numSamples > 0) {
if((instrHeader.instrument.midiEnabled | instrHeader.instrument.midiChannel | instrHeader.instrument.midiProgram |
    instrHeader.instrument.muteComputer) != 0) {
madeWith.reset(verOldModPlug | verNewModPlug);
}
std::vector <SAMPLEINDEX> sampleSlots = AllocateXMSamples(*this, instrHeader.numSamples);
for(size_t k = 0 + 12; k < 96 + 12; k++) {
if(Instruments[instr]->Keyboard[k] < sampleSlots.size()) {
Instruments[instr]->Keyboard[k] = sampleSlots[Instruments[instr]->Keyboard[k]];
}
}
if(fileHeader.version >= 0x0104) {
sampleFlags.clear();
}
std::vector <uint32> sampleSize(instrHeader.numSamples);
// cybernostra weekend has instrHeader.sampleHeaderSize = 0x12, which would leave out the sample name, but FT2 still reads the name.
MPT_ASSERT(instrHeader.sampleHeaderSize == 0 || instrHeader.sampleHeaderSize == sizeof(XMSample));
for(SAMPLEINDEX sample = 0; sample < instrHeader.numSamples; sample++) {
XMSample sampleHeader;
file.ReadStruct(sampleHeader);
sampleFlags.push_back(sampleHeader.GetSampleFormat());
sampleSize[sample] = sampleHeader.length;
sampleReserved |= sampleHeader.reserved;
if(sample < sampleSlots.size()) {
SAMPLEINDEX mptSample = sampleSlots[sample];
sampleHeader.ConvertToMPT(Samples[mptSample]);
instrHeader.instrument.ApplyAutoVibratoToMPT(Samples[mptSample]);
m_szNames[mptSample] = mpt::String::ReadBuf(mpt::String::spacePadded, sampleHeader.name);
if((sampleHeader.flags & 3) == 3 && madeWith[verNewModPlug]) {
madeWith.set(verModPlug1_09);
}
}
}
if(fileHeader.version >= 0x0104) {
for(SAMPLEINDEX sample = 0; sample < instrHeader.numSamples; sample++) {
// Always read as many bytes as specified in the header, even if the sample reader would probably read less bytes.
FileReader sampleChunk = file.ReadChunk(
        sampleFlags[sample].GetEncoding() != SampleIO::ADPCM ? sampleSize[sample] : (16 +
                                                                                     (sampleSize[sample] + 1) / 2));
if(sample < sampleSlots.size() && (loadFlags & loadSampleData)) {
if(!ReadSampleData(Samples[sampleSlots[sample]], sampleFlags[sample], sampleChunk, isOXM)) {
unsupportedSamples = true;
}
}
}
}
}
}
if(sampleReserved == 0 && madeWith[verNewModPlug] &&
   memchr(fileHeader.songName, '\0', sizeof(fileHeader.songName)) != nullptr) {
madeWith.set(verConfirmed);
}
if(fileHeader.version < 0x0104) {
if(loadFlags & (loadPatternData | loadSampleData)) {
ReadXMPatterns(file, fileHeader, *this);
}
if(loadFlags & loadSampleData) {
for(SAMPLEINDEX sample = 1; sample <= GetNumSamples(); sample++) {
sampleFlags[sample - 1].ReadSample(Samples[sample], file);
}
}
}
if(unsupportedSamples) {
AddToLog(LogWarning, U_("Some compressed samples could not be loaded because they use an unsupported codec."));
}
if(file.ReadMagic("text")) {
m_songMessage.Read(file, file.ReadUint32LE(), SongMessage::leCR);
madeWith.set(verConfirmed);
}
bool hasMidiConfig = false;
if(file.ReadMagic("MIDI")) {
file.ReadStructPartial<MIDIMacroConfigData>(m_MidiCfg, file.ReadUint32LE());
m_MidiCfg.Sanitize();
hasMidiConfig = true;
madeWith.set(verConfirmed);
}
if(file.ReadMagic("PNAM")) {
const PATTERNINDEX namedPats = std::min(static_cast<PATTERNINDEX>(file.ReadUint32LE() / MAX_PATTERNNAME),
                                        Patterns.Size());
for(PATTERNINDEX pat = 0; pat < namedPats; pat++) {
char patName[MAX_PATTERNNAME];
file.ReadString<mpt::String::maybeNullTerminated>(patName, MAX_PATTERNNAME);
Patterns[pat].SetName(patName);
}
madeWith.set(verConfirmed);
}
if(file.ReadMagic("CNAM")) {
const CHANNELINDEX namedChans = std::min(static_cast<CHANNELINDEX>(file.ReadUint32LE() / MAX_CHANNELNAME),
                                         GetNumChannels());
for(CHANNELINDEX chn = 0; chn < namedChans; chn++) {
file.ReadString<mpt::String::maybeNullTerminated>(ChnSettings[chn].szName, MAX_CHANNELNAME);
}
madeWith.set(verConfirmed);
}
if(file.CanRead(8)) {
FileReader::off_t oldPos = file.GetPosition();
LoadMixPlugins(file);
if(file.GetPosition() != oldPos) {
madeWith.set(verConfirmed);
}
}
if(madeWith[verConfirmed]) {
if(madeWith[verModPlug1_09]) {
m_dwLastSavedWithVersion = MPT_V("1.09.00.00");
madeWithTracker = U_("ModPlug Tracker 1.09");
} else if(madeWith[verNewModPlug]) {
m_dwLastSavedWithVersion = MPT_V("1.16.00.00");
madeWithTracker = U_("ModPlug Tracker 1.10 - 1.16");
}
}
if(!memcmp(fileHeader.trackerName, "OpenMPT ", 8)) {
std::string mptVersion(fileHeader.trackerName + 8, 12);
m_dwLastSavedWithVersion = Version::Parse(mpt::ToUnicode(mpt::Charset::ASCII, mptVersion));
madeWith = verOpenMPT | verConfirmed;
if(m_dwLastSavedWithVersion < MPT_V("1.22.07.19"))
m_nMixLevels = MixLevels::Compatible;
else
m_nMixLevels = MixLevels::CompatibleFT2;
}
if(m_dwLastSavedWithVersion && !madeWith[verOpenMPT]) {
m_nMixLevels = MixLevels::Original;
m_playBehaviour.reset();
}
if(madeWith[verFT2Generic]) {
m_nMixLevels = MixLevels::CompatibleFT2;
if(!hasMidiConfig) {
// Prevent these commands from being interpreted as filter commands by erasing the default MIDI Config.
m_MidiCfg.ClearZxxMacros();
}
if(fileHeader.version >=
   0x0104    // Old versions of FT2 didn't have (smooth) ramping. Disable it for those versions where we can be sure that there should be no ramping.
#ifdef MODPLUG_TRACKER
&& TrackerSettings::Instance().autoApplySmoothFT2Ramping
#endif // MODPLUG_TRACKER
        ) {
m_playBehaviour.set(kFT2VolumeRamping);
}
}
if(madeWithTracker.empty()) {
if(madeWith[verDigiTrakker] && sampleReserved == 0 && (instrType ? instrType : -1) == -1) {
madeWithTracker = U_("DigiTrakker");
} else if(madeWith[verFT2Generic]) {
madeWithTracker = U_("FastTracker 2 or compatible");
} else {
madeWithTracker = U_("Unknown");
}
}
bool isOpenMPTMade = false; // specific for OpenMPT 1.17+
if(GetNumInstruments()) {
isOpenMPTMade = LoadExtendedInstrumentProperties(file);
}
LoadExtendedSongProperties(file, true, &isOpenMPTMade);
if(isOpenMPTMade && m_dwLastSavedWithVersion < MPT_V("1.17.00.00")) {
// when saving a file in OpenMPT for the first time.
m_dwLastSavedWithVersion = MPT_V("1.17.00.00");
}
if(m_dwLastSavedWithVersion >= MPT_V("1.17.00.00")) {
madeWithTracker = U_("OpenMPT ") + m_dwLastSavedWithVersion.ToUString();
}
if(m_dwLastSavedWithVersion && m_dwLastSavedWithVersion < MPT_V("1.22.02.02")) {
if(!Patterns.IsValidPat(0xFE))
Order().RemovePattern(0xFE);
if(!Patterns.IsValidPat(0xFF))
Order().Replace(0xFF, Order.GetInvalidPatIndex());
}
if(isOXM) {
m_modFormat.formatName = U_("OggMod FastTracker 2");
m_modFormat.type = U_("oxm");
m_modFormat.originalFormatName = MPT_UFORMAT("FastTracker 2 v{}.{}")(fileHeader.version >> 8,
                                                                     mpt::ufmt::hex0<2>(fileHeader.version & 0xFF));
m_modFormat.originalType = U_("xm");
m_modFormat.madeWithTracker = std::move(madeWithTracker);
m_modFormat.charset = m_dwLastSavedWithVersion ? mpt::Charset::Windows1252 : mpt::Charset::CP437;
} else {
m_modFormat.formatName = MPT_UFORMAT("FastTracker 2 v{}.{}")(fileHeader.version >> 8,
                                                             mpt::ufmt::hex0<2>(fileHeader.version & 0xFF));
m_modFormat.type = U_("xm");
m_modFormat.madeWithTracker = std::move(madeWithTracker);
m_modFormat.charset = m_dwLastSavedWithVersion ? mpt::Charset::Windows1252 : mpt::Charset::CP437;
}
return true;
}
#ifndef MODPLUG_NO_FILESAVE
bool CSoundFile::SaveXM(std::ostream &f, bool compatibilityExport) {
bool addChannel = false; // avoid odd channel count for FT2 compatibility

XMFileHeader fileHeader;
MemsetZero(fileHeader);
memcpy(fileHeader.signature, "Extended Module: ", 17);
mpt::String::WriteBuf(mpt::String::spacePadded, fileHeader.songName) = m_songName;
fileHeader.eof = 0x1A;
const std::string openMptTrackerName = mpt::ToCharset(GetCharsetFile(), Version::Current().GetOpenMPTVersionString());
mpt::String::WriteBuf(mpt::String::spacePadded, fileHeader.trackerName) = openMptTrackerName;
fileHeader.version = 0x0104;                    // XM Format v1.04
fileHeader.size = sizeof(XMFileHeader) - 60;    // minus everything before this field
fileHeader.restartPos = Order().GetRestartPos();
fileHeader.channels = m_nChannels;
if((m_nChannels % 2u) && m_nChannels < 32) {
fileHeader.channels++;
addChannel = true;
} else if(compatibilityExport && fileHeader.channels > 32) {
fileHeader.channels = 32;
}
// +++ and --- patterns are not taken into consideration as FastTracker does not support them.

const ORDERINDEX trimmedLength = Order().GetLengthTailTrimmed();
std::vector <uint8> orderList(trimmedLength);
const ORDERINDEX orderLimit = compatibilityExport ? 256 : uint16_max;
ORDERINDEX numOrders = 0;
PATTERNINDEX numPatterns = Patterns.GetNumPatterns();
bool changeOrderList = false;
for(ORDERINDEX ord = 0; ord < trimmedLength; ord++) {
PATTERNINDEX pat = Order()[ord];
if(pat == Order.GetIgnoreIndex() || pat == Order.GetInvalidPatIndex() || pat > uint8_max) {
changeOrderList = true;
} else if(numOrders < orderLimit) {
orderList[numOrders++] = static_cast<uint8>(pat);
if(pat >= numPatterns)
numPatterns = pat + 1;
}
}
if(changeOrderList) {
AddToLog(LogWarning, U_("Skip and stop order list items (+++ and ---) are not saved in XM files."));
}
orderList.resize(compatibilityExport ? 256 : numOrders);
fileHeader.orders = numOrders;
fileHeader.patterns = numPatterns;
fileHeader.size += static_cast<uint32>(orderList.size());
uint16 writeInstruments;
if(m_nInstruments > 0)
fileHeader.instruments = writeInstruments = m_nInstruments;
else
fileHeader.instruments = writeInstruments = m_nSamples;
if(m_SongFlags[SONG_LINEARSLIDES]) fileHeader.flags |= XMFileHeader::linearSlides;
if(m_SongFlags[SONG_EXFILTERRANGE] && !compatibilityExport) fileHeader.flags |= XMFileHeader::extendedFilterRange;
fileHeader.flags = fileHeader.flags;
fileHeader.tempo = mpt::saturate_cast<uint16>(m_nDefaultTempo.GetInt());
fileHeader.speed = static_cast<uint16>(Clamp(m_nDefaultSpeed, 1u, 31u));
mpt::IO::Write(f, fileHeader);
mpt::IO::Write(f, orderList);
#define ASSERT_CAN_WRITE(x) \
    if(len > s.size() - x) /*Buffer running out? Make it larger.*/ \
        s.resize(s.size() + 10 * 1024, 0);
std::vector <uint8> s(64 * 64 * 5, 0);
for(PATTERNINDEX pat = 0; pat < numPatterns; pat++) {
uint8 patHead[9] = {0};
patHead[0] = 9;
if(!Patterns.IsValidPat(pat)) {
patHead[5] = 64;
mpt::IO::Write(f, patHead);
continue;
}
const uint16 numRows = mpt::saturate_cast<uint16>(Patterns[pat].GetNumRows());
patHead[5] = static_cast<uint8>(numRows & 0xFF);
patHead[6] = static_cast<uint8>(numRows >> 8);
auto p = Patterns[pat].cbegin();
size_t len = 0;
bool emptyPattern = true;
for(size_t j = m_nChannels * numRows; j > 0; j--, p++) {
if(compatibilityExport && m_nChannels - ((j - 1) % m_nChannels) > 32) continue;
uint8 note = p->note;
uint8 command = p->command, param = p->param;
ModSaveCommand(command, param, true, compatibilityExport);
if(note >= NOTE_MIN_SPECIAL) note = 97;
else if((note <= 12) || (note > 96 + 12)) note = 0;
else
note -= 12;
uint8 vol = 0;
if(p->volcmd != VOLCMD_NONE) {
switch (p->volcmd) {
case VOLCMD_VOLUME:
vol = 0x10 + p->vol;
break;
case VOLCMD_VOLSLIDEDOWN:
vol = 0x60 + (p->vol & 0x0F);
break;
case VOLCMD_VOLSLIDEUP:
vol = 0x70 + (p->vol & 0x0F);
break;
case VOLCMD_FINEVOLDOWN:
vol = 0x80 + (p->vol & 0x0F);
break;
case VOLCMD_FINEVOLUP:
vol = 0x90 + (p->vol & 0x0F);
break;
case VOLCMD_VIBRATOSPEED:
vol = 0xA0 + (p->vol & 0x0F);
break;
case VOLCMD_VIBRATODEPTH:
vol = 0xB0 + (p->vol & 0x0F);
break;
case VOLCMD_PANNING:
vol = 0xC0 + (p->vol / 4);
if(vol > 0xCF) vol = 0xCF;
break;
case VOLCMD_PANSLIDELEFT:
vol = 0xD0 + (p->vol & 0x0F);
break;
case VOLCMD_PANSLIDERIGHT:
vol = 0xE0 + (p->vol & 0x0F);
break;
case VOLCMD_TONEPORTAMENTO:
vol = 0xF0 + (p->vol & 0x0F);
break;
}
if(compatibilityExport && p->vol == 0) {
switch (p->volcmd) {
case VOLCMD_VOLUME:
case VOLCMD_PANNING:
case VOLCMD_VIBRATODEPTH:
case VOLCMD_TONEPORTAMENTO:
case VOLCMD_PANSLIDELEFT:    // Doesn't have memory, but does weird things with zero param.
break;
default:
vol = 0;
}
}
}
if(!p->IsEmpty())
emptyPattern = false;
// We have to avoid this, so we add a "break to row 0" command in the last row.
if(j == 1 && emptyPattern && numRows != 64) {
command = 0x0D;
param = 0;
}
if((note) && (p->instr) && (vol > 0x0F) && (command) && (param)) {
s[len++] = note;
s[len++] = p->instr;
s[len++] = vol;
s[len++] = command;
s[len++] = param;
} else {
uint8 b = 0x80;
if(note) b |= 0x01;
if(p->instr) b |= 0x02;
if(vol >= 0x10) b |= 0x04;
if(command) b |= 0x08;
if(param) b |= 0x10;
s[len++] = b;
if(b & 1) s[len++] = note;
if(b & 2) s[len++] = p->instr;
if(b & 4) s[len++] = vol;
if(b & 8) s[len++] = command;
if(b & 16) s[len++] = param;
}
if(addChannel && (j % m_nChannels == 1 || m_nChannels == 1)) {
ASSERT_CAN_WRITE(1);
s[len++] = 0x80;
}
ASSERT_CAN_WRITE(5);
}
if(emptyPattern && numRows == 64) {
len = 0;
}
if(len > uint16_max) {
AddToLog(LogWarning, MPT_UFORMAT(
        "Warning: File format limit was reached. Some pattern data may not get written to file. (pattern {})")(pat));
len = uint16_max;
}
patHead[7] = static_cast<uint8>(len & 0xFF);
patHead[8] = static_cast<uint8>(len >> 8);
mpt::IO::Write(f, patHead);
if(len) mpt::IO::WriteRaw(f, s.data(), len);
}
#undef ASSERT_CAN_WRITE
std::vector<bool> sampleAssigned(GetNumSamples() + 1, false);
for(INSTRUMENTINDEX ins = 1; ins <= GetNumInstruments(); ins++) {
if(Instruments[ins] != nullptr) {
Instruments[ins]->GetSamples(sampleAssigned);
}
}
for(INSTRUMENTINDEX ins = 1; ins <= writeInstruments; ins++) {
XMInstrumentHeader insHeader;
std::vector <SAMPLEINDEX> samples;
if(GetNumInstruments()) {
if(Instruments[ins] != nullptr) {
insHeader.ConvertToXM(*Instruments[ins], compatibilityExport);
samples = insHeader.instrument.GetSampleList(*Instruments[ins], compatibilityExport);
if(samples.size() > 0 && samples[0] <= GetNumSamples()) {
insHeader.instrument.ApplyAutoVibratoToXM(Samples[samples[0]], GetType());
}
std::vector <SAMPLEINDEX> additionalSamples;
// We look for unassigned samples directly after the samples assigned to our current instrument, so if
// we will assign those to sample 1. Any samples before the first referenced sample are going to be lost,
for(auto smp : samples) {
while (++smp <= GetNumSamples()
       && !sampleAssigned[smp]
       && insHeader.numSamples < (compatibilityExport ? 16 : 32)) {
sampleAssigned[smp] = true;            // Don't want to add this sample again.
additionalSamples.push_back(smp);
insHeader.numSamples++;
}
}
samples.insert(samples.end(), additionalSamples.begin(), additionalSamples.end());
} else {
MemsetZero(insHeader);
}
} else {
MemsetZero(insHeader);
insHeader.numSamples = 1;
insHeader.instrument.ApplyAutoVibratoToXM(Samples[ins], GetType());
samples.push_back(ins);
}
insHeader.Finalise();
size_t insHeaderSize = insHeader.size;
mpt::IO::WritePartial(f, insHeader, insHeaderSize);
std::vector <SampleIO> sampleFlags(samples.size());
for(SAMPLEINDEX smp = 0; smp < samples.size(); smp++) {
XMSample xmSample;
if(samples[smp] <= GetNumSamples()) {
xmSample.ConvertToXM(Samples[samples[smp]], GetType(), compatibilityExport);
} else {
MemsetZero(xmSample);
}
sampleFlags[smp] = xmSample.GetSampleFormat();
mpt::String::WriteBuf(mpt::String::spacePadded, xmSample.name) = m_szNames[samples[smp]];
mpt::IO::Write(f, xmSample);
}
for(SAMPLEINDEX smp = 0; smp < samples.size(); smp++) {
if(samples[smp] <= GetNumSamples()) {
sampleFlags[smp].WriteSample(f, Samples[samples[smp]]);
}
}
}
if(!compatibilityExport) {
if(!m_songMessage.empty()) {
uint32 size = mpt::saturate_cast<uint32>(m_songMessage.length());
mpt::IO::WriteRaw(f, "text", 4);
mpt::IO::WriteIntLE<uint32>(f, size);
mpt::IO::WriteRaw(f, m_songMessage.c_str(), size);
}
if(!m_MidiCfg.IsMacroDefaultSetupUsed()) {
mpt::IO::WriteRaw(f, "MIDI", 4);
mpt::IO::WriteIntLE<uint32>(f, sizeof(MIDIMacroConfigData));
mpt::IO::Write(f, static_cast<MIDIMacroConfigData &>(m_MidiCfg));
}
const PATTERNINDEX numNamedPats = Patterns.GetNumNamedPatterns();
if(numNamedPats > 0) {
mpt::IO::WriteRaw(f, "PNAM", 4);
mpt::IO::WriteIntLE<uint32>(f, numNamedPats * MAX_PATTERNNAME);
for(PATTERNINDEX pat = 0; pat < numNamedPats; pat++) {
char name[MAX_PATTERNNAME];
mpt::String::WriteBuf(mpt::String::maybeNullTerminated, name) = Patterns[pat].GetName();
mpt::IO::Write(f, name);
}
}
{
CHANNELINDEX numNamedChannels = 0;
for(CHANNELINDEX chn = 0; chn < m_nChannels; chn++) {
if(ChnSettings[chn].szName[0]) numNamedChannels = chn + 1;
}
if(numNamedChannels) {
mpt::IO::WriteRaw(f, "CNAM", 4);
mpt::IO::WriteIntLE<uint32>(f, numNamedChannels * MAX_CHANNELNAME);
for(CHANNELINDEX chn = 0; chn < numNamedChannels; chn++) {
char name[MAX_CHANNELNAME];
mpt::String::WriteBuf(mpt::String::maybeNullTerminated, name) = ChnSettings[chn].szName;
mpt::IO::Write(f, name);
}
}
}
SaveMixPlugins(&f);
if(GetNumInstruments()) {
SaveExtendedInstrumentProperties(writeInstruments, f);
}
SaveExtendedSongProperties(f);
}
return true;
}
#endif // MODPLUG_NO_FILESAVE
OPENMPT_NAMESPACE_END
