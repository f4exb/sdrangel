///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <time.h>

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "opencv2/imgproc/imgproc.hpp"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "util/messagequeue.h"

#include "atvmodreport.h"
#include "atvmodsource.h"

const float ATVModSource::m_blackLevel = 0.3f;
const float ATVModSource::m_spanLevel = 0.7f;
const int ATVModSource::m_levelNbSamples = 10000; // every 10ms
const int ATVModSource::m_nbBars = 6;
const int ATVModSource::m_cameraFPSTestNbFrames = 100;
const int ATVModSource::m_ssbFftLen = 1024;

const ATVModSource::LineType ATVModSource::StdPAL625_F1Start[] = {
    LineBroadPulses,       // 1 (index 0)
    LineBroadPulses,       // 2
    LineBroadShortPulses,  // 3
    LineShortPulses,       // 4
    LineShortPulses        // 5
};

const ATVModSource::LineType ATVModSource::StdPAL625_F2Start[] = {
    LineBroadPulses,       // 314 (index 313)
    LineBroadPulses,       // 315
    LineShortPulses,       // 316
    LineShortPulses,       // 317
    LineShortBlackPulses   // 318
};

const ATVModSource::LineType ATVModSource::StdPAL525_F1Start[] = {
    LineShortPulses, // 1 (index 0)
    LineShortPulses,
    LineShortPulses,
    LineBroadPulses, // 4
    LineBroadPulses,
    LineBroadPulses,
    LineShortPulses, // 7
    LineShortPulses,
    LineShortPulses, // 9
};

const ATVModSource::LineType ATVModSource::StdPAL525_F2Start[] = {
    LineShortPulses,      // 264 (index 263)
    LineShortPulses,      // 265
    LineShortBroadPulses, // 266
    LineBroadPulses,      // 267
    LineBroadPulses,      // 268
    LineBroadShortPulses, // 269
    LineShortPulses,      // 270
    LineShortPulses,      // 271
    LineShortBlackPulses  // 272
};

const ATVModSource::LineType ATVModSource::Std819_F1Start[] = {
    LineBroadPulses,      // 1 (index 0)
    LineBroadPulses,      // 2
    LineBroadPulses,      // 3
    LineBroadShortPulses, // 4
    LineShortPulses,      // 5
    LineShortPulses,      // 6
    LineShortPulses,      // 7
};

const ATVModSource::LineType ATVModSource::Std819_F2Start[] = {
    LineBroadPulses, // 410 (index 409)
    LineBroadPulses, // 411
    LineBroadPulses, // 412
    LineShortPulses, // 413
    LineShortPulses, // 414
    LineShortPulses, // 415
};

const ATVModSource::LineType ATVModSource::StdShort_F1Start[] = {
    LineBroadPulses,
    LineShortPulses,
};

const ATVModSource::LineType ATVModSource::StdShort_F2Start[] = {
    LineBroadPulses,
    LineShortPulses,
};

ATVModSource::ATVModSource() :
    m_channelSampleRate(1000000),
    m_channelFrequencyOffset(0),
	m_modPhasor(0.0f),
    m_tvSampleRate(1000000),
    m_horizontalCount(0),
    m_lineCount(0),
    m_imageLine(0),
	m_imageOK(false),
	m_videoFPSq(1.0f),
    m_videoFPSCount(0.0f),
	m_videoPrevFPSCount(0),
	m_videoEOF(false),
	m_videoOK(false),
	m_cameraIndex(-1),
	//m_showOverlayText(false),
    m_SSBFilter(nullptr),
    m_SSBFilterBuffer(nullptr),
    m_SSBFilterBufferIndex(0),
    m_DSBFilter(nullptr),
    m_DSBFilterBuffer(nullptr),
    m_DSBFilterBufferIndex(0),
    m_messageQueueToGUI(nullptr)
{
    scanCameras();

    m_SSBFilter = new fftfilt(0, m_settings.m_rfBandwidth / (float) m_channelSampleRate, m_ssbFftLen); // arbitrary cutoff
    m_SSBFilterBuffer = new Complex[m_ssbFftLen/2]; // filter returns data exactly half of its size
    std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer + (m_ssbFftLen>>1), Complex{0, 0});

    m_DSBFilter = new fftfilt((2.0f * m_settings.m_rfBandwidth) / (float) m_channelSampleRate, 2*m_ssbFftLen); // arbitrary cutoff
    m_DSBFilterBuffer = new Complex[m_ssbFftLen];
    std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0, 0});

    m_interpolatorDistanceRemain = 0.0f;
    m_interpolatorDistance = 1.0f;

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    applySettings(m_settings, true); // does applyStandard() too;

    m_lineType = getLineType(m_settings.m_atvStd, m_lineCount);
}

