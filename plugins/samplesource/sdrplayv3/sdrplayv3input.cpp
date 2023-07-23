///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include <errno.h>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSDRPlayV3Report.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "sdrplayv3input.h"

#include <device/deviceapi.h>

#include "sdrplayv3thread.h"

MESSAGE_CLASS_DEFINITION(SDRPlayV3Input::MsgConfigureSDRPlayV3, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayV3Input::MsgStartStop, Message)

SDRPlayV3Input::SDRPlayV3Input(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_dev(nullptr),
    m_devParams(nullptr),
    m_sdrPlayThread(nullptr),
    m_deviceDescription("SDRPlayV3"),
    m_devNumber(0),
    m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();
    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SDRPlayV3Input::networkManagerFinished
    );
}

SDRPlayV3Input::~SDRPlayV3Input()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SDRPlayV3Input::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void SDRPlayV3Input::destroy()
{
    delete this;
}

bool SDRPlayV3Input::openDevice()
{
    qDebug() << "SDRPlayV3Input::openDevice";
    m_devNumber = m_deviceAPI->getSamplingDeviceSequence();

    if (m_dev != 0)
    {
        closeDevice();
    }

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("SDRPlayV3Input::openDevice: could not allocate SampleFifo");
        return false;
    }

    sdrplay_api_LockDeviceApi();

    sdrplay_api_ErrT err;
    unsigned int count;
    if ((err = sdrplay_api_GetDevices(m_devs, &count, sizeof(m_devs) / sizeof(sdrplay_api_DeviceT))) == sdrplay_api_Success)
    {
        m_dev = &m_devs[m_devNumber];
        m_dev->tuner = sdrplay_api_Tuner_A;
        m_dev->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
        if ((err = sdrplay_api_SelectDevice(m_dev)) == sdrplay_api_Success)
        {
            sdrplay_api_UnlockDeviceApi();
            if ((err = sdrplay_api_GetDeviceParams(m_dev->dev, &m_devParams)) == sdrplay_api_Success)
            {
                qDebug() << "SDRPlayV3Input::openDevice: opened successfully";
            }
            else
            {
                qCritical() << "SDRPlayV3Input::openDevice: could not get device parameters: " << sdrplay_api_GetErrorString(err);
                return false;
            }
        }
        else
        {
            qCritical() << "SDRPlayV3Input::openDevice: could not select device: " << sdrplay_api_GetErrorString(err);
            sdrplay_api_UnlockDeviceApi();
            return false;
        }
    }
    else
    {
        qCritical() << "SDRPlayV3Input::openDevice: could not get devices: " << sdrplay_api_GetErrorString(err);
        sdrplay_api_UnlockDeviceApi();
        return false;
    }

    sdrplay_api_UnlockDeviceApi();

    return true;
}

bool SDRPlayV3Input::start()
{
    qDebug() << "SDRPlayV3Input::start";

    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

    m_sdrPlayThread = new SDRPlayV3Thread(m_dev, &m_sampleFifo);
    m_sdrPlayThread->setLog2Decimation(m_settings.m_log2Decim);
    m_sdrPlayThread->setFcPos((int) m_settings.m_fcPos);
    m_sdrPlayThread->startWork();

    m_running = m_sdrPlayThread->isRunning();
    applySettings(m_settings, QList<QString>(), true, true);

    return true;
}

void SDRPlayV3Input::closeDevice()
{
    qDebug() << "SDRPlayV3Input::closeDevice";
    if (m_dev != 0)
    {
        sdrplay_api_ReleaseDevice(m_dev);
        m_dev = 0;
    }
    m_deviceDescription.clear();
}

void SDRPlayV3Input::init()
{
    applySettings(m_settings, QList<QString>(), true, true);
}

void SDRPlayV3Input::stop()
{
    qDebug() << "SDRPlayV3Input::stop";
    QMutexLocker mutexLocker(&m_mutex);

    if(m_sdrPlayThread)
    {
        m_sdrPlayThread->stopWork();
        delete m_sdrPlayThread;
        m_sdrPlayThread = nullptr;
    }

    m_running = false;
}

QByteArray SDRPlayV3Input::serialize() const
{
    return m_settings.serialize();
}

