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

#ifndef INCLUDE_DSDVOCODERMBELIB_H
#define INCLUDE_DSDVOCODERMBELIB_H

#include "dsdprotocol.h"

/**
 * Software vocoder: mbevocoder soft decision decoding of the voice frames the
 * decoders emit (AMBE 3600x2400, AMBE+2 3600x2450 and 2450, IMBE 7200x4400)
 * to 160 samples at 8 kHz per frame. One instance per audio stream (a DMR
 * slot). Compiled to a silent stub when mbevocoder is not available.
 */
class DSDVocoderMbelib
{
public:
    struct Result
    {
        int m_correctedBits = 0;    //!< Bits corrected by the vocoder's own codes
        int m_protectedBits = 0;    //!< Bits those codes protect (the corrected fraction estimates the raw BER)
        bool m_muted = false;       //!< Too many errors: the library muted or repeated the frame
        bool m_tone = false;
    };

    static constexpr int m_samplesPerFrame = 160;

    DSDVocoderMbelib();
    ~DSDVocoderMbelib();
    DSDVocoderMbelib(const DSDVocoderMbelib&) = delete;
    DSDVocoderMbelib& operator=(const DSDVocoderMbelib&) = delete;

    static bool isAvailable();
    static const char *getVersion();
    /**
     * The channel's bit errors in an MBE frame, whatever vocoder plays it: a hard decision decode of
     * the frame's own codes (C0 and C1 of the AMBE frames, 47 bits; the Golay and Hamming blocks of
     * IMBE, 137 bits), mbelib's, or sdrbase's for AMBE+2 when mbelib is not built in. An
     * uncorrectable C0 counts as 4 errors. False for the rates without such codes.
     */
    static bool channelErrors(const DSDVoiceFrame& frame, int& errors, int& bits);
    /**
     * The DTMF key of an AMBE tone frame (mbevocoder's docs/tone-frames.md), whatever vocoder plays it:
     * the frame's codes decoded hard and the tone id parsed (DVDTMF), accepted as mbelib accepts a tone
     * (AMBE+2: fewer than 6 errors; D-STAR: fewer than 2 in C0 and 3 in all). 0 for a voice frame, a
     * tone that is no DTMF key, or a rate without tone frames (IMBE). Without mbelib, AMBE+2's only.
     */
    static char dtmfKey(const DSDVoiceFrame& frame);

    void reset();
    //! Decode one frame to audio (float, full scale +-32767). Returns false if the frame could not be
    //! decoded at all (unsupported rate); the audio is then silence.
    bool decode(const DSDVoiceFrame& frame, float *audio, Result& result);

private:
    struct State;
    State *m_state;
};

#endif // INCLUDE_DSDVOCODERMBELIB_H
