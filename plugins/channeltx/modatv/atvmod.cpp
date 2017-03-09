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

#include "opencv2/imgproc/imgproc.hpp"

#include "dsp/upchannelizer.h"
#include "atvmod.h"

MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureATVMod, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureImageFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureVideoFileName, Message)

const float ATVMod::m_blackLevel = 0.3f;
const float ATVMod::m_spanLevel = 0.7f;
const int ATVMod::m_levelNbSamples = 10000; // every 10ms
const int ATVMod::m_nbBars = 6;

ATVMod::ATVMod() :
	m_modPhasor(0.0f),
    m_evenImage(true),
    m_tvSampleRate(1000000),
    m_settingsMutex(QMutex::Recursive),
    m_horizontalCount(0),
    m_lineCount(0),
	m_imageOK(false)
{
    setObjectName("ATVMod");

    m_config.m_outputSampleRate = 1000000;
    m_config.m_inputFrequencyOffset = 0;
    m_config.m_rfBandwidth = 1000000;
    m_config.m_atvModInput = ATVModInputHBars;
    m_config.m_atvStd = ATVStdPAL625;

    applyStandard();

    m_interpolatorDistanceRemain = 0.0f;
    m_interpolatorDistance = 1.0f;

    apply(true);

    m_movingAverage.resize(16, 0);
}

ATVMod::~ATVMod()
{
}

void ATVMod::configure(MessageQueue* messageQueue,
            Real rfBandwidth,
            ATVStd atvStd,
            ATVModInput atvModInput,
            Real uniformLevel,
			ATVModulation atvModulation,
            bool channelMute)
{
    Message* cmd = MsgConfigureATVMod::create(rfBandwidth, atvStd, atvModInput, uniformLevel, atvModulation);
    messageQueue->push(cmd);
}

void ATVMod::pullAudio(int nbSamples)
{
}

void ATVMod::pull(Sample& sample)
{
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

    switch (m_running.m_atvModulation)
    {
    case ATVModulationFM: // FM half bandwidth deviation
    	m_modPhasor += (t - 0.5f) * M_PI;
    	if (m_modPhasor > 2.0f * M_PI) m_modPhasor -= 2.0f * M_PI; // limit growth
    	if (m_modPhasor < 2.0f * M_PI) m_modPhasor += 2.0f * M_PI; // limit growth
    	m_modSample.real(cos(m_modPhasor) * 29204.0f); // -1 dB
    	m_modSample.imag(sin(m_modPhasor) * 29204.0f);
    	break;
    case ATVModulationAM: // AM 90%
    default:
        m_modSample.real((t*1.8f + 0.1f) * 16384.0f); // modulate and scale zero frequency carrier
        m_modSample.imag(0.0f);
    }
}

void ATVMod::pullVideo(Real& sample)
{
    if ((m_lineCount < 5 + m_nbBlankLines) || (m_lineCount > 621) || ((m_lineCount > 309) && (m_lineCount < 317 + m_nbBlankLines)))
    {
        pullVSyncLine(sample);
    }
    else
    {
        pullImageLine(sample);
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
        else
        {
            m_lineCount = 0;
            m_evenImage = !m_evenImage;
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
        m_config.m_atvModInput = cfg.getATVModInput();
        m_config.m_atvStd = cfg.getATVStd();
        m_config.m_uniformLevel = cfg.getUniformLevel();
        m_config.m_atvModulation = cfg.getModulation();

        apply();

        qDebug() << "ATVMod::handleMessage: MsgConfigureATVMod:"
                << " m_rfBandwidth: " << m_config.m_rfBandwidth
                << " m_atvStd: " << (int) m_config.m_atvStd
                << " m_atvModInput: " << (int) m_config.m_atvModInput
                << " m_uniformLevel: " << m_config.m_uniformLevel
				<< " m_atvModulation: " << (int) m_config.m_atvModulation;

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
    else
    {
        return false;
    }
}

void ATVMod::apply(bool force)
{
    if ((m_config.m_outputSampleRate != m_running.m_outputSampleRate) ||
        (m_config.m_atvStd != m_running.m_atvStd) ||
        (m_config.m_rfBandwidth != m_running.m_rfBandwidth) || force)
    {
        int rateUnits = getSampleRateUnits(m_config.m_atvStd);
        m_tvSampleRate = (m_config.m_outputSampleRate / rateUnits) * rateUnits; // make sure working sample rate is a multiple of rate units

        m_settingsMutex.lock();

        if (m_tvSampleRate > 0)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorDistance = (Real) m_tvSampleRate / (Real) m_config.m_outputSampleRate;
            m_interpolator.create(48, m_tvSampleRate, m_config.m_rfBandwidth / 2.2, 3.0);
        }
        else
        {
            m_tvSampleRate = m_config.m_outputSampleRate;
        }

        applyStandard(); // set all timings
        m_settingsMutex.unlock();
    }

    if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
        (m_config.m_outputSampleRate != m_running.m_outputSampleRate))
    {
        m_settingsMutex.lock();
        m_carrierNco.setFreq(m_config.m_inputFrequencyOffset, m_config.m_outputSampleRate);
        m_settingsMutex.unlock();
    }

    m_running.m_outputSampleRate = m_config.m_outputSampleRate;
    m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
    m_running.m_rfBandwidth = m_config.m_rfBandwidth;
    m_running.m_atvModInput = m_config.m_atvModInput;
    m_running.m_atvStd = m_config.m_atvStd;
    m_running.m_uniformLevel = m_config.m_uniformLevel;
    m_running.m_atvModulation = m_config.m_atvModulation;
}

int ATVMod::getSampleRateUnits(ATVStd std)
{
    switch(std)
    {
    case ATVStdPAL525:
    	return 1008000;
    	break;
    case ATVStdPAL625:
    default:
        return 1000000; // Exact MS/s - us
    }
}

void ATVMod::applyStandard()
{
    int rateUnits = getSampleRateUnits(m_config.m_atvStd);
    m_pointsPerTU = m_tvSampleRate / rateUnits; // TV sample rate is already set at a multiple of rate units

    switch(m_config.m_atvStd)
    {
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
        m_nbSyncLinesH     = 5;
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
        m_nbSyncLinesH     = 5;
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
}

void ATVMod::openImage(const QString& fileName)
{
    m_imageOriginal = cv::imread(qPrintable(fileName), CV_LOAD_IMAGE_GRAYSCALE);
	m_imageOK = m_imageOriginal.data != 0;

	if (m_imageOK)
	{
	    resizeImage();
	}
}

void ATVMod::openVideo(const QString& fileName)
{
    m_videoOK = m_video.open(qPrintable(fileName));

    if (m_videoOK)
    {
        m_videoFPS = m_video.get(CV_CAP_PROP_FPS);
        m_videoWidth = (int) m_video.get(CV_CAP_PROP_FRAME_WIDTH);
        m_videoHeight = (int) m_video.get(CV_CAP_PROP_FRAME_HEIGHT);
        qDebug("ATVMod::openVideo(: FPS: %f size: %d x %d", m_videoFPS, m_videoWidth, m_videoHeight);
    }
}

void ATVMod::resizeImage()
{
    float fy = (m_nbImageLines - 2*m_nbBlankLines) / (float) m_imageOriginal.rows;
    float fx = m_pointsPerImgLine / (float) m_imageOriginal.cols;
    cv::resize(m_imageOriginal, m_image, cv::Size(), fx, fy);
    qDebug("ATVMod::resizeImage: %d x %d -> %d x %d", m_imageOriginal.cols, m_imageOriginal.rows, m_image.cols, m_image.rows);
}
