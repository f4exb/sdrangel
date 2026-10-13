///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_SELCALDEMODSINK_H
#define INCLUDE_SELCALDEMODSINK_H

#include <array>
#include <memory>
#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "audio/audiofifo.h"
#include "util/messagequeue.h"
#include "selcaldemodsettings.h"
#include "selcaldecoder.h"
#include "selcalcarriertracker.h"

class fftfilt;
class ChannelAPI;
class SELCALDemod;
class SpectrumVis;

class SELCALDemodSink : public ChannelSampleSink {
public:
    explicit SELCALDemodSink(SELCALDemod *demod = nullptr);
    ~SELCALDemodSink();

    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end) override;
    void applyChannelSettings(int sampleRate, int frequencyOffset, bool force = false);
    void applySettings(const QStringList& keys, const SELCALDemodSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);
    void reset();

    void setMessageQueueToChannel(MessageQueue *queue) { m_messageQueueToChannel = queue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void setCenterFrequency(qint64 frequency) { m_centerFrequency = frequency; }
    void setSpectrumSink(SpectrumVis *spectrumSink) { m_spectrumSink = spectrumSink; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    int getAudioSampleRate() const { return m_audioSampleRate; }
    double getMagSq() const { return m_lastMagSq; }
    void getMagSqLevels(double& average, double& peak, int& count);

private:
    SELCALDemodSettings m_settings;
    ChannelAPI *m_channel = nullptr;
    MessageQueue *m_messageQueueToChannel = nullptr;
    qint64 m_centerFrequency = 0;
    int m_channelSampleRate = 48000;
    int m_channelFrequencyOffset = 0;
    NCO m_nco;
    Interpolator m_interpolator;
    Real m_distance = 6;
    Real m_distanceRemain = 0;
    std::unique_ptr<fftfilt> m_filter;
    SELCALDecoder m_decoder;
    double m_decoderOffset = 0;
    double m_decoderUncertainty = 0;
    SELCALCarrierTracker m_carrierTracker;
    double m_carrierOffset = 0;
    int m_carrierHoldover = 0; // Samples for which the last carrier offset is still used
    int m_carrierTail = 0;     // Delayed samples still to be processed with the lost carrier
    NCO m_carrierNco;
    Complex m_carrierPhasor{};
    Complex m_amCarrier{};
    std::array<Complex, SELCALCarrierTracker::latency()> m_carrierDelay{};
    int m_carrierDelayPosition = 0;
    SpectrumVis *m_spectrumSink = nullptr;
    SampleVector m_spectrumBuffer;
    double m_dc = 0;
    double m_audioPower = 1e-6;
    double m_squelchLevel = 1e-8;
    double m_squelchPower = 1e-12;
    int m_squelchCount = 0;
    bool m_squelchOpen = false;
    double m_magSqSum = 0;
    double m_magSqPeak = 0;
    double m_lastMagSq = 1e-12;
    int m_magSqCount = 0;
    std::uint64_t m_audioSampleCount = 0;

    // Received power in 10 ms blocks, to measure the RSSI of each call's bursts.
    static constexpr int m_powerBlockSize = 80;
    static constexpr int m_filterLength = 512; // FFT length of the receive filter
    static constexpr int m_carrierHoldoverLength = 10 * SELCALDecoder::m_sampleRate; // 10 s

    std::array<double, 512> m_powerBlocks{};
    double m_powerBlockSum = 0;
    int m_powerBlockSamples = 0;
    std::uint64_t m_filterInputCount = 0;
    int m_statusCounter = 0;
    AudioFifo m_audioFifo;
    AudioVector m_audioBuffer;
    int m_audioSampleRate = 48000;
    Interpolator m_audioInterpolator;
    Real m_audioDistance = 1.0f / 6;
    Real m_audioDistanceRemain = 0;
    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill = 0;

    void processOneSample(const Complex& sample);
    void processAudio(float sample);
    void monitor(float sample);
    void writeDemod();
    void configureFilter(bool recreate);
    void resetTuning();
    void restartSampleCount();
    double burstPower(std::uint64_t start, std::uint64_t end) const;
    bool trackingCarrier() const;
    void updateDecoderOffset();
};

#endif
