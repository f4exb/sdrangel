///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGUDPSourceReport.h"

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "dsp/dspcommands.h"
#include "util/db.h"

#include "udpsource.h"
#include "udpsourcemsg.h"

MESSAGE_CLASS_DEFINITION(UDPSource::MsgConfigureUDPSource, Message)
MESSAGE_CLASS_DEFINITION(UDPSource::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(UDPSource::MsgUDPSourceSpectrum, Message)
MESSAGE_CLASS_DEFINITION(UDPSource::MsgResetReadIndex, Message)

const QString UDPSource::m_channelIdURI = "sdrangel.channeltx.udpsource";
const QString UDPSource::m_channelId = "UDPSource";

UDPSource::UDPSource(DeviceSinkAPI *deviceAPI) :
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

    applyChannelSettings(m_basebandSampleRate, m_outputSampleRate, m_inputFrequencyOffset, true);
    applySettings(m_settings, true);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

UDPSource::~UDPSource()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete m_SSBFilter;
    delete[] m_SSBFilterBuffer;
}

void UDPSource::start()
{
    m_udpHandler.start();
    applyChannelSettings(m_basebandSampleRate, m_outputSampleRate, m_inputFrequencyOffset, true);
}

void UDPSource::stop()
{
    m_udpHandler.stop();
}

void UDPSource::pull(Sample& sample)
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

