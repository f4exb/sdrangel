///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "libfreedv.h"

#include "SWGChannelSettings.h"
#include "SWGFreeDVDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGFreeDVDemodReport.h"

#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "util/db.h"

#include "freedvdemod.h"

MESSAGE_CLASS_DEFINITION(FreeDVDemod::MsgConfigureFreeDVDemod, Message)
MESSAGE_CLASS_DEFINITION(FreeDVDemod::MsgResyncFreeDVDemod, Message)
MESSAGE_CLASS_DEFINITION(FreeDVDemod::MsgConfigureFreeDVDemodPrivate, Message)
MESSAGE_CLASS_DEFINITION(FreeDVDemod::MsgConfigureChannelizer, Message)

const QString FreeDVDemod::m_channelIdURI = "sdrangel.channel.freedvdemod";
const QString FreeDVDemod::m_channelId = "FreeDVDemod";

FreeDVDemod::FreeDVStats::FreeDVStats()
{
    init();
}

void FreeDVDemod::FreeDVStats::init()
{
    m_sync = false;
    m_snrEst = -20;
    m_clockOffset = 0;
    m_freqOffset = 0;
    m_syncMetric = 0;
    m_totalBitErrors = 0;
    m_lastTotalBitErrors = 0;
    m_ber = 0;
    m_frameCount = 0;
    m_berFrameCount = 0;
    m_fps = 1;
}

void FreeDVDemod::FreeDVStats::collect(struct FreeDV::freedv *freeDV)
{
    struct FreeDV::MODEM_STATS stats;

    FreeDV::freedv_get_modem_extended_stats(freeDV, &stats);
    m_totalBitErrors = FreeDV::freedv_get_total_bit_errors(freeDV);
    m_clockOffset = stats.clock_offset;
    m_freqOffset = stats.foff;
    m_syncMetric = stats.sync_metric;
    m_sync = stats.sync != 0;
    m_snrEst = stats.snr_est;

    if (m_berFrameCount >= m_fps)
    {
        m_ber = m_totalBitErrors - m_lastTotalBitErrors;
        m_ber = m_ber < 0 ? 0 : m_ber;
        m_berFrameCount = 0;
        m_lastTotalBitErrors = m_totalBitErrors;
//        qDebug("FreeDVStats::collect: demod sync: %s sync metric: %f demod snr: %3.2f dB  BER: %d clock offset: %f freq offset: %f",
//            m_sync ? "ok" : "ko", m_syncMetric, m_snrEst, m_ber, m_clockOffset, m_freqOffset);
    }

    m_berFrameCount++;
    m_frameCount++;
}

FreeDVDemod::FreeDVSNR::FreeDVSNR()
{
    m_sum = 0.0f;
    m_peak = 0.0f;
    m_n = 0;
    m_reset = true;
}

void FreeDVDemod::FreeDVSNR::accumulate(float snrdB)
{
    if (m_reset)
    {
        m_sum = CalcDb::powerFromdB(snrdB);
        m_peak = snrdB;
        m_n = 1;
        m_reset = false;
    }
    else
    {
        m_sum += CalcDb::powerFromdB(snrdB);
        m_peak = std::max(m_peak, snrdB);
        m_n++;
    }
}

FreeDVDemod::LevelRMS::LevelRMS()
{
    m_sum = 0.0f;
    m_peak = 0.0f;
    m_n = 0;
    m_reset = true;
}

void FreeDVDemod::LevelRMS::accumulate(float level)
{
    if (m_reset)
    {
        m_sum = level * level;
        m_peak = std::fabs(level);
        m_n = 1;
        m_reset = false;
    }
    else
    {
        m_sum += level * level;
        m_peak = std::max(m_peak, std::fabs(level));
        m_n++;
    }
}

FreeDVDemod::FreeDVDemod(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_hiCutoff(6000),
        m_lowCutoff(0),
        m_volume(2),
        m_spanLog2(3),
        m_sum(fftfilt::cmplx{0,0}),
        m_inputSampleRate(48000),
        m_modemSampleRate(48000),
        m_speechSampleRate(8000), // fixed 8 kS/s
        m_inputFrequencyOffset(0),
        m_audioMute(false),
        m_simpleAGC(0.003f, 0.0f, 1e-6f),
        m_agcActive(false),
        m_squelchDelayLine(2*48000),
        m_audioActive(false),
        m_sampleSink(0),
        m_audioFifo(24000),
        m_freeDV(0),
        m_nSpeechSamples(0),
        m_nMaxModemSamples(0),
        m_iSpeech(0),
        m_iModem(0),
        m_speechOut(0),
        m_modIn(0),
        m_levelInNbSamples(480), // 10ms @ 48 kS/s
        m_settingsMutex(QMutex::Recursive)
{
	setObjectName(m_channelId);

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifo, getInputMessageQueue());
    uint32_t audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate();
    applyAudioSampleRate(audioSampleRate);

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;

	m_magsq = 0.0f;
	m_magsqSum = 0.0f;
	m_magsqPeak = 0.0f;
	m_magsqCount = 0;

    m_simpleAGC.resizeNew(m_modemSampleRate/10, 0.003);

	SSBFilter = new fftfilt(m_lowCutoff / m_modemSampleRate, m_hiCutoff / m_modemSampleRate, ssbFftLen);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
	applySettings(m_settings, true);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

FreeDVDemod::~FreeDVDemod()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);

	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete SSBFilter;
}

