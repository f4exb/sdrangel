///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_IEEE_802_15_4_MODSOURCE_H
#define INCLUDE_IEEE_802_15_4_MODSOURCE_H

#include <QMutex>
#include <QDebug>

#include <iostream>
#include <fstream>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/raisedcosine.h"
#include "dsp/fmpreemphasis.h"
#include "util/lfsr.h"
#include "util/movingaverage.h"
#include "util/message.h"

#include "ieee_802_15_4_modsettings.h"

class BasebandSampleSink;
class ScopeVis;
class QUdpSocket;

class IEEE_802_15_4_ModSource : public QObject, public ChannelSampleSource
{
    Q_OBJECT
public:
    class MsgCloseUDP : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgCloseUDP* create() {
            return new MsgCloseUDP();
        }

   private:

        MsgCloseUDP() :
            Message()
        { }
    };

    class MsgOpenUDP : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgOpenUDP* create(const QString& udpAddress, uint16_t udpPort) {
            return new MsgOpenUDP(udpAddress, udpPort);
        }

        const QString& getUDPAddress() const { return m_udpAddress; }
        uint16_t getUDPPort() const { return m_udpPort; }

   private:
        QString m_udpAddress;
        uint16_t m_udpPort;

        MsgOpenUDP(const QString& udpAddress, uint16_t udpPort) :
            Message(),
            m_udpAddress(udpAddress),
            m_udpPort(udpPort)
        { }
    };

    IEEE_802_15_4_ModSource();
    virtual ~IEEE_802_15_4_ModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }
    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applySettings(const IEEE_802_15_4_ModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    bool handleMessage(const Message& cmd);

    void addTxFrame(const QString& data);
    void addTxFrame(const QByteArray& data);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_spectrumRate;
    IEEE_802_15_4_ModSettings m_settings;

    NCO m_carrierNco;
    Real m_linearGain;
    Complex m_modSample;
    double *m_sinLUT;

    int m_chips[2];                     // Chips. Odd/even for O-QPSK
    bool m_chipOdd;
    int m_diffBit;                      // Output of differential coder
    RaisedCosine<Real> m_pulseShapeI;   // Pulse shaping filters
    RaisedCosine<Real> m_pulseShapeQ;
    Lowpass<Complex> m_lowpass;         // Low pass filter to limit RF bandwidth
    LFSR m_scrambler;                   // Scrambler

    BasebandSampleSink* m_spectrumSink; // Spectrum GUI to display baseband waveform
    ScopeVis* m_scopeSink;              // Scope GUI to display baseband waveform
    SampleVector m_specSampleBuffer;
    int m_specSampleBufferIndex;
    static const int m_specSampleBufferSize = 1024;
    SampleVector m_scopeSampleBuffer;
    int m_scopeSampleBufferIndex;
    static const int m_scopeSampleBufferSize = 4800;
    Interpolator m_interpolator;        // Interpolator to downsample to 4k in spectrum
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    static const int m_levelNbSamples = 480;  // every 10ms assuming 48k Sa/s

    int m_sampleIdx;                    // Sample index in to chip
    int m_samplesPerChip;               // Number of samples per chip
    int m_chipsPerSymbol;               // Number of chips per symbol
    int m_bitsPerSymbol;                // Number of bits per symbol
    int m_chipRate;

    int m_symbol;
    int m_chipIdx;

    Real m_pow;                         // In dB
    Real m_powRamp;                     // In dB
    enum IEEE_802_15_4_ModState {
        idle, ramp_up, tx, ramp_down, wait
    } m_state;                          // States for sample modulation
    int m_frameRepeatCount;
    uint64_t m_waitCounter;             // Samples to wait before retransmission

    uint8_t m_bits[4+1+1+127];          // Bits to transmit (preamble, SFD, length, payload)
    int m_byteIdx;                      // Index in to m_bits
    int m_bitIdx;                       // Index in to current byte of m_bits
    int m_bitCount;                     // Count of number of valid bits in m_bits
    int m_bitCountTotal;
    std::ofstream m_basebandFile;       // For debug output of baseband waveform
    QUdpSocket *m_udpSocket;
    MessageQueue m_inputMessageQueue;   //!< Queue for asynchronous inbound communication

    bool chipsValid();                  // Are there any chips to transmit
    int getSymbol();
    int getChip();
    void convert(const QString dataStr, QByteArray& data);
    void initTX();
    void openUDP(const QString& udpAddress, uint16_t udpPort);
    void closeUDP();
    void createHalfSine(int sampleRate, int chipRate);

    void calculateLevel(Real& sample);
    void modulateSample();
    void sampleToSpectrum(Complex sample);
    void sampleToScope(Complex sample);

private slots:
    void handleInputMessages();
    void udpRx();
};

#endif // INCLUDE_IEEE_802_15_4_MODSOURCE_H
