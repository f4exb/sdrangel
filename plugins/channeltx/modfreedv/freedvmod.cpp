///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "freedvmod.h"

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "codec2/freedv_api.h"

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGFreeDVModReport.h"

#include "dsp/upchannelizer.h"
#include "dsp/dspengine.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "dsp/dspcommands.h"
#include "device/devicesinkapi.h"
#include "util/db.h"

MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFreeDVMod, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(FreeDVMod::MsgReportFileSourceStreamTiming, Message)

const QString FreeDVMod::m_channelIdURI = "sdrangel.channeltx.freedvmod";
const QString FreeDVMod::m_channelId = "FreeDVMod";
const int FreeDVMod::m_levelNbSamples = 480; // every 10ms
const int FreeDVMod::m_ssbFftLen = 1024;

FreeDVMod::FreeDVMod(DeviceSinkAPI *deviceAPI) :
    ChannelSourceAPI(m_channelIdURI),
    m_deviceAPI(deviceAPI),
    m_basebandSampleRate(48000),
    m_outputSampleRate(48000),
    m_modemSampleRate(48000), // // default 2400A mode
    m_inputFrequencyOffset(0),
    m_lowCutoff(0.0),
    m_hiCutoff(6000.0),
    m_SSBFilter(0),
	m_SSBFilterBuffer(0),
	m_SSBFilterBufferIndex(0),
    m_sampleSink(0),
    m_audioFifo(4800),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_inputSampleRate(8000), // all modes take 8000 S/s input
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
	m_freeDV(0),
	m_nSpeechSamples(0),
	m_nNomModemSamples(0),
	m_iSpeech(0),
	m_iModem(0),
	m_speechIn(0),
	m_modOut(0),
	m_scaleFactor(SDR_TX_SCALEF)
{
	setObjectName(m_channelId);

	DSPEngine::instance()->getAudioDeviceManager()->addAudioSource(&m_audioFifo, getInputMessageQueue());
    m_audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getInputSampleRate();

    m_SSBFilter = new fftfilt(m_lowCutoff / m_audioSampleRate, m_hiCutoff / m_audioSampleRate, m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size
    std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer+(m_ssbFftLen>>1), Complex{0,0});

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

    m_sum.real(0.0f);
    m_sum.imag(0.0f);
    m_undersampleCount = 0;
    m_sumCount = 0;

	m_magsq = 0.0;

	m_toneNco.setFreq(1000.0, m_inputSampleRate);

	// CW keyer
	m_cwKeyer.setSampleRate(m_inputSampleRate);
	m_cwKeyer.setWPM(13);
	m_cwKeyer.setMode(CWKeyerSettings::CWNone);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applySettings(m_settings, true);
    applyChannelSettings(m_basebandSampleRate, m_outputSampleRate, m_inputFrequencyOffset, true);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

FreeDVMod::~FreeDVMod()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;

    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSource(&m_audioFifo);

    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;

    delete m_SSBFilter;
    delete[] m_SSBFilterBuffer;

    if (m_freeDV) {
        freedv_close(m_freeDV);
    }
}

void FreeDVMod::pull(Sample& sample)
{
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
    ci *= 0.891235351562f * SDR_TX_SCALEF; //scaling at -1 dB to account for possible filter overshoot

    m_settingsMutex.unlock();

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
	m_movingAverage(magsq);
	m_magsq = m_movingAverage.asDouble();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void FreeDVMod::pullAudio(int nbSamples)
{
    unsigned int nbSamplesAudio = nbSamples * ((Real) m_audioSampleRate / (Real) m_modemSampleRate);

    if (nbSamplesAudio > m_audioBuffer.size())
    {
        m_audioBuffer.resize(nbSamplesAudio);
    }

    m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioBuffer[0]), nbSamplesAudio);
    m_audioBufferFill = 0;
}

void FreeDVMod::modulateSample()
{
    pullAF(m_modSample);
    if (!m_settings.m_gaugeInputElseModem) {
        calculateLevel(m_modSample);
    }
    m_audioBufferFill++;
}

