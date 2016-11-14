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

#include <QtPlugin>
#include <QAction>
#include <libusb-1.0/libusb.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "sdrplaygui.h"
#include "sdrplayplugin.h"
#include <device/devicesourceapi.h>

const PluginDescriptor SDRPlayPlugin::m_pluginDescriptor = {
    QString("SDRPlay Input"),
    QString("2.3.0"),
    QString("(c) Edouard Griffiths, F4EXB"),
    QString("https://github.com/f4exb/sdrangel"),
    true,
    QString("https://github.com/f4exb/sdrangel")
};

const QString SDRPlayPlugin::m_deviceTypeID = SDRPLAY_DEVICE_TYPE_ID;

SDRPlayPlugin::SDRPlayPlugin(QObject* parent) :
    QObject(parent)
{
}

const PluginDescriptor& SDRPlayPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void SDRPlayPlugin::initPlugin(PluginAPI* pluginAPI)
{
    pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SamplingDevices SDRPlayPlugin::enumSampleSources()
{
    // USB device:
    // New USB device found, idVendor=1df7, idProduct=2500
    // New USB device strings: Mfr=0, Product=0, SerialNumber=0

    libusb_context *usb_context = 0;
    ssize_t num_devs;
    SamplingDevices result;
    libusb_device **devs;
    libusb_device *dev;
    int i = 0;

    if (libusb_init(&usb_context)) {
        return result;
    }

    num_devs = libusb_get_device_list(usb_context, &devs);

    if (num_devs > 0)
    {
        while ((dev = devs[i++]) != 0)
        {
            struct libusb_device_descriptor desc;
            struct libusb_config_descriptor *conf_desc = 0;
            int res = libusb_get_device_descriptor(dev, &desc);
            unsigned short dev_vid = desc.idVendor;
            unsigned short dev_pid = desc.idProduct;

            if ((dev_vid == 0x1df7) && (dev_pid == 0x2500))
            {
                QString displayedName(QString("SDRPlay[0] 0"));

                result.append(SamplingDevice(displayedName,
                        m_deviceTypeID,
                        QString("0"),
                        0));
                break;
            }
        }
    }

    libusb_free_device_list(devs, 1);

    if (usb_context)
    {
        libusb_exit(usb_context);
    }

    return result;
}

PluginGUI* SDRPlayPlugin::createSampleSourcePluginGUI(const QString& sourceId,QWidget **widget, DeviceSourceAPI *deviceAPI)
{
    if(sourceId == m_deviceTypeID)
    {
        SDRPlayGui* gui = new SDRPlayGui(deviceAPI);
        *widget = gui;
        return gui;
    }
    else
    {
        return 0;
    }
}
