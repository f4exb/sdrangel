///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELTX_MODDATV_DATVMODSOURCE_H_
#define PLUGINS_CHANNELTX_MODDATV_DATVMODSOURCE_H_

#include <vector>
#include <fstream>
#include <iostream>
#include <cstdint>

#include <QObject>
#include <QMutex>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/rootraisedcosine.h"
#include "util/movingaverage.h"
#include "dsp/fftfilt.h"
#include "util/message.h"

#include "datvmodsettings.h"

#include "dvb-s/dvb-s.h"
#include "dvb-s2/DVBS2.h"

class MessageQueue;
class QUdpSocket;

class DATVModSource : public ChannelSampleSource
{
public:
    DATVModSource();
    ~DATVModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples);

    int getEffectiveSampleRate() const { return m_channelSampleRate; };
    double getMagSq() const { return m_movingAverage.asDouble(); }

    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void geTsFileInfos(int& mpegTSBitrate, int& mpegTSSize) const {
        mpegTSBitrate = m_mpegTSBitrate;
        mpegTSSize = (int) m_mpegTSSize;
    }
    int64_t getUdpByteCount() const { return m_udpAbsByteCount; }
    int getDataRate() const { return getDVBSDataBitrate(m_settings); }

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const DATVModSettings& settings, bool force = false);
    void openTsFile(const QString& fileName);
    void seekTsFileStream(int seekPercentage);
    void reportTsFileSourceStreamTiming();
    void reportUDPBitrate();
    void reportUDPBufferUtilization();

private:
    uint8_t m_mpegTS[188];                                      //!< MPEG transport stream packet
    std::ifstream m_mpegTSStream;                               //!< File to read transport stream from
    int m_mpegTSBitrate;                                        //!< Bitrate of transport stream file
    std::streampos m_mpegTSSize;                                //!< Size in bytes of transport stream file
    RootRaisedCosine<Real> m_pulseShapeI;                       //!< Square root raised cosine filter for I samples
    RootRaisedCosine<Real> m_pulseShapeQ;                       //!< Square root raised cosine filter for Q samples
    DVBS m_dvbs;                                                //!< DVB-S encoder
    int m_sampleIdx;
    int m_frameIdx;                                             //!< Frame index in to TS file
    int m_frameCount;                                           //!< Count of frames transmitted, including null frames
    float m_tsRatio;                                            //!< DVB data rate to TS file bitrate ratio
    int m_symbolCount;                                          //!< Number of symbols returned from DVB-S encoder
    int m_symbolSel;                                            //!< I or Q symbol for BPSK modulation only
    int m_symbolIdx;
    int m_samplesPerSymbol;
    uint8_t m_iqSymbols[DVBS::m_maxIQSymbols*2];

    DVBS2 m_dvbs2;
    DVB2FrameFormat m_dvbs2Format;
    scmplx *m_plFrame;

    QUdpSocket *m_udpSocket;                                    //!< UDP socket to receive MPEG transport stream via
    int m_udpByteCount;                                         //!< Count of bytes received via UDP for bitrate calculation
    int64_t m_udpAbsByteCount;                                  //!< Count of bytes received via UDP since the begining
    boost::chrono::steady_clock::time_point m_udpTimingStart;   //!< When we last started counting UDP bytes
    uint8_t m_udpBuffer[188*10];
    int m_udpBufferIdx;                                         //!< TS frame index into buffer
    int m_udpBufferCount;                                       //!< Number of TS frames in buffer
    int m_udpMaxBufferUtilization;

    int m_sampleRate;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    DATVModSettings m_settings;

    NCO m_carrierNco;
    Complex m_modSample;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    MovingAverageUtil<double, double, 16> m_movingAverage;
    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    bool m_tsFileOK;

    MessageQueue *m_messageQueueToGUI;

    static const int m_levelNbSamples;

    void pullFinalize(Complex& ci, Sample& sample);
    void calculateLevel(Real& sample);
    void modulateSample();

    int getTSBitrate(const QString& filename);
    int getDVBSDataBitrate(const DATVModSettings& settings) const;
    void checkBitrates();
    void updateUDPBufferUtilization();

    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }

};


#endif /* PLUGINS_CHANNELTX_MODATV_DATVMOD_H_ */