ATVModSource::~ATVModSource()
{
    if (m_video.isOpened()) {
	    m_video.release();
	}

    releaseCameras();
    delete m_SSBFilter;
    delete m_DSBFilter;
    delete[] m_SSBFilterBuffer;
    delete[] m_DSBFilterBuffer;
}

void ATVModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void ATVModSource::prefetch(unsigned int nbSamples)
{
    (void) nbSamples;
}

void ATVModSource::pullOne(Sample& sample)
{
	if (m_settings.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
		return;
	}

    Complex ci;

    if ((m_tvSampleRate == m_channelSampleRate) && (!m_settings.m_forceDecimator)) // no interpolation nor decimation
    {
        modulateSample();
        pullFinalize(m_modSample, sample);
    }
    else
    {
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
        pullFinalize(ci, sample);
    }
}

void ATVModSource::pullFinalize(Complex& ci, Sample& sample)
{
    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
    m_movingAverage(magsq);

    sample.m_real = (FixReal) ci.real();
    sample.m_imag = (FixReal) ci.imag();
}

void ATVModSource::modulateSample()
{
    Real t;

    pullVideo(t);
    calculateLevel(t);

    t = m_settings.m_invertedVideo ? 1.0f - t : t;

    switch (m_settings.m_atvModulation)
    {
    case ATVModSettings::ATVModulationFM: // FM half bandwidth deviation
    	m_modPhasor += (t - 0.5f) * m_settings.m_fmExcursion * M_PI;
    	if (m_modPhasor > 2.0f * M_PI)  m_modPhasor -= 2.0f * M_PI;  // limit growth
    	if (m_modPhasor < 0) m_modPhasor += 2.0f * M_PI;             // limit growth
    	m_modSample.real(cos(m_modPhasor) * m_settings.m_rfScalingFactor); // -1 dB
    	m_modSample.imag(sin(m_modPhasor) * m_settings.m_rfScalingFactor);
    	break;
    case ATVModSettings::ATVModulationLSB:
    case ATVModSettings::ATVModulationUSB:
        m_modSample = modulateSSB(t);
        m_modSample *= m_settings.m_rfScalingFactor;
        break;
    case ATVModSettings::ATVModulationVestigialLSB:
    case ATVModSettings::ATVModulationVestigialUSB:
        m_modSample = modulateVestigialSSB(t);
        m_modSample *= m_settings.m_rfScalingFactor;
        break;
    case ATVModSettings::ATVModulationAM: // AM 90%
    default:
        m_modSample.real((t*1.8f + 0.1f) * (m_settings.m_rfScalingFactor/2.0f)); // modulate and scale zero frequency carrier
        m_modSample.imag(0.0f);
    }
}

Complex& ATVModSource::modulateSSB(Real& sample)
{
    int n_out;
    Complex ci(sample, 0.0f);
    fftfilt::cmplx *filtered;

    n_out = m_SSBFilter->runSSB(ci, &filtered, m_settings.m_atvModulation == ATVModSettings::ATVModulationUSB);

    if (n_out > 0)
    {
        std::copy(filtered, filtered + n_out, m_SSBFilterBuffer);
        m_SSBFilterBufferIndex = 0;
    }

    m_SSBFilterBufferIndex++;

    return m_SSBFilterBuffer[m_SSBFilterBufferIndex-1];
}

Complex& ATVModSource::modulateVestigialSSB(Real& sample)
{
    int n_out;
    Complex ci(sample, 0.0f);
    fftfilt::cmplx *filtered;

    n_out = m_DSBFilter->runAsym(ci, &filtered, m_settings.m_atvModulation == ATVModSettings::ATVModulationVestigialUSB);

    if (n_out > 0)
    {
        std::copy(filtered, filtered + n_out, m_DSBFilterBuffer);
        m_DSBFilterBufferIndex = 0;
    }

    m_DSBFilterBufferIndex++;

    return m_DSBFilterBuffer[m_DSBFilterBufferIndex-1];
}

