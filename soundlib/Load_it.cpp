
#include "stdafx.h"
#include "Loaders.h"
#include "tuningcollection.h"
#include "mod_specifications.h"
#ifdef MODPLUG_TRACKER
#include "../mptrack/Moddoc.h"
#include "../mptrack/TrackerSettings.h"
#endif // MODPLUG_TRACKER
#ifdef MPT_EXTERNAL_SAMPLES
#include "../common/mptPathString.h"
#endif // MPT_EXTERNAL_SAMPLES
#include "../common/serialization_utils.h"
#ifndef MODPLUG_NO_FILESAVE
#include "../common/mptFileIO.h"
#endif // MODPLUG_NO_FILESAVE
#include "plugins/PlugInterface.h"
#include <sstream>
#include "../common/version.h"
#include "ITTools.h"
#include "mpt/io/base.hpp"
#include "mpt/io/io.hpp"
#include "mpt/io/io_stdstream.hpp"
OPENMPT_NAMESPACE_BEGIN
const uint16
verMptFileVer = 0x891;
const uint16 verMptFileVerLoadLimit = 0x1000; // If cwtv-field is greater or equal to this value,
#ifndef MODPLUG_NO_FILESAVE
static bool AreNonDefaultTuningsUsed(const CSoundFile &sf) {
const INSTRUMENTINDEX numIns = sf.GetNumInstruments();
for(INSTRUMENTINDEX i = 1; i <= numIns; i++) {
if(sf.Instruments[i] != nullptr && sf.Instruments[i]->pTuning != nullptr)
return true;
}
return false;
}
static void WriteTuningCollection(std::ostream &oStrm, const CTuningCollection &tc) {
tc.Serialize(oStrm, U_("Tune specific tunings"));
}
static void WriteTuningMap(std::ostream &oStrm, const CSoundFile &sf) {
if(sf.GetNumInstruments() > 0) {
//For example if there are 6 instruments and

std::map < CTuning * , uint16 > tNameToShort_Map;
unsigned short figMap = 0;
for(INSTRUMENTINDEX i = 1; i <= sf.GetNumInstruments(); i++) {
CTuning *pTuning = nullptr;
if(sf.Instruments[i] != nullptr) {
pTuning = sf.Instruments[i]->pTuning;
}
auto iter = tNameToShort_Map.find(pTuning);
if(iter != tNameToShort_Map.end())
continue; //Tuning already mapped.

tNameToShort_Map[pTuning] = figMap;
figMap++;
}
const uint16 tuningMapSize = static_cast<uint16>(tNameToShort_Map.size());
mpt::IO::WriteIntLE<uint16>(oStrm, tuningMapSize);
for(auto &iter : tNameToShort_Map) {
if(iter.first)
mpt::IO::WriteSizedStringLE<uint8>(oStrm, mpt::ToCharset(mpt::Charset::UTF8, iter.first->GetName()));
else //Case: Using original IT tuning.
mpt::IO::WriteSizedStringLE<uint8>(oStrm, "->MPT_ORIGINAL_IT<-");
mpt::IO::WriteIntLE<uint16>(oStrm, iter.second);
}
for(INSTRUMENTINDEX i = 1; i <= sf.GetNumInstruments(); i++) {
CTuning *pTuning = nullptr;
if(sf.Instruments[i] != nullptr) {
pTuning = sf.Instruments[i]->pTuning;
}
auto iter = tNameToShort_Map.find(pTuning);
if(iter == tNameToShort_Map.end()) //Should never happen
{
sf.AddToLog(LogError, U_("Error: 210807_1"));
return;
}
mpt::IO::WriteIntLE<uint16>(oStrm, iter->second);
}
}
}
#endif // MODPLUG_NO_FILESAVE
static void
ReadTuningCollection(std::istream &iStrm, CTuningCollection &tc, const std::size_t dummy, mpt::Charset defaultCharset) {
MPT_UNREFERENCED_PARAMETER(dummy);
mpt::ustring name;
tc.Deserialize(iStrm, name, defaultCharset);
}
template<class TUNNUMTYPE, class STRSIZETYPE>
static bool
ReadTuningMapTemplate(std::istream &iStrm, std::map <uint16, mpt::ustring> &shortToTNameMap, mpt::Charset charset,
                      const size_t maxNum = 500) {
TUNNUMTYPE numTuning = 0;
mpt::IO::ReadIntLE<TUNNUMTYPE>(iStrm, numTuning);
if(numTuning > maxNum)
return true;
for(size_t i = 0; i < numTuning; i++) {
std::string temp;
uint16 ui = 0;
if(!mpt::IO::ReadSizedStringLE<STRSIZETYPE>(iStrm, temp, 255))
return true;
mpt::IO::ReadIntLE<uint16>(iStrm, ui);
shortToTNameMap[ui] = mpt::ToUnicode(charset, temp);
}
if(iStrm.good())
return false;
else
return true;
}
static void
ReadTuningMapImpl(std::istream &iStrm, CSoundFile &csf, mpt::Charset charset, const size_t = 0, bool old = false) {
std::map <uint16, mpt::ustring> shortToTNameMap;
if(old) {
ReadTuningMapTemplate<uint32, uint32>(iStrm, shortToTNameMap, charset);
} else {
ReadTuningMapTemplate<uint16, uint8>(iStrm, shortToTNameMap, charset);
}
std::vector <mpt::ustring> notFoundTunings;
for(INSTRUMENTINDEX i = 1; i <= csf.GetNumInstruments(); i++) {
uint16 ui = 0;
mpt::IO::ReadIntLE<uint16>(iStrm, ui);
auto iter = shortToTNameMap.find(ui);
if(csf.Instruments[i] && iter != shortToTNameMap.end()) {
const mpt::ustring str = iter->second;
if(str == U_("->MPT_ORIGINAL_IT<-")) {
csf.Instruments[i]->pTuning = nullptr;
continue;
}
csf.Instruments[i]->pTuning = csf.GetTuneSpecificTunings().GetTuning(str);
if(csf.Instruments[i]->pTuning)
continue;
#ifdef MODPLUG_TRACKER
CTuning *localTuning = TrackerSettings::Instance().oldLocalTunings->GetTuning(str);
            if(localTuning)
            {
                std::unique_ptr<CTuning> pNewTuning = std::unique_ptr<CTuning>(new CTuning(*localTuning));
                CTuning *pT = csf.GetTuneSpecificTunings().AddTuning(std::move(pNewTuning));
                if(pT)
                {
                    csf.AddToLog(LogInformation, U_("Local tunings are deprecated and no longer supported. Tuning '") + str + U_("' found in Local tunings has been copied to Tune-specific tunings and will be saved in the module file."));
                    csf.Instruments[i]->pTuning = pT;
                    if(csf.GetpModDoc() != nullptr)
                    {
                        csf.GetpModDoc()->SetModified();
                    }
                    continue;
                } else
                {
                    csf.AddToLog(LogError, U_("Copying Local tuning '") + str + U_("' to Tune-specific tunings failed."));
                }
            }
#endif
if(str == U_("12TET [[fs15 1.17.02.49]]") || str == U_("12TET")) {
std::unique_ptr <CTuning> pNewTuning = csf.CreateTuning12TET(str);
CTuning *pT = csf.GetTuneSpecificTunings().AddTuning(std::move(pNewTuning));
if(pT) {
#ifdef MODPLUG_TRACKER
csf.AddToLog(LogInformation, U_("Built-in tunings will no longer be used. Tuning '") + str + U_("' has been copied to Tune-specific tunings and will be saved in the module file."));
                        csf.Instruments[i]->pTuning = pT;
                        if(csf.GetpModDoc() != nullptr)
                        {
                            csf.GetpModDoc()->SetModified();
                        }
#endif
continue;
} else {
#ifdef MODPLUG_TRACKER
csf.AddToLog(LogError, U_("Copying Built-in tuning '") + str + U_("' to Tune-specific tunings failed."));
#endif
}
}
if(!mpt::contains(notFoundTunings, str)) {
notFoundTunings.push_back(str);
csf.AddToLog(LogWarning, U_("Tuning '") + str + U_("' used by the module was not found."));
#ifdef MODPLUG_TRACKER
if(csf.GetpModDoc() != nullptr)
                {
                    csf.GetpModDoc()->SetModified(); // The tuning is changed so the modified flag is set.
                }
#endif // MODPLUG_TRACKER
}
csf.Instruments[i]->pTuning = csf.GetDefaultTuning();
} else {
if(csf.Instruments[i])
csf.Instruments[i]->pTuning = csf.GetDefaultTuning();
}
}
}
static void ReadTuningMap(std::istream &iStrm, CSoundFile &csf, const size_t dummy, mpt::Charset charset) {
ReadTuningMapImpl(iStrm, csf, charset, dummy, false);
}
size_t CSoundFile::ITInstrToMPT(FileReader &file, ModInstrument &ins, uint16 trkvers) {
if(trkvers < 0x0200) {
ITOldInstrument instrumentHeader;
if(!file.ReadStruct(instrumentHeader)) {
return 0;
} else {
instrumentHeader.ConvertToMPT(ins);
return sizeof(ITOldInstrument);
}
} else {
const FileReader::off_t offset = file.GetPosition();
ITInstrumentEx instrumentHeader;
file.ReadStructPartial(instrumentHeader);
size_t instSize = instrumentHeader.ConvertToMPT(ins, GetType());
file.Seek(offset + instSize);
// This chunk was also written in later versions (probably to maintain compatibility with
if(file.ReadMagic("MSNI")) {
FileReader modularData = file.ReadChunk(file.ReadUint32LE());
instSize += 8 + modularData.GetLength();
if(modularData.ReadMagic("GULP")) {
ins.nMixPlug = modularData.ReadUint8();
if(ins.nMixPlug > MAX_MIXPLUGINS) ins.nMixPlug = 0;
}
}
return instSize;
}
}
static void CopyPatternName(CPattern &pattern, FileReader &file) {
char name[MAX_PATTERNNAME] = "";
file.ReadString<mpt::String::maybeNullTerminated>(name, MAX_PATTERNNAME);
pattern.SetName(name);
}
mpt::ustring CSoundFile::GetSchismTrackerVersion(uint16
cwtv,
uint32 reserved
)
{
// = 0x050: anywhere from 2007-04-17 to 2009-10-31

cwtv &= 0xFFF;
if(cwtv > 0x050)
{
int32 date = SchismTrackerEpoch + (cwtv < 0xFFF ? cwtv - 0x050 : reserved);
int32 y = static_cast<int32>((Util::mul32to64(10000, date) + 14780) / 3652425);
int32 ddd = date - (365 * y + y / 4 - y / 100 + y / 400);
if(ddd < 0)
{
y--;
ddd = date - (365 * y + y / 4 - y / 100 + y / 400);
}
int32 mi = (100 * ddd + 52) / 3060;
return
MPT_UFORMAT("Schism Tracker {}-{}-{}")(
mpt::ufmt::dec0<4>(y
+ (mi + 2) / 12),
mpt::ufmt::dec0<2>((mi
+ 2) % 12 + 1),
mpt::ufmt::dec0<2>(ddd
- (mi * 306 + 5) / 10 + 1));
} else
{
return
MPT_UFORMAT("Schism Tracker 0.{}")(
mpt::ufmt::hex0<2>(cwtv)
);
}
}
static bool ValidateHeader(const ITFileHeader &fileHeader) {
if((std::memcmp(fileHeader.id, "IMPM", 4) && std::memcmp(fileHeader.id, "tpm.", 4))
   || fileHeader.insnum > 0xFF
   || fileHeader.smpnum >= MAX_SAMPLES
        ) {
return false;
}
return true;
}
static uint64 GetHeaderMinimumAdditionalSize(const ITFileHeader &fileHeader) {
return fileHeader.ordnum + (fileHeader.insnum + fileHeader.smpnum + fileHeader.patnum) * 4;
}
CSoundFile::ProbeResult CSoundFile::ProbeFileHeaderIT(MemoryFileReader file, const uint64 *pfilesize) {
ITFileHeader fileHeader;
if(!file.ReadStruct(fileHeader)) {
return ProbeWantMoreData;
}
if(!ValidateHeader(fileHeader)) {
return ProbeFailure;
}
return ProbeAdditionalSize(file, pfilesize, GetHeaderMinimumAdditionalSize(fileHeader));
}
bool CSoundFile::ReadIT(FileReader &file, ModLoadingFlags loadFlags) {
file.Rewind();
ITFileHeader fileHeader;
if(!file.ReadStruct(fileHeader)) {
return false;
}
if(!ValidateHeader(fileHeader)) {
return false;
}
if(!file.CanRead(mpt::saturate_cast<FileReader::off_t>(GetHeaderMinimumAdditionalSize(fileHeader)))) {
return false;
}
if(loadFlags == onlyVerifyHeader) {
return true;
}
InitializeGlobals(MOD_TYPE_IT);
bool interpretModPlugMade = false;
mpt::ustring madeWithTracker;
size_t mptStartPos = 0;
if(!memcmp(fileHeader.id, "tpm.", 4)) {
SetType(MOD_TYPE_MPT);
file.Seek(file.GetLength() - 4);
mptStartPos = file.ReadUint32LE();
} else {
if(fileHeader.cwtv > 0x888 && fileHeader.cwtv <= 0xFFF) {
file.Seek(file.GetLength() - 4);
mptStartPos = file.ReadUint32LE();
if(mptStartPos >= 0x100 && mptStartPos < file.GetLength()) {
if(file.Seek(mptStartPos) && file.ReadMagic("228")) {
SetType(MOD_TYPE_MPT);
if(fileHeader.cwtv >= verMptFileVerLoadLimit) {
AddToLog(LogError,
         U_("The file informed that it is incompatible with this version of OpenMPT. Loading was terminated."));
return false;
} else if(fileHeader.cwtv > verMptFileVer) {
AddToLog(LogInformation,
         U_("The loaded file was made with a more recent OpenMPT version and this version may not be able to load all the features or play the file correctly."));
}
}
}
}
if(GetType() == MOD_TYPE_IT) {
if((fileHeader.cwtv & 0xF000) == 0x5000) {
uint32 mptVersion = (fileHeader.cwtv & 0x0FFF) << 16;
if(!memcmp(&fileHeader.reserved, "OMPT", 4))
interpretModPlugMade = true;
else if(mptVersion >= 0x01'29'00'00)
mptVersion |= fileHeader.reserved & 0xFFFF;
m_dwLastSavedWithVersion = Version(mptVersion);
} else if(fileHeader.cmwt == 0x888 || fileHeader.cwtv == 0x888) {
interpretModPlugMade = true;
m_dwLastSavedWithVersion = MPT_V("1.17.00.00");
} else if(fileHeader.cwtv == 0x0217 && fileHeader.cmwt == 0x0200 && fileHeader.reserved == 0) {
if(memchr(fileHeader.chnpan, 0xFF, sizeof(fileHeader.chnpan)) != nullptr) {
m_dwLastSavedWithVersion = MPT_V("1.16.00.00");
madeWithTracker = U_("ModPlug Tracker 1.09 - 1.16");
} else {
m_dwLastSavedWithVersion = MPT_V("1.17.00.00");
madeWithTracker = U_("OpenMPT 1.17 (compatibility export)");
}
interpretModPlugMade = true;
} else if(fileHeader.cwtv == 0x0214 && fileHeader.cmwt == 0x0202 && fileHeader.reserved == 0) {
m_dwLastSavedWithVersion = MPT_V("1.09.00.00");
madeWithTracker = U_("ModPlug Tracker b3.3 - 1.09");
interpretModPlugMade = true;
} else if(fileHeader.cwtv == 0x0300 && fileHeader.cmwt == 0x0300 && fileHeader.reserved == 0 &&
          fileHeader.ordnum == 256 && fileHeader.sep == 128 && fileHeader.pwd == 0) {
m_dwLastSavedWithVersion = MPT_V("1.17.02.20");
interpretModPlugMade = true;
}
}
}
m_SongFlags.set(SONG_LINEARSLIDES, (fileHeader.flags & ITFileHeader::linearSlides) != 0);
m_SongFlags.set(SONG_ITOLDEFFECTS, (fileHeader.flags & ITFileHeader::itOldEffects) != 0);
m_SongFlags.set(SONG_ITCOMPATGXX, (fileHeader.flags & ITFileHeader::itCompatGxx) != 0);
m_SongFlags.set(SONG_EXFILTERRANGE, (fileHeader.flags & ITFileHeader::extendedFilterRange) != 0);
m_songName = mpt::String::ReadBuf(mpt::String::spacePadded, fileHeader.songname);
if((fileHeader.special & ITFileHeader::embedPatternHighlights)) {
// Note: OpenMPT 1.17.03.02 was the first version to properly make use of the time signature in the IT header.
//   Luckily OpenMPT 1.17.03.02 should not be very wide-spread.
if(!m_dwLastSavedWithVersion || m_dwLastSavedWithVersion >= MPT_V("1.17.03.02")) {
m_nDefaultRowsPerBeat = fileHeader.highlight_minor;
m_nDefaultRowsPerMeasure = fileHeader.highlight_major;
}
}
m_nDefaultGlobalVolume = fileHeader.globalvol << 1;
if(m_nDefaultGlobalVolume > MAX_GLOBAL_VOLUME)
m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;
if(fileHeader.speed)
m_nDefaultSpeed = fileHeader.speed;
m_nDefaultTempo.Set(std::max(uint8(31), static_cast<uint8>(fileHeader.tempo)));
m_nSamplePreAmp = std::min(static_cast<uint8>(fileHeader.mv), uint8(128));
for(CHANNELINDEX i = 0; i < 64; i++)
if(fileHeader.chnpan[i] != 0xFF) {
ChnSettings[i].Reset();
ChnSettings[i].nVolume = Clamp<uint8, uint8>(fileHeader.chnvol[i], 0, 64);
if(fileHeader.chnpan[i] & 0x80) ChnSettings[i].dwFlags.set(CHN_MUTE);
uint8 n = fileHeader.chnpan[i] & 0x7F;
if(n <= 64) ChnSettings[i].nPan = n * 4;
if(n == 100) ChnSettings[i].dwFlags.set(CHN_SURROUND);
}
file.Seek(sizeof(ITFileHeader));
if(GetType() == MOD_TYPE_MPT && fileHeader.cwtv > 0x88A && fileHeader.cwtv <= 0x88D) {
uint16 version = file.ReadUint16LE();
if(version != 0)
return false;
uint32 numOrd = file.ReadUint32LE();
if(numOrd > ModSpecs::mptm.ordersMax || !ReadOrderFromFile<uint32le>(Order(), file, numOrd))
return false;
} else {
ReadOrderFromFile<uint8>(Order(), file, fileHeader.ordnum, 0xFF, 0xFE);
}
std::vector <uint32le> insPos, smpPos, patPos;
if(!file.ReadVector(insPos, fileHeader.insnum)
   || !file.ReadVector(smpPos, fileHeader.smpnum)
   || !file.ReadVector(patPos, fileHeader.patnum)) {
return false;
}
// We will consider the history invalid if it ends after the first parapointer.
uint32 minPtr = std::numeric_limits<decltype(minPtr)>::max();
for(uint32 pos : insPos) {
if(pos > 0 && pos < minPtr)
minPtr = pos;
}
for(uint32 pos : smpPos) {
if(pos > 0 && pos < minPtr)
minPtr = pos;
}
for(uint32 pos : patPos) {
if(pos > 0 && pos < minPtr)
minPtr = pos;
}
if(fileHeader.special & ITFileHeader::embedSongMessage) {
minPtr = std::min(minPtr, fileHeader.msgoffset.get());
}
const bool possiblyUNMO3 = fileHeader.cmwt == 0x0214 && (fileHeader.cwtv == 0x0214 || fileHeader.cwtv == 0)
                           && fileHeader.highlight_major == 0 && fileHeader.highlight_minor == 0
                           && fileHeader.pwd == 0 && fileHeader.reserved == 0
                           && (fileHeader.flags &
                               (ITFileHeader::useMIDIPitchController | ITFileHeader::reqEmbeddedMIDIConfig)) == 0;
if(possiblyUNMO3 && fileHeader.insnum == 0 && fileHeader.smpnum > 0 &&
   file.GetPosition() + 4 * smpPos.size() + 2 <= minPtr) {
// as it always sets the instrument mode flag and writes non-zero row highlights.
bool oldUNMO3 = true;
for(uint16 i = 0; i < fileHeader.smpnum; i++) {
if(file.ReadUint32LE() != 0) {
oldUNMO3 = false;
file.SkipBack(4 + i * 4);
break;
}
}
if(oldUNMO3) {
madeWithTracker = U_("UNMO3 <= 2.4");
}
}
if(possiblyUNMO3 && fileHeader.cwtv == 0) {
madeWithTracker = U_("UNMO3 v0/1");
}
// even if they don't write the edit history count. So we have to filter this out...
if(fileHeader.special & ITFileHeader::embedEditHistory) {
const uint16 nflt = file.ReadUint16LE();
if(file.CanRead(nflt * sizeof(ITHistoryStruct)) && file.GetPosition() + nflt * sizeof(ITHistoryStruct) <= minPtr) {
m_FileHistory.resize(nflt);
for(auto &mptHistory : m_FileHistory) {
ITHistoryStruct itHistory;
file.ReadStruct(itHistory);
itHistory.ConvertToMPT(mptHistory);
}
if(possiblyUNMO3 && nflt == 0) {
if(fileHeader.special & ITFileHeader::embedPatternHighlights)
madeWithTracker = U_("UNMO3 <= 2.4.0.1");  // Set together with MIDI macro embed flag
else
madeWithTracker = U_("UNMO3");  // Either 2.4.0.2+ or no MIDI macros embedded
}
} else {
file.SkipBack(2);
}
} else if(possiblyUNMO3 && fileHeader.special <= 1) {
// Otherwise we end up here and might have to read the edit history length.
if(file.ReadUint16LE() == 0) {
madeWithTracker = U_("UNMO3 <= 2.4");
} else {
file.SkipBack(2);
}
}
bool hasMidiConfig = (fileHeader.flags & ITFileHeader::reqEmbeddedMIDIConfig) ||
                     (fileHeader.special & ITFileHeader::embedMIDIConfiguration);
if(hasMidiConfig && file.ReadStruct<MIDIMacroConfigData>(m_MidiCfg)) {
m_MidiCfg.Sanitize();
}
if(fileHeader.cwtv < 0x0214) {
m_MidiCfg.ClearZxxMacros();
}
FileReader patNames;
if(file.ReadMagic("PNAM")) {
patNames = file.ReadChunk(file.ReadUint32LE());
}
m_nChannels = 1;
if(file.ReadMagic("CNAM")) {
FileReader chnNames = file.ReadChunk(file.ReadUint32LE());
const CHANNELINDEX readChns = std::min(MAX_BASECHANNELS,
                                       static_cast<CHANNELINDEX>(chnNames.GetLength() / MAX_CHANNELNAME));
m_nChannels = readChns;
for(CHANNELINDEX i = 0; i < readChns; i++) {
chnNames.ReadString<mpt::String::maybeNullTerminated>(ChnSettings[i].szName, MAX_CHANNELNAME);
}
}
FileReader pluginChunk = file.ReadChunk(
        (minPtr >= file.GetPosition()) ? minPtr - file.GetPosition() : file.BytesLeft());
const bool isBeRoTracker = LoadMixPlugins(pluginChunk);
if((fileHeader.special & ITFileHeader::embedSongMessage) && fileHeader.msglength > 0 &&
   file.Seek(fileHeader.msgoffset)) {
m_songMessage.Read(file, fileHeader.msglength, SongMessage::leAutodetect);
}
m_nInstruments = 0;
if(fileHeader.flags & ITFileHeader::instrumentMode) {
m_nInstruments = std::min(static_cast<INSTRUMENTINDEX>(fileHeader.insnum),
                          static_cast<INSTRUMENTINDEX>(MAX_INSTRUMENTS - 1));
}
for(INSTRUMENTINDEX i = 0; i < GetNumInstruments(); i++) {
if(insPos[i] > 0 && file.Seek(insPos[i]) &&
   file.CanRead(fileHeader.cmwt < 0x200 ? sizeof(ITOldInstrument) : sizeof(ITInstrument))) {
ModInstrument *instrument = AllocateInstrument(i + 1);
if(instrument != nullptr) {
ITInstrToMPT(file, *instrument, fileHeader.cmwt);
instrument->midiPWD = fileHeader.pwd;
}
}
}
FileReader::off_t lastSampleOffset = 0;
if(fileHeader.smpnum > 0) {
lastSampleOffset = smpPos[fileHeader.smpnum - 1] + sizeof(ITSample);
}
bool possibleXMconversion = false;
m_nSamples = std::min(static_cast<SAMPLEINDEX>(fileHeader.smpnum), static_cast<SAMPLEINDEX>(MAX_SAMPLES - 1));
bool lastSampleCompressed = false;
for(SAMPLEINDEX i = 0; i < GetNumSamples(); i++) {
ITSample sampleHeader;
if(smpPos[i] > 0 && file.Seek(smpPos[i]) && file.ReadStruct(sampleHeader)) {
ModSample &sample = Samples[i + 1];
size_t sampleOffset = sampleHeader.ConvertToMPT(sample);
m_szNames[i + 1] = mpt::String::ReadBuf(mpt::String::spacePadded, sampleHeader.name);
if(!file.Seek(sampleOffset))
continue;
lastSampleCompressed = false;
if(sample.uFlags[CHN_ADLIB]) {
OPLPatch patch;
if(file.ReadArray(patch)) {
sample.SetAdlib(true, patch);
}
} else if(!sample.uFlags[SMP_KEEPONDISK]) {
SampleIO sampleIO = sampleHeader.GetSampleFormat(fileHeader.cwtv);
if(loadFlags & loadSampleData) {
sampleIO.ReadSample(sample, file);
} else {
if(sampleIO.IsVariableLengthEncoded())
lastSampleCompressed = true;
else
file.Skip(sampleIO.CalculateEncodedSize(sample.nLength));
}
if(sampleIO.GetEncoding() == SampleIO::unsignedPCM && sample.nLength != 0) {
possibleXMconversion = true;
}
} else {
size_t strLen;
file.ReadVarInt(strLen);
if((loadFlags & loadSampleData) && strLen) {
std::string filenameU8;
file.ReadString<mpt::String::maybeNullTerminated>(filenameU8, strLen);
#if defined(MPT_EXTERNAL_SAMPLES)
SetSamplePath(i + 1, mpt::PathString::FromUTF8(filenameU8));
#elif !defined(LIBOPENMPT_BUILD_TEST)
AddToLog(LogWarning, MPT_UFORMAT("Loading external sample {} ('{}') failed: External samples are not supported.")(i + 1,
                                                                                                                  mpt::ToUnicode(
                                                                                                                          mpt::Charset::UTF8,
                                                                                                                          filenameU8)));
#endif  // MPT_EXTERNAL_SAMPLES
} else {
file.Skip(strLen);
}
}
lastSampleOffset = std::max(lastSampleOffset, file.GetPosition());
}
}
m_nSamples = std::max(SAMPLEINDEX(1), GetNumSamples());
if(possibleXMconversion && fileHeader.cwtv == 0x0204 && fileHeader.cmwt == 0x0200 && fileHeader.special == 0 &&
   fileHeader.reserved == 0
   && (fileHeader.flags & ~ITFileHeader::linearSlides) ==
      (ITFileHeader::useStereoPlayback | ITFileHeader::instrumentMode | ITFileHeader::itOldEffects)
   && fileHeader.globalvol == 128 && fileHeader.mv == 48 && fileHeader.sep == 128 && fileHeader.pwd == 0 &&
   fileHeader.msglength == 0) {
for(uint8 pan : fileHeader.chnpan) {
if(pan != 0x20 && pan != 0xA0)
possibleXMconversion = false;
}
for(uint8 vol : fileHeader.chnvol) {
if(vol != 0x40)
possibleXMconversion = false;
}
for(size_t i = 20; i < std::size(fileHeader.songname); i++) {
if(fileHeader.songname[i] != 0)
possibleXMconversion = false;
}
if(possibleXMconversion)
madeWithTracker = U_("XM Conversion");
}
m_nMinPeriod = 0;
m_nMaxPeriod = int32_max;
PATTERNINDEX numPats = std::min(static_cast<PATTERNINDEX>(patPos.size()), GetModSpecifications().patternsMax);
if(numPats != patPos.size()) {
AddToLog(LogWarning,
         MPT_UFORMAT("The module contains {} patterns but only {} patterns can be loaded in this OpenMPT version.")(
                 patPos.size(), numPats));
}
if(!(loadFlags & loadPatternData)) {
numPats = 0;
}
for(PATTERNINDEX pat = 0; pat < numPats; pat++) {
if(patPos[pat] == 0 || !file.Seek(patPos[pat]))
continue;
uint16 len = file.ReadUint16LE();
ROWINDEX numRows = file.ReadUint16LE();
if(numRows < 1
   || numRows > MAX_PATTERN_ROWS
   || !file.Skip(4))
continue;
FileReader patternData = file.ReadChunk(len);
ROWINDEX row = 0;
std::vector <uint8> chnMask(GetNumChannels());
while (row < numRows && patternData.CanRead(1)) {
uint8 b = patternData.ReadUint8();
if(!b) {
row++;
continue;
}
CHANNELINDEX ch = (b & IT_bitmask_patternChanField_c);   // 0x7f We have some data grab a byte keeping only 7 bits
if(ch) {
ch = (ch - 1);// & IT_bitmask_patternChanMask_c;   // 0x3f mask of the byte again, keeping only 6 bits
}
if(ch >= chnMask.size()) {
chnMask.resize(ch + 1, 0);
}
if(b & IT_bitmask_patternChanEnabled_c)            // 0x80 check if the upper bit is enabled.
{
chnMask[ch] = patternData.ReadUint8();       // set the channel mask for this channel.
}
if(chnMask[ch] & 0x0F)         // if this channel is used set m_nChannels
{
if(ch >= GetNumChannels() && ch < MAX_BASECHANNELS) {
m_nChannels = ch + 1;
}
}
if(chnMask[ch] & 1)
patternData.Skip(1);
if(chnMask[ch] & 2)
patternData.Skip(1);
if(chnMask[ch] & 4)
patternData.Skip(1);
if(chnMask[ch] & 8)
patternData.Skip(2);
}
lastSampleOffset = std::max(lastSampleOffset, file.GetPosition());
}
if(lastSampleOffset > 0) {
file.Seek(lastSampleOffset);
if(lastSampleCompressed) {
while (file.CanRead(4)) {
if(file.ReadMagic("XTPM") || file.ReadMagic("STPM")) {
uint32 id = file.ReadUint32LE();
file.SkipBack(8);
if(!(id & 0x80808080) && (id & 0x60606060)) {
break;
}
}
file.Skip(file.ReadUint16LE());
}
}
}
interpretModPlugMade |= LoadExtendedInstrumentProperties(file);
if(interpretModPlugMade && !isBeRoTracker) {
m_playBehaviour.reset();
m_nMixLevels = MixLevels::Original;
}
LoadExtendedSongProperties(file, false, &interpretModPlugMade);
Patterns.ResizeArray(numPats);
for(PATTERNINDEX pat = 0; pat < numPats; pat++) {
if(patPos[pat] == 0 || !file.Seek(patPos[pat])) {
if(!Patterns.Insert(pat, 64)) {
AddToLog(LogWarning, MPT_UFORMAT("Allocating patterns failed starting from pattern {}")(pat));
break;
}
CopyPatternName(Patterns[pat], patNames);
continue;
}
uint16 len = file.ReadUint16LE();
ROWINDEX numRows = file.ReadUint16LE();
if(!file.Skip(4)
   || !Patterns.Insert(pat, numRows))
continue;
FileReader patternData = file.ReadChunk(len);
CopyPatternName(Patterns[pat], patNames);
std::vector <uint8> chnMask(GetNumChannels());
std::vector <ModCommand> lastValue(GetNumChannels(), ModCommand::Empty());
auto patData = Patterns[pat].begin();
ROWINDEX row = 0;
while (row < numRows && patternData.CanRead(1)) {
uint8 b = patternData.ReadUint8();
if(!b) {
row++;
patData += GetNumChannels();
continue;
}
CHANNELINDEX ch = b & IT_bitmask_patternChanField_c; // 0x7f

if(ch) {
ch = (ch - 1); //& IT_bitmask_patternChanMask_c; // 0x3f
}
if(ch >= chnMask.size()) {
chnMask.resize(ch + 1, 0);
lastValue.resize(ch + 1, ModCommand::Empty());
MPT_ASSERT(chnMask.size() <= GetNumChannels());
}
if(b & IT_bitmask_patternChanEnabled_c)  // 0x80
{
chnMask[ch] = patternData.ReadUint8();
}
ModCommand dummy = ModCommand::Empty();
ModCommand &m = ch < m_nChannels ? patData[ch] : dummy;
if(chnMask[ch] & 0x10) {
m.note = lastValue[ch].note;
}
if(chnMask[ch] & 0x20) {
m.instr = lastValue[ch].instr;
}
if(chnMask[ch] & 0x40) {
m.volcmd = lastValue[ch].volcmd;
m.vol = lastValue[ch].vol;
}
if(chnMask[ch] & 0x80) {
m.command = lastValue[ch].command;
m.param = lastValue[ch].param;
}
if(chnMask[ch] & 1)    // Note
{
uint8 note = patternData.ReadUint8();
if(note < 0x80)
note += NOTE_MIN;
if(!(GetType() & MOD_TYPE_MPT)) {
if(note > NOTE_MAX && note < 0xFD) note = NOTE_FADE;
else if(note == 0xFD) note = NOTE_NONE;
}
m.note = note;
lastValue[ch].note = note;
}
if(chnMask[ch] & 2) {
uint8 instr = patternData.ReadUint8();
m.instr = instr;
lastValue[ch].instr = instr;
}
if(chnMask[ch] & 4) {
uint8 vol = patternData.ReadUint8();
if(vol <= 64) {
m.volcmd = VOLCMD_VOLUME;
m.vol = vol;
} else if(vol >= 128 && vol <= 192) {
m.volcmd = VOLCMD_PANNING;
m.vol = vol - 128;
} else if(vol < 75) {
m.volcmd = VOLCMD_FINEVOLUP;
m.vol = vol - 65;
} else if(vol < 85) {
m.volcmd = VOLCMD_FINEVOLDOWN;
m.vol = vol - 75;
} else if(vol < 95) {
m.volcmd = VOLCMD_VOLSLIDEUP;
m.vol = vol - 85;
} else if(vol < 105) {
m.volcmd = VOLCMD_VOLSLIDEDOWN;
m.vol = vol - 95;
} else if(vol < 115) {
m.volcmd = VOLCMD_PORTADOWN;
m.vol = vol - 105;
} else if(vol < 125) {
m.volcmd = VOLCMD_PORTAUP;
m.vol = vol - 115;
} else if(vol >= 193 && vol <= 202) {
m.volcmd = VOLCMD_TONEPORTAMENTO;
m.vol = vol - 193;
} else if(vol >= 203 && vol <= 212) {
m.volcmd = VOLCMD_VIBRATODEPTH;
m.vol = vol - 203;
if(m.vol && m_dwLastSavedWithVersion && m_dwLastSavedWithVersion <= MPT_V("1.17.02.54"))
m.volcmd = VOLCMD_VIBRATOSPEED;
} else if(vol >= 223 && vol <= 232) {
m.volcmd = VOLCMD_OFFSET;
m.vol = vol - 223;
}
lastValue[ch].volcmd = m.volcmd;
lastValue[ch].vol = m.vol;
}
if(chnMask[ch] & 8) {
const auto[command, param] = patternData.ReadArray<uint8, 2>();
m.command = command;
m.param = param;
S3MConvert(m, true);
lastValue[ch].command = m.command;
lastValue[ch].param = m.param;
}
}
}
if(!m_dwLastSavedWithVersion && fileHeader.cwtv == 0x0888) {
m_dwLastSavedWithVersion = MPT_V("1.17.00.00");
}
if(m_dwLastSavedWithVersion && madeWithTracker.empty()) {
madeWithTracker = U_("OpenMPT ") + mpt::ufmt::val(m_dwLastSavedWithVersion);
if(memcmp(&fileHeader.reserved, "OMPT", 4) && (fileHeader.cwtv & 0xF000) == 0x5000) {
madeWithTracker += U_(" (compatibility export)");
} else if(m_dwLastSavedWithVersion.IsTestVersion()) {
madeWithTracker += U_(" (test build)");
}
} else {
const int32 schismDateVersion =
        SchismTrackerEpoch + ((fileHeader.cwtv == 0x1FFF) ? fileHeader.reserved : (fileHeader.cwtv - 0x1050));
switch (fileHeader.cwtv >> 12) {
case 0:
if(isBeRoTracker) {
madeWithTracker = U_("BeRoTracker");
} else if(fileHeader.cwtv == 0x0214 && fileHeader.cmwt == 0x0200 && fileHeader.flags == 9 && fileHeader.special == 0
          && fileHeader.highlight_major == 0 && fileHeader.highlight_minor == 0
          && fileHeader.insnum == 0 && fileHeader.patnum + 1 == fileHeader.ordnum
          && fileHeader.globalvol == 128 && fileHeader.mv == 100 && fileHeader.speed == 1 && fileHeader.sep == 128 &&
          fileHeader.pwd == 0
          && fileHeader.msglength == 0 && fileHeader.msgoffset == 0 && fileHeader.reserved == 0) {
madeWithTracker = U_("OpenSPC conversion");
} else if(fileHeader.cwtv == 0x0214 && fileHeader.cmwt == 0x0200 && fileHeader.highlight_major == 0 &&
          fileHeader.highlight_minor == 0 && fileHeader.reserved == 0) {
m_dwLastSavedWithVersion = MPT_V("1.00.00.A5");
madeWithTracker = U_("ModPlug Tracker 1.00a5");
interpretModPlugMade = true;
} else if(fileHeader.cwtv == 0x0214 && fileHeader.cmwt == 0x0214 && !memcmp(&fileHeader.reserved, "CHBI", 4)) {
madeWithTracker = U_("ChibiTracker");
m_playBehaviour.reset(kITShortSampleRetrig);
} else if(fileHeader.cwtv == 0x0214 && fileHeader.cmwt == 0x0214 && fileHeader.special <= 1 && fileHeader.pwd == 0 &&
          fileHeader.reserved == 0
          && (fileHeader.flags &
              (ITFileHeader::vol0Optimisations | ITFileHeader::instrumentMode | ITFileHeader::useMIDIPitchController |
               ITFileHeader::reqEmbeddedMIDIConfig | ITFileHeader::extendedFilterRange)) == ITFileHeader::instrumentMode
          && m_nSamples > 0 && (Samples[1].filename == "XXXXXXXX.YYY")) {
madeWithTracker = U_("CheeseTracker");
} else if(fileHeader.cwtv == 0 && madeWithTracker.empty()) {
madeWithTracker = U_("Unknown");
} else if(fileHeader.cmwt < 0x0300 && madeWithTracker.empty()) {
if(fileHeader.cmwt > 0x0214) {
madeWithTracker = U_("Impulse Tracker 2.15");
} else if(fileHeader.cwtv > 0x0214) {
madeWithTracker = MPT_UFORMAT("Impulse Tracker 2.14p{}")(fileHeader.cwtv - 0x0214);
} else {
madeWithTracker = MPT_UFORMAT("Impulse Tracker {}.{}")((fileHeader.cwtv & 0x0F00) >> 8,
                                                       mpt::ufmt::hex0<2>((fileHeader.cwtv & 0xFF)));
}
if(m_FileHistory.empty() && fileHeader.reserved != 0) {
uint32 editTime = DecodeITEditTimer(fileHeader.cwtv, fileHeader.reserved);
FileHistory hist;
hist.openTime = static_cast<uint32>(editTime * (HISTORY_TIMER_PRECISION / 18.2));
m_FileHistory.push_back(hist);
}
}
break;
case 1:
madeWithTracker = GetSchismTrackerVersion(fileHeader.cwtv, fileHeader.reserved);
if(schismDateVersion < SchismVersionFromDate<2015, 01, 29>::date && m_SongFlags[SONG_LINEARSLIDES])
m_playBehaviour.reset(kPeriodsAreHertz);
else if(schismDateVersion < SchismVersionFromDate<2021, 05, 02>::date && !m_SongFlags[SONG_LINEARSLIDES])
m_playBehaviour.reset(kPeriodsAreHertz);
if(schismDateVersion < SchismVersionFromDate<2016, 05, 13>::date)
m_playBehaviour.reset(kITShortSampleRetrig);
if(schismDateVersion < SchismVersionFromDate<2021, 05, 02>::date)
m_playBehaviour.reset(kITDoNotOverrideChannelPan);
if(schismDateVersion < SchismVersionFromDate<2021, 05, 02>::date)
m_playBehaviour.reset(kITPanningReset);
break;
case 4:
madeWithTracker = MPT_UFORMAT("pyIT {}.{}")((fileHeader.cwtv & 0x0F00) >> 8,
                                            mpt::ufmt::hex0<2>(fileHeader.cwtv & 0xFF));
break;
case 6:
madeWithTracker = U_("BeRoTracker");
break;
case 7:
if(fileHeader.cwtv == 0x7FFF && fileHeader.cmwt == 0x0215)
madeWithTracker = U_("munch.py");
else
madeWithTracker = MPT_UFORMAT("ITMCK {}.{}.{}")((fileHeader.cwtv >> 8) & 0x0F, (fileHeader.cwtv >> 4) & 0x0F,
                                                fileHeader.cwtv & 0x0F);
break;
case 0xD:
madeWithTracker = U_("spc2it");
break;
}
}
if(GetType() == MOD_TYPE_MPT) {
if(fileHeader.cwtv > 0x0889 && file.Seek(mptStartPos)) {
LoadMPTMProperties(file, fileHeader.cwtv);
}
}
m_modFormat.formatName = (GetType() == MOD_TYPE_MPT) ? U_("OpenMPT MPTM") : MPT_UFORMAT("Impulse Tracker {}.{}")(
        fileHeader.cmwt >> 8, mpt::ufmt::hex0<2>(fileHeader.cmwt & 0xFF));
m_modFormat.type = (GetType() == MOD_TYPE_MPT) ? U_("mptm") : U_("it");
m_modFormat.madeWithTracker = std::move(madeWithTracker);
m_modFormat.charset = m_dwLastSavedWithVersion ? mpt::Charset::Windows1252 : mpt::Charset::CP437;
return true;
}
void CSoundFile::LoadMPTMProperties(FileReader &file, uint16 cwtv) {
std::istringstream iStrm(mpt::buffer_cast<std::string>(file.GetRawDataAsByteVector()));
if(cwtv >= 0x88D) {
srlztn::SsbRead ssb(iStrm);
ssb.BeginRead("mptm", Version::Current().GetRawVersion());
int8 useUTF8Tuning = 0;
ssb.ReadItem(useUTF8Tuning, "UTF8Tuning");
mpt::Charset TuningCharset = useUTF8Tuning ? mpt::Charset::UTF8 : GetCharsetInternal();
ssb.ReadItem(GetTuneSpecificTunings(), "0", [TuningCharset](std::istream &iStrm, CTuningCollection &tc,
                                                            const std::size_t dummy) {
return ReadTuningCollection(iStrm, tc, dummy, TuningCharset);
});
ssb.ReadItem(*this, "1", [TuningCharset](std::istream &iStrm, CSoundFile &csf, const std::size_t dummy) {
return ReadTuningMap(iStrm, csf, dummy, TuningCharset);
});
ssb.ReadItem(Order, "2", &ReadModSequenceOld);
ssb.ReadItem(Patterns, FileIdPatterns, &ReadModPatterns);
mpt::Charset sequenceDefaultCharset = GetCharsetInternal();
ssb.ReadItem(Order, FileIdSequences, [sequenceDefaultCharset](std::istream &iStrm, ModSequenceSet &seq,
                                                              std::size_t nSize) {
return ReadModSequences(iStrm, seq, nSize, sequenceDefaultCharset);
});
if(ssb.GetStatus() & srlztn::SNT_FAILURE) {
AddToLog(LogError, U_("Unknown error occurred while deserializing file."));
}
} else {
mpt::ustring name;
if(GetTuneSpecificTunings().Deserialize(iStrm, name, GetCharsetInternal()) != Tuning::SerializationResult::Success) {
AddToLog(LogError, U_("Loading tune specific tunings failed."));
} else {
ReadTuningMapImpl(iStrm, *this, GetCharsetInternal(), 0, cwtv < 0x88C);
}
}
}
#ifndef MODPLUG_NO_FILESAVE
static uint32 SaveITEditHistory(const CSoundFile &sndFile, std::ostream *file) {
size_t num = sndFile.GetFileHistory().size();
#ifdef MODPLUG_TRACKER
const CModDoc *pModDoc = sndFile.GetpModDoc();
    num += (pModDoc != nullptr) ? 1 : 0;	// + 1 for this session
#endif // MODPLUG_TRACKER
uint16 fnum = mpt::saturate_cast<uint16>(num);    // Number of entries that are actually going to be written
const uint32 bytesWritten = 2 + fnum * 8;        // Number of bytes that are actually going to be written

if(!file) {
return bytesWritten;
}
std::ostream &f = *file;
mpt::IO::WriteIntLE(f, fnum);
const size_t start = (num > uint16_max) ? num - uint16_max : 0;
for(size_t n = start; n < num; n++) {
FileHistory mptHistory;
#ifdef MODPLUG_TRACKER
if(n < sndFile.GetFileHistory().size())
#endif // MODPLUG_TRACKER
{
mptHistory = sndFile.GetFileHistory()[n];
#ifdef MODPLUG_TRACKER
} else if(pModDoc != nullptr)
        {
            const time_t creationTime = pModDoc->GetCreationTime();

            MemsetZero(mptHistory.loadDate);
            const tm* const p = localtime(&creationTime);
            if (p != nullptr)
                mptHistory.loadDate = *p;
            else
                sndFile.AddToLog(LogError, U_("Unable to retrieve current time."));

            mptHistory.openTime = (uint32)(difftime(time(nullptr), creationTime) * HISTORY_TIMER_PRECISION);
#endif // MODPLUG_TRACKER
}
ITHistoryStruct itHistory;
itHistory.ConvertToIT(mptHistory);
mpt::IO::Write(f, itHistory);
}
return bytesWritten;
}
bool CSoundFile::SaveIT(std::ostream &f, const mpt::PathString &filename, bool compatibilityExport) {
const CModSpecifications &specs = (GetType() == MOD_TYPE_MPT ? ModSpecs::mptm : (compatibilityExport ? ModSpecs::it
                                                                                                     : ModSpecs::itEx));
uint32 dwChnNamLen;
ITFileHeader itHeader;
uint64 dwPos = 0;
uint32 dwHdrPos = 0, dwExtra = 0;
MemsetZero(itHeader);
dwChnNamLen = 0;
memcpy(itHeader.id, "IMPM", 4);
mpt::String::WriteBuf(mpt::String::nullTerminated, itHeader.songname) = m_songName;
itHeader.highlight_minor = (uint8)
std::min(m_nDefaultRowsPerBeat, ROWINDEX(uint8_max));
itHeader.highlight_major = (uint8)
std::min(m_nDefaultRowsPerMeasure, ROWINDEX(uint8_max));
if(GetType() == MOD_TYPE_MPT) {
itHeader.ordnum = Order().GetLengthTailTrimmed();
if(Order().NeedsExtraDatafield() && itHeader.ordnum > 256) {
itHeader.ordnum = 256;
}
} else {
itHeader.ordnum = std::min(Order().GetLengthTailTrimmed(), specs.ordersMax) + 1;
if(itHeader.ordnum < 2)
itHeader.ordnum = 2;
}
itHeader.insnum = std::min(m_nInstruments, specs.instrumentsMax);
itHeader.smpnum = std::min(m_nSamples, specs.samplesMax);
itHeader.patnum = std::min(Patterns.GetNumPatterns(), specs.patternsMax);
std::vector <uint32le> patpos(itHeader.patnum);
std::vector <uint32le> smppos(itHeader.smpnum);
std::vector <uint32le> inspos(itHeader.insnum);
if(GetType() == MOD_TYPE_MPT) {
itHeader.cwtv = verMptFileVer;    // Used in OMPT-hack versioning.
itHeader.cmwt = 0x888;
} else {
const uint32 mptVersion = Version::Current().GetRawVersion();
itHeader.cwtv = 0x5000 | static_cast<uint16>((mptVersion >> 16) &
                                             0x0FFF); // format: txyy (t = tracker ID, x = version major, yy = version minor), e.g. 0x5117 (OpenMPT = 5, 117 = v1.17)
itHeader.cmwt = 0x0214;    // Common compatible tracker :)
for(INSTRUMENTINDEX nIns = 1; nIns <= GetNumInstruments(); nIns++) {
if(Instruments[nIns] && Instruments[nIns]->PitchEnv.dwFlags[ENV_FILTER]) {
itHeader.cmwt = 0x0216;
break;
}
}
if(compatibilityExport)
itHeader.reserved = mptVersion & 0xFFFF;
else
memcpy(&itHeader.reserved, "OMPT", 4);
}
itHeader.flags = ITFileHeader::useStereoPlayback | ITFileHeader::useMIDIPitchController;
itHeader.special = ITFileHeader::embedEditHistory | ITFileHeader::embedPatternHighlights;
if(m_nInstruments) itHeader.flags |= ITFileHeader::instrumentMode;
if(m_SongFlags[SONG_LINEARSLIDES]) itHeader.flags |= ITFileHeader::linearSlides;
if(m_SongFlags[SONG_ITOLDEFFECTS]) itHeader.flags |= ITFileHeader::itOldEffects;
if(m_SongFlags[SONG_ITCOMPATGXX]) itHeader.flags |= ITFileHeader::itCompatGxx;
if(m_SongFlags[SONG_EXFILTERRANGE] && !compatibilityExport) itHeader.flags |= ITFileHeader::extendedFilterRange;
itHeader.globalvol = static_cast<uint8>(m_nDefaultGlobalVolume / 2u);
itHeader.mv = static_cast<uint8>(std::min(m_nSamplePreAmp, uint32(128)));
itHeader.speed = mpt::saturate_cast<uint8>(m_nDefaultSpeed);
itHeader.tempo = mpt::saturate_cast<uint8>(
        m_nDefaultTempo.GetInt()); // We save the real tempo in an extension below if it exceeds 255.
itHeader.sep = 128; // pan separation
for(INSTRUMENTINDEX ins = 1; ins <= GetNumInstruments(); ins++) {
if(Instruments[ins] != nullptr && Instruments[ins]->midiPWD != 0) {
itHeader.pwd = static_cast<uint8>(std::abs(Instruments[ins]->midiPWD));
break;
}
}
dwHdrPos = sizeof(itHeader) + itHeader.ordnum;
memset(itHeader.chnpan, 0xA0, 64);
memset(itHeader.chnvol, 64, 64);
for(CHANNELINDEX ich = 0;
    ich < std::min(m_nChannels, CHANNELINDEX(64)); ich++) // Header only has room for settings for 64 chans...
{
itHeader.chnpan[ich] = (uint8)(ChnSettings[ich].nPan >> 2);
if(ChnSettings[ich].dwFlags[CHN_SURROUND]) itHeader.chnpan[ich] = 100;
itHeader.chnvol[ich] = (uint8)(ChnSettings[ich].nVolume);
#ifdef MODPLUG_TRACKER
if(TrackerSettings::Instance().MiscSaveChannelMuteStatus)
#endif
if(ChnSettings[ich].dwFlags[CHN_MUTE]) itHeader.chnpan[ich] |= 0x80;
}
if(!compatibilityExport) {
for(CHANNELINDEX i = 0; i < m_nChannels; i++) {
if(ChnSettings[i].szName[0]) {
dwChnNamLen = (i + 1) * MAX_CHANNELNAME;
}
}
if(dwChnNamLen) dwExtra += dwChnNamLen + 8;
}
if(!m_MidiCfg.IsMacroDefaultSetupUsed()) {
itHeader.flags |= ITFileHeader::reqEmbeddedMIDIConfig;
itHeader.special |= ITFileHeader::embedMIDIConfiguration;
dwExtra += sizeof(MIDIMacroConfigData);
}
const PATTERNINDEX numNamedPats = compatibilityExport ? 0 : Patterns.GetNumNamedPatterns();
if(numNamedPats > 0) {
dwExtra += (numNamedPats * MAX_PATTERNNAME) + 8;
}
if(!compatibilityExport) {
dwExtra += SaveMixPlugins(nullptr, true);
}
dwExtra += SaveITEditHistory(*this, nullptr);
uint16 msglength = 0;
if(!m_songMessage.empty()) {
itHeader.special |= ITFileHeader::embedSongMessage;
itHeader.msglength = msglength = mpt::saturate_cast<uint16>(m_songMessage.length() + 1u);
itHeader.msgoffset = dwHdrPos + dwExtra + (itHeader.insnum + itHeader.smpnum + itHeader.patnum) * 4;
}
mpt::IO::Write(f, itHeader);
Order().WriteAsByte(f, itHeader.ordnum);
mpt::IO::Write(f, inspos);
mpt::IO::Write(f, smppos);
mpt::IO::Write(f, patpos);
SaveITEditHistory(*this, &f);
if(itHeader.flags & ITFileHeader::reqEmbeddedMIDIConfig) {
mpt::IO::Write(f, static_cast<MIDIMacroConfigData &>(m_MidiCfg));
}
if(numNamedPats) {
mpt::IO::WriteRaw(f, "PNAM", 4);
mpt::IO::WriteIntLE<uint32>(f, numNamedPats * MAX_PATTERNNAME);
for(PATTERNINDEX pat = 0; pat < numNamedPats; pat++) {
char name[MAX_PATTERNNAME];
mpt::String::WriteBuf(mpt::String::maybeNullTerminated, name) = Patterns[pat].GetName();
mpt::IO::Write(f, name);
}
}
if(dwChnNamLen && !compatibilityExport) {
mpt::IO::WriteRaw(f, "CNAM", 4);
mpt::IO::WriteIntLE<uint32>(f, dwChnNamLen);
uint32 nChnNames = dwChnNamLen / MAX_CHANNELNAME;
for(uint32 inam = 0; inam < nChnNames; inam++) {
char name[MAX_CHANNELNAME];
mpt::String::WriteBuf(mpt::String::maybeNullTerminated, name) = ChnSettings[inam].szName;
mpt::IO::Write(f, name);
}
}
if(!compatibilityExport) {
SaveMixPlugins(&f, false);
}
dwPos = dwHdrPos + dwExtra + (itHeader.insnum + itHeader.smpnum + itHeader.patnum) * 4;
if(itHeader.special & ITFileHeader::embedSongMessage) {
dwPos += msglength;
mpt::IO::WriteRaw(f, m_songMessage.c_str(), msglength);
}
const ModInstrument dummyInstr;
for(INSTRUMENTINDEX nins = 1; nins <= itHeader.insnum; nins++) {
ITInstrumentEx iti;
uint32 instSize;
const ModInstrument &instr = (Instruments[nins] != nullptr) ? *Instruments[nins] : dummyInstr;
instSize = iti.ConvertToIT(instr, compatibilityExport, *this);
inspos[nins - 1] = static_cast<uint32>(dwPos);
dwPos += instSize;
mpt::IO::WritePartial(f, iti, instSize);
}
ITSample itss;
MemsetZero(itss);
for(SAMPLEINDEX smp = 0; smp < itHeader.smpnum; smp++) {
smppos[smp] = static_cast<uint32>(dwPos);
dwPos += sizeof(ITSample);
mpt::IO::Write(f, itss);
}
bool needsMptPatSave = false;
for(PATTERNINDEX pat = 0; pat < itHeader.patnum; pat++) {
uint32 dwPatPos = static_cast<uint32>(dwPos);
if(!Patterns.IsValidPat(pat)) continue;
if(Patterns[pat].GetOverrideSignature())
needsMptPatSave = true;
if(Patterns[pat].GetNumRows() == 64 && Patterns.IsPatternEmpty(pat)) {
patpos[pat] = 0;
continue;
}
patpos[pat] = static_cast<uint32>(dwPos);
ROWINDEX writeRows = mpt::saturate_cast<uint16>(Patterns[pat].GetNumRows());
uint16 writeSize = 0;
uint16le patinfo[4];
patinfo[0] = 0;
patinfo[1] = static_cast<uint16>(writeRows);
patinfo[2] = 0;
patinfo[3] = 0;
mpt::IO::Write(f, patinfo);
dwPos += 8;
struct ChnState {
ModCommand lastCmd;
uint8 mask = 0xFF;
};
const CHANNELINDEX maxChannels = std::min(specs.channelsMax, GetNumChannels());
std::vector <ChnState> chnStates(maxChannels);
std::vector <uint8> buf(7 * maxChannels + 1);
for(ROWINDEX row = 0; row < writeRows; row++) {
uint32 len = 0;
const ModCommand *m = Patterns[pat].GetpModCommand(row, 0);
for(CHANNELINDEX ch = 0; ch < maxChannels; ch++, m++) {
if(m->IsPcNote()) {
needsMptPatSave = true;
continue;
}
auto &chnState = chnStates[ch];
uint8 b = 0;
uint8 command = m->command;
uint8 param = m->param;
uint8 vol = 0xFF;
uint8 note = m->note;
if(note != NOTE_NONE) b |= 1;
if(m->IsNote()) note -= NOTE_MIN;
if(note == NOTE_FADE && GetType() != MOD_TYPE_MPT) note = 0xF6;
if(m->instr) b |= 2;
if(m->volcmd != VOLCMD_NONE) {
vol = std::min(m->vol, uint8(9));
switch (m->volcmd) {
case VOLCMD_VOLUME:
vol = std::min(m->vol, uint8(64));
break;
case VOLCMD_PANNING:
vol = std::min(m->vol, uint8(64)) + 128;
break;
case VOLCMD_VOLSLIDEUP:
vol += 85;
break;
case VOLCMD_VOLSLIDEDOWN:
vol += 95;
break;
case VOLCMD_FINEVOLUP:
vol += 65;
break;
case VOLCMD_FINEVOLDOWN:
vol += 75;
break;
case VOLCMD_VIBRATODEPTH:
vol += 203;
break;
case VOLCMD_TONEPORTAMENTO:
vol += 193;
break;
case VOLCMD_PORTADOWN:
vol += 105;
break;
case VOLCMD_PORTAUP:
vol += 115;
break;
case VOLCMD_VIBRATOSPEED:
if(command == CMD_NONE) {
command = CMD_VIBRATO;
param = std::min(m->vol, uint8(15)) << 4;
vol = 0xFF;
} else {
vol = 203;
}
break;
case VOLCMD_OFFSET:
if(!compatibilityExport)
vol += 223;
else
vol = 0xFF;
break;
default:
vol = 0xFF;
}
}
if(vol != 0xFF) b |= 4;
if(command != CMD_NONE) {
S3MSaveConvert(command, param, true, compatibilityExport);
if(command) b |= 8;
}
if(b) {
if(b & 1) {
if((note == chnState.lastCmd.note) && (chnState.lastCmd.volcmd & 1)) {
b &= ~1;
b |= 0x10;
} else {
chnState.lastCmd.note = note;
chnState.lastCmd.volcmd |= 1;
}
}
if(b & 2) {
if((m->instr == chnState.lastCmd.instr) && (chnState.lastCmd.volcmd & 2)) {
b &= ~2;
b |= 0x20;
} else {
chnState.lastCmd.instr = m->instr;
chnState.lastCmd.volcmd |= 2;
}
}
if(b & 4) {
if((vol == chnState.lastCmd.vol) && (chnState.lastCmd.volcmd & 4)) {
b &= ~4;
b |= 0x40;
} else {
chnState.lastCmd.vol = vol;
chnState.lastCmd.volcmd |= 4;
}
}
if(b & 8) {
if((command == chnState.lastCmd.command) && (param == chnState.lastCmd.param) && (chnState.lastCmd.volcmd & 8)) {
b &= ~8;
b |= 0x80;
} else {
chnState.lastCmd.command = command;
chnState.lastCmd.param = param;
chnState.lastCmd.volcmd |= 8;
}
}
if(b != chnState.mask) {
chnState.mask = b;
buf[len++] = static_cast<uint8>((ch + 1) | IT_bitmask_patternChanEnabled_c);
buf[len++] = b;
} else {
buf[len++] = static_cast<uint8>(ch + 1);
}
if(b & 1) buf[len++] = note;
if(b & 2) buf[len++] = m->instr;
if(b & 4) buf[len++] = vol;
if(b & 8) {
buf[len++] = command;
buf[len++] = param;
}
}
}
buf[len++] = 0;
if(writeSize > uint16_max - len) {
AddToLog(LogWarning, MPT_UFORMAT(
        "Warning: File format limit was reached. Some pattern data may not get written to file. (pattern {})")(pat));
break;
} else {
dwPos += len;
writeSize += (uint16)
len;
mpt::IO::WriteRaw(f, buf.data(), len);
}
}
mpt::IO::SeekAbsolute(f, dwPatPos);
patinfo[0] = writeSize;
mpt::IO::Write(f, patinfo);
mpt::IO::SeekAbsolute(f, dwPos);
}
for(SAMPLEINDEX smp = 1; smp <= itHeader.smpnum; smp++) {
const ModSample &sample = Samples[smp];
#ifdef MODPLUG_TRACKER
uint32 type = GetType() == MOD_TYPE_IT ? 1 : 4;
        if(compatibilityExport) type = 2;
        bool compress = ((((sample.GetNumChannels() > 1) ? TrackerSettings::Instance().MiscITCompressionStereo : TrackerSettings::Instance().MiscITCompressionMono) & type) != 0);
#else
bool compress = false;
#endif // MODPLUG_TRACKER
itss.ConvertToIT(sample, GetType(), compress, itHeader.cmwt >= 0x215, GetType() == MOD_TYPE_MPT);
const bool isExternal = itss.cvt == ITSample::cvtExternalSample;
mpt::String::WriteBuf(mpt::String::nullTerminated, itss.name) = m_szNames[smp];
itss.samplepointer = static_cast<uint32>(dwPos);
if(dwPos > uint32_max) {
AddToLog(LogWarning, MPT_UFORMAT("Cannot save sample {}: File size exceeds 4 GB.")(smp));
itss.samplepointer = 0;
itss.length = 0;
}
SmpLength smpLength = itss.length;    // Possibly truncated to 2^32 samples
mpt::IO::SeekAbsolute(f, smppos[smp - 1]);
mpt::IO::Write(f, itss);
if(dwPos > uint32_max) {
continue;
}
mpt::IO::SeekAbsolute(f, dwPos);
if(!isExternal) {
if(sample.nLength > smpLength && smpLength != 0) {
AddToLog(LogWarning, MPT_UFORMAT("Truncating sample {}: Length exceeds exceeds 4 gigasamples.")(smp));
}
dwPos += itss.GetSampleFormat().WriteSample(f, sample, smpLength);
} else {
#ifdef MPT_EXTERNAL_SAMPLES
const std::string filenameU8 = GetSamplePath(smp).AbsolutePathToRelative(filename.GetPath()).ToUTF8();
            const size_t strSize = filenameU8.size();
            size_t intBytes = 0;
            if(mpt::IO::WriteVarInt(f, strSize, &intBytes))
            {
                dwPos += intBytes + strSize;
                mpt::IO::WriteRaw(f, filenameU8.data(), strSize);
            }
#else
MPT_UNREFERENCED_PARAMETER(filename);
#endif // MPT_EXTERNAL_SAMPLES
}
}
if(!compatibilityExport) {
if(GetNumInstruments()) {
SaveExtendedInstrumentProperties(itHeader.insnum, f);
}
SaveExtendedSongProperties(f);
}
mpt::IO::SeekAbsolute(f, dwHdrPos);
mpt::IO::Write(f, inspos);
mpt::IO::Write(f, smppos);
mpt::IO::Write(f, patpos);
if(GetType() == MOD_TYPE_IT) {
return true;
}
bool success = true;
mpt::IO::SeekEnd(f);
const mpt::IO::Offset MPTStartPos = mpt::IO::TellWrite(f);
srlztn::SsbWrite ssb(f);
ssb.BeginWrite("mptm", Version::Current().GetRawVersion());
if(GetTuneSpecificTunings().GetNumTunings() > 0 || AreNonDefaultTuningsUsed(*this))
ssb.WriteItem(int8(1), "UTF8Tuning");
if(GetTuneSpecificTunings().GetNumTunings() > 0)
ssb.WriteItem(GetTuneSpecificTunings(), "0", &WriteTuningCollection);
if(AreNonDefaultTuningsUsed(*this))
ssb.WriteItem(*this, "1", &WriteTuningMap);
if(Order().NeedsExtraDatafield())
ssb.WriteItem(Order, "2", &WriteModSequenceOld);
if(needsMptPatSave)
ssb.WriteItem(Patterns, FileIdPatterns, &WriteModPatterns);
ssb.WriteItem(Order, FileIdSequences, &WriteModSequences);
ssb.FinishWrite();
if(ssb.GetStatus() & srlztn::SNT_FAILURE) {
AddToLog(LogError, U_("Error occurred in writing MPTM extensions."));
}
if(!f.good()) {
f.clear();
success = false;
}
mpt::IO::WriteIntLE<uint32>(f, static_cast<uint32>(MPTStartPos));
mpt::IO::SeekEnd(f);
return success;
}
#endif // MODPLUG_NO_FILESAVE
#ifndef MODPLUG_NO_FILESAVE
uint32 CSoundFile::SaveMixPlugins(std::ostream *file, bool updatePlugData) {
#ifndef NO_PLUGINS
uint32 totalSize = 0;
for(PLUGINDEX i = 0; i < MAX_MIXPLUGINS; i++) {
const SNDMIXPLUGIN &plugin = m_MixPlugins[i];
if(plugin.IsValidPlugin()) {
uint32 chunkSize = sizeof(SNDMIXPLUGININFO) + 4; // plugininfo+4 (datalen)
if(plugin.pMixPlugin && updatePlugData) {
plugin.pMixPlugin->SaveAllParameters();
}
const uint32 extraDataSize =
        4 + sizeof(float32) + // 4 for ID and size of dryRatio
        4 + sizeof(int32);    // Default Program

chunkSize += extraDataSize + 4; // +4 is for size field itself

const uint32 plugDataSize = std::min(mpt::saturate_cast<uint32>(plugin.pluginData.size()), uint32_max - chunkSize);
chunkSize += plugDataSize;
if(file) {
std::ostream &f = *file;
char id[4] = {'F', 'X', '0', '0'};
if(i >= 100) id[1] = '0' + (i / 100u);
id[2] += (i / 10u) % 10u;
id[3] += (i % 10u);
mpt::IO::WriteRaw(f, id, 4);
mpt::IO::WriteIntLE<uint32>(f, chunkSize);
mpt::IO::Write(f, m_MixPlugins[i].Info);
mpt::IO::WriteIntLE<uint32>(f, plugDataSize);
if(plugDataSize) {
mpt::IO::WriteRaw(f, m_MixPlugins[i].pluginData.data(), plugDataSize);
}
mpt::IO::WriteIntLE<uint32>(f, extraDataSize);
mpt::IO::WriteRaw(f, "DWRT", 4);
static_assert(sizeof(IEEE754binary32LE) == 4);
mpt::IO::Write(f, IEEE754binary32LE(m_MixPlugins[i].fDryRatio));
mpt::IO::WriteRaw(f, "PROG", 4);
static_assert(sizeof(m_MixPlugins[i].defaultProgram) == sizeof(int32));
mpt::IO::WriteIntLE<int32>(f, m_MixPlugins[i].defaultProgram);
}
totalSize += chunkSize + 8;
}
}
std::vector <uint32le> chinfo(GetNumChannels());
uint32 numChInfo = 0;
for(CHANNELINDEX j = 0; j < GetNumChannels(); j++) {
if((chinfo[j] = ChnSettings[j].nMixPlugin) != 0) {
numChInfo = j + 1;
}
}
if(numChInfo) {
if(file) {
std::ostream &f = *file;
mpt::IO::WriteRaw(f, "CHFX", 4);
mpt::IO::WriteIntLE<uint32>(f, numChInfo * 4);
chinfo.resize(numChInfo);
mpt::IO::Write(f, chinfo);
}
totalSize += numChInfo * 4 + 8;
}
return totalSize;
#else
MPT_UNREFERENCED_PARAMETER(file);
    MPT_UNREFERENCED_PARAMETER(updatePlugData);
    return 0;
#endif // NO_PLUGINS
}
#endif // MODPLUG_NO_FILESAVE
bool CSoundFile::LoadMixPlugins(FileReader &file) {
bool isBeRoTracker = false;
while (file.CanRead(9)) {
char code[4];
file.ReadArray(code);
const uint32 chunkSize = file.ReadUint32LE();
if(!memcmp(code, "IMPI", 4)     // IT instrument, we definitely read too far
   || !memcmp(code, "IMPS", 4)  // IT sample, ditto
   || !memcmp(code, "XTPM", 4)  // Instrument extensions, ditto
   || !memcmp(code, "STPM", 4)  // Song extensions, ditto
   || !file.CanRead(chunkSize)) {
file.SkipBack(8);
return isBeRoTracker;
}
FileReader chunk = file.ReadChunk(chunkSize);
if(!memcmp(code, "CHFX", 4)) {
for(auto &chn : ChnSettings) {
chn.nMixPlugin = static_cast<PLUGINDEX>(chunk.ReadUint32LE());
}
#ifndef NO_PLUGINS
}
#define MPT_ISDIGIT(x) (code[(x)] >= '0' && code[(x)] <= '9')
else if(code[0] == 'F' && (code[1] == 'X' || MPT_ISDIGIT(1)) && MPT_ISDIGIT(2) && MPT_ISDIGIT(3))
#undef MPT_ISDIGIT
{
PLUGINDEX plug = (code[2] - '0') * 10 + (code[3] - '0');    //calculate plug-in number.
if(code[1] != 'X') plug += (code[1] - '0') * 100;
if(plug < MAX_MIXPLUGINS) {
ReadMixPluginChunk(chunk, m_MixPlugins[plug]);
}
#endif // NO_PLUGINS
} else if(!memcmp(code, "MODU", 4)) {
isBeRoTracker = true;
m_dwLastSavedWithVersion = Version();    // Reset MPT detection for old files that have a similar fingerprint
}
}
return isBeRoTracker;
}
#ifndef NO_PLUGINS
void CSoundFile::ReadMixPluginChunk(FileReader &file, SNDMIXPLUGIN &plugin) {
file.ReadStruct(plugin.Info);
mpt::String::SetNullTerminator(plugin.Info.szName.buf);
mpt::String::SetNullTerminator(plugin.Info.szLibraryName.buf);
plugin.editorX = plugin.editorY = int32_min;
FileReader pluginDataChunk = file.ReadChunk(file.ReadUint32LE());
plugin.pluginData.resize(mpt::saturate_cast<size_t>(pluginDataChunk.BytesLeft()));
pluginDataChunk.ReadRaw(mpt::as_span(plugin.pluginData));
if(FileReader modularData = file.ReadChunk(file.ReadUint32LE()); modularData.IsValid()) {
while (modularData.CanRead(5)) {
char code[4];
modularData.ReadArray(code);
uint32 dataSize = 0;
if(!memcmp(code, "DWRT", 4) || !memcmp(code, "PROG", 4)) {
dataSize = 4;
} else {
dataSize = modularData.ReadUint32LE();
}
FileReader dataChunk = modularData.ReadChunk(dataSize);
if(!memcmp(code, "DWRT", 4)) {
plugin.fDryRatio = dataChunk.ReadFloatLE();
} else if(!memcmp(code, "PROG", 4)) {
plugin.defaultProgram = dataChunk.ReadUint32LE();
} else if(!memcmp(code, "MCRO", 4)) {
}
}
}
}
#endif // NO_PLUGINS
#ifndef MODPLUG_NO_FILESAVE
void CSoundFile::SaveExtendedSongProperties(std::ostream &f) const {
const CModSpecifications &specs = GetModSpecifications();
mpt::IO::WriteIntLE<uint32>(f, MagicBE("MPTS"));
#define WRITEMODULARHEADER(code, fsize) \
    { \
        mpt::IO::WriteIntLE<uint32>(f, code); \
        MPT_ASSERT(mpt::in_range<uint16>(fsize)); \
        const uint16 _size = fsize; \
        mpt::IO::WriteIntLE<uint16>(f, _size); \
    }
#define WRITEMODULAR(code, field) \
    { \
        WRITEMODULARHEADER(code, sizeof(field)) \
        mpt::IO::WriteIntLE(f, field); \
    }
if(m_nDefaultTempo.GetInt() > 255) {
uint32 tempo = m_nDefaultTempo.GetInt();
WRITEMODULAR(MagicBE("DT.."), tempo);
}
if(m_nDefaultTempo.GetFract() != 0 && specs.hasFractionalTempo) {
uint32 tempo = m_nDefaultTempo.GetFract();
WRITEMODULAR(MagicLE("DTFR"), tempo);
}
if(m_nDefaultRowsPerBeat > 255 || m_nDefaultRowsPerMeasure > 255 || GetType() == MOD_TYPE_XM) {
WRITEMODULAR(MagicBE("RPB."), m_nDefaultRowsPerBeat);
WRITEMODULAR(MagicBE("RPM."), m_nDefaultRowsPerMeasure);
}
if(GetType() != MOD_TYPE_XM) {
WRITEMODULAR(MagicBE("C..."), m_nChannels);
}
if((GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) && GetNumChannels() > 64) {
WRITEMODULARHEADER(MagicBE("ChnS"), (GetNumChannels() - 64) * 2);
for(CHANNELINDEX chn = 64; chn < GetNumChannels(); chn++) {
uint8 panvol[2];
panvol[0] = (uint8)(ChnSettings[chn].nPan >> 2);
if(ChnSettings[chn].dwFlags[CHN_SURROUND]) panvol[0] = 100;
if(ChnSettings[chn].dwFlags[CHN_MUTE]) panvol[0] |= 0x80;
panvol[1] = (uint8)
ChnSettings[chn].nVolume;
mpt::IO::Write(f, panvol);
}
}
{
WRITEMODULARHEADER(MagicBE("TM.."), 1);
uint8 mode = static_cast<uint8>(m_nTempoMode);
mpt::IO::WriteIntLE(f, mode);
}
const int32 tmpMixLevels = static_cast<int32>(m_nMixLevels);
WRITEMODULAR(MagicBE("PMM."), tmpMixLevels);
if(m_dwCreatedWithVersion) {
WRITEMODULAR(MagicBE("CWV."), m_dwCreatedWithVersion.GetRawVersion());
}
WRITEMODULAR(MagicBE("LSWV"), Version::Current().GetRawVersion());
WRITEMODULAR(MagicBE("SPA."), m_nSamplePreAmp);
WRITEMODULAR(MagicBE("VSTV"), m_nVSTiVolume);
if(GetType() == MOD_TYPE_XM && m_nDefaultGlobalVolume != MAX_GLOBAL_VOLUME) {
WRITEMODULAR(MagicBE("DGV."), m_nDefaultGlobalVolume);
}
if(GetType() != MOD_TYPE_XM && Order().GetRestartPos() != 0) {
WRITEMODULAR(MagicBE("RP.."), Order().GetRestartPos());
}
if(m_nResampling != SRCMODE_DEFAULT && specs.hasDefaultResampling) {
WRITEMODULAR(MagicLE("RSMP"), static_cast<uint32>(m_nResampling));
}
if(GetType() == MOD_TYPE_MPT) {
for(SAMPLEINDEX smp = 1; smp <= GetNumSamples(); smp++) {
const ModSample &sample = Samples[smp];
if(sample.nLength && sample.HasCustomCuePoints()) {
WRITEMODULARHEADER(MagicLE("CUES"), static_cast<uint16>(2 + std::size(sample.cues) * 4));
mpt::IO::WriteIntLE<uint16>(f, smp);
for(auto cue : sample.cues) {
mpt::IO::WriteIntLE<uint32>(f, cue);
}
}
}
}
if(!m_tempoSwing.empty()) {
std::ostringstream oStrm;
TempoSwing::Serialize(oStrm, m_tempoSwing);
std::string data = oStrm.str();
uint16 length = mpt::saturate_cast<uint16>(data.size());
WRITEMODULARHEADER(MagicLE("SWNG"), length);
mpt::IO::WriteRaw(f, data.data(), length);
}
{
uint8 bits[(kMaxPlayBehaviours + 7) / 8u];
MemsetZero(bits);
size_t maxBit = 0;
for(size_t i = 0; i < kMaxPlayBehaviours; i++) {
if(m_playBehaviour[i]) {
bits[i >> 3] |= 1 << (i & 0x07);
maxBit = i + 8;
}
}
uint16 numBytes = static_cast<uint16>(maxBit / 8u);
WRITEMODULARHEADER(MagicBE("MSF."), numBytes);
mpt::IO::WriteRaw(f, bits, numBytes);
}
if(!m_songArtist.empty() && specs.hasArtistName) {
std::string songArtistU8 = mpt::ToCharset(mpt::Charset::UTF8, m_songArtist);
uint16 length = mpt::saturate_cast<uint16>(songArtistU8.length());
WRITEMODULARHEADER(MagicLE("AUTH"), length);
mpt::IO::WriteRaw(f, songArtistU8.c_str(), length);
}
#ifdef MODPLUG_TRACKER
if(GetMIDIMapper().GetCount() > 0)
    {
        const size_t objectsize = GetMIDIMapper().Serialize();
        if(!mpt::in_range<uint16>(objectsize))
        {
            AddToLog(LogWarning, U_("Too many MIDI Mapping directives to save; data won't be written."));
        } else
        {
            WRITEMODULARHEADER(MagicBE("MIMA"), static_cast<uint16>(objectsize));
            GetMIDIMapper().Serialize(&f);
        }
    }
    {
        CHANNELINDEX numChannels = 0;
        for(CHANNELINDEX i = 0; i < m_nChannels; i++)
        {
            if(ChnSettings[i].color != ModChannelSettings::INVALID_COLOR)
            {
                numChannels = i + 1;
            }
        }
        if(numChannels > 0)
        {
            WRITEMODULARHEADER(MagicLE("CCOL"), numChannels * 4);
            for(CHANNELINDEX i = 0; i < numChannels; i++)
            {
                uint32 color = ChnSettings[i].color;
                if(color != ModChannelSettings::INVALID_COLOR)
                    color &= 0x00FFFFFF;
                std::array<uint8, 4> rgb{static_cast<uint8>(color), static_cast<uint8>(color >> 8), static_cast<uint8>(color >> 16), static_cast<uint8>(color >> 24)};
                mpt::IO::Write(f, rgb);
            }
        }
    }
#endif
#undef WRITEMODULAR
#undef WRITEMODULARHEADER
return;
}
#endif // MODPLUG_NO_FILESAVE
template<typename T>
void ReadField(FileReader &chunk, std::size_t size, T &field) {
field = chunk.ReadSizedIntLE<T>(size);
}
template<typename T>
void ReadFieldCast(FileReader &chunk, std::size_t size, T &field) {
static_assert(sizeof(T) <= sizeof(int32));
field = static_cast<T>(chunk.ReadSizedIntLE<int32>(size));
}
void CSoundFile::LoadExtendedSongProperties(FileReader &file, bool ignoreChannelCount, bool *pInterpretMptMade) {
if(!file.ReadMagic("STPM"))    // 'MPTS'
{
return;
}
if(pInterpretMptMade != nullptr)
*pInterpretMptMade = true;
m_playBehaviour.reset();
while (file.CanRead(7)) {
const uint32 code = file.ReadUint32LE();
const uint16 size = file.ReadUint16LE();
if(code == MagicLE("228\x04")) {
file.SkipBack(6);
break;
} else if((code & 0x80808080) || !(code & 0x60606060) || !file.CanRead(size)) {
break;
}
FileReader chunk = file.ReadChunk(size);
switch (code)                    // interpret field code
{
case MagicBE("DT.."): {
uint32 tempo;
ReadField(chunk, size, tempo);
m_nDefaultTempo.Set(tempo, m_nDefaultTempo.GetFract());
break;
}
case MagicLE("DTFR"): {
uint32 tempoFract;
ReadField(chunk, size, tempoFract);
m_nDefaultTempo.Set(m_nDefaultTempo.GetInt(), tempoFract);
break;
}
case MagicBE("RPB."):
ReadField(chunk, size, m_nDefaultRowsPerBeat);
break;
case MagicBE("RPM."):
ReadField(chunk, size, m_nDefaultRowsPerMeasure);
break;
case MagicBE("C..."):
if(!ignoreChannelCount) {
CHANNELINDEX chn = 0;
ReadField(chunk, size, chn);
m_nChannels = Clamp(chn, m_nChannels, MAX_BASECHANNELS);
}
break;
case MagicBE("TM.."):
ReadFieldCast(chunk, size, m_nTempoMode);
break;
case MagicBE("PMM."):
ReadFieldCast(chunk, size, m_nMixLevels);
break;
case MagicBE("CWV."): {
uint32 ver = 0;
ReadField(chunk, size, ver);
m_dwCreatedWithVersion = Version(ver);
break;
}
case MagicBE("LSWV"): {
uint32 ver = 0;
ReadField(chunk, size, ver);
if(ver != 0) { m_dwLastSavedWithVersion = Version(ver); }
break;
}
case MagicBE("SPA."):
ReadField(chunk, size, m_nSamplePreAmp);
break;
case MagicBE("VSTV"):
ReadField(chunk, size, m_nVSTiVolume);
break;
case MagicBE("DGV."):
ReadField(chunk, size, m_nDefaultGlobalVolume);
break;
case MagicBE("RP.."):
if(GetType() != MOD_TYPE_XM) {
ORDERINDEX restartPos;
ReadField(chunk, size, restartPos);
Order().SetRestartPos(restartPos);
}
break;
case MagicLE("RSMP"):
ReadFieldCast(chunk, size, m_nResampling);
if(!Resampling::IsKnownMode(m_nResampling)) m_nResampling = SRCMODE_DEFAULT;
break;
#ifdef MODPLUG_TRACKER
case MagicBE("MIMA"): GetMIDIMapper().Deserialize(chunk); break;

            case MagicLE("CCOL"):
                {
                    const CHANNELINDEX numChannels = std::min(MAX_BASECHANNELS, static_cast<CHANNELINDEX>(size / 4u));
                    for(CHANNELINDEX i = 0; i < numChannels; i++)
                    {
                        auto rgb = chunk.ReadArray<uint8, 4>();
                        if(rgb[3])
                            ChnSettings[i].color = ModChannelSettings::INVALID_COLOR;
                        else
                            ChnSettings[i].color = rgb[0] | (rgb[1] << 8) | (rgb[2] << 16);
                    }
                }
                break;
#endif
case MagicLE("AUTH"): {
std::string artist;
chunk.ReadString<mpt::String::spacePadded>(artist, chunk.GetLength());
m_songArtist = mpt::ToUnicode(mpt::Charset::UTF8, artist);
}
break;
case MagicBE("ChnS"):
if(size <= (MAX_BASECHANNELS - 64) * 2 && (size % 2u) == 0) {
static_assert(mpt::array_size<decltype(ChnSettings)>::size >= 64);
const CHANNELINDEX loopLimit = std::min(uint16(64 + size / 2), uint16(std::size(ChnSettings)));
for(CHANNELINDEX chn = 64; chn < loopLimit; chn++) {
auto[pan, vol] = chunk.ReadArray<uint8, 2>();
if(pan != 0xFF) {
ChnSettings[chn].nVolume = vol;
ChnSettings[chn].nPan = 128;
ChnSettings[chn].dwFlags.reset();
if(pan & 0x80) ChnSettings[chn].dwFlags.set(CHN_MUTE);
pan &= 0x7F;
if(pan <= 64) ChnSettings[chn].nPan = pan << 2;
if(pan == 100) ChnSettings[chn].dwFlags.set(CHN_SURROUND);
}
}
}
break;
case MagicLE("CUES"):
if(size > 2) {
SAMPLEINDEX smp = chunk.ReadUint16LE();
if(smp > 0 && smp <= GetNumSamples()) {
ModSample &sample = Samples[smp];
for(auto &cue : sample.cues) {
cue = chunk.ReadUint32LE();
}
}
}
break;
case MagicLE("SWNG"):
if(size > 2) {
std::istringstream iStrm(mpt::buffer_cast<std::string>(chunk.ReadRawDataAsByteVector()));
TempoSwing::Deserialize(iStrm, m_tempoSwing, chunk.GetLength());
}
break;
case MagicBE("MSF."): {
size_t bit = 0;
m_playBehaviour.reset();
while (chunk.CanRead(1) && bit < m_playBehaviour.size()) {
uint8 b = chunk.ReadUint8();
for(uint8 i = 0; i < 8; i++, bit++) {
if((b & (1 << i)) && bit < m_playBehaviour.size()) {
m_playBehaviour.set(bit);
}
}
}
}
break;
}
}
Limit(m_nDefaultTempo, GetModSpecifications().GetTempoMin(), GetModSpecifications().GetTempoMax());
if(m_nTempoMode >= TempoMode::NumModes) m_nTempoMode = TempoMode::Classic;
if(m_nMixLevels >= MixLevels::NumMixLevels) m_nMixLevels = MixLevels::Original;
//m_nVSTiVolume
LimitMax(m_nDefaultGlobalVolume, MAX_GLOBAL_VOLUME);
if(!m_tempoSwing.empty()) m_tempoSwing.resize(m_nDefaultRowsPerBeat);
}
OPENMPT_NAMESPACE_END
