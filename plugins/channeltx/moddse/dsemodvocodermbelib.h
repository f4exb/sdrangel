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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERMBELIB_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERMBELIB_H_

#include "dsemodvocoder.h"

struct mbe_encoder;

/**
 * Software vocoder back end using the mbevocoder encoder (mbe_encoder* API).
 * Encoding is synchronous: the frame is delivered from within encode().
 */
class DSEModVocoderMbelib : public DSEModVocoder
{
public:
    DSEModVocoderMbelib();
    virtual ~DSEModVocoderMbelib();

    virtual bool open(Rate rate);
    virtual void close();
    virtual bool isReady() const { return m_encoder != nullptr; }
    virtual bool canOpen() const { return isAvailable(); }
    virtual void encode(uint32_t sequence, const int16_t *pcm);
    virtual bool encodeTone(uint32_t sequence, float f1, float f2, int amplitude);
    virtual int getLatencyFrames() const { return 0; }
    virtual const char *getName() const { return "mbevocoder"; }

    void setGain(int gainDB);
    void setToneDetect(bool on);
    void setNoiseSuppress(bool on);
    uint32_t getToneFrameCount() const { return m_toneFrameCount; }
    static bool isAvailable(); //!< Built with the mbevocoder encoder
    static const char *getVersion();

private:
    mbe_encoder *m_encoder;
    float m_gainAdjust;
    bool m_toneDetect;
    bool m_noiseSuppress;
    uint32_t m_toneFrameCount;

    void reopen();
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERMBELIB_H_
