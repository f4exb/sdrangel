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

#include <QTime>
#include <QDebug>

#include <stdio.h>
#include <complex.h>

#include "dsp/scopevis.h"
#include "atvdemodsink.h"

const int ATVDemodSink::m_ssbFftLen = 1024;

ATVDemodSink::ATVDemodSink() :
    m_channelSampleRate(1000000),
    m_channelFrequencyOffset(0),
    m_samplesPerLine(100),
	m_samplesPerLineFrac(0.0f),
    m_videoTabIndex(0),
    m_scopeSink(nullptr),
    m_registeredTVScreen(nullptr),
    m_numberSamplesPerHTop(0),
    m_fieldIndex(0),
    m_synchroSamples(0),
    m_effMin(20.0f),
    m_effMax(-20.0f),
    m_ampMin(-1.0f),
    m_ampMax(1.0f),
    m_ampDelta(2.0f),
    m_amSampleIndex(0),
    m_sampleOffset(0),
	m_sampleOffsetFrac(0.0f),
    m_sampleOffsetDetected(0),
    m_lineIndex(0),
	m_hSyncShift(0.0f),
    m_hSyncErrorCount(0),
    m_ampAverage(4800),
    m_bfoPLL(200/1000000, 100/1000000, 0.01),
    m_bfoFilter(200.0, 1000000.0, 0.9),
    m_DSBFilter(nullptr),
    m_DSBFilterBuffer(nullptr),
    m_DSBFilterBufferIndex(0)
{
    qDebug("ATVDemodSink::ATVDemodSink");
    //*************** ATV PARAMETERS  ***************
    m_synchroSamples=0;
    m_interleaved = true;

    m_DSBFilter = new fftfilt(m_settings.m_fftBandwidth / (float) m_channelSampleRate, 2*m_ssbFftLen); // arbitrary cutoff
    m_DSBFilterBuffer = new Complex[m_ssbFftLen];
    std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
    std::fill(m_fltBufferI, m_fltBufferI+6, 0.0f);
    std::fill(m_fltBufferQ, m_fltBufferQ+6, 0.0f);

    m_objPhaseDiscri.setFMScaling(1.0f);

	applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

ATVDemodSink::~ATVDemodSink()
{
    delete m_DSBFilter;
    delete[] m_DSBFilterBuffer;
}

void ATVDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    //********** Let's rock and roll buddy ! **********

    //********** Accessing ATV Screen context **********

    for (SampleVector::const_iterator it = begin; it != end; ++it /* ++it **/)
    {
        Complex c(it->real(), it->imag());

        if (m_settings.m_inputFrequencyOffset != 0) {
            c *= m_nco.nextIQ();
        }

        demod(c);
    }

    if ((m_videoTabIndex == 1) && (m_scopeSink)) // do only if scope tab is selected and scope is available
    {
        std::vector<SampleVector::const_iterator> vbegin;
        vbegin.push_back(m_scopeSampleBuffer.begin());
        m_scopeSink->feed(vbegin, m_scopeSampleBuffer.end() - m_scopeSampleBuffer.begin()); // m_ssb = positive only
        m_scopeSampleBuffer.clear();
    }
}

