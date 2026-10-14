///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef MODEMDRM_DRMAUDIODECODER_H_
#define MODEMDRM_DRMAUDIODECODER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "export.h"
#include "drmsdc.h"

namespace modemdrm
{

struct MODEMDRM_API AudioDecoderCapabilities
{
    std::string backend;
    bool available = false;
    bool aac = false;
    bool xheAAC = false;
};

struct MODEMDRM_API PCMFrame
{
    std::vector<std::int16_t> samples;
    int sampleRate = 0;
    int channels = 0;
    bool concealed = false;
};

class MODEMDRM_API DRMAudioDecoder
{
public:
    virtual ~DRMAudioDecoder() = default;

    virtual AudioDecoderCapabilities capabilities() const = 0;
    virtual bool configure(
        const SDCAudioInformation& audio,
        std::string* error = nullptr
    ) = 0;
    virtual bool decode(
        const std::vector<std::uint8_t>& codecFrame,
        PCMFrame& output,
        std::string* error = nullptr
    ) = 0;
    virtual bool conceal(
        PCMFrame& output,
        std::string* error = nullptr
    ) = 0;
    virtual void reset() = 0;

    static std::unique_ptr<DRMAudioDecoder> create();
};

} // namespace modemdrm

#endif // MODEMDRM_DRMAUDIODECODER_H_
