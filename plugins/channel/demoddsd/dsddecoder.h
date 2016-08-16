///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNEL_DEMODDSD_DSDDECODER_H_
#define PLUGINS_CHANNEL_DEMODDSD_DSDDECODER_H_

#include "dsd_decoder.h"

class AudioFifo;

class DSDDecoder
{
public:
    DSDDecoder();
    ~DSDDecoder();

    void pushSample(short sample) { m_decoder.run(sample); }
    short *getAudio(int& nbSamples) { return m_decoder.getAudio(nbSamples); }
    void resetAudio() { m_decoder.resetAudio(); }
    void enableMbelib(bool enable) { m_decoder.enableMbelib(enable); }

    bool mbeDVReady() const { return m_decoder.mbeDVReady(); }
    void resetMbeDV() { m_decoder.resetMbeDV(); }
    const unsigned char *getMbeDVFrame() const { return m_decoder.getMbeDVFrame(); }
    int getMbeRateIndex() const { return (int) m_decoder.getMbeRate(); }

    int getInLevel() const { return m_decoder.getInLevel(); }
    int getSamplesPerSymbol() const { return m_decoder.getSamplesPerSymbol(); }
    DSDcc::DSDDecoder::DSDSyncType getSyncType() const { return m_decoder.getSyncType(); }
    DSDcc::DSDDecoder::DSDStationType getStationType() const { return m_decoder.getStationType(); }
    const char *getFrameTypeText() const { return m_decoder.getFrameTypeText(); }
    const char *getSlot0Text() const { return m_decoder.getSlot0Text(); }
    const char *getSlot1Text() const { return m_decoder.getSlot1Text(); }
    unsigned char getColorCode() const { return m_decoder.getColorCode(); }
    const DSDcc::DSDDstar& getDStarDecoder() const { return m_decoder.getDStarDecoder(); }
    const DSDcc::DSDdPMR& getDPMRDecoder() const { return m_decoder.getDPMRDecoder(); }

    void setAudioGain(float gain) { m_decoder.setAudioGain(gain); }
    void setBaudRate(int baudRate);

private:
    DSDcc::DSDDecoder m_decoder;
};

#endif /* PLUGINS_CHANNEL_DEMODDSD_DSDDECODER_H_ */
