
#include "stdafx.h"
#include "MixerSettings.h"
#include "Snd_defs.h"
#include "../common/misc_util.h"
OPENMPT_NAMESPACE_BEGIN
MixerSettings::MixerSettings() {
m_nStereoSeparation = 128;
m_nMaxMixChannels = MAX_CHANNELS;
DSPMask = 0;
MixerFlags = 0;
gnChannels = 2;
gdwMixingFreq = 48000;
m_nPreAmp = 128;
VolumeRampUpMicroseconds = 363; // 16 @44100
VolumeRampDownMicroseconds = 952; // 42 @44100

NumInputChannels = 0;
}
int32 MixerSettings::GetVolumeRampUpSamples() const {
return Util::muldivr(VolumeRampUpMicroseconds, gdwMixingFreq, 1000000);
}
int32 MixerSettings::GetVolumeRampDownSamples() const {
return Util::muldivr(VolumeRampDownMicroseconds, gdwMixingFreq, 1000000);
}
void MixerSettings::SetVolumeRampUpSamples(int32
rampUpSamples)
{
VolumeRampUpMicroseconds = Util::muldivr(rampUpSamples, 1000000, gdwMixingFreq);
}
void MixerSettings::SetVolumeRampDownSamples(int32
rampDownSamples)
{
VolumeRampDownMicroseconds = Util::muldivr(rampDownSamples, 1000000, gdwMixingFreq);
}


OPENMPT_NAMESPACE_END