void ATVDemodSink::demod(Complex& c)
{
    float sampleNormI;
    float sampleNormQ;
    float sampleNorm;
    float sample;
    int sampleVideo;

    //********** FFT filtering **********

    if (m_settings.m_fftFiltering)
    {
        int n_out;
        Complex *filtered;

        n_out = m_DSBFilter->runAsym(c, &filtered, m_settings.m_atvModulation != ATVDemodSettings::ATV_LSB); // all usb except explicitely lsb

        if (n_out > 0)
        {
            std::copy(filtered, filtered + n_out, m_DSBFilterBuffer);
            m_DSBFilterBufferIndex = 0;
        }
        else if (m_DSBFilterBufferIndex < m_ssbFftLen - 1) // safe
        {
            m_DSBFilterBufferIndex++;
        }
    }

    //********** demodulation **********

    const float& fltI = m_settings.m_fftFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex].real() : c.real();
    const float& fltQ = m_settings.m_fftFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex].imag() : c.imag();
    double magSq;

    if ((m_settings.m_atvModulation == ATVDemodSettings::ATV_FM1) || (m_settings.m_atvModulation == ATVDemodSettings::ATV_FM2))
    {
        //Amplitude FM
        magSq = fltI*fltI + fltQ*fltQ;
        m_magSqAverage(magSq);
        sampleNorm = sqrt(magSq);
        sampleNormI = fltI/sampleNorm;
        sampleNormQ = fltQ/sampleNorm;

        //-2 > 2 : 0 -> 1 volt
        //0->0.3 synchro  0.3->1 image

        if (m_settings.m_atvModulation == ATVDemodSettings::ATV_FM1)
        {
            //YDiff Cd
            sample = m_fltBufferI[0]*(sampleNormQ - m_fltBufferQ[1]);
            sample -= m_fltBufferQ[0]*(sampleNormI - m_fltBufferI[1]);

            sample += 2.0f;
            sample /= 4.0f;

        }
        else
        {
            //YDiff Folded
            sample =  m_fltBufferI[2]*((m_fltBufferQ[5]-sampleNormQ)/16.0f + m_fltBufferQ[1] - m_fltBufferQ[3]);
            sample -= m_fltBufferQ[2]*((m_fltBufferI[5]-sampleNormI)/16.0f + m_fltBufferI[1] - m_fltBufferI[3]);

            sample += 2.125f;
            sample /= 4.25f;

            m_fltBufferI[5] = m_fltBufferI[4];
            m_fltBufferQ[5] = m_fltBufferQ[4];

            m_fltBufferI[4] = m_fltBufferI[3];
            m_fltBufferQ[4] = m_fltBufferQ[3];

            m_fltBufferI[3] = m_fltBufferI[2];
            m_fltBufferQ[3] = m_fltBufferQ[2];

            m_fltBufferI[2] = m_fltBufferI[1];
            m_fltBufferQ[2] = m_fltBufferQ[1];
        }

        m_fltBufferI[1] = m_fltBufferI[0];
        m_fltBufferQ[1] = m_fltBufferQ[0];

        m_fltBufferI[0] = sampleNormI;
        m_fltBufferQ[0] = sampleNormQ;

        if (m_settings.m_fmDeviation != 1.0f)
        {
            sample = ((sample - 0.5f) / m_settings.m_fmDeviation) + 0.5f;
        }
    }
    else if (m_settings.m_atvModulation == ATVDemodSettings::ATV_AM)
    {
        //Amplitude AM
        magSq = fltI*fltI + fltQ*fltQ;
        m_magSqAverage(magSq);
        sampleNorm = sqrt(magSq);
        float sampleRaw = sampleNorm / SDR_RX_SCALEF;
        m_ampAverage(sampleRaw);
        sample = sampleRaw / (2.0f * m_ampAverage.asFloat()); // AGC
    }
    else if ((m_settings.m_atvModulation == ATVDemodSettings::ATV_USB) || (m_settings.m_atvModulation == ATVDemodSettings::ATV_LSB))
    {
        magSq = fltI*fltI + fltQ*fltQ;
        m_magSqAverage(magSq);
        sampleNorm = sqrt(magSq);

        Real bfoValues[2];
        float fltFiltered = m_bfoFilter.run(fltI);
        m_bfoPLL.process(fltFiltered, bfoValues);

        // do the mix

        float mixI = fltI * bfoValues[0] - fltQ * bfoValues[1];
        float mixQ = fltI * bfoValues[1] + fltQ * bfoValues[0];

        if (m_settings.m_atvModulation == ATVDemodSettings::ATV_USB) {
            sample = (mixI + mixQ);
        } else {
            sample = (mixI - mixQ);
        }
    }
    else if (m_settings.m_atvModulation == ATVDemodSettings::ATV_FM3)
    {
        float rawDeviation;
        sample = m_objPhaseDiscri.phaseDiscriminatorDelta(c, magSq, rawDeviation) + 0.5f;
        m_magSqAverage(magSq);
        sampleNorm = sqrt(magSq);
    }
    else
    {
        magSq = fltI*fltI + fltQ*fltQ;
        m_magSqAverage(magSq);
        sampleNorm = sqrt(magSq);
        sample = 0.0f;
    }

    //********** AM sample normalization and coarse scale estimation **********

    if ((m_settings.m_atvModulation == ATVDemodSettings::ATV_AM)
        || (m_settings.m_atvModulation == ATVDemodSettings::ATV_USB)
        || (m_settings.m_atvModulation == ATVDemodSettings::ATV_LSB))
    {
        // Mini and Maxi Amplitude tracking

        if (sample < m_effMin) {
            m_effMin = sample;
        }

        if (sample > m_effMax) {
            m_effMax = sample;
        }

        if (m_amSampleIndex < m_samplesPerLine * m_settings.m_nbLines * 2) // calculate on two full images
        {
            m_amSampleIndex++;
        }
        else
        {
            // scale signal based on extrema on the estimation period
            m_ampMin = m_effMin;
            m_ampMax = m_effMax;
            m_ampDelta = (m_ampMax - m_ampMin);

            if (m_ampDelta <= 0.0) {
                m_ampDelta = 1.0f;
            }

            // readjustment
            m_ampDelta /= m_settings.m_amScalingFactor / 100.0f;
            m_ampMin += m_ampDelta * (m_settings.m_amOffsetFactor / 100.0f);

            // qDebug("ATVDemod::demod: m_ampMin: %f m_ampMax: %f m_ampDelta: %f", m_ampMin, m_ampMax, m_ampDelta);

            //Reset extrema
            m_effMin = 20.0f;
            m_effMax = -20.0f;

            m_amSampleIndex = 0;
        }

        //Normalisation of current sample
        sample -= m_ampMin;
        sample /= m_ampDelta;
    }

    sample = m_settings.m_invertVideo ? 1.0f - sample : sample;
    // 0.0 -> 1.0
    sample = (sample < 0.0f) ? 0.0f : (sample > 1.0f) ? 1.0f : sample;

    if ((m_videoTabIndex == 1) && (m_scopeSink != 0)) { // feed scope buffer only if scope is present and visible
        m_scopeSampleBuffer.push_back(Sample(sample * (SDR_RX_SCALEF - 1.0f), 0.0f));
    }

    //********** gray level **********
    // -0.3 -> 0.7 / 0.7
    sampleVideo = (int) ((sample - m_settings.m_levelBlack) * m_sampleRangeCorrection);

    // 0 -> 255
    sampleVideo = (sampleVideo < 0) ? 0 : (sampleVideo > 255) ? 255 : sampleVideo;

    //********** process video sample **********

    if (m_registeredTVScreen) // can process only if the screen is available (set via the GUI)
    {
        processSample(sample, sampleVideo);
    }
}

