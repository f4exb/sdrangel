///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "sdrplaygui.h"
#include "sdrplayinput.h"

#include <device/devicesourceapi.h>

#include "sdrplaythread.h"

MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgConfigureSDRPlay, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgReportSDRPlay, Message)

SDRPlayInput::SDRPlayInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_sdrPlayThread(0),
    m_deviceDescription("SDRPlay"),
    m_samplesPerPacket(4096),
    m_mirStreamRunning(false)
{
}

SDRPlayInput::~SDRPlayInput()
{
    stop();
}

bool SDRPlayInput::init(const Message& cmd)
{
    return false;
}

bool SDRPlayInput::start(int device)
{
    mir_sdr_ErrT r;
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("SDRPlayInput::start: could not allocate SampleFifo");
        return false;
    }

    if((m_sdrPlayThread = new SDRPlayThread(&m_sampleFifo)) == 0)
    {
        qFatal("SDRPlayInput::start: failed to create thread");
        return false;
    }

    int agcSetPoint = m_settings.m_gainRedctionIndex;
    double sampleRateMHz = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex) / 1e3;
    double frequencyMHz = m_settings.m_centerFrequency / 1e6;
    int infoOverallGr;

    r = mir_sdr_DCoffsetIQimbalanceControl(1, 0);
    if (r != mir_sdr_Success)
    {
        qCritical("SDRPlayInput::start: mir_sdr_DCoffsetIQimbalanceControl failed with code %d", (int) r);
    }

    r = mir_sdr_AgcControl(mir_sdr_AGC_DISABLE, agcSetPoint, 0, 0, 0, 0, 1);
    if (r != mir_sdr_Success)
    {
        qCritical("SDRPlayInput::start: mir_sdr_AgcControl failed with code %d", (int) r);
    }

    qDebug("SDRPlayInput::start: sampleRateMHz: %lf frequencyMHz: %lf", sampleRateMHz, frequencyMHz);

    r = mir_sdr_StreamInit(
            &agcSetPoint,
            sampleRateMHz,
            frequencyMHz,
            mir_sdr_BW_1_536,
            mir_sdr_IF_Zero,
            1, /* LNA */
            &infoOverallGr,
            0, /* use internal gr tables according to band */
            &m_samplesPerPacket,
            m_sdrPlayThread->streamCallback,
            callbackGC,
            0);

    if (r != mir_sdr_Success)
    {
        qCritical("SDRPlayInput::start: mir_sdr_StreamInit failed with code %d", (int) r);
    }
    else
    {
        qDebug("SDRPlayInput::start: Mir stream started: samplesPerPacket: %d", m_samplesPerPacket);
        m_mirStreamRunning = true;
        m_sdrPlayThread->startWork();
    }
}

void SDRPlayInput::stop()
{
    mir_sdr_ErrT r;
    QMutexLocker mutexLocker(&m_mutex);

    r = mir_sdr_StreamUninit();

    if (r != mir_sdr_Success)
    {
        qCritical("SDRPlayInput::stop: stream uninit failed with code %d", (int) r);
    }

    m_mirStreamRunning = false;

    if(m_sdrPlayThread != 0)
    {
        m_sdrPlayThread->stopWork();
        delete m_sdrPlayThread;
        m_sdrPlayThread = 0;
    }
}

const QString& SDRPlayInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SDRPlayInput::getSampleRate() const
{
    int rate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);
    return (rate * 1000) / (1<<m_settings.m_log2Decim);
}

quint64 SDRPlayInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

