///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QtPlugin>

#include <regex>
#include <string>

#include "lime/LimeSuite.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "limesdr/devicelimesdrparam.h"

#ifdef SERVER_MODE
#include "limesdroutput.h"
#else
#include "limesdroutputgui.h"
#endif
#include "limesdroutputplugin.h"
#include "limesdroutputwebapiadapter.h"

const PluginDescriptor LimeSDROutputPlugin::m_pluginDescriptor = {
    QString("LimeSDR Output"),
    QString("4.11.10"),
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

void LimeSDROutputPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    lms_info_str_t* deviceList;
    int nbDevices;
    SamplingDevices result;

    if ((nbDevices = LMS_GetDeviceList(0)) <= 0)
    {
        qDebug("LimeSDROutputPlugin::enumOriginDevices: Could not find any LimeSDR device");
        return; // do nothing
    }

    deviceList = new lms_info_str_t[nbDevices];

    if (LMS_GetDeviceList(deviceList) < 0)
    {
        qDebug("LimeSDROutputPlugin::enumOriginDevices: Could not obtain LimeSDR devices information");
        delete[] deviceList;
        return; // do nothing
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

            QString displayedName(QString("LimeSDR[%1:%2] %3").arg(i).arg("%1").arg(serial.c_str()));

            originDevices.append(OriginDevice(
                displayedName,
                m_hardwareID,
                QString(deviceList[i]),
                i,
                limeSDRParams.m_nbRxChannels,
                limeSDRParams.m_nbTxChannels
            ));
        }
    }

    delete[] deviceList;

	listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices LimeSDROutputPlugin::enumSampleSinks(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            for (unsigned int j = 0; j < it->nbTxStreams; j++)
            {
                qDebug("LimeSDROutputPlugin::enumSampleSinks: device #%d channel %u: %s", it->sequence, j, qPrintable(it->serial));
                QString displayedName(it->displayableName.arg(j));
                result.append(SamplingDevice(
                    displayedName,
                    it->hardwareId,
                    m_deviceTypeID,
                    it->serial,
                    it->sequence,
                    PluginInterface::SamplingDevice::PhysicalDevice,
                    PluginInterface::SamplingDevice::StreamSingleTx,
                    it->nbTxStreams,
                    j
                ));
            }
        }
    }

    return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* LimeSDROutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sinkId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
PluginInstanceGUI* LimeSDROutputPlugin::createSampleSinkPluginInstanceGUI(
        const QString& sinkId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    if(sinkId == m_deviceTypeID)
    {
        LimeSDROutputGUI* gui = new LimeSDROutputGUI(deviceUISet);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
#endif

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

DeviceSampleSink* LimeSDROutputPlugin::createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI)
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

DeviceWebAPIAdapter *LimeSDROutputPlugin::createDeviceWebAPIAdapter() const
{
    return new LimeSDROutputWebAPIAdapter();
}
