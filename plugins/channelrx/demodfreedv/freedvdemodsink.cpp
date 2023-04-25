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

#include <QDebug>

#include "codec2/freedv_api.h"
#include "codec2/modem_stats.h"

#include "dsp/basebandsamplesink.h"
#include "audio/audiooutputdevice.h"
#include "util/db.h"

#include "freedvdemodsink.h"

const unsigned int FreeDVDemodSink::m_ssbFftLen = 1024;
const float        FreeDVDemodSink::m_agcTarget = 3276.8f; // -10 dB amplitude => -20 dB power: center of normal signal

FreeDVDemodSink::FreeDVStats::FreeDVStats()
{
    init();
}

void FreeDVDemodSink::FreeDVStats::init()
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

void FreeDVDemodSink::FreeDVStats::collect(struct freedv *freeDV)
{
    struct MODEM_STATS stats;

    freedv_get_modem_extended_stats(freeDV, &stats);
    m_totalBitErrors = freedv_get_total_bit_errors(freeDV);
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

FreeDVDemodSink::FreeDVSNR::FreeDVSNR()
{
    m_sum = 0.0f;
    m_peak = 0.0f;
    m_n = 0;
    m_reset = true;
}

void FreeDVDemodSink::FreeDVSNR::accumulate(float snrdB)
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

FreeDVDemodSink::LevelRMS::LevelRMS()
{
    m_sum = 0.0f;
    m_peak = 0.0f;
    m_n = 0;
    m_reset = true;
}

void FreeDVDemodSink::LevelRMS::accumulate(float level)
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

FreeDVDemodSink::FreeDVDemodSink() :
        m_hiCutoff(6000),
        m_lowCutoff(0),
        m_volume(2),
        m_spanLog2(3),
        m_sum(fftfilt::cmplx{0,0}),
        m_channelSampleRate(48000),
        m_modemSampleRate(48000),
        m_speechSampleRate(8000), // fixed 8 kS/s
        m_audioSampleRate(48000),
        m_channelFrequencyOffset(0),
        m_audioMute(false),
        m_simpleAGC(0.003f, 0.0f, 1e-6f),
        m_agcActive(false),
        m_squelchDelayLine(2*48000),
        m_audioActive(false),
        m_spectrumSink(nullptr),
        m_audioFifo(24000),
        m_freeDV(nullptr),
        m_nSpeechSamples(0),
        m_nMaxModemSamples(0),
        m_iSpeech(0),
        m_iModem(0),
        m_speechOut(nullptr),
        m_modIn(nullptr),
        m_levelInNbSamples(480) // 10ms @ 48 kS/s
{
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;

	m_magsq = 0.0f;
	m_magsqSum = 0.0f;
	m_magsqPeak = 0.0f;
	m_magsqCount = 0;

    m_simpleAGC.resizeNew(m_modemSampleRate/10, 0.003);

	SSBFilter = new fftfilt(m_lowCutoff / m_modemSampleRate, m_hiCutoff / m_modemSampleRate, m_ssbFftLen);
    m_SSBFilterBuffer = new fftfilt::cmplx[m_ssbFftLen];
    std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer + m_ssbFftLen, fftfilt::cmplx{0.0f, 0.0f});
    m_SSBFilterBufferIndex = 0;

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
	applySettings(m_settings, true);
}

FreeDVDemodSink::~FreeDVDemodSink()
{
    delete SSBFilter;
    delete[] m_SSBFilterBuffer;
}

void FreeDVDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    if (!m_freeDV) {
        return;
    }

    QMutexLocker mlock(&m_mutex);
	Complex ci;

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
	}

    if (m_spectrumSink && (m_sampleBuffer.size() != 0))
    {
		m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
        m_sampleBuffer.clear();
	}
}

