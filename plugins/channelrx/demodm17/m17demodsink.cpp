///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGDSDDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGDSDDemodReport.h"

#include "dsp/dspengine.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"
#include "dsp/dspcommands.h"
#include "feature/feature.h"
#include "audio/audiooutputdevice.h"
#include "util/db.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "m17demodsink.h"

M17DemodSink::M17DemodSink() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_audioSampleRate(48000),
    m_interpolatorDistance(0.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_sampleCount(0),
    m_squelchCount(0),
    m_squelchGate(0),
    m_squelchLevel(1e-4),
    m_squelchOpen(false),
    m_squelchWasOpen(false),
    m_squelchDelayLine(24000),
    m_audioFifo(48000),
    m_scopeXY(nullptr),
    m_scopeEnabled(true)
{
	m_audioBuffer.resize(1<<14);
    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    m_m17DemodProcessor.setAudioFifo(&m_audioFifo);

	m_sampleBuffer = new FixReal[1<<17]; // 128 kS
	m_sampleBufferIndex = 0;
	m_scaleFromShort = SDR_RX_SAMP_SZ < sizeof(short)*8 ? 1 : 1<<(SDR_RX_SAMP_SZ - sizeof(short)*8);

	m_magsq = 0.0f;
    m_magsqSum = 0.0f;
    m_magsqPeak = 0.0f;
    m_magsqCount = 0;

	applySettings(m_settings, QList<QString>(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

M17DemodSink::~M17DemodSink()
{
    delete[] m_sampleBuffer;
}

void M17DemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;
    int samplesPerSymbol = 10;

	m_scopeSampleBuffer.clear();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
        {
            FixReal sample, delayedSample;
            qint16 sampleM17;

            Real re = ci.real() / SDR_RX_SCALED;
            Real im = ci.imag() / SDR_RX_SCALED;
            Real magsq = re*re + im*im;
            m_movingAverage(magsq);

            m_magsqSum += magsq;

            if (magsq > m_magsqPeak) {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;

            Real demod = m_phaseDiscri.phaseDiscriminator(ci);
            m_sampleCount++;

            // AF processing

            if (m_movingAverage.asDouble() > m_squelchLevel)
            {
                if (m_squelchGate > 0)
                {

                    if (m_squelchCount < m_squelchGate*2) {
                        m_squelchCount++;
                    }

                    m_squelchDelayLine.write(demod);
                    m_squelchOpen = m_squelchCount > m_squelchGate;
                }
                else
                {
                    m_squelchOpen = true;
                }
            }
            else
            {
                if (m_squelchGate > 0)
                {
                    if (m_squelchCount > 0) {
                        m_squelchCount--;
                    }

                    m_squelchDelayLine.write(0);
                    m_squelchOpen = m_squelchCount > m_squelchGate;
                }
                else
                {
                    m_squelchOpen = false;
                }
            }

            if (m_squelchOpen)
            {
                if (m_squelchGate > 0)
                {
                    sampleM17 = m_squelchDelayLine.readBack(m_squelchGate) * 32768.0f;   // M17 decoder takes int16 samples
                    m_m17DemodProcessor.pushSample(sampleM17);
                    sample = m_squelchDelayLine.readBack(m_squelchGate) * SDR_RX_SCALEF; // scale to sample size
                }
                else
                {
                    sampleM17 = demod * 32768.0f;   // M17 decoder takes int16 samples
                    m_m17DemodProcessor.pushSample(sampleM17);
                    sample = demod * SDR_RX_SCALEF; // scale to sample size
                }
            }
            else
            {
                sampleM17 = 0;
                sample = 0;

                if (m_squelchWasOpen)
                {
                    if (m_m17DemodProcessor.getStreamElsePacket()) { // if packet kepp last values
                        m_m17DemodProcessor.resetInfo();
                    }

                    m_m17DemodProcessor.setDCDOff(); // indicate loss of carrier
                }
            }

            m_squelchWasOpen = m_squelchOpen;

            m_demodBuffer[m_demodBufferFill] = sampleM17;
            ++m_demodBufferFill;

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
                            fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
                        }
                    }
                }

                m_demodBufferFill = 0;
            }

            // if (m_settings.m_enableCosineFiltering) { // show actual input to FSK demod
            // 	sample = m_dsdDecoder.getFilteredSample() * m_scaleFromShort;
            // }

            if (m_sampleBufferIndex < (1<<17)-1) {
                m_sampleBufferIndex++;
            } else {
                m_sampleBufferIndex = 0;
            }

            m_sampleBuffer[m_sampleBufferIndex] = sample;

            if (m_sampleBufferIndex < samplesPerSymbol) {
                delayedSample = m_sampleBuffer[(1<<17) - samplesPerSymbol + m_sampleBufferIndex]; // wrap
            } else {
                delayedSample = m_sampleBuffer[m_sampleBufferIndex - samplesPerSymbol];
            }

            // if (m_settings.m_syncOrConstellation)
            // {
            //     Sample s(sample, m_dsdDecoder.getSymbolSyncSample() * m_scaleFromShort * 0.84);
            //     m_scopeSampleBuffer.push_back(s);
            // }
            // else
            // {
                Sample s(sample, delayedSample); // I=signal, Q=signal delayed by 20 samples (2400 baud: lowest rate)
                m_scopeSampleBuffer.push_back(s);
            // }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
	}

    if ((m_scopeXY != nullptr) && (m_scopeEnabled))
    {
        m_scopeXY->feed(m_scopeSampleBuffer.begin(), m_scopeSampleBuffer.end(), true); // true = real samples for what it's worth
    }
}

