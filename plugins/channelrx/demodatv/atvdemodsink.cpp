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

#include "audio/audiooutput.h"

#include "atvdemodsink.h"

const int ATVDemodSink::m_ssbFftLen = 1024;

ATVDemodSink::ATVDemodSink() :
    m_channelSampleRate(1000000),
    m_channelFrequencyOffset(0),
    m_tvSampleRate(1000000),
    m_samplesPerLine(100),
    m_videoTabIndex(0),
    m_scopeSink(nullptr),
    m_registeredTVScreen(nullptr),
    m_numberSamplesPerHTop(0),
    m_imageIndex(0),
    m_synchroSamples(0),
    m_horizontalSynchroDetected(false),
    m_verticalSynchroDetected(false),
    m_ampLineSum(0.0f),
    m_ampLineAvg(0.0f),
    m_effMin(2000000.0f),
    m_effMax(-2000000.0f),
    m_ampMin(0.0f),
    m_ampMax(0.3f),
    m_ampDelta(0.3f),
    m_ampSample(0.0f),
    m_colIndex(0),
    m_sampleIndex(0),
    m_amSampleIndex(0),
    m_rowIndex(0),
    m_lineIndex(0),
    m_objAvgColIndex(3),
    m_bfoPLL(200/1000000, 100/1000000, 0.01),
    m_bfoFilter(200.0, 1000000.0, 0.9),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_DSBFilter(nullptr),
    m_DSBFilterBuffer(nullptr),
    m_DSBFilterBufferIndex(0)
{
    qDebug("ATVDemodSink::ATVDemodSink");
    //*************** ATV PARAMETERS  ***************
    //m_intNumberSamplePerLine=0;
    m_synchroSamples=0;
    m_interleaved = true;
    m_firstRowIndexEven = 0;
    m_firstRowIndexOdd = 0;

    m_DSBFilter = new fftfilt(m_settings.m_fftBandwidth / (float) m_tvSampleRate, 2*m_ssbFftLen); // arbitrary cutoff
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
    float fltI;
    float fltQ;
    Complex ci;

    //********** Let's rock and roll buddy ! **********

    //********** Accessing ATV Screen context **********

    for (SampleVector::const_iterator it = begin; it != end; ++it /* ++it **/)
    {

        fltI = it->real();
        fltQ = it->imag();
        Complex c(fltI, fltQ);

        if (m_settings.m_inputFrequencyOffset != 0) {
            c *= m_nco.nextIQ();
        }

        if ((m_tvSampleRate == m_channelSampleRate) && (!m_settings.m_forceDecimator)) // no decimation
        {
            demod(c);
        }
        else
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                demod(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }

    if ((m_videoTabIndex == 1) && (m_scopeSink)) // do only if scope tab is selected and scope is available
    {
        m_scopeSink->feed(m_scopeSampleBuffer.begin(), m_scopeSampleBuffer.end(), false); // m_ssb = positive only
        m_scopeSampleBuffer.clear();
    }
}

void ATVDemodSink::demod(Complex& c)
{
    float divSynchroBlack = 1.0f - m_settings.m_levelBlack;
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

    float fftScale = 1.0f;
    const float& fltI = m_settings.m_fftFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex].real() : c.real();
    const float& fltQ = m_settings.m_fftFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex].imag() : c.imag();
    double magSq;

    if ((m_settings.m_atvModulation == ATVDemodSettings::ATV_FM1) || (m_settings.m_atvModulation == ATVDemodSettings::ATV_FM2))
    {
        //Amplitude FM
        magSq = fltI*fltI + fltQ*fltQ;
        m_objMagSqAverage(magSq);
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
        m_objMagSqAverage(magSq);
        sampleNorm = sqrt(magSq);
        sample = sampleNorm / SDR_RX_SCALEF;
    }
    else if ((m_settings.m_atvModulation == ATVDemodSettings::ATV_USB) || (m_settings.m_atvModulation == ATVDemodSettings::ATV_LSB))
    {
        magSq = fltI*fltI + fltQ*fltQ;
        m_objMagSqAverage(magSq);
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
        m_objMagSqAverage(magSq);
        sampleNorm = sqrt(magSq);
    }
    else
    {
        magSq = fltI*fltI + fltQ*fltQ;
        m_objMagSqAverage(magSq);
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

        if (m_amSampleIndex < m_samplesPerLine * m_settings.m_nbLines) // do not extend estimation period past a full image
        {
            m_amSampleIndex++;
        }
        else
        {
            // scale signal based on extrema on the estimation period
            m_ampMin = m_effMin;
            m_ampMax = m_effMax;
            m_ampDelta = (m_ampMax - m_ampMin) * 0.3f;
            m_ampSample = 0.3f; // allow passing to fine scale estimation

            if (m_ampDelta <= 0.0) {
                m_ampDelta = 0.3f;
            }

            qDebug("ATVDemod::demod: m_ampMin: %f m_ampMax: %f m_ampDelta: %f", m_ampMin, m_ampMax, m_ampDelta);

            //Reset extrema
            m_effMin = 2000000.0f;
            m_effMax = -2000000.0;

            m_amSampleIndex = 0;
        }

        //Normalisation of current sample
        sample -= m_ampMin;
        sample /= (m_ampDelta * 3.1f);
    }

    sample = m_settings.m_invertVideo ? 1.0f - sample : sample;
    // 0.0 -> 1.0
    sample = (sample < 0.0f) ? 0.0f : (sample > 1.0f) ? 1.0f : sample;

    if ((m_videoTabIndex == 1) && (m_scopeSink != 0)) { // feed scope buffer only if scope is present and visible
        m_scopeSampleBuffer.push_back(Sample(sample*SDR_RX_SCALEF, 0.0f));
    }

    //********** gray level **********
    // -0.3 -> 0.7 / 0.7
    sampleVideo = (int) (255.0*(sample - m_settings.m_levelBlack) / (1.0f - m_settings.m_levelBlack));

    // 0 -> 255
    sampleVideo = (sampleVideo < 0) ? 0 : (sampleVideo > 255) ? 255 : sampleVideo;

    //********** process video sample **********

    if (m_registeredTVScreen) // can process only if the screen is available (set via the GUI)
    {
        if (m_settings.m_atvStd == ATVDemodSettings::ATVStdHSkip) {
            processHSkip(sample, sampleVideo);
        } else {
            processClassic(sample, sampleVideo);
        }
    }
}

