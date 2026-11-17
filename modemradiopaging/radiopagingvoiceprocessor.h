///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGVOICEPROCESSOR_H
#define MODEMRADIOPAGING_RADIOPAGINGVOICEPROCESSOR_H

#include "dsp/firfilter.h"
#include "audio/audiocompressorsnd.h"
#include "export.h"

// The voice of a voice page before the modulator, -1 to +1 is the full deviation:
//   band limit to the voice band, 300 to 3000 Hz (no DC or hum taking deviation);
//   a slow automatic gain to a level, its gain held in pauses (the noise is not raised);
//   a compressor (the speech is denser, its level still varies);
//   a clipper and a low pass at 3000 Hz again for what the clipper adds, with headroom
//   for the overshoot of the filter: the deviation is never over the full scale.
class MODEMRADIOPAGING_API RadioPagingVoiceProcessor
{
public:
    static constexpr float m_lowCutoff = 300.0f;        //!< Hz
    static constexpr float m_highCutoff = 3000.0f;      //!< Hz
    static constexpr float m_peak = 0.92f;              //!< Largest level out

    explicit RadioPagingVoiceProcessor(int sampleRate);

    //!< Start again: the filters, the gain and the compressor of a new voice
    void reset();
    float process(float sample);
    //!< Samples between a sample in and its sample out
    int delay() const { return m_delay; }

private:
    int m_sampleRate;
    int m_delay;
    Bandpass<Real> m_bandpass;
    Lowpass<Real> m_lowpass;
    AudioCompressorSnd m_compressor;
    float m_envelope;                   //!< Peak level of the band limited voice
    float m_gain;
    float m_attack;                     //!< Coefficients of the envelope, per sample
    float m_release;
    float m_gainRate;                   //!< Coefficient of the gain change, per sample
};

#endif // MODEMRADIOPAGING_RADIOPAGINGVOICEPROCESSOR_H