void ATVModSource::pullVideo(Real& sample)
{
    if (m_settings.m_atvStd == ATVModSettings::ATVStdHSkip)
    {
        pullImageSample(sample, m_lineCount == m_nbLines - 1); // pull image line without sync at end of image
    }
    else
    {
        switch(m_lineType)
        {
            case LineImage:
                pullImageSample(sample);
                break;
            case LineImageHalf1Short:
                pullImageFirstHalfShortSample(sample);
                break;
            case LineImageHalf1Broad:
                pullImageFirstHalfBroadSample(sample);
                break;
            case LineImageHalf2:
                pullImageLastHalfSample(sample);
                break;
            case LineShortPulses:
                pullVSyncLineEquPulsesSample(sample);
                break;
            case LineBroadPulses:
                pullVSyncLineLongPulsesSample(sample);
                break;
            case LineShortBroadPulses:
                pullVSyncLineEquLongPulsesSample(sample);
                break;
            case LineBroadShortPulses:
                pullVSyncLineLongEquPulsesSample(sample);
                break;
            case LineShortBlackPulses:
                pullVSyncLineEquBlackPulsesSample(sample);
                break;
            case LineBlack:
            default:
                pullBlackLineSample(sample);
        }
    }

    if (m_horizontalCount < m_nbHorizPoints - 1)
    {
        m_horizontalCount++;
    }
    else
    {
        if (m_lineCount < m_nbLines - 1)
        {
            m_lineCount++;

            if ((m_lineType == LineImage)
             || (m_lineType == LineImageHalf1Short)
             || (m_lineType == LineImageHalf1Broad)
             || (m_lineType == LineImageHalf2)) {
                m_imageLine += m_interlaced ? 2 : 1;
            }

            if (m_lineCount == m_nbLinesField1) { // field 1 -> field 2 or first field2
                m_imageLine = m_imageLineStart2; // field2 image line start index
            }

            m_lineType = getLineType(m_settings.m_atvStd, m_lineCount);
        }
        else // new image
        {
            m_lineCount = 0;
            m_imageLine = m_imageLineStart1; // field1 image line start index
            m_lineType = getLineType(m_settings.m_atvStd, m_lineCount);

            if ((m_settings.m_atvModInput == ATVModSettings::ATVModInputVideo) && m_videoOK && (m_settings.m_videoPlay) && !m_videoEOF)
            {
            	int grabOK = 0;
            	int fpsIncrement = (int) m_videoFPSCount - m_videoPrevFPSCount;

            	// move a number of frames according to increment
            	// use grab to test for EOF then retrieve to preserve last valid frame as the current original frame
            	// TODO: handle pause (no move)
            	for (int i = 0; i < fpsIncrement; i++)
            	{
            		grabOK = m_video.grab();
            		if (!grabOK) break;
            	}

            	if (grabOK)
            	{
            		cv::Mat colorFrame;
            		m_video.retrieve(colorFrame);

            		if (!colorFrame.empty()) // some frames may not come out properly
            		{
            		    if (m_settings.m_showOverlayText) {
            		        mixImageAndText(colorFrame);
            		    }

            		    cv::cvtColor(colorFrame, m_videoframeOriginal, cv::COLOR_RGB2GRAY);
            		    resizeVideo();
            		}
            	}
            	else
            	{
            	    if (m_settings.m_videoPlayLoop) { // play loop
            	        seekVideoFileStream(0);
            	    } else { // stops
            	        m_videoEOF = true;
            	    }
            	}

            	if (m_videoFPSCount < m_videoFPS)
            	{
            		m_videoPrevFPSCount = (int) m_videoFPSCount;
                	m_videoFPSCount += m_videoFPSq;
            	}
            	else
            	{
            		m_videoPrevFPSCount = 0;
            		m_videoFPSCount = m_videoFPSq;
            	}
            }
            else if ((m_settings.m_atvModInput == ATVModSettings::ATVModInputCamera) && (m_settings.m_cameraPlay))
            {
                ATVCamera& camera = m_cameras[m_cameraIndex];

                if (camera.m_videoFPS < 0.0f) // default frame rate when it could not be obtained via get
                {
                    time_t start, end;
                    cv::Mat frame;

                    if (getMessageQueueToGUI())
                    {
                        ATVModReport::MsgReportCameraData *report;
                        report = ATVModReport::MsgReportCameraData::create(
                                camera.m_cameraNumber,
                                0.0f,
                                camera.m_videoFPSManual,
                                camera.m_videoFPSManualEnable,
                                camera.m_videoWidth,
                                camera.m_videoHeight,
                                1); // open splash screen on GUI side
                        getMessageQueueToGUI()->push(report);
                    }

                    int nbFrames = 0;

                    time(&start);

                    for (int i = 0; i < m_cameraFPSTestNbFrames; i++)
                    {
                        camera.m_camera >> frame;
                        if (!frame.empty()) nbFrames++;
                    }

                    time(&end);

                    double seconds = difftime (end, start);
                    // take a 10% guard and divide bandwidth between all cameras as a hideous hack
                    camera.m_videoFPS = ((nbFrames / seconds) * 0.9) / m_cameras.size();
                    camera.m_videoFPSq = camera.m_videoFPS / m_fps;
                    camera.m_videoFPSCount = camera.m_videoFPSq;
                    camera.m_videoPrevFPSCount = 0;

                    if (getMessageQueueToGUI())
                    {
                        ATVModReport::MsgReportCameraData *report;
                        report = ATVModReport::MsgReportCameraData::create(
                                camera.m_cameraNumber,
                                camera.m_videoFPS,
                                camera.m_videoFPSManual,
                                camera.m_videoFPSManualEnable,
                                camera.m_videoWidth,
                                camera.m_videoHeight,
                                2); // close splash screen on GUI side
                        getMessageQueueToGUI()->push(report);
                    }
                }
                else if (camera.m_videoFPS == 0.0f) // Hideous hack for windows
                {
                    camera.m_videoFPS = 5.0f;
                    camera.m_videoFPSq = camera.m_videoFPS / m_fps;
                    camera.m_videoFPSCount = camera.m_videoFPSq;
                    camera.m_videoPrevFPSCount = 0;

                    if (getMessageQueueToGUI())
                    {
                        ATVModReport::MsgReportCameraData *report;
                        report = ATVModReport::MsgReportCameraData::create(
                                camera.m_cameraNumber,
                                camera.m_videoFPS,
                                camera.m_videoFPSManual,
                                camera.m_videoFPSManualEnable,
                                camera.m_videoWidth,
                                camera.m_videoHeight,
                                0);
                        getMessageQueueToGUI()->push(report);
                    }
                }

                int fpsIncrement = (int) camera.m_videoFPSCount - camera.m_videoPrevFPSCount;

                // move a number of frames according to increment
                // use grab to test for EOF then retrieve to preserve last valid frame as the current original frame
                cv::Mat colorFrame;
                int grabOK = 0;

                for (int i = 0; i < fpsIncrement; i++)
                {
                    grabOK = camera.m_camera.grab();
                    if (!grabOK) break;
                    // camera.m_camera >> colorFrame;
                    // if (colorFrame.empty()) break;
                }

                if (grabOK) {
                    camera.m_camera.retrieve(colorFrame);
                }

                if (!colorFrame.empty()) // some frames may not come out properly
                {
                    if (m_settings.m_showOverlayText) {
                        mixImageAndText(colorFrame);
                    }

                    cv::cvtColor(colorFrame, camera.m_videoframeOriginal, cv::COLOR_RGB2GRAY);
                    resizeCamera();
                }

                if (camera.m_videoFPSCount < (camera.m_videoFPSManualEnable ? camera.m_videoFPSManual : camera.m_videoFPS))
                {
                    camera.m_videoPrevFPSCount = (int) camera.m_videoFPSCount;
                    camera.m_videoFPSCount += (camera.m_videoFPSManualEnable ? camera.m_videoFPSqManual : camera.m_videoFPSq);
                }
                else
                {
                    camera.m_videoPrevFPSCount = 0;
                    camera.m_videoFPSCount = (camera.m_videoFPSManualEnable ? camera.m_videoFPSqManual : camera.m_videoFPSq);
                }
            }
        }

        m_horizontalCount = 0;
    }
}

void ATVModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = std::sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void ATVModSource::getBaseValues(int outputSampleRate, int linesPerSecond, int& sampleRateUnits, uint32_t& nbPointsPerRateUnit)
{
    int maxPoints = outputSampleRate / linesPerSecond;
    int i = maxPoints;

    for (; i > 0; i--)
    {
        if ((i * linesPerSecond) % 10 == 0)
            break;
    }

    nbPointsPerRateUnit = i == 0 ? maxPoints : i;
    sampleRateUnits = nbPointsPerRateUnit * linesPerSecond;
}

float ATVModSource::getRFBandwidthDivisor(ATVModSettings::ATVModulation modulation)
{
    switch(modulation)
    {
    case ATVModSettings::ATVModulationLSB:
    case ATVModSettings::ATVModulationUSB:
    case ATVModSettings::ATVModulationVestigialLSB:
    case ATVModSettings::ATVModulationVestigialUSB:
        return 1.05f;
        break;
    case ATVModSettings::ATVModulationAM:
    case ATVModSettings::ATVModulationFM:
    default:
        return 2.2f;
    }
}

void ATVModSource::applyStandard(const ATVModSettings& settings)
{
    m_pointsPerSync  = (uint32_t) ((4.7f / 64.0f) * m_pointsPerLine);
    m_pointsPerBP    = (uint32_t) ((5.8f / 64.0f) * m_pointsPerLine);
    m_pointsPerFP    = (uint32_t) ((1.5f / 64.0f) * m_pointsPerLine);
    m_pointsPerVEqu  = (uint32_t) ((2.3f / 64.0f) * m_pointsPerLine); // short vertical sync pulse (equalizing)
    m_pointsPerVSync = (uint32_t) (((32.0f - 4.7f) / 64.0f) * m_pointsPerLine); // broad vertical sync pulse (field sync)

    m_pointsPerImgLine = m_pointsPerLine - m_pointsPerSync - m_pointsPerBP - m_pointsPerFP;
    m_nbHorizPoints    = m_pointsPerLine;

    m_pointsPerHBar    = std::max(1, m_pointsPerImgLine / m_nbBars);
    m_hBarIncrement    = m_spanLevel / (float) (m_nbBars - 1);
    m_vBarIncrement    = m_spanLevel / (float) (m_nbBars - 1);

    m_nbLines          = settings.m_nbLines;
    m_nbLines2         = m_nbLines / 2;
    m_fps              = settings.m_fps * 1.0f;

//    qDebug() << "ATVMod::applyStandard: "
//            << " m_nbLines: " << m_config.m_nbLines
//            << " m_fps: " << m_config.m_fps
//            << " rateUnits: " << rateUnits
//            << " nbPointsPerRateUnit: " << nbPointsPerRateUnit
//            << " m_tvSampleRate: " << m_tvSampleRate;

    switch(settings.m_atvStd)
    {
    case ATVModSettings::ATVStdHSkip:
        m_nbImageLines     = m_nbLines;      // lines less the total number of sync lines (0)
        m_nbImageLines2    = m_nbImageLines; // non interlaced (unused)
        m_imageLineStart1  = 0;
        m_imageLineStart2  = 0;
        m_interlaced       = false;
        m_blankLineLvel    = 0.7f;
        m_nbLines2         = m_nbLines;  // non interlaced
        m_nbLinesField1    = m_nbLines;  // non interlaced
        break;
    case ATVModSettings::ATVStdShort:
        m_nbImageLines     = m_nbLines - 2;  // lines less the total number of sync lines
        m_nbImageLines2    = m_nbImageLines; // non interlaced (unused)
        m_imageLineStart1  = 0;
        m_imageLineStart2  = 0;
        m_interlaced       = false;
        m_blankLineLvel    = 0.7f;
        m_nbLines2         = m_nbLines;      // non interlaced
        m_nbLinesField1    = m_nbLines;      // non interlaced
        break;
    case ATVModSettings::ATVStdShortInterlaced:
        m_nbImageLines2    = m_nbLines2 - 2; // lines in half image less number of sync/service lines
        m_nbImageLines     = 2*m_nbImageLines2;
        m_imageLineStart1  = 1; //m_nbLines % 2;
        m_imageLineStart2  = 0; //1 - (m_nbLines % 2);
        m_interlaced       = true;
        m_blankLineLvel    = 0.7f;
        m_nbLinesField1    = m_nbLines2;
        break;
    case ATVModSettings::ATVStdPAL525: // Follows PAL-M standard
        m_nbImageLines2    = m_nbLines2 - 19; // 525 -> 243 per half
        m_nbImageLines     = 2*m_nbImageLines2;
        m_imageLineStart1  = 1;
        m_imageLineStart2  = 0;
        m_interlaced       = true;
        m_blankLineLvel    = m_blackLevel;
        m_nbLinesField1    = m_nbLines2+1;
        break;
    case ATVModSettings::ATVStd819: // Follows 819 F standard (belgium)
        m_nbImageLines2    = m_nbLines2 - 29;
        m_nbImageLines     = 2*m_nbImageLines2;
        m_imageLineStart1  = 0;
        m_imageLineStart2  = 1;
        m_interlaced       = true;
        m_blankLineLvel    = m_blackLevel;
        m_nbLinesField1    = m_nbLines2;
        break;
    case ATVModSettings::ATVStdPAL625: // Follows PAL-B/G/H standard
    default:
        m_nbImageLines2    = m_nbLines2 - 24; // 625 -> 288 per half
        m_nbImageLines     = 2*m_nbImageLines2;
        m_imageLineStart1  = 0;
        m_imageLineStart2  = 1;
        m_interlaced       = true;
        m_blankLineLvel    = m_blackLevel;
        m_nbLinesField1    = m_nbLines2+1;
    }

    m_linesPerVBar = m_nbImageLines  / m_nbBars;

    if (m_imageOK)
    {
        resizeImage();
    }

    if (m_videoOK)
    {
    	calculateVideoSizes();
    	resizeVideo();
    }

    calculateCamerasSizes();
}

