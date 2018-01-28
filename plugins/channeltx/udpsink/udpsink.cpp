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

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "util/db.h"

#include "udpsinkmsg.h"
#include "udpsink.h"

MESSAGE_CLASS_DEFINITION(UDPSink::MsgConfigureUDPSink, Message)
MESSAGE_CLASS_DEFINITION(UDPSink::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(UDPSink::MsgUDPSinkSpectrum, Message)
MESSAGE_CLASS_DEFINITION(UDPSink::MsgResetReadIndex, Message)

const QString UDPSink::m_channelIdURI = "sdrangel.channeltx.udpsink";
const QString UDPSink::m_channelId = "UDPSink";

UDPSink::UDPSink(DeviceSinkAPI *deviceAPI) :
    ChannelSourceAPI(m_channelIdURI),
    m_deviceAPI(deviceAPI),
    m_basebandSampleRate(48000),
    m_outputSampleRate(48000),
    m_inputFrequencyOffset(0),
    m_squelch(1e-6),
    m_spectrum(0),
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
    m_modPhasor(0.0f),
    m_SSBFilterBufferIndex(0),
    m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

    m_udpHandler.setFeedbackMessageQueue(&m_inputMessageQueue);
    m_SSBFilter = new fftfilt(m_settings.m_lowCutoff / m_settings.m_inputSampleRate, m_settings.m_rfBandwidth / m_settings.m_inputSampleRate, m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applyChannelSettings(m_basebandSampleRate, m_outputSampleRate, m_inputFrequencyOffset, true);
    applySettings(m_settings, true);
}

UDPSink::~UDPSink()
{
    delete[] m_SSBFilterBuffer;
    delete m_SSBFilter;
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void UDPSink::start()
{
    m_udpHandler.start();
    applyChannelSettings(m_basebandSampleRate, m_outputSampleRate, m_inputFrequencyOffset, true);
}

void UDPSink::stop()
{
    m_udpHandler.stop();
}

void UDPSink::pull(Sample& sample)
{
    if (m_settings.m_channelMute)
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
    magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
    m_movingAverage.feed(magsq);
    m_magsq = m_movingAverage.average();

    sample.m_real = (FixReal) ci.real();
    sample.m_imag = (FixReal) ci.imag();
}

void UDPSink::modulateSample()
{
    if (m_settings.m_sampleFormat == UDPSinkSettings::FormatS16LE) // Linear I/Q transponding
    {
        Sample s;

        m_udpHandler.readSample(s);

        uint64_t magsq = s.m_real * s.m_real + s.m_imag * s.m_imag;
        m_inMovingAverage.feed(magsq/(SDR_TX_SCALED*SDR_TX_SCALED));
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            m_modSample.real(s.m_real * m_settings.m_gainOut);
            m_modSample.imag(s.m_imag * m_settings.m_gainOut);
            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatNFM)
    {
        FixReal t;
        readMonoSample(t);

        m_inMovingAverage.feed((t*t)/1073741824.0);
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            m_modPhasor += (m_settings.m_fmDeviation / m_settings.m_inputSampleRate) * (t / SDR_TX_SCALEF) * M_PI * 2.0f;
            m_modSample.real(cos(m_modPhasor) * 0.3162292f * SDR_TX_SCALEF * m_settings.m_gainOut);
            m_modSample.imag(sin(m_modPhasor) * 0.3162292f * SDR_TX_SCALEF * m_settings.m_gainOut);
            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatAM)
    {
        FixReal t;
        readMonoSample(t);
        m_inMovingAverage.feed((t*t)/(SDR_TX_SCALED*SDR_TX_SCALED));
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            m_modSample.real(((t / SDR_TX_SCALEF)*m_settings.m_amModFactor*m_settings.m_gainOut + 1.0f) * (SDR_TX_SCALEF/2)); // modulate and scale zero frequency carrier
            m_modSample.imag(0.0f);
            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else if ((m_settings.m_sampleFormat == UDPSinkSettings::FormatLSB) || (m_settings.m_sampleFormat == UDPSinkSettings::FormatUSB))
    {
        FixReal t;
        Complex c, ci;
        fftfilt::cmplx *filtered;
        int n_out = 0;

        readMonoSample(t);
        m_inMovingAverage.feed((t*t)/(SDR_TX_SCALED*SDR_TX_SCALED));
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            ci.real((t / SDR_TX_SCALEF) * m_settings.m_gainOut);
            ci.imag(0.0f);

            n_out = m_SSBFilter->runSSB(ci, &filtered, (m_settings.m_sampleFormat == UDPSinkSettings::FormatUSB));

            if (n_out > 0)
            {
                memcpy((void *) m_SSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
                m_SSBFilterBufferIndex = 0;
            }

            c = m_SSBFilterBuffer[m_SSBFilterBufferIndex];
            m_modSample.real(m_SSBFilterBuffer[m_SSBFilterBufferIndex].real() * SDR_TX_SCALEF);
            m_modSample.imag(m_SSBFilterBuffer[m_SSBFilterBufferIndex].imag() * SDR_TX_SCALEF);
            m_SSBFilterBufferIndex++;

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
    else if (m_spectrum)
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
        qreal rmsLevel = m_levelSum > 0.0 ? sqrt((m_levelSum/(SDR_TX_SCALED*SDR_TX_SCALED)) / m_levelNbSamples) : 0.0;
        emit levelChanged(rmsLevel, m_peakLevel / SDR_TX_SCALEF, m_levelNbSamples);
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
        qDebug() << "UDPSink::handleMessage: MsgChannelizerNotification";

        applyChannelSettings(notif.getBasebandSampleRate(), notif.getSampleRate(), notif.getFrequencyOffset());

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "UDPSink::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureUDPSink::match(cmd))
    {
        MsgConfigureUDPSink& cfg = (MsgConfigureUDPSink&) cmd;
        qDebug() << "UDPSink::handleMessage: MsgConfigureUDPSink";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (UDPSinkMessages::MsgSampleRateCorrection::match(cmd))
    {
        UDPSinkMessages::MsgSampleRateCorrection& cfg = (UDPSinkMessages::MsgSampleRateCorrection&) cmd;
        Real newSampleRate = m_actualInputSampleRate + cfg.getCorrectionFactor() * m_actualInputSampleRate;

        // exclude values too way out nominal sample rate (20%)
        if ((newSampleRate < m_settings.m_inputSampleRate * 1.2) && (newSampleRate >  m_settings.m_inputSampleRate * 0.8))
        {
            m_actualInputSampleRate = newSampleRate;

            if ((cfg.getRawDeltaRatio() > -0.05) && (cfg.getRawDeltaRatio() < 0.05))
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
            m_interpolatorDistance = (Real) m_actualInputSampleRate / (Real) m_outputSampleRate;
            //m_interpolator.create(48, m_actualInputSampleRate, m_settings.m_rfBandwidth / 2.2, 3.0); // causes clicking: leaving at standard frequency
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
    else if (MsgResetReadIndex::match(cmd))
    {
        m_settingsMutex.lock();
        m_udpHandler.resetReadIndex();
        m_settingsMutex.unlock();

        qDebug() << "UDPSink::handleMessage: MsgResetReadIndex";

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

void UDPSink::setSpectrum(bool enabled)
{
    Message* cmd = MsgUDPSinkSpectrum::create(enabled);
    getInputMessageQueue()->push(cmd);
}

void UDPSink::resetReadIndex()
{
    Message* cmd = MsgResetReadIndex::create();
    getInputMessageQueue()->push(cmd);
}

void UDPSink::applyChannelSettings(int basebandSampleRate, int outputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "UDPSink::applyChannelSettings:"
            << " basebandSampleRate: " << basebandSampleRate
            << " outputSampleRate: " << outputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((inputFrequencyOffset != m_inputFrequencyOffset) ||
        (outputSampleRate != m_outputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_carrierNco.setFreq(inputFrequencyOffset, outputSampleRate);
        m_settingsMutex.unlock();
    }

    if (((outputSampleRate != m_outputSampleRate) && (!m_settings.m_autoRWBalance)) || force)
    {
        m_settingsMutex.lock();
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_settings.m_inputSampleRate / (Real) outputSampleRate;
        m_interpolator.create(48, m_settings.m_inputSampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
        m_settingsMutex.unlock();
    }

    m_basebandSampleRate = basebandSampleRate;
    m_outputSampleRate = outputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void UDPSink::applySettings(const UDPSinkSettings& settings, bool force)
{
    qDebug() << "UDPSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_sampleFormat: " << settings.m_sampleFormat
            << " m_inputSampleRate: " << settings.m_inputSampleRate
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_udpAddressStr: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_channelMute: " << settings.m_channelMute
            << " m_gainIn: " << settings.m_gainIn
            << " m_gainOut: " << settings.m_gainOut
            << " m_squelchGate: " << settings.m_squelchGate
            << " m_squelch: " << settings.m_squelch << "dB"
            << " m_squelchEnabled: " << settings.m_squelchEnabled
            << " m_autoRWBalance: " << settings.m_autoRWBalance
            << " m_stereoInput: " << settings.m_stereoInput
            << " force: " << force;

    if((settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
       (settings.m_inputSampleRate != m_settings.m_inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) settings.m_inputSampleRate / (Real) m_outputSampleRate;
        m_interpolator.create(48, settings.m_inputSampleRate, settings.m_rfBandwidth / 2.2, 3.0);
        m_actualInputSampleRate = settings.m_inputSampleRate;
        m_udpHandler.resetReadIndex();
        m_sampleRateSum = 0.0;
        m_sampleRateAvgCounter = 0;
        m_spectrumChunkSize = settings.m_inputSampleRate * 0.05; // 50 ms chunk
        m_spectrumChunkCounter = 0;
        m_levelNbSamples = settings.m_inputSampleRate * 0.01; // every 10 ms
        m_levelCalcCount = 0;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_udpHandler.resizeBuffer(settings.m_inputSampleRate);
        m_inMovingAverage.resize(settings.m_inputSampleRate * 0.01, 1e-10); // 10 ms
        m_squelchThreshold = settings.m_inputSampleRate * settings.m_squelchGate;
        initSquelch(m_squelchOpen);
        m_SSBFilter->create_filter(settings.m_lowCutoff / settings.m_inputSampleRate, settings.m_rfBandwidth / settings.m_inputSampleRate);
        m_settingsMutex.unlock();
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        m_squelch = CalcDb::powerFromdB(settings.m_squelch);
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        m_squelchThreshold = m_outputSampleRate * settings.m_squelchGate;
        initSquelch(m_squelchOpen);
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress) ||
        (settings.m_udpPort != m_settings.m_udpPort) || force)
    {
        m_settingsMutex.lock();
        m_udpHandler.configureUDPLink(settings.m_udpAddress, settings.m_udpPort);
        m_settingsMutex.unlock();
    }

    if ((settings.m_channelMute != m_settings.m_channelMute) || force)
    {
        if (!settings.m_channelMute) {
            m_udpHandler.resetReadIndex();
        }
    }

    if ((settings.m_autoRWBalance != m_settings.m_autoRWBalance) || force)
    {
        m_settingsMutex.lock();
        m_udpHandler.setAutoRWBalance(settings.m_autoRWBalance);

        if (!settings.m_autoRWBalance)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorConsumed = false;
            m_interpolatorDistance = (Real) settings.m_inputSampleRate / (Real) m_outputSampleRate;
            m_interpolator.create(48, settings.m_inputSampleRate, settings.m_rfBandwidth / 2.2, 3.0);
            m_actualInputSampleRate = settings.m_inputSampleRate;
            m_udpHandler.resetReadIndex();
        }

        m_settingsMutex.unlock();
    }

    m_settings = settings;
}

QByteArray UDPSink::serialize() const
{
    return m_settings.serialize();
}

bool UDPSink::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureUDPSink *msg = MsgConfigureUDPSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureUDPSink *msg = MsgConfigureUDPSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}
