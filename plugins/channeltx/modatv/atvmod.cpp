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
#include "atvmod.h"

MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureATVMod, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureImageFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportVideoFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportVideoFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureCameraIndex, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportCameraData, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureOverlayText, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureShowOverlayText, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgReportEffectiveSampleRate, Message)

const float ATVMod::m_blackLevel = 0.3f;
const float ATVMod::m_spanLevel = 0.7f;
const int ATVMod::m_levelNbSamples = 10000; // every 10ms
const int ATVMod::m_nbBars = 6;
const int ATVMod::m_cameraFPSTestNbFrames = 100;
const int ATVMod::m_ssbFftLen = 1024;

ATVMod::ATVMod() :
	m_modPhasor(0.0f),
    m_evenImage(true),
    m_tvSampleRate(1000000),
    m_settingsMutex(QMutex::Recursive),
    m_horizontalCount(0),
    m_lineCount(0),
	m_imageOK(false),
	m_videoFPSq(1.0f),
    m_videoFPSCount(0.0f),
	m_videoPrevFPSCount(0),
	m_videoEOF(false),
	m_videoOK(false),
	m_cameraIndex(-1),
	m_showOverlayText(false),
    m_SSBFilter(0),
    m_DSBFilter(0),
    m_SSBFilterBuffer(0),
    m_DSBFilterBuffer(0),
    m_SSBFilterBufferIndex(0),
    m_DSBFilterBufferIndex(0)
{
    setObjectName("ATVMod");
    scanCameras();

    m_config.m_outputSampleRate = 1000000;
    m_config.m_inputFrequencyOffset = 0;
    m_config.m_rfBandwidth = 1000000;
    m_config.m_atvModInput = ATVModInputHBars;
    m_config.m_atvStd = ATVStdPAL625;

    m_SSBFilter = new fftfilt(0, m_config.m_rfBandwidth / m_config.m_outputSampleRate, m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size
    memset(m_SSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen>>1));

    m_DSBFilter = new fftfilt((2.0f * m_config.m_rfBandwidth) / m_config.m_outputSampleRate, 2 * m_ssbFftLen);
    m_DSBFilterBuffer = new Complex[m_ssbFftLen];
    memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));

    applyStandard();

    m_interpolatorDistanceRemain = 0.0f;
    m_interpolatorDistance = 1.0f;

    apply(true);

    m_movingAverage.resize(16, 0);
}

ATVMod::~ATVMod()
{
	if (m_video.isOpened()) m_video.release();
	releaseCameras();
}

void ATVMod::configure(MessageQueue* messageQueue,
            Real rfBandwidth,
            Real rfOppBandwidth,
            ATVStd atvStd,
            ATVModInput atvModInput,
            Real uniformLevel,
			ATVModulation atvModulation,
            bool videoPlayLoop,
            bool videoPlay,
            bool cameraPlay,
            bool channelMute,
            bool invertedVideo)
{
    Message* cmd = MsgConfigureATVMod::create(
            rfBandwidth,
            rfOppBandwidth,
            atvStd,
            atvModInput,
            uniformLevel,
            atvModulation,
            videoPlayLoop,
            videoPlay,
            cameraPlay,
            channelMute,
            invertedVideo);
    messageQueue->push(cmd);
}

void ATVMod::pullAudio(int nbSamples)
{
}

void ATVMod::pull(Sample& sample)
{
	if (m_running.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
		return;
	}

    Complex ci;

    m_settingsMutex.lock();

    if (m_tvSampleRate == m_running.m_outputSampleRate) // no interpolation nor decimation
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

    t = m_running.m_invertedVideo ? 1.0f - t : t;

    switch (m_running.m_atvModulation)
    {
    case ATVModulationFM: // FM half bandwidth deviation
    	m_modPhasor += (t - 0.5f) * M_PI;
    	if (m_modPhasor > 2.0f * M_PI) m_modPhasor -= 2.0f * M_PI; // limit growth
    	if (m_modPhasor < 2.0f * M_PI) m_modPhasor += 2.0f * M_PI; // limit growth
    	m_modSample.real(cos(m_modPhasor) * 29204.0f); // -1 dB
    	m_modSample.imag(sin(m_modPhasor) * 29204.0f);
    	break;
    case ATVModulationLSB:
    case ATVModulationUSB:
        m_modSample = modulateSSB(t);
        m_modSample *= 29204.0f;
        break;
    case ATVModulationVestigialLSB:
    case ATVModulationVestigialUSB:
        m_modSample = modulateVestigialSSB(t);
        m_modSample *= 29204.0f;
        break;
    case ATVModulationAM: // AM 90%
    default:
        m_modSample.real((t*1.8f + 0.1f) * 16384.0f); // modulate and scale zero frequency carrier
        m_modSample.imag(0.0f);
    }
}