void ATVModSource::openImage(const QString& fileName)
{
    m_imageFromFile = cv::imread(qPrintable(fileName), cv::ImreadModes::IMREAD_GRAYSCALE);
	m_imageOK = m_imageFromFile.data != 0;

	if (m_imageOK)
	{
        m_settings.m_imageFileName = fileName;
        m_imageFromFile.copyTo(m_imageOriginal);

        if (m_settings.m_showOverlayText) {
            mixImageAndText(m_imageOriginal);
	    }

	    resizeImage();
	}
	else
	{
	    m_settings.m_imageFileName.clear();
        qDebug("ATVModSource::openImage: cannot open image file %s", qPrintable(fileName));
	}
}

void ATVModSource::openVideo(const QString& fileName)
{
	//if (m_videoOK && m_video.isOpened()) m_video.release(); should be done by OpenCV in open method

    m_videoOK = m_video.open(qPrintable(fileName));

    if (m_videoOK)
    {
        m_settings.m_videoFileName = fileName;
        m_videoFPS = m_video.get(cv::CAP_PROP_FPS);
        m_videoWidth = (int) m_video.get(cv::CAP_PROP_FRAME_WIDTH);
        m_videoHeight = (int) m_video.get(cv::CAP_PROP_FRAME_HEIGHT);
        m_videoLength = (int) m_video.get(cv::CAP_PROP_FRAME_COUNT);
        int ex = static_cast<int>(m_video.get(cv::CAP_PROP_FOURCC));
        char ext[] = {(char)(ex & 0XFF),(char)((ex & 0XFF00) >> 8),(char)((ex & 0XFF0000) >> 16),(char)((ex & 0XFF000000) >> 24),0};

        qDebug("ATVModSource::openVideo: %s FPS: %f size: %d x %d #frames: %d codec: %s",
                m_video.isOpened() ? "OK" : "KO",
                m_videoFPS,
                m_videoWidth,
                m_videoHeight,
                m_videoLength,
                ext);

        calculateVideoSizes();
        m_videoEOF = false;

        if (getMessageQueueToGUI())
        {
            ATVModReport::MsgReportVideoFileSourceStreamData *report;
            report = ATVModReport::MsgReportVideoFileSourceStreamData::create(m_videoFPS, m_videoLength);
            getMessageQueueToGUI()->push(report);
        }
    }
    else
    {
        m_settings.m_videoFileName.clear();
        qDebug("ATVModSource::openVideo: cannot open video file %s", qPrintable(fileName));
    }
}