bool SDRPlayV3Input::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSDRPlayV3* message = MsgConfigureSDRPlayV3::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRPlayV3* messageToGUI = MsgConfigureSDRPlayV3::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& SDRPlayV3Input::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SDRPlayV3Input::getSampleRate() const
{
    uint32_t fsHz = m_settings.m_devSampleRate;
    sdrplay_api_Bw_MHzT bwType = SDRPlayV3Bandwidths::getBandwidthEnum(m_settings.m_bandwidthIndex);
    sdrplay_api_If_kHzT ifType = SDRPlayV3IF::getIFEnum(m_settings.m_ifFrequencyIndex);

    if(
        ((fsHz == 8192000) && (bwType == sdrplay_api_BW_1_536) && (ifType == sdrplay_api_IF_2_048)) ||
        ((fsHz == 8000000) && (bwType == sdrplay_api_BW_1_536) && (ifType == sdrplay_api_IF_2_048)) ||
        ((fsHz == 8000000) && (bwType == sdrplay_api_BW_5_000) && (ifType == sdrplay_api_IF_2_048)) ||
        ((fsHz == 2000000) && (bwType <= sdrplay_api_BW_0_300) && (ifType == sdrplay_api_IF_0_450))) {
        fsHz /= 4;
    } else if ((fsHz == 2000000) && (bwType == sdrplay_api_BW_0_600) && (ifType == sdrplay_api_IF_0_450)) {
        fsHz /= 2;
    }else if ((fsHz == 6000000) && (bwType <= sdrplay_api_BW_1_536) && (ifType == sdrplay_api_IF_1_620)) {
        fsHz /= 3;
    }

    return fsHz / (1 << m_settings.m_log2Decim);
}

