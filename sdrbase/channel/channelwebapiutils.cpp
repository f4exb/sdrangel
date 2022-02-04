///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "channelwebapiutils.h"

#include "SWGDeviceState.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceReport.h"
#include "SWGChannelSettings.h"
#include "SWGDeviceSet.h"
#include "SWGChannelActions.h"
#include "SWGFileSinkActions.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"

#include "maincore.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "channel/channelutils.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"
#include "feature/featureset.h"
#include "feature/feature.h"

bool ChannelWebAPIUtils::getDeviceSettings(unsigned int deviceIndex, SWGSDRangel::SWGDeviceSettings &deviceSettingsResponse, DeviceSet *&deviceSet)
{
    QString errorResponse;
    int httpRC;

    // Get current device settings
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getDeviceSettings - not a sample source device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getDeviceSettings - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getDeviceSettings: get device settings error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

bool ChannelWebAPIUtils::getFeatureSettings(unsigned int featureSetIndex, unsigned int featureIndex, SWGSDRangel::SWGFeatureSettings &featureSettingsResponse, Feature *&feature)
{
    QString errorResponse;
    int httpRC;
    FeatureSet *featureSet;

    // Get current feature settings
    std::vector<FeatureSet*> featureSets = MainCore::instance()->getFeatureeSets();
    if (featureSetIndex < featureSets.size())
    {
        featureSet = featureSets[featureSetIndex];
        if (featureIndex < (unsigned int)featureSet->getNumberOfFeatures())
        {
            feature = featureSet->getFeatureAt(featureIndex);
            httpRC = feature->webapiSettingsGet(featureSettingsResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getFeatureSettings: no feature " << featureSetIndex << ":" << featureIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getFeatureSettings: no feature set " << featureSetIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getFeatureSettings: get feature settings error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}


// Get device center frequency
bool ChannelWebAPIUtils::getCenterFrequency(unsigned int deviceIndex, double &frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        return WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", frequencyInHz);
    }
    else
    {
        return false;
    }
}

// Set device center frequency
bool ChannelWebAPIUtils::setCenterFrequency(unsigned int deviceIndex, double frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    int httpRC;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        // Patch centerFrequency
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        double freq;
        if (WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", freq))
        {
            WebAPIUtils::setSubObjectDouble(*jsonObj, "centerFrequency", frequencyInHz);
            QStringList deviceSettingsKeys;
            deviceSettingsKeys.append("centerFrequency");
            deviceSettingsResponse.init();
            deviceSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;

            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();

            httpRC = source->webapiSettingsPutPatch(false, deviceSettingsKeys, deviceSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::setCenterFrequency: set device frequency %f OK", frequencyInHz);
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::setCenterFrequency: set device frequency error %d: %s",
                    httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            qWarning("ChannelWebAPIUtils::setCenterFrequency: no centerFrequency key in device settings");
            return false;
        }
    }
    else
    {
        return false;
    }
}

// Start acquisition
bool ChannelWebAPIUtils::run(unsigned int deviceIndex, int subsystemIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        runResponse.setState(new QString());
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiRun(1, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiRun(1, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiRun(1, subsystemIndex, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::run - unknown device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::run - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::run: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

// Stop acquisition
bool ChannelWebAPIUtils::stop(unsigned int deviceIndex, int subsystemIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        runResponse.setState(new QString());
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiRun(0, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiRun(0, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiRun(0, subsystemIndex, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::stop - unknown device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::stop - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::stop: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

// Get input frequency offset for a channel
bool ChannelWebAPIUtils::getFrequencyOffset(unsigned int deviceIndex, int channelIndex, int& offset)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    QJsonObject *jsonObj;
    double offsetD;

    ChannelAPI *channel = MainCore::instance()->getChannel(deviceIndex, channelIndex);
    if (channel != nullptr)
    {
        httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        if (httpRC/100 != 2)
        {
            qWarning("ChannelWebAPIUtils::getFrequencyOffset: get channel settings error %d: %s",
                httpRC, qPrintable(errorResponse));
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();
        if (WebAPIUtils::getSubObjectDouble(*jsonObj, "inputFrequencyOffset", offsetD))
        {
            offset = (int)offsetD;
            return true;
        }
    }
    return false;
}

// Set input frequency offset for a channel
bool ChannelWebAPIUtils::setFrequencyOffset(unsigned int deviceIndex, int channelIndex, int offset)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    QJsonObject *jsonObj;

    ChannelAPI *channel = MainCore::instance()->getChannel(deviceIndex, channelIndex);
    if (channel != nullptr)
    {
        httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        if (httpRC/100 != 2)
        {
            qWarning("ChannelWebAPIUtils::setFrequencyOffset: get channel settings error %d: %s",
                httpRC, qPrintable(errorResponse));
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();

        if (WebAPIUtils::setSubObjectDouble(*jsonObj, "inputFrequencyOffset", (double)offset))
        {
            QStringList keys;
            keys.append("inputFrequencyOffset");
            channelSettingsResponse.init();
            channelSettingsResponse.fromJsonObject(*jsonObj);
            httpRC = channel->webapiSettingsPutPatch(false, keys, channelSettingsResponse, errorResponse);
            if (httpRC/100 != 2)
            {
                qWarning("ChannelWebAPIUtils::setFrequencyOffset: patch channel settings error %d: %s",
                    httpRC, qPrintable(errorResponse));
                return false;
            }

            return true;
        }
    }
    return false;
}

// Start or stop all file sinks in a given device set
bool ChannelWebAPIUtils::startStopFileSinks(unsigned int deviceIndex, bool start)
{
    MainCore *mainCore = MainCore::instance();
    ChannelAPI *channel;
    int channelIndex = 0;
    while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
    {
        if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.filesink"))
        {
            QStringList channelActionKeys = {"record"};
            SWGSDRangel::SWGChannelActions channelActions;
            SWGSDRangel::SWGFileSinkActions *fileSinkAction = new SWGSDRangel::SWGFileSinkActions();
            QString errorResponse;
            int httpRC;

            fileSinkAction->setRecord(start);
            channelActions.setFileSinkActions(fileSinkAction);
            httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
            if (httpRC/100 != 2)
            {
                qWarning("ChannelWebAPIUtils::startStopFileSinks: webapiActionsPost error %d: %s",
                    httpRC, qPrintable(errorResponse));
                return false;
            }
        }
        channelIndex++;
    }
    return true;
}

// Send AOS actions to all channels that support it
// See also: FeatureWebAPIUtils::satelliteAOS
bool ChannelWebAPIUtils::satelliteAOS(const QString name, bool northToSouthPass, const QString &tle, QDateTime dateTime)
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*> deviceSets = mainCore->getDeviceSets();
    for (unsigned int deviceIndex = 0; deviceIndex < deviceSets.size(); deviceIndex++)
    {
        ChannelAPI *channel;
        int channelIndex = 0;
        while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
        {
            if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.aptdemod"))
            {
                QStringList channelActionKeys = {"aos"};
                SWGSDRangel::SWGChannelActions channelActions;
                SWGSDRangel::SWGAPTDemodActions *aptDemodAction = new SWGSDRangel::SWGAPTDemodActions();
                SWGSDRangel::SWGAPTDemodActions_aos *aosAction = new SWGSDRangel::SWGAPTDemodActions_aos();
                QString errorResponse;
                int httpRC;

                aosAction->setSatelliteName(new QString(name));
                aosAction->setNorthToSouthPass(northToSouthPass);
                aosAction->setTle(new QString(tle));
                aosAction->setDateTime(new QString(dateTime.toString(Qt::ISODateWithMs)));
                aptDemodAction->setAos(aosAction);

                channelActions.setAptDemodActions(aptDemodAction);
                httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
                if (httpRC/100 != 2)
                {
                    qWarning("ChannelWebAPIUtils::satelliteAOS: webapiActionsPost error %d: %s",
                        httpRC, qPrintable(errorResponse));
                    return false;
                }
            }
            channelIndex++;
        }
    }
    return true;
}

// Send LOS actions to all channels that support it
bool ChannelWebAPIUtils::satelliteLOS(const QString name)
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*> deviceSets = mainCore->getDeviceSets();
    for (unsigned int deviceIndex = 0; deviceIndex < deviceSets.size(); deviceIndex++)
    {
        ChannelAPI *channel;
        int channelIndex = 0;
        while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
        {
            if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.aptdemod"))
            {
                QStringList channelActionKeys = {"los"};
                SWGSDRangel::SWGChannelActions channelActions;
                SWGSDRangel::SWGAPTDemodActions *aptDemodAction = new SWGSDRangel::SWGAPTDemodActions();
                SWGSDRangel::SWGAPTDemodActions_los *losAction = new SWGSDRangel::SWGAPTDemodActions_los();
                QString errorResponse;
                int httpRC;

                losAction->setSatelliteName(new QString(name));
                aptDemodAction->setLos(losAction);

                channelActions.setAptDemodActions(aptDemodAction);
                httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
                if (httpRC/100 != 2)
                {
                    qWarning("ChannelWebAPIUtils::satelliteLOS: webapiActionsPost error %d: %s",
                        httpRC, qPrintable(errorResponse));
                    return false;
                }
            }
            channelIndex++;
        }
    }
    return true;
}

bool ChannelWebAPIUtils::getDeviceSetting(unsigned int deviceIndex, const QString &setting, int &value)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        return WebAPIUtils::getSubObjectInt(*jsonObj, setting, value);
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getDeviceReportValue(unsigned int deviceIndex, const QString &key, QString &value)
{
    SWGSDRangel::SWGDeviceReport deviceReport;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get device report
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceReport.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceReport.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiReportGet(deviceReport, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceReport.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceReport.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiReportGet(deviceReport, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceReport.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceReport.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiReportGet(deviceReport, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getDeviceReportValue: unknown device type " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getDeviceReportValue: no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getDeviceReportValue: get device report error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    // Get value of requested key
    QJsonObject *jsonObj = deviceReport.asJsonObject();
    if (WebAPIUtils::getSubObjectString(*jsonObj, key, value))
    {
        // Done
        return true;
    }
    else
    {
        qWarning("ChannelWebAPIUtils::getDeviceReportValue: no key %s in device report", qPrintable(key));
        return false;
    }
}

bool ChannelWebAPIUtils::patchDeviceSetting(unsigned int deviceIndex, const QString &setting, int value)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    if (getDeviceSettings(deviceIndex, deviceSettingsResponse, deviceSet))
    {
        // Patch centerFrequency
        QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
        int oldValue;
        if (WebAPIUtils::getSubObjectInt(*jsonObj, setting, oldValue))
        {
            WebAPIUtils::setSubObjectInt(*jsonObj, setting, value);
            QStringList deviceSettingsKeys;
            deviceSettingsKeys.append(setting);
            deviceSettingsResponse.init();
            deviceSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;

            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();

            httpRC = source->webapiSettingsPutPatch(false, deviceSettingsKeys, deviceSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::patchDeviceSetting: set device setting %s OK", qPrintable(setting));
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::patchDeviceSetting: set device setting error %d: %s",
                    httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            qWarning("ChannelWebAPIUtils::patchDeviceSetting: no key %s in device settings", qPrintable(setting));
            return false;
        }
    }
    else
    {
        return false;
    }
}

// Set feature setting
bool ChannelWebAPIUtils::patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, const QString &value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    int httpRC;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        // Patch settings
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();
        QString oldValue;
        if (WebAPIUtils::getSubObjectString(*jsonObj, setting, oldValue))
        {
            WebAPIUtils::setSubObjectString(*jsonObj, setting, value);
            QStringList featureSettingsKeys;
            featureSettingsKeys.append(setting);
            featureSettingsResponse.init();
            featureSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;

            httpRC = feature->webapiSettingsPutPatch(false, featureSettingsKeys, featureSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %s OK", qPrintable(setting), qPrintable(value));
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %s error %d: %s",
                    qPrintable(setting), qPrintable(value), httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            qWarning("ChannelWebAPIUtils::patchFeatureSetting: no key %s in feature settings", qPrintable(setting));
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::patchFeatureSetting(unsigned int featureSetIndex, unsigned int featureIndex, const QString &setting, double value)
{
    SWGSDRangel::SWGFeatureSettings featureSettingsResponse;
    QString errorResponse;
    int httpRC;
    Feature *feature;

    if (getFeatureSettings(featureSetIndex, featureIndex, featureSettingsResponse, feature))
    {
        // Patch settings
        QJsonObject *jsonObj = featureSettingsResponse.asJsonObject();
        double oldValue;
        if (WebAPIUtils::getSubObjectDouble(*jsonObj, setting, oldValue))
        {
            WebAPIUtils::setSubObjectDouble(*jsonObj, setting, value);
            QStringList featureSettingsKeys;
            featureSettingsKeys.append(setting);
            featureSettingsResponse.init();
            featureSettingsResponse.fromJsonObject(*jsonObj);
            SWGSDRangel::SWGErrorResponse errorResponse2;

            httpRC = feature->webapiSettingsPutPatch(false, featureSettingsKeys, featureSettingsResponse, *errorResponse2.getMessage());

            if (httpRC/100 == 2)
            {
                qDebug("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %f OK", qPrintable(setting), value);
                return true;
            }
            else
            {
                qWarning("ChannelWebAPIUtils::patchFeatureSetting: set feature setting %s to %f error %d: %s",
                    qPrintable(setting), value, httpRC, qPrintable(*errorResponse2.getMessage()));
                return false;
            }
        }
        else
        {
            qWarning("ChannelWebAPIUtils::patchFeatureSetting: no key %s in feature settings", qPrintable(setting));
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool ChannelWebAPIUtils::getFeatureReportValue(unsigned int featureSetIndex, unsigned int featureIndex, const QString &key, int &value)
{
    SWGSDRangel::SWGFeatureReport featureReport;
    QString errorResponse;
    int httpRC;
    FeatureSet *featureSet;
    Feature *feature;

    // Get feature report
    std::vector<FeatureSet*> featureSets = MainCore::instance()->getFeatureeSets();
    if (featureSetIndex < featureSets.size())
    {
        featureSet = featureSets[featureSetIndex];
        if (featureIndex < (unsigned int)featureSet->getNumberOfFeatures())
        {
            feature = featureSet->getFeatureAt(featureIndex);
            httpRC = feature->webapiReportGet(featureReport, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getFeatureReportValue: no feature " << featureSetIndex << ":" << featureIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getFeatureReportValue: no feature set " << featureSetIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getFeatureReportValue: get feature report error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    // Get value of requested key
    QJsonObject *jsonObj = featureReport.asJsonObject();
    if (WebAPIUtils::getSubObjectInt(*jsonObj, key, value))
    {
        // Done
        return true;
    }
    else
    {
        qWarning("ChannelWebAPIUtils::getFeatureReportValue: no key %s in feature report", qPrintable(key));
        return false;
    }
}
