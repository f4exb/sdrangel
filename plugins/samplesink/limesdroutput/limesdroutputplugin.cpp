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
#include <QAction>

#include <regex>
#include <string>

#include "lime/LimeSuite.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "device/devicesinkapi.h"

#include "limesdroutputgui.h"
#include "limesdroutputplugin.h"

const PluginDescriptor LimeSDROutputPlugin::m_pluginDescriptor = {
    QString("LimeSDR Output"),
    QString("3.7.8"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString LimeSDROutputPlugin::m_hardwareID = "LimeSDR";
const QString LimeSDROutputPlugin::m_deviceTypeID = LIMESDROUTPUT_DEVICE_TYPE_ID;

LimeSDROutputPlugin::LimeSDROutputPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& LimeSDROutputPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void LimeSDROutputPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSink(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices LimeSDROutputPlugin::enumSampleSinks()
{
    lms_info_str_t* deviceList;
    int nbDevices;
    SamplingDevices result;

    if ((nbDevices = LMS_GetDeviceList(0)) <= 0)
    {
        qDebug("LimeSDROutputPlugin::enumSampleSources: Could not find any LimeSDR device");
        return result; // empty result
    }

    deviceList = new lms_info_str_t[nbDevices];

    if (LMS_GetDeviceList(deviceList) < 0)
    {
        qDebug("LimeSDROutputPlugin::enumSampleSources: Could not obtain LimeSDR devices information");
        delete[] deviceList;
        return result; // empty result
    }
    else
    {
        for (int i = 0; i < nbDevices; i++)
        {
            std::string serial("N/D");
            findSerial((const char *) deviceList[i], serial);

            qDebug("LimeSDROutputPlugin::enumSampleSources: device #%d: %s", i, (char *) deviceList[i]);
            QString displayedName(QString("LimeSDR[%1] %2").arg(i).arg(serial.c_str()));
            result.append(SamplingDevice(displayedName,
                    m_hardwareID,
                    m_deviceTypeID,
                    QString(deviceList[i]),
                    i));
        }
    }

    delete[] deviceList;
    return result;
}

PluginInstanceGUI* LimeSDROutputPlugin::createSampleSinkPluginInstanceGUI(const QString& sinkId,QWidget **widget, DeviceSinkAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        LimeSDROutputGUI* gui = new LimeSDROutputGUI(deviceAPI);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}

bool LimeSDROutputPlugin::findSerial(const char *lmsInfoStr, std::string& serial)
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

DeviceSampleSink* LimeSDROutputPlugin::createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceSinkAPI *deviceAPI)
{
    if(sinkId == m_deviceTypeID)
    {
        LimeSDROutput* output = new LimeSDROutput(deviceAPI);
        return output;
    }
    else
    {
        return 0;
    }

}

