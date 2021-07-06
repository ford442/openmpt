
#include "stdafx.h"
#include "Loaders.h"
#include "Tables.h"
#ifndef MODPLUG_NO_FILESAVE
#include "mpt/io/base.hpp"
#include "mpt/io/io.hpp"
#include "mpt/io/io_stdstream.hpp"
#include "../common/mptFileIO.h"
#endif
#ifdef MPT_EXTERNAL_SAMPLES
#include "../common/mptPathString.h"
#endif  // MPT_EXTERNAL_SAMPLES
OPENMPT_NAMESPACE_BEGIN
void CSoundFile::ConvertModCommand(ModCommand &m) {
switch (m.command) {
case 0x00:
if(m.param) m.command = CMD_ARPEGGIO;
break;
case 0x01:
m.command = CMD_PORTAMENTOUP;
break;
case 0x02:
m.command = CMD_PORTAMENTODOWN;
break;
case 0x03:
m.command = CMD_TONEPORTAMENTO;
break;
case 0x04:
m.command = CMD_VIBRATO;
break;
case 0x05:
m.command = CMD_TONEPORTAVOL;
break;
case 0x06:
m.command = CMD_VIBRATOVOL;
break;
case 0x07:
m.command = CMD_TREMOLO;
break;
case 0x08:
m.command = CMD_PANNING8;
break;
case 0x09:
m.command = CMD_OFFSET;
break;
case 0x0A:
m.command = CMD_VOLUMESLIDE;
break;
case 0x0B:
m.command = CMD_POSITIONJUMP;
break;
case 0x0C:
m.command = CMD_VOLUME;
break;
case 0x0D:
m.command = CMD_PATTERNBREAK;
m.param = ((m.param >> 4) * 10) + (m.param & 0x0F);
break;
case 0x0E:
m.command = CMD_MODCMDEX;
break;
case 0x0F:
// 0x20 is Tempo: ProTracker, XMPlay, Imago Orpheus, Cubic Player, ChibiTracker, BeRoTracker, DigiTrakker, DigiTrekker, Disorder Tracker 2, DMP, Extreme's Tracker, ...
if(m.param < 0x20)
m.command = CMD_SPEED;
else
m.command = CMD_TEMPO;
break;
case 'G' - 55:
m.command = CMD_GLOBALVOLUME;
break;        //16
case 'H' - 55:
m.command = CMD_GLOBALVOLSLIDE;
break;
case 'K' - 55:
m.command = CMD_KEYOFF;
break;
case 'L' - 55:
m.command = CMD_SETENVPOSITION;
break;
case 'P' - 55:
m.command = CMD_PANNINGSLIDE;
break;
case 'R' - 55:
m.command = CMD_RETRIG;
break;
case 'T' - 55:
m.command = CMD_TREMOR;
break;
case 'W' - 55:
m.command = CMD_DUMMY;
break;
case 'X' - 55:
m.command = CMD_XFINEPORTAUPDOWN;
break;
case 'Y' - 55:
m.command = CMD_PANBRELLO;
break;            //34
case 'Z' - 55:
m.command = CMD_MIDI;
break;                //35
case '\\' - 56:
m.command = CMD_SMOOTHMIDI;
break;        //rewbs.smoothVST: 36 - note: this is actually displayed as "-" in FT2, but seems to be doing nothing.
case '#' + 3:
m.command = CMD_XPARAM;
break;            //rewbs.XMfixes - Xm.param is 38
default:
m.command = CMD_NONE;
}
}
#ifndef MODPLUG_NO_FILESAVE
void CSoundFile::ModSaveCommand(uint8 & command, uint8 & param, bool
toXM,
bool compatibilityExport
) const
{
switch(command)
{
case CMD_NONE:
command = param = 0;
break;
case CMD_ARPEGGIO:
command = 0;
break;
case CMD_PORTAMENTOUP:
if (
GetType() &
(MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_STM|MOD_TYPE_MPT))
{
if ((param & 0xF0) == 0xE0) { command = 0x0E;
param = ((param & 0x0F) >> 2) | 0x10;
break; }
else if ((param & 0xF0) == 0xF0) { command = 0x0E;
param &= 0x0F; param |= 0x10; break; }
}
command = 0x01;
break;
case CMD_PORTAMENTODOWN:
if(
GetType() &
(MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_STM|MOD_TYPE_MPT))
{
if ((param & 0xF0) == 0xE0) { command = 0x0E;
param = ((param & 0x0F) >> 2) | 0x20;
break; }
else if ((param & 0xF0) == 0xF0) { command = 0x0E;
param &= 0x0F; param |= 0x20; break; }
}
command = 0x02;
break;
case CMD_TONEPORTAMENTO:
command = 0x03;
break;
case CMD_VIBRATO:
command = 0x04;
break;
case CMD_TONEPORTAVOL:
command = 0x05;
break;
case CMD_VIBRATOVOL:
command = 0x06;
break;
case CMD_TREMOLO:
command = 0x07;
break;
case CMD_PANNING8:
command = 0x08;
if(
GetType() &
MOD_TYPE_S3M)
{
if(param <= 0x80)
{
param = mpt::saturate_cast<uint8>(param * 2);
}
else if(param == 0xA4)    // surround
{
if(compatibilityExport || !toXM)
{
command = param = 0;
}
else
{
command = 'X' - 55;
param = 91;
}
}
}
break;
case CMD_OFFSET:
command = 0x09;
break;
case CMD_VOLUMESLIDE:
command = 0x0A;
break;
case CMD_POSITIONJUMP:
command = 0x0B;
break;
case CMD_VOLUME:
command = 0x0C;
break;
case CMD_PATTERNBREAK:
command = 0x0D;
param = ((param / 10) << 4) | (param % 10);
break;
case CMD_MODCMDEX:
command = 0x0E;
break;
case CMD_SPEED:
command = 0x0F;
param = std::min(param, uint8(0x1F));
break;
case CMD_TEMPO:
command = 0x0F;
param = std::max(param, uint8(0x20));
break;
case CMD_GLOBALVOLUME:
command = 'G' - 55;
break;
case CMD_GLOBALVOLSLIDE:
command = 'H' - 55;
break;
case CMD_KEYOFF:
command = 'K' - 55;
break;
case CMD_SETENVPOSITION:
command = 'L' - 55;
break;
case CMD_PANNINGSLIDE:
command = 'P' - 55;
break;
case CMD_RETRIG:
command = 'R' - 55;
break;
case CMD_TREMOR:
command = 'T' - 55;
break;
case CMD_DUMMY:
command = 'W' - 55;
break;
case CMD_XFINEPORTAUPDOWN:
command = 'X' - 55;
if(
compatibilityExport &&param
>= 0x30)    // X1x and X2x are legit, everything above are MPT extensions, which don't belong here.
param = 0;    // Don't set command to 0 to indicate that there *was* some X command here...
break;
case CMD_PANBRELLO:
if(compatibilityExport)
command = param = 0;
else
command = 'Y' - 55;
break;
case CMD_MIDI:
if(compatibilityExport)
command = param = 0;
else
command = 'Z' - 55;
break;
case CMD_SMOOTHMIDI: //rewbs.smoothVST: 36
if(compatibilityExport)
command = param = 0;
else
command = '\\' - 56;
break;
case CMD_XPARAM: //rewbs.XMfixes - XParam is 38
if(compatibilityExport)
command = param = 0;
else
command = '#' + 3;
break;
case CMD_S3MCMDEX:
switch(param & 0xF0)
{
case 0x10:
command = 0x0E;
param = (param & 0x0F) | 0x30;
break;
case 0x20:
command = 0x0E;
param = (param & 0x0F) | 0x50;
break;
case 0x30:
command = 0x0E;
param = (param & 0x0F) | 0x40;
break;
case 0x40:
command = 0x0E;
param = (param & 0x0F) | 0x70;
break;
case 0x90:
if(compatibilityExport)
command = param = 0;
else
command = 'X' - 55;
break;
case 0xB0:
command = 0x0E;
param = (param & 0x0F) | 0x60;
break;
case 0xA0:
case 0x50:
case 0x70:
case 0x60:
command = param = 0;
break;
default:
command = 0x0E;
break;
}
break;
default:
command = param = 0;
}
if(command > 0x0F && !toXM)
{
command = param = 0;
}
}
#endif  // MODPLUG_NO_FILESAVE
struct MODFileHeader {
uint8be numOrders;
uint8be restartPos;
uint8be orderList[128];
};
MPT_BINARY_STRUCT(MODFileHeader,
130)
struct MODSampleHeader {
char name[22];
uint16be length;
uint8be finetune;
uint8be volume;
uint16be loopStart;
uint16be loopLength;
void ConvertToMPT(ModSample &mptSmp, bool is4Chn) const {
mptSmp.Initialize(MOD_TYPE_MOD);
mptSmp.nLength = length * 2;
mptSmp.nFineTune = MOD2XMFineTune(finetune & 0x0F);
mptSmp.nVolume = 4u * std::min(volume.get(), uint8(64));
SmpLength lStart = loopStart * 2;
SmpLength lLength = loopLength * 2;
if(lLength > 2 && (lStart + lLength > mptSmp.nLength)
   && (lStart / 2 + lLength <= mptSmp.nLength)) {
lStart /= 2;
}
if(mptSmp.nLength == 2) {
mptSmp.nLength = 0;
}
if(mptSmp.nLength) {
mptSmp.nLoopStart = lStart;
mptSmp.nLoopEnd = lStart + lLength;
if(mptSmp.nLoopStart >= mptSmp.nLength) {
mptSmp.nLoopStart = mptSmp.nLength - 1;
}
if(mptSmp.nLoopStart > mptSmp.nLoopEnd || mptSmp.nLoopEnd < 4 || mptSmp.nLoopEnd - mptSmp.nLoopStart < 4) {
mptSmp.nLoopStart = 0;
mptSmp.nLoopEnd = 0;
}
// To be able to correctly play both modules, we will draw a somewhat arbitrary line here and trust the loop points in MODs with more than
if(mptSmp.nLoopEnd <= 8 && mptSmp.nLoopStart == 0 && mptSmp.nLength > mptSmp.nLoopEnd && is4Chn) {
mptSmp.nLoopEnd = 0;
}
if(mptSmp.nLoopEnd > mptSmp.nLoopStart) {
mptSmp.uFlags.set(CHN_LOOP);
}
}
}
SmpLength ConvertToMOD(const ModSample &mptSmp) {
SmpLength writeLength = mptSmp.HasSampleData() ? mptSmp.nLength : 0;
if((writeLength % 2u) != 0) {
writeLength++;
}
LimitMax(writeLength, SmpLength(0x1FFFE));
length = static_cast<uint16>(writeLength / 2u);
if(mptSmp.RelativeTone < 0) {
finetune = 0x08;
} else if(mptSmp.RelativeTone > 0) {
finetune = 0x07;
} else {
finetune = XM2MODFineTune(mptSmp.nFineTune);
}
volume = static_cast<uint8>(mptSmp.nVolume / 4u);
loopStart = 0;
loopLength = 1;
if(mptSmp.uFlags[CHN_LOOP] && (mptSmp.nLoopStart + 2u) < writeLength) {
const SmpLength loopEnd = Clamp(mptSmp.nLoopEnd, (mptSmp.nLoopStart & ~1) + 2u, writeLength) & ~1;
loopStart = static_cast<uint16>(mptSmp.nLoopStart / 2u);
loopLength = static_cast<uint16>((loopEnd - (mptSmp.nLoopStart & ~1)) / 2u);
}
return writeLength;
}
uint32 GetInvalidByteScore() const {
return ((volume > 64) ? 1 : 0)
       + ((finetune > 15) ? 1 : 0)
       + ((loopStart > length * 2) ? 1 : 0);
}
static constexpr uint32
INVALID_BYTE_THRESHOLD = 40;
// (https://www.pouet.net/prod.php?which=830) which have 3 \0 bytes in
static constexpr uint32
INVALID_BYTE_FRAGILE_THRESHOLD = 1;
static SampleIO GetSampleFormat() {
return SampleIO(
        SampleIO::_8bit,
        SampleIO::mono,
        SampleIO::bigEndian,
        SampleIO::signedPCM);
}
};
MPT_BINARY_STRUCT(MODSampleHeader,
30)
using MODPatternData = std::array<std::array<std::array < uint8, 4>, 4>, 64>;
struct AMInstrument {
char am[2];        // "AM"
char zero[4];
uint16be startLevel;   // Start level
uint16be attack1Level; // Attack 1 level
uint16be attack1Speed; // Attack 1 speed
uint16be attack2Level; // Attack 2 level
uint16be attack2Speed; // Attack 2 speed
uint16be sustainLevel; // Sustain level
uint16be decaySpeed;   // Decay speed
uint16be sustainTime;  // Sustain time
uint16be nt;           // ?
uint16be releaseSpeed; // Release speed
uint16be waveform;     // Waveform
int16be pitchFall;    // Pitch fall
uint16be vibAmp;       // Vibrato amplitude
uint16be vibSpeed;     // Vibrato speed
uint16be octave;       // Base frequency

void ConvertToMPT(ModSample &sample, ModInstrument &ins, mpt::fast_prng &rng) const {
sample.nLength = waveform == 3 ? 1024 : 32;
sample.nLoopStart = 0;
sample.nLoopEnd = sample.nLength;
sample.uFlags.set(CHN_LOOP);
sample.nVolume = 256;  // prelude.mod has volume 0 in sample header
sample.nVibDepth = mpt::saturate_cast<uint8>(vibAmp * 2);
sample.nVibRate = static_cast<uint8>(vibSpeed);
sample.nVibType = VIB_SINE;
sample.RelativeTone = static_cast<int8>(-12 * octave);
if(sample.AllocateSample()) {
int8 * p = sample.sample8();
for(SmpLength i = 0; i < sample.nLength; i++) {
switch (waveform) {
default:
case 0:
p[i] = ModSinusTable[i * 2];
break; // Sine
case 1:
p[i] = static_cast<int8>(-128 + i * 8);
break; // Saw
case 2:
p[i] = i < 16 ? -128 : 127;
break; // Square
case 3:
p[i] = mpt::random<int8>(rng);
break; // Noise
}
}
}
InstrumentEnvelope &volEnv = ins.VolEnv;
volEnv.dwFlags.set(ENV_ENABLED);
volEnv.reserve(6);
volEnv.push_back(0, static_cast<EnvelopeNode::value_t>(startLevel / 4));
const struct {
uint16 level, speed;
} points[] = {{startLevel,   0},
              {attack1Level, attack1Speed},
              {attack2Level, attack2Speed},
              {sustainLevel, decaySpeed},
              {sustainLevel, sustainTime},
              {0,            releaseSpeed}};
for(uint8 i = 1; i < std::size(points); i++) {
int duration = std::min(points[i].speed, uint16(256));
if(i != 4) {
if(duration == 0) {
volEnv.dwFlags.set(ENV_LOOP);
volEnv.nLoopStart = volEnv.nLoopEnd = static_cast<uint8>(volEnv.size() - 1);
break;
}
int a, b;
if(points[i].level > points[i - 1].level) {
a = points[i].level - points[i - 1].level;
b = 256 - points[i - 1].level;
} else {
a = points[i - 1].level - points[i].level;
b = points[i - 1].level;
}
if(i == 5)
b = 256;
else if(b == 0)
b = 1;
duration = std::max((256 * a) / (duration * b), 1);
}
if(duration > 0) {
volEnv.push_back(volEnv.back().tick + static_cast<EnvelopeNode::tick_t>(duration),
                 static_cast<EnvelopeNode::value_t>(points[i].level / 4));
}
}
if(pitchFall) {
InstrumentEnvelope &pitchEnv = ins.PitchEnv;
pitchEnv.dwFlags.set(ENV_ENABLED);
pitchEnv.reserve(2);
pitchEnv.push_back(0, ENVELOPE_MID);
pitchEnv.push_back(static_cast<EnvelopeNode::tick_t>(1024 / abs(pitchFall)),
                   pitchFall > 0 ? ENVELOPE_MIN : ENVELOPE_MAX);
}
}
};
MPT_BINARY_STRUCT(AMInstrument,
36)

