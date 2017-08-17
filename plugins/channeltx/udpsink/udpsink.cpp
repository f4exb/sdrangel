///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include "dsp/upchannelizer.h"
#include "util/db.h"
#include "udpsinkmsg.h"
#include "udpsink.h"

MESSAGE_CLASS_DEFINITION(UDPSink::MsgUDPSinkConfigure, Message)
MESSAGE_CLASS_DEFINITION(UDPSink::MsgUDPSinkSpectrum, Message)

UDPSink::UDPSink(MessageQueue* uiMessageQueue, UDPSinkGUI* udpSinkGUI, BasebandSampleSink* spectrum) :
    m_uiMessageQueue(uiMessageQueue),
    m_udpSinkGUI(udpSinkGUI),
    m_spectrum(spectrum),
    m_spectrumEnabled(false),
    m_spectrumChunkSize(2160),
    m_spectrumChunkCounter(0),
    m_magsq(1e-10),
    m_movingAverage(16, 1e-10),
    m_inMovingAverage(480, 1e-10),
    m_sampleRateSum(0),
    m_sampleRateAvgCounter(0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_levelNbSamples(480),
    m_squelchOpen(false),
    m_squelchOpenCount(0),
    m_squelchCloseCount(0),
    m_squelchThreshold(4800),
    m_settingsMutex(QMutex::Recursive)
{
    setObjectName("UDPSink");
    m_udpHandler.setFeedbackMessageQueue(&m_inputMessageQueue);
    apply(true);
}

UDPSink::~UDPSink()
{
}

void UDPSink::start()
{
    m_udpHandler.start();
}

void UDPSink::stop()
{
    m_udpHandler.stop();
}

void UDPSink::pull(Sample& sample)
{
    if (m_running.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        initSquelch(false);
        return;
    }

    Complex ci;

    m_settingsMutex.lock();

    if (m_interpolatorDistance > 1.0f) // decimate
    {
        modulateSample();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }
    else
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    m_settingsMutex.unlock();

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    magsq /= (1<<30);
    m_movingAverage.feed(magsq);
    m_magsq = m_movingAverage.average();

    sample.m_real = (FixReal) ci.real();
    sample.m_imag = (FixReal) ci.imag();
}

void UDPSink::modulateSample()
{
    //Real t;
    Sample s;

    if (m_running.m_sampleFormat == FormatS16LE)
    {
        m_udpHandler.readSample(s);

        uint64_t magsq = s.m_real * s.m_real + s.m_imag * s.m_imag;
        m_inMovingAverage.feed(magsq/1073741824.0);
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            m_modSample.real(s.m_real * m_running.m_volume);
            m_modSample.imag(s.m_imag * m_running.m_volume);
            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else
    {
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
        initSquelch(false);
    }

    if (m_spectrum && m_spectrumEnabled && (m_spectrumChunkCounter < m_spectrumChunkSize - 1))
    {
        Sample s;
        s.m_real = (FixReal) m_modSample.real();
        s.m_imag = (FixReal) m_modSample.imag();
        m_sampleBuffer.push_back(s);
        m_spectrumChunkCounter++;
    }
    else
    {
        m_spectrum->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
        m_sampleBuffer.clear();
        m_spectrumChunkCounter = 0;
    }
}

void UDPSink::calculateLevel(Real sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        qreal rmsLevel = m_levelSum > 0.0 ? sqrt(m_levelSum / m_levelNbSamples) : 0.0;
        //qDebug("NFMMod::calculateLevel: %f %f", rmsLevel, m_peakLevel);
        emit levelChanged(rmsLevel, m_peakLevel, m_levelNbSamples);
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void UDPSink::calculateLevel(Complex sample)
{
    Real t = std::abs(sample);

    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), t);
        m_levelSum += (t * t);
        m_levelCalcCount++;
    }
    else
    {
        qreal rmsLevel = m_levelSum > 0.0 ? sqrt((m_levelSum/(1<<30)) / m_levelNbSamples) : 0.0;
        emit levelChanged(rmsLevel, m_peakLevel / 32768.0, m_levelNbSamples);
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

bool UDPSink::handleMessage(const Message& cmd)
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;

        m_config.m_basebandSampleRate = notif.getBasebandSampleRate();
        m_config.m_outputSampleRate = notif.getSampleRate();
        m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

        apply(false);

        qDebug() << "UDPSink::handleMessage: MsgChannelizerNotification:"
                << " m_basebandSampleRate: " << m_config.m_basebandSampleRate
                << " m_outputSampleRate: " << m_config.m_outputSampleRate
                << " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

        return true;
    }
    else if (MsgUDPSinkConfigure::match(cmd))
    {
        MsgUDPSinkConfigure& cfg = (MsgUDPSinkConfigure&) cmd;

        m_config.m_sampleFormat = cfg.getSampleFormat();
        m_config.m_inputSampleRate = cfg.getInputSampleRate();
        m_config.m_rfBandwidth = cfg.getRFBandwidth();
        m_config.m_fmDeviation = cfg.getFMDeviation();
        m_config.m_udpAddressStr = cfg.getUDPAddress();
        m_config.m_udpPort = cfg.getUDPPort();
        m_config.m_channelMute = cfg.getChannelMute();
        m_config.m_volume = cfg.getVolume();
        m_config.m_squelch = CalcDb::powerFromdB(cfg.getSquelchDB());

        apply(cfg.getForce());

        qDebug() << "UDPSink::handleMessage: MsgUDPSinkConfigure:"
                << " m_sampleFormat: " << m_config.m_sampleFormat
                << " m_inputSampleRate: " << m_config.m_inputSampleRate
                << " m_rfBandwidth: " << m_config.m_rfBandwidth
                << " m_fmDeviation: " << m_config.m_fmDeviation
                << " m_udpAddressStr: " << m_config.m_udpAddressStr
                << " m_udpPort: " << m_config.m_udpPort
                << " m_channelMute: " << m_config.m_channelMute
                << " m_volume: " << m_config.m_volume
                << " squelchDB: " << cfg.getSquelchDB()
                << " m_squelch: " << m_config.m_squelch;

        return true;
    }
    else if (UDPSinkMessages::MsgSampleRateCorrection::match(cmd))
    {
        UDPSinkMessages::MsgSampleRateCorrection& cfg = (UDPSinkMessages::MsgSampleRateCorrection&) cmd;
        Real newSampleRate = m_actualInputSampleRate + cfg.getCorrectionFactor() * m_actualInputSampleRate;

        // exclude values too way out nominal sample rate (20%)
        if ((newSampleRate < m_running.m_inputSampleRate * 1.2) && (newSampleRate >  m_running.m_inputSampleRate * 0.8))
        {
            m_actualInputSampleRate = newSampleRate;

            if ((cfg.getRawDeltaRatio() > -0.05) || (cfg.getRawDeltaRatio() < 0.05))
            {
                if (m_sampleRateAvgCounter < m_sampleRateAverageItems)
                {
                    m_sampleRateSum += m_actualInputSampleRate;
                    m_sampleRateAvgCounter++;
                }
            }
            else
            {
                m_sampleRateSum = 0.0;
                m_sampleRateAvgCounter = 0;
            }

            if (m_sampleRateAvgCounter == m_sampleRateAverageItems)
            {
                float avgRate = m_sampleRateSum / m_sampleRateAverageItems;
                qDebug("UDPSink::handleMessage: MsgSampleRateCorrection: corr: %+.6f new rate: %.0f: avg rate: %.0f",
                        cfg.getCorrectionFactor(),
                        m_actualInputSampleRate,
                        avgRate);
                m_actualInputSampleRate = avgRate;
                m_sampleRateSum = 0.0;
                m_sampleRateAvgCounter = 0;
            }
//            else
//            {
//                qDebug("UDPSink::handleMessage: MsgSampleRateCorrection: corr: %+.6f new rate: %.0f",
//                        cfg.getCorrectionFactor(),
//                        m_actualInputSampleRate);
//            }

            m_settingsMutex.lock();
            m_interpolatorDistanceRemain = 0;
            m_interpolatorConsumed = false;
            m_interpolatorDistance = (Real) m_actualInputSampleRate / (Real) m_config.m_outputSampleRate;
            //m_interpolator.create(48, m_actualInputSampleRate, m_config.m_rfBandwidth / 2.2, 3.0); // causes clicking: leaving at standard frequency
            m_settingsMutex.unlock();
        }

        return true;
    }
    else if (MsgUDPSinkSpectrum::match(cmd))
    {
        MsgUDPSinkSpectrum& spc = (MsgUDPSinkSpectrum&) cmd;
        m_spectrumEnabled = spc.getEnabled();
        qDebug() << "UDPSink::handleMessage: MsgUDPSinkSpectrum: m_spectrumEnabled: " << m_spectrumEnabled;

        return true;
    }
    else
    {
        if(m_spectrum != 0)
        {
           return m_spectrum->handleMessage(cmd);
        }
        else
        {
            return false;
        }
    }
}

