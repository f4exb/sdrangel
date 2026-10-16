///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_DABDEMODSINK_H
#define INCLUDE_DABDEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "audio/audiofifo.h"

#include "dabdemodsettings.h"

#include <atomic>
#include <complex>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "modemdab/dabreceiver.h"

#define DABDEMOD_CHANNEL_SAMPLE_RATE 2048000

class ChannelAPI;
class DABDemod;

class DABDemodSink : public ChannelSampleSink {
public:
    DABDemodSink();
    ~DABDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const DABDemodSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);
    void applyDABAudioSampleRate(int sampleRate);
    int getAudioSampleRate() const { return m_audioSampleRate; }
    int getChannelSampleRate() const { return m_channelSampleRate; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }

    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    double getMagSq() const { return m_magsq; }

    // Decoded audio arrived in the last half second (muted or not)
    bool isAudioActive() const;
    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

    void reset();
    void resetService();
    void setSPICache(const std::vector<modemdr::MOTFile>& files);

    // Reports, as messages to the channel
    void systemData(bool sync, float snr, int32_t freqOffset);
    void ensembleName(const QString& name, int id);
    void programName(const QString& name, int id);
    void programData(int bitrate, const QString& audio, const QString& language, const QString& programType);
    void audio(int16_t *buffer, int size, int samplerate, bool stereo);
    void programQuality(int16_t frames, int16_t rs, int16_t aac);
    void fibQuality(int16_t percent);
    void data(const QString& data);
    void motData(const uint8_t *data, int len, const QString& filename, int contentSubType);
    void tii(int tii);

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    DABDemodSettings m_settings;
    ChannelAPI *m_channel;

    int m_audioSampleRate;      // Output device sample rate
    int m_dabAudioSampleRate;
    bool m_dabAudioSampleRateReported;  // To the GUI, which clears it when the program changes
    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    bool m_programSet;

    // The receiver (modemdab)
    std::unique_ptr<modemdab::DABReceiver> m_native;
    std::vector<std::complex<float>> m_nativeBuffer;
    uint32_t m_nativeVersion;
    int m_nativeEnsembleId;
    std::string m_nativeEnsembleLabel;
    std::map<uint32_t, std::string> m_nativePrograms;
    std::string m_nativeProgramData;
    // The receiver's audio counters at the last quality report, and the figures reported
    struct AudioCounts
    {
        bool valid = false;
        int64_t superframes = 0;
        int64_t fireCodeErrors = 0;
        int64_t rsUncorrectableRows = 0;
        int64_t audioFrames = 0;
        int64_t audioFrameErrors = 0;
    };
    AudioCounts m_audioCounts;
    int16_t m_frameQuality = 0;
    int16_t m_rsQuality = 0;
    int16_t m_aacQuality = 0;
    int m_nativeTII;                    // Last reported: -2 none yet, DABDemod::TIINone none sent
    QString m_nativeServiceInfo;        // Last reported, summarized
    std::shared_ptr<const modemdr::SPIData> m_nativeSPI;
    int m_nativeStatusBlocks;

    // Input level and clipping, accumulated between signal quality reports
    double m_inputPower;
    int64_t m_inputSamples;
    int64_t m_inputClipped;
    int64_t m_lastRSCorrected;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    std::atomic<qint64> m_lastAudioMs{0};  // When decoded audio last arrived, for the GUI's thread
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToChannel;

    MovingAverageUtil<Real, double, 16> m_movingAverage;

    Interpolator m_audioInterpolator;
    Real m_audioInterpolatorDistance;
    Real m_audioInterpolatorDistanceRemain;
    AudioVector m_audioBuffer;
    AudioFifo m_audioFifo;
    std::size_t m_audioBufferFill;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    void processOneSample(Complex &ci);
    void processNative();
    void nativeStatus();
    void audioQuality(const modemdab::DABReceiverStatus& status);
    void resetAudioQuality();
    void nativeLabel();
    void nativeServiceInfo(const modemdab::DABReceiverStatus& status);
    void signalQuality();
    void selectNativeProgram();
    void processOneAudioSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void setProgram();
};

#endif // INCLUDE_DABDEMODSINK_H