void ATVDemodSink::applyStandard(int sampleRate, const ATVDemodSettings& settings, float lineDuration)
{
    switch(settings.m_atvStd)
    {
    case ATVDemodSettings::ATVStdHSkip:
        // what is left in a line for the image
        m_numberOfSyncLines  = 0;
        m_numberOfBlackLines = 0;
        m_numberOfEqLines    = 0; // not applicable
        m_numberSamplesHSyncCrop = (int) (0.09f * lineDuration * sampleRate); // 9% of full line empirically
        m_interleaved = false; // irrelevant
        m_firstRowIndexEven  = 0; // irrelevant
        m_firstRowIndexOdd   = 0; // irrelevant
        break;
    case ATVDemodSettings::ATVStdShort:
        // what is left in a line for the image
        m_numberOfSyncLines  = 4;
        m_numberOfBlackLines = 5;
        m_numberOfEqLines    = 0;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        m_interleaved = false;
        m_firstRowIndexEven  = 0; // irrelevant
        m_firstRowIndexOdd   = 0; // irrelevant
        break;
    case ATVDemodSettings::ATVStdShortInterleaved:
        // what is left in a line for the image
        m_numberOfSyncLines  = 4;
        m_numberOfBlackLines = 7;
        m_numberOfEqLines    = 0;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        m_interleaved = true;
        m_firstRowIndexEven  = 0;
        m_firstRowIndexOdd   = 1;
        break;
    case ATVDemodSettings::ATVStd405: // Follows loosely the 405 lines standard
        // what is left in a ine for the image
        m_numberOfSyncLines  = 24; // (15+7)*2 - 20
        m_numberOfBlackLines = 30; // above + 6
        m_numberOfEqLines    = 3;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        m_interleaved = true;
        m_firstRowIndexEven  = 0;
        m_firstRowIndexOdd   = 3;
        break;
    case ATVDemodSettings::ATVStdPAL525: // Follows PAL-M standard
        // what is left in a 64/1.008 us line for the image
        m_numberOfSyncLines  = 40; // (15+15)*2 - 20
        m_numberOfBlackLines = 46; // above + 6
        m_numberOfEqLines    = 3;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        m_interleaved = true;
        m_firstRowIndexEven  = 0;
        m_firstRowIndexOdd   = 3;
        break;
    case ATVDemodSettings::ATVStdPAL625: // Follows PAL-B/G/H standard
    default:
        // what is left in a 64 us line for the image
        m_numberOfSyncLines  = 44; // (15+17)*2 - 20
        m_numberOfBlackLines = 50; // above + 6
        m_numberOfEqLines    = 3;
        m_numberSamplesHSyncCrop = (int) (0.085f * lineDuration * sampleRate); // 8.5% of full line empirically
        m_interleaved = true;
        m_firstRowIndexEven  = 0;
        m_firstRowIndexOdd   = 3;
    }

    // for now all standards apply this
    m_numberSamplesPerLineSignals = (int) ((12.0f/64.0f) * lineDuration * sampleRate);  // 12.0 = 2.6 + 4.7 + 4.7 : front porch + horizontal sync pulse + back porch
    m_numberSamplesPerHSync = (int) ((9.6f/64.0f) * lineDuration * sampleRate);         //  9.4 = 4.7 + 4.7       : horizontal sync pulse + back porch
    m_numberSamplesPerHTopNom = (int) ((4.7f/64.0f) * lineDuration * sampleRate);       //  4.7                   : horizontal sync pulse (ultra black) nominal value
    m_numberSamplesPerHTop = m_numberSamplesPerHTopNom + settings.m_topTimeFactor;      // adjust the value used in the system
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
        ATVDemodSettings::getBaseValues(channelSampleRate, m_settings.m_nbLines * m_settings.m_fps, m_tvSampleRate, m_samplesPerLineNom);
        m_samplesPerLine = m_samplesPerLineNom + m_settings.m_lineTimeFactor;
        qDebug() << "ATVDemodSink::applyChannelSettings:"
                << " m_tvSampleRate: " << m_tvSampleRate
                << " m_fftBandwidth: " << m_settings.m_fftBandwidth
                << " m_fftOppBandwidth:" << m_settings.m_fftOppBandwidth
                << " m_bfoFrequency: " << m_settings.m_bfoFrequency;

        if (m_tvSampleRate > 0)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorDistance = (Real) m_tvSampleRate / (Real) channelSampleRate;
            m_interpolator.create(24,
                m_tvSampleRate,
                m_settings.m_fftBandwidth / ATVDemodSettings::getRFBandwidthDivisor(m_settings.m_atvModulation),
                3.0
            );
        }
        else
        {
            m_tvSampleRate = channelSampleRate;
        }

        m_DSBFilter->create_asym_filter(
            m_settings.m_fftOppBandwidth / (float) m_tvSampleRate,
            m_settings.m_fftBandwidth / (float) m_tvSampleRate
        );
        std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
        m_DSBFilterBufferIndex = 0;

        m_bfoPLL.configure((float) m_settings.m_bfoFrequency / (float) m_tvSampleRate,
                100.0 / m_tvSampleRate,
                0.01);
        m_bfoFilter.setFrequencies(m_tvSampleRate, m_settings.m_bfoFrequency);
    }

    applyStandard(m_tvSampleRate, m_settings, ATVDemodSettings::getNominalLineTime(m_settings.m_nbLines, m_settings.m_fps));

    if (m_registeredTVScreen)
    {
        m_registeredTVScreen->resizeTVScreen(
            m_samplesPerLine - m_numberSamplesPerLineSignals,
            m_settings.m_nbLines - m_numberOfBlackLines
        );
    }

    m_imageIndex = 0;
    m_colIndex = 0;
    m_rowIndex = 0;

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void ATVDemodSink::applySettings(const ATVDemodSettings& settings, bool force)
{
    qDebug() << "ATVDemodSink::applySettings:"
            << "m_inputFrequencyOffset:" << settings.m_inputFrequencyOffset
            << "m_forceDecimator:" << settings.m_forceDecimator
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
            << "m_lineTimeFactor:" << settings.m_lineTimeFactor
            << "m_topTimeFactor:" << settings.m_topTimeFactor
            << "m_rgbColor:" << settings.m_rgbColor
            << "m_title:" << settings.m_title
            << "m_udpAddress:" << settings.m_udpAddress
            << "m_udpPort:" << settings.m_udpPort
            << "force:" << force;

    if ((settings.m_nbLines != m_settings.m_nbLines)
     || (settings.m_fps != m_settings.m_fps)
     || (settings.m_atvStd != m_settings.m_atvStd)
     || (settings.m_atvModulation != m_settings.m_atvModulation)
     || (settings.m_fftBandwidth != m_settings.m_fftBandwidth)
     || (settings.m_fftOppBandwidth != m_settings.m_fftOppBandwidth)
     || (settings.m_atvStd != m_settings.m_atvStd)
     || (settings.m_lineTimeFactor != m_settings.m_lineTimeFactor) || force)
    {
        ATVDemodSettings::getBaseValues(m_channelSampleRate, settings.m_nbLines * settings.m_fps, m_tvSampleRate, m_samplesPerLineNom);
        m_samplesPerLine = m_samplesPerLineNom + settings.m_lineTimeFactor;

        qDebug() << "ATVDemodSink::applySettings:"
                << " m_tvSampleRate: " << m_tvSampleRate
                << " m_fftBandwidth: " << settings.m_fftBandwidth
                << " m_fftOppBandwidth:" << settings.m_fftOppBandwidth
                << " m_bfoFrequency: " << settings.m_bfoFrequency;

        if (m_tvSampleRate > 0)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorDistance = (Real) m_tvSampleRate / (Real) m_channelSampleRate;
            m_interpolator.create(24,
                m_tvSampleRate,
                settings.m_fftBandwidth / ATVDemodSettings::getRFBandwidthDivisor(settings.m_atvModulation),
                3.0
            );
        }
        else
        {
            m_tvSampleRate = m_channelSampleRate;
        }

        m_DSBFilter->create_asym_filter(
            settings.m_fftOppBandwidth / (float) m_tvSampleRate,
            settings.m_fftBandwidth / (float) m_tvSampleRate
        );
        std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
        m_DSBFilterBufferIndex = 0;

        m_bfoPLL.configure((float) settings.m_bfoFrequency / (float) m_tvSampleRate,
                100.0 / m_tvSampleRate,
                0.01);
        m_bfoFilter.setFrequencies(m_tvSampleRate, settings.m_bfoFrequency);

        applyStandard(m_tvSampleRate, settings, ATVDemodSettings::getNominalLineTime(settings.m_nbLines, settings.m_fps));

        if (m_registeredTVScreen)
        {
            m_registeredTVScreen->resizeTVScreen(
                m_samplesPerLine - m_numberSamplesPerLineSignals,
                m_settings.m_nbLines - m_numberOfBlackLines
            );
        }

        m_imageIndex = 0;
        m_colIndex = 0;
        m_rowIndex = 0;
    }

    if ((settings.m_topTimeFactor != m_settings.m_topTimeFactor) || force) {
        m_numberSamplesPerHTop = m_numberSamplesPerHTopNom + settings.m_topTimeFactor;
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        m_objPhaseDiscri.setFMScaling(1.0f / settings.m_fmDeviation);
    }

    m_settings = settings;
}