void UDPSource::modulateSample()
{
    if (m_settings.m_sampleFormat == UDPSourceSettings::FormatSnLE) // Linear I/Q transponding
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
    else if (m_settings.m_sampleFormat == UDPSourceSettings::FormatNFM)
    {
        qint16 t;
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
    else if (m_settings.m_sampleFormat == UDPSourceSettings::FormatAM)
    {
        qint16 t;
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
    else if ((m_settings.m_sampleFormat == UDPSourceSettings::FormatLSB) || (m_settings.m_sampleFormat == UDPSourceSettings::FormatUSB))
    {
        qint16 t;
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

            n_out = m_SSBFilter->runSSB(ci, &filtered, (m_settings.m_sampleFormat == UDPSourceSettings::FormatUSB));

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

void UDPSource::calculateLevel(Real sample)
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

void UDPSource::calculateLevel(Complex sample)
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

bool UDPSource::handleMessage(const Message& cmd)
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
        qDebug() << "UDPSource::handleMessage: MsgChannelizerNotification";

        applyChannelSettings(notif.getBasebandSampleRate(), notif.getSampleRate(), notif.getFrequencyOffset());

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "UDPSource::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureUDPSource::match(cmd))
    {
        MsgConfigureUDPSource& cfg = (MsgConfigureUDPSource&) cmd;
        qDebug() << "UDPSource::handleMessage: MsgConfigureUDPSource";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (UDPSourceMessages::MsgSampleRateCorrection::match(cmd))
    {
        UDPSourceMessages::MsgSampleRateCorrection& cfg = (UDPSourceMessages::MsgSampleRateCorrection&) cmd;
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
                qDebug("UDPSource::handleMessage: MsgSampleRateCorrection: corr: %+.6f new rate: %.0f: avg rate: %.0f",
                        cfg.getCorrectionFactor(),
                        m_actualInputSampleRate,
                        avgRate);
                m_actualInputSampleRate = avgRate;
                m_sampleRateSum = 0.0;
                m_sampleRateAvgCounter = 0;
            }
//            else
//            {
//                qDebug("UDPSource::handleMessage: MsgSampleRateCorrection: corr: %+.6f new rate: %.0f",
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
    else if (MsgUDPSourceSpectrum::match(cmd))
    {
        MsgUDPSourceSpectrum& spc = (MsgUDPSourceSpectrum&) cmd;
        m_spectrumEnabled = spc.getEnabled();
        qDebug() << "UDPSource::handleMessage: MsgUDPSourceSpectrum: m_spectrumEnabled: " << m_spectrumEnabled;

        return true;
    }
    else if (MsgResetReadIndex::match(cmd))
    {
        m_settingsMutex.lock();
        m_udpHandler.resetReadIndex();
        m_settingsMutex.unlock();

        qDebug() << "UDPSource::handleMessage: MsgResetReadIndex";

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
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

void UDPSource::setSpectrum(bool enabled)
{
    Message* cmd = MsgUDPSourceSpectrum::create(enabled);
    getInputMessageQueue()->push(cmd);
}

void UDPSource::resetReadIndex()
{
    Message* cmd = MsgResetReadIndex::create();
    getInputMessageQueue()->push(cmd);
}

void UDPSource::applyChannelSettings(int basebandSampleRate, int outputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "UDPSource::applyChannelSettings:"
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

void UDPSource::applySettings(const UDPSourceSettings& settings, bool force)
{
    qDebug() << "UDPSource::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_sampleFormat: " << settings.m_sampleFormat
            << " m_inputSampleRate: " << settings.m_inputSampleRate
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_amModFactor: " << settings.m_amModFactor
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

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_sampleFormat != m_settings.m_sampleFormat) || force) {
        reverseAPIKeys.append("sampleFormat");
    }
    if ((settings.m_inputSampleRate != m_settings.m_inputSampleRate) || force) {
        reverseAPIKeys.append("inputSampleRate");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_lowCutoff != m_settings.m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_amModFactor != m_settings.m_amModFactor) || force) {
        reverseAPIKeys.append("amModFactor");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }
    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
    }
    if ((settings.m_gainIn != m_settings.m_gainIn) || force) {
        reverseAPIKeys.append("gainIn");
    }
    if ((settings.m_gainOut != m_settings.m_gainOut) || force) {
        reverseAPIKeys.append("gainOut");
    }
    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force) {
        reverseAPIKeys.append("squelchGate");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_squelchEnabled != m_settings.m_squelchEnabled) || force) {
        reverseAPIKeys.append("squelchEnabled");
    }
    if ((settings.m_autoRWBalance != m_settings.m_autoRWBalance) || force) {
        reverseAPIKeys.append("autoRWBalance");
    }
    if ((settings.m_stereoInput != m_settings.m_stereoInput) || force) {
        reverseAPIKeys.append("stereoInput");
    }

    if((settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
       (settings.m_lowCutoff != m_settings.m_lowCutoff) ||
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

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

QByteArray UDPSource::serialize() const
{
    return m_settings.serialize();
}

bool UDPSource::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int UDPSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSourceSettings(new SWGSDRangel::SWGUDPSourceSettings());
    response.getUdpSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int UDPSource::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    UDPSourceSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("sampleFormat")) {
        settings.m_sampleFormat = (UDPSourceSettings::SampleFormat) response.getUdpSourceSettings()->getSampleFormat();
    }
    if (channelSettingsKeys.contains("inputSampleRate")) {
        settings.m_inputSampleRate = response.getUdpSourceSettings()->getInputSampleRate();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getUdpSourceSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getUdpSourceSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getUdpSourceSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getUdpSourceSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("amModFactor")) {
        settings.m_amModFactor = response.getUdpSourceSettings()->getAmModFactor();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getUdpSourceSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("gainIn")) {
        settings.m_gainIn = response.getUdpSourceSettings()->getGainIn();
    }
    if (channelSettingsKeys.contains("gainOut")) {
        settings.m_gainOut = response.getUdpSourceSettings()->getGainOut();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getUdpSourceSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getUdpSourceSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("squelchEnabled")) {
        settings.m_squelchEnabled = response.getUdpSourceSettings()->getSquelchEnabled() != 0;
    }
    if (channelSettingsKeys.contains("autoRWBalance")) {
        settings.m_autoRWBalance = response.getUdpSourceSettings()->getAutoRwBalance() != 0;
    }
    if (channelSettingsKeys.contains("stereoInput")) {
        settings.m_stereoInput = response.getUdpSourceSettings()->getStereoInput() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getUdpSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getUdpSourceSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getUdpSourceSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getUdpSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getUdpSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getUdpSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getUdpSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getUdpSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getUdpSourceSettings()->getReverseApiChannelIndex();
    }

    if (frequencyOffsetChanged)
    {
        UDPSource::MsgConfigureChannelizer *msgChan = UDPSource::MsgConfigureChannelizer::create(
                settings.m_inputSampleRate,
                settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(msgChan);
    }

    MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUDPSource *msgToGUI = MsgConfigureUDPSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int UDPSource::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSourceReport(new SWGSDRangel::SWGUDPSourceReport());
    response.getUdpSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void UDPSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const UDPSourceSettings& settings)
{
    response.getUdpSourceSettings()->setSampleFormat((int) settings.m_sampleFormat);
    response.getUdpSourceSettings()->setInputSampleRate(settings.m_inputSampleRate);
    response.getUdpSourceSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getUdpSourceSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getUdpSourceSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getUdpSourceSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getUdpSourceSettings()->setAmModFactor(settings.m_amModFactor);
    response.getUdpSourceSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getUdpSourceSettings()->setGainIn(settings.m_gainIn);
    response.getUdpSourceSettings()->setGainOut(settings.m_gainOut);
    response.getUdpSourceSettings()->setSquelch(settings.m_squelch);
    response.getUdpSourceSettings()->setSquelchGate(settings.m_squelchGate);
    response.getUdpSourceSettings()->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    response.getUdpSourceSettings()->setAutoRwBalance(settings.m_autoRWBalance ? 1 : 0);
    response.getUdpSourceSettings()->setStereoInput(settings.m_stereoInput ? 1 : 0);
    response.getUdpSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getUdpSourceSettings()->getUdpAddress()) {
        *response.getUdpSourceSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getUdpSourceSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getUdpSourceSettings()->setUdpPort(settings.m_udpPort);

    if (response.getUdpSourceSettings()->getTitle()) {
        *response.getUdpSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getUdpSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getUdpSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getUdpSourceSettings()->getReverseApiAddress()) {
        *response.getUdpSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUdpSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getUdpSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUdpSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getUdpSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void UDPSource::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getUdpSourceReport()->setInputPowerDb(CalcDb::dbPower(getInMagSq()));
    response.getUdpSourceReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getUdpSourceReport()->setSquelch(m_squelchOpen ? 1 : 0);
    response.getUdpSourceReport()->setBufferGauge(getBufferGauge());
    response.getUdpSourceReport()->setChannelSampleRate(m_outputSampleRate);
}

void UDPSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("UDPSource"));
    swgChannelSettings->setUdpSourceSettings(new SWGSDRangel::SWGUDPSourceSettings());
    SWGSDRangel::SWGUDPSourceSettings *swgUDPSourceSettings = swgChannelSettings->getUdpSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("sampleFormat") || force) {
        swgUDPSourceSettings->setSampleFormat((int) settings.m_sampleFormat);
    }
    if (channelSettingsKeys.contains("inputSampleRate") || force) {
        swgUDPSourceSettings->setInputSampleRate(settings.m_inputSampleRate);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgUDPSourceSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgUDPSourceSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("lowCutoff") || force) {
        swgUDPSourceSettings->setLowCutoff(settings.m_lowCutoff);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgUDPSourceSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("amModFactor") || force) {
        swgUDPSourceSettings->setAmModFactor(settings.m_amModFactor);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgUDPSourceSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("gainIn") || force) {
        swgUDPSourceSettings->setGainIn(settings.m_gainIn);
    }
    if (channelSettingsKeys.contains("gainOut") || force) {
        swgUDPSourceSettings->setGainOut(settings.m_gainOut);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgUDPSourceSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("squelchGate") || force) {
        swgUDPSourceSettings->setSquelchGate(settings.m_squelchGate);
    }
    if (channelSettingsKeys.contains("squelchEnabled") || force) {
        swgUDPSourceSettings->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("autoRWBalance") || force) {
        swgUDPSourceSettings->setAutoRwBalance(settings.m_autoRWBalance ? 1 : 0);
    }
    if (channelSettingsKeys.contains("stereoInput") || force) {
        swgUDPSourceSettings->setStereoInput(settings.m_stereoInput ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgUDPSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgUDPSourceSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgUDPSourceSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgUDPSourceSettings->setTitle(new QString(settings.m_title));
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void UDPSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "UDPSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("UDPSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