void UDPSink::configure(MessageQueue* messageQueue,
        SampleFormat sampleFormat,
        Real outputSampleRate,
        Real rfBandwidth,
        int fmDeviation,
        QString& udpAddress,
        int udpPort,
        bool channelMute,
        Real volume,
        Real squelchDB,
        bool force)
{
    Message* cmd = MsgUDPSinkConfigure::create(sampleFormat,
            outputSampleRate,
            rfBandwidth,
            fmDeviation,
            udpAddress,
            udpPort,
            channelMute,
            volume,
            squelchDB,
            force);
    messageQueue->push(cmd);
}

void UDPSink::setSpectrum(MessageQueue* messageQueue, bool enabled)
{
    Message* cmd = MsgUDPSinkSpectrum::create(enabled);
    messageQueue->push(cmd);
}


void UDPSink::apply(bool force)
{
    if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
        (m_config.m_outputSampleRate != m_running.m_outputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_carrierNco.setFreq(m_config.m_inputFrequencyOffset, m_config.m_outputSampleRate);
        m_settingsMutex.unlock();
    }

    if((m_config.m_outputSampleRate != m_running.m_outputSampleRate) ||
       (m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
       (m_config.m_inputSampleRate != m_running.m_inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_config.m_inputSampleRate / (Real) m_config.m_outputSampleRate;
        m_interpolator.create(48, m_config.m_inputSampleRate, m_config.m_rfBandwidth / 2.2, 3.0);
        m_actualInputSampleRate = m_config.m_inputSampleRate;
        m_udpHandler.resetReadIndex();
        m_sampleRateSum = 0.0;
        m_sampleRateAvgCounter = 0;
        m_spectrumChunkSize = m_config.m_inputSampleRate * 0.05; // 50 ms chunk
        m_spectrumChunkCounter = 0;
        m_levelNbSamples = m_config.m_inputSampleRate * 0.01; // every 10 ms
        m_levelCalcCount = 0;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_udpHandler.resizeBuffer(m_config.m_inputSampleRate);
        m_inMovingAverage.resize(m_config.m_inputSampleRate * 0.01, 1e-10); // 10 ms
        m_squelchThreshold = m_config.m_inputSampleRate * 0.1; // 100 ms
        m_settingsMutex.unlock();
    }

    if ((m_config.m_udpAddressStr != m_running.m_udpAddressStr) ||
        (m_config.m_udpPort != m_running.m_udpPort) || force)
    {
        m_settingsMutex.lock();
        m_udpHandler.configureUDPLink(m_config.m_udpAddressStr, m_config.m_udpPort);
        m_settingsMutex.unlock();
    }

    if ((m_config.m_channelMute != m_running.m_channelMute) || force)
    {
        if (!m_config.m_channelMute) {
            m_udpHandler.resetReadIndex();
        }
    }

    m_running = m_config;
}
