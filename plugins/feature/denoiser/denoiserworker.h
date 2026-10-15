///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#ifndef INCLUDE_FEATURE_DENOISER_DENOISERWORKER_H_
#define INCLUDE_FEATURE_DENOISER_DENOISERWORKER_H_

#include <QObject>
#include <QRecursiveMutex>
#include <QByteArray>
#include <QDebug>

#include "util/movingaverage.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "dsp/dsptypes.h"
#include "dsp/datafifo.h"
#include "audio/audiofifo.h"

#include "denoisersettings.h"
#include "nvidiaaudioeffects.h"

class WavFileRecord;
class Vst3Effect;
struct DenoiseState;

class DenoiserWorker : public QObject {
    Q_OBJECT
public:
    class MsgConfigureDenoiserWorker : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const DenoiserSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }
        static MsgConfigureDenoiserWorker* create(const DenoiserSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureDenoiserWorker(settings, settingsKeys, force);
        }
    private:
        DenoiserSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;
        MsgConfigureDenoiserWorker(const DenoiserSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
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

    class MsgReportNvidiaStatus : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const QString& getError() const { return m_error; }
        static MsgReportNvidiaStatus* create(const QString& error) { 
            return new MsgReportNvidiaStatus(error); 
        }
    private:
        QString m_error;
        explicit MsgReportNvidiaStatus(const QString& error) : Message(), m_error(error) {}
    };

    class MsgReportVst3Status : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const QString& getError() const { return m_error; }
        static MsgReportVst3Status* create(const QString& error) { 
            return new MsgReportVst3Status(error); 
        }
    private:
        QString m_error;
        explicit MsgReportVst3Status(const QString& error) : 
            Message(), 
            m_error(error) 
        {}
    };

    explicit DenoiserWorker(QObject *parent = nullptr);
    ~DenoiserWorker() override;
    void reset();
    void startWork();
    void stopWork();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToFeature(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void setDemodProducer(QObject *producer) { m_demodProducer = producer; }
    void applySampleRate(int sampleRate);
    void applySettings(const DenoiserSettings& settings, const QStringList& settingsKeys, bool force = false);
    void setVst3Effect(Vst3Effect *effect);
	double getMagSq() const { return m_magsq; }
	double getMagSqAvg() const { return (double) m_channelPowerAvg; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of audio samples analyzed
	 */
	void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    static constexpr int m_nvidiaFrameSize = 480;
    static constexpr int m_vst3BlockSize = 512;

    DataFifo *m_dataFifo;
    QObject *m_demodProducer;
    int m_sinkSampleRate;
	MessageQueue m_inputMessageQueue;  //!< Queue for asynchronous inbound communication
    MessageQueue *m_msgQueueToFeature; //!< Queue to report channel change to main feature object
    DenoiserSettings m_settings;
    double m_magsq;
    SampleVector m_sampleBuffer;
    MovingAverageUtil<double, double, 480> m_channelPowerAvg;
    WavFileRecord* m_wavFileRecord;
    int m_recordSilenceNbSamples;
    int m_recordSilenceCount;
    int m_nbBytes;
	AudioVector m_audioBuffer;
	AudioFifo m_audioFifo;
	std::size_t m_audioBufferFill;
    DenoiseState *m_rnnoiseState;
    float m_rnnoiseIn[480];
    float m_rnnoiseOut[480];
    int m_rnnoiseFill;

    NvidiaAudioEffects m_nvidiaDenoiser;
    alignas(64) float m_nvidiaIn[m_nvidiaFrameSize];
    alignas(64) float m_nvidiaOut[m_nvidiaFrameSize];
    int m_nvidiaFill;
    bool m_nvidiaInitAttempted;
    bool m_nvidiaReady;
    bool m_nvidiaResetPending;
    bool m_nvidiaStatusReported;
    quint32 m_nvidiaGeneration;  //!< Incremented on any change that invalidates a model load

    Vst3Effect *m_vst3Effect;
    float m_vst3Input[2][m_vst3BlockSize];
    float m_vst3Output[2][m_vst3BlockSize];
    int m_vst3Fill;
    bool m_vst3FailureReported;

    quint32 m_levelCalcCount = 0;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel = 0.0f;
    Real m_levelSum = 0.0f;

    QRecursiveMutex m_mutex;

    static const int m_levelNbSamples;

    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void feedPart(
        const QByteArray::const_iterator& begin,
        const QByteArray::const_iterator& end,
        DataFifo::DataType dataType
    );

    bool handleMessage(const Message& cmd);
    void writeSampleToFile(const Sample& sample);
    void flushAudio();
    void processSample(
        DataFifo::DataType dataType,
        const QByteArray::const_iterator& begin,
        int i
    );
    void processI16DenoiserNone(const double& samplefp);
    void processI16DenoiserRNNoise(const double& samplefp);
    void processCI16DenoiserNone(const double& samplefpRe, const double& samplefpIm);
    void processCI16DenoiserRNNoise(const double& samplefpRe, const double& samplefpIm);
    void processNvidiaSample(const double& samplefp);
    void processVst3Sample(double left, double right);
    void reportNvidiaError(const QString& error);
    void resetNvidia();
    void prepareNvidia();
    void calculateLevel(const Real& sample);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_FEATURE_DENOISER_DENOISERWORKER_H_
