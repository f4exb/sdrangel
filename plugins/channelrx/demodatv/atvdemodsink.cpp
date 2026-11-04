///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Vort <vvort@yandex.ru>                                     //
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

#include <algorithm>
#include <cmath>

#include <stdio.h>
#include <complex.h>

#include "dsp/scopevis.h"
#include "atvdemodsink.h"

const int ATVDemodSink::m_ssbFftLen = 1024;

ATVDemodSink::ATVDemodSink() :
    m_channelSampleRate(1000000),
    m_channelFrequencyOffset(0),
    m_videoSampleRate(1000000),
    m_samplesPerLine(100),
	m_samplesPerLineFrac(0.0f),
    m_videoTabIndex(0),
    m_scopeSink(nullptr),
    m_registeredTVScreen(nullptr),
    m_numberSamplesPerHTop(0),
    m_numberSamplesPerHSyncMin(1),
    m_fieldIndex(0),
    m_synchroSamples(0),
    m_fieldDetectSampleCount(0),
    m_vSyncDetectSampleCount(0),
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
    m_hSyncCandidate(false),
    m_hSyncCandidateShift(0.0f),
    m_hSyncCandidateAge(0),
    m_hSyncCandidateBelow(0),
    m_hSyncCandidateAbove(0),
    prevSample(0.0f),
    m_magSqCount(0),
    m_ampAverage(4800),
    m_bfoPLL(200/1000000, 100/1000000, 0.01),
    m_bfoFilter(200.0, 1000000.0, 0.9),
    m_DSBFilter(nullptr),
    m_DSBFilterBuffer(nullptr),
    m_DSBFilterBufferIndex(0),
    m_useNCO(false),
    m_vsbFrontEnd(false),
    m_carrierReferenced(false),
    m_carrierVideoRange(1.0f),
    m_envelopeLPAlpha(1.0f),
    m_envelopeLP(0.0f),
    m_envelopeLineMax(0.0f),
    m_videoScale(0.0f),
    m_syncTipEnvelope(0.0f),
    m_colourLineFirst(0),
    m_colourLineStart(0),
    m_videoSampleCount(0),
    m_pixelStep(1),
    m_currentRow(-1),
    m_syncFilterIndex(0),
    m_syncFilterSum(0.0f),
    m_syncFilterScale(1.0f)
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

	applySettings(QStringList(), m_settings, true);
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

        if (m_useNCO) {
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

    //********** AM / VSB **********

    if (m_vsbFrontEnd)
    {
        if (++m_magSqCount >= m_magSqInterval)
        {
            m_magSqCount = 0;
            m_magSqAverage(c.real()*c.real() + c.imag()*c.imag());
        }

        Complex *filtered;
        int n_out = m_vsbFilter.process(c, &filtered);

        for (int i = 0; i < n_out; i++)
        {
            if (m_carrierReferenced)
            {
                // Scale from the carrier as a TV does: sync tip 0, peak white 1. The sync tip
                // is the peak of the smoothed envelope, tracked line by line
                float envelope = m_synchronousDetector.process(filtered[i]);

                if (m_settings.m_ghostCancellation && m_ghostEstimator.feed(m_sampleOffset, m_synchronousDetector.getGhostInput())) {
                    m_synchronousDetector.setGhost(m_ghostEstimator.getDelay(), m_ghostEstimator.getGain());
                }

                m_envelopeLP += m_envelopeLPAlpha * (envelope - m_envelopeLP);
                m_envelopeLineMax = std::max(m_envelopeLineMax, m_envelopeLP);
                float video = (m_syncTipEnvelope - envelope) * m_videoScale;
                processVideo(video, envelope);
            }
            else
            {
                // Same AGC as AM
                float envelope = std::abs(filtered[i]);
                float sampleRaw = envelope / SDR_RX_SCALEF;
                m_ampAverage(sampleRaw);
                processVideo(sampleRaw / (2.0f * (float) m_ampAverage.instantAverage()), envelope);
            }
        }

        return;
    }

    //********** FFT filtering **********

    if (m_settings.m_fftFiltering)
    {
        int n_out;
        Complex *filtered;

        n_out = m_DSBFilter->runAsym(c, &filtered, m_settings.m_atvModulation != ATVDemodSettings::ATV_LSB); // all usb except explicitly lsb

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
        sample = sampleRaw / (2.0f * (float) m_ampAverage.instantAverage()); // AGC (average of what has arrived, while the window fills)
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

    processVideo(sample, 0.0f);
}

void ATVDemodSink::processVideo(float sample, float envelope)
{
    int sampleVideo = 0;

    //********** AM sample normalization and coarse scale estimation **********

    if (!m_carrierReferenced &&
        ((m_settings.m_atvModulation == ATVDemodSettings::ATV_AM)
        || (m_settings.m_atvModulation == ATVDemodSettings::ATV_USB)
        || (m_settings.m_atvModulation == ATVDemodSettings::ATV_LSB)))
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

    if (!m_carrierReferenced) { // already scaled with sync tip low
        sample = m_settings.m_invertVideo ? 1.0f - sample : sample;
    }

    float composite = sample; // colour needs the chroma excursions beyond black and white
    // 0.0 -> 1.0
    sample = (sample < 0.0f) ? 0.0f : (sample > 1.0f) ? 1.0f : sample;

    if ((m_videoTabIndex == 1) && (m_scopeSink != 0)) { // feed scope buffer only if scope is present and visible
        m_scopeSampleBuffer.push_back(Sample(sample * (SDR_RX_SCALEF - 1.0f), 0.0f));
    }

    //********** gray level **********
    if (!m_settings.isColour()) // colour is decoded, and its levels taken, a whole line at a time
    {
        // -0.3 -> 0.7 / 0.7
        sampleVideo = (int) ((sample - m_settings.m_levelBlack) * m_sampleRangeCorrection);

        // 0 -> 255
        sampleVideo = (sampleVideo < 0) ? 0 : (sampleVideo > 255) ? 255 : sampleVideo;
    }

    //********** process video sample **********

    if (m_registeredTVScreen) // can process only if the screen is available (set via the GUI)
    {
        if (m_settings.isColour() && (m_sampleOffset >= 0) && (m_sampleOffset < m_samplesPerLine))
        {
            m_colourLine[m_sampleOffset] = m_carrierReferenced ? envelope : composite;
            m_colourLineFirst = std::min(m_colourLineFirst, m_sampleOffset);
            m_colourLineStart = m_videoSampleCount - m_sampleOffset;
        }

        m_videoSampleCount++;

        if (m_syncFilter.size() > 1)
        {
            // Sync is sliced with the colour subcarrier removed, as a TV's sync separator does
            m_syncFilterSum += composite - m_syncFilter[m_syncFilterIndex];
            m_syncFilter[m_syncFilterIndex] = composite;
            m_syncFilterIndex = m_syncFilterIndex + 1 == (int) m_syncFilter.size() ? 0 : m_syncFilterIndex + 1;
            float syncSample = m_syncFilterSum * m_syncFilterScale;
            processSample(syncSample, sampleVideo);
        }
        else
        {
            processSample(sample, sampleVideo);
        }
    }
}

// Writes a line held in m_lineRGB to the screen: decoding colour first, and reducing noise
void ATVDemodSink::writeLine()
{
    if (m_settings.isColour())
    {
        m_colourDecoder.decodeLine(m_colourLine.data(), m_samplesPerLine, m_colourLineFirst, m_colourLineStart, m_lineRGB.data());
        m_colourLineFirst = m_samplesPerLine;
    }

    const int firstPixel = m_numberSamplesPerHSync / m_pixelStep;
    const int pixels = m_samplesPerLine / m_pixelStep;

    if (m_noiseReduction.isEnabled()) {
        m_noiseReduction.processRow(m_currentRow, &m_lineRGB[firstPixel]);
    }

    for (int i = std::max(firstPixel - 2, 0); i < pixels; i++) {
        m_tvScreenBuffer->setSampleValue(i - firstPixel, (int) m_lineRGB[i]);
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
    m_numberSamplesPerHSyncMin    = std::max(1, (int) std::round(lineDuration * sampleRate * 1.0 / 64.0)); // shorter than an equalizing pulse, longer than ringing

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

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        qDebug() << "ATVDemodSink::applyChannelSettings:"
                << " m_channelSampleRate: " << channelSampleRate
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

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

    applyVideoSettings();
}

void ATVDemodSink::applyVideoSettings()
{
    m_vsbFrontEnd = m_settings.usesVSBFrontEnd();
    m_videoSampleRate = m_settings.getVideoSampleRate(m_channelSampleRate);

    float lineFrequency = m_settings.getLineFrequency();
    m_samplesPerLine = (int) (m_videoSampleRate / lineFrequency);
    m_samplesPerLine = m_samplesPerLine < 1 ? 1 : m_samplesPerLine;
    m_samplesPerLineFrac = m_videoSampleRate / lineFrequency - m_samplesPerLine;
    m_ampAverage.resize(m_samplesPerLine * m_settings.m_nbLines * 2); // AGC average in two full images

    qDebug() << "ATVDemodSink::applyVideoSettings:"
            << " m_channelSampleRate: " << m_channelSampleRate
            << " m_videoSampleRate: " << m_videoSampleRate
            << " m_samplesPerLine:" << m_samplesPerLine
            << " m_samplesPerLineFrac:" << m_samplesPerLineFrac;

    applyStandard(m_videoSampleRate, m_settings.m_atvStd, 1.0f / lineFrequency);

    // For colour, the VSB front end needs the centre of the video band, not the vision carrier,
    // at DC, or the top of the chroma wraps round when the sample rate is only just wide enough.
    // Mono is not upsampled, so keeps the carrier at DC and the video is limited to half the rate
    float shift = 0.0f;

    if (m_vsbFrontEnd)
    {
        bool usb = m_settings.m_atvModulation != ATVDemodSettings::ATV_LSB;

        if (m_settings.isColour())
        {
            float centreOffset = ATVDemodSettings::getColourBandCentreOffset(m_settings.m_colourSystem);
            shift = usb ? centreOffset : -centreOffset;
        }

        m_vsbFilter.configure(
            m_channelSampleRate,
            -shift,
            m_settings.getVideoBandwidth(),
            usb,
            m_videoSampleRate / m_channelSampleRate
        );
        m_synchronousDetector.configure(m_videoSampleRate, -shift, lineFrequency);
    }

    m_ghostEstimator.configure(m_videoSampleRate, m_samplesPerLine, std::min(m_settings.getVideoBandwidth(), m_videoSampleRate / 2.0f));

    m_nco.setFreq(-(m_channelFrequencyOffset + shift), m_channelSampleRate);
    m_useNCO = (m_channelFrequencyOffset != 0) || (shift != 0.0f);

    // For negatively modulated AM, levels are taken from the carrier
    m_carrierReferenced = m_vsbFrontEnd && m_settings.m_invertVideo;
    m_carrierVideoRange = 1.0f - m_settings.getWhiteCarrier();
    m_envelopeLPAlpha = 1.0f / (0.5e-6f * m_videoSampleRate); // ~0.5 us, so noise does not set the peak
    m_envelopeLP = 0.0f;
    m_envelopeLineMax = 0.0f;
    m_syncTipEnvelope = 0.0f;
    m_videoScale = 0.0f;

    if (m_settings.isColour())
    {
        m_colourDecoder.configure(m_videoSampleRate, m_settings.m_colourSystem);
        m_colourDecoder.setCarrierReferenced(m_carrierReferenced);
        m_colourLine.assign(m_samplesPerLine, 0.0f);
        m_colourLineFirst = m_samplesPerLine;
    }

    // The subcarrier is in the video when decoding colour, and in mono when the rate is high enough
    if (m_settings.isColour() || m_vsbFrontEnd)
    {
        float subcarrier = m_settings.isColour() ? ATVDemodSettings::getSubcarrierFrequency(m_settings.m_colourSystem)
            : ATVDemodSettings::getSubcarrierFrequency(m_settings.m_nbLines == 525 ? ATVDemodSettings::ColourNTSC : ATVDemodSettings::ColourPAL);
        m_syncFilter.assign(std::max(1, (int) std::round(m_videoSampleRate / subcarrier)), 0.0f);
    }
    else
    {
        m_syncFilter.assign(1, 0.0f);
    }

    m_syncFilterIndex = 0;
    m_syncFilterSum = 0.0f;
    m_syncFilterScale = 1.0f / m_syncFilter.size();
    m_lineRGB.assign(m_samplesPerLine, 0);
    m_pixelStep = m_settings.isColour() ? m_colourDecoder.getPixelStep() : 1; // A wider picture would show nothing more
    int screenWidth = (m_samplesPerLine - m_numberSamplesPerLineSignals) / m_pixelStep;
    m_noiseReduction.configure(screenWidth, m_settings.m_nbLines - m_numberOfBlackLines);

    if (m_registeredTVScreen)
    {
        m_registeredTVScreen->resizeTVScreen(
            screenWidth,
            m_settings.m_nbLines - m_numberOfBlackLines
        );
        m_tvScreenBuffer = m_registeredTVScreen->getBackBuffer();
    }

    m_fieldIndex = 0;
    m_amSampleIndex = 0;
}

void ATVDemodSink::applySettings(const QStringList& settingsKeys, const ATVDemodSettings& settings, bool force)
{
    qDebug() << "ATVDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("fftBandwidth") || settingsKeys.contains("fftOppBandwidth") || force)
    {
        m_DSBFilter->create_asym_filter(
            settings.m_fftOppBandwidth / (float) m_channelSampleRate,
            settings.m_fftBandwidth / (float) m_channelSampleRate
        );
        std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
        m_DSBFilterBufferIndex = 0;
    }

    if (settingsKeys.contains("bfoFrequency") || force)
    {
        m_bfoPLL.configure((float) settings.m_bfoFrequency / (float) m_channelSampleRate,
                100.0 / m_channelSampleRate,
                0.01);
        m_bfoFilter.setFrequencies(m_channelSampleRate, settings.m_bfoFrequency);
    }

    if (settingsKeys.contains("fmDeviation") || force) {
        m_objPhaseDiscri.setFMScaling(1.0f / settings.m_fmDeviation);
    }

    if (settingsKeys.contains("levelBlack") || force) {
        m_sampleRangeCorrection = 255.0f / (1.0f - settings.m_levelBlack);
    }

    bool videoChanged = settingsKeys.contains("nbLines")
        || settingsKeys.contains("fps")
        || settingsKeys.contains("atvStd")
        || settingsKeys.contains("atvModulation")
        || settingsKeys.contains("colourSystem")
        || settingsKeys.contains("invertVideo")
        || settingsKeys.contains("fftFiltering")
        || force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (settingsKeys.contains("colourSaturation") || force) {
        m_colourDecoder.setSaturation(m_settings.m_colourSaturation);
    }

    if (settingsKeys.contains("noiseReduction") || force) {
        m_noiseReduction.setStrength(m_settings.m_noiseReduction);
    }

    if (settingsKeys.contains("colourHue") || force) {
        m_colourDecoder.setHue(m_settings.m_colourHue);
    }

    if (settingsKeys.contains("palDelayLine") || force) {
        m_colourDecoder.setDelayLine(m_settings.m_palDelayLine);
    }

    if ((settingsKeys.contains("ghostCancellation") || force) && !m_settings.m_ghostCancellation)
    {
        m_synchronousDetector.setGhost(1.0f, 0.0f);
        m_ghostEstimator.reset();
    }

    if (videoChanged) {
        applyVideoSettings();
    }
}
