
#pragma once
#include "openmpt/all/BuildSettings.hpp"
#include "mpt/base/macros.hpp"
#include "mpt/string/types.hpp"
#include "openmpt/soundbase/Dither.hpp"
#include "openmpt/soundbase/DitherModPlug.hpp"
#include "openmpt/soundbase/DitherNone.hpp"
#include "openmpt/soundbase/DitherSimple.hpp"
#include "mptRandom.h"
#include <vector>
#include <variant>
#include <cstddef>
OPENMPT_NAMESPACE_BEGIN


using Dither_Default = Dither_Simple;
class DitherNamesOpenMPT {
public:
static mpt::ustring GetModeName(std::size_t mode) {
mpt::ustring result;
switch (mode) {
case 0:
result = MPT_USTRING("no");
break;
case 1:
result = MPT_USTRING("default");
break;
case 2:
result = MPT_USTRING("0.5 bit");
break;
case 3:
result = MPT_USTRING("1 bit");
break;
default:
result = MPT_USTRING("");
break;
}
return result;
}
};
using DithersOpenMPT =
Dithers <std::variant<MultiChannelDither <
                      Dither_None>, MultiChannelDither<Dither_Default>, MultiChannelDither<Dither_ModPlug>, MultiChannelDither<Dither_Simple>>, DitherNamesOpenMPT, 4, 1, 0, mpt::good_prng>;
struct DithersWrapperOpenMPT
        : DithersOpenMPT {
template<typename Trd>
DithersWrapperOpenMPT(Trd &rd, std::size_t mode = DithersOpenMPT::DefaultDither,
                      std::size_t channels = DithersOpenMPT::DefaultChannels)
        : DithersOpenMPT(rd, mode, channels) {
return;
}
};
OPENMPT_NAMESPACE_END
