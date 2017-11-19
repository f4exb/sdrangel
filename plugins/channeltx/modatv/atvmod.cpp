///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include <time.h>

#include "opencv2/imgproc/imgproc.hpp"

#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "device/devicesinkapi.h"

#include "atvmod.h"

MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureATVMod, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureImageFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportVideoFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportVideoFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureCameraIndex, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureCameraData, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportCameraData, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureOverlayText, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureShowOverlayText, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportEffectiveSampleRate, Message)

const QString ATVMod::m_channelID = "sdrangel.channeltx.modatv";
const float ATVMod::m_blackLevel = 0.3f;
const float ATVMod::m_spanLevel = 0.7f;
const int ATVMod::m_levelNbSamples = 10000; // every 10ms
const int ATVMod::m_nbBars = 6;
const int ATVMod::m_cameraFPSTestNbFrames = 100;
const int ATVMod::m_ssbFftLen = 1024;

ATVMod::ATVMod(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_absoluteFrequencyOffset(0),
	m_modPhasor(0.0f),
    m_tvSampleRate(1000000),
    m_evenImage(true),
    m_settingsMutex(QMutex::Recursive),
    m_horizontalCount(0),
    m_lineCount(0),
    m_movingAverage(40, 0),
	m_imageOK(false),
	m_videoFPSq(1.0f),
    m_videoFPSCount(0.0f),
	m_videoPrevFPSCount(0),
	m_videoEOF(false),
	m_videoOK(false),
	m_cameraIndex(-1),
	m_showOverlayText(false),
    m_SSBFilter(0),
    m_SSBFilterBuffer(0),
    m_SSBFilterBufferIndex(0),
    m_DSBFilter(0),
    m_DSBFilterBuffer(0),
    m_DSBFilterBufferIndex(0)
{
    setObjectName("ATVMod");
    scanCameras();

    m_SSBFilter = new fftfilt(0, m_settings.m_rfBandwidth / m_settings.m_outputSampleRate, m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size
    memset(m_SSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen>>1));

    m_DSBFilter = new fftfilt((2.0f * m_settings.m_rfBandwidth) / m_settings.m_outputSampleRate, 2 * m_ssbFftLen);
    m_DSBFilterBuffer = new Complex[m_ssbFftLen];
    memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));

    m_interpolatorDistanceRemain = 0.0f;
    m_interpolatorDistance = 1.0f;

    m_movingAverage.resize(16, 0);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applySettings(m_settings, true); // does applyStandard() too;
}