void ATVDemodSink::applyStandard(int sampleRate, ATVDemodSettings::ATVStd atvStd, float lineDuration)
{
    switch(atvStd)
    {
    case ATVDemodSettings::ATVStdHSkip:
        // what is left in a line for the image
        m_interleaved        = false; // irrelevant
        m_numberOfBlackLines = 0;
        m_numberSamplesHSyncCrop = (int) (0.09f * lineDuration * sampleRate); // 9% of full line empirically
        break;
    case ATVDemodSettings::ATVStdShort:
        // what is left in a line for the image
        m_interleaved        = false;
        m_numberOfVSyncLines = 2;
        m_numberOfBlackLines = 4;
        m_firstVisibleLine   = 3;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        break;
    case ATVDemodSettings::ATVStdShortInterleaved:
        // what is left in a line for the image
        m_interleaved        = true;
        m_numberOfVSyncLines = 2;
        m_numberOfBlackLines = 5;
        m_firstVisibleLine   = 3;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        break;
    case ATVDemodSettings::ATVStd819: // 819 lines standard F
        // what is left in a line for the image
        m_interleaved        = true;
        m_numberOfVSyncLines = 4;
        m_numberOfBlackLines = 59;
        m_firstVisibleLine   = 27;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        break;
    case ATVDemodSettings::ATVStdPAL525: // Follows PAL-M standard
        // what is left in a 64/1.008 us line for the image
        m_interleaved        = true;
        m_numberOfVSyncLines = 4;
        m_numberOfBlackLines = 45;
        m_firstVisibleLine   = 20;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        break;
    case ATVDemodSettings::ATVStdPAL625: // Follows PAL-B/G/H standard
    default:
        // what is left in a 64 us line for the image
        m_interleaved        = true;
        m_numberOfVSyncLines = 3;
        m_numberOfBlackLines = 49;
        m_firstVisibleLine   = 23;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
    }

    // for now all standards apply this

    // Rec. ITU-R BT.1700
    // Table 2. Details of line synchronizing signals
    m_numberSamplesPerLineSignals = (int)(lineDuration * sampleRate * 12.0  / 64.0); // "a", Line-blanking interval
    m_numberSamplesPerHSync       = (int)(lineDuration * sampleRate * 10.5  / 64.0); // "b", Interval between time datum and back edge of line-blanking pulse
    m_numberSamplesPerHTop        = (int)(lineDuration * sampleRate *  4.7  / 64.0); // "d", Duration of synchronizing pulse

    // Table 3. Details of field synchronizing signals
    float hl = 32.0f; // half of the line
    float p  = 2.35f; // "p", Duration of equalizing pulse
    float q  = 27.3f; // "q", Duration of field-synchronizing pulse

    // In the first half of the first line field index is detected
    m_fieldDetectStartPos = (int)(lineDuration * sampleRate * p / 64.0);
    m_fieldDetectEndPos   = (int)(lineDuration * sampleRate * q / 64.0);
    // In the second half of the first line vertical synchronization is detected
    m_vSyncDetectStartPos = (int)(lineDuration * sampleRate * (p + hl) / 64.0);
    m_vSyncDetectEndPos   = (int)(lineDuration * sampleRate * (q + hl) / 64.0);

    float fieldDetectPercent = 0.75f; // It is better not to detect field index than detect it wrong
    float detectTotalLen = lineDuration * sampleRate * (q - p) / 64.0; // same for field index and vSync detection
    m_fieldDetectThreshold1 = (int)(detectTotalLen * fieldDetectPercent);
    m_fieldDetectThreshold2 = (int)(detectTotalLen * (1.0f - fieldDetectPercent));

    float vSyncDetectPercent = 0.5f;
    m_vSyncDetectThreshold = (int)(detectTotalLen * vSyncDetectPercent);
}