void M17DemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("M17DemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    int upsampling = sampleRate / 8000;

    qDebug("M17DemodSink::applyAudioSampleRate: audio rate: %d upsample by %d", sampleRate, upsampling);

    if (sampleRate % 8000 != 0) {
        qDebug("M17DemodSink::applyAudioSampleRate: audio will sound best with sample rates that are integer multiples of 8 kS/s");
    }

    m_m17DemodProcessor.setUpsampling(upsampling);
    m_audioSampleRate = sampleRate;

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(m_channel, sampleRate);
            messageQueue->push(msg);
        }
    }
}

void M17DemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "DSDDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " inputFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, (m_settings.m_rfBandwidth) / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) channelSampleRate / (Real) 48000;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}


void M17DemodSink::applySettings(const M17DemodSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "M17DemodSink::applySettings: "
            << " settingsKeys: " << settingsKeys
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_volume: " << settings.m_volume
            << " m_baudRate: " << settings.m_baudRate
            << " m_squelchGate" << settings.m_squelchGate
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_syncOrConstellation: " << settings.m_syncOrConstellation
            << " m_highPassFilter: "<< settings.m_highPassFilter
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_traceLengthMutliplier: " << settings.m_traceLengthMutliplier
            << " m_traceStroke: " << settings.m_traceStroke
            << " m_traceDecay: " << settings.m_traceDecay
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    if (settingsKeys.contains("rfBandwidth") || force)
    {
        m_interpolator.create(16, m_channelSampleRate, (settings.m_rfBandwidth) / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) 48000;
        //m_phaseDiscri.setFMScaling((float) settings.m_rfBandwidth / (float) settings.m_fmDeviation);
    }

    if (settingsKeys.contains("fmDeviation") || force) {
        m_phaseDiscri.setFMScaling(48000.0f / (2.0f*settings.m_fmDeviation));
    }

    if (settingsKeys.contains("squelchGate") || force)
    {
        m_squelchGate = 480 * settings.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
        m_squelchCount = 0; // reset squelch open counter
    }

    if (settingsKeys.contains("squelch") || force) {
        m_squelchLevel = std::pow(10.0, settings.m_squelch / 10.0); // input is a value in dB
    }

    if (settingsKeys.contains("audioMute") || force) {
        m_m17DemodProcessor.setAudioMute(settings.m_audioMute);
    }

    if (settingsKeys.contains("volume") || force) {
        m_m17DemodProcessor.setVolume(settings.m_volume);
    }

    if (settingsKeys.contains("baudRate") || force)
    {
        // m_dsdDecoder.setBaudRate(settings.m_baudRate); (future)
    }

    if (settingsKeys.contains("highPassFilter") || force) {
        m_m17DemodProcessor.setHP(settings.m_highPassFilter);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void M17DemodSink::configureMyPosition(float myLatitude, float myLongitude)
{
    m_latitude = myLatitude;
    m_longitude = myLongitude;
}