bool SDRPlayInput::handleMessage(const Message& message)
{
    if (MsgConfigureSDRPlay::match(message))
    {
        MsgConfigureSDRPlay& conf = (MsgConfigureSDRPlay&) message;
        qDebug() << "SDRPlayInput::handleMessage: MsgConfigureSDRPlay";

        bool success = applySettings(conf.getSettings(), false);

        if (!success)
        {
            qDebug("SDRPlayInput::handleMessage: config error");
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool SDRPlayInput::applySettings(const SDRPlaySettings& settings, bool force)
{
    bool forwardChange = false;
    QMutexLocker mutexLocker(&m_mutex);

    if (m_settings.m_dcBlock != settings.m_dcBlock)
    {
        m_settings.m_dcBlock = settings.m_dcBlock;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    if (m_settings.m_iqCorrection != settings.m_iqCorrection)
    {
        m_settings.m_iqCorrection = settings.m_iqCorrection;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        m_settings.m_log2Decim = settings.m_log2Decim;
        forwardChange = true;

        if (m_sdrPlayThread != 0)
        {
            m_sdrPlayThread->setLog2Decimation(m_settings.m_log2Decim);
            qDebug() << "SDRPlayInput: set decimation to " << (1<<m_settings.m_log2Decim);
        }
    }

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        m_settings.m_fcPos = settings.m_fcPos;

        if (m_sdrPlayThread != 0)
        {
            m_sdrPlayThread->setFcPos((int) m_settings.m_fcPos);
            qDebug() << "SDRPlayInput: set fc pos (enum) to " << (int) m_settings.m_fcPos;
        }
    }

    if ((m_settings.m_devSampleRateIndex != settings.m_devSampleRateIndex) || force)
    {
        m_settings.m_devSampleRateIndex = settings.m_devSampleRateIndex;
        forwardChange = true;

        if (m_mirStreamRunning)
        {
            reinitMirSDR(mir_sdr_CHANGE_FS_FREQ);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        forwardChange = true;

        if (m_mirStreamRunning)
        {
            reinitMirSDR(mir_sdr_CHANGE_RF_FREQ);
        }
    }

    if ((m_settings.m_frequencyBandIndex != settings.m_frequencyBandIndex) || force)
    {
        m_settings.m_frequencyBandIndex = settings.m_frequencyBandIndex;
        // change of frequency is done already
    }

    if ((m_settings.m_bandwidthIndex != settings.m_bandwidthIndex) || force)
    {
        m_settings.m_bandwidthIndex = settings.m_bandwidthIndex;

        if (m_mirStreamRunning)
        {
            reinitMirSDR(mir_sdr_CHANGE_BW_TYPE);
        }
    }

    // TODO: change IF mode
    // TODO: change LO mode

    if ((m_settings.m_gainRedctionIndex != settings.m_gainRedctionIndex) || force)
    {
        if (m_settings.m_bandwidthIndex < 4)
        {
            m_settings.m_gainRedctionIndex = settings.m_gainRedctionIndex;

            if (m_mirStreamRunning)
            {
                reinitMirSDR(mir_sdr_CHANGE_GR);
            }
        }
        else
        {
            if (settings.m_gainRedctionIndex > 85)
            {
                if (m_settings.m_gainRedctionIndex < 85)
                {
                    m_settings.m_gainRedctionIndex = 85;

                    if (m_mirStreamRunning)
                    {
                        reinitMirSDR(mir_sdr_CHANGE_GR);
                    }
                }
            }
            else
            {
                m_settings.m_gainRedctionIndex = settings.m_gainRedctionIndex;

                if (m_mirStreamRunning)
                {
                    reinitMirSDR(mir_sdr_CHANGE_GR);
                }
            }
        }
    }

    if ((m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force)
    {
        m_settings.m_LOppmTenths = settings.m_LOppmTenths;

        mir_sdr_ErrT r = mir_sdr_SetPpm(m_settings.m_LOppmTenths / 10.0);

        if (r != mir_sdr_Success)
        {
            qDebug("SDRPlayInput::applySettings: mir_sdr_SetPpm failed with code %d", (int) r);
        }
    }

    if ((m_settings.m_mirDcCorrIndex != settings.m_mirDcCorrIndex) || force)
    {
        m_settings.m_mirDcCorrIndex = settings.m_mirDcCorrIndex;

        if (m_mirStreamRunning)
        {
            mir_sdr_ErrT r = mir_sdr_SetDcMode(m_settings.m_mirDcCorrIndex, 0);

            if (r != mir_sdr_Success)
            {
                qDebug("SDRPlayInput::applySettings: mir_sdr_SetDcMode failed with code %d", (int) r);
            }
        }
    }

    if ((m_settings.m_mirDcCorrTrackTimeIndex != settings.m_mirDcCorrTrackTimeIndex) || force)
    {
        m_settings.m_mirDcCorrTrackTimeIndex = settings.m_mirDcCorrTrackTimeIndex;

        if (m_mirStreamRunning)
        {
            mir_sdr_ErrT r = mir_sdr_SetDcTrackTime(m_settings.m_mirDcCorrTrackTimeIndex);

            if (r != mir_sdr_Success)
            {
                qDebug("SDRPlayInput::applySettings: mir_sdr_SetDcTrackTime failed with code %d", (int) r);
            }
        }
    }

    if (forwardChange)
    {
        int sampleRate = getSampleRate();
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
    }

    return true;
}

void SDRPlayInput::reinitMirSDR(mir_sdr_ReasonForReinitT reasonForReinit)
{
    int grdB = m_settings.m_gainRedctionIndex;
    int rate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);
    int gRdBsystem;

    mir_sdr_ErrT r = mir_sdr_Reinit(
            &grdB,
            rate / 1e3,
            m_settings.m_centerFrequency / 1e6,
            (mir_sdr_Bw_MHzT) m_settings.m_bandwidthIndex,
            mir_sdr_IF_Zero,
            mir_sdr_LO_Auto,
            1, // LNA
            &gRdBsystem,
            0, // use mir_sdr_SetGr() to set initial gain reduction
            &m_samplesPerPacket,
            reasonForReinit);

    if (r != mir_sdr_Success)
    {
        qCritical("SDRPlayInput::reinitMirSDR (%d): MirSDR stream reinit failed with code %d", (int) reasonForReinit, (int) r);
        m_mirStreamRunning = false;
    }
    else
    {
        qDebug("SDRPlayInput::reinitMirSDR (%d): MirSDR stream restarted: samplesPerPacket: %d", (int) reasonForReinit, m_samplesPerPacket);
    }
}

void SDRPlayInput::callbackGC(unsigned int gRdB, unsigned int lnaGRdB, void *cbContext)
{
    return;
}