void FreeDVDemod::configure(MessageQueue* messageQueue,
		Real Bandwidth,
		Real LowCutoff,
		Real volume,
		int spanLog2,
		bool audioBinaural,
		bool audioFlipChannel,
		bool dsb,
		bool audioMute,
		bool agc,
		bool agcClamping,
        int agcTimeLog2,
        int agcPowerThreshold,
        int agcThresholdGate)
{
	Message* cmd = MsgConfigureFreeDVDemodPrivate::create(
	        Bandwidth,
	        LowCutoff,
	        volume,
	        spanLog2,
	        audioBinaural,
	        audioFlipChannel,
	        dsb,
	        audioMute,
	        agc,
	        agcClamping,
	        agcTimeLog2,
	        agcPowerThreshold,
	        agcThresholdGate);
	messageQueue->push(cmd);
}

void FreeDVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
	Complex ci;
	fftfilt::cmplx *sideband;
	int n_out;

	m_settingsMutex.lock();

	int decim = 1<<(m_spanLog2 - 1);
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if(m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
		{
            n_out = SSBFilter->runSSB(ci, &sideband, true);
			m_interpolatorDistanceRemain += m_interpolatorDistance;
		}
		else
		{
			n_out = 0;
		}

		for (int i = 0; i < n_out; i++)
		{
			// Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
			// smart decimation with bit gain using float arithmetic (23 bits significand)

			m_sum += sideband[i];

			if (!(m_undersampleCount++ & decim_mask))
			{
				Real avgr = m_sum.real() / decim;
				Real avgi = m_sum.imag() / decim;
				m_magsq = (avgr * avgr + avgi * avgi) / (SDR_RX_SCALED*SDR_RX_SCALED);

                m_magsqSum += m_magsq;

                if (m_magsq > m_magsqPeak)
                {
                    m_magsqPeak = m_magsq;
                }

                m_magsqCount++;
                m_sampleBuffer.push_back(Sample(avgr, avgi));
                m_sum.real(0.0);
                m_sum.imag(0.0);
			}

            // fftfilt::cmplx z = sideband[i];
            // Real demod = (z.real() + z.imag()) * 0.7;
            Real demod = sideband[i].real(); // works as good

            if (m_agcActive)
            {
                m_simpleAGC.feed(demod);
                demod *= (m_settings.m_volumeIn * 3276.8f) / m_simpleAGC.getValue(); // provision for peak to average ratio (here 10) compensated by m_volumeIn
                // if (i == 0) {
                //     qDebug("FreeDVDemod::feed: m_simpleAGC: %f", m_simpleAGC.getValue());
                // }
            }
            else
            {
                demod *= m_settings.m_volumeIn;
            }


            pushSampleToDV((qint16) demod);
		}
	}

	uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

	if (res != m_audioBufferFill)
	{
        qDebug("FreeDVDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
	}

	m_audioBufferFill = 0;

	if (m_sampleSink != 0)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void FreeDVDemod::start()
{
    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void FreeDVDemod::stop()
{
}

bool FreeDVDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;
		qDebug("FreeDVDemod::handleMessage: MsgChannelizerNotification: m_sampleRate");

		applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "FreeDVDemod::handleMessage: MsgConfigureChannelizer: sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureFreeDVDemod::match(cmd))
    {
        MsgConfigureFreeDVDemod& cfg = (MsgConfigureFreeDVDemod&) cmd;
        qDebug("FreeDVDemod::handleMessage: MsgConfigureFreeDVDemod");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgResyncFreeDVDemod::match(cmd))
    {
        qDebug("FreeDVDemod::handleMessage: MsgResyncFreeDVDemod");
        m_settingsMutex.lock();
        FreeDV::freedv_set_sync(m_freeDV, FreeDV::unsync);
        m_settingsMutex.unlock();
        return true;
    }
    else if (BasebandSampleSink::MsgThreadedSink::match(cmd))
    {
        BasebandSampleSink::MsgThreadedSink& cfg = (BasebandSampleSink::MsgThreadedSink&) cmd;
        const QThread *thread = cfg.getThread();
        qDebug("FreeDVDemod::handleMessage: BasebandSampleSink::MsgThreadedSink: %p", thread);
        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        uint32_t sampleRate = cfg.getSampleRate();

        qDebug() << "FreeDVDemod::handleMessage: DSPConfigureAudio:"
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
		if(m_sampleSink != 0)
		{
		   return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void FreeDVDemod::pushSampleToDV(int16_t sample)
{
    qint16 audioSample;

    if (m_levelIn.m_n >= m_levelInNbSamples)
    {
        qreal rmsLevel = sqrt(m_levelIn.m_sum / m_levelInNbSamples);
        // qDebug("FreeDVDemod::pushSampleToDV: rmsLevel: %f m_peak: %f m_levelInNbSamples: %d",
        //     rmsLevel, m_levelIn.m_peak, m_levelInNbSamples);
        emit levelInChanged(rmsLevel, m_levelIn.m_peak, m_levelInNbSamples);
        m_levelIn.m_reset = true;
    }

    m_levelIn.accumulate(sample/29491.2f); // scale on 90% (0.9 * 32768.0)

    if (m_iModem == m_nin)
    {
        int nout = FreeDV::freedv_rx(m_freeDV, m_speechOut, m_modIn);
        m_freeDVStats.collect(m_freeDV);
        m_freeDVSNR.accumulate(m_freeDVStats.m_snrEst);

        if (m_settings.m_audioMute)
        {
            for (uint32_t i = 0; i < nout * m_audioResampler.getDecimation(); i++) {
                pushSampleToAudio(0);
            }
        }
        else
        {
            for (int i = 0; i < nout; i++)
            {
                while (!m_audioResampler.upSample(m_speechOut[i], audioSample)) {
                    pushSampleToAudio(audioSample);
                }

                pushSampleToAudio(audioSample);
            }
        }

        m_iModem = 0;
        m_iSpeech = 0;
    }

    m_modIn[m_iModem++] = sample;
}

void FreeDVDemod::pushSampleToAudio(int16_t sample)
{
    m_audioBuffer[m_audioBufferFill].l = sample * m_volume;
    m_audioBuffer[m_audioBufferFill].r = sample * m_volume;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

        if (res != m_audioBufferFill) {
            qDebug("FreeDVDemod::pushSampleToAudio: %u/%u samples written", res, m_audioBufferFill);
        }

        m_audioBufferFill = 0;
    }
}

void FreeDVDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "FreeDVDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((m_inputFrequencyOffset != inputFrequencyOffset) ||
        (m_inputSampleRate != inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((m_inputSampleRate != inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, inputSampleRate, m_hiCutoff * 1.5f, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) inputSampleRate / (Real) m_modemSampleRate;
        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void FreeDVDemod::applyAudioSampleRate(int sampleRate)
{
    qDebug("FreeDVDemod::applyAudioSampleRate: %d", sampleRate);

    m_settingsMutex.lock();
    m_audioFifo.setSize(sampleRate);
    m_audioResampler.setDecimation(sampleRate / m_speechSampleRate);
    m_audioResampler.setAudioFilters(sampleRate, sampleRate, 250, 3300, 4.0f);
    m_settingsMutex.unlock();

    m_audioSampleRate = sampleRate;
}

void FreeDVDemod::applyFreeDVMode(FreeDVDemodSettings::FreeDVMode mode)
{
    m_hiCutoff = FreeDVDemodSettings::getHiCutoff(mode);
    m_lowCutoff = FreeDVDemodSettings::getLowCutoff(mode);
    uint32_t modemSampleRate = FreeDVDemodSettings::getModSampleRate(mode);

    m_settingsMutex.lock();
    SSBFilter->create_filter(m_lowCutoff / (float) modemSampleRate, m_hiCutoff / (float) modemSampleRate);

    // baseband interpolator
    if (modemSampleRate != m_modemSampleRate)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                modemSampleRate, m_settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);

        m_interpolatorDistanceRemain = 0;
        //m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_inputSampleRate / (Real) modemSampleRate;
        m_interpolator.create(16, m_inputSampleRate, m_hiCutoff * 1.5f, 2.0f);
        m_modemSampleRate = modemSampleRate;

        m_simpleAGC.resizeNew(modemSampleRate/10, 0.003);
        m_levelInNbSamples = m_modemSampleRate / 100; // 10ms

        if (getMessageQueueToGUI())
        {
            DSPConfigureAudio *cfg = new DSPConfigureAudio(m_modemSampleRate);
            getMessageQueueToGUI()->push(cfg);
        }
    }

    // FreeDV object

    if (m_freeDV) {
        FreeDV::freedv_close(m_freeDV);
    }

    int fdv_mode = -1;

    switch(mode)
    {
    case FreeDVDemodSettings::FreeDVMode700C:
        fdv_mode = FREEDV_MODE_700C;
        break;
    case FreeDVDemodSettings::FreeDVMode700D:
        fdv_mode = FREEDV_MODE_700D;
        break;
    case FreeDVDemodSettings::FreeDVMode800XA:
        fdv_mode = FREEDV_MODE_800XA;
        break;
    case FreeDVDemodSettings::FreeDVMode1600:
        fdv_mode = FREEDV_MODE_1600;
        break;
    case FreeDVDemodSettings::FreeDVMode2400A:
    default:
        fdv_mode = FREEDV_MODE_2400A;
        break;
    }

    if (fdv_mode == FREEDV_MODE_700D)
    {
        struct FreeDV::freedv_advanced adv;
        adv.interleave_frames = 1;
        m_freeDV = FreeDV::freedv_open_advanced(fdv_mode, &adv);
    }
    else
    {
        m_freeDV = FreeDV::freedv_open(fdv_mode);
    }

    if (m_freeDV)
    {
        FreeDV::freedv_set_test_frames(m_freeDV, 0);
        FreeDV::freedv_set_snr_squelch_thresh(m_freeDV, -100.0);
        FreeDV::freedv_set_squelch_en(m_freeDV, 0);
        FreeDV::freedv_set_clip(m_freeDV, 0);
        FreeDV::freedv_set_ext_vco(m_freeDV, 0);
        FreeDV::freedv_set_sync(m_freeDV, FreeDV::manualsync);

        FreeDV::freedv_set_callback_txt(m_freeDV, nullptr, nullptr, nullptr);
        FreeDV::freedv_set_callback_protocol(m_freeDV, nullptr, nullptr, nullptr);
        FreeDV::freedv_set_callback_data(m_freeDV, nullptr, nullptr, nullptr);

        int nSpeechSamples = FreeDV::freedv_get_n_speech_samples(m_freeDV);
        int nMaxModemSamples = FreeDV::freedv_get_n_max_modem_samples(m_freeDV);
        int Fs = FreeDV::freedv_get_modem_sample_rate(m_freeDV);
        int Rs = FreeDV::freedv_get_modem_symbol_rate(m_freeDV);
        m_freeDVStats.init();

        if (nSpeechSamples != m_nSpeechSamples)
        {
            if (m_speechOut) {
                delete[] m_speechOut;
            }

            m_speechOut = new int16_t[nSpeechSamples];
            m_nSpeechSamples = nSpeechSamples;
        }

        if (nMaxModemSamples != m_nMaxModemSamples)
        {
            if (m_modIn) {
                delete[] m_modIn;
            }

            m_modIn = new int16_t[nMaxModemSamples];
            m_nMaxModemSamples = nMaxModemSamples;
        }

        m_iSpeech = 0;
        m_iModem = 0;
        m_nin = FreeDV::freedv_nin(m_freeDV);

        if (m_nin > 0) {
            m_freeDVStats.m_fps = m_modemSampleRate / m_nin;
        }

        qDebug() << "FreeDVMod::applyFreeDVMode:"
                << " fdv_mode: " << fdv_mode
                << " m_modemSampleRate: " << m_modemSampleRate
                << " m_lowCutoff: " << m_lowCutoff
                << " m_hiCutoff: " << m_hiCutoff
                << " Fs: " << Fs
                << " Rs: " << Rs
                << " m_nSpeechSamples: " << m_nSpeechSamples
                << " m_nMaxModemSamples: " << m_nMaxModemSamples
                << " m_nin: " << m_nin
                << " FPS: " << m_freeDVStats.m_fps;
    }

    m_settingsMutex.unlock();
}

void FreeDVDemod::applySettings(const FreeDVDemodSettings& settings, bool force)
{
    qDebug() << "FreeDVDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_volume: " << settings.m_volume
            << " m_volumeIn: " << settings.m_volumeIn
            << " m_spanLog2: " << settings.m_spanLog2
            << " m_audioMute: " << settings.m_audioMute
            << " m_agcActive: " << settings.m_agc
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }

    if ((m_settings.m_volume != settings.m_volume) || force)
    {
        reverseAPIKeys.append("volume");
        m_volume = settings.m_volume;
        m_volume /= 4.0; // for 3276.8
    }

    if ((m_settings.m_volumeIn != settings.m_volumeIn) || force)
    {
        reverseAPIKeys.append("volumeIn");
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        reverseAPIKeys.append("audioDeviceName");
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->addAudioSink(&m_audioFifo, getInputMessageQueue(), audioDeviceIndex);
        uint32_t audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

        if (m_audioSampleRate != audioSampleRate) {
            applyAudioSampleRate(audioSampleRate);
        }
    }

    if ((m_settings.m_spanLog2 != settings.m_spanLog2) || force) {
        reverseAPIKeys.append("spanLog2");
    }
    if ((m_settings.m_audioMute != settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((m_settings.m_agc != settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
    }

    if ((settings.m_freeDVMode != m_settings.m_freeDVMode) || force) {
        applyFreeDVMode(settings.m_freeDVMode);
    }

    m_spanLog2 = settings.m_spanLog2;
    m_audioMute = settings.m_audioMute;
    m_agcActive = settings.m_agc;

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

void FreeDVDemod::getSNRLevels(double& avg, double& peak, int& nbSamples)
{
    if (m_freeDVSNR.m_n > 0)
    {
        avg = CalcDb::dbPower(m_freeDVSNR.m_sum / m_freeDVSNR.m_n);
        peak = m_freeDVSNR.m_peak;
        nbSamples = m_freeDVSNR.m_n;
        m_freeDVSNR.m_reset = true;
    }
    else
    {
        avg = 0.0;
        peak = 0.0;
        nbSamples = 1;
    }
}

QByteArray FreeDVDemod::serialize() const
{
    return m_settings.serialize();
}

bool FreeDVDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureFreeDVDemod *msg = MsgConfigureFreeDVDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureFreeDVDemod *msg = MsgConfigureFreeDVDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int FreeDVDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreeDvDemodSettings(new SWGSDRangel::SWGFreeDVDemodSettings());
    response.getFreeDvDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int FreeDVDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    FreeDVDemodSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getFreeDvDemodSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getFreeDvDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("volumeIn")) {
        settings.m_volumeIn = response.getFreeDvDemodSettings()->getVolumeIn();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_spanLog2 = response.getFreeDvDemodSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getFreeDvDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getFreeDvDemodSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFreeDvDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFreeDvDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getFreeDvDemodSettings()->getAudioDeviceName();
    }

    if (frequencyOffsetChanged)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                m_modemSampleRate, settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);
    }

    MsgConfigureFreeDVDemod *msg = MsgConfigureFreeDVDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("FreeDVDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFreeDVDemod *msgToGUI = MsgConfigureFreeDVDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int FreeDVDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreeDvDemodReport(new SWGSDRangel::SWGFreeDVDemodReport());
    response.getFreeDvDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void FreeDVDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FreeDVDemodSettings& settings)
{
    response.getFreeDvDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getFreeDvDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getFreeDvDemodSettings()->setVolume(settings.m_volume);
    response.getFreeDvDemodSettings()->setVolumeIn(settings.m_volumeIn);
    response.getFreeDvDemodSettings()->setSpanLog2(settings.m_spanLog2);
    response.getFreeDvDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getFreeDvDemodSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getFreeDvDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getFreeDvDemodSettings()->setFreeDvMode((int) settings.m_freeDVMode);

    if (response.getFreeDvDemodSettings()->getTitle()) {
        *response.getFreeDvDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getFreeDvDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getFreeDvDemodSettings()->getAudioDeviceName()) {
        *response.getFreeDvDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getFreeDvDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
}

void FreeDVDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getFreeDvDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getFreeDvDemodReport()->setSquelch(m_audioActive ? 1 : 0);
    response.getFreeDvDemodReport()->setAudioSampleRate(m_audioSampleRate);
    response.getFreeDvDemodReport()->setChannelSampleRate(m_inputSampleRate);
}

void FreeDVDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreeDVDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setTx(0);
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("SSBDemod"));
    swgChannelSettings->setFreeDvDemodSettings(new SWGSDRangel::SWGFreeDVDemodSettings());
    SWGSDRangel::SWGFreeDVDemodSettings *swgFreeDVDemodSettings = swgChannelSettings->getFreeDvDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgFreeDVDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgFreeDVDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("volumeIn") || force) {
        swgFreeDVDemodSettings->setVolumeIn(settings.m_volumeIn);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgFreeDVDemodSettings->setSpanLog2(settings.m_spanLog2);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgFreeDVDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc") || force) {
        swgFreeDVDemodSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgFreeDVDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgFreeDVDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgFreeDVDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
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

void FreeDVDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FreeDVDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("FreeDVDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
