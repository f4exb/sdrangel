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

#include "dsp/basebandsamplesink.h"
#include "freedvmodsource.h"

const int FreeDVModSource::m_levelNbSamples = 80; // every 10ms
const int FreeDVModSource::m_ssbFftLen = 1024;

FreeDVModSource::FreeDVModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_modemSampleRate(48000), // // default 2400A mode
    m_lowCutoff(0.0),
    m_hiCutoff(6000.0),
    m_SSBFilter(nullptr),
	m_SSBFilterBuffer(nullptr),
	m_SSBFilterBufferIndex(0),
    m_audioSampleRate(48000),
    m_audioFifo(12000),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
	m_freeDV(nullptr),
	m_nSpeechSamples(0),
	m_nNomModemSamples(0),
	m_iSpeech(0),
	m_iModem(0),
	m_speechIn(nullptr),
	m_modOut(nullptr),
	m_scaleFactor(SDR_TX_SCALEF),
    m_mutex(QMutex::Recursive)
{
    m_audioFifo.setLabel("FreeDVModSource.m_audioFifo");
    m_SSBFilter = new fftfilt(m_lowCutoff / m_audioSampleRate, m_hiCutoff / m_audioSampleRate, m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size
    std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer+(m_ssbFftLen>>1), Complex{0,0});

	m_audioBuffer.resize(24000);
	m_audioBufferFill = 0;
	m_audioReadBuffer.resize(24000);
	m_audioReadBufferFill = 0;

    m_sum.real(0.0f);
    m_sum.imag(0.0f);
    m_undersampleCount = 0;
    m_sumCount = 0;

	m_magsq = 0.0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

FreeDVModSource::~FreeDVModSource()
{

    delete m_SSBFilter;
    delete[] m_SSBFilterBuffer;

    if (m_freeDV) {
        freedv_close(m_freeDV);
    }
}

void FreeDVModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    QMutexLocker mlock(&m_mutex);
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void FreeDVModSource::pullOne(Sample& sample)
{
	Complex ci;

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

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
	m_movingAverage(magsq);
	m_magsq = m_movingAverage.asDouble();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void FreeDVModSource::prefetch(unsigned int nbSamples)
{
    unsigned int nbSamplesAudio = nbSamples * ((Real) m_audioSampleRate / (Real) m_channelSampleRate);
    pullAudio(nbSamplesAudio);
}

void FreeDVModSource::pullAudio(unsigned int nbSamples)
{
    QMutexLocker mlock(&m_mutex);
    unsigned int nbSamplesAudio = nbSamples * ((Real) m_audioSampleRate / (Real) m_modemSampleRate);

    if (nbSamplesAudio > m_audioBuffer.size()) {
        m_audioBuffer.resize(nbSamplesAudio);
    }

    std::copy(&m_audioReadBuffer[0], &m_audioReadBuffer[nbSamplesAudio], &m_audioBuffer[0]);
    m_audioBufferFill = 0;

    if (m_audioReadBufferFill > nbSamplesAudio) // copy back remaining samples at the start of the read buffer
    {
        std::copy(&m_audioReadBuffer[nbSamplesAudio], &m_audioReadBuffer[m_audioReadBufferFill], &m_audioReadBuffer[0]);
        m_audioReadBufferFill = m_audioReadBufferFill - nbSamplesAudio; // adjust current read buffer fill pointer
    }
}

qint16 FreeDVModSource::getAudioSample()
{
    qint16 sample;

    if (m_audioBufferFill < m_audioBuffer.size())
    {
        sample = (m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) * (m_settings.m_volumeFactor / 2.0f);
        m_audioBufferFill++;
    }
    else
    {
        unsigned int size = m_audioBuffer.size();
        qDebug("FreeDVModSource::getAudioSample: starve audio samples: size: %u", size);
        sample = (m_audioBuffer[size-1].l + m_audioBuffer[size-1].r) * (m_settings.m_volumeFactor / 2.0f);
    }

    return sample;
}

void FreeDVModSource::modulateSample()
{
    pullAF(m_modSample);
    if (!m_settings.m_gaugeInputElseModem) {
        calculateLevel(m_modSample);
    }
}

void FreeDVModSource::pullAF(Complex& sample)
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
                if (m_ifstream && m_ifstream->is_open())
                {
                    std::fill(m_speechIn, m_speechIn + m_nSpeechSamples, 0);

                    if (m_ifstream->eof())
                    {
                        if (m_settings.m_playLoop)
                        {
                            m_ifstream->clear();
                            m_ifstream->seekg(0, std::ios::beg);
                        }
                    }

                    if (m_ifstream->eof())
                    {
                        std::fill(m_modOut, m_modOut + m_nNomModemSamples, 0);
                    }
                    else
                    {

                        m_ifstream->read(reinterpret_cast<char*>(m_speechIn), sizeof(int16_t) * m_nSpeechSamples);

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
                qint16 audioSample = getAudioSample();

                while (!m_audioResampler.downSample(audioSample, m_speechIn[i]))
                {
                    audioSample = getAudioSample();
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

        if (m_spectrumSink) {
            m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true); // SSB
        }

        m_sampleBuffer.clear();
    }

    sample = m_SSBFilterBuffer[m_SSBFilterBufferIndex++];
}

void FreeDVModSource::calculateLevel(Complex& sample)
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
        m_rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void FreeDVModSource::calculateLevel(qint16& sample)
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
        m_rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void FreeDVModSource::applyAudioSampleRate(unsigned int sampleRate)
{
    qDebug("FreeDVModSource::applyAudioSampleRate: %d", sampleRate);
    // TODO: put up simple IIR interpolator when sampleRate < m_modemSampleRate

    m_audioResampler.setDecimation(sampleRate / m_channelSampleRate);
    m_audioResampler.setAudioFilters(sampleRate, sampleRate, 250, 3300);

    m_audioSampleRate = sampleRate;
}

void FreeDVModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "FreeDVMod::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_modemSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_modemSampleRate, m_hiCutoff, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void FreeDVModSource::applyFreeDVMode(FreeDVModSettings::FreeDVMode mode)
{
    m_hiCutoff = FreeDVModSettings::getHiCutoff(mode);
    m_lowCutoff = FreeDVModSettings::getLowCutoff(mode);
    int modemSampleRate = FreeDVModSettings::getModSampleRate(mode);
    QMutexLocker mlock(&m_mutex);

    m_SSBFilter->create_filter(m_lowCutoff / modemSampleRate, m_hiCutoff / modemSampleRate);

    // baseband interpolator and filter
    if (modemSampleRate != m_modemSampleRate)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) modemSampleRate / (Real) m_channelSampleRate;
        m_interpolator.create(48, modemSampleRate, m_hiCutoff, 3.0);
        m_modemSampleRate = modemSampleRate;
    }

    // FreeDV object

    if (m_freeDV) {
        freedv_close(m_freeDV);
    }

    int fdv_mode = -1;

    switch(mode)
    {
    case FreeDVModSettings::FreeDVMode700C:
        fdv_mode = FREEDV_MODE_700C;
        m_scaleFactor = SDR_TX_SCALEF / 6.4f;
        break;
    case FreeDVModSettings::FreeDVMode700D:
        fdv_mode = FREEDV_MODE_700D;
        m_scaleFactor = SDR_TX_SCALEF / 3.2f;
        break;
    case FreeDVModSettings::FreeDVMode800XA:
        fdv_mode = FREEDV_MODE_800XA;
        m_scaleFactor = SDR_TX_SCALEF / 10.3f;
        break;
    case FreeDVModSettings::FreeDVMode1600:
        fdv_mode = FREEDV_MODE_1600;
        m_scaleFactor = SDR_TX_SCALEF / 4.0f;
        break;
    case FreeDVModSettings::FreeDVMode2400A:
    default:
        fdv_mode = FREEDV_MODE_2400A;
        m_scaleFactor = SDR_TX_SCALEF / 10.3f;
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

            m_speechIn = new int16_t[nSpeechSamples];
            m_nSpeechSamples = nSpeechSamples;
        }

        if (nNomModemSamples != m_nNomModemSamples)
        {
            if (m_modOut) {
                delete[] m_modOut;
            }

            m_modOut = new int16_t[nNomModemSamples];
            m_nNomModemSamples = nNomModemSamples;
        }

        m_iSpeech = 0;
        m_iModem = 0;

        qDebug() << "FreeDVMod::applyFreeDVMode:"
                << " fdv_mode: " << fdv_mode
                << " m_modemSampleRate: " << m_modemSampleRate
                << " m_lowCutoff: " << m_lowCutoff
                << " m_hiCutoff: " << m_hiCutoff
                << " Fs: " << Fs
                << " Rs: " << Rs
                << " m_nSpeechSamples: " << m_nSpeechSamples
                << " m_nNomModemSamples: " << m_nNomModemSamples;
    }
}

void FreeDVModSource::applySettings(const FreeDVModSettings& settings, bool force)
{
    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        m_toneNco.setFreq(settings.m_toneFrequency, m_channelSampleRate);
    }

    if ((settings.m_modAFInput != m_settings.m_modAFInput) || force)
    {
        if (settings.m_modAFInput == FreeDVModSettings::FreeDVModInputAudio) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    m_settings = settings;
}

void FreeDVModSource::handleAudio()
{
    unsigned int nbRead;

    while ((nbRead = m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioReadBuffer[m_audioReadBufferFill]), 4096)) != 0)
    {
        if (m_audioReadBufferFill + nbRead + 4096 < m_audioReadBuffer.size()) {
            m_audioReadBufferFill += nbRead;
        }
    }
}
