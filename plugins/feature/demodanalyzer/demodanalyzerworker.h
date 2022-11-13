///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_DEMODALYZERWORKER_H
#define INCLUDE_DEMODALYZERWORKER_H

#include <vector>

#include <QObject>
#include <QRecursiveMutex>
#include <QByteArray>

#include "dsp/dsptypes.h"
#include "dsp/decimators.h"
#include "dsp/datafifo.h"
#include "util/movingaverage.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "demodanalyzersettings.h"

class BasebandSampleSink;
class ScopeVis;
class ChannelAPI;
class Feature;
class WavFileRecord;

class DemodAnalyzerWorker : public QObject {
    Q_OBJECT
public:
    class MsgConfigureDemodAnalyzerWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DemodAnalyzerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDemodAnalyzerWorker* create(const DemodAnalyzerSettings& settings, bool force)
        {
            return new MsgConfigureDemodAnalyzerWorker(settings, force);
        }

    private:
        DemodAnalyzerSettings m_settings;
        bool m_force;

        MsgConfigureDemodAnalyzerWorker(const DemodAnalyzerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConnectFifo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        DataFifo *getFifo() { return m_fifo; }
        bool getConnect() const { return m_connect; }

        static MsgConnectFifo* create(DataFifo *fifo, bool connect) {
            return new MsgConnectFifo(fifo, connect);
        }
    private:
        DataFifo *m_fifo;
        bool m_connect;
        MsgConnectFifo(DataFifo *fifo, bool connect) :
            Message(),
            m_fifo(fifo),
            m_connect(connect)
        { }
    };

    DemodAnalyzerWorker();
	~DemodAnalyzerWorker();
    void reset();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }

    void applySampleRate(int sampleRate);
	void applySettings(const DemodAnalyzerSettings& settings, bool force = false);

	double getMagSq() const { return m_magsq; }
	double getMagSqAvg() const { return (double) m_channelPowerAvg; }
    void setScopeVis(ScopeVis* scopeVis) { m_scopeVis = scopeVis; }

    static const unsigned int m_corrFFTLen;
    static const unsigned int m_ssbFftLen;

private:
    DataFifo *m_dataFifo;
    int m_channelSampleRate;
    int m_sinkSampleRate;
	MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report channel change to main feature object
    DemodAnalyzerSettings m_settings;
	double m_magsq;
	SampleVector m_sampleBuffer;
    std::vector<qint16> m_convBuffer;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimators;
    int m_sampleBufferSize;
	MovingAverageUtil<double, double, 480> m_channelPowerAvg;
    ScopeVis* m_scopeVis;
    WavFileRecord* m_wavFileRecord;
    int m_recordSilenceNbSamples;
    int m_recordSilenceCount;
    int m_nbBytes;
    QRecursiveMutex m_mutex;

    void feedPart(
        const QByteArray::const_iterator& begin,
        const QByteArray::const_iterator& end,
        DataFifo::DataType dataType
    );

    bool handleMessage(const Message& cmd);
    void decimate(int countSamples);
    void writeSampleToFile(const Sample& sample);

    inline void processSample(
        DataFifo::DataType dataType,
        const QByteArray::const_iterator& begin,
        int countSamples,
        int i
    )
    {
        switch(dataType)
        {
            case DataFifo::DataTypeI16: {
                int16_t *s = (int16_t*) begin;
                double re = s[i] / (double) std::numeric_limits<int16_t>::max();
                m_magsq = re*re;
                m_channelPowerAvg(m_magsq);

                if (m_settings.m_log2Decim == 0)
                {
                    m_sampleBuffer[i].setReal(re * SDR_RX_SCALEF);
                    m_sampleBuffer[i].setImag(0);
                }
                else
                {
                    m_convBuffer[2*i] = s[i];
                    m_convBuffer[2*i+1] = 0;

                    if (i == countSamples - 1) {
                        decimate(countSamples);
                    }
                }
            }
            break;
            case DataFifo::DataTypeCI16: {
                int16_t *s = (int16_t*) begin;
                double re = s[2*i]   / (double) std::numeric_limits<int16_t>::max();
                double im = s[2*i+1] / (double) std::numeric_limits<int16_t>::max();
                m_magsq = re*re + im*im;
                m_channelPowerAvg(m_magsq);

                if (m_settings.m_log2Decim == 0)
                {
                    m_sampleBuffer[i].setReal(re * SDR_RX_SCALEF);
                    m_sampleBuffer[i].setImag(im * SDR_RX_SCALEF);
                }
                else
                {
                    m_convBuffer[2*i]   = s[2*i];
                    m_convBuffer[2*i+1] = s[2*i+1];

                    if (i == countSamples - 1) {
                        decimate(countSamples);
                    }
                }
            }
            break;
        }
    }

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_DEMODALYZERWORKER_H
