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

#ifndef PLUGINS_CHANNELRX_DEMODDSD_DSDDECODER_H_
#define PLUGINS_CHANNELRX_DEMODDSD_DSDDECODER_H_

#include "dsd_decoder.h"

class AudioFifo;

class DSDDecoder
{
public:
    DSDDecoder();
    ~DSDDecoder();

    void pushSample(short sample) { m_decoder.run(sample); }
    short getFilteredSample() const { return m_decoder.getFilteredSample(); }
    short getSymbolSyncSample() const { return m_decoder.getSymbolSyncSample(); }

    short *getAudio1(int& nbSamples) { return m_decoder.getAudio1(nbSamples); }
    void resetAudio1() { m_decoder.resetAudio1(); }
    short *getAudio2(int& nbSamples) { return m_decoder.getAudio2(nbSamples); }
    void resetAudio2() { m_decoder.resetAudio2(); }

    void enableMbelib(bool enable) { m_decoder.enableMbelib(enable); }

    bool mbeDVReady1() const { return m_decoder.mbeDVReady1(); }
    void resetMbeDV1() { m_decoder.resetMbeDV1(); }
    bool mbeDVReady2() const { return m_decoder.mbeDVReady2(); }
    void resetMbeDV2() { m_decoder.resetMbeDV2(); }
    const unsigned char *getMbeDVFrame1() const { return m_decoder.getMbeDVFrame1(); }
    const unsigned char *getMbeDVFrame2() const { return m_decoder.getMbeDVFrame2(); }
    bool getVoice1On() const { return m_decoder.getVoice1On(); }
    bool getVoice2On() const { return m_decoder.getVoice2On(); }
    void setTDMAStereo(bool tdmaStereo) { m_decoder.setTDMAStereo(tdmaStereo); }
    bool getSymbolPLLLocked() const { return m_decoder.getSymbolPLLLocked(); }

    int getMbeRateIndex() const { return (int) m_decoder.getMbeRate(); }

    int getInLevel() const { return m_decoder.getInLevel(); }
    int getCarrierPos() const { return m_decoder.getCarrierPos(); }
    int getZeroCrossingPos() const { return m_decoder.getZeroCrossingPos(); }
    int getSymbolSyncQuality() const { return m_decoder.getSymbolSyncQuality(); }
    int getSamplesPerSymbol() const { return m_decoder.getSamplesPerSymbol(); }
    void enableCosineFiltering(bool on) { m_decoder.enableCosineFiltering(on); }
    DSDcc::DSDDecoder::DSDSyncType getSyncType() const { return m_decoder.getSyncType(); }
    DSDcc::DSDDecoder::DSDStationType getStationType() const { return m_decoder.getStationType(); }
    const char *getFrameTypeText() const { return m_decoder.getFrameTypeText(); }
    const DSDcc::DSDDMR& getDMRDecoder() const { return m_decoder.getDMRDecoder(); }
    const DSDcc::DSDDstar& getDStarDecoder() const { return m_decoder.getDStarDecoder(); }
    const DSDcc::DSDdPMR& getDPMRDecoder() const { return m_decoder.getDPMRDecoder(); }
    const DSDcc::DSDYSF& getYSFDecoder() const { return m_decoder.getYSFDecoder(); }

    void setMyPoint(float lat, float lon) { m_decoder.setMyPoint(lat, lon); }
    void setAudioGain(float gain) { m_decoder.setAudioGain(gain); }
    void setBaudRate(int baudRate);
    void setSymbolPLLLock(bool pllLock) { m_decoder.setSymbolPLLLock(pllLock); }
    void useHPMbelib(bool useHP) { m_decoder.useHPMbelib(useHP); }

private:
    DSDcc::DSDDecoder m_decoder;
};

#endif /* PLUGINS_CHANNELRX_DEMODDSD_DSDDECODER_H_ */
