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

#include "boost/format.hpp"
#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>

#include "audio/audiooutputdevice.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"
#include "pipes/datapipes.h"
#include "util/db.h"
#include "maincore.h"

#include "rdsparser.h"
#include "bfmdemodsink.h"

const Real BFMDemodSink::default_deemphasis = 50.0; // 50 us
const int  BFMDemodSink::default_excursion = 750000; // +/- 75 kHz

BFMDemodSink::BFMDemodSink() :
    m_channel(nullptr),
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_audioSampleRate(48000),
    m_audioBufferFill(0),
    m_audioFifo(48000),
    m_pilotPLL(19000/384000, 50/384000, 0.01),
    m_deemphasisFilterX(default_deemphasis * 48000 * 1.0e-6),
    m_deemphasisFilterY(default_deemphasis * 48000 * 1.0e-6),
	m_fmExcursion(default_excursion)
{
    m_magsq = 0.0f;
    m_magsqSum = 0.0f;
    m_magsqPeak = 0.0f;
    m_magsqCount = 0;

    m_squelchLevel = 0;
    m_squelchState = 0;

    m_interpolatorDistance = 0.0f;
    m_interpolatorDistanceRemain = 0.0f;

    m_interpolatorRDSDistance = 0.0f;
    m_interpolatorRDSDistanceRemain = 0.0f;

    m_interpolatorStereoDistance = 0.0f;
    m_interpolatorStereoDistanceRemain = 0.0f;

    m_spectrumSink = nullptr;
    m_m1Arg = 0;

    m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, filtFftLen);

	m_deemphasisFilterX.configure(default_deemphasis * m_audioSampleRate * 1.0e-6);
	m_deemphasisFilterY.configure(default_deemphasis * m_audioSampleRate * 1.0e-6);
 	m_phaseDiscri.setFMScaling(384000/m_fmExcursion);

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

    m_demodBuffer.resize(1<<13);
    m_demodBufferFill = 0;

	applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

BFMDemodSink::~BFMDemodSink()
{
    delete m_rfFilter;
}

void BFMDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci, cs, cr;
	fftfilt::cmplx *rf;
	int rf_out;
	double msq;
	Real demod;

	m_sampleBuffer.clear();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real() / SDR_RX_SCALEF, it->imag() / SDR_RX_SCALEF);
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i =0 ; i  <rf_out; i++)
		{
			msq = rf[i].real()*rf[i].real() + rf[i].imag()*rf[i].imag();
            m_magsqSum += msq;

            if (msq > m_magsqPeak) {
                m_magsqPeak = msq;
            }

            m_magsqCount++;

			if (msq >= m_squelchLevel)
			{
			    if (m_squelchState < m_settings.m_rfBandwidth / 10) { // twice attack and decay rate
			        m_squelchState++;
			    }
			}
			else
			{
			    if (m_squelchState > 0) {
			        m_squelchState--;
			    }
			}

			if (m_squelchState > m_settings.m_rfBandwidth / 20) { // squelch open
				demod = m_phaseDiscri.phaseDiscriminator(rf[i]);
			} else {
				demod = 0;
			}

			if (!m_settings.m_showPilot) {
				m_sampleBuffer.push_back(Sample(demod * SDR_RX_SCALEF, 0.0));
			}

			if (m_settings.m_rdsActive)
			{
				//Complex r(demod * 2.0 * std::cos(3.0 * m_pilotPLLSamples[3]), 0.0);
				Complex r(demod * 2.0 * std::cos(3.0 * m_pilotPLLSamples[3]), 0.0);

				if (m_interpolatorRDS.decimate(&m_interpolatorRDSDistanceRemain, r, &cr))
				{
					bool bit;

					if (m_rdsDemod.process(cr.real(), bit))
					{
						if (m_rdsDecoder.frameSync(bit)) {
						    m_rdsParser.parseGroup(m_rdsDecoder.getGroup());
						}
					}

					m_interpolatorRDSDistanceRemain += m_interpolatorRDSDistance;
				}
			}

			Real sampleStereo = 0.0f;

			// Process stereo if stereo mode is selected

			if (m_settings.m_audioStereo)
			{
				m_pilotPLL.process(demod, m_pilotPLLSamples);

				if (m_settings.m_showPilot) {
					m_sampleBuffer.push_back(Sample(m_pilotPLLSamples[1] * SDR_RX_SCALEF, 0.0)); // debug 38 kHz pilot
				}

				if (m_settings.m_lsbStereo)
				{
					// 1.17 * 0.7 = 0.819
					Complex s(demod * m_pilotPLLSamples[1], demod * m_pilotPLLSamples[2]);

					if (m_interpolatorStereo.decimate(&m_interpolatorStereoDistanceRemain, s, &cs))
					{
						sampleStereo = cs.real() + cs.imag();
						m_interpolatorStereoDistanceRemain += m_interpolatorStereoDistance;
					}
				}
				else
				{
					Complex s(demod * 1.17 * m_pilotPLLSamples[1], 0);

					if (m_interpolatorStereo.decimate(&m_interpolatorStereoDistanceRemain, s, &cs))
					{
						sampleStereo = cs.real();
						m_interpolatorStereoDistanceRemain += m_interpolatorStereoDistance;
					}
				}
			}

			Complex e(demod, 0);

			if (m_interpolator.decimate(&m_interpolatorDistanceRemain, e, &ci))
			{
				if (m_settings.m_audioStereo)
				{
					Real deemph_l, deemph_r; // Pre-emphasis is applied on each channel before multiplexing
					m_deemphasisFilterX.process(ci.real() + sampleStereo, deemph_l);
					m_deemphasisFilterY.process(ci.real() - sampleStereo, deemph_r);
                    m_audioBuffer[m_audioBufferFill].l = (qint16)(deemph_l * (1<<12) * m_settings.m_volume);
                    m_audioBuffer[m_audioBufferFill].r = (qint16)(deemph_r * (1<<12) * m_settings.m_volume);
				}
				else
				{
					Real deemph;
					m_deemphasisFilterX.process(ci.real(), deemph);
					quint16 sample = (qint16)(deemph * (1<<12) * m_settings.m_volume);
					m_audioBuffer[m_audioBufferFill].l = sample;
					m_audioBuffer[m_audioBufferFill].r = sample;
				}

                m_demodBuffer[m_demodBufferFill++] = m_audioBuffer[m_audioBufferFill].l;
                m_demodBuffer[m_demodBufferFill++] = m_audioBuffer[m_audioBufferFill].r;

				++m_audioBufferFill;

				if (m_audioBufferFill >= m_audioBuffer.size())
				{
					std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

					if(res != m_audioBufferFill) {
						qDebug("BFMDemodSink::feed: %lu/%lu audio samples written", res, m_audioBufferFill);
					}

					m_audioBufferFill = 0;
				}

                if (m_demodBufferFill >= m_demodBuffer.size())
                {
                    QList<ObjectPipe*> dataPipes;
                    MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

                    if (dataPipes.size() > 0)
                    {
                        QList<ObjectPipe*>::iterator it = dataPipes.begin();

                        for (; it != dataPipes.end(); ++it)
                        {
                            DataFifo *fifo = qobject_cast<DataFifo*>((*it)->m_element);

                            if (fifo) {
                                fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeCI16);
                            }
                        }
                    }

                    m_demodBufferFill = 0;
                }

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

void BFMDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("BFMDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    qDebug("BFMDemodSink::applyAudioSampleRate: %u", sampleRate);

    m_interpolator.create(16, m_channelSampleRate, m_settings.m_afBandwidth);
    m_interpolatorDistanceRemain = (Real) m_channelSampleRate / sampleRate;
    m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) sampleRate;

    m_interpolatorStereo.create(16, m_channelSampleRate, m_settings.m_afBandwidth);
    m_interpolatorStereoDistanceRemain = (Real) m_channelSampleRate / sampleRate;
    m_interpolatorStereoDistance =  (Real) m_channelSampleRate / (Real) sampleRate;

    m_deemphasisFilterX.configure(default_deemphasis * sampleRate * 1.0e-6);
    m_deemphasisFilterY.configure(default_deemphasis * sampleRate * 1.0e-6);

    m_audioSampleRate = sampleRate;
}

void BFMDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "BFMDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_pilotPLL.configure(19000.0/channelSampleRate, 50.0/channelSampleRate, 0.01);

        m_interpolator.create(16, channelSampleRate, m_settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) channelSampleRate / m_audioSampleRate;
        m_interpolatorDistance =  (Real) channelSampleRate / (Real) m_audioSampleRate;

        m_interpolatorStereo.create(16, channelSampleRate, m_settings.m_afBandwidth);
        m_interpolatorStereoDistanceRemain = (Real) channelSampleRate / m_audioSampleRate;
        m_interpolatorStereoDistance =  (Real) channelSampleRate / (Real) m_audioSampleRate;

        m_interpolatorRDS.create(4, channelSampleRate, 600.0);
        m_interpolatorRDSDistanceRemain = (Real) channelSampleRate / 250000.0;
        m_interpolatorRDSDistance =  (Real) channelSampleRate / 250000.0;

        Real lowCut = -(m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        Real hiCut  = (m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_phaseDiscri.setFMScaling(channelSampleRate / m_fmExcursion);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void BFMDemodSink::applySettings(const BFMDemodSettings& settings, bool force)
{
    qDebug() << "BFMDemodSink::applySettings: MsgConfigureBFMDemod:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioStereo: " << settings.m_audioStereo
            << " m_lsbStereo: " << settings.m_lsbStereo
            << " m_showPilot: " << settings.m_showPilot
            << " m_rdsActive: " << settings.m_rdsActive
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " force: " << force;

    if ((settings.m_audioStereo && (settings.m_audioStereo != m_settings.m_audioStereo)) || force) {
        m_pilotPLL.configure(19000.0/m_channelSampleRate, 50.0/m_channelSampleRate, 0.01);
    }

    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) m_channelSampleRate / m_audioSampleRate;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) m_audioSampleRate;

        m_interpolatorStereo.create(16, m_channelSampleRate, settings.m_afBandwidth);
        m_interpolatorStereoDistanceRemain = (Real) m_channelSampleRate / m_audioSampleRate;
        m_interpolatorStereoDistance =  (Real) m_channelSampleRate / (Real) m_audioSampleRate;

        m_interpolatorRDS.create(4, m_channelSampleRate, 600.0);
        m_interpolatorRDSDistanceRemain = (Real) m_channelSampleRate / 250000.0;
        m_interpolatorRDSDistance =  (Real) m_channelSampleRate / 250000.0;

        m_lowpass.create(21, m_audioSampleRate, settings.m_afBandwidth);
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        Real lowCut = -(settings.m_rfBandwidth / 2.0) / m_channelSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.0) / m_channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_phaseDiscri.setFMScaling(m_channelSampleRate / m_fmExcursion);
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        m_squelchLevel = std::pow(10.0, settings.m_squelch / 10.0);
    }

    m_settings = settings;
}