quint64 SDRPlayV3Input::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SDRPlayV3Input::setCenterFrequency(qint64 centerFrequency)
{
    SDRPlayV3Settings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureSDRPlayV3* message = MsgConfigureSDRPlayV3::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRPlayV3* messageToGUI = MsgConfigureSDRPlayV3::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool SDRPlayV3Input::handleMessage(const Message& message)
{
    if (MsgConfigureSDRPlayV3::match(message))
    {
        MsgConfigureSDRPlayV3& conf = (MsgConfigureSDRPlayV3&) message;
        qDebug() << "SDRPlayV3Input::handleMessage: MsgConfigureSDRPlayV3";

        if (!applySettings( conf.getSettings(), conf.getSettingsKeys(), false, conf.getForce())) {
            qDebug("SDRPlayV3Input::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SDRPlayV3Input::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool SDRPlayV3Input::applySettings(const SDRPlayV3Settings& settings, const QList<QString>& settingsKeys, bool forwardChange, bool force)
{
    qDebug() << "SDRPlayV3Input::applySettings: forwardChange: " << forwardChange << " force: " << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    sdrplay_api_ErrT err;

    if (settingsKeys.contains("tuner") || force)
    {
        if (m_running)
        {
            if (getDeviceId() == SDRPLAY_RSPduo_ID)
            {
                if (((m_dev->tuner == sdrplay_api_Tuner_A) && (settings.m_tuner == 1))
                    || ((m_dev->tuner == sdrplay_api_Tuner_B) && (settings.m_tuner == 0)))
                {
                    if ((err = sdrplay_api_SwapRspDuoActiveTuner (m_dev->dev, &m_dev->tuner, settings.m_antenna == 1 ? sdrplay_api_RspDuo_AMPORT_1 : sdrplay_api_RspDuo_AMPORT_2)) != sdrplay_api_Success)
                        qCritical() << "SDRPlayV3Input::applySettings: could not swap tuners: " << sdrplay_api_GetErrorString(err);
                }
            }
        }
    }

    if (settingsKeys.contains("dcBlock") || force)
    {
        if (m_running)
        {
            if (m_dev->tuner == sdrplay_api_Tuner_A)
                m_devParams->rxChannelA->ctrlParams.dcOffset.DCenable = settings.m_dcBlock ? 1 : 0;
            else
                m_devParams->rxChannelB->ctrlParams.dcOffset.DCenable = settings.m_dcBlock ? 1 : 0;
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Ctrl_DCoffsetIQimbalance, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set DC offset enable: " << sdrplay_api_GetErrorString(err);
        }
     }

    if (settingsKeys.contains("iqCorrection") || force)
    {
        if (m_running)
        {
            if (m_dev->tuner == sdrplay_api_Tuner_A)
                m_devParams->rxChannelA->ctrlParams.dcOffset.IQenable = settings.m_iqCorrection ? 1 : 0;
            else
                m_devParams->rxChannelB->ctrlParams.dcOffset.IQenable = settings.m_iqCorrection ? 1 : 0;
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Ctrl_DCoffsetIQimbalance, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set IQ offset enable: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("ifAGC") || force)
    {
        if (m_running)
        {
            if (m_dev->tuner == sdrplay_api_Tuner_A)
                m_devParams->rxChannelA->ctrlParams.agc.enable = settings.m_ifAGC ? sdrplay_api_AGC_CTRL_EN : sdrplay_api_AGC_DISABLE;
            else
                m_devParams->rxChannelB->ctrlParams.agc.enable = settings.m_ifAGC ? sdrplay_api_AGC_CTRL_EN : sdrplay_api_AGC_DISABLE;
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set AGC enable: " << sdrplay_api_GetErrorString(err);
        }
    }

    // Need to reset IF gain manual setting after AGC is disabled
    if (settingsKeys.contains("lnaIndex")
        || settingsKeys.contains("ifGain")
        || settingsKeys.contains("ifAGC") || force)
    {
        if (m_running)
        {
            if (m_dev->tuner == sdrplay_api_Tuner_A)
            {
                m_devParams->rxChannelA->tunerParams.gain.gRdB = -settings.m_ifGain;
                m_devParams->rxChannelA->tunerParams.gain.LNAstate = settings.m_lnaIndex;
                m_devParams->rxChannelA->tunerParams.gain.minGr = sdrplay_api_EXTENDED_MIN_GR;
            }
            else
            {
                m_devParams->rxChannelB->tunerParams.gain.gRdB = -settings.m_ifGain;
                m_devParams->rxChannelB->tunerParams.gain.LNAstate = settings.m_lnaIndex;
                m_devParams->rxChannelB->tunerParams.gain.minGr = sdrplay_api_EXTENDED_MIN_GR;
            }
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set gain: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("devSampleRate") || force)
    {
        if (m_running)
        {
            int sampleRate = settings.m_devSampleRate;
            m_devParams->devParams->fsFreq.fsHz = (double)sampleRate;
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Dev_Fs, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set sample rate to " << sampleRate << " : " << sdrplay_api_GetErrorString(err);
            else
                qDebug() <<  "SDRPlayV3Input::applySettings: sample rate set to " << sampleRate;
            forwardChange = true;
        }
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        if (m_sdrPlayThread)
        {
            m_sdrPlayThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "SDRPlayV3Input::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (settingsKeys.contains("fcPos") || force)
    {
        if (m_sdrPlayThread)
        {
            m_sdrPlayThread->setFcPos((int) settings.m_fcPos);
            qDebug() << "SDRPlayV3Input:applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_sdrPlayThread) {
            m_sdrPlayThread->setIQOrder(settings.m_iqOrder);
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("LOppmTenths")
        || settingsKeys.contains("fcPos")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency") || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                settings.m_transverterDeltaFrequency,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                settings.m_transverterMode);

        forwardChange = true;

        if (m_running)
        {
            if (setDeviceCenterFrequency(deviceCenterFrequency)) {
                qDebug() << "SDRPlayV3Input::applySettings: center freq: " << settings.m_centerFrequency << " Hz";
            }
        }
    }

    if (settingsKeys.contains("bandwidthIndex") || force)
    {
        if (m_running)
        {
            if (m_dev->tuner == sdrplay_api_Tuner_A)
                m_devParams->rxChannelA->tunerParams.bwType = SDRPlayV3Bandwidths::getBandwidthEnum(settings.m_bandwidthIndex);
            else
                m_devParams->rxChannelB->tunerParams.bwType = SDRPlayV3Bandwidths::getBandwidthEnum(settings.m_bandwidthIndex);
            m_sdrPlayThread->resetRfChanged();
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set bandwidth: " << sdrplay_api_GetErrorString(err);
            if (!m_sdrPlayThread->waitForRfChanged())
                qCritical() << "SDRPlayV3Input::applySettings: could not set bandwidth: Rf update timed out";
        }
    }

    if (settingsKeys.contains("ifFrequencyIndex") || force)
    {
        if (m_running)
        {
            if (m_dev->tuner == sdrplay_api_Tuner_A)
                m_devParams->rxChannelA->tunerParams.ifType = SDRPlayV3IF::getIFEnum(settings.m_ifFrequencyIndex);
            else
                m_devParams->rxChannelB->tunerParams.ifType = SDRPlayV3IF::getIFEnum(settings.m_ifFrequencyIndex);
            m_sdrPlayThread->resetRfChanged();
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Tuner_IfType, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set IF frequency: " << sdrplay_api_GetErrorString(err);
            if (!m_sdrPlayThread->waitForRfChanged())
                qCritical() << "SDRPlayV3Input::applySettings: could not set IF frequency: Rf update timed out";
        }
    }

    if (settingsKeys.contains("biasTee") || force)
    {
        if (m_running)
        {
            sdrplay_api_ReasonForUpdateT update = sdrplay_api_Update_None;
            sdrplay_api_ReasonForUpdateExtension1T updateExt = sdrplay_api_Update_Ext1_None;
            switch (getDeviceId())
            {
            case SDRPLAY_RSP1A_ID:
                m_devParams->rxChannelA->rsp1aTunerParams.biasTEnable = settings.m_biasTee;
                update = sdrplay_api_Update_Rsp1a_BiasTControl;
                break;
            case SDRPLAY_RSP2_ID:
                m_devParams->rxChannelA->rsp2TunerParams.biasTEnable = settings.m_biasTee;
                update = sdrplay_api_Update_Rsp2_BiasTControl;
                break;
            case SDRPLAY_RSPduo_ID:
                // Only channel B has bias tee
                if (m_dev->tuner == sdrplay_api_Tuner_B)
                {
                    m_devParams->rxChannelB->rspDuoTunerParams.biasTEnable = settings.m_biasTee;
                    update = sdrplay_api_Update_RspDuo_BiasTControl;
                }
                break;
            case SDRPLAY_RSPdx_ID:
                m_devParams->devParams->rspDxParams.biasTEnable = settings.m_biasTee;
                updateExt = sdrplay_api_Update_RspDx_BiasTControl;
                break;
            default:
                // SDRPLAY_RSP1_ID doesn't have a bias tee
                break;
            }
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, update, updateExt)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set state of bias tee: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("amNotch") || force)
    {
        if (m_running)
        {
            sdrplay_api_ReasonForUpdateT update = sdrplay_api_Update_None;
            sdrplay_api_ReasonForUpdateExtension1T updateExt = sdrplay_api_Update_Ext1_None;
            switch (getDeviceId())
            {
            case SDRPLAY_RSPduo_ID:
                if (m_dev->tuner == sdrplay_api_Tuner_A)
                {
                    m_devParams->rxChannelA->rspDuoTunerParams.tuner1AmNotchEnable = settings.m_amNotch;
                    update = sdrplay_api_Update_RspDuo_Tuner1AmNotchControl;
                }
                break;
            default:
                // Other devices don't have AM notch filter
                break;
            }
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, update, updateExt)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set state of AM notch filter: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("fmNotch") || force)
    {
        if (m_running)
        {
            sdrplay_api_ReasonForUpdateT update = sdrplay_api_Update_None;
            sdrplay_api_ReasonForUpdateExtension1T updateExt = sdrplay_api_Update_Ext1_None;
            switch (getDeviceId())
            {
            case SDRPLAY_RSP1A_ID:
                m_devParams->devParams->rsp1aParams.rfNotchEnable = settings.m_fmNotch;
                update = sdrplay_api_Update_Rsp1a_RfNotchControl;
                break;
            case SDRPLAY_RSP2_ID:
                m_devParams->rxChannelA->rsp2TunerParams.rfNotchEnable = settings.m_fmNotch;
                update = sdrplay_api_Update_Rsp2_RfNotchControl;
                break;
            case SDRPLAY_RSPduo_ID:
                if (m_dev->tuner == sdrplay_api_Tuner_A)
                    m_devParams->rxChannelA->rspDuoTunerParams.rfNotchEnable = settings.m_fmNotch;
                else
                    m_devParams->rxChannelB->rspDuoTunerParams.rfNotchEnable = settings.m_fmNotch;
                update = sdrplay_api_Update_RspDuo_RfNotchControl;
                break;
            case SDRPLAY_RSPdx_ID:
                m_devParams->devParams->rspDxParams.rfNotchEnable = settings.m_fmNotch;
                updateExt = sdrplay_api_Update_RspDx_RfNotchControl;
                break;
            default:
                // SDRPLAY_RSP1_ID doesn't have notch filter
                break;
            }
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, update, updateExt)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set state of MW/FM notch filter: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("dabNotch") || force)
    {
        if (m_running)
        {
            sdrplay_api_ReasonForUpdateT update = sdrplay_api_Update_None;
            sdrplay_api_ReasonForUpdateExtension1T updateExt = sdrplay_api_Update_Ext1_None;
            switch (getDeviceId())
            {
            case SDRPLAY_RSP1A_ID:
                m_devParams->devParams->rsp1aParams.rfDabNotchEnable = settings.m_dabNotch;
                update = sdrplay_api_Update_Rsp1a_RfDabNotchControl;
                break;
            case SDRPLAY_RSPduo_ID:
                if (m_dev->tuner == sdrplay_api_Tuner_A)
                    m_devParams->rxChannelA->rspDuoTunerParams.rfDabNotchEnable = settings.m_dabNotch;
                else
                    m_devParams->rxChannelB->rspDuoTunerParams.rfDabNotchEnable = settings.m_dabNotch;
                update = sdrplay_api_Update_RspDuo_RfDabNotchControl;
                break;
            case SDRPLAY_RSPdx_ID:
                m_devParams->devParams->rspDxParams.rfDabNotchEnable = settings.m_dabNotch;
                updateExt = sdrplay_api_Update_RspDx_RfDabNotchControl;
                break;
            default:
                // SDRPLAY_RSP1_ID and SDRPLAY_RSP2_ID don't have DAB filter
                break;
            }
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, update, updateExt)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set state of DAB notch filter: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("antenna") || force)
    {
        if (m_running)
        {
            sdrplay_api_ReasonForUpdateT update = sdrplay_api_Update_None;
            sdrplay_api_ReasonForUpdateExtension1T updateExt = sdrplay_api_Update_Ext1_None;
            switch (getDeviceId())
            {
            case SDRPLAY_RSP2_ID:
                m_devParams->rxChannelA->rsp2TunerParams.amPortSel = settings.m_antenna == 2 ? sdrplay_api_Rsp2_AMPORT_1 : sdrplay_api_Rsp2_AMPORT_2;
                m_devParams->rxChannelA->rsp2TunerParams.antennaSel = settings.m_antenna == 1 ? sdrplay_api_Rsp2_ANTENNA_B : sdrplay_api_Rsp2_ANTENNA_A;
                update = (sdrplay_api_ReasonForUpdateT) (sdrplay_api_Update_Rsp2_AntennaControl | sdrplay_api_Update_Rsp2_AmPortSelect);
                break;
            case SDRPLAY_RSPduo_ID:
                if (m_dev->tuner == sdrplay_api_Tuner_A)
                {
                    m_devParams->rxChannelA->rspDuoTunerParams.tuner1AmPortSel = settings.m_antenna == 1 ? sdrplay_api_RspDuo_AMPORT_1 : sdrplay_api_RspDuo_AMPORT_2;
                    update = sdrplay_api_Update_RspDuo_AmPortSelect;
                }
                break;
            case SDRPLAY_RSPdx_ID:
                m_devParams->devParams->rspDxParams.antennaSel = (sdrplay_api_RspDx_AntennaSelectT)settings.m_antenna;
                updateExt = sdrplay_api_Update_RspDx_AntennaControl;
                break;
            default:
                // SDRPLAY_RSP1_ID and SDRPLAY_RSP1A_ID only have one antenna
                break;
            }
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, update, updateExt)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set antenna: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("extRef") || force)
    {
        if (m_running)
        {
            sdrplay_api_ReasonForUpdateT update = sdrplay_api_Update_None;
            sdrplay_api_ReasonForUpdateExtension1T updateExt = sdrplay_api_Update_Ext1_None;
            switch (getDeviceId())
            {
            case SDRPLAY_RSP2_ID:
                m_devParams->devParams->rsp2Params.extRefOutputEn = settings.m_extRef;
                update = sdrplay_api_Update_Rsp2_ExtRefControl;
                break;
            case SDRPLAY_RSPduo_ID:
                m_devParams->devParams->rspDuoParams.extRefOutputEn = settings.m_extRef;
                update = sdrplay_api_Update_RspDuo_ExtRefControl;
                break;
            default:
                // Other devices do not have external reference output
                break;
            }
            if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, update, updateExt)) != sdrplay_api_Success)
                qCritical() << "SDRPlayV3Input::applySettings: could not set state of external reference output: " << sdrplay_api_GetErrorString(err);
        }
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (forwardChange)
    {
        int sampleRate = getSampleRate();
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    return true;
}

bool SDRPlayV3Input::setDeviceCenterFrequency(quint64 freq_hz)
{
    qint64 df = ((qint64)freq_hz * m_settings.m_LOppmTenths) / 10000000LL;
    freq_hz += df;

    sdrplay_api_ErrT err;
    if (m_dev->tuner == sdrplay_api_Tuner_A)
        m_devParams->rxChannelA->tunerParams.rfFreq.rfHz = (double)freq_hz;
    else
        m_devParams->rxChannelB->tunerParams.rfFreq.rfHz = (double)freq_hz;
    m_sdrPlayThread->resetRfChanged();
    if ((err = sdrplay_api_Update(m_dev->dev, m_dev->tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
    {
        qWarning("SDRPlayV3Input::setDeviceCenterFrequency: could not set frequency to %llu Hz", freq_hz);
        return false;
    }
    else if (!m_sdrPlayThread->waitForRfChanged())
    {
        qWarning() << "SDRPlayV3Input::setDeviceCenterFrequency: could not set frequency: Rf update timed out";
        return false;
    }
    else
    {
        qDebug("SDRPlayV3Input::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
        return true;
    }
}

int SDRPlayV3Input::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SDRPlayV3Input::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

int SDRPlayV3Input::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setSdrPlayV3Settings(new SWGSDRangel::SWGSDRPlayV3Settings());
    response.getSdrPlayV3Settings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SDRPlayV3Input::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    SDRPlayV3Settings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureSDRPlayV3 *msg = MsgConfigureSDRPlayV3::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSDRPlayV3 *msgToGUI = MsgConfigureSDRPlayV3::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void SDRPlayV3Input::webapiUpdateDeviceSettings(
        SDRPlayV3Settings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getSdrPlayV3Settings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getSdrPlayV3Settings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("ifFrequencyIndex")) {
        settings.m_ifFrequencyIndex = response.getSdrPlayV3Settings()->getIfFrequencyIndex();
    }
    if (deviceSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getSdrPlayV3Settings()->getBandwidthIndex();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getSdrPlayV3Settings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getSdrPlayV3Settings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("fcPos"))
    {
        int fcPos = response.getSdrPlayV3Settings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (SDRPlayV3Settings::fcPos_t) fcPos;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getSdrPlayV3Settings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getSdrPlayV3Settings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("lnaIndex")) {
        settings.m_lnaIndex = response.getSdrPlayV3Settings()->getLnaIndex();
    }
    if (deviceSettingsKeys.contains("ifAGC")) {
        settings.m_ifAGC = response.getSdrPlayV3Settings()->getIfAgc() != 0;
    }
    if (deviceSettingsKeys.contains("ifGain")) {
        settings.m_ifGain = response.getSdrPlayV3Settings()->getIfGain();
    }
    if (deviceSettingsKeys.contains("amNotch")) {
        settings.m_amNotch = response.getSdrPlayV3Settings()->getAmNotch();
    }
    if (deviceSettingsKeys.contains("fmNotch")) {
        settings.m_fmNotch = response.getSdrPlayV3Settings()->getFmNotch();
    }
    if (deviceSettingsKeys.contains("dabNotch")) {
        settings.m_dabNotch = response.getSdrPlayV3Settings()->getDabNotch();
    }
    if (deviceSettingsKeys.contains("extRef")) {
        settings.m_extRef = response.getSdrPlayV3Settings()->getExtRef();
    }
    if (deviceSettingsKeys.contains("tuner")) {
        settings.m_tuner = response.getSdrPlayV3Settings()->getTuner();
    }
    if (deviceSettingsKeys.contains("antenna")) {
        settings.m_antenna = response.getSdrPlayV3Settings()->getAntenna();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getSdrPlayV3Settings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getSdrPlayV3Settings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getSdrPlayV3Settings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("biasTee")) {
        settings.m_biasTee = response.getSdrPlayV3Settings()->getBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSdrPlayV3Settings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSdrPlayV3Settings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSdrPlayV3Settings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSdrPlayV3Settings()->getReverseApiDeviceIndex();
    }
}

void SDRPlayV3Input::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRPlayV3Settings& settings)
{
    response.getSdrPlayV3Settings()->setCenterFrequency(settings.m_centerFrequency);
    response.getSdrPlayV3Settings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getSdrPlayV3Settings()->setIfFrequencyIndex(settings.m_ifFrequencyIndex);
    response.getSdrPlayV3Settings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getSdrPlayV3Settings()->setDevSampleRate(settings.m_devSampleRate);
    response.getSdrPlayV3Settings()->setLog2Decim(settings.m_log2Decim);
    response.getSdrPlayV3Settings()->setFcPos((int) settings.m_fcPos);
    response.getSdrPlayV3Settings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getSdrPlayV3Settings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getSdrPlayV3Settings()->setLnaIndex(settings.m_lnaIndex);
    response.getSdrPlayV3Settings()->setIfAgc(settings.m_ifAGC ? 1 : 0);
    response.getSdrPlayV3Settings()->setIfGain(settings.m_ifGain);
    response.getSdrPlayV3Settings()->setAmNotch(settings.m_amNotch);
    response.getSdrPlayV3Settings()->setFmNotch(settings.m_fmNotch);
    response.getSdrPlayV3Settings()->setDabNotch(settings.m_dabNotch);
    response.getSdrPlayV3Settings()->setExtRef(settings.m_extRef);
    response.getSdrPlayV3Settings()->setTuner(settings.m_tuner);
    response.getSdrPlayV3Settings()->setAntenna(settings.m_antenna);
    response.getSdrPlayV3Settings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getSdrPlayV3Settings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getSdrPlayV3Settings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getSdrPlayV3Settings()->setBiasTee(settings.m_biasTee ? 1 : 0);

    response.getSdrPlayV3Settings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSdrPlayV3Settings()->getReverseApiAddress()) {
        *response.getSdrPlayV3Settings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSdrPlayV3Settings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSdrPlayV3Settings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSdrPlayV3Settings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int SDRPlayV3Input::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSdrPlayV3Report(new SWGSDRangel::SWGSDRPlayV3Report());
    response.getSdrPlayV3Report()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void SDRPlayV3Input::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getSdrPlayV3Report()->setIntermediateFrequencies(new QList<SWGSDRangel::SWGFrequency*>);

    for (unsigned int i = 0; i < SDRPlayV3IF::getNbIFs(); i++)
    {
        response.getSdrPlayV3Report()->getIntermediateFrequencies()->append(new SWGSDRangel::SWGFrequency);
        response.getSdrPlayV3Report()->getIntermediateFrequencies()->back()->setFrequency(SDRPlayV3IF::getIF(i));
    }

    response.getSdrPlayV3Report()->setBandwidths(new QList<SWGSDRangel::SWGBandwidth*>);

    for (unsigned int i = 0; i < SDRPlayV3Bandwidths::getNbBandwidths(); i++)
    {
        response.getSdrPlayV3Report()->getBandwidths()->append(new SWGSDRangel::SWGBandwidth);
        response.getSdrPlayV3Report()->getBandwidths()->back()->setBandwidth(SDRPlayV3Bandwidths::getBandwidth(i));
    }

    switch(getDeviceId())
    {
    case SDRPLAY_RSP1_ID:
        response.getSdrPlayV3Report()->setDeviceType(new QString("RSP1"));
        break;
    case SDRPLAY_RSP1A_ID:
        response.getSdrPlayV3Report()->setDeviceType(new QString("RSP1A"));
        break;
    case SDRPLAY_RSP2_ID:
        response.getSdrPlayV3Report()->setDeviceType(new QString("RSP2"));
        break;
    case SDRPLAY_RSPduo_ID:
        response.getSdrPlayV3Report()->setDeviceType(new QString("RSPduo"));
        break;
    case SDRPLAY_RSPdx_ID:
        response.getSdrPlayV3Report()->setDeviceType(new QString("RSPdx"));
        break;
    default:
        response.getSdrPlayV3Report()->setDeviceType(new QString("Unknown"));
        break;
    }
}

void SDRPlayV3Input::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const SDRPlayV3Settings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SDRplayV3"));
    swgDeviceSettings->setSdrPlayV3Settings(new SWGSDRangel::SWGSDRPlayV3Settings());
    SWGSDRangel::SWGSDRPlayV3Settings *swgSDRPlayV3Settings = swgDeviceSettings->getSdrPlayV3Settings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgSDRPlayV3Settings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgSDRPlayV3Settings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("ifFrequencyIndex") || force) {
        swgSDRPlayV3Settings->setIfFrequencyIndex(settings.m_ifFrequencyIndex);
    }
    if (deviceSettingsKeys.contains("bandwidthIndex") || force) {
        swgSDRPlayV3Settings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgSDRPlayV3Settings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgSDRPlayV3Settings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgSDRPlayV3Settings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgSDRPlayV3Settings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgSDRPlayV3Settings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lnaIndex") || force) {
        swgSDRPlayV3Settings->setLnaIndex(settings.m_lnaIndex);
    }
    if (deviceSettingsKeys.contains("ifAGC") || force) {
        swgSDRPlayV3Settings->setIfAgc(settings.m_ifAGC ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ifGain") || force) {
        swgSDRPlayV3Settings->setIfGain(settings.m_ifGain);
    }
    if (deviceSettingsKeys.contains("amNotch") || force) {
        swgSDRPlayV3Settings->setAmNotch(settings.m_amNotch);
    }
    if (deviceSettingsKeys.contains("fmNotch") || force) {
        swgSDRPlayV3Settings->setFmNotch(settings.m_fmNotch);
    }
    if (deviceSettingsKeys.contains("dabNotch") || force) {
        swgSDRPlayV3Settings->setDabNotch(settings.m_dabNotch);
    }
    if (deviceSettingsKeys.contains("extRef") || force) {
        swgSDRPlayV3Settings->setExtRef(settings.m_extRef);
    }
    if (deviceSettingsKeys.contains("tuner") || force) {
        swgSDRPlayV3Settings->setTuner(settings.m_tuner);
    }
    if (deviceSettingsKeys.contains("antenna") || force) {
        swgSDRPlayV3Settings->setAntenna(settings.m_antenna);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgSDRPlayV3Settings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgSDRPlayV3Settings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgSDRPlayV3Settings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("biasTee") || force) {
        swgSDRPlayV3Settings->setBiasTee(settings.m_biasTee ? 1 : 0);
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void SDRPlayV3Input::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SDRplayV3"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

void SDRPlayV3Input::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SDRPlayV3Input::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SDRPlayV3Input::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

int SDRPlayV3Input::getDeviceId()
{
    if (m_dev != nullptr)
        return m_dev->hwVer; // E.g. SDRPLAY_RSPduo_ID
    else
        return -1;
}

// ====================================================================

sdrplay_api_Bw_MHzT SDRPlayV3Bandwidths::m_bwEnums[m_nb_bw] = {
    sdrplay_api_BW_0_200,
    sdrplay_api_BW_0_300,
    sdrplay_api_BW_0_600,
    sdrplay_api_BW_1_536,
    sdrplay_api_BW_5_000,
    sdrplay_api_BW_6_000,
    sdrplay_api_BW_7_000,
    sdrplay_api_BW_8_000
};

unsigned int SDRPlayV3Bandwidths::m_bw[m_nb_bw] = {
    200000,    // 0
    300000,    // 1
    600000,    // 2
    1536000,   // 3
    5000000,   // 4
    6000000,   // 5
    7000000,   // 6
    8000000,   // 7
};

sdrplay_api_Bw_MHzT SDRPlayV3Bandwidths::getBandwidthEnum(unsigned int bandwidth_index)
{
    if (bandwidth_index < m_nb_bw)
    {
        return m_bwEnums[bandwidth_index];
    }
    else
    {
        return m_bwEnums[0];
    }
}

unsigned int SDRPlayV3Bandwidths::getBandwidth(unsigned int bandwidth_index)
{
    if (bandwidth_index < m_nb_bw)
    {
        return m_bw[bandwidth_index];
    }
    else
    {
        return m_bw[0];
    }
}

unsigned int SDRPlayV3Bandwidths::getBandwidthIndex(unsigned int bandwidth)
{
    for (unsigned int i=0; i < m_nb_bw; i++)
    {
        if (bandwidth == m_bw[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayV3Bandwidths::getNbBandwidths()
{
    return SDRPlayV3Bandwidths::m_nb_bw;
}

// ====================================================================

sdrplay_api_If_kHzT SDRPlayV3IF::m_ifEnums[m_nb_if] = {
    sdrplay_api_IF_Zero,
    sdrplay_api_IF_0_450,
    sdrplay_api_IF_1_620,
    sdrplay_api_IF_2_048
};

unsigned int SDRPlayV3IF::m_if[m_nb_if] = {
    0,         // 0
    450000,    // 1
    1620000,   // 2
    2048000,   // 3
};

sdrplay_api_If_kHzT SDRPlayV3IF::getIFEnum(unsigned int if_index)
{
    if (if_index < m_nb_if)
    {
        return m_ifEnums[if_index];
    }
    else
    {
        return m_ifEnums[0];
    }
}

unsigned int SDRPlayV3IF::getIF(unsigned int if_index)
{
    if (if_index < m_nb_if)
    {
        return m_if[if_index];
    }
    else
    {
        return m_if[0];
    }
}

unsigned int SDRPlayV3IF::getIFIndex(unsigned int iff)
{
    for (unsigned int i=0; i < m_nb_if; i++)
    {
        if (iff == m_if[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayV3IF::getNbIFs()
{
    return SDRPlayV3IF::m_nb_if;
}

// ====================================================================

// LNA state attenuation values - for some devices, such as rsp1, this is combination of LNA and mixer attenuation

const int SDRPlayV3LNA::rsp1Attenuation[3][5] =
{
    {4, 0, 24, 19, 43},
    {4, 0,  7, 19, 26},
    {4, 0,  5, 19, 24}
};

const int SDRPlayV3LNA::rsp1AAttenuation[4][11] =
{
    {7,  0, 6, 12, 18, 37, 42, 61},
    {10, 0, 6, 12, 18, 20, 26, 32, 38, 57, 62},
    {10, 0, 7, 13, 19, 20, 27, 33, 39, 45, 64},
    { 9, 0, 6, 12, 20, 26, 32, 38, 43, 62}
};

const int SDRPlayV3LNA::rsp2Attenuation[3][10] =
{
    {9, 0, 10, 15, 21, 24, 34, 39, 45, 64},
    {6, 0,  7, 10, 17, 22, 41},
    {6, 0,  5, 21, 15, 15, 32}
};

const int SDRPlayV3LNA::rspDuoAttenuation[5][11] =
{
    { 7, 0, 6, 12, 18, 37, 42, 61},
    {10, 0, 6, 12, 18, 20, 26, 32, 38, 57, 62},
    {10, 0, 7, 13, 19, 20, 27, 33, 39, 45, 64},
    { 9, 0, 6, 12, 20, 26, 32, 38, 43, 62},
    { 5, 0, 6, 12, 18, 37}         // HiZ port
};

const int SDRPlayV3LNA::rspDxAttenuation[7][29] =
{
    {22, 0, 3,  6,  9, 12, 15, 18, 21, 24, 25, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60},
    {19, 0, 3,  6,  9, 12, 15, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60},
    {20, 0, 3,  6,  9, 12, 15, 18, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60},
    {27, 0, 3,  6,  9, 12, 15, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84},
    {28, 0, 3,  6,  9, 12, 15, 18, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84},
    {21, 0, 7, 10, 13, 16, 19, 22, 25, 31, 34, 37, 40, 43, 46, 49, 52, 55, 58, 61, 64, 67},
    {19, 0, 5,  8, 11, 14, 17, 20, 32, 35, 38, 41, 44, 47, 50, 53, 56, 59, 62, 65}
};

const int *SDRPlayV3LNA::getAttenuations(int deviceId, qint64 frequency)
{
    int row;
    const int *lnaAttenuation;

    switch (deviceId)
    {
    case SDRPLAY_RSP1_ID:
        if (frequency < 420000000)
            row = 0;
        else if (frequency < 10000000000)
            row = 1;
        else
            row = 2;
        lnaAttenuation = &rsp1Attenuation[row][0];
        break;
    case SDRPLAY_RSP1A_ID:
        if (frequency < 60000000)
            row = 0;
        else if (frequency < 420000000)
            row = 1;
        else if (frequency < 1000000000)
            row = 2;
        else
            row = 3;
        lnaAttenuation = &rsp1AAttenuation[row][0];
        break;
    case SDRPLAY_RSP2_ID:
        if (frequency < 420000000)
            row = 0;
        else if (frequency < 1000000000)
            row = 1;
        else
            row = 2;
        lnaAttenuation = &rsp2Attenuation[row][0];
        break;
    case SDRPLAY_RSPduo_ID:
        if (frequency < 60000000)
            row = 0;
        else if (frequency < 420000000)
            row = 1;
        else if (frequency < 1000000000)
            row = 2;
        else
            row = 3;
        lnaAttenuation = &rspDuoAttenuation[row][0];
        break;
    case SDRPLAY_RSPdx_ID:
        if (frequency < 2000000)
            row = 0;
        else if (frequency < 12000000)
            row = 1;
        else if (frequency < 60000000)
            row = 2;
        else if (frequency < 250000000)
            row = 3;
        else if (frequency < 420000000)
            row = 4;
        else if (frequency < 1000000000)
            row = 5;
        else
            row = 6;
        lnaAttenuation = &rspDxAttenuation[row][0];
        break;
    default:
        lnaAttenuation = nullptr;
        break;
    }
    return lnaAttenuation;
}

