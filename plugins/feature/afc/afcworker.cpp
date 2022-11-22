///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include "SWGDeviceState.h"
#include "SWGSuccessResponse.h"
#include "SWGDeviceSettings.h"
#include "SWGChannelSettings.h"
#include "SWGErrorResponse.h"

#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "channel/channelapi.h"
#include "feature/feature.h"
#include "maincore.h"

#include "afcreport.h"
#include "afcworker.h"

MESSAGE_CLASS_DEFINITION(AFCWorker::MsgConfigureAFCWorker, Message)
MESSAGE_CLASS_DEFINITION(AFCWorker::MsgTrackedDeviceChange, Message)
MESSAGE_CLASS_DEFINITION(AFCWorker::MsgDeviceTrack, Message)
MESSAGE_CLASS_DEFINITION(AFCWorker::MsgDevicesApply, Message)

AFCWorker::AFCWorker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToGUI(nullptr),
    m_freqTracker(nullptr),
    m_trackerDeviceFrequency(0),
    m_trackerChannelOffset(0),
    m_updateTimer(this)
{
    qDebug("AFCWorker::AFCWorker");
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateTarget()));

    if (m_settings.m_hasTargetFrequency) {
    	m_updateTimer.start(m_settings.m_trackerAdjustPeriod * 1000);
    }
}

AFCWorker::~AFCWorker()
{
    m_inputMessageQueue.clear();
    stopWork();
}

void AFCWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void AFCWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void AFCWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void AFCWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool AFCWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureAFCWorker::match(cmd))
    {
        qDebug() << "AFCWorker::handleMessage: MsgConfigureAFCWorker";
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAFCWorker& cfg = (MsgConfigureAFCWorker&) cmd;

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MainCore::MsgChannelSettings::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MainCore::MsgChannelSettings& cfg = (MainCore::MsgChannelSettings&) cmd;
        SWGSDRangel::SWGChannelSettings *swgChannelSettings = cfg.getSWGSettings();
        qDebug() << "AFCWorker::handleMessage: MainCore::MsgChannelSettings:" << *swgChannelSettings->getChannelType();
        processChannelSettings(cfg.getChannelAPI(), swgChannelSettings);

        delete swgChannelSettings;
        return true;
    }
    else if (MsgDeviceTrack::match(cmd))
    {
        qDebug() << "AFCWorker::handleMessage: MsgDeviceTrack";
        QMutexLocker mutexLocker(&m_mutex);
        updateTarget();
        return true;
    }
    else if (MsgDevicesApply::match(cmd))
    {
        qDebug() << "AFCWorker::handleMessage: MsgDevicesApply";
        QMutexLocker mutexLocker(&m_mutex);
        initTrackerDeviceSet(m_settings.m_trackerDeviceSetIndex);
        initTrackedDeviceSet(m_settings.m_trackedDeviceSetIndex);
        return true;
    }
    else
    {
        return false;
    }
}