bool ATVDemodSink::getBFOLocked()
{
    if ((m_settings.m_atvModulation == ATVDemodSettings::ATV_USB) || (m_settings.m_atvModulation == ATVDemodSettings::ATV_LSB)) {
        return m_bfoPLL.locked();
    } else {
        return false;
    }
}

void ATVDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ATVDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if (channelSampleRate == 0)
    {
        qDebug("ATVDemodSink::applyChannelSettings: aborting");
        return;
    }

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        unsigned int samplesPerLineNom;
        ATVDemodSettings::getBaseValues(channelSampleRate, m_settings.m_nbLines * m_settings.m_fps, samplesPerLineNom);
        m_samplesPerLine = samplesPerLineNom;
		m_samplesPerLineFrac = (float)channelSampleRate / (m_settings.m_nbLines * m_settings.m_fps) - m_samplesPerLine;
        qDebug() << "ATVDemodSink::applyChannelSettings:"
                << " m_channelSampleRate: " << m_channelSampleRate
                << " m_fftBandwidth: " << m_settings.m_fftBandwidth
                << " m_fftOppBandwidth:" << m_settings.m_fftOppBandwidth
                << " m_bfoFrequency: " << m_settings.m_bfoFrequency;

        m_channelSampleRate = channelSampleRate;

        m_DSBFilter->create_asym_filter(
            m_settings.m_fftOppBandwidth / (float) m_channelSampleRate,
            m_settings.m_fftBandwidth / (float) m_channelSampleRate
        );
        std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
        m_DSBFilterBufferIndex = 0;

        m_bfoPLL.configure((float) m_settings.m_bfoFrequency / (float) m_channelSampleRate,
                100.0 / m_channelSampleRate,
                0.01);
        m_bfoFilter.setFrequencies(m_channelSampleRate, m_settings.m_bfoFrequency);
    }

    applyStandard(m_channelSampleRate, m_settings.m_atvStd, ATVDemodSettings::getNominalLineTime(m_settings.m_nbLines, m_settings.m_fps));

    if (m_registeredTVScreen)
    {
        m_registeredTVScreen->resizeTVScreen(
            m_samplesPerLine - m_numberSamplesPerLineSignals,
            m_settings.m_nbLines - m_numberOfBlackLines
        );
		m_tvScreenBuffer = m_registeredTVScreen->getBackBuffer();
    }

    m_fieldIndex = 0;

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void ATVDemodSink::applySettings(const ATVDemodSettings& settings, bool force)
{
    qDebug() << "ATVDemodSink::applySettings:"
            << "m_inputFrequencyOffset:" << settings.m_inputFrequencyOffset
            << "m_bfoFrequency:" << settings.m_bfoFrequency
            << "m_atvModulation:" << settings.m_atvModulation
            << "m_fmDeviation:" << settings.m_fmDeviation
            << "m_fftFiltering:" << settings.m_fftFiltering
            << "m_fftOppBandwidth:" << settings.m_fftOppBandwidth
            << "m_fftBandwidth:" << settings.m_fftBandwidth
            << "m_nbLines:" << settings.m_nbLines
            << "m_fps:" << settings.m_fps
            << "m_atvStd:" << settings.m_atvStd
            << "m_hSync:" << settings.m_hSync
            << "m_vSync:" << settings.m_vSync
            << "m_invertVideo:" << settings.m_invertVideo
            << "m_halfFrames:" << settings.m_halfFrames
            << "m_levelSynchroTop:" << settings.m_levelSynchroTop
            << "m_levelBlack:" << settings.m_levelBlack
            << "m_rgbColor:" << settings.m_rgbColor
            << "m_title:" << settings.m_title
            << "m_udpAddress:" << settings.m_udpAddress
            << "m_udpPort:" << settings.m_udpPort
            << "force:" << force;

    if ((settings.m_fftBandwidth != m_settings.m_fftBandwidth)
     || (settings.m_fftOppBandwidth != m_settings.m_fftOppBandwidth) || force)
    {
        m_DSBFilter->create_asym_filter(
            settings.m_fftOppBandwidth / (float) m_channelSampleRate,
            settings.m_fftBandwidth / (float) m_channelSampleRate
        );
        std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
        m_DSBFilterBufferIndex = 0;
    }

    if ((settings.m_bfoFrequency != m_settings.m_bfoFrequency) || force)
    {
        m_bfoPLL.configure((float) settings.m_bfoFrequency / (float) m_channelSampleRate,
                100.0 / m_channelSampleRate,
                0.01);
        m_bfoFilter.setFrequencies(m_channelSampleRate, settings.m_bfoFrequency);
    }

    if ((settings.m_nbLines != m_settings.m_nbLines)
     || (settings.m_fps != m_settings.m_fps)
     || (settings.m_atvStd != m_settings.m_atvStd) || force)
    {
        unsigned int samplesPerLineNom;
        ATVDemodSettings::getBaseValues(m_channelSampleRate, settings.m_nbLines * settings.m_fps, samplesPerLineNom);
        m_samplesPerLine = samplesPerLineNom;
		m_samplesPerLineFrac = (float)m_channelSampleRate / (settings.m_nbLines * settings.m_fps) - m_samplesPerLine;
		m_ampAverage.resize(m_samplesPerLine * settings.m_nbLines * 2); // AGC average in two full images

        qDebug() << "ATVDemodSink::applySettings:"
                << " m_channelSampleRate: " << m_channelSampleRate
                << " m_samplesPerLine:" << m_samplesPerLine
                << " m_samplesPerLineFrac:" << m_samplesPerLineFrac;

        applyStandard(m_channelSampleRate, settings.m_atvStd,
            ATVDemodSettings::getNominalLineTime(settings.m_nbLines, settings.m_fps));

        if (m_registeredTVScreen)
        {
            m_registeredTVScreen->resizeTVScreen(
                m_samplesPerLine - m_numberSamplesPerLineSignals,
                settings.m_nbLines - m_numberOfBlackLines
            );
			m_tvScreenBuffer = m_registeredTVScreen->getBackBuffer();
        }

        m_fieldIndex = 0;
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        m_objPhaseDiscri.setFMScaling(1.0f / settings.m_fmDeviation);
    }

    if ((settings.m_levelBlack != m_settings.m_levelBlack) || force) {
        m_sampleRangeCorrection = 255.0f / (1.0f - m_settings.m_levelBlack);
    }

    m_settings = settings;
}
