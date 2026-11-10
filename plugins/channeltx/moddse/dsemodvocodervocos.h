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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERVOCOS_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERVOCOS_H_

#include <string>
#include <vector>

#include "dsp/dv/vocoscodec.h"

#include "dsemodvocoder.h"

/**
 * The V1 encoder of Angel61 revision 3 (sdrbase/dsp/dv/ANGEL61.md 3.1.1): vocoscodec's EnCodec
 * encoder through the library loaded at run time (VocosCodec), its model set checked.
 *
 * The processor feeds 20 ms frames at 24 kHz (480 samples); the codec's frames are 320 samples, so
 * the input is buffered and each 20 ms makes one packet or two, delivered in order with the frame's
 * sequence. A packet is 16 codebooks (20 bytes): the protocol truncates it to the codebook count of
 * the frame kind that carries it (2.3), which is the same packet as an encoder of that count makes.
 * Cannot open when the library or the models are missing: the error says which.
 */
class DSEModVocoderVocos : public DSEModVocoder
{
public:
    DSEModVocoderVocos();
    virtual ~DSEModVocoderVocos();

    virtual bool open(Rate rate);
    virtual void close();
    //! Stays open: loading the model takes 400 ms the first time and 60 ms after that, and a key-up
    //! spent on it is that much speech queued up at the start of the transmission
    virtual void endTransmission() {}
    virtual bool isReady() const { return m_rate != RateNone; }
    virtual bool canOpen() const;
    virtual bool canEncode(Rate rate) const { return isVocos(rate); }
    virtual void encode(uint32_t sequence, const int16_t *pcm);
    virtual int getLatencyFrames() const { return 0; }     //!< a packet goes out as soon as its 320 samples are in
    virtual const char *getName() const { return "vocos"; }

    //! Where the models are (empty: VocosCodec's default); before open()
    void setModelDirectory(const std::string& directory) { m_modelDirectory = directory; }
    const std::string& getError() const { return m_error; }

private:
    VocosCodec::Encoder m_encoder;
    std::string m_modelDirectory;
    std::string m_error;
    std::vector<int16_t> m_pending;     //!< input not yet a whole codec frame
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERVOCOS_H_