ATVMod::~ATVMod()
{
	if (m_video.isOpened()) m_video.release();
	releaseCameras();
	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void ATVMod::pullAudio(int nbSamples __attribute__((unused)))
{
}

void ATVMod::pull(Sample& sample)
{
	if (m_settings.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
		return;
	}

    Complex ci;

    m_settingsMutex.lock();

    if ((m_tvSampleRate == m_settings.m_outputSampleRate) && (!m_settings.m_forceDecimator)) // no interpolation nor decimation
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

void ATVMod::pullFinalize(Complex& ci, Sample& sample)
{
    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    m_settingsMutex.unlock();

    Real magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    magsq /= (1<<30);
    m_movingAverage.feed(magsq);

    sample.m_real = (FixReal) ci.real();
    sample.m_imag = (FixReal) ci.imag();
}

void ATVMod::modulateSample()
{
    Real t;

    pullVideo(t);
    calculateLevel(t);

    t = m_settings.m_invertedVideo ? 1.0f - t : t;

    switch (m_settings.m_atvModulation)
    {
    case ATVModSettings::ATVModulationFM: // FM half bandwidth deviation
    	m_modPhasor += (t - 0.5f) * m_settings.m_fmExcursion * 2.0f * M_PI;
    	if (m_modPhasor > 2.0f * M_PI) m_modPhasor -= 2.0f * M_PI; // limit growth
    	if (m_modPhasor < 2.0f * M_PI) m_modPhasor += 2.0f * M_PI; // limit growth
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

Complex& ATVMod::modulateSSB(Real& sample)
{
    int n_out;
    Complex ci(sample, 0.0f);
    fftfilt::cmplx *filtered;

    n_out = m_SSBFilter->runSSB(ci, &filtered, m_settings.m_atvModulation == ATVModSettings::ATVModulationUSB);

    if (n_out > 0)
    {
        memcpy((void *) m_SSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
        m_SSBFilterBufferIndex = 0;
    }

    m_SSBFilterBufferIndex++;

    return m_SSBFilterBuffer[m_SSBFilterBufferIndex-1];
}

Complex& ATVMod::modulateVestigialSSB(Real& sample)
{
    int n_out;
    Complex ci(sample, 0.0f);
    fftfilt::cmplx *filtered;

    n_out = m_DSBFilter->runAsym(ci, &filtered, m_settings.m_atvModulation == ATVModSettings::ATVModulationVestigialUSB);

    if (n_out > 0)
    {
        memcpy((void *) m_DSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
        m_DSBFilterBufferIndex = 0;
    }

    m_DSBFilterBufferIndex++;

    return m_DSBFilterBuffer[m_DSBFilterBufferIndex-1];
}

void ATVMod::pullVideo(Real& sample)
{
    if ((m_settings.m_atvStd == ATVModSettings::ATVStdHSkip) && (m_lineCount == m_nbLines2)) // last line in skip mode
    {
        pullImageLine(sample, true); // pull image line without sync
    }
    else if (m_lineCount < m_nbLines2 + 1) // even image or non interlaced
    {
        int iLine = m_lineCount;

        if (iLine < m_nbSyncLinesHeadE + m_nbBlankLines)
        {
            pullVSyncLine(sample);
        }
        else if (iLine > m_nbLines2 - m_nbSyncLinesBottom)
        {
            pullVSyncLine(sample);
        }
        else
        {
            pullImageLine(sample);
        }
    }
    else // odd image
    {
        int iLine = m_lineCount - m_nbLines2 - 1;

        if (iLine < m_nbSyncLinesHeadO + m_nbBlankLines)
        {
            pullVSyncLine(sample);
        }
        else if (iLine > m_nbLines2 - 1 - m_nbSyncLinesBottom)
        {
            pullVSyncLine(sample);
        }
        else
        {
            pullImageLine(sample);
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
            if (m_lineCount > (m_nbLines/2)) m_evenImage = !m_evenImage;
        }
        else // new image
        {
            m_lineCount = 0;
            m_evenImage = !m_evenImage;

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
            		    if (m_showOverlayText) {
            		        mixImageAndText(colorFrame);
            		    }

            		    cv::cvtColor(colorFrame, m_videoframeOriginal, CV_BGR2GRAY);
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
                ATVCamera& camera = m_cameras[m_cameraIndex]; // currently selected canera

                if (camera.m_videoFPS < 0.0f) // default frame rate when it could not be obtained via get
                {
                    time_t start, end;
                    cv::Mat frame;

                    if (getMessageQueueToGUI())
                    {
                        MsgReportCameraData *report;
                        report = MsgReportCameraData::create(
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
                        MsgReportCameraData *report;
                        report = MsgReportCameraData::create(
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
                        MsgReportCameraData *report;
                        report = MsgReportCameraData::create(
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

                for (int i = 0; i < fpsIncrement; i++)
                {
                    camera.m_camera >> colorFrame;
                    if (colorFrame.empty()) break;
                }

                if (!colorFrame.empty()) // some frames may not come out properly
                {
                    if (m_showOverlayText) {
                        mixImageAndText(colorFrame);
                    }

                    cv::cvtColor(colorFrame, camera.m_videoframeOriginal, CV_BGR2GRAY);
                    resizeCamera();
                }

                if (camera.m_videoFPSCount < camera.m_videoFPSManualEnable ? camera.m_videoFPSManual : camera.m_videoFPS)
                {
                    camera.m_videoPrevFPSCount = (int) camera.m_videoFPSCount;
                    camera.m_videoFPSCount += camera.m_videoFPSManualEnable ? camera.m_videoFPSqManual : camera.m_videoFPSq;
                }
                else
                {
                    camera.m_videoPrevFPSCount = 0;
                    camera.m_videoFPSCount = camera.m_videoFPSManualEnable ? camera.m_videoFPSqManual : camera.m_videoFPSq;
                }
            }
        }

        m_horizontalCount = 0;
    }
}

void ATVMod::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        qreal rmsLevel = std::sqrt(m_levelSum / m_levelNbSamples);
        //qDebug("NFMMod::calculateLevel: %f %f", rmsLevel, m_peakLevel);
        emit levelChanged(rmsLevel, m_peakLevel, m_levelNbSamples);
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void ATVMod::start()
{
    qDebug() << "ATVMod::start: m_outputSampleRate: " << m_settings.m_outputSampleRate
            << " m_inputFrequencyOffset: " << m_settings.m_inputFrequencyOffset;
}

void ATVMod::stop()
{
}

bool ATVMod::handleMessage(const Message& cmd)
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;

        ATVModSettings settings = m_settings;
        settings.m_outputSampleRate = notif.getSampleRate();
        settings.m_inputFrequencyOffset = notif.getFrequencyOffset();

        applySettings(settings);

        qDebug() << "ATVMod::handleMessage: MsgChannelizerNotification:"
                << " m_outputSampleRate: " << settings.m_outputSampleRate
                << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset;

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
                m_channelizer->getOutputSampleRate(),
                cfg.getCenterFrequency());

        qDebug() << "SSBMod::handleMessage: MsgConfigureChannelizer: sampleRate: " << m_channelizer->getOutputSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        return true;
    }
    else if (MsgConfigureATVMod::match(cmd))
    {
        MsgConfigureATVMod& cfg = (MsgConfigureATVMod&) cmd;

        ATVModSettings settings = cfg.getSettings();

        // These settings are set with UpChannelizer::MsgChannelizerNotification
        m_absoluteFrequencyOffset = settings.m_inputFrequencyOffset;
        settings.m_outputSampleRate = m_settings.m_outputSampleRate;
        settings.m_inputFrequencyOffset = m_settings.m_inputFrequencyOffset;

        applySettings(settings, cfg.getForce());

        qDebug() << "ATVMod::handleMessage: MsgConfigureATVMod:"
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
                << " force: " << cfg.getForce();

        return true;
    }
    else if (MsgConfigureImageFileName::match(cmd))
    {
        MsgConfigureImageFileName& conf = (MsgConfigureImageFileName&) cmd;
        openImage(conf.getFileName());
        return true;
    }
    else if (MsgConfigureVideoFileName::match(cmd))
    {
        MsgConfigureVideoFileName& conf = (MsgConfigureVideoFileName&) cmd;
        openVideo(conf.getFileName());
        return true;
    }
    else if (MsgConfigureVideoFileSourceSeek::match(cmd))
    {
        MsgConfigureVideoFileSourceSeek& conf = (MsgConfigureVideoFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekVideoFileStream(seekPercentage);
        return true;
    }
    else if (MsgConfigureVideoFileSourceStreamTiming::match(cmd))
    {
        int framesCount;

        if (m_videoOK && m_video.isOpened())
        {
            framesCount = m_video.get(CV_CAP_PROP_POS_FRAMES);;
        } else {
            framesCount = 0;
        }

        if (getMessageQueueToGUI())
        {
            MsgReportVideoFileSourceStreamTiming *report;
            report = MsgReportVideoFileSourceStreamTiming::create(framesCount);
            getMessageQueueToGUI()->push(report);
        }

        return true;
    }
    else if (MsgConfigureCameraIndex::match(cmd))
    {
    	MsgConfigureCameraIndex& cfg = (MsgConfigureCameraIndex&) cmd;
    	uint32_t index = cfg.getIndex() & 0x7FFFFFF;

    	if (index < m_cameras.size())
    	{
    		m_cameraIndex = index;

    		if (getMessageQueueToGUI())
    		{
                MsgReportCameraData *report;
                report = MsgReportCameraData::create(
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

    	return true;
    }
    else if (MsgConfigureCameraData::match(cmd))
    {
    	MsgConfigureCameraData& cfg = (MsgConfigureCameraData&) cmd;
    	uint32_t index = cfg.getIndex() & 0x7FFFFFF;
    	float mnaualFPS = cfg.getManualFPS();
    	bool manualFPSEnable = cfg.getManualFPSEnable();

    	if (index < m_cameras.size())
    	{
    		m_cameras[index].m_videoFPSManual = mnaualFPS;
            m_cameras[index].m_videoFPSManualEnable = manualFPSEnable;
    	}

    	return true;
    }
    else if (MsgConfigureOverlayText::match(cmd))
    {
        MsgConfigureOverlayText& cfg = (MsgConfigureOverlayText&) cmd;
        m_overlayText = cfg.getOverlayText().toStdString();
        return true;
    }
    else if (MsgConfigureShowOverlayText::match(cmd))
    {
        MsgConfigureShowOverlayText& cfg = (MsgConfigureShowOverlayText&) cmd;
        bool showOverlayText = cfg.getShowOverlayText();

        if (!m_imageFromFile.empty())
        {
            m_imageFromFile.copyTo(m_imageOriginal);

            if (showOverlayText) {
                qDebug("ATVMod::handleMessage: overlay text");
                mixImageAndText(m_imageOriginal);
            } else{
                qDebug("ATVMod::handleMessage: clear text");
            }

            resizeImage();
        }

        m_showOverlayText = showOverlayText;
        return true;
    }
    else
    {
        return false;
    }
}

void ATVMod::getBaseValues(int outputSampleRate, int linesPerSecond, int& sampleRateUnits, uint32_t& nbPointsPerRateUnit)
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

float ATVMod::getRFBandwidthDivisor(ATVModSettings::ATVModulation modulation)
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

void ATVMod::applyStandard()
{
    m_pointsPerSync  = (uint32_t) ((4.7f / 64.0f) * m_pointsPerLine);
    m_pointsPerBP    = (uint32_t) ((4.7f / 64.0f) * m_pointsPerLine);
    m_pointsPerFP    = (uint32_t) ((2.6f / 64.0f) * m_pointsPerLine);
    m_pointsPerFSync = (uint32_t) ((2.3f / 64.0f) * m_pointsPerLine);

    m_pointsPerImgLine = m_pointsPerLine - m_pointsPerSync - m_pointsPerBP - m_pointsPerFP;
    m_nbHorizPoints    = m_pointsPerLine;

    m_pointsPerHBar    = m_pointsPerImgLine / m_nbBars;
    m_hBarIncrement    = m_spanLevel / (float) m_nbBars;
    m_vBarIncrement    = m_spanLevel / (float) m_nbBars;

    m_nbLines          = m_settings.m_nbLines;
    m_nbLines2         = m_nbLines / 2;
    m_fps              = m_settings.m_fps * 1.0f;

//    qDebug() << "ATVMod::applyStandard: "
//            << " m_nbLines: " << m_config.m_nbLines
//            << " m_fps: " << m_config.m_fps
//            << " rateUnits: " << rateUnits
//            << " nbPointsPerRateUnit: " << nbPointsPerRateUnit
//            << " m_tvSampleRate: " << m_tvSampleRate
//            << " m_pointsPerTU: " << m_pointsPerTU;

    switch(m_settings.m_atvStd)
    {
    case ATVModSettings::ATVStdHSkip:
        m_nbImageLines     = m_nbLines; // lines less the total number of sync lines
        m_nbImageLines2    = m_nbImageLines; // force non interleaved for vbars
        m_interleaved       = false;
        m_nbSyncLinesHeadE = 0; // number of sync lines on the top of a frame even
        m_nbSyncLinesHeadO = 0; // number of sync lines on the top of a frame odd
        m_nbSyncLinesBottom = -1; // force no vsync in even block
        m_nbLongSyncLines  = 0;
        m_nbHalfLongSync   = 0;
        m_nbWholeEqLines   = 0;
        m_singleLongSync   = true;
        m_nbBlankLines     = 0;
        m_blankLineLvel    = 0.7f;
        m_nbLines2         = m_nbLines - 1;
        break;
    case ATVModSettings::ATVStdShort:
        m_nbImageLines     = m_nbLines - 2; // lines less the total number of sync lines
        m_nbImageLines2    = m_nbImageLines; // force non interleaved for vbars
        m_interleaved       = false;
        m_nbSyncLinesHeadE = 1; // number of sync lines on the top of a frame even
        m_nbSyncLinesHeadO = 1; // number of sync lines on the top of a frame odd
        m_nbSyncLinesBottom = 0;
        m_nbLongSyncLines  = 1;
        m_nbHalfLongSync   = 0;
        m_nbWholeEqLines   = 0;
        m_singleLongSync   = true;
        m_nbBlankLines     = 1;
        m_blankLineLvel    = 0.7f;
        m_nbLines2         = m_nbLines; // force non interleaved => treated as even for all lines
        break;
    case ATVModSettings::ATVStdShortInterleaved:
        m_nbImageLines     = m_nbLines - 2; // lines less the total number of sync lines
        m_nbImageLines2    = m_nbImageLines / 2;
        m_interleaved       = true;
        m_nbSyncLinesHeadE = 1; // number of sync lines on the top of a frame even
        m_nbSyncLinesHeadO = 1; // number of sync lines on the top of a frame odd
        m_nbSyncLinesBottom = 0;
        m_nbLongSyncLines  = 1;
        m_nbHalfLongSync   = 0;
        m_nbWholeEqLines   = 0;
        m_singleLongSync   = true;
        m_nbBlankLines     = 1;
        m_blankLineLvel    = 0.7f;
        break;
    case ATVModSettings::ATVStd405: // Follows loosely the 405 lines standard
        m_nbImageLines     = m_nbLines - 15; // lines less the total number of sync lines
        m_nbImageLines2    = m_nbImageLines / 2;
        m_interleaved       = true;
        m_nbSyncLinesHeadE = 5; // number of sync lines on the top of a frame even
        m_nbSyncLinesHeadO = 4; // number of sync lines on the top of a frame odd
        m_nbSyncLinesBottom = 3;
        m_nbLongSyncLines  = 2;
        m_nbHalfLongSync   = 1;
        m_nbWholeEqLines   = 2;
        m_singleLongSync   = false;
        m_nbBlankLines     = 7; // yields 376 lines (195 - 7) * 2
        m_blankLineLvel    = m_blackLevel;
        break;
    case ATVModSettings::ATVStdPAL525: // Follows PAL-M standard
        m_nbImageLines     = m_nbLines - 15;
        m_nbImageLines2    = m_nbImageLines / 2;
        m_interleaved       = true;
        m_nbSyncLinesHeadE = 5;
        m_nbSyncLinesHeadO = 4; // number of sync lines on the top of a frame odd
        m_nbSyncLinesBottom = 3;
        m_nbLongSyncLines  = 2;
        m_nbHalfLongSync   = 1;
        m_nbWholeEqLines   = 2;
        m_singleLongSync   = false;
        m_nbBlankLines     = 15; // yields 480 lines (255 - 15) * 2
        m_blankLineLvel    = m_blackLevel;
        break;
    case ATVModSettings::ATVStdPAL625: // Follows PAL-B/G/H standard
    default:
        m_nbImageLines     = m_nbLines - 15;
        m_nbImageLines2    = m_nbImageLines / 2;
        m_interleaved       = true;
        m_nbSyncLinesHeadE = 5;
        m_nbSyncLinesHeadO = 4; // number of sync lines on the top of a frame odd
        m_nbSyncLinesBottom = 3;
        m_nbLongSyncLines  = 2;
        m_nbHalfLongSync   = 1;
        m_nbWholeEqLines   = 2;
        m_singleLongSync   = false;
        m_nbBlankLines     = 17; // yields 576 lines (305 - 17) * 2
        m_blankLineLvel    = m_blackLevel;
    }

    m_linesPerVBar = m_nbImageLines2  / m_nbBars;

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

void ATVMod::openImage(const QString& fileName)
{
    m_imageFromFile = cv::imread(qPrintable(fileName), CV_LOAD_IMAGE_GRAYSCALE);
	m_imageOK = m_imageFromFile.data != 0;

	if (m_imageOK)
	{
        m_imageFromFile.copyTo(m_imageOriginal);

        if (m_showOverlayText) {
            mixImageAndText(m_imageOriginal);
	    }

	    resizeImage();
	}
}

void ATVMod::openVideo(const QString& fileName)
{
	//if (m_videoOK && m_video.isOpened()) m_video.release(); should be done by OpenCV in open method

    m_videoOK = m_video.open(qPrintable(fileName));

    if (m_videoOK)
    {
        m_videoFPS = m_video.get(CV_CAP_PROP_FPS);
        m_videoWidth = (int) m_video.get(CV_CAP_PROP_FRAME_WIDTH);
        m_videoHeight = (int) m_video.get(CV_CAP_PROP_FRAME_HEIGHT);
        m_videoLength = (int) m_video.get(CV_CAP_PROP_FRAME_COUNT);
        int ex = static_cast<int>(m_video.get(CV_CAP_PROP_FOURCC));
        char ext[] = {(char)(ex & 0XFF),(char)((ex & 0XFF00) >> 8),(char)((ex & 0XFF0000) >> 16),(char)((ex & 0XFF000000) >> 24),0};

        qDebug("ATVMod::openVideo: %s FPS: %f size: %d x %d #frames: %d codec: %s",
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
            MsgReportVideoFileSourceStreamData *report;
            report = MsgReportVideoFileSourceStreamData::create(m_videoFPS, m_videoLength);
            getMessageQueueToGUI()->push(report);
        }
    }
    else
    {
        qDebug("ATVMod::openVideo: cannot open video file");
    }
}

void ATVMod::resizeImage()
{
    float fy = (m_nbImageLines - 2*m_nbBlankLines) / (float) m_imageOriginal.rows;
    float fx = m_pointsPerImgLine / (float) m_imageOriginal.cols;
    cv::resize(m_imageOriginal, m_image, cv::Size(), fx, fy);
    qDebug("ATVMod::resizeImage: %d x %d -> %d x %d", m_imageOriginal.cols, m_imageOriginal.rows, m_image.cols, m_image.rows);
}

void ATVMod::calculateVideoSizes()
{
	m_videoFy = (m_nbImageLines - 2*m_nbBlankLines) / (float) m_videoHeight;
	m_videoFx = m_pointsPerImgLine / (float) m_videoWidth;
	m_videoFPSq = m_videoFPS / m_fps;
    m_videoFPSCount = m_videoFPSq;
    m_videoPrevFPSCount = 0;

	qDebug("ATVMod::calculateVideoSizes: factors: %f x %f FPSq: %f", m_videoFx, m_videoFy, m_videoFPSq);
}

void ATVMod::resizeVideo()
{
	if (!m_videoframeOriginal.empty()) {
		cv::resize(m_videoframeOriginal, m_videoFrame, cv::Size(), m_videoFx, m_videoFy); // resize current frame
	}
}

void ATVMod::calculateCamerasSizes()
{
    for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it)
	{
		it->m_videoFy = (m_nbImageLines - 2*m_nbBlankLines) / (float) it->m_videoHeight;
		it->m_videoFx = m_pointsPerImgLine / (float) it->m_videoWidth;
		it->m_videoFPSq = it->m_videoFPS / m_fps;
		it->m_videoFPSqManual = it->m_videoFPSManual / m_fps;
	    it->m_videoFPSCount = 0; //it->m_videoFPSq;
	    it->m_videoPrevFPSCount = 0;

        qDebug("ATVMod::calculateCamerasSizes: [%d] factors: %f x %f FPSq: %f", (int) (it - m_cameras.begin()),  it->m_videoFx, it->m_videoFy, it->m_videoFPSq);
	}
}

void ATVMod::resizeCameras()
{
    for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it)
	{
		if (!it->m_videoframeOriginal.empty()) {
			cv::resize(it->m_videoframeOriginal, it->m_videoFrame, cv::Size(), it->m_videoFx, it->m_videoFy); // resize current frame
		}
	}
}

void ATVMod::resizeCamera()
{
    ATVCamera& camera = m_cameras[m_cameraIndex];

    if (!camera.m_videoframeOriginal.empty()) {
        cv::resize(camera.m_videoframeOriginal, camera.m_videoFrame, cv::Size(), camera.m_videoFx, camera.m_videoFy); // resize current frame
    }
}

void ATVMod::seekVideoFileStream(int seekPercentage)
{
    QMutexLocker mutexLocker(&m_settingsMutex);

    if ((m_videoOK) && m_video.isOpened())
    {
        int seekPoint = ((m_videoLength * seekPercentage) / 100);
        m_video.set(CV_CAP_PROP_POS_FRAMES, seekPoint);
        m_videoFPSCount = m_videoFPSq;
        m_videoPrevFPSCount = 0;
        m_videoEOF = false;
    }
}

void ATVMod::scanCameras()
{
	for (int i = 0; i < 4; i++)
	{
		ATVCamera newCamera;
		m_cameras.push_back(newCamera);
		m_cameras.back().m_cameraNumber = i;
		m_cameras.back().m_camera.open(i);

		if (m_cameras.back().m_camera.isOpened())
		{
			m_cameras.back().m_videoFPS = m_cameras.back().m_camera.get(CV_CAP_PROP_FPS);
			m_cameras.back().m_videoWidth = (int) m_cameras.back().m_camera.get(CV_CAP_PROP_FRAME_WIDTH);
			m_cameras.back().m_videoHeight = (int) m_cameras.back().m_camera.get(CV_CAP_PROP_FRAME_HEIGHT);

			//m_cameras.back().m_videoFPS = m_cameras.back().m_videoFPS < 0 ? 16.3f : m_cameras.back().m_videoFPS;

			qDebug("ATVMod::scanCameras: [%d] FPS: %f %dx%d",
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

void ATVMod::releaseCameras()
{
	for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it)
	{
		if (it->m_camera.isOpened()) it->m_camera.release();
	}
}

void ATVMod::getCameraNumbers(std::vector<int>& numbers)
{
    for (std::vector<ATVCamera>::iterator it = m_cameras.begin(); it != m_cameras.end(); ++it) {
        numbers.push_back(it->m_cameraNumber);
    }

    if (m_cameras.size() > 0)
    {
        m_cameraIndex = 0;

        if (getMessageQueueToGUI())
        {
            MsgReportCameraData *report;
            report = MsgReportCameraData::create(
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

void ATVMod::mixImageAndText(cv::Mat& image)
{
    int fontFace = cv::FONT_HERSHEY_PLAIN;
    double fontScale = image.rows / 100.0;
    int thickness = image.cols / 160;
    int baseline=0;

    fontScale = fontScale < 4.0f ? 4.0f : fontScale; // minimum size
    cv::Size textSize = cv::getTextSize(m_overlayText, fontFace, fontScale, thickness, &baseline);
    baseline += thickness;

    // position the text in the top left corner
    cv::Point textOrg(6, textSize.height+10);
    // then put the text itself
    cv::putText(image, m_overlayText, textOrg, fontFace, fontScale, cv::Scalar::all(255*m_settings.m_uniformLevel), thickness, CV_AA);
}

void ATVMod::applySettings(const ATVModSettings& settings, bool force)
{
    if ((settings.m_outputSampleRate != m_settings.m_outputSampleRate)
        || (settings.m_atvStd != m_settings.m_atvStd)
        || (settings.m_nbLines != m_settings.m_nbLines)
        || (settings.m_fps != m_settings.m_fps)
        || (settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_atvModulation != m_settings.m_atvModulation) || force)
    {
        getBaseValues(settings.m_outputSampleRate, settings.m_nbLines * settings.m_fps, m_tvSampleRate, m_pointsPerLine);

        m_settingsMutex.lock();

        if (m_tvSampleRate > 0)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorDistance = (Real) m_tvSampleRate / (Real) settings.m_outputSampleRate;
            m_interpolator.create(32,
                    m_tvSampleRate,
                    settings.m_rfBandwidth / getRFBandwidthDivisor(settings.m_atvModulation),
                    3.0);
        }
        else
        {
            m_tvSampleRate = settings.m_outputSampleRate;
        }

        m_SSBFilter->create_filter(0, settings.m_rfBandwidth / m_tvSampleRate);
        memset(m_SSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen>>1));
        m_SSBFilterBufferIndex = 0;

        applyStandard(); // set all timings
        m_settingsMutex.unlock();

        if (getMessageQueueToGUI())
        {
            MsgReportEffectiveSampleRate *report;
            report = MsgReportEffectiveSampleRate::create(m_tvSampleRate, m_pointsPerLine);
            getMessageQueueToGUI()->push(report);
        }
    }

    if ((settings.m_outputSampleRate != m_settings.m_outputSampleRate)
        || (settings.m_rfOppBandwidth != m_settings.m_rfOppBandwidth)
        || (settings.m_rfBandwidth != m_settings.m_rfBandwidth)
        || (settings.m_nbLines != m_settings.m_nbLines) // difference in line period may have changed TV sample rate
        || (settings.m_fps != m_settings.m_fps)         //
        || force)
    {
        m_settingsMutex.lock();

        m_DSBFilter->create_asym_filter(settings.m_rfOppBandwidth / m_tvSampleRate, settings.m_rfBandwidth / m_tvSampleRate);
        memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));
        m_DSBFilterBufferIndex = 0;

        m_settingsMutex.unlock();
    }

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) ||
        (settings.m_outputSampleRate != m_settings.m_outputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_carrierNco.setFreq(settings.m_inputFrequencyOffset, settings.m_outputSampleRate);
        m_settingsMutex.unlock();
    }

    m_settings = settings;
}
