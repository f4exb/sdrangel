///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2014 John Greb <hexameron>                                          //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                          //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include <QtPlugin>
#include <rtl-sdr.h>

#include "plugin/pluginapi.h"

#ifdef SERVER_MODE
#include "rtlsdrinput.h"
#else
#include "rtlsdrgui.h"
#endif
#include "rtlsdrplugin.h"
#include "rtlsdrwebapiadapter.h"
#ifdef ANDROID
#include "util/android.h"
#endif

const PluginDescriptor RTLSDRPlugin::m_pluginDescriptor = {
    QStringLiteral("RTLSDR"),
	QStringLiteral("RTL-SDR Input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "RTLSDR";
static constexpr const char* const m_deviceTypeID = RTLSDR_DEVICE_TYPE_ID;

RTLSDRPlugin::RTLSDRPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& RTLSDRPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void RTLSDRPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void RTLSDRPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

#ifdef ANDROID
    typedef struct rtlsdr_dongle {
        uint16_t vid;
        uint16_t pid;
        const char *name;
    } rtlsdr_dongle_t;

    // This list comes from librtlsdr.c
    rtlsdr_dongle_t known_devices[] = {
        { 0x0bda, 0x2832, "Generic RTL2832U" },
        { 0x0bda, 0x2838, "Generic RTL2832U OEM" },
        { 0x0413, 0x6680, "DigitalNow Quad DVB-T PCI-E card" },
        { 0x0413, 0x6f0f, "Leadtek WinFast DTV Dongle mini D" },
        { 0x0458, 0x707f, "Genius TVGo DVB-T03 USB dongle (Ver. B)" },
        { 0x0ccd, 0x00a9, "Terratec Cinergy T Stick Black (rev 1)" },
        { 0x0ccd, 0x00b3, "Terratec NOXON DAB/DAB+ USB dongle (rev 1)" },
        { 0x0ccd, 0x00b4, "Terratec Deutschlandradio DAB Stick" },
        { 0x0ccd, 0x00b5, "Terratec NOXON DAB Stick - Radio Energy" },
        { 0x0ccd, 0x00b7, "Terratec Media Broadcast DAB Stick" },
        { 0x0ccd, 0x00b8, "Terratec BR DAB Stick" },
        { 0x0ccd, 0x00b9, "Terratec WDR DAB Stick" },
        { 0x0ccd, 0x00c0, "Terratec MuellerVerlag DAB Stick" },
        { 0x0ccd, 0x00c6, "Terratec Fraunhofer DAB Stick" },
        { 0x0ccd, 0x00d3, "Terratec Cinergy T Stick RC (Rev.3)" },
        { 0x0ccd, 0x00d7, "Terratec T Stick PLUS" },
        { 0x0ccd, 0x00e0, "Terratec NOXON DAB/DAB+ USB dongle (rev 2)" },
        { 0x1554, 0x5020, "PixelView PV-DT235U(RN)" },
        { 0x15f4, 0x0131, "Astrometa DVB-T/DVB-T2" },
        { 0x15f4, 0x0133, "HanfTek DAB+FM+DVB-T" },
        { 0x185b, 0x0620, "Compro Videomate U620F"},
        { 0x185b, 0x0650, "Compro Videomate U650F"},
        { 0x185b, 0x0680, "Compro Videomate U680F"},
        { 0x1b80, 0xd393, "GIGABYTE GT-U7300" },
        { 0x1b80, 0xd394, "DIKOM USB-DVBT HD" },
        { 0x1b80, 0xd395, "Peak 102569AGPK" },
        { 0x1b80, 0xd397, "KWorld KW-UB450-T USB DVB-T Pico TV" },
        { 0x1b80, 0xd398, "Zaapa ZT-MINDVBZP" },
        { 0x1b80, 0xd39d, "SVEON STV20 DVB-T USB & FM" },
        { 0x1b80, 0xd3a4, "Twintech UT-40" },
        { 0x1b80, 0xd3a8, "ASUS U3100MINI_PLUS_V2" },
        { 0x1b80, 0xd3af, "SVEON STV27 DVB-T USB & FM" },
        { 0x1b80, 0xd3b0, "SVEON STV21 DVB-T USB & FM" },
        { 0x1d19, 0x1101, "Dexatek DK DVB-T Dongle (Logilink VG0002A)" },
        { 0x1d19, 0x1102, "Dexatek DK DVB-T Dongle (MSI DigiVox mini II V3.0)" },
        { 0x1d19, 0x1103, "Dexatek Technology Ltd. DK 5217 DVB-T Dongle" },
        { 0x1d19, 0x1104, "MSI DigiVox Micro HD" },
        { 0x1f4d, 0xa803, "Sweex DVB-T USB" },
        { 0x1f4d, 0xb803, "GTek T803" },
        { 0x1f4d, 0xc803, "Lifeview LV5TDeluxe" },
        { 0x1f4d, 0xd286, "MyGica TD312" },
        { 0x1f4d, 0xd803, "PROlectrix DV107669" },
    };

    int deviceNo = 0;
    for (int i = 0; i < sizeof(known_devices)/sizeof(rtlsdr_dongle_t); i++)
    {
        QStringList serialStrings = Android::listUSBDeviceSerials(known_devices[i].vid, known_devices[i].pid);

        for (const auto serial : serialStrings)
        {
            QString displayableName(QString("RTL-SDR[%1] %2").arg(deviceNo).arg(serial));

            originDevices.append(OriginDevice(
                displayableName,
                m_hardwareID,
                serial,
                deviceNo, // sequence
                1, // Nb Rx
                0  // Nb Tx
            ));

            deviceNo++;
        }
    }

    listedHwIds.append(m_hardwareID);
#else
	int count = rtlsdr_get_device_count();
	char vendor[256];
	char product[256];
	char serial[256];

	for(int i = 0; i < count; i++)
    {
		vendor[0] = '\0';
		product[0] = '\0';
		serial[0] = '\0';

		if(rtlsdr_get_device_usb_strings((uint32_t)i, vendor, product, serial) != 0)
			continue;
		QString displayableName(QString("RTL-SDR[%1] %2").arg(i).arg(serial));

        originDevices.append(OriginDevice(
            displayableName,
            m_hardwareID,
            serial,
            i, // sequence
            1, // Nb Rx
            0  // Nb Tx
        ));
	}

    listedHwIds.append(m_hardwareID);
#endif
}

PluginInterface::SamplingDevices RTLSDRPlugin::enumSampleSources(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            result.append(SamplingDevice(
                it->displayableName,
                it->hardwareId,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::PhysicalDevice,
                PluginInterface::SamplingDevice::StreamSingleRx,
                1,
                0
            ));
            qDebug("RTLSDRPlugin::enumSampleSources: enumerated RTL-SDR device #%d", it->sequence);
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* RTLSDRPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
DeviceGUI* RTLSDRPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if(sourceId == m_deviceTypeID) {
		RTLSDRGui* gui = new RTLSDRGui(deviceUISet);
		*widget = gui;
		return gui;
	} else {
		return 0;
	}
}
#endif

DeviceSampleSource *RTLSDRPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        RTLSDRInput* input = new RTLSDRInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *RTLSDRPlugin::createDeviceWebAPIAdapter() const
{
    return new RTLSDRWebAPIAdapter();
}
