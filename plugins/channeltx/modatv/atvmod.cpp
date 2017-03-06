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

#include "dsp/upchannelizer.h"
#include "atvmod.h"

MESSAGE_CLASS_DEFINITION(ATVMod::MsgConfigureATVMod, Message)

const float ATVMod::m_blackLevel = 0.3f;
const float ATVMod::m_spanLevel = 0.7f;
const int ATVMod::m_levelNbSamples = 10000; // every 10ms

ATVMod::ATVMod() :
    m_evenImage(true),
    m_tvSampleRate(1000000),
    m_settingsMutex(QMutex::Recursive),
    m_horizontalCount(0),
    m_lineCount(0)
{
    setObjectName("ATVMod");

    m_config.m_outputSampleRate = 1000000;
    m_config.m_inputFrequencyOffset = 0;
    m_config.m_rfBandwidth = 1000000;
    m_config.m_atvModInput = ATVModInputBarChart;
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
            bool channelMute)
{
    Message* cmd = MsgConfigureATVMod::create(rfBandwidth, atvStd, atvModInput, uniformLevel);
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

    // TODO For now do AM 95%
    m_modSample.real((t*0.95 + 1.0f) * 16384.0f); // modulate and scale zero frequency carrier
    m_modSample.imag(0.0f);
}

void ATVMod::pullVideo(Real& sample)
{
    if ((m_lineCount < 21) || (m_lineCount > 621) || ((m_lineCount > 309) && (m_lineCount < 335)))
    {
        pullVSyncLine(m_horizontalCount - (m_pointsPerSync + m_pointsPerBP), sample);
    }
    else
    {
        pullImageLine(sample);
    }

    if (m_horizontalCount < m_nbHorizPoints)
    {
        m_horizontalCount++;
    }
    else
    {
        if (m_lineCount < m_nbLines)
        {
            m_lineCount++;
        }
        else
        {
            m_lineCount = 0;
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

        apply();

        qDebug() << "ATVMod::handleMessage: MsgConfigureATVMod:"
                << " m_rfBandwidth: " << m_config.m_rfBandwidth
                << " m_atvStd: " << (int) m_config.m_atvStd
                << " m_atvModInput: " << (int) m_config.m_atvModInput
                << " m_uniformLevel: " << m_config.m_uniformLevel;

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
        int rateMultiple = getSampleRateMultiple(m_config.m_atvStd);
        m_tvSampleRate = (m_config.m_outputSampleRate / rateMultiple) * rateMultiple;

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
}

int ATVMod::getSampleRateMultiple(ATVStd std)
{
    switch(std)
    {
    case ATVStdPAL625:
    default:
        return 1000000;
    }
}

void ATVMod::applyStandard()
{
    int rateMultiple = getSampleRateMultiple(m_config.m_atvStd);
    m_pointsPerTU = m_tvSampleRate / rateMultiple;

    switch(m_config.m_atvStd)
    {
    case ATVStdPAL625:
    default:
        m_pointsPerSync = 5 * m_pointsPerTU;         // would be 4.7 us actually
        m_pointsPerBP = 5 * m_pointsPerTU;           // would be 4.7 us actually
        m_pointsPerFP = 2 * m_pointsPerTU;           // would be 1.5 us actually
        m_pointsPerFSync = 3 * m_pointsPerTU;        // would be 2.3 us actually
        m_pointsPerLine = (64 - 12) * m_pointsPerTU; // what is left in a 64 us line
        m_pointsPerBar = 10 * m_pointsPerTU;         // set a bar length to 10 us (~5 bars per line)
        m_nbLines = 625;
        m_interlaced = true;
        m_nbHorizPoints = 64 * m_pointsPerTU;
    }
}
