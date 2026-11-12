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

#include <algorithm>
#include <cmath>

#include <QDebug>

#include "dsemodvocodermbelib.h"

#ifdef DSEMOD_HAS_MBELIB
#include <mbevocoder/mbevocoder.h>
#endif

DSEModVocoderMbelib::DSEModVocoderMbelib() :
    m_encoder(nullptr),
    m_gainAdjust(0.0f),
    m_toneDetect(true),
    m_noiseSuppress(false),
    m_toneFrameCount(0)
{
}

DSEModVocoderMbelib::~DSEModVocoderMbelib()
{
    close();
}

bool DSEModVocoderMbelib::isAvailable()
{
#ifdef DSEMOD_HAS_MBELIB
    return true;
#else
    return false;
#endif
}

const char *DSEModVocoderMbelib::getVersion()
{
#ifdef DSEMOD_HAS_MBELIB
    return mbe_versionString();
#else
    return "not built in";
#endif
}

void DSEModVocoderMbelib::setGain(int gainDB)
{
    // The encoder option is subtracted from the log2 gain: convert dB to log2 units
    float gainAdjust = -gainDB / (20.0f * std::log10(2.0f));

    if (gainAdjust != m_gainAdjust)
    {
        m_gainAdjust = gainAdjust;
        reopen();
    }
}

void DSEModVocoderMbelib::setToneDetect(bool on)
{
    if (on != m_toneDetect)
    {
        m_toneDetect = on;
        reopen();
    }
}

void DSEModVocoderMbelib::setNoiseSuppress(bool on)
{
    if (on != m_noiseSuppress)
    {
        m_noiseSuppress = on;
        reopen();
    }
}

// Encoder options are fixed at creation: recreate the encoder when they change while it is open.
// The analysis history restarts, which is inaudible against speech.
void DSEModVocoderMbelib::reopen()
{
    if (m_encoder) {
        open(m_rate);
    }
}

bool DSEModVocoderMbelib::open(Rate rate)
{
    close();
    m_rate = rate;

#ifdef DSEMOD_HAS_MBELIB
    mbe_encoder_options options;
    mbe_encoderDefaultOptions(&options);
    options.gain_adjust = m_gainAdjust;
    options.tone_detect = m_toneDetect ? 1 : 0;
    options.noise_suppress = m_noiseSuppress ? 1 : 0;
    mbe_rate mbeRate = (rate == Rate3600x2400) ? MBE_RATE_AMBE3600x2400
        : (rate == RateIMBE7200x4400) ? MBE_RATE_IMBE7200x4400
        : MBE_RATE_AMBE3600x2450;
    m_encoder = mbe_encoderCreate(mbeRate, &options);

    if (!m_encoder)
    {
        qWarning("DSEModVocoderMbelib::open: mbe_encoderCreate failed for rate %d", (int) rate);
        return false;
    }

    m_toneFrameCount = 0;
    qDebug("DSEModVocoderMbelib::open: %s rate %d delay %d samples tone detect %d noise suppression %d",
        mbe_versionString(), (int) mbeRate, mbe_encoderDelaySamples(m_encoder), (int) m_toneDetect, (int) m_noiseSuppress);
    return true;
#else
    qWarning("DSEModVocoderMbelib::open: mbevocoder encoder not built in");
    return false;
#endif
}

void DSEModVocoderMbelib::close()
{
#ifdef DSEMOD_HAS_MBELIB
    if (m_encoder)
    {
        mbe_encoderDestroy(m_encoder);
        m_encoder = nullptr;
    }
#endif
    m_rate = RateNone;
}

bool DSEModVocoderMbelib::encodeTone(uint32_t sequence, float f1, float f2, int amplitude)
{
#ifdef DSEMOD_HAS_MBELIB
    if (!m_encoder || (m_rate == RateIMBE7200x4400)) { // IMBE has no tone frames
        return false;
    }

    int toneId = mbe_toneIdFromFrequencies(f1, f2);

    if (toneId < 0) {
        return false;
    }

    uint8_t frame[m_maxFrameBytes];

    // Fails for tones the rate cannot carry (KNOX on D-STAR): the audio is coded as voice instead
    if (mbe_encoderToneDvsi(m_encoder, toneId, std::max(0, std::min(127, amplitude)), frame) != 0) {
        return false;
    }

    m_toneFrameCount++;
    deliver(sequence, frame);
    return true;
#else
    (void) sequence; (void) f1; (void) f2; (void) amplitude;
    return false;
#endif
}

void DSEModVocoderMbelib::encode(uint32_t sequence, const int16_t *pcm)
{
#ifdef DSEMOD_HAS_MBELIB
    if (!m_encoder) {
        return;
    }

    uint8_t frame[m_maxFrameBytes];
    int status;

    if (m_rate == RateIMBE7200x4400)
    {
        char bits[144];
        status = mbe_encoderEncode(m_encoder, pcm, MBE_LAYOUT_P25P1, bits);
        mbe_packBits(bits, 144, frame, 1);
    }
    else
    {
        status = mbe_encoderEncodeDvsi(m_encoder, pcm, frame);
    }

    if (status == 0)
    {
        if (mbe_encoderFrameFlags(m_encoder) & MBE_ENCODER_FLAG_TONE) {
            m_toneFrameCount++;
        }

        deliver(sequence, frame);
    }
    else
    {
        deliver(sequence, getSilenceFrame(m_rate));
    }
#else
    (void) pcm;
    deliver(sequence, getSilenceFrame(m_rate));
#endif
}
