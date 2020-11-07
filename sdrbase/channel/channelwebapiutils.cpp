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
#include "SWGChannelSettings.h"
#include "SWGDeviceSet.h"

#include "maincore.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"

bool ChannelWebAPIUtils::getCenterFrequency(int deviceIndex, double &frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get current device settings
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if ((deviceIndex >= 0) && (deviceIndex < deviceSets.size()))
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiSettingsGet(deviceSettingsResponse, *errorResponse.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiSettingsGet(deviceSettingsResponse, *errorResponse.getMessage());
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiSettingsGet(deviceSettingsResponse, *errorResponse.getMessage());
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getCenterFrequency - not a sample source device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getCenterFrequency - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getCenterFrequency: get device frequency error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        return false;
    }

    QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
    return WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", frequencyInHz);
}

bool ChannelWebAPIUtils::setCenterFrequency(int deviceIndex, double frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    SWGSDRangel::SWGErrorResponse errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get current device settings
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if ((deviceIndex >= 0) && (deviceIndex < deviceSets.size()))
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiSettingsGet(deviceSettingsResponse, *errorResponse.getMessage());
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiSettingsGet(deviceSettingsResponse, *errorResponse.getMessage());
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiSettingsGet(deviceSettingsResponse, *errorResponse.getMessage());
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::setCenterFrequency: not a sample source device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::setCenterFrequency: no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::setCenterFrequency: get device frequency error %d: %s",
            httpRC, qPrintable(*errorResponse.getMessage()));
        return false;
    }

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

    return true;
}
