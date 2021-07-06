
#pragma once
#include "openmpt/all/BuildSettings.hpp"
#include "Snd_defs.h"
#include "ModChannel.h"
OPENMPT_NAMESPACE_BEGIN

class CResampler;
// Sample conversion traits

template<int channelsOut, int channelsIn, typename out, typename in>
struct MixerTraits {
enum : int {
numChannelsIn = channelsIn
};    // Number of channels in sample
enum : int {
numChannelsOut = channelsOut
};    // Number of mixer output channels
typedef out output_t;                            // Output buffer sample type
typedef in input_t;                                // Input buffer sample type
typedef out outbuf_t[channelsOut];                // Output buffer sampling point type
// static MPT_CONSTEXPRINLINE output_t Convert(const input_t x)
};
// Interpolation templates

template<class Traits>
struct NoInterpolation {
MPT_FORCEINLINE void Start(const ModChannel &, const CResampler &) {}
MPT_FORCEINLINE void End(const ModChannel &) {}
MPT_FORCEINLINE void
operator()(typename Traits::outbuf_t &outSample, const typename Traits::input_t *const inBuffer, const int32) {
static_assert(static_cast<int>(Traits::numChannelsIn) <= static_cast<int>(Traits::numChannelsOut),
              "Too many input channels");
for(int i = 0; i < Traits::numChannelsIn; i++) {
outSample[i] = Traits::Convert(inBuffer[i]);
}
}
};
// Main sample render loop template
// Traits: A class derived from MixerTraits that defines the number of channels, sample buffer types, etc..
// FilterFunc: Functor for applying the resonant filter
template<class Traits, class InterpolationFunc, class FilterFunc, class MixFunc>
static void SampleLoop(ModChannel &chn, const CResampler &resampler, typename Traits::output_t *MPT_RESTRICT outBuffer,
                       unsigned int numSamples) {
ModChannel &c = chn;
const typename Traits::input_t *MPT_RESTRICT
inSample = static_cast<const typename Traits::input_t *>(c.pCurrentSample);
InterpolationFunc interpolate;
FilterFunc filter;
MixFunc mix;
interpolate.Start(c, resampler);
filter.Start(c);
mix.Start(c);
unsigned int samples = numSamples;
SamplePosition smpPos = c.position;    // Fixed-point sample position
const SamplePosition increment = c.increment;    // Fixed-point sample increment

while (samples--) {
typename Traits::outbuf_t outSample;
interpolate(outSample, inSample + smpPos.GetInt() * Traits::numChannelsIn, smpPos.GetFract());
filter(outSample, c);
mix(outSample, c, outBuffer);
outBuffer += Traits::numChannelsOut;
smpPos += increment;
}
mix.End(c);
filter.End(c);
interpolate.End(c);
c.position = smpPos;
}
typedef void (*MixFuncInterface)(ModChannel &, const CResampler &, mixsample_t *, unsigned int);
OPENMPT_NAMESPACE_END
