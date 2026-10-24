///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021-2026 Jon Beniston, M7RCE                                   //
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

#ifndef INCLUDE_ACARSDEMODSINK_H
#define INCLUDE_ACARSDEMODSINK_H

#include <QVector>

#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "acarsdemodsettings.h"
#include "acarsoqpsk.h"
#include "acarsvdl2.h"
#include "acarshfdl.h"
#include "acarsaero.h"
#include "acarsiridium.h"
#include "acarsiridiumscanner.h"

#define ACARSDEMOD_CHANNEL_SAMPLE_RATE ACARSOQPSK_CHANNEL_SAMPLE_RATE

class ChannelAPI;
class AcarsDemod;
class ScopeVis;

class AcarsDemodSink : public ChannelSampleSink {
public:
    AcarsDemodSink(AcarsDemod *acarsDemod);
    ~AcarsDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const AcarsDemodSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    // The fixed rate the demodulator for the given settings runs at. Aero needs the
    // whole settings object rather than just the mode, because its rate depends on the
    // submode as well - the MSK rates and the OQPSK rate run at different sample rates.
    static int channelSampleRate(const AcarsDemodSettings& settings)
    {
        switch (settings.m_mode)
        {
        case AcarsDemodSettings::VDL2: return ACARSVDL2_CHANNEL_SAMPLE_RATE;
        case AcarsDemodSettings::HFDL: return ACARSHFDL_CHANNEL_SAMPLE_RATE;
        case AcarsDemodSettings::Aero:
            return AcarsAeroReceiver::channelSampleRate(
                AcarsAeroReceiver::submodeRate(settings.m_aeroChannel));
        // Iridium scans rather than tuning one channel, so the rate is the width of band
        // it covers, not the rate one receiver runs at
        case AcarsDemodSettings::Iridium:
            return AcarsDemodSettings::iridiumScanChannels(settings.m_iridiumBandwidth)
                 * ACARSIRIDIUM_SCAN_SPACING;
        default: return ACARSDEMOD_CHANNEL_SAMPLE_RATE;
        }
    }

    double getMagSq() const { return m_magsq; }

    //! Aero link quality: EVM as a fraction and the recent signal unit CRC pass rate,
    //! both negative until the receiver has something to report
    void getAeroQuality(double& evm, double& suRate) const
    {
        evm = m_aeroReceiver.evm();
        suRate = m_aeroReceiver.suRate();
    }

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

    ScopeVis* m_scopeSink;
    AcarsDemod *m_acarsDemod;
    AcarsDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToChannel;
    MovingAverageUtil<Real, double, 16> m_movingAverage;

    AcarsOqpskReceiver m_receiver;
    AcarsVdl2Receiver m_vdl2Receiver;
    AcarsHfdlReceiver m_hfdlReceiver;
    AcarsAeroReceiver m_aeroReceiver;
    AcarsIridiumScanner m_iridiumScanner;
    int m_iridiumChannels;      //!< What the scanner is currently configured for

    //!< Scanning the whole band decodes a dozen frames a second, so none of them are
    //!< logged individually - what the band is doing is summarised this often instead,
    //!< as the change in the scanner's counters since the last one
    static constexpr double m_iridiumLogSeconds = 60.0;
    qint64 m_iridiumLogCount;   //!< Input samples since the last summary
    int m_iridiumCarrierCount;  //!< Reassembled messages whose type carries a payload

    struct IridiumLog
    {
        int m_bursts = 0;
        int m_duplicates = 0;
        int m_frames = 0;
        int m_ida = 0;
        int m_messages = 0;
        int m_carriers = 0;
        int m_acars = 0;
    };

    IridiumLog m_iridiumLog;    //!< The counters as of the last summary

    //!< Whether a Demod Analyzer is attached, rechecked every so many samples rather
    //!< than per sample. A tenth of a second at the fastest rate the plugin runs at
    bool m_demodPipesPresent;
    int m_demodPipeCheck;
    static const int m_demodPipeCheckInterval = 100000;

    static const int m_scopeBufferSize = 512;
    SampleVector m_scopeBuffer;
    std::vector<SampleVector::const_iterator> m_scopeBegin;
    int m_scopeBufferIndex;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    void processOneSample(Complex &ci);
    void logIridiumSummary();
    void sampleToScope(Real ch1, Real ch2);
    Real scopeChannel(int selection, const Complex& ci, Real magsq) const;

    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
};

#endif // INCLUDE_ACARSDEMODSINK_H