void FreeDVMod::pullAF(Complex& sample)
{
	if (m_settings.m_audioMute)
	{
        sample.real(0.0f);
        sample.imag(0.0f);
        return;
	}

    Complex ci;
    fftfilt::cmplx *filtered;
    int n_out = 0;

    int decim = 1<<(m_settings.m_spanLog2 - 1);
    unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    if (m_iModem >= m_nNomModemSamples)
    {
        switch (m_settings.m_modAFInput)
        {
        case FreeDVModSettings::FreeDVModInputTone:
            for (int i = 0; i < m_nSpeechSamples; i++)
            {
                m_speechIn[i] = m_toneNco.next() * 32768.0f * m_settings.m_volumeFactor;
                if (m_settings.m_gaugeInputElseModem) {
                    calculateLevel(m_speechIn[i]);
                }
            }
            freedv_tx(m_freeDV, m_modOut, m_speechIn);
            break;
        case FreeDVModSettings::FreeDVModInputFile:
            if (m_iModem >= m_nNomModemSamples)
            {
                if (m_ifstream.is_open())
                {
                    std::fill(m_speechIn, m_speechIn + m_nSpeechSamples, 0);

                    if (m_ifstream.eof())
                    {
                        if (m_settings.m_playLoop)
                        {
                            m_ifstream.clear();
                            m_ifstream.seekg(0, std::ios::beg);
                        }
                    }

                    if (m_ifstream.eof())
                    {
                        std::fill(m_modOut, m_modOut + m_nNomModemSamples, 0);
                    }
                    else
                    {

                        m_ifstream.read(reinterpret_cast<char*>(m_speechIn), sizeof(int16_t) * m_nSpeechSamples);

                        if ((m_settings.m_volumeFactor != 1.0) || m_settings.m_gaugeInputElseModem)
                        {
                            for (int i = 0; i < m_nSpeechSamples; i++)
                            {
                                if (m_settings.m_volumeFactor != 1.0) {
                                    m_speechIn[i] *= m_settings.m_volumeFactor;
                                }
                                if (m_settings.m_gaugeInputElseModem) {
                                    calculateLevel(m_speechIn[i]);
                                }
                            }
                        }

                        freedv_tx(m_freeDV, m_modOut, m_speechIn);
                    }
                }
                else
                {
                    std::fill(m_modOut, m_modOut + m_nNomModemSamples, 0);
                }
            }
            break;
        case FreeDVModSettings::FreeDVModInputAudio:
            for (int i = 0; i < m_nSpeechSamples; i++)
            {
                qint16 audioSample = (m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) * (m_settings.m_volumeFactor / 2.0f);
                m_audioBufferFill++;

                while (!m_audioResampler.downSample(audioSample, m_speechIn[i]))
                {
                    audioSample = (m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) * (m_settings.m_volumeFactor / 2.0f);
                    m_audioBufferFill++;
                }

                if (m_settings.m_gaugeInputElseModem) {
                    calculateLevel(m_speechIn[i]);
                }
            }
            freedv_tx(m_freeDV, m_modOut, m_speechIn);
            break;
        case FreeDVModSettings::FreeDVModInputCWTone:
            for (int i = 0; i < m_nSpeechSamples; i++)
            {
                Real fadeFactor;

                if (m_cwKeyer.getSample())
                {
                    m_cwKeyer.getCWSmoother().getFadeSample(true, fadeFactor);
                    m_speechIn[i] = m_toneNco.next() * 32768.0f * fadeFactor * m_settings.m_volumeFactor;
                }
                else
                {
                    if (m_cwKeyer.getCWSmoother().getFadeSample(false, fadeFactor))
                    {
                        m_speechIn[i] = m_toneNco.next() * 32768.0f * fadeFactor * m_settings.m_volumeFactor;
                    }
                    else
                    {
                        m_speechIn[i] = 0;
                        m_toneNco.setPhase(0);
                    }
                }

                if (m_settings.m_gaugeInputElseModem) {
                    calculateLevel(m_speechIn[i]);
                }
            }
            freedv_tx(m_freeDV, m_modOut, m_speechIn);
            break;
        case FreeDVModSettings::FreeDVModInputNone:
        default:
            std::fill(m_speechIn, m_speechIn + m_nSpeechSamples, 0);
            freedv_tx(m_freeDV, m_modOut, m_speechIn);
            break;
        }

        m_iModem = 0;
    }

    ci.real(m_modOut[m_iModem++] / m_scaleFactor);
    ci.imag(0.0f);

    n_out = m_SSBFilter->runSSB(ci, &filtered, true); // USB

    if (n_out > 0)
    {
        memcpy((void *) m_SSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
        m_SSBFilterBufferIndex = 0;

        for (int i = 0; i < n_out; i++)
        {
            // Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
            // smart decimation with bit gain using float arithmetic (23 bits significand)

            m_sum += filtered[i];

            if (!(m_undersampleCount++ & decim_mask))
            {
                Real avgr = (m_sum.real() / decim) * 0.891235351562f * SDR_TX_SCALEF; //scaling at -1 dB to account for possible filter overshoot
                Real avgi = (m_sum.imag() / decim) * 0.891235351562f * SDR_TX_SCALEF;
                m_sampleBuffer.push_back(Sample(avgr, avgi));
                m_sum.real(0.0);
                m_sum.imag(0.0);
            }
        }

        if (m_sampleSink != 0)
        {
            m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true); // SSB
        }

        m_sampleBuffer.clear();
    }

    sample = m_SSBFilterBuffer[m_SSBFilterBufferIndex++];
}