void FreeDVDemodSink::processOneSample(Complex &ci)
{
	fftfilt::cmplx *sideband;
	int n_out = 0;
	int decim = 1<<(m_spanLog2 - 1);
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    m_sum += m_SSBFilterBuffer[m_SSBFilterBufferIndex];

    if (!(m_undersampleCount++ & decim_mask))
    {
        Real avgr = m_sum.real() / decim;
        Real avgi = m_sum.imag() / decim;
        m_magsq = (avgr*avgr + avgi*avgi) / (SDR_RX_SCALED*SDR_RX_SCALED);

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

    fftfilt::cmplx z = m_SSBFilterBuffer[m_SSBFilterBufferIndex];
    Real demod = (z.real() + z.imag()) * 0.7;
    // Real demod = m_SSBFilterBuffer[m_SSBFilterBufferIndex].real(); // works as good

    if (m_agcActive)
    {
        m_simpleAGC.feed(demod);
        demod *= (m_settings.m_volumeIn * 3276.8f) / m_simpleAGC.getValue(); // provision for peak to average ratio (here 10) compensated by m_volumeIn
    }
    else
    {
        demod *= m_settings.m_volumeIn;
    }

    pushSampleToDV((qint16) demod);
    n_out = SSBFilter->runSSB(ci, &sideband, true); // always USB side

    if (n_out > 0)
    {
        std::copy(sideband, sideband + n_out, m_SSBFilterBuffer);
        m_SSBFilterBufferIndex = 0;
    }
    else if (m_SSBFilterBufferIndex < m_ssbFftLen - 1)
    {
        m_SSBFilterBufferIndex++;
    }
}

void FreeDVDemodSink::pushSampleToDV(int16_t sample)
{
    qint16 audioSample;

    calculateLevel(sample);

    if (m_iModem == m_nin)
    {
        int nout = freedv_rx(m_freeDV, m_speechOut, m_modIn);
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

void FreeDVDemodSink::calculateLevel(int16_t& sample)
{
    if (m_levelIn.m_n >= m_levelInNbSamples)
    {
        m_rmsLevel = sqrt(m_levelIn.m_sum / m_levelInNbSamples);
        m_peakLevel = m_levelIn.m_peak;
        m_levelIn.m_reset = true;
    }

    m_levelIn.accumulate(sample/29491.2f); // scale on 90% (0.9 * 32768.0)
}

void FreeDVDemodSink::pushSampleToAudio(int16_t sample)
{
    m_audioBuffer[m_audioBufferFill].l = sample * m_volume;
    m_audioBuffer[m_audioBufferFill].r = sample * m_volume;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

        if (res != m_audioBufferFill) {
            qDebug("FreeDVDemodSink::pushSampleToAudio: %lu/%lu samples written", res, m_audioBufferFill);
        }

        m_audioBufferFill = 0;
    }
}

void FreeDVDemodSink::applyChannelSettings(int channelSampleRate, int channnelFrequencyOffset, bool force)
{
    qDebug() << "FreeDVDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " inputFrequencyOffset: " << channnelFrequencyOffset;

    if ((m_channelFrequencyOffset != channnelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channnelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_hiCutoff * 1.5f, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_modemSampleRate;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channnelFrequencyOffset;
}

void FreeDVDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("FreeDVDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    qDebug("FreeDVDemodSink::applyAudioSampleRate: %d", sampleRate);

    m_audioFifo.setSize(sampleRate);
    m_audioResampler.setDecimation(sampleRate / m_speechSampleRate);
    m_audioResampler.setAudioFilters(sampleRate, sampleRate, 250, 3300, 4.0f);
    m_audioSampleRate = sampleRate;
}

void FreeDVDemodSink::applyFreeDVMode(FreeDVDemodSettings::FreeDVMode mode)
{
    m_hiCutoff = FreeDVDemodSettings::getHiCutoff(mode);
    m_lowCutoff = FreeDVDemodSettings::getLowCutoff(mode);
    uint32_t modemSampleRate = FreeDVDemodSettings::getModSampleRate(mode);
    QMutexLocker mlock(&m_mutex);

    SSBFilter->create_filter(m_lowCutoff / (float) modemSampleRate, m_hiCutoff / (float) modemSampleRate);

    // baseband interpolator
    if (modemSampleRate != m_modemSampleRate)
    {
        m_interpolatorDistanceRemain = 0;
        //m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) modemSampleRate;
        m_interpolator.create(16, m_channelSampleRate, m_hiCutoff * 1.5f, 2.0f);
        m_modemSampleRate = modemSampleRate;

        m_simpleAGC.resizeNew(modemSampleRate/10, 0.003);
        m_levelInNbSamples = m_modemSampleRate / 100; // 10ms
    }

    // FreeDV object

    if (m_freeDV) {
        freedv_close(m_freeDV);
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
        freedv_set_squelch_en(m_freeDV, 0);
        freedv_set_clip(m_freeDV, 0);
        freedv_set_ext_vco(m_freeDV, 0);
        freedv_set_sync(m_freeDV, FREEDV_SYNC_MANUAL);

        int nSpeechSamples = freedv_get_n_speech_samples(m_freeDV);
        int nMaxModemSamples = freedv_get_n_max_modem_samples(m_freeDV);
        int Fs = freedv_get_modem_sample_rate(m_freeDV);
        int Rs = freedv_get_modem_symbol_rate(m_freeDV);
        m_freeDVStats.init();

        if (nSpeechSamples > m_nSpeechSamples)
        {
            if (m_speechOut) {
                delete[] m_speechOut;
            }

            m_speechOut = new int16_t[nSpeechSamples];
            m_nSpeechSamples = nSpeechSamples;
        }

        if (nMaxModemSamples > m_nMaxModemSamples)
        {
            if (m_modIn) {
                delete[] m_modIn;
            }

            m_modIn = new int16_t[nMaxModemSamples];
            m_nMaxModemSamples = nMaxModemSamples;
        }

        m_iSpeech = 0;
        m_iModem = 0;
        m_nin = freedv_nin(m_freeDV);

        if (m_nin > 0) {
            m_freeDVStats.m_fps = m_modemSampleRate / m_nin;
        }

        qDebug() << "FreeDVDemodSink::applyFreeDVMode:"
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
    else
    {
        qCritical("FreeDVDemodSink::applyFreeDVMode: m_freeDV was not allocated");
    }
}

void FreeDVDemodSink::applySettings(const FreeDVDemodSettings& settings, bool force)
{
    qDebug() << "FreeDVDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_freeDVMode: " << (int) settings.m_freeDVMode
            << " m_volume: " << settings.m_volume
            << " m_volumeIn: " << settings.m_volumeIn
            << " m_spanLog2: " << settings.m_spanLog2
            << " m_audioMute: " << settings.m_audioMute
            << " m_agcActive: " << settings.m_agc
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    if ((m_settings.m_volume != settings.m_volume) || force)
    {
        m_volume = settings.m_volume;
        m_volume /= 4.0; // for 3276.8
    }

    m_spanLog2 = settings.m_spanLog2;
    m_audioMute = settings.m_audioMute;
    m_agcActive = settings.m_agc;
    m_settings = settings;
}

void FreeDVDemodSink::getSNRLevels(double& avg, double& peak, int& nbSamples)
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

void FreeDVDemodSink::resyncFreeDV()
{
    QMutexLocker mlock(&m_mutex);
    freedv_set_sync(m_freeDV, FREEDV_SYNC_UNSYNC);
}