Complex& ATVMod::modulateSSB(Real& sample)
{
    int n_out;
    Complex ci(sample, 0.0f);
    fftfilt::cmplx *filtered;

    n_out = m_SSBFilter->runSSB(ci, &filtered, m_running.m_atvModulation == ATVModulationUSB);

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

    n_out = m_DSBFilter->runAsym(ci, &filtered, m_running.m_atvModulation == ATVModulationVestigialUSB);

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
    int iLine = m_lineCount % m_nbLines2;

    if (m_lineCount < m_nbLines2) // even image or non interlaced
    {
        if (iLine < m_nbSyncLinesHead)
        {
            pullVSyncLine(sample);
        }
        else if (iLine < m_nbSyncLinesHead + m_nbBlankLines)
        {
            pullVSyncLine(sample); // pull black line
        }
        else if (iLine < m_nbLines2 - 3)
        {
            pullImageLine(sample);
        }
        else
        {
            pullVSyncLine(sample);
        }
    }
    else // odd image
    {
        if (iLine < m_nbSyncLinesHead - 1)
        {
            pullVSyncLine(sample);
        }
        else if (iLine < m_nbSyncLinesHead + m_nbBlankLines - 1)
        {
            pullVSyncLine(sample); // pull black line
        }
        else if (iLine < m_nbLines2 - 4)
        {
            pullImageLine(sample);
        }
        else
        {
            pullVSyncLine(sample);
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

            if ((m_running.m_atvModInput == ATVModInputVideo) && m_videoOK && (m_running.m_videoPlay) && !m_videoEOF)
            {
            	int grabOK;
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
            	    if (m_running.m_videoPlayLoop) { // play loop
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
            else if ((m_running.m_atvModInput == ATVModInputCamera) && (m_running.m_cameraPlay))
            {
                ATVCamera& camera = m_cameras[m_cameraIndex]; // currently selected canera

                if (camera.m_videoFPS < 0.0f) // default frame rate when it could not be obtained via get
                {
                    time_t start, end;
                    cv::Mat frame;

                    MsgReportCameraData *report;
                    report = MsgReportCameraData::create(
                            camera.m_cameraNumber,
                            0.0f,
                            camera.m_videoWidth,
                            camera.m_videoHeight,
                            1); // open splash screen on GUI side
                    getOutputMessageQueue()->push(report);
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

                    report = MsgReportCameraData::create(
                            camera.m_cameraNumber,
                            camera.m_videoFPS,
                            camera.m_videoWidth,
                            camera.m_videoHeight,
                            2); // close splash screen on GUI side
                    getOutputMessageQueue()->push(report);
                }
                else if (camera.m_videoFPS == 0.0f) // Hideous hack for windows
                {
                    camera.m_videoFPS = 5.0f;
                    camera.m_videoFPSq = camera.m_videoFPS / m_fps;
                    camera.m_videoFPSCount = camera.m_videoFPSq;
                    camera.m_videoPrevFPSCount = 0;

                    MsgReportCameraData *report;
                    report = MsgReportCameraData::create(
                            camera.m_cameraNumber,
                            camera.m_videoFPS,
                            camera.m_videoWidth,
                            camera.m_videoHeight,
                            0);
                    getOutputMessageQueue()->push(report);
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

                if (camera.m_videoFPSCount < camera.m_videoFPS)
                {
                    camera.m_videoPrevFPSCount = (int) camera.m_videoFPSCount;
                    camera.m_videoFPSCount += camera.m_videoFPSq;
                }
                else
                {
                    camera.m_videoPrevFPSCount = 0;
                    camera.m_videoFPSCount = camera.m_videoFPSq;
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
    qDebug() << "ATVMod::start: m_outputSampleRate: " << m_config.m_outputSampleRate
            << " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;
}

void ATVMod::stop()
{
}

bool ATVMod::handleMessage(const Message& cmd)
{
    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;

        m_config.m_outputSampleRate = notif.getSampleRate();
        m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

        apply();

        qDebug() << "ATVMod::handleMessage: MsgChannelizerNotification:"
                << " m_outputSampleRate: " << m_config.m_outputSampleRate
                << " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

        return true;
    }
    else if (MsgConfigureATVMod::match(cmd))
    {
        MsgConfigureATVMod& cfg = (MsgConfigureATVMod&) cmd;

        m_config.m_rfBandwidth = cfg.getRFBandwidth();
        m_config.m_rfOppBandwidth = cfg.getRFOppBandwidth();
        m_config.m_atvModInput = cfg.getATVModInput();
        m_config.m_atvStd = cfg.getATVStd();
        m_config.m_uniformLevel = cfg.getUniformLevel();
        m_config.m_atvModulation = cfg.getModulation();
        m_config.m_videoPlayLoop = cfg.getVideoPlayLoop();
        m_config.m_videoPlay = cfg.getVideoPlay();
        m_config.m_cameraPlay = cfg.getCameraPlay();
        m_config.m_channelMute = cfg.getChannelMute();
        m_config.m_invertedVideo = cfg.getInvertedVideo();

        apply();

        qDebug() << "ATVMod::handleMessage: MsgConfigureATVMod:"
                << " m_rfBandwidth: " << m_config.m_rfBandwidth
                << " m_rfOppBandwidth: " << m_config.m_rfOppBandwidth
                << " m_atvStd: " << (int) m_config.m_atvStd
                << " m_atvModInput: " << (int) m_config.m_atvModInput
                << " m_uniformLevel: " << m_config.m_uniformLevel
				<< " m_atvModulation: " << (int) m_config.m_atvModulation
				<< " m_videoPlayLoop: " << m_config.m_videoPlayLoop
				<< " m_videoPlay: " << m_config.m_videoPlay
				<< " m_cameraPlay: " << m_config.m_cameraPlay
				<< " m_channelMute: " << m_config.m_channelMute
				<< " m_invertedVideo: " << m_config.m_invertedVideo;

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

        MsgReportVideoFileSourceStreamTiming *report;
        report = MsgReportVideoFileSourceStreamTiming::create(framesCount);
        getOutputMessageQueue()->push(report);

        return true;
    }
    else if (MsgConfigureCameraIndex::match(cmd))
    {
    	MsgConfigureCameraIndex& cfg = (MsgConfigureCameraIndex&) cmd;
    	int index = cfg.getIndex();

    	if (index < m_cameras.size())
    	{
    		m_cameraIndex = index;
    		MsgReportCameraData *report;
            report = MsgReportCameraData::create(
            		m_cameras[m_cameraIndex].m_cameraNumber,
    				m_cameras[m_cameraIndex].m_videoFPS,
    				m_cameras[m_cameraIndex].m_videoWidth,
    				m_cameras[m_cameraIndex].m_videoHeight,
    				0);
            getOutputMessageQueue()->push(report);
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

void ATVMod::apply(bool force)
{
    if ((m_config.m_outputSampleRate != m_running.m_outputSampleRate)
        || (m_config.m_atvStd != m_running.m_atvStd)
        || (m_config.m_rfBandwidth != m_running.m_rfBandwidth)
        || (m_config.m_atvModulation != m_running.m_atvModulation) || force)
    {
        int rateUnits = getSampleRateUnits(m_config.m_atvStd);
        m_tvSampleRate = (m_config.m_outputSampleRate / rateUnits) * rateUnits; // make sure working sample rate is a multiple of rate units

        m_settingsMutex.lock();

        if (m_tvSampleRate > 0)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorDistance = (Real) m_tvSampleRate / (Real) m_config.m_outputSampleRate;
            m_interpolator.create(48,
                    m_tvSampleRate,
                    m_config.m_rfBandwidth / getRFBandwidthDivisor(m_config.m_atvModulation),
                    3.0);
        }
        else
        {
            m_tvSampleRate = m_config.m_outputSampleRate;
        }

        m_SSBFilter->create_filter(0, m_config.m_rfBandwidth / m_tvSampleRate);
        memset(m_SSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen>>1));
        m_SSBFilterBufferIndex = 0;

        applyStandard(); // set all timings
        m_settingsMutex.unlock();

        MsgReportEffectiveSampleRate *report;
        report = MsgReportEffectiveSampleRate::create(m_tvSampleRate);
        getOutputMessageQueue()->push(report);
    }

    if ((m_config.m_outputSampleRate != m_running.m_outputSampleRate) ||
        (m_config.m_rfOppBandwidth != m_running.m_rfOppBandwidth) ||
        (m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
        (m_config.m_atvStd != m_running.m_atvStd) || force) // difference in TV standard may have changed TV sample rate
    {
        m_settingsMutex.lock();

        m_DSBFilter->create_asym_filter(m_config.m_rfOppBandwidth / m_tvSampleRate, m_config.m_rfBandwidth / m_tvSampleRate);
        memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));
        m_DSBFilterBufferIndex = 0;

        m_settingsMutex.unlock();
    }

    if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
        (m_config.m_outputSampleRate != m_running.m_outputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_carrierNco.setFreq(m_config.m_inputFrequencyOffset, m_config.m_outputSampleRate);
        m_settingsMutex.unlock();
    }

    m_running.m_outputSampleRate = m_config.m_outputSampleRate;
    m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
    m_running.m_rfBandwidth = m_config.m_rfBandwidth;
    m_running.m_rfOppBandwidth = m_config.m_rfOppBandwidth;
    m_running.m_atvModInput = m_config.m_atvModInput;
    m_running.m_atvStd = m_config.m_atvStd;
    m_running.m_uniformLevel = m_config.m_uniformLevel;
    m_running.m_atvModulation = m_config.m_atvModulation;
    m_running.m_videoPlayLoop = m_config.m_videoPlayLoop;
    m_running.m_videoPlay = m_config.m_videoPlay;
    m_running.m_cameraPlay = m_config.m_cameraPlay;
    m_running.m_channelMute = m_config.m_channelMute;
    m_running.m_invertedVideo = m_config.m_invertedVideo;
}

int ATVMod::getSampleRateUnits(ATVStd std)
{
    switch(std)
    {
    case ATVStd405:
        return 729000;  // 72 * 10125
        break;
    case ATVStdPAL525:
    	return 1008000; // 64 * 15750
    	break;
    case ATVStdPAL625:
    default:
        return 1000000; // 64 * 15625 Exact MS/s - us
    }
}

float ATVMod::getRFBandwidthDivisor(ATVModulation modulation)
{
    switch(modulation)
    {
    case ATVModulationLSB:
    case ATVModulationUSB:
    case ATVModulationVestigialLSB:
    case ATVModulationVestigialUSB:
        return 1.1f;
        break;
    case ATVModulationAM:
    case ATVModulationFM:
    default:
        return 2.2f;
    }
}

void ATVMod::applyStandard()
{
    int rateUnits = getSampleRateUnits(m_config.m_atvStd);
    m_pointsPerTU = m_tvSampleRate / rateUnits; // TV sample rate is already set at a multiple of rate units

    switch(m_config.m_atvStd)
    {
    case ATVStd405:    // Follows loosely the 405 lines standard
        m_pointsPerSync    = (uint32_t) roundf(4.7f * m_pointsPerTU); // normal sync pulse (4.7/1.008 us)
        m_pointsPerBP      = (uint32_t) roundf(4.7f * m_pointsPerTU); // back porch        (4.7/1.008 us)
        m_pointsPerFP      = (uint32_t) roundf(1.5f * m_pointsPerTU); // front porch       (1.5/1.008 us)
        m_pointsPerFSync   = (uint32_t) roundf(2.3f * m_pointsPerTU); // equalizing pulse  (2.3/1.008 us)
        // what is left in a 72/0.729 us line for the image
        m_pointsPerImgLine = 72 * m_pointsPerTU - m_pointsPerSync - m_pointsPerBP - m_pointsPerFP;
        m_nbLines          = 405;
        m_nbLines2         = 203;
        m_nbImageLines     = 390;
        m_nbImageLines2    = 195;
        m_interlaced       = true;
        m_nbHorizPoints    = 72 * m_pointsPerTU; // full line
        m_nbSyncLinesHead  = 5;
        m_nbBlankLines     = 7; // yields 376 lines (195 - 7) * 2
        m_pointsPerHBar    = m_pointsPerImgLine / m_nbBars;
        m_linesPerVBar     = m_nbImageLines2  / m_nbBars;
        m_hBarIncrement    = m_spanLevel / (float) m_nbBars;
        m_vBarIncrement    = m_spanLevel / (float) m_nbBars;
        m_fps              = 25.0f;
        break;
    case ATVStdPAL525: // Follows PAL-M standard
        m_pointsPerSync    = (uint32_t) roundf(4.7f * m_pointsPerTU); // normal sync pulse (4.7/1.008 us)
        m_pointsPerBP      = (uint32_t) roundf(4.7f * m_pointsPerTU); // back porch        (4.7/1.008 us)
        m_pointsPerFP      = (uint32_t) roundf(1.5f * m_pointsPerTU); // front porch       (1.5/1.008 us)
        m_pointsPerFSync   = (uint32_t) roundf(2.3f * m_pointsPerTU); // equalizing pulse  (2.3/1.008 us)
        // what is left in a 64/1.008 us line for the image
        m_pointsPerImgLine = 64 * m_pointsPerTU - m_pointsPerSync - m_pointsPerBP - m_pointsPerFP;
        m_nbLines          = 525;
        m_nbLines2         = 263;
        m_nbImageLines     = 510;
        m_nbImageLines2    = 255;
        m_interlaced       = true;
        m_nbHorizPoints    = 64 * m_pointsPerTU; // full line
        m_nbSyncLinesHead  = 5;
        m_nbBlankLines     = 15; // yields 480 lines (255 - 15) * 2
        m_pointsPerHBar    = m_pointsPerImgLine / m_nbBars;
        m_linesPerVBar     = m_nbImageLines2  / m_nbBars;
        m_hBarIncrement    = m_spanLevel / (float) m_nbBars;
        m_vBarIncrement    = m_spanLevel / (float) m_nbBars;
        m_fps              = 30.0f;
        break;
    case ATVStdPAL625: // Follows PAL-B/G/H standard
    default:
        m_pointsPerSync    = (uint32_t) roundf(4.7f * m_pointsPerTU); // normal sync pulse (4.7 us)
        m_pointsPerBP      = (uint32_t) roundf(4.7f * m_pointsPerTU); // back porch        (4.7 us)
        m_pointsPerFP      = (uint32_t) roundf(1.5f * m_pointsPerTU); // front porch       (1.5 us)
        m_pointsPerFSync   = (uint32_t) roundf(2.3f * m_pointsPerTU); // equalizing pulse  (2.3 us)
        // what is left in a 64 us line for the image
        m_pointsPerImgLine = 64 * m_pointsPerTU - m_pointsPerSync - m_pointsPerBP - m_pointsPerFP;
        m_nbLines          = 625;
        m_nbLines2         = 313;
        m_nbImageLines     = 610;
        m_nbImageLines2    = 305;
        m_interlaced       = true;
        m_nbHorizPoints    = 64 * m_pointsPerTU; // full line
        m_nbSyncLinesHead  = 5;
        m_nbBlankLines     = 17; // yields 576 lines (305 - 17) * 2
        m_pointsPerHBar    = m_pointsPerImgLine / m_nbBars;
        m_linesPerVBar     = m_nbImageLines2 / m_nbBars;
        m_hBarIncrement    = m_spanLevel / (float) m_nbBars;
        m_vBarIncrement    = m_spanLevel / (float) m_nbBars;
        m_fps              = 25.0f;
    }

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

        MsgReportVideoFileSourceStreamData *report;
        report = MsgReportVideoFileSourceStreamData::create(m_videoFPS, m_videoLength);
        getOutputMessageQueue()->push(report);
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
	    it->m_videoFPSCount = it->m_videoFPSq;
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
        MsgReportCameraData *report;
        report = MsgReportCameraData::create(
                m_cameras[0].m_cameraNumber,
                m_cameras[0].m_videoFPS,
                m_cameras[0].m_videoWidth,
                m_cameras[0].m_videoHeight,
                0);
        getOutputMessageQueue()->push(report);
    }
}

void ATVMod::mixImageAndText(cv::Mat& image)
{
    int fontFace = cv::FONT_HERSHEY_PLAIN;
    double fontScale = image.rows / 100.0;
    int thickness = image.cols / 160;
    int baseline=0;

    cv::Size textSize = cv::getTextSize(m_overlayText, fontFace, fontScale, thickness, &baseline);
    baseline += thickness;

    // position the text in the top left corner
    cv::Point textOrg(6, textSize.height+10);
    // then put the text itself
    cv::putText(image, m_overlayText, textOrg, fontFace, fontScale, cv::Scalar::all(255*m_running.m_uniformLevel), thickness, CV_AA);
}

