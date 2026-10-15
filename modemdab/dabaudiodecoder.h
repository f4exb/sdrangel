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

#ifndef MODEMDAB_DABAUDIODECODER_H_
#define MODEMDAB_DABAUDIODECODER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "export.h"
#include "dabaudioframing.h"

namespace modemdab
{

// Decoded audio: interleaved stereo (mono is duplicated).
struct MODEMDAB_API DABPCM
{
    std::vector<int16_t> samples;
    int sampleRate = 0;
    bool concealed = false;

    int frames() const { return static_cast<int>(samples.size() / 2); }
};

// The audio codec, which third-party libraries provide: FAAD2 for DAB+ (HE-AAC v2) and
// FFmpeg for DAB (MPEG Layer II). A lost frame is concealed by fading the last frame
// out, then silence, and the next good frame fades in, so the timeline is kept.
class MODEMDAB_API DABAudioDecoder
{
public:
    virtual ~DABAudioDecoder() = default;

    virtual std::string backend() const = 0;
    virtual void reset() = 0;
    // Decode one DAB+ AU or one DAB audio frame. Returns false (and conceals) on error.
    bool decode(const uint8_t *data, int size, DABPCM& pcm);
    // Output a lost frame's worth of concealment
    void conceal(DABPCM& pcm);

    static bool aacAvailable();
    static bool mp2Available();
    // Null if the codec is not available
    static std::unique_ptr<DABAudioDecoder> createAAC(const DABPlusParameters& parameters);
    static std::unique_ptr<DABAudioDecoder> createMP2();

protected:
    virtual bool decodeFrame(const uint8_t *data, int size, DABPCM& pcm) = 0;
    // Samples per channel in a frame and the output rate, until a frame is decoded
    virtual int frameLength() const = 0;
    virtual int sampleRate() const = 0;

private:
    std::vector<int16_t> m_last;    // Last good frame
    int m_lastRate = 0;
    int m_concealed = 0;            // Consecutive concealed frames
};

} // namespace modemdab

#endif // MODEMDAB_DABAUDIODECODER_H_