struct PT36IffChunk {
enum ChunkIdentifiers {
idVERS = MagicBE("VERS"),
idINFO = MagicBE("INFO"),
idCMNT = MagicBE("CMNT"),
idPTDT = MagicBE("PTDT"),
};
uint32be signature;  // IFF chunk name
uint32be chunksize;  // chunk size without header
};
MPT_BINARY_STRUCT(PT36IffChunk,
8)

struct PT36InfoChunk {
char name[32];
uint16be numSamples;
uint16be numOrders;
uint16be numPatterns;
uint16be volume;
uint16be tempo;
uint16be flags;
uint16be dateDay;
uint16be dateMonth;
uint16be dateYear;
uint16be dateHour;
uint16be dateMinute;
uint16be dateSecond;
uint16be playtimeHour;
uint16be playtimeMinute;
uint16be playtimeSecond;
uint16be playtimeMsecond;
};
MPT_BINARY_STRUCT(PT36InfoChunk,
64)
static bool IsMagic(const char *magic1, const char (&magic2)[5]) {
return std::memcmp(magic1, magic2, 4) == 0;
}
static uint32
ReadSample(FileReader &file, MODSampleHeader &sampleHeader, ModSample &sample, mpt::charbuf<MAX_SAMPLENAME> &sampleName,
           bool is4Chn) {
file.ReadStruct(sampleHeader);
sampleHeader.ConvertToMPT(sample, is4Chn);
sampleName = mpt::String::ReadBuf(mpt::String::spacePadded, sampleHeader.name);
for(auto &c : sampleName.buf) {
if(c > 0 && c < ' ') {
c = ' ';
}
}
return sampleHeader.GetInvalidByteScore();
}
static uint32 CountMalformedMODPatternData(const MODPatternData &patternData, const bool allow31Samples) {
const uint8 mask = allow31Samples ? 0xE0 : 0xF0;
uint32 malformedBytes = 0;
for(const auto &row : patternData) {
for(const auto &data : row) {
if(data[0] & mask)
malformedBytes++;
}
}
return malformedBytes;
}
template<typename TFileReader>
static bool ValidateMODPatternData(TFileReader &file, const uint32 threshold, const bool allow31Samples) {
MODPatternData patternData;
if(!file.Read(patternData))
return false;
return CountMalformedMODPatternData(patternData, allow31Samples) <= threshold;
}
static PATTERNINDEX GetNumPatterns(FileReader &file, ModSequence &Order, ORDERINDEX numOrders, SmpLength totalSampleLen,
                                   CHANNELINDEX &numChannels, SmpLength wowSampleLen, bool validateHiddenPatterns) {
PATTERNINDEX numPatterns = 0;         // Total number of patterns in file (determined by going through the whole order list) with pattern number < 128
PATTERNINDEX officialPatterns = 0;    // Number of patterns only found in the "official" part of the order list (i.e. order positions < claimed order length)
PATTERNINDEX numPatternsIllegal = 0;  // Total number of patterns in file, also counting in "invalid" pattern indexes >= 128

for(ORDERINDEX ord = 0; ord < 128; ord++) {
PATTERNINDEX pat = Order[ord];
if(pat < 128 && numPatterns <= pat) {
numPatterns = pat + 1;
if(ord < numOrders) {
officialPatterns = numPatterns;
}
}
if(pat >= numPatternsIllegal) {
numPatternsIllegal = pat + 1;
}
}
Order.resize(numOrders);
const size_t patternStartOffset = file.GetPosition();
const size_t sizeWithoutPatterns = totalSampleLen + patternStartOffset;
if(wowSampleLen && (wowSampleLen + patternStartOffset) + numPatterns * 8 * 256 == (file.GetLength() & ~1)) {
file.Seek(patternStartOffset + numPatterns * 4 * 256);
if(ValidateMODPatternData(file, 16, true))
numChannels = 8;
file.Seek(patternStartOffset);
} else if(numPatterns != officialPatterns && validateHiddenPatterns) {
// seem to have the "correct" file size when only taking the "official" patterns into account,
// wolf3.mod (MD5 af60840815aa9eef43820a7a04417fa6, SHA1 24d6c2e38894f78f6c5c6a4b693a016af8fa037b)
// If that is the case, we assume it's part of the sample data and only consider the "official" patterns.
file.Seek(patternStartOffset + officialPatterns * 1024);
if(!ValidateMODPatternData(file, 64, true))
numPatterns = officialPatterns;
file.Seek(patternStartOffset);
}
#ifdef MPT_BUILD_DEBUG
// "killing butterfly" (MD5 bd676358b1dbb40d40f25435e845cf6b, SHA1 9df4ae21214ff753802756b616a0cafaeced8021),
    // See also the above check for ambiguities with SoundTracker modules.
    if(numPatterns != officialPatterns && sizeWithoutPatterns + officialPatterns * numChannels * 256 == file.GetLength())
    {
        MPT_ASSERT(false);
    } else
#endif
if(numPatternsIllegal > numPatterns &&
   sizeWithoutPatterns + numPatternsIllegal * numChannels * 256 == file.GetLength()) {
numPatterns = numPatternsIllegal;
} else if(numPatternsIllegal >= 0xFF) {
Order.Replace(0xFE, Order.GetIgnoreIndex());
Order.Replace(0xFF, Order.GetInvalidPatIndex());
}
return numPatterns;
}
void CSoundFile::ReadMODPatternEntry(FileReader &file, ModCommand &m) {
ReadMODPatternEntry(file.ReadArray<uint8, 4>(), m);
}
void CSoundFile::ReadMODPatternEntry(const std::array<uint8, 4> data, ModCommand &m) {
uint16 period = (((static_cast<uint16>(data[0]) & 0x0F) << 8) | data[1]);
size_t note = NOTE_NONE;
if(period > 0 && period != 0xFFF) {
note = std::size(ProTrackerPeriodTable) + 23 + NOTE_MIN;
for(size_t i = 0; i < std::size(ProTrackerPeriodTable); i++) {
if(period >= ProTrackerPeriodTable[i]) {
if(period != ProTrackerPeriodTable[i] && i != 0) {
uint16 p1 = ProTrackerPeriodTable[i - 1];
uint16 p2 = ProTrackerPeriodTable[i];
if(p1 - period < (period - p2)) {
note = i + 23 + NOTE_MIN;
break;
}
}
note = i + 24 + NOTE_MIN;
break;
}
}
}
m.note = static_cast<ModCommand::NOTE>(note);
m.instr = (data[2] >> 4) | (data[0] & 0x10);
m.command = data[2] & 0x0F;
m.param = data[3];
}
struct MODMagicResult {
const mpt::uchar *madeWithTracker = nullptr;
uint32 invalidByteThreshold = MODSampleHeader::INVALID_BYTE_THRESHOLD;
CHANNELINDEX numChannels = 0;
bool isNoiseTracker = false;
bool isStartrekker = false;
bool isGenericMultiChannel = false;
bool setMODVBlankTiming = false;
};
static bool CheckMODMagic(const char magic[4], MODMagicResult &result) {
if(IsMagic(magic, "M.K.")      // ProTracker and compatible
   || IsMagic(magic, "M!K!")   // ProTracker (>64 patterns)
   || IsMagic(magic, "PATT")   // ProTracker 3.6
   || IsMagic(magic, "NSMS")   // kingdomofpleasure.mod by bee hunter
   || IsMagic(magic, "LARD"))  // judgement_day_gvine.mod by 4-mat
{
result.madeWithTracker = UL_("Generic ProTracker or compatible");
result.numChannels = 4;
} else if(IsMagic(magic, "M&K!")     // "His Master's Noise" musicdisk
          || IsMagic(magic, "FEST")  // "His Master's Noise" musicdisk
          || IsMagic(magic, "N.T.")) {
result.madeWithTracker = UL_("NoiseTracker");
result.isNoiseTracker = true;
result.numChannels = 4;
} else if(IsMagic(magic, "OKTA")
          || IsMagic(magic, "OCTA")) {
result.madeWithTracker = UL_("Oktalyzer");
result.numChannels = 8;
} else if(IsMagic(magic, "CD81")
          || IsMagic(magic, "CD61")) {
result.madeWithTracker = UL_("Octalyser (Atari)");
result.numChannels = magic[2] - '0';
} else if(IsMagic(magic, "M\0\0\0") || IsMagic(magic, "8\0\0\0")) {
result.madeWithTracker = UL_("Inconexia demo (delta samples)");
result.invalidByteThreshold = MODSampleHeader::INVALID_BYTE_FRAGILE_THRESHOLD;
result.numChannels = (magic[0] == '8') ? 8 : 4;
} else if(!memcmp(magic, "FA0", 3) && magic[3] >= '4' && magic[3] <= '8') {
result.madeWithTracker = UL_("Digital Tracker");
result.numChannels = magic[3] - '0';
} else if((!memcmp(magic, "FLT", 3) || !memcmp(magic, "EXO", 3)) && magic[3] >= '4' && magic[3] <= '9') {
result.madeWithTracker = UL_("Startrekker");
result.isStartrekker = true;
result.setMODVBlankTiming = true;
result.numChannels = magic[3] - '0';
} else if(magic[0] >= '1' && magic[0] <= '9' && !memcmp(magic + 1, "CHN", 3)) {
result.madeWithTracker = UL_("Generic MOD-compatible Tracker");
result.isGenericMultiChannel = true;
result.numChannels = magic[0] - '0';
} else if(magic[0] >= '1' && magic[0] <= '9' && magic[1] >= '0' && magic[1] <= '9'
          && (!memcmp(magic + 2, "CH", 2) || !memcmp(magic + 2, "CN", 2))) {
result.madeWithTracker = UL_("Generic MOD-compatible Tracker");
result.isGenericMultiChannel = true;
result.numChannels = (magic[0] - '0') * 10 + magic[1] - '0';
} else if(!memcmp(magic, "TDZ", 3) && magic[3] >= '4' && magic[3] <= '9') {
result.madeWithTracker = UL_("TakeTracker");
result.numChannels = magic[3] - '0';
} else {
return false;
}
return true;
}
CSoundFile::ProbeResult CSoundFile::ProbeFileHeaderMOD(MemoryFileReader file, const uint64 *pfilesize) {
if(!file.CanRead(1080 + 4)) {
return ProbeWantMoreData;
}
file.Seek(1080);
char magic[4];
file.ReadArray(magic);
MODMagicResult modMagicResult;
if(!CheckMODMagic(magic, modMagicResult)) {
return ProbeFailure;
}
file.Seek(20);
uint32 invalidBytes = 0;
for(SAMPLEINDEX smp = 1; smp <= 31; smp++) {
MODSampleHeader sampleHeader;
file.ReadStruct(sampleHeader);
invalidBytes += sampleHeader.GetInvalidByteScore();
}
if(invalidBytes > modMagicResult.invalidByteThreshold) {
return ProbeFailure;
}
MPT_UNREFERENCED_PARAMETER(pfilesize);
return ProbeSuccess;
}
bool CSoundFile::ReadMOD(FileReader &file, ModLoadingFlags loadFlags) {
char magic[4];
if(!file.Seek(1080) || !file.ReadArray(magic)) {
return false;
}
InitializeGlobals(MOD_TYPE_MOD);
MODMagicResult modMagicResult;
if(!CheckMODMagic(magic, modMagicResult)
   || modMagicResult.numChannels < 1
   || modMagicResult.numChannels > MAX_BASECHANNELS) {
return false;
}
if(loadFlags == onlyVerifyHeader) {
return true;
}
m_nChannels = modMagicResult.numChannels;
bool isNoiseTracker = modMagicResult.isNoiseTracker;
bool isStartrekker = modMagicResult.isStartrekker;
bool isGenericMultiChannel = modMagicResult.isGenericMultiChannel;
bool isInconexia = IsMagic(magic, "M\0\0\0") || IsMagic(magic, "8\0\0\0");
bool hasRepLen0 = false;
bool hasEmptySampleWithVolume = false;
if(modMagicResult.setMODVBlankTiming) {
m_playBehaviour.set(kMODVBlankTiming);
}
const bool isFLT8 = isStartrekker && m_nChannels == 8;
const bool isMdKd = IsMagic(magic, "M.K.");
const bool isHMNT = IsMagic(magic, "M&K!") || IsMagic(magic, "FEST");
bool maybeWOW = isMdKd;
file.Seek(0);
file.ReadString<mpt::String::spacePadded>(m_songName, 20);
SmpLength totalSampleLen = 0, wowSampleLen = 0;
m_nSamples = 31;
uint32 invalidBytes = 0;
for(SAMPLEINDEX smp = 1; smp <= 31; smp++) {
MODSampleHeader sampleHeader;
invalidBytes += ReadSample(file, sampleHeader, Samples[smp], m_szNames[smp], m_nChannels == 4);
totalSampleLen += Samples[smp].nLength;
if(isHMNT)
Samples[smp].nFineTune = -static_cast<int8>(sampleHeader.finetune << 3);
else if(Samples[smp].nLength > 65535)
isNoiseTracker = false;
if(sampleHeader.length && !sampleHeader.loopLength)
hasRepLen0 = true;
else if(!sampleHeader.length && sampleHeader.volume == 64)
hasEmptySampleWithVolume = true;
if(maybeWOW) {
wowSampleLen += sampleHeader.length * 2;
if(sampleHeader.finetune)
maybeWOW = false;
else if(sampleHeader.length > 0 && sampleHeader.volume != 64)
maybeWOW = false;
}
}
if(invalidBytes > modMagicResult.invalidByteThreshold) {
return false;
}
MODFileHeader fileHeader;
file.ReadStruct(fileHeader);
file.Skip(4);  // Magic bytes (we already parsed these)

if(fileHeader.restartPos > 0)
maybeWOW = false;
if(!maybeWOW)
wowSampleLen = 0;
ReadOrderFromArray(Order(), fileHeader.orderList);
ORDERINDEX realOrders = fileHeader.numOrders;
if(realOrders > 128) {
realOrders = 128;
} else if(realOrders == 0) {
realOrders = 128;
while (realOrders > 1 && Order()[realOrders - 1] == 0) {
realOrders--;
}
}
PATTERNINDEX numPatterns = GetNumPatterns(file, Order(), realOrders, totalSampleLen, m_nChannels, wowSampleLen, false);
if(maybeWOW && GetNumChannels() == 8) {
modMagicResult.madeWithTracker = UL_("Mod's Grave");
isGenericMultiChannel = true;
}
if(isFLT8) {
for(auto &pat : Order()) {
pat /= 2u;
}
}
realOrders--;
Order().SetRestartPos(fileHeader.restartPos);
// M.K. files that have restart pos == 0x78: action's batman by DJ Uno, VALLEY.MOD, WormsTDC.MOD, ZWARTZ.MOD
MPT_ASSERT(fileHeader.restartPos != 0x78 || fileHeader.restartPos + 1u >= realOrders);
if(fileHeader.restartPos > realOrders || (fileHeader.restartPos == 0x78 && m_nChannels == 4)) {
Order().SetRestartPos(0);
}
m_nDefaultSpeed = 6;
m_nDefaultTempo.Set(125);
m_nMinPeriod = 14 * 4;
m_nMaxPeriod = 3424 * 4;
m_nSamplePreAmp = Clamp(256 / m_nChannels, 32, 128);
m_SongFlags.reset();  // SONG_ISAMIGA will be set conditionally
SetupMODPanning();
// - Use the same code to find notes that would be out-of-range on Amiga.
bool onlyAmigaNotes = true;
bool fix7BitPanning = false;
uint8 maxPanning = 0;  // For detecting 8xx-as-sync
const uint8 ENABLE_MOD_PANNING_THRESHOLD = 0x30;
if(!isNoiseTracker) {
bool leftPanning = false, extendedPanning = false;  // For detecting 800-880 panning
isNoiseTracker = isMdKd;
for(PATTERNINDEX pat = 0; pat < numPatterns; pat++) {
uint16 patternBreaks = 0;
for(uint32 i = 0; i < 256; i++) {
ModCommand m;
ReadMODPatternEntry(file, m);
if(!m.IsAmigaNote()) {
isNoiseTracker = onlyAmigaNotes = false;
}
if((m.command > 0x06 && m.command < 0x0A)
   || (m.command == 0x0E && m.param > 0x01)
   || (m.command == 0x0F && m.param > 0x1F)
   || (m.command == 0x0D && ++patternBreaks > 1)) {
isNoiseTracker = false;
}
if(m.command == 0x08) {
maxPanning = std::max(maxPanning, m.param);
if(m.param < 0x80)
leftPanning = true;
else if(m.param > 0x8F && m.param != 0xA4)
extendedPanning = true;
} else if(m.command == 0x0E && (m.param & 0xF0) == 0x80) {
maxPanning = std::max(maxPanning, static_cast<uint8>((m.param & 0x0F) << 4));
}
}
}
fix7BitPanning = leftPanning && !extendedPanning && maxPanning >= ENABLE_MOD_PANNING_THRESHOLD;
}
file.Seek(1084);
const CHANNELINDEX readChannels = (isFLT8 ? 4 : m_nChannels); // 4 channels per pattern in FLT8 format.
if(isFLT8) numPatterns++; // as one logical pattern consists of two real patterns in FLT8 format, the highest pattern number has to be increased by one.
bool hasTempoCommands = false, definitelyCIA = false;    // for detecting VBlank MODs
bool filterState = false;
int filterTransitions = 0;
Patterns.ResizeArray(numPatterns);
for(PATTERNINDEX pat = 0; pat < numPatterns; pat++) {
ModCommand *rowBase = nullptr;
if(isFLT8) {
PATTERNINDEX actualPattern = pat / 2u;
if((pat % 2u) == 0 && !Patterns.Insert(actualPattern, 64)) {
break;
}
rowBase = Patterns[actualPattern].GetpModCommand(0, (pat % 2u) == 0 ? 0 : 4);
} else {
if(!Patterns.Insert(pat, 64)) {
break;
}
rowBase = Patterns[pat].GetpModCommand(0, 0);
}
if(rowBase == nullptr || !(loadFlags & loadPatternData)) {
break;
}
std::vector <ModCommand::INSTR> lastInstrument(GetNumChannels(), 0);
std::vector <uint8> instrWithoutNoteCount(GetNumChannels(), 0);
for(ROWINDEX row = 0; row < 64; row++, rowBase += m_nChannels) {
bool hasSpeedOnRow = false, hasTempoOnRow = false;
for(CHANNELINDEX chn = 0; chn < readChannels; chn++) {
ModCommand &m = rowBase[chn];
ReadMODPatternEntry(file, m);
if(m.command || m.param) {
if(isStartrekker && m.command == 0x0E) {
m.command = CMD_NONE;
m.param = 0;
} else if(isStartrekker && m.command == 0x0F && m.param > 0x1F) {
m.param = 0x1F;
}
ConvertModCommand(m);
}
if(m.command == CMD_TEMPO) {
hasTempoOnRow = true;
if(m.param < 100)
hasTempoCommands = true;
} else if(m.command == CMD_SPEED) {
hasSpeedOnRow = true;
} else if(m.command == CMD_PATTERNBREAK && isNoiseTracker) {
m.param = 0;
} else if(m.command == CMD_PANNING8 && fix7BitPanning) {
if(m.param == 0xA4) {
m.command = CMD_S3MCMDEX;
m.param = 0x91;
} else {
m.param = mpt::saturate_cast<ModCommand::PARAM>(m.param * 2);
}
} else if(m.command == CMD_MODCMDEX && m.param < 0x10) {
bool newState = !(m.param & 0x01);
if(newState != filterState) {
filterState = newState;
filterTransitions++;
}
}
if(m.note == NOTE_NONE && m.instr > 0 && !isFLT8) {
if(lastInstrument[chn] > 0 && lastInstrument[chn] != m.instr) {
if(++instrWithoutNoteCount[chn] >= 4) {
m_playBehaviour.set(kMODSampleSwap);
}
}
} else if(m.note != NOTE_NONE) {
instrWithoutNoteCount[chn] = 0;
}
if(m.instr != 0) {
lastInstrument[chn] = m.instr;
}
}
if(hasSpeedOnRow && hasTempoOnRow)
definitelyCIA = true;
}
}
if(onlyAmigaNotes && !hasRepLen0 && (IsMagic(magic, "M.K.") || IsMagic(magic, "M!K!") || IsMagic(magic, "PATT"))) {
m_SongFlags.set(SONG_AMIGALIMITS);
m_SongFlags.set(SONG_PT_MODE);
m_playBehaviour.set(kMODSampleSwap);
m_playBehaviour.set(kMODOutOfRangeNoteDelay);
m_playBehaviour.set(kMODTempoOnSecondTick);
if(maxPanning < ENABLE_MOD_PANNING_THRESHOLD) {
m_playBehaviour.set(kMODIgnorePanning);
if(fileHeader.restartPos != 0x7F) {
m_playBehaviour.set(kMODOneShotLoops);
}
}
} else if(!onlyAmigaNotes && fileHeader.restartPos == 0x7F && isMdKd && fileHeader.restartPos + 1u >= realOrders) {
modMagicResult.madeWithTracker = UL_("ScreamTracker");
}
if(onlyAmigaNotes && !isGenericMultiChannel && filterTransitions < 7) {
m_SongFlags.set(SONG_ISAMIGA);
}
if(isGenericMultiChannel || isMdKd) {
m_playBehaviour.set(kFT2MODTremoloRampWaveform);
}
if(isInconexia) {
m_playBehaviour.set(kMODIgnorePanning);
}
if(loadFlags & loadSampleData) {
file.Seek(1084 + (readChannels * 64 * 4) * numPatterns);
for(SAMPLEINDEX smp = 1; smp <= 31; smp++) {
ModSample &sample = Samples[smp];
if(sample.nLength) {
SampleIO::Encoding encoding = SampleIO::signedPCM;
if(isInconexia)
encoding = SampleIO::deltaPCM;
else if(file.ReadMagic("ADPCM"))
encoding = SampleIO::ADPCM;
SampleIO sampleIO(
        SampleIO::_8bit,
        SampleIO::mono,
        SampleIO::littleEndian,
        encoding);
// On the other hand, the loop points in Purple Motions's SOUL-O-M.MOD are completely broken and shouldn't be treated like this.
FileReader::off_t nextSample = file.GetPosition() + sampleIO.CalculateEncodedSize(sample.nLength);
if(isMdKd && onlyAmigaNotes && !hasEmptySampleWithVolume)
sample.nLength = std::max(sample.nLength, sample.nLoopEnd);
sampleIO.ReadSample(sample, file);
file.Seek(nextSample);
}
}
}
#if defined(MPT_EXTERNAL_SAMPLES) || defined(MPT_BUILD_FUZZER)
if((loadFlags & loadSampleData) && isStartrekker)
    {
#ifdef MPT_EXTERNAL_SAMPLES
        std::optional<InputFile> amFile;
        FileReader amData;
        if(file.GetOptionalFileName())
        {
            mpt::PathString filename = file.GetOptionalFileName().value();
            const mpt::PathString exts[] = {P_(".nt"), P_(".NT"), P_(".as"), P_(".AS")};
            for(const auto &ext : exts)
            {
                mpt::PathString infoName = filename + ext;
                char stMagic[16];
                if(infoName.IsFile())
                {
                    amFile.emplace(infoName, SettingCacheCompleteFileBeforeLoading());
                    if(amFile->IsValid() && (amData = GetFileReader(*amFile)).IsValid() && amData.ReadArray(stMagic))
                    {
                        if(!memcmp(stMagic, "ST1.2 ModuleINFO", 16))
                            modMagicResult.madeWithTracker = UL_("Startrekker 1.2");
                        else if(!memcmp(stMagic, "ST1.3 ModuleINFO", 16))
                            modMagicResult.madeWithTracker = UL_("Startrekker 1.3");
                        else if(!memcmp(stMagic, "AudioSculpture10", 16))
                            modMagicResult.madeWithTracker = UL_("AudioSculpture 1.0");
                        else
                            continue;

                        if(amData.Seek(144))
                        {
                            m_nInstruments = 31;
                            break;
                        }
                    }
                }
            }
        }
#elif defined(MPT_BUILD_FUZZER)
        FileReader amData = file.GetChunkAt(1084, 31 * 120);
        m_nInstruments = 31;
#endif

        for(SAMPLEINDEX smp = 1; smp <= m_nInstruments; smp++)
        {
            ModInstrument *ins = AllocateInstrument(smp, smp);
            if(ins == nullptr)
            {
                break;
            }
            ins->name = m_szNames[smp];

            AMInstrument am;
            if(amData.ReadStructPartial(am) && !memcmp(am.am, "AM", 2) && am.waveform < 4)
            {
                am.ConvertToMPT(Samples[smp], *ins, AccessPRNG());
            }
            amData.Skip(120 - sizeof(AMInstrument));
        }
    }
#endif  // MPT_EXTERNAL_SAMPLES || MPT_BUILD_FUZZER
// There is no perfect way to do this, since both MOD types look the same,
// In the pattern loader above, a second condition is used: Only tempo commands
if(isMdKd && hasTempoCommands && !definitelyCIA) {
const double songTime = GetLength(eNoAdjust).front().duration;
if(songTime >= 600.0) {
m_playBehaviour.set(kMODVBlankTiming);
if(GetLength(eNoAdjust, GetLengthTarget(songTime)).front().targetReached) {
m_playBehaviour.reset(kMODVBlankTiming);
} else {
modMagicResult.madeWithTracker = UL_("ProTracker (VBlank)");
}
}
}
std::transform(std::begin(magic), std::end(magic), std::begin(magic),
               [](unsigned char c) -> unsigned char { return (c < ' ') ? ' ' : c; });
m_modFormat.formatName = MPT_UFORMAT("ProTracker MOD ({})")(
        mpt::ToUnicode(mpt::Charset::ASCII, std::string(std::begin(magic), std::end(magic))));
m_modFormat.type = U_("mod");
if(modMagicResult.madeWithTracker)
m_modFormat.madeWithTracker = modMagicResult.madeWithTracker;
m_modFormat.charset = mpt::Charset::ISO8859_1;
return true;
}
template<size_t N>
static uint32 CountInvalidChars(const char (&name)[N]) {
uint32 invalidChars = 0;
for(int8 c : name)  // char can be signed or unsigned
{
if(c != 0 && c < ' ')
invalidChars++;
}
return invalidChars;
}
enum STVersions {
UST1_00,              // Ultimate Soundtracker 1.0-1.21 (K. Obarski)
UST1_80,              // Ultimate Soundtracker 1.8-2.0 (K. Obarski)
ST2_00_Exterminator,  // SoundTracker 2.0 (The Exterminator), D.O.C. Sountracker II (Unknown/D.O.C.)
ST_III,               // Defjam Soundtracker III (Il Scuro/Defjam), Alpha Flight SoundTracker IV (Alpha Flight), D.O.C. SoundTracker IV (Unknown/D.O.C.), D.O.C. SoundTracker VI (Unknown/D.O.C.)
ST_IX,                // D.O.C. SoundTracker IX (Unknown/D.O.C.)
MST1_00,              // Master Soundtracker 1.0 (Tip/The New Masters)
ST2_00,               // SoundTracker 2.0, 2.1, 2.2 (Unknown/D.O.C.)
};
struct M15FileHeaders {
char songname[20];
MODSampleHeader sampleHeaders[15];
MODFileHeader fileHeader;
};
MPT_BINARY_STRUCT(M15FileHeaders,
20 + 15 * 30 + 130)
static bool ValidateHeader(const M15FileHeaders &fileHeaders) {
// files with *too* many bogus characters. Arbitrary threshold: 48 bogus characters in total
uint32 invalidChars = CountInvalidChars(fileHeaders.songname);
if(invalidChars > 5) {
return false;
}
SmpLength totalSampleLen = 0;
uint8 allVolumes = 0;
for(SAMPLEINDEX smp = 0; smp < 15; smp++) {
const MODSampleHeader &sampleHeader = fileHeaders.sampleHeaders[smp];
invalidChars += CountInvalidChars(sampleHeader.name);
if(invalidChars > 48
   || sampleHeader.volume > 64
   || sampleHeader.finetune != 0
   || sampleHeader.length > 32768) {
return false;
}
totalSampleLen += sampleHeader.length;
allVolumes |= sampleHeader.volume;
}
if(totalSampleLen == 0 || allVolumes == 0) {
return false;
}
if(fileHeaders.fileHeader.numOrders > 128 || fileHeaders.fileHeader.restartPos > 220) {
return false;
}
uint8 maxPattern = *std::max_element(std::begin(fileHeaders.fileHeader.orderList),
                                     std::end(fileHeaders.fileHeader.orderList));
if(maxPattern > 63) {
return false;
}
if(fileHeaders.fileHeader.restartPos == 0 && fileHeaders.fileHeader.numOrders == 0 && maxPattern == 0) {
return false;
}
return true;
}
template<typename TFileReader>
static bool ValidateFirstM15Pattern(TFileReader &file) {
return ValidateMODPatternData(file, 512 / 64 * 2, false);
}
CSoundFile::ProbeResult CSoundFile::ProbeFileHeaderM15(MemoryFileReader file, const uint64 *pfilesize) {
M15FileHeaders fileHeaders;
if(!file.ReadStruct(fileHeaders)) {
return ProbeWantMoreData;
}
if(!ValidateHeader(fileHeaders)) {
return ProbeFailure;
}
if(!file.CanRead(sizeof(MODPatternData))) {
return ProbeWantMoreData;
}
if(!ValidateFirstM15Pattern(file)) {
return ProbeFailure;
}
MPT_UNREFERENCED_PARAMETER(pfilesize);
return ProbeSuccess;
}
bool CSoundFile::ReadM15(FileReader &file, ModLoadingFlags loadFlags) {
file.Rewind();
M15FileHeaders fileHeaders;
if(!file.ReadStruct(fileHeaders)) {
return false;
}
if(!ValidateHeader(fileHeaders)) {
return false;
}
if(!ValidateFirstM15Pattern(file)) {
return false;
}
char songname[20];
std::memcpy(songname, fileHeaders.songname, 20);
InitializeGlobals(MOD_TYPE_MOD);
m_playBehaviour.reset(kMODOneShotLoops);
m_playBehaviour.set(kMODIgnorePanning);
m_playBehaviour.set(kMODSampleSwap);  // untested
m_nChannels = 4;
STVersions minVersion = UST1_00;
bool hasDiskNames = true;
SmpLength totalSampleLen = 0;
m_nSamples = 15;
file.Seek(20);
for(SAMPLEINDEX smp = 1; smp <= 15; smp++) {
MODSampleHeader sampleHeader;
ReadSample(file, sampleHeader, Samples[smp], m_szNames[smp], true);
totalSampleLen += Samples[smp].nLength;
if(m_szNames[smp][0] &&
   ((memcmp(m_szNames[smp].buf, "st-", 3) && memcmp(m_szNames[smp].buf, "ST-", 3)) || m_szNames[smp][5] != ':')) {
hasDiskNames = false;
}
if(sampleHeader.loopLength > 1) {
Samples[smp].nLoopStart = sampleHeader.loopStart;
Samples[smp].nLoopEnd = sampleHeader.loopStart + sampleHeader.loopLength * 2;
Samples[smp].SanitizeLoops();
}
if(sampleHeader.length > 4999 || sampleHeader.loopStart > 9999)
minVersion = std::max(minVersion, MST1_00);
}
MODFileHeader fileHeader;
file.ReadStruct(fileHeader);
ReadOrderFromArray(Order(), fileHeader.orderList);
PATTERNINDEX numPatterns = GetNumPatterns(file, Order(), fileHeader.numOrders, totalSampleLen, m_nChannels, 0, true);
if(fileHeader.restartPos == 0 && fileHeader.numOrders == 0 && numPatterns <= 1) {
return false;
}
if(file.BytesLeft() + 65536 < numPatterns * 64u * 4u * 4u + totalSampleLen)
return false;
if(loadFlags == onlyVerifyHeader)
return true;
if(!fileHeader.restartPos)
fileHeader.restartPos = 0x78;
if(!memcmp(songname, "jjk55", 6))
fileHeader.restartPos = 0x78;
m_nDefaultTempo.Set(125);
if(fileHeader.restartPos != 0x78) {
m_nDefaultTempo = TEMPO((709379.0 * 125.0 / 50.0) / ((240 - fileHeader.restartPos) * 122.0));
if(minVersion > UST1_80) {
minVersion = std::max(minVersion, hasDiskNames ? ST_IX : MST1_00);
} else {
minVersion = std::max(minVersion, hasDiskNames ? UST1_80 : ST2_00_Exterminator);
}
}
m_nMinPeriod = 113 * 4;
m_nMaxPeriod = 856 * 4;
m_nSamplePreAmp = 64;
m_SongFlags.set(SONG_PT_MODE);
m_songName = mpt::String::ReadBuf(mpt::String::spacePadded, songname);
SetupMODPanning();
FileReader::off_t patOffset = file.GetPosition();
uint32 illegalBytes = 0, totalNumDxx = 0;
for(PATTERNINDEX pat = 0; pat < numPatterns; pat++) {
const bool patternInUse = mpt::contains(Order(), pat);
uint8 numDxx = 0;
uint8 emptyCmds = 0;
MODPatternData patternData;
file.ReadArray(patternData);
if(patternInUse) {
illegalBytes += CountMalformedMODPatternData(patternData, false);
// This also allows to play some rather damaged files like
// "operation wolf" soundtrack have 15 patterns for several songs, but the last few patterns are just garbage.
if(illegalBytes > 512)
return false;
}
for(ROWINDEX row = 0; row < 64; row++) {
for(CHANNELINDEX chn = 0; chn < 4; chn++) {
const auto &data = patternData[row][chn];
const uint8 eff = data[2] & 0x0F, param = data[3];
if(emptyCmds != 0 && !memcmp(data.data(), "\0\0\0\0", 4)) {
emptyCmds++;
if(emptyCmds > 32) {
minVersion = ST2_00;
}
} else {
emptyCmds = 0;
}
switch (eff) {
case 1:
case 2:
if(param > 0x1F && minVersion == UST1_80) {
minVersion = hasDiskNames ? UST1_80 : UST1_00;
} else if(eff == 1 && param > 0 && param < 0x03) {
minVersion = std::max(minVersion, ST2_00_Exterminator);
} else if(eff == 1 && (param == 0x37 || param == 0x47) && minVersion <= ST2_00_Exterminator) {
minVersion = hasDiskNames ? UST1_80 : UST1_00;
}
break;
case 0x0B:
minVersion = ST2_00;
break;
case 0x0C:
case 0x0D:
case 0x0E:
minVersion = std::max(minVersion, ST2_00_Exterminator);
if(eff == 0x0D) {
emptyCmds = 1;
if(param == 0 && row == 0) {
break;
}
numDxx++;
}
break;
case 0x0F:
minVersion = std::max(minVersion, ST_III);
break;
}
}
}
if(numDxx > 0 && numDxx < 3) {
minVersion = ST2_00;
}
totalNumDxx += numDxx;
}
if(totalNumDxx > numPatterns + 32u && minVersion == ST2_00)
minVersion = MST1_00;
file.Seek(patOffset);
if(loadFlags & loadPatternData)
Patterns.ResizeArray(numPatterns);
for(PATTERNINDEX pat = 0; pat < numPatterns; pat++) {
MODPatternData patternData;
file.ReadArray(patternData);
if(!(loadFlags & loadPatternData) || !Patterns.Insert(pat, 64)) {
continue;
}
uint8 autoSlide[4] = {0, 0, 0, 0};
for(ROWINDEX row = 0; row < 64; row++) {
PatternRow rowBase = Patterns[pat].GetpModCommand(row, 0);
for(CHANNELINDEX chn = 0; chn < 4; chn++) {
ModCommand &m = rowBase[chn];
ReadMODPatternEntry(patternData[row][chn], m);
if(!m.param || m.command == 0x0E) {
autoSlide[chn] = 0;
}
if(m.command || m.param) {
if(autoSlide[chn] != 0) {
if(autoSlide[chn] & 0xF0) {
m.volcmd = VOLCMD_VOLSLIDEUP;
m.vol = autoSlide[chn] >> 4;
} else {
m.volcmd = VOLCMD_VOLSLIDEDOWN;
m.vol = autoSlide[chn] & 0x0F;
}
}
if(m.command == 0x0D) {
if(minVersion != ST2_00) {
m.command = 0x0A;
} else {
m.param = 0;
}
} else if(m.command == 0x0C) {
m.param &= 0x7F;
} else if(m.command == 0x0E && (m.param > 0x01 || minVersion < ST_IX)) {
m.command = 0x0A;
autoSlide[chn] = m.param;
} else if(m.command == 0x0F) {
m.param &= 0x0F;
}
if(minVersion <= UST1_80) {
switch (m.command) {
case 0:
if(m.param < 0x03) {
m.command = CMD_NONE;
} else {
m.command = CMD_ARPEGGIO;
}
break;
case 1:
m.command = CMD_ARPEGGIO;
break;
case 2:
if(m.param & 0x0F) {
m.command = CMD_PORTAMENTOUP;
m.param &= 0x0F;
} else if(m.param >> 4) {
m.command = CMD_PORTAMENTODOWN;
m.param >>= 4;
}
break;
default:
m.command = CMD_NONE;
break;
}
} else {
ConvertModCommand(m);
}
} else {
autoSlide[chn] = 0;
}
}
}
}
const mpt::uchar *madeWithTracker = UL_("");
switch (minVersion) {
case UST1_00:
madeWithTracker = UL_("Ultimate Soundtracker 1.0-1.21");
break;
case UST1_80:
madeWithTracker = UL_("Ultimate Soundtracker 1.8-2.0");
break;
case ST2_00_Exterminator:
madeWithTracker = UL_("SoundTracker 2.0 / D.O.C. SoundTracker II");
break;
case ST_III:
madeWithTracker = UL_("Defjam Soundtracker III / Alpha Flight SoundTracker IV / D.O.C. SoundTracker IV / VI");
break;
case ST_IX:
madeWithTracker = UL_("D.O.C. SoundTracker IX");
break;
case MST1_00:
madeWithTracker = UL_("Master Soundtracker 1.0");
break;
case ST2_00:
madeWithTracker = UL_("SoundTracker 2.0 / 2.1 / 2.2");
break;
}
m_modFormat.formatName = U_("Soundtracker");
m_modFormat.type = U_("stk");
m_modFormat.madeWithTracker = madeWithTracker;
m_modFormat.charset = mpt::Charset::ISO8859_1;
if(loadFlags & loadSampleData) {
for(SAMPLEINDEX smp = 1; smp <= 15; smp++) {
file.Skip(Samples[smp].nLoopStart);
Samples[smp].nLength -= Samples[smp].nLoopStart;
Samples[smp].nLoopEnd -= Samples[smp].nLoopStart;
Samples[smp].nLoopStart = 0;
MODSampleHeader::GetSampleFormat().ReadSample(Samples[smp], file);
}
}
return true;
}
CSoundFile::ProbeResult CSoundFile::ProbeFileHeaderICE(MemoryFileReader file, const uint64 *pfilesize) {
if(!file.CanRead(1464 + 4)) {
return ProbeWantMoreData;
}
file.Seek(1464);
char magic[4];
file.ReadArray(magic);
if(!IsMagic(magic, "MTN\0") && !IsMagic(magic, "IT10")) {
return ProbeFailure;
}
file.Seek(20);
uint32 invalidBytes = 0;
for(SAMPLEINDEX smp = 1; smp <= 31; smp++) {
MODSampleHeader sampleHeader;
if(!file.ReadStruct(sampleHeader)) {
return ProbeWantMoreData;
}
invalidBytes += sampleHeader.GetInvalidByteScore();
}
if(invalidBytes > MODSampleHeader::INVALID_BYTE_THRESHOLD) {
return ProbeFailure;
}
const auto[numOrders, numTracks] = file.ReadArray<uint8, 2>();
if(numOrders > 128) {
return ProbeFailure;
}
uint8 tracks[128 * 4];
file.ReadArray(tracks);
for(auto track : tracks) {
if(track > numTracks) {
return ProbeFailure;
}
}
MPT_UNREFERENCED_PARAMETER(pfilesize);
return ProbeSuccess;
}
bool CSoundFile::ReadICE(FileReader &file, ModLoadingFlags loadFlags) {
char magic[4];
if(!file.Seek(1464) || !file.ReadArray(magic)) {
return false;
}
InitializeGlobals(MOD_TYPE_MOD);
m_playBehaviour.reset(kMODOneShotLoops);
m_playBehaviour.set(kMODIgnorePanning);
m_playBehaviour.set(kMODSampleSwap);  // untested

if(IsMagic(magic, "MTN\0")) {
m_modFormat.formatName = U_("MnemoTroN SoundTracker");
m_modFormat.type = U_("st26");
m_modFormat.madeWithTracker = U_("SoundTracker 2.6");
m_modFormat.charset = mpt::Charset::ISO8859_1;
} else if(IsMagic(magic, "IT10")) {
m_modFormat.formatName = U_("Ice Tracker");
m_modFormat.type = U_("ice");
m_modFormat.madeWithTracker = U_("Ice Tracker 1.0 / 1.1");
m_modFormat.charset = mpt::Charset::ISO8859_1;
} else {
return false;
}
file.Seek(0);
file.ReadString<mpt::String::spacePadded>(m_songName, 20);
m_nSamples = 31;
uint32 invalidBytes = 0;
for(SAMPLEINDEX smp = 1; smp <= 31; smp++) {
MODSampleHeader sampleHeader;
invalidBytes += ReadSample(file, sampleHeader, Samples[smp], m_szNames[smp], true);
}
if(invalidBytes > MODSampleHeader::INVALID_BYTE_THRESHOLD) {
return false;
}
const auto[numOrders, numTracks] = file.ReadArray<uint8, 2>();
if(numOrders > 128) {
return false;
}
uint8 tracks[128 * 4];
file.ReadArray(tracks);
for(auto track : tracks) {
if(track > numTracks) {
return false;
}
}
if(loadFlags == onlyVerifyHeader) {
return true;
}
m_nChannels = 4;
m_nInstruments = 0;
m_nDefaultSpeed = 6;
m_nDefaultTempo.Set(125);
m_nMinPeriod = 14 * 4;
m_nMaxPeriod = 3424 * 4;
m_nSamplePreAmp = 64;
m_SongFlags.set(SONG_PT_MODE | SONG_IMPORTED);
SetupMODPanning();
Order().resize(numOrders);
uint8 speed[2] = {0, 0}, speedPos = 0;
Patterns.ResizeArray(numOrders);
for(PATTERNINDEX pat = 0; pat < numOrders; pat++) {
Order()[pat] = pat;
if(!Patterns.Insert(pat, 64))
continue;
for(CHANNELINDEX chn = 0; chn < 4; chn++) {
file.Seek(1468 + tracks[pat * 4 + chn] * 64u * 4u);
ModCommand *m = Patterns[pat].GetpModCommand(0, chn);
for(ROWINDEX row = 0; row < 64; row++, m += 4) {
ReadMODPatternEntry(file, *m);
if((m->command || m->param)
   && !(m->command == 0x0E && m->param >= 0x10)     // Exx only sets filter
   && !(m->command >= 0x05 && m->command <= 0x09))  // These don't exist in ST2.6
{
ConvertModCommand(*m);
} else {
m->command = CMD_NONE;
}
}
}
auto m = Patterns[pat].begin();
for(ROWINDEX row = 0; row < 64; row++) {
for(CHANNELINDEX chn = 0; chn < 4; chn++, m++) {
if(m->command == CMD_SPEED || m->command == CMD_TEMPO) {
m->command = CMD_SPEED;
speedPos = 0;
if(m->param & 0xF0) {
if((m->param >> 4) != (m->param & 0x0F) && (m->param & 0x0F) != 0) {
speed[0] = m->param >> 4;
speed[1] = m->param & 0x0F;
speedPos = 1;
}
m->param >>= 4;
}
}
}
if(speedPos) {
Patterns[pat].WriteEffect(EffectWriter(CMD_SPEED, speed[speedPos - 1]).Row(row));
speedPos++;
if(speedPos == 3)
speedPos = 1;
}
}
}
if(loadFlags & loadSampleData) {
file.Seek(1468 + numTracks * 64u * 4u);
for(SAMPLEINDEX smp = 1; smp <= 31; smp++)
if(Samples[smp].nLength) {
SampleIO(
        SampleIO::_8bit,
        SampleIO::mono,
        SampleIO::littleEndian,
        SampleIO::signedPCM)
        .ReadSample(Samples[smp], file);
}
}
return true;
}
struct PT36Header {
char magicFORM[4]; // "FORM"
uint32be size;
char magicMODL[4]; // "MODL"
};
MPT_BINARY_STRUCT(PT36Header,
12)
static bool ValidateHeader(const PT36Header &fileHeader) {
if(std::memcmp(fileHeader.magicFORM, "FORM", 4)) {
return false;
}
if(std::memcmp(fileHeader.magicMODL, "MODL", 4)) {
return false;
}
return true;
}
CSoundFile::ProbeResult CSoundFile::ProbeFileHeaderPT36(MemoryFileReader file, const uint64 *pfilesize) {
PT36Header fileHeader;
if(!file.ReadStruct(fileHeader)) {
return ProbeWantMoreData;
}
if(!ValidateHeader(fileHeader)) {
return ProbeFailure;
}
MPT_UNREFERENCED_PARAMETER(pfilesize);
return ProbeSuccess;
}
bool CSoundFile::ReadPT36(FileReader &file, ModLoadingFlags loadFlags) {
file.Rewind();
PT36Header fileHeader;
if(!file.ReadStruct(fileHeader)) {
return false;
}
if(!ValidateHeader(fileHeader)) {
return false;
}
bool ok = false, infoOk = false;
FileReader commentChunk;
mpt::ustring version;
PT36InfoChunk info;
MemsetZero(info);
PT36IffChunk iffHead;
if(!file.ReadStruct(iffHead)) {
return false;
}
iffHead.chunksize -= 4;
do {
iffHead.chunksize -= 8;
if(loadFlags == onlyVerifyHeader && iffHead.signature == PT36IffChunk::idPTDT) {
return true;
}
FileReader chunk = file.ReadChunk(iffHead.chunksize);
if(!chunk.IsValid()) {
break;
}
switch (iffHead.signature) {
case PT36IffChunk::idVERS:
chunk.Skip(4);
if(chunk.ReadMagic("PT") && iffHead.chunksize > 6) {
chunk.ReadString<mpt::String::maybeNullTerminated>(version, mpt::Charset::ISO8859_1, iffHead.chunksize - 6);
}
break;
case PT36IffChunk::idINFO:
infoOk = chunk.ReadStruct(info);
break;
case PT36IffChunk::idCMNT:
commentChunk = chunk;
break;
case PT36IffChunk::idPTDT:
ok = ReadMOD(chunk, loadFlags);
break;
}
} while (file.ReadStruct(iffHead));
if(version.empty()) {
version = U_("3.6");
}
if(ok && infoOk) {
bool vblank = (info.flags & 0x100) == 0;
m_playBehaviour.set(kMODVBlankTiming, vblank);
if(info.volume != 0)
m_nSamplePreAmp = std::min(uint16(64), static_cast<uint16>(info.volume));
if(info.tempo != 0 && !vblank)
m_nDefaultTempo.Set(info.tempo);
if(info.name[0])
m_songName = mpt::String::ReadBuf(mpt::String::maybeNullTerminated, info.name);
if(mpt::is_in_range(info.dateMonth, 1, 12) && mpt::is_in_range(info.dateDay, 1, 31) &&
   mpt::is_in_range(info.dateHour, 0, 23)
   && mpt::is_in_range(info.dateMinute, 0, 59) && mpt::is_in_range(info.dateSecond, 0, 59)) {
FileHistory mptHistory;
mptHistory.loadDate.tm_year = info.dateYear;
mptHistory.loadDate.tm_mon = info.dateMonth - 1;
mptHistory.loadDate.tm_mday = info.dateDay;
mptHistory.loadDate.tm_hour = info.dateHour;
mptHistory.loadDate.tm_min = info.dateMinute;
mptHistory.loadDate.tm_sec = info.dateSecond;
m_FileHistory.push_back(mptHistory);
}
}
if(ok) {
if(commentChunk.IsValid()) {
std::string author;
commentChunk.ReadString<mpt::String::maybeNullTerminated>(author, 32);
if(author != "UNNAMED AUTHOR")
m_songArtist = mpt::ToUnicode(mpt::Charset::ISO8859_1, author);
if(!commentChunk.NoBytesLeft()) {
m_songMessage.ReadFixedLineLength(commentChunk, commentChunk.BytesLeft(), 40, 0);
}
}
m_modFormat.madeWithTracker = U_("ProTracker ") + version;
}
m_SongFlags.set(SONG_PT_MODE);
m_playBehaviour.set(kMODIgnorePanning);
m_playBehaviour.set(kMODOneShotLoops);
m_playBehaviour.reset(kMODSampleSwap);
return ok;
}
#ifndef MODPLUG_NO_FILESAVE
bool CSoundFile::SaveMod(std::ostream &f) const {
if(m_nChannels == 0) {
return false;
}
{
char name[20];
mpt::String::WriteBuf(mpt::String::maybeNullTerminated, name) = m_songName;
mpt::IO::Write(f, name);
}
std::vector <SmpLength> sampleLength(32, 0);
std::vector <SAMPLEINDEX> sampleSource(32, 0);
if(GetNumInstruments()) {
INSTRUMENTINDEX lastIns = std::min(INSTRUMENTINDEX(31), GetNumInstruments());
for(INSTRUMENTINDEX ins = 1; ins <= lastIns; ins++)
if(Instruments[ins]) {
for(auto smp : Instruments[ins]->Keyboard) {
if(smp > 0 && smp <= GetNumSamples()) {
sampleSource[ins] = smp;
break;
}
}
}
} else {
for(SAMPLEINDEX i = 1; i <= 31; i++) {
sampleSource[i] = i;
}
}
for(SAMPLEINDEX smp = 1; smp <= 31; smp++) {
MODSampleHeader sampleHeader;
mpt::String::WriteBuf(mpt::String::maybeNullTerminated, sampleHeader.name) = m_szNames[sampleSource[smp]];
sampleLength[smp] = sampleHeader.ConvertToMOD(
        sampleSource[smp] <= GetNumSamples() ? GetSample(sampleSource[smp]) : ModSample(MOD_TYPE_MOD));
mpt::IO::Write(f, sampleHeader);
}
MODFileHeader fileHeader;
MemsetZero(fileHeader);
PATTERNINDEX writePatterns = 0;
uint8 writtenOrders = 0;
for(ORDERINDEX ord = 0; ord < Order().GetLength() && writtenOrders < 128; ord++) {
if(ord == Order().GetRestartPos()) {
fileHeader.restartPos = writtenOrders;
}
if(Order()[ord] < 128) {
fileHeader.orderList[writtenOrders++] = static_cast<uint8>(Order()[ord]);
if(writePatterns <= Order()[ord]) {
writePatterns = Order()[ord] + 1;
}
}
}
fileHeader.numOrders = writtenOrders;
mpt::IO::Write(f, fileHeader);
char modMagic[4];
CHANNELINDEX writeChannels = std::min(CHANNELINDEX(99), GetNumChannels());
if(writeChannels == 4) {
if(writePatterns <= 64)
memcpy(modMagic, "M.K.", 4);
else
memcpy(modMagic, "M!K!", 4);
} else if(writeChannels < 10) {
memcpy(modMagic, "0CHN", 4);
modMagic[0] += static_cast<char>(writeChannels);
} else {
memcpy(modMagic, "00CH", 4);
modMagic[0] += static_cast<char>(writeChannels / 10u);
modMagic[1] += static_cast<char>(writeChannels % 10u);
}
mpt::IO::Write(f, modMagic);
bool invalidInstruments = false;
std::vector <uint8> events;
for(PATTERNINDEX pat = 0; pat < writePatterns; pat++) {
if(!Patterns.IsValidPat(pat)) {
events.assign(writeChannels * 64 * 4, 0);
mpt::IO::Write(f, events);
continue;
}
for(ROWINDEX row = 0; row < 64; row++) {
if(row >= Patterns[pat].GetNumRows()) {
events.assign(writeChannels * 4, 0);
mpt::IO::Write(f, events);
continue;
}
PatternRow rowBase = Patterns[pat].GetRow(row);
events.resize(writeChannels * 4);
size_t eventByte = 0;
for(CHANNELINDEX chn = 0; chn < writeChannels; chn++, eventByte += 4) {
const ModCommand &m = rowBase[chn];
uint8 command = m.command, param = m.param;
ModSaveCommand(command, param, false, true);
if(m.volcmd == VOLCMD_VOLUME && !command && !param) {
command = 0x0C;
param = std::min(m.vol, uint8(64));
}
uint16 period = 0;
if(m.note >= 24 + NOTE_MIN && m.note < std::size(ProTrackerPeriodTable) + 24 + NOTE_MIN) {
period = ProTrackerPeriodTable[m.note - 24 - NOTE_MIN];
}
const uint8 instr = (m.instr > 31) ? 0 : m.instr;
if(m.instr > 31)
invalidInstruments = true;
events[eventByte + 0] = ((period >> 8) & 0x0F) | (instr & 0x10);
events[eventByte + 1] = period & 0xFF;
events[eventByte + 2] = ((instr & 0x0F) << 4) | (command & 0x0F);
events[eventByte + 3] = param;
}
mpt::IO::WriteRaw(f, mpt::as_span(events));
}
}
if(invalidInstruments) {
AddToLog(LogWarning,
         U_("Warning: This track references sample slots higher than 31. Such samples cannot be saved in the MOD format, and thus the notes will not sound correct. Use the Cleanup tool to rearrange and remove unused samples."));
}
for(PATTERNINDEX pat = writePatterns; pat < Patterns.Size(); pat++) {
if(Patterns.IsValidPat(pat)) {
AddToLog(LogWarning,
         U_("Warning: This track contains at least one pattern after the highest pattern number referred to in the sequence. Such patterns are not saved in the MOD format."));
break;
}
}
for(SAMPLEINDEX smp = 1; smp <= 31; smp++) {
if(sampleLength[smp] == 0) {
continue;
}
const ModSample &sample = Samples[sampleSource[smp]];
const mpt::IO::Offset sampleStart = mpt::IO::TellWrite(f);
const size_t writtenBytes = MODSampleHeader::GetSampleFormat().WriteSample(f, sample, sampleLength[smp]);
const int8 silence = 0;
if((writtenBytes % 2u) != 0) {
mpt::IO::Write(f, silence);
}
if(!sample.uFlags[CHN_LOOP] && writtenBytes >= 2) {
const mpt::IO::Offset sampleEnd = mpt::IO::TellWrite(f);
mpt::IO::SeekAbsolute(f, sampleStart);
mpt::IO::Write(f, silence);
mpt::IO::Write(f, silence);
mpt::IO::SeekAbsolute(f, sampleEnd);
}
}
return true;
}
#endif  // MODPLUG_NO_FILESAVE
OPENMPT_NAMESPACE_END