void ATVModSource::resizeImage()
{
    float fy = (float) m_nbImageLines / (float) m_imageOriginal.rows;
    float fx = m_pointsPerImgLine / (float) m_imageOriginal.cols;
    cv::resize(m_imageOriginal, m_image, cv::Size(), fx, fy);
    qDebug("ATVModSource::resizeImage: %d x %d -> %d x %d", m_imageOriginal.cols, m_imageOriginal.rows, m_image.cols, m_image.rows);
}

void ATVModSource::calculateVideoSizes()
{
	m_videoFy = (float) m_nbImageLines / (float) m_videoHeight;
	m_videoFx = m_pointsPerImgLine / (float) m_videoWidth;
	m_videoFPSq = m_videoFPS / m_fps;
    m_videoFPSCount = m_videoFPSq;
    m_videoPrevFPSCount = 0;

	qDebug("ATVModSource::calculateVideoSizes: factors: %f x %f FPSq: %f", m_videoFx, m_videoFy, m_videoFPSq);
}

void ATVModSource::resizeVideo()
{
	if (!m_videoframeOriginal.empty()) {
		cv::resize(m_videoframeOriginal, m_videoFrame, cv::Size(), m_videoFx, m_videoFy); // resize current frame
	}
}

void ATVModSource::calculateCamerasSizes()
{
    for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it)
	{
		it->m_videoFy = (float) m_nbImageLines / (float) it->m_videoHeight;
		it->m_videoFx = m_pointsPerImgLine / (float) it->m_videoWidth;
		it->m_videoFPSq = it->m_videoFPS / m_fps;
		it->m_videoFPSqManual = it->m_videoFPSManual / m_fps;
	    it->m_videoFPSCount = 0; //it->m_videoFPSq;
	    it->m_videoPrevFPSCount = 0;

        qDebug("ATVModSource::calculateCamerasSizes: [%d] factors: %f x %f FPSq: %f",
            (int) (it - m_cameras.begin()),  it->m_videoFx, it->m_videoFy, it->m_videoFPSq);
	}
}

void ATVModSource::resizeCameras()
{
    for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it)
	{
		if (!it->m_videoframeOriginal.empty()) {
			cv::resize(it->m_videoframeOriginal, it->m_videoFrame, cv::Size(), it->m_videoFx, it->m_videoFy); // resize current frame
		}
	}
}

void ATVModSource::resizeCamera()
{
    if (!m_cameras[m_cameraIndex].m_videoframeOriginal.empty()) {
        cv::resize(m_cameras[m_cameraIndex].m_videoframeOriginal, m_cameras[m_cameraIndex].m_videoFrame, cv::Size(), m_cameras[m_cameraIndex].m_videoFx, m_cameras[m_cameraIndex].m_videoFy); // resize current frame
    }
}

void ATVModSource::seekVideoFileStream(int seekPercentage)
{
    if ((m_videoOK) && m_video.isOpened())
    {
        int seekPoint = ((m_videoLength * seekPercentage) / 100);
        m_video.set(cv::CAP_PROP_POS_FRAMES, seekPoint);
        m_videoFPSCount = m_videoFPSq;
        m_videoPrevFPSCount = 0;
        m_videoEOF = false;
    }
}