void FreeDVMod::calculateLevel(Complex& sample)
{
    Real t = sample.real(); // TODO: possibly adjust depending on sample type

    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), t);
        m_levelSum += t * t;
        m_levelCalcCount++;
    }
    else
    {
        qreal rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        //qDebug("NFMMod::calculateLevel: %f %f", rmsLevel, m_peakLevel);
        emit levelChanged(rmsLevel, m_peakLevel, m_levelNbSamples);
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void FreeDVMod::calculateLevel(qint16& sample)
{
    Real t = sample / SDR_TX_SCALEF;

    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), t);
        m_levelSum += t * t;
        m_levelCalcCount++;
    }
    else
    {
        qreal rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        //qDebug("FreeDVMod::calculateLevel: %f %f", rmsLevel, m_peakLevel);
        emit levelChanged(rmsLevel, m_peakLevel, m_levelNbSamples);
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void FreeDVMod::start()
{
	qDebug() << "FreeDVMod::start: m_outputSampleRate: " << m_outputSampleRate
			<< " m_inputFrequencyOffset: " << m_settings.m_inputFrequencyOffset;

	m_audioFifo.clear();
	applyChannelSettings(m_basebandSampleRate, m_outputSampleRate, m_inputFrequencyOffset, true);
}

void FreeDVMod::stop()
{
}

bool FreeDVMod::handleMessage(const Message& cmd)
{
	if (UpChannelizer::MsgChannelizerNotification::match(cmd))
	{
		UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
		qDebug() << "FreeDVMod::handleMessage: MsgChannelizerNotification";

		applyChannelSettings(notif.getBasebandSampleRate(), notif.getSampleRate(), notif.getFrequencyOffset());

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "FreeDVMod::handleMessage: MsgConfigureChannelizer: sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureFreeDVMod::match(cmd))
    {
        MsgConfigureFreeDVMod& cfg = (MsgConfigureFreeDVMod&) cmd;
        qDebug() << "FreeDVMod::handleMessage: MsgConfigureFreeDVMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        openFileStream();
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);

        return true;
    }
    else if (MsgConfigureFileSourceStreamTiming::match(cmd))
    {
    	std::size_t samplesCount;

    	if (m_ifstream.eof()) {
    		samplesCount = m_fileSize / sizeof(Real);
    	} else {
    		samplesCount = m_ifstream.tellg() / sizeof(Real);
    	}

        if (getMessageQueueToGUI())
        {
            MsgReportFileSourceStreamTiming *report;
            report = MsgReportFileSourceStreamTiming::create(samplesCount);
            getMessageQueueToGUI()->push(report);
        }

        return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) cmd;

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendCWSettings(cfg.getSettings());
        }

        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        uint32_t sampleRate = cfg.getSampleRate();

        qDebug() << "FreeDVMod::handleMessage: DSPConfigureAudio:"
                << " sampleRate: " << sampleRate;

        if (sampleRate != m_audioSampleRate) {
            applyAudioSampleRate(sampleRate);
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        return true;
    }
	else
	{
		return false;
	}
}

void FreeDVMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_inputSampleRate = 48000; // fixed rate
    m_recordLength = m_fileSize / (sizeof(Real) * m_inputSampleRate);

    qDebug() << "FreeDVMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    if (getMessageQueueToGUI())
    {
        MsgReportFileSourceStreamData *report;
        report = MsgReportFileSourceStreamData::create(m_inputSampleRate, m_recordLength);
        getMessageQueueToGUI()->push(report);
    }
}

void FreeDVMod::seekFileStream(int seekPercentage)
{
    QMutexLocker mutexLocker(&m_settingsMutex);

    if (m_ifstream.is_open())
    {
        int seekPoint = ((m_recordLength * seekPercentage) / 100) * m_inputSampleRate;
        seekPoint *= sizeof(Real);
        m_ifstream.clear();
        m_ifstream.seekg(seekPoint, std::ios::beg);
    }
}

void FreeDVMod::applyAudioSampleRate(int sampleRate)
{
    qDebug("FreeDVMod::applyAudioSampleRate: %d", sampleRate);
    // TODO: put up simple IIR interpolator when sampleRate < m_modemSampleRate

    m_settingsMutex.lock();
    m_audioResampler.setDecimation(sampleRate / m_inputSampleRate);
    m_audioResampler.setAudioFilters(sampleRate, m_inputSampleRate, 250, 3300);
    m_settingsMutex.unlock();

    m_audioSampleRate = sampleRate;
}

void FreeDVMod::applyChannelSettings(int basebandSampleRate, int outputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "FreeDVMod::applyChannelSettings:"
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

    if ((outputSampleRate != m_outputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_modemSampleRate / (Real) outputSampleRate;
        m_interpolator.create(48, m_modemSampleRate, m_hiCutoff, 3.0);
        m_settingsMutex.unlock();
    }

    m_basebandSampleRate = basebandSampleRate;
    m_outputSampleRate = outputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void FreeDVMod::applyFreeDVMode(FreeDVModSettings::FreeDVMode mode)
{
    m_hiCutoff = FreeDVModSettings::getHiCutoff(mode);
    m_lowCutoff = FreeDVModSettings::getLowCutoff(mode);
    int modemSampleRate = FreeDVModSettings::getModSampleRate(mode);

    m_settingsMutex.lock();

    // baseband interpolator and filter
    if (modemSampleRate != m_modemSampleRate)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                modemSampleRate, m_settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);

        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) modemSampleRate / (Real) m_outputSampleRate;
        m_interpolator.create(48, modemSampleRate, m_hiCutoff, 3.0);
        m_SSBFilter->create_filter(m_lowCutoff / modemSampleRate, m_hiCutoff / modemSampleRate);
        m_modemSampleRate = modemSampleRate;

        if (getMessageQueueToGUI())
        {
            DSPConfigureAudio *cfg = new DSPConfigureAudio(m_modemSampleRate);
            getMessageQueueToGUI()->push(cfg);
        }
    }

    // FreeDV object

    if (m_freeDV) {
        freedv_close(m_freeDV);
    }

    int fdv_mode = -1;

    switch(mode)
    {
    case FreeDVModSettings::FreeDVMode700D:
        fdv_mode = FREEDV_MODE_700D;
        m_scaleFactor = SDR_TX_SCALEF / 3.2f;
        break;
    case FreeDVModSettings::FreeDVMode800XA:
        fdv_mode = FREEDV_MODE_800XA;
        m_scaleFactor = SDR_TX_SCALEF / 8.2f;
        break;
    case FreeDVModSettings::FreeDVMode1600:
        fdv_mode = FREEDV_MODE_1600;
        m_scaleFactor = SDR_TX_SCALEF / 3.2f;
        break;
    case FreeDVModSettings::FreeDVMode2400A:
    default:
        fdv_mode = FREEDV_MODE_2400A;
        m_scaleFactor = SDR_TX_SCALEF / 8.2f;
        break;
    }

    if (fdv_mode == FREEDV_MODE_700D)
    {
        struct freedv_advanced adv;
        adv.interleave_frames = 1;
        m_freeDV = freedv_open_advanced(fdv_mode, &adv);
    }
    else
    {
        m_freeDV = freedv_open(fdv_mode);
    }

    if (m_freeDV)
    {
        freedv_set_test_frames(m_freeDV, 0);
        freedv_set_snr_squelch_thresh(m_freeDV, -100.0);
        freedv_set_squelch_en(m_freeDV, 1);
        freedv_set_clip(m_freeDV, 0);
        freedv_set_tx_bpf(m_freeDV, 1);
        freedv_set_ext_vco(m_freeDV, 0);

        int nSpeechSamples = freedv_get_n_speech_samples(m_freeDV);
        int nNomModemSamples = freedv_get_n_nom_modem_samples(m_freeDV);
        int Fs = freedv_get_modem_sample_rate(m_freeDV);
        int Rs = freedv_get_modem_symbol_rate(m_freeDV);

        if (nSpeechSamples != m_nSpeechSamples)
        {
            if (m_speechIn) {
                delete[] m_speechIn;
            }

            m_speechIn = new int16_t[m_nSpeechSamples];
            m_nSpeechSamples = nSpeechSamples;
        }

        if (nNomModemSamples != m_nNomModemSamples)
        {
            if (m_modOut) {
                delete[] m_modOut;
            }

            m_modOut = new int16_t[m_nNomModemSamples];
            m_nNomModemSamples = nNomModemSamples;
        }

        m_iSpeech = 0;
        m_iModem = 0;

        qDebug() << "FreeDVMod::applyFreeDVMode:"
                << " fdv_mode: " << fdv_mode
                << " m_modemSampleRate: " << m_modemSampleRate
                << " Fs: " << Fs
                << " Rs: " << Rs
                << " m_nSpeechSamples: " << m_nSpeechSamples
                << " m_nNomModemSamples: " << m_nNomModemSamples;
    }

    m_settingsMutex.unlock();
}

