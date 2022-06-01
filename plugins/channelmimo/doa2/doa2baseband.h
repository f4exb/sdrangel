///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_DOA2BASEBAND_H
#define INCLUDE_DOA2BASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplemififo.h"
#include "util/messagequeue.h"
#include "doa2streamsink.h"
#include "doa2corr.h"

class DownChannelizer;
class BasebandSampleSink;
class ScopeVis;

class DOA2Baseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getLog2Decim() const { return m_log2Decim; }
        int getFilterChainHash() const { return m_filterChainHash; }

        static MsgConfigureChannelizer* create(unsigned int log2Decim, unsigned int filterChainHash) {
            return new MsgConfigureChannelizer(log2Decim, filterChainHash);
        }

    private:
        unsigned int m_log2Decim;
        unsigned int m_filterChainHash;

        MsgConfigureChannelizer(unsigned int log2Decim, unsigned int filterChainHash) :
            Message(),
            m_log2Decim(log2Decim),
            m_filterChainHash(filterChainHash)
        { }
    };

    class MsgConfigureCorrelation : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        DOA2Settings::CorrelationType getCorrelationType() const { return m_correlationType; }

        static MsgConfigureCorrelation *create(DOA2Settings::CorrelationType correlationType) {
            return new MsgConfigureCorrelation(correlationType);
        }

    private:
        DOA2Settings::CorrelationType m_correlationType;

        MsgConfigureCorrelation(DOA2Settings::CorrelationType correlationType) :
            Message(),
            m_correlationType(correlationType)
        {}
    };

    class MsgSignalNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getInputSampleRate() const { return m_inputSampleRate; }
        qint64 getCenterFrequency() const { return m_centerFrequency; }
        int getStreamIndex() const { return m_streamIndex; }

        static MsgSignalNotification* create(int inputSampleRate, qint64 centerFrequency, int streamIndex) {
            return new MsgSignalNotification(inputSampleRate, centerFrequency, streamIndex);
        }
    private:
        int m_inputSampleRate;
        qint64 m_centerFrequency;
        int m_streamIndex;

        MsgSignalNotification(int inputSampleRate, qint64 centerFrequency, int streamIndex) :
            Message(),
            m_inputSampleRate(inputSampleRate),
            m_centerFrequency(centerFrequency),
            m_streamIndex(streamIndex)
        { }
    };

    DOA2Baseband(int fftSize);
    ~DOA2Baseband();
    void reset();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication

    void setScopeSink(ScopeVis *scopeSink) { m_scopeSink = scopeSink; }
    void setPhase(int phase) { m_correlator.setPhase(phase); }

	void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int streamIndex);
    void setBasebandSampleRate(unsigned int sampleRate);
    float getPhi() const { return m_phi; }
    void setMagThreshold(float threshold) { m_magThreshold = threshold * SDR_RX_SCALED * SDR_RX_SCALED; }
    void setFFTAveraging(int nbFFT);

private:
    void processFifo(const std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend);
    void run();
    bool handleMessage(const Message& cmd);
    void processDOA(const std::vector<Complex>::iterator& begin, int nbSamples, bool reverse = true);

    DOA2Correlator m_correlator;
    DOA2Settings::CorrelationType m_correlationType;
    int m_fftSize;
    int m_samplesCount;    //!< Number of samples processed by DOA
    float m_magSum;        //!< Squared magnitudes accumulator
    float m_wphSum;        //!< Phase difference accumulator (averaging weighted by squared magnitude)
    float m_phi;           //!< Resulting calculated phase difference
    double m_magThreshold; //!< Squared magnitude scaled threshold
    int m_fftAvg;          //!< Average over a certain number of FFTs
    int m_fftAvgCount;     //!< FFT averaging counter
    SampleMIFifo m_sampleMIFifo;
    std::vector<SampleVector::const_iterator> m_vbegin;
    int m_sizes[2];
    DOA2StreamSink m_sinks[2];
    DownChannelizer *m_channelizers[2];
    ScopeVis *m_scopeSink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    QMutex m_mutex;
    unsigned int m_lastStream;

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_DOA2BASEBAND_H
