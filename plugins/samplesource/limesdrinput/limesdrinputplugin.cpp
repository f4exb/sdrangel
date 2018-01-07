///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QtPlugin>

#include <regex>
#include <string>

#include "lime/LimeSuite.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "device/devicesourceapi.h"

#ifdef SERVER_MODE
#include "limesdrinput.h"
#else
#include "limesdrinputgui.h"
#endif
#include "limesdrinputplugin.h"

const PluginDescriptor LimeSDRInputPlugin::m_pluginDescriptor = {
    QString("LimeSDR Input"),
    QString("3.10.1"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString LimeSDRInputPlugin::m_hardwareID = "LimeSDR";
const QString LimeSDRInputPlugin::m_deviceTypeID = LIMESDR_DEVICE_TYPE_ID;

LimeSDRInputPlugin::LimeSDRInputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& LimeSDRInputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void LimeSDRInputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices LimeSDRInputPlugin::enumSampleSources()
{
    lms_info_str_t* deviceList;
    int nbDevices;
    SamplingDevices result;

    if ((nbDevices = LMS_GetDeviceList(0)) <= 0)
    {
        qDebug("LimeSDRInputPlugin::enumSampleSources: Could not find any LimeSDR device");
        return result; // empty result
    }

    deviceList = new lms_info_str_t[nbDevices];

    if (LMS_GetDeviceList(deviceList) < 0)
    {
        qDebug("LimeSDRInputPlugin::enumSampleSources: Could not obtain LimeSDR devices information");
        delete[] deviceList;
        return result; // empty result
    }
    else
    {
        for (int i = 0; i < nbDevices; i++)
        {
            std::string serial("N/D");
            findSerial((const char *) deviceList[i], serial);

            DeviceLimeSDRParams limeSDRParams;
            limeSDRParams.open(deviceList[i]);
            limeSDRParams.close();

            for (unsigned int j = 0; j < limeSDRParams.m_nbRxChannels; j++)
            {
                qDebug("LimeSDRInputPlugin::enumSampleSources: device #%d channel %u: %s", i, j, (char *) deviceList[i]);
                QString displayedName(QString("LimeSDR[%1:%2] %3").arg(i).arg(j).arg(serial.c_str()));
                result.append(SamplingDevice(displayedName,
                        m_hardwareID,
                        m_deviceTypeID,
                        QString(deviceList[i]),
                        i,
                        PluginInterface::SamplingDevice::PhysicalDevice,
                        true,
                        limeSDRParams.m_nbRxChannels,
                        j));
            }
        }
    }

    delete[] deviceList;
    return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* LimeSDRInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* LimeSDRInputPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sourceId == m_deviceTypeID)
    {
        LimeSDRInputGUI* gui = new LimeSDRInputGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

bool LimeSDRInputPlugin::findSerial(const char *lmsInfoStr, std::string& serial)
{
    std::regex serial_reg("serial=([0-9,A-F]+)");
    std::string input(lmsInfoStr);
    std::smatch result;
    std::regex_search(input, result, serial_reg);

    if (result[1].str().length()>0)
    {
        serial = result[1].str();
        return true;
    }
    else
    {
        return false;
    }
}

DeviceSampleSource *LimeSDRInputPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        LimeSDRInput* input = new LimeSDRInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