void ATVModSource::scanCameras()
{
	for (int i = 0; i < 4; i++)
	{
		ATVCamera newCamera;
		m_cameras.push_back(newCamera);
		m_cameras.back().m_cameraNumber = i;
		m_cameras.back().m_camera.open(i);

		if (m_cameras.back().m_camera.isOpened())
		{
			m_cameras.back().m_videoFPS = m_cameras.back().m_camera.get(cv::CAP_PROP_FPS);
			m_cameras.back().m_videoWidth = (int) m_cameras.back().m_camera.get(cv::CAP_PROP_FRAME_WIDTH);
			m_cameras.back().m_videoHeight = (int) m_cameras.back().m_camera.get(cv::CAP_PROP_FRAME_HEIGHT);

			//m_cameras.back().m_videoFPS = m_cameras.back().m_videoFPS < 0 ? 16.3f : m_cameras.back().m_videoFPS;

			qDebug("ATVModSource::scanCameras: [%d] FPS: %f %dx%d",
			        i,
			        m_cameras.back().m_videoFPS,
			        m_cameras.back().m_videoWidth ,
			        m_cameras.back().m_videoHeight);
		}
		else
		{
			m_cameras.pop_back();
		}
	}

	if (m_cameras.size() > 0)
	{
	    calculateCamerasSizes();
		m_cameraIndex = 0;
	}
}

void ATVModSource::releaseCameras()
{
	for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it)
	{
		if (it->m_camera.isOpened()) it->m_camera.release();
	}
}

void ATVModSource::getCameraNumbers(std::vector<int>& numbers)
{
    for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it) {
        numbers.push_back(it->m_cameraNumber);
    }

    if (m_cameras.size() > 0)
    {
        m_cameraIndex = 0;

        if (getMessageQueueToGUI())
        {
            ATVModReport::MsgReportCameraData *report;
            report = ATVModReport::MsgReportCameraData::create(
                    m_cameras[0].m_cameraNumber,
                    m_cameras[0].m_videoFPS,
                    m_cameras[0].m_videoFPSManual,
                    m_cameras[0].m_videoFPSManualEnable,
                    m_cameras[0].m_videoWidth,
                    m_cameras[0].m_videoHeight,
                    0);
            getMessageQueueToGUI()->push(report);
        }
    }
}

void ATVModSource::mixImageAndText(cv::Mat& image)
{
    int fontFace = cv::FONT_HERSHEY_PLAIN;
    double fontScale = image.rows / 100.0;
    int thickness = image.cols / 160;
    int baseline=0;

    fontScale = fontScale < 4.0f ? 4.0f : fontScale; // minimum size
    cv::Size textSize = cv::getTextSize(m_settings.m_overlayText.toStdString(), fontFace, fontScale, thickness, &baseline);
    baseline += thickness;

    // position the text in the top left corner
    cv::Point textOrg(6, textSize.height+10);
    // then put the text itself
    cv::putText(image, m_settings.m_overlayText.toStdString(), textOrg, fontFace, fontScale, cv::Scalar::all(255*m_settings.m_uniformLevel), thickness, cv::LINE_AA);
}

void ATVModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ATVModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        getBaseValues(channelSampleRate, m_settings.m_nbLines * m_settings.m_fps, m_tvSampleRate, m_pointsPerLine);

        if (m_tvSampleRate > 0)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorDistance = (Real) m_tvSampleRate / (Real) channelSampleRate;
            m_interpolator.create(32,
                    m_tvSampleRate,
                    m_settings.m_rfBandwidth / getRFBandwidthDivisor(m_settings.m_atvModulation),
                    3.0);
        }
        else
        {
            m_tvSampleRate = channelSampleRate;
        }

        m_SSBFilter->create_filter(0, m_settings.m_rfBandwidth / (float) m_tvSampleRate);
        std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer + (m_ssbFftLen/2), Complex{0.0, 0.0});
        m_SSBFilterBufferIndex = 0;

        m_DSBFilter->create_asym_filter(
            m_settings.m_rfOppBandwidth / (float) m_tvSampleRate,
            m_settings.m_rfBandwidth / (float) m_tvSampleRate
        );
        std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
        m_DSBFilterBufferIndex = 0;

        applyStandard(m_settings); // set all timings

        if (getMessageQueueToGUI())
        {
            ATVModReport::MsgReportEffectiveSampleRate *report;
            report = ATVModReport::MsgReportEffectiveSampleRate::create(m_tvSampleRate, m_pointsPerLine);
            getMessageQueueToGUI()->push(report);
        }
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void ATVModSource::applySettings(const ATVModSettings& settings, bool force)
{
    qDebug() << "ATVModSource::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_rfOppBandwidth: " << settings.m_rfOppBandwidth
            << " m_atvStd: " << (int) settings.m_atvStd
            << " m_nbLines: " << settings.m_nbLines
            << " m_fps: " << settings.m_fps
            << " m_atvModInput: " << (int) settings.m_atvModInput
            << " m_uniformLevel: " << settings.m_uniformLevel
            << " m_atvModulation: " << (int) settings.m_atvModulation
            << " m_videoPlayLoop: " << settings.m_videoPlayLoop
            << " m_videoPlay: " << settings.m_videoPlay
            << " m_cameraPlay: " << settings.m_cameraPlay
            << " m_channelMute: " << settings.m_channelMute
            << " m_invertedVideo: " << settings.m_invertedVideo
            << " m_rfScalingFactor: " << settings.m_rfScalingFactor
            << " m_fmExcursion: " << settings.m_fmExcursion
            << " m_forceDecimator: " << settings.m_forceDecimator
            << " m_showOverlayText: " << settings.m_showOverlayText
            << " m_overlayText: " << settings.m_overlayText
            << " force: " << force;

    if ((settings.m_atvStd != m_settings.m_atvStd)
        || (settings.m_nbLines != m_settings.m_nbLines)
        || (settings.m_fps != m_settings.m_fps)
        || (settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_atvModulation != m_settings.m_atvModulation) || force)
    {
        getBaseValues(m_channelSampleRate, settings.m_nbLines * settings.m_fps, m_tvSampleRate, m_pointsPerLine);

        if (m_tvSampleRate > 0)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorDistance = (Real) m_tvSampleRate / (Real) m_channelSampleRate;
            m_interpolator.create(32,
                    m_tvSampleRate,
                    settings.m_rfBandwidth / getRFBandwidthDivisor(settings.m_atvModulation),
                    3.0);
        }

        m_SSBFilter->create_filter(0, settings.m_rfBandwidth / (float) m_tvSampleRate);
        std::fill(m_SSBFilterBuffer, m_SSBFilterBuffer + (m_ssbFftLen/2), Complex{0.0, 0.0});
        m_SSBFilterBufferIndex = 0;

        applyStandard(settings); // set all timings

        if (getMessageQueueToGUI())
        {
            ATVModReport::MsgReportEffectiveSampleRate *report;
            report = ATVModReport::MsgReportEffectiveSampleRate::create(m_tvSampleRate, m_pointsPerLine);
            getMessageQueueToGUI()->push(report);
        }
    }

    if ((settings.m_rfOppBandwidth != m_settings.m_rfOppBandwidth)
        || (settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_nbLines != m_settings.m_nbLines) // difference in line period may have changed TV sample rate
        || (settings.m_fps != m_settings.m_fps)         //
        || force)
    {
        m_DSBFilter->create_asym_filter(
            settings.m_rfOppBandwidth / (float) m_tvSampleRate,
            settings.m_rfBandwidth / (float) m_tvSampleRate
        );
        std::fill(m_DSBFilterBuffer, m_DSBFilterBuffer + m_ssbFftLen, Complex{0.0, 0.0});
        m_DSBFilterBufferIndex = 0;
    }

    if ((settings.m_showOverlayText != m_settings.m_showOverlayText) || force)
    {
        if (!m_imageFromFile.empty())
        {
            m_imageFromFile.copyTo(m_imageOriginal);

            if (settings.m_showOverlayText) {
                qDebug("ATVModSource::applySettings: set overlay text");
                mixImageAndText(m_imageOriginal);
            } else{
                qDebug("ATVModSource::applySettings: clear overlay text");
            }

            resizeImage();
        }
    }

    m_settings = settings;
}

void ATVModSource::reportVideoFileSourceStreamTiming()
{
    int framesCount;

    if (m_videoOK && m_video.isOpened())
    {
        framesCount = m_video.get(cv::CAP_PROP_POS_FRAMES);;
    } else {
        framesCount = 0;
    }

    if (getMessageQueueToGUI())
    {
        ATVModReport::MsgReportVideoFileSourceStreamTiming *report;
        report = ATVModReport::MsgReportVideoFileSourceStreamTiming::create(framesCount);
        getMessageQueueToGUI()->push(report);
    }
}

void ATVModSource::configureCameraIndex(unsigned int index)
{
    if (index < m_cameras.size())
    {
        m_cameraIndex = index;

        if (getMessageQueueToGUI())
        {
            ATVModReport::MsgReportCameraData *report;
            report = ATVModReport::MsgReportCameraData::create(
                    m_cameras[m_cameraIndex].m_cameraNumber,
                    m_cameras[m_cameraIndex].m_videoFPS,
                    m_cameras[m_cameraIndex].m_videoFPSManual,
                    m_cameras[m_cameraIndex].m_videoFPSManualEnable,
                    m_cameras[m_cameraIndex].m_videoWidth,
                    m_cameras[m_cameraIndex].m_videoHeight,
                    0);
            getMessageQueueToGUI()->push(report);
        }
    }
}

void ATVModSource::configureCameraData(uint32_t index, float mnaualFPS, bool manualFPSEnable)
{
    if (index < m_cameras.size())
    {
        m_cameras[index].m_videoFPSManual = mnaualFPS;
        m_cameras[index].m_videoFPSManualEnable = manualFPSEnable;
    }
}