void FreeDVMod::applySettings(const FreeDVModSettings& settings, bool force)
{
    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        reverseAPIKeys.append("toneFrequency");
    }
    if ((settings.m_volumeFactor != m_settings.m_volumeFactor) || force) {
        reverseAPIKeys.append("volumeFactor");
    }
    if ((settings.m_spanLog2 != m_settings.m_spanLog2) || force) {
        reverseAPIKeys.append("spanLog2");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_playLoop != m_settings.m_playLoop) || force) {
        reverseAPIKeys.append("playLoop");
    }
    if ((settings.m_playLoop != m_settings.m_gaugeInputElseModem) || force) {
        reverseAPIKeys.append("gaugeInputElseModem");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_freeDVMode != m_settings.m_freeDVMode) || force) {
        reverseAPIKeys.append("freeDVMode");
    }
    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force) {
        reverseAPIKeys.append("modAFInput");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }

    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force)
    {
        m_settingsMutex.lock();
        m_toneNco.setFreq(settings.m_toneFrequency, m_inputSampleRate);
        m_settingsMutex.unlock();
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->addAudioSource(&m_audioFifo, getInputMessageQueue(), audioDeviceIndex);
        uint32_t audioSampleRate = audioDeviceManager->getInputSampleRate(audioDeviceIndex);

        if (m_audioSampleRate != audioSampleRate) {
            applyAudioSampleRate(audioSampleRate);
        }
    }

    if ((m_settings.m_freeDVMode != settings.m_freeDVMode) || force) {
        applyFreeDVMode(settings.m_freeDVMode);
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

QByteArray FreeDVMod::serialize() const
{
    return m_settings.serialize();
}

bool FreeDVMod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureFreeDVMod *msg = MsgConfigureFreeDVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureFreeDVMod *msg = MsgConfigureFreeDVMod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int FreeDVMod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreeDvModSettings(new SWGSDRangel::SWGFreeDVModSettings());
    response.getFreeDvModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int FreeDVMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    FreeDVModSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getFreeDvModSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = response.getFreeDvModSettings()->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = response.getFreeDvModSettings()->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_spanLog2 = response.getFreeDvModSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getFreeDvModSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = response.getFreeDvModSettings()->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("gaugeInputElseModem")) {
        settings.m_gaugeInputElseModem = response.getFreeDvModSettings()->getGaugeInputElseModem() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFreeDvModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFreeDvModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("freeDVMode")) {
        settings.m_freeDVMode = (FreeDVModSettings::FreeDVMode) response.getFreeDvModSettings()->getFreeDvMode();
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (FreeDVModSettings::FreeDVModInputAF) response.getFreeDvModSettings()->getModAfInput();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getFreeDvModSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFreeDvModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFreeDvModSettings()->getReverseApiAddress() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFreeDvModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFreeDvModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getFreeDvModSettings()->getReverseApiChannelIndex();
    }

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getFreeDvModSettings()->getCwKeyer();
        CWKeyerSettings cwKeyerSettings = m_cwKeyer.getSettings();

        if (channelSettingsKeys.contains("cwKeyer.loop")) {
            cwKeyerSettings.m_loop = apiCwKeyerSettings->getLoop() != 0;
        }
        if (channelSettingsKeys.contains("cwKeyer.mode")) {
            cwKeyerSettings.m_mode = (CWKeyerSettings::CWMode) apiCwKeyerSettings->getMode();
        }
        if (channelSettingsKeys.contains("cwKeyer.text")) {
            cwKeyerSettings.m_text = *apiCwKeyerSettings->getText();
        }
        if (channelSettingsKeys.contains("cwKeyer.sampleRate")) {
            cwKeyerSettings.m_sampleRate = apiCwKeyerSettings->getSampleRate();
        }
        if (channelSettingsKeys.contains("cwKeyer.wpm")) {
            cwKeyerSettings.m_wpm = apiCwKeyerSettings->getWpm();
        }

        m_cwKeyer.setLoop(cwKeyerSettings.m_loop);
        m_cwKeyer.setMode(cwKeyerSettings.m_mode);
        //m_cwKeyer.setSampleRate(cwKeyerSettings.m_sampleRate);
        m_cwKeyer.setText(cwKeyerSettings.m_text);
        m_cwKeyer.setWPM(cwKeyerSettings.m_wpm);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            CWKeyer::MsgConfigureCWKeyer *msgCwKeyer = CWKeyer::MsgConfigureCWKeyer::create(cwKeyerSettings, force);
            m_guiMessageQueue->push(msgCwKeyer);
        }
    }

    if (frequencyOffsetChanged)
    {
        FreeDVMod::MsgConfigureChannelizer *msgChan = FreeDVMod::MsgConfigureChannelizer::create(
                m_audioSampleRate, settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(msgChan);
    }

    MsgConfigureFreeDVMod *msg = MsgConfigureFreeDVMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFreeDVMod *msgToGUI = MsgConfigureFreeDVMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int FreeDVMod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreeDvModReport(new SWGSDRangel::SWGFreeDVModReport());
    response.getFreeDvModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void FreeDVMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FreeDVModSettings& settings)
{
    response.getFreeDvModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getFreeDvModSettings()->setToneFrequency(settings.m_toneFrequency);
    response.getFreeDvModSettings()->setVolumeFactor(settings.m_volumeFactor);
    response.getFreeDvModSettings()->setSpanLog2(settings.m_spanLog2);
    response.getFreeDvModSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getFreeDvModSettings()->setPlayLoop(settings.m_playLoop ? 1 : 0);
    response.getFreeDvModSettings()->setRgbColor(settings.m_rgbColor);
    response.getFreeDvModSettings()->setGaugeInputElseModem(settings.m_gaugeInputElseModem ? 1 : 0);

    if (response.getFreeDvModSettings()->getTitle()) {
        *response.getFreeDvModSettings()->getTitle() = settings.m_title;
    } else {
        response.getFreeDvModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getFreeDvModSettings()->setModAfInput((int) settings.m_modAFInput);

    if (response.getFreeDvModSettings()->getAudioDeviceName()) {
        *response.getFreeDvModSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getFreeDvModSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    if (!response.getFreeDvModSettings()->getCwKeyer()) {
        response.getFreeDvModSettings()->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getFreeDvModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_cwKeyer.getSettings();
    apiCwKeyerSettings->setLoop(cwKeyerSettings.m_loop ? 1 : 0);
    apiCwKeyerSettings->setMode((int) cwKeyerSettings.m_mode);
    apiCwKeyerSettings->setSampleRate(cwKeyerSettings.m_sampleRate);

    if (apiCwKeyerSettings->getText()) {
        *apiCwKeyerSettings->getText() = cwKeyerSettings.m_text;
    } else {
        apiCwKeyerSettings->setText(new QString(cwKeyerSettings.m_text));
    }

    apiCwKeyerSettings->setWpm(cwKeyerSettings.m_wpm);

    response.getFreeDvModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFreeDvModSettings()->getReverseApiAddress()) {
        *response.getFreeDvModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFreeDvModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFreeDvModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFreeDvModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getFreeDvModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void FreeDVMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getFreeDvModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getFreeDvModReport()->setAudioSampleRate(m_audioSampleRate);
    response.getFreeDvModReport()->setChannelSampleRate(m_outputSampleRate);
}

void FreeDVMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreeDVModSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setTx(1);
    swgChannelSettings->setChannelType(new QString("FreeDVMod"));
    swgChannelSettings->setFreeDvModSettings(new SWGSDRangel::SWGFreeDVModSettings());
    SWGSDRangel::SWGFreeDVModSettings *swgFreeDVModSettings = swgChannelSettings->getFreeDvModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgFreeDVModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swgFreeDVModSettings->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swgFreeDVModSettings->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgFreeDVModSettings->setSpanLog2(settings.m_spanLog2);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgFreeDVModSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swgFreeDVModSettings->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("gaugeInputElseModem") || force) {
        swgFreeDVModSettings->setPlayLoop(settings.m_gaugeInputElseModem ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgFreeDVModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgFreeDVModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("freeDVMode") || force) {
        swgFreeDVModSettings->setFreeDvMode((int) settings.m_freeDVMode);
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swgFreeDVModSettings->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgFreeDVModSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    if (force)
    {
        const CWKeyerSettings& cwKeyerSettings = m_cwKeyer.getSettings();
        swgFreeDVModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgFreeDVModSettings->getCwKeyer();
        apiCwKeyerSettings->setLoop(cwKeyerSettings.m_loop ? 1 : 0);
        apiCwKeyerSettings->setMode(cwKeyerSettings.m_mode);
        apiCwKeyerSettings->setSampleRate(cwKeyerSettings.m_sampleRate);
        apiCwKeyerSettings->setText(new QString(cwKeyerSettings.m_text));
        apiCwKeyerSettings->setWpm(cwKeyerSettings.m_wpm);
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

void FreeDVMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setTx(1);
    swgChannelSettings->setChannelType(new QString("FreeDVMod"));
    swgChannelSettings->setFreeDvModSettings(new SWGSDRangel::SWGFreeDVModSettings());
    SWGSDRangel::SWGFreeDVModSettings *swgFreeDVModSettings = swgChannelSettings->getFreeDvModSettings();

    swgFreeDVModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgFreeDVModSettings->getCwKeyer();
    apiCwKeyerSettings->setLoop(cwKeyerSettings.m_loop ? 1 : 0);
    apiCwKeyerSettings->setMode(cwKeyerSettings.m_mode);
    apiCwKeyerSettings->setSampleRate(cwKeyerSettings.m_sampleRate);
    apiCwKeyerSettings->setText(new QString(cwKeyerSettings.m_text));
    apiCwKeyerSettings->setWpm(cwKeyerSettings.m_wpm);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex)
            .arg(m_settings.m_reverseAPIChannelIndex);
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

void FreeDVMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FreeDVMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("FreeDVMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