void AFCWorker::applySettings(const AFCSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AFCWorker::applySettings:" << settings.getDebugString(settingsKeys, force)  << " force: " << force;

    if (settingsKeys.contains("trackerDeviceSetIndex") || force) {
        initTrackerDeviceSet(settings.m_trackerDeviceSetIndex);
    }

    if (settingsKeys.contains("trackedDeviceSetIndex") || force) {
        initTrackedDeviceSet(settings.m_trackedDeviceSetIndex);
    }

    if (settingsKeys.contains("trackerAdjustPeriod") || force) {
        m_updateTimer.setInterval(settings.m_trackerAdjustPeriod * 1000);
    }

    if (settingsKeys.contains("hasTargetFrequency") || force)
    {
        if (settings.m_hasTargetFrequency) {
            m_updateTimer.start(m_settings.m_trackerAdjustPeriod * 1000);
        } else {
            m_updateTimer.stop();
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}


void AFCWorker::initTrackerDeviceSet(int deviceSetIndex)
{
    if (deviceSetIndex < 0) {
        return;
    }

    MainCore *mainCore = MainCore::instance();
    m_trackerDeviceSet = mainCore->getDeviceSets()[deviceSetIndex];

    for (int i = 0; i < m_trackerDeviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channel = m_trackerDeviceSet->getChannelAt(i);

        if (channel->getURI() == "sdrangel.channel.freqtracker")
        {
            m_freqTracker = channel;
            SWGSDRangel::SWGDeviceSettings resDevice;
            SWGSDRangel::SWGChannelSettings resChannel;
            SWGSDRangel::SWGErrorResponse error;

            int rc = m_webAPIAdapterInterface->devicesetDeviceSettingsGet(deviceSetIndex, resDevice, error);

            if (rc / 100 == 2)
            {
                QJsonObject *jsonObj = resDevice.asJsonObject();
                QJsonValue freqValue;

                if (WebAPIUtils::extractValue(*jsonObj, "centerFrequency", freqValue))
                {
                    double freq = freqValue.toDouble();
                    m_trackerDeviceFrequency = freq;
                }
                else
                {
                    qDebug() << "AFCWorker::initTrackerDeviceSet: cannot find device frequency";
                }
            }
            else
            {
                qDebug() << "AFCWorker::initTrackerDeviceSet: devicesetDeviceSettingsGet error" << rc << ":" << *error.getMessage();
            }


            rc = m_webAPIAdapterInterface->devicesetChannelSettingsGet(deviceSetIndex, i, resChannel, error);

            if (rc / 100 == 2) {
                m_trackerChannelOffset = resChannel.getFreqTrackerSettings()->getInputFrequencyOffset();
            } else {
                qDebug() << "AFCWorker::initTrackerDeviceSet: devicesetChannelSettingsGet error" << rc << ":" << *error.getMessage();
            }

            break;
        }
    }
}

void AFCWorker::initTrackedDeviceSet(int deviceSetIndex)
{
    if (deviceSetIndex < 0) {
        return;
    }

    MainCore *mainCore = MainCore::instance();
    m_trackedDeviceSet = mainCore->getDeviceSets()[deviceSetIndex];
    m_channelsMap.clear();

    for (int i = 0; i < m_trackedDeviceSet->getNumberOfChannels(); i++)
    {
        ChannelAPI *channel = m_trackedDeviceSet->getChannelAt(i);

        if (channel->getURI() != "sdrangel.channel.freqtracker")
        {
            SWGSDRangel::SWGChannelSettings resChannel;
            SWGSDRangel::SWGErrorResponse error;

            int rc = m_webAPIAdapterInterface->devicesetChannelSettingsGet(deviceSetIndex, i, resChannel, error);

            if (rc / 100 == 2)
            {
                QJsonObject *jsonObj = resChannel.asJsonObject();
                QJsonValue directionValue;
                QJsonValue channelOffsetValue;

                if (WebAPIUtils::extractValue(*jsonObj, "direction", directionValue))
                {
                    int direction = directionValue.toInt();

                    if (WebAPIUtils::extractValue(*jsonObj, "inputFrequencyOffset", channelOffsetValue))
                    {
                        int channelOffset = channelOffsetValue.toInt();
                        m_channelsMap.insert(channel, ChannelTracking{channelOffset, m_trackerChannelOffset, direction});
                    }
                    else
                    {
                        qDebug() << "AFCWorker::initTrackedDeviceSet: cannot find channel offset frequency";
                    }
                }
                else
                {
                    qDebug() << "AFCWorker::initTrackedDeviceSet: cannot find channel direction";
                }
            }
            else
            {
                qDebug() << "AFCWorker::initTrackedDeviceSet: devicesetChannelSettingsGet error" << rc << ":" << *error.getMessage();
            }
        }
    }
}

void AFCWorker::processChannelSettings(
    const ChannelAPI *channelAPI,
    SWGSDRangel::SWGChannelSettings *swgChannelSettings)
{
    MainCore *mainCore = MainCore::instance();
    QJsonObject *jsonObj = swgChannelSettings->asJsonObject();
    QJsonValue channelOffsetValue;

    if (WebAPIUtils::extractValue(*jsonObj, "inputFrequencyOffset", channelOffsetValue))
    {
        if (*swgChannelSettings->getChannelType() == "FreqTracker")
        {
            int trackerChannelOffset = channelOffsetValue.toInt();

            if (trackerChannelOffset != m_trackerChannelOffset)
            {
                qDebug("AFCWorker::processChannelSettings: FreqTracker offset change: %d", trackerChannelOffset);
                m_trackerChannelOffset = trackerChannelOffset;
                QMap<ChannelAPI*, ChannelTracking>::iterator it = m_channelsMap.begin();

                for (; it != m_channelsMap.end(); ++it)
                {
                    if (mainCore->existsChannel(it.key()))
                    {
                        int channelOffset = it.value().m_channelOffset + trackerChannelOffset - it.value().m_trackerOffset;
                        updateChannelOffset(it.key(), it.value().m_channelDirection, channelOffset);
                    }
                    else
                    {
                        m_channelsMap.erase(it);
                    }
                }
            }
        }
        else if (m_channelsMap.contains(const_cast<ChannelAPI*>(channelAPI)))
        {
            int channelOffset = channelOffsetValue.toInt();
            m_channelsMap[const_cast<ChannelAPI*>(channelAPI)].m_channelOffset = channelOffset;
            m_channelsMap[const_cast<ChannelAPI*>(channelAPI)].m_trackerOffset = m_trackerChannelOffset;
        }
    }
}

bool AFCWorker::updateChannelOffset(ChannelAPI *channelAPI, int direction, int offset)
{
    SWGSDRangel::SWGChannelSettings swgChannelSettings;
    SWGSDRangel::SWGErrorResponse errorResponse;
    QString channelId;
    channelAPI->getIdentifier(channelId);
    swgChannelSettings.init();
    qDebug() << "AFCWorker::updateChannelOffset:" << channelId << ":" << offset;

    QStringList channelSettingsKeys;
    channelSettingsKeys.append("inputFrequencyOffset");
    QString jsonSettingsStr = tr("\"inputFrequencyOffset\":%1").arg(offset);

    QString jsonStr = tr("{ \"channelType\": \"%1\", \"direction\": \"%2\", \"%3Settings\": {%4}}")
        .arg(QString(channelId))
        .arg(direction)
        .arg(QString(channelId))
        .arg(jsonSettingsStr);
    swgChannelSettings.fromJson(jsonStr);

    int httpRC = m_webAPIAdapterInterface->devicesetChannelSettingsPutPatch(
        m_trackedDeviceSet->getIndex(),
        channelAPI->getIndexInDeviceSet(),
        false, // PATCH
        channelSettingsKeys,
        swgChannelSettings,
        errorResponse
    );

    if (httpRC / 100 != 2)
    {
        qDebug() << "AFCWorker::updateChannelOffset: error code" << httpRC << ":" << *errorResponse.getMessage();
        return false;
    }

    return true;
}

void AFCWorker::updateTarget()
{
    SWGSDRangel::SWGDeviceSettings resDevice;
    SWGSDRangel::SWGChannelSettings resChannel;
    SWGSDRangel::SWGErrorResponse error;

    int rc = m_webAPIAdapterInterface->devicesetDeviceSettingsGet(m_settings.m_trackerDeviceSetIndex, resDevice, error);

    if (rc / 100 == 2)
    {
        QJsonObject *jsonObj = resDevice.asJsonObject();
        QJsonValue freqValue;

        if (WebAPIUtils::extractValue(*jsonObj, "centerFrequency", freqValue))
        {
            double freq = freqValue.toDouble();
            m_trackerDeviceFrequency = freq;
        }
        else
        {
            qDebug() << "AFCWorker::updateTarget: cannot find device frequency";
            return;
        }
    }
    else
    {
        qDebug() << "AFCWorker::updateTarget: devicesetDeviceSettingsGet error" << rc << ":" << *error.getMessage();
        return;
    }

    int64_t trackerFrequency = m_trackerDeviceFrequency + m_trackerChannelOffset;
    int64_t correction = m_settings.m_targetFrequency - trackerFrequency;
    int64_t tolerance = m_settings.m_freqTolerance;

    if ((correction > -tolerance) && (correction < tolerance))
    {
        reportUpdateTarget(correction, false);
        return;
    }

    if (m_settings.m_transverterTarget) // act on transverter
    {
        QJsonObject *jsonObj = resDevice.asJsonObject();
        QJsonValue xverterFrequencyValue;

        // adjust transverter
        if (WebAPIUtils::extractValue(*jsonObj, "transverterDeltaFrequency", xverterFrequencyValue))
        {
            double xverterFrequency = xverterFrequencyValue.toDouble();
            updateDeviceFrequency(m_trackerDeviceSet, "transverterDeltaFrequency", xverterFrequency + correction);
        }
        else
        {
            qDebug() << "AFCWorker::updateTarget: cannot find device transverter frequency";
            return;
        }

        // adjust tracker offset
        if (updateChannelOffset(m_freqTracker, 0, m_trackerChannelOffset + correction)) {
            m_trackerChannelOffset += correction;
        }

        reportUpdateTarget(correction, true);
    }
    else // act on device
    {
        QJsonObject *jsonObj = resDevice.asJsonObject();
        QJsonValue deviceFrequencyValue;

        if (WebAPIUtils::extractValue(*jsonObj, "centerFrequency", deviceFrequencyValue))
        {
            double deviceFrequency = deviceFrequencyValue.toDouble();
            updateDeviceFrequency(m_trackerDeviceSet, "centerFrequency", deviceFrequency + correction);
        }
        else
        {
            qDebug() << "AFCWorker::updateTarget: cannot find device transverter frequency";
            return;
        }

        reportUpdateTarget(correction, true);
    }
}

bool AFCWorker::updateDeviceFrequency(DeviceSet *deviceSet, const QString& key, int64_t frequency)
{
    SWGSDRangel::SWGDeviceSettings swgDeviceSettings;
    SWGSDRangel::SWGErrorResponse errorResponse;
    QStringList deviceSettingsKeys;
    deviceSettingsKeys.append(key);
    int deviceIndex = deviceSet->getIndex();
    DeviceAPI *deviceAPI = deviceSet->m_deviceAPI;
    swgDeviceSettings.init();
    QString jsonSettingsStr = tr("\"%1\":%2").arg(key).arg(frequency);
    QString deviceSettingsKey;
    getDeviceSettingsKey(deviceAPI, deviceSettingsKey);
    qDebug() << "AFCWorker::updateDeviceFrequency:"
        << deviceAPI->getHardwareId()
        << ":" << key
        << ":" << frequency;

    QString jsonStr = tr("{ \"deviceHwType\": \"%1\", \"direction\": \"%2\", \"%3\": {%4}}")
        .arg(deviceAPI->getHardwareId())
        .arg(getDeviceDirection(deviceAPI))
        .arg(deviceSettingsKey)
        .arg(jsonSettingsStr);
    swgDeviceSettings.fromJson(jsonStr);

    int httpRC = m_webAPIAdapterInterface->devicesetDeviceSettingsPutPatch
    (
        deviceIndex,
        false, // PATCH
        deviceSettingsKeys,
        swgDeviceSettings,
        errorResponse
    );

    if (httpRC / 100 != 2)
    {
        qDebug("AFCWorker::updateDeviceFrequency: error %d: %s", httpRC, qPrintable(*errorResponse.getMessage()));
        return false;
    }

    return true;
}

int AFCWorker::getDeviceDirection(DeviceAPI *deviceAPI)
{
    if (deviceAPI->getSampleSink()) {
        return 1;
    } else if (deviceAPI->getSampleMIMO()) {
        return 2;
    }

    return 0;
}

void AFCWorker::getDeviceSettingsKey(DeviceAPI *deviceAPI, QString& settingsKey)
{
    const QString& deviceHwId = deviceAPI->getHardwareId();

    if (deviceAPI->getSampleSink())
    {
        if (WebAPIUtils::m_sinkDeviceHwIdToSettingsKey.contains(deviceHwId)) {
            settingsKey = WebAPIUtils::m_sinkDeviceHwIdToSettingsKey[deviceHwId];
        }
    }
    else if (deviceAPI->getSampleMIMO())
    {
        if (WebAPIUtils::m_mimoDeviceHwIdToSettingsKey.contains(deviceHwId)) {
            settingsKey = WebAPIUtils::m_mimoDeviceHwIdToSettingsKey[deviceHwId];
        }
    }
    else
    {
        if (WebAPIUtils::m_sourceDeviceHwIdToSettingsKey.contains(deviceHwId)) {
            settingsKey = WebAPIUtils::m_sourceDeviceHwIdToSettingsKey[deviceHwId];
        }
    }
}

void AFCWorker::reportUpdateTarget(int correction, bool done)
{
    if (m_msgQueueToGUI)
    {
        AFCReport::MsgUpdateTarget *msg = AFCReport::MsgUpdateTarget::create(correction, done);
        m_msgQueueToGUI->push(msg);
    }
}
