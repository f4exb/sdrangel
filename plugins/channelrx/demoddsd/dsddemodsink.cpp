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
#include "SWGRDSReport.h"

#include "dsp/dspengine.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/datafifo.h"
#include "dsp/dspcommands.h"
#include "feature/feature.h"
#include "audio/audiooutputdevice.h"
#include "util/db.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "dsddemodsink.h"

DSDDemodSink::DSDDemodSink() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_ambeFeature(nullptr),
    m_audioSampleRate(48000),
    m_interpolatorDistance(0.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_sampleCount(0),
    m_squelchCount(0),
    m_squelchGate(0),
    m_squelchLevel(1e-4),
    m_squelchOpen(false),
    m_squelchDelayLine(24000),
    m_audioFifo1(48000),
    m_audioFifo2(48000),
    m_scopeXY(nullptr),
    m_scopeEnabled(true),
    m_dsdDecoder(),
    m_signalFormat(signalFormatNone)
{
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

	m_sampleBuffer = new FixReal[1<<17]; // 128 kS
	m_sampleBufferIndex = 0;
	m_scaleFromShort = SDR_RX_SAMP_SZ < sizeof(short)*8 ? 1 : 1<<(SDR_RX_SAMP_SZ - sizeof(short)*8);

	m_magsq = 0.0f;
    m_magsqSum = 0.0f;
    m_magsqPeak = 0.0f;
    m_magsqCount = 0;

	applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

DSDDemodSink::~DSDDemodSink()
{
    delete[] m_sampleBuffer;
}

void DSDDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{	Complex ci;
	int samplesPerSymbol = m_dsdDecoder.getSamplesPerSymbol();

	m_scopeSampleBuffer.clear();

	m_dsdDecoder.enableMbelib(!m_ambeFeature); // disable mbelib if DV serial support is present and activated else enable it

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
        {
            FixReal sample, delayedSample;
            qint16 sampleDSD;

            Real re = ci.real() / SDR_RX_SCALED;
            Real im = ci.imag() / SDR_RX_SCALED;
            Real magsq = re*re + im*im;
            m_movingAverage(magsq);

            m_magsqSum += magsq;

            if (magsq > m_magsqPeak)
            {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;

            Real demod = m_phaseDiscri.phaseDiscriminator(ci) * m_settings.m_demodGain; // [-1.0:1.0]
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
                    sampleDSD = m_squelchDelayLine.readBack(m_squelchGate) * 32768.0f;   // DSD decoder takes int16 samples
                    sample = m_squelchDelayLine.readBack(m_squelchGate) * SDR_RX_SCALEF; // scale to sample size
                }
                else
                {
                    sampleDSD = demod * 32768.0f;   // DSD decoder takes int16 samples
                    sample = demod * SDR_RX_SCALEF; // scale to sample size
                }
            }
            else
            {
                sampleDSD = 0;
                sample = 0;
            }

            m_dsdDecoder.pushSample(sampleDSD);

            m_demodBuffer[m_demodBufferFill] = sampleDSD;
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

            if (m_settings.m_enableCosineFiltering) { // show actual input to FSK demod
            	sample = m_dsdDecoder.getFilteredSample() * m_scaleFromShort;
            }

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

            if (m_settings.m_syncOrConstellation)
            {
                Sample s(sample, m_dsdDecoder.getSymbolSyncSample() * m_scaleFromShort * 0.84);
                m_scopeSampleBuffer.push_back(s);
            }
            else
            {
                Sample s(sample, delayedSample); // I=signal, Q=signal delayed by 20 samples (2400 baud: lowest rate)
                m_scopeSampleBuffer.push_back(s);
            }

            if (m_ambeFeature && isNotYSFWide())
            {
                if ((m_settings.m_slot1On) && m_dsdDecoder.mbeDVReady1())
                {
                    if (!m_settings.m_audioMute)
                    {
                        DSPPushMbeFrame *msg = new DSPPushMbeFrame(
                            m_dsdDecoder.getMbeDVFrame1(),
                            m_dsdDecoder.getMbeRateIndex(),
                            m_settings.m_volume * 10.0,
                            m_settings.m_tdmaStereo ? 1 : 3, // left or both channels
                            m_settings.m_highPassFilter,
                            m_audioSampleRate/8000, // upsample from native 8k
                            &m_audioFifo1
                        );
                        m_ambeFeature->handleMessage(*msg);
                        delete msg;
                    }

                    m_dsdDecoder.resetMbeDV1();
                }

                if ((m_settings.m_slot2On) && m_dsdDecoder.mbeDVReady2())
                {
                    if (!m_settings.m_audioMute)
                    {
                        DSPPushMbeFrame *msg = new DSPPushMbeFrame(
                            m_dsdDecoder.getMbeDVFrame2(),
                            m_dsdDecoder.getMbeRateIndex(),
                            m_settings.m_volume * 10.0,
                            m_settings.m_tdmaStereo ? 2 : 3, // right or both channels
                            m_settings.m_highPassFilter,
                            m_audioSampleRate/8000, // upsample from native 8k
                            &m_audioFifo2
                        );
                        m_ambeFeature->handleMessage(*msg);
                        delete msg;
                    }

                    m_dsdDecoder.resetMbeDV2();
                }
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
	}

    if (!m_ambeFeature)
	{
	    if (m_settings.m_slot1On)
	    {
	        int nbAudioSamples;
	        short *dsdAudio = m_dsdDecoder.getAudio1(nbAudioSamples);

	        if (nbAudioSamples > 0)
	        {
	            if (!m_settings.m_audioMute) {
	                m_audioFifo1.write((const quint8*) dsdAudio, nbAudioSamples);
	            }

	            m_dsdDecoder.resetAudio1();
	        }
	    }

        if (m_settings.m_slot2On)
        {
            int nbAudioSamples;
            short *dsdAudio = m_dsdDecoder.getAudio2(nbAudioSamples);

            if (nbAudioSamples > 0)
            {
                if (!m_settings.m_audioMute) {
                    m_audioFifo2.write((const quint8*) dsdAudio, nbAudioSamples);
                }

                m_dsdDecoder.resetAudio2();
            }
        }
	}

    if ((m_scopeXY != nullptr) && (m_scopeEnabled))
    {
        m_scopeXY->feed(m_scopeSampleBuffer.begin(), m_scopeSampleBuffer.end(), true); // true = real samples for what it's worth
    }
}

void DSDDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("DSDDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    int upsampling = sampleRate / 8000;

    qDebug("DSDDemodSink::applyAudioSampleRate: audio rate: %d upsample by %d", sampleRate, upsampling);

    if (sampleRate % 8000 != 0) {
        qDebug("DSDDemodSink::applyAudioSampleRate: audio will sound best with sample rates that are integer multiples of 8 kS/s");
    }

    m_dsdDecoder.setUpsampling(upsampling);
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

void DSDDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
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


void DSDDemodSink::applySettings(const DSDDemodSettings& settings, bool force)
{
    qDebug() << "DSDDemodSink::applySettings: "
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_demodGain: " << settings.m_demodGain
            << " m_volume: " << settings.m_volume
            << " m_baudRate: " << settings.m_baudRate
            << " m_squelchGate" << settings.m_squelchGate
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_enableCosineFiltering: " << settings.m_enableCosineFiltering
            << " m_syncOrConstellation: " << settings.m_syncOrConstellation
            << " m_slot1On: " << settings.m_slot1On
            << " m_slot2On: " << settings.m_slot2On
            << " m_tdmaStereo: " << settings.m_tdmaStereo
            << " m_pllLock: " << settings.m_pllLock
            << " m_highPassFilter: "<< settings.m_highPassFilter
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_traceLengthMutliplier: " << settings.m_traceLengthMutliplier
            << " m_traceStroke: " << settings.m_traceStroke
            << " m_traceDecay: " << settings.m_traceDecay
            << " m_streamIndex: " << settings.m_streamIndex
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, (settings.m_rfBandwidth) / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) 48000;
        //m_phaseDiscri.setFMScaling((float) settings.m_rfBandwidth / (float) settings.m_fmDeviation);
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling(48000.0f / (2.0f*settings.m_fmDeviation));
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        m_squelchGate = 480 * settings.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
        m_squelchCount = 0; // reset squelch open counter
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        // input is a value in dB
        m_squelchLevel = std::pow(10.0, settings.m_squelch / 10.0);
    }

    if ((settings.m_volume != m_settings.m_volume) || force)
    {
        m_dsdDecoder.setAudioGain(settings.m_volume);
    }

    if ((settings.m_baudRate != m_settings.m_baudRate) || force)
    {
        m_dsdDecoder.setBaudRate(settings.m_baudRate);
    }

    if ((settings.m_enableCosineFiltering != m_settings.m_enableCosineFiltering) || force)
    {
        m_dsdDecoder.enableCosineFiltering(settings.m_enableCosineFiltering);
    }

    if ((settings.m_tdmaStereo != m_settings.m_tdmaStereo) || force)
    {
        m_dsdDecoder.setTDMAStereo(settings.m_tdmaStereo);
    }

    if ((settings.m_pllLock != m_settings.m_pllLock) || force)
    {
        m_dsdDecoder.setSymbolPLLLock(settings.m_pllLock);
    }

    if ((settings.m_highPassFilter != m_settings.m_highPassFilter) || force)
    {
        m_dsdDecoder.useHPMbelib(settings.m_highPassFilter);
    }

    m_settings = settings;
}

void DSDDemodSink::configureMyPosition(float myLatitude, float myLongitude)
{
    m_dsdDecoder.setMyPoint(myLatitude, myLongitude);
}

const char *DSDDemodSink::updateAndGetStatusText()
{
    formatStatusText();
    return m_formatStatusText;
}

bool DSDDemodSink::isNotYSFWide()
{
    if (getDecoder().getSyncType() == DSDcc::DSDDecoder::DSDSyncYSF) {
        return getDecoder().getYSFDecoder().getFICH().isNarrowMode();
    }

    return true;
}

void DSDDemodSink::formatStatusText()
{
    switch (getDecoder().getSyncType())
    {
    case DSDcc::DSDDecoder::DSDSyncDMRDataMS:
    case DSDcc::DSDDecoder::DSDSyncDMRDataP:
    case DSDcc::DSDDecoder::DSDSyncDMRVoiceMS:
    case DSDcc::DSDDecoder::DSDSyncDMRVoiceP:
        if (m_signalFormat != signalFormatDMR)
        {
            strcpy(m_formatStatusText, "Sta: __ S1: __________________________ S2: __________________________");
        }

        switch (getDecoder().getStationType())
        {
        case DSDcc::DSDDecoder::DSDBaseStation:
            memcpy(&m_formatStatusText[5], "BS ", 3);
            break;
        case DSDcc::DSDDecoder::DSDMobileStation:
            memcpy(&m_formatStatusText[5], "MS ", 3);
            break;
        default:
            memcpy(&m_formatStatusText[5], "NA ", 3);
            break;
        }

        memcpy(&m_formatStatusText[12], getDecoder().getDMRDecoder().getSlot0Text(), 26);
        memcpy(&m_formatStatusText[43], getDecoder().getDMRDecoder().getSlot1Text(), 26);
        m_signalFormat = signalFormatDMR;
        break;
    case DSDcc::DSDDecoder::DSDSyncDStarHeaderN:
    case DSDcc::DSDDecoder::DSDSyncDStarHeaderP:
    case DSDcc::DSDDecoder::DSDSyncDStarN:
    case DSDcc::DSDDecoder::DSDSyncDStarP:
        if (m_signalFormat != signalFormatDStar)
        {
                                     //           1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
                                     // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
            strcpy(m_formatStatusText, "________/____>________|________>________|____________________|______:___/_____._");
                                     // MY            UR       RPT1     RPT2     Info                 Loc    Target
        }

        {
            const std::string& rpt1 = getDecoder().getDStarDecoder().getRpt1();
            const std::string& rpt2 = getDecoder().getDStarDecoder().getRpt2();
            const std::string& mySign = getDecoder().getDStarDecoder().getMySign();
            const std::string& yrSign = getDecoder().getDStarDecoder().getYourSign();

            if (rpt1.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[23], rpt1.c_str(), 8);
            }
            if (rpt2.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[32], rpt2.c_str(), 8);
            }
            if (yrSign.length() > 0) { // 0 or 8
                memcpy(&m_formatStatusText[14], yrSign.c_str(), 8);
            }
            if (mySign.length() > 0) { // 0 or 13
                memcpy(&m_formatStatusText[0], mySign.c_str(), 13);
            }
            memcpy(&m_formatStatusText[41], getDecoder().getDStarDecoder().getInfoText(), 20);
            memcpy(&m_formatStatusText[62], getDecoder().getDStarDecoder().getLocator(), 6);
            snprintf(&m_formatStatusText[69], 82-69, "%03d/%07.1f",
                    getDecoder().getDStarDecoder().getBearing(),
                    getDecoder().getDStarDecoder().getDistance());
        }

        m_formatStatusText[82] = '\0';
        m_signalFormat = signalFormatDStar;
        break;
    case DSDcc::DSDDecoder::DSDSyncDPMR:
        snprintf(m_formatStatusText, 82, "%s CC: %04d OI: %08d CI: %08d",
                DSDcc::DSDdPMR::dpmrFrameTypes[(int) getDecoder().getDPMRDecoder().getFrameType()],
                getDecoder().getDPMRDecoder().getColorCode(),
                getDecoder().getDPMRDecoder().getOwnId(),
                getDecoder().getDPMRDecoder().getCalledId());
        m_signalFormat = signalFormatDPMR;
        break;
    case DSDcc::DSDDecoder::DSDSyncNXDNP:
    case DSDcc::DSDDecoder::DSDSyncNXDNN:
        if (getDecoder().getNXDNDecoder().getRFChannel() == DSDcc::DSDNXDN::NXDNRCCH)
        {
            //           1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
            // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
            // RC r cc mm llllll ssss
            snprintf(m_formatStatusText, 82, "RC %s %02d %02X %06X %02X",
                    getDecoder().getNXDNDecoder().isFullRate() ? "F" : "H",
                    getDecoder().getNXDNDecoder().getRAN(),
                    getDecoder().getNXDNDecoder().getMessageType(),
                    getDecoder().getNXDNDecoder().getLocationId(),
                    getDecoder().getNXDNDecoder().getServicesFlag());
        }
        else if ((getDecoder().getNXDNDecoder().getRFChannel() == DSDcc::DSDNXDN::NXDNRTCH)
            || (getDecoder().getNXDNDecoder().getRFChannel() == DSDcc::DSDNXDN::NXDNRDCH))
        {
            if (getDecoder().getNXDNDecoder().isIdle()) {
                snprintf(m_formatStatusText, 82, "%s IDLE", getDecoder().getNXDNDecoder().getRFChannelStr());
            }
            else
            {
                //           1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
                // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
                // Rx r cc mm sssss>gddddd
                snprintf(m_formatStatusText, 82, "%s %s %02d %02X %05d>%c%05d",
                        getDecoder().getNXDNDecoder().getRFChannelStr(),
                        getDecoder().getNXDNDecoder().isFullRate() ? "F" : "H",
                        getDecoder().getNXDNDecoder().getRAN(),
                        getDecoder().getNXDNDecoder().getMessageType(),
                        getDecoder().getNXDNDecoder().getSourceId(),
                        getDecoder().getNXDNDecoder().isGroupCall() ? 'G' : 'I',
                        getDecoder().getNXDNDecoder().getDestinationId());
            }
        }
        else
        {
            //           1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
            // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
            // RU
            snprintf(m_formatStatusText, 82, "RU");
        }
        m_signalFormat = signalFormatNXDN;
        break;
    case DSDcc::DSDDecoder::DSDSyncYSF:
        //           1    1    2    2    3    3    4    4    5    5    6    6    7    7    8
        // 0....5....0....5....0....5....0....5....0....5....0....5....0....5....0....5....0..
        // C V2 RI 0:7 WL000|ssssssssss>dddddddddd |UUUUUUUUUU>DDDDDDDDDD|44444
        if (getDecoder().getYSFDecoder().getFICHError() == DSDcc::DSDYSF::FICHNoError)
        {
            snprintf(m_formatStatusText, 82, "%s ", DSDcc::DSDYSF::ysfChannelTypeText[(int) getDecoder().getYSFDecoder().getFICH().getFrameInformation()]);
        }
        else
        {
            snprintf(m_formatStatusText, 82, "%d ", (int) getDecoder().getYSFDecoder().getFICHError());
        }

        snprintf(&m_formatStatusText[2], 80, "%s %s %d:%d %c%c",
                DSDcc::DSDYSF::ysfDataTypeText[(int) getDecoder().getYSFDecoder().getFICH().getDataType()],
                DSDcc::DSDYSF::ysfCallModeText[(int) getDecoder().getYSFDecoder().getFICH().getCallMode()],
                getDecoder().getYSFDecoder().getFICH().getBlockTotal(),
                getDecoder().getYSFDecoder().getFICH().getFrameTotal(),
                (getDecoder().getYSFDecoder().getFICH().isNarrowMode() ? 'N' : 'W'),
                (getDecoder().getYSFDecoder().getFICH().isInternetPath() ? 'I' : 'L'));

        if (getDecoder().getYSFDecoder().getFICH().isSquelchCodeEnabled())
        {
            snprintf(&m_formatStatusText[14], 82-14, "%03d", getDecoder().getYSFDecoder().getFICH().getSquelchCode());
        }
        else
        {
            strncpy(&m_formatStatusText[14], "---", 82-14);
        }

        char dest[13];

        if (getDecoder().getYSFDecoder().radioIdMode())
        {
            snprintf(dest, 12, "%-5s:%-5s",
                    getDecoder().getYSFDecoder().getDestId(),
                    getDecoder().getYSFDecoder().getSrcId());
        }
        else
        {
            snprintf(dest, 11, "%-10s", getDecoder().getYSFDecoder().getDest());
        }

        snprintf(&m_formatStatusText[17], 82-17, "|%-10s>%s|%-10s>%-10s|%-5s",
                getDecoder().getYSFDecoder().getSrc(),
                dest,
                getDecoder().getYSFDecoder().getUplink(),
                getDecoder().getYSFDecoder().getDownlink(),
                getDecoder().getYSFDecoder().getRem4());

        m_signalFormat = signalFormatYSF;
        break;
    default:
        m_signalFormat = signalFormatNone;
        m_formatStatusText[0] = '\0';
        break;
    }

    m_formatStatusText[82] = '\0'; // guard
}
