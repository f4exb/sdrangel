#include <QtPlugin>
#include <rtl-sdr.h>

#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include <device/devicesourceapi.h>

#ifdef SERVER_MODE
#include "rtlsdrinput.h"
#else
#include "rtlsdrgui.h"
#endif
#include "rtlsdrplugin.h"

const PluginDescriptor RTLSDRPlugin::m_pluginDescriptor = {
	QString("RTL-SDR Input"),
	QString("3.11.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

const QString RTLSDRPlugin::m_hardwareID = "RTLSDR";
const QString RTLSDRPlugin::m_deviceTypeID = RTLSDR_DEVICE_TYPE_ID;

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

PluginInterface::SamplingDevices RTLSDRPlugin::enumSampleSources()
{
	SamplingDevices result;
	int count = rtlsdr_get_device_count();
	char vendor[256];
	char product[256];
	char serial[256];

	for(int i = 0; i < count; i++) {
		vendor[0] = '\0';
		product[0] = '\0';
		serial[0] = '\0';

		if(rtlsdr_get_device_usb_strings((uint32_t)i, vendor, product, serial) != 0)
			continue;
		QString displayedName(QString("RTL-SDR[%1] %2").arg(i).arg(serial));

		result.append(SamplingDevice(displayedName,
		        m_hardwareID,
				m_deviceTypeID,
				QString(serial),
				i,
				PluginInterface::SamplingDevice::PhysicalDevice,
				true,
				1,
				0));
	}
	return result;
}

#ifdef SERVER_MODE
PluginInstanceGUI* RTLSDRPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId __attribute((unused)),
        QWidget **widget __attribute((unused)),
        DeviceUISet *deviceUISet __attribute((unused)))
{
    return 0;
}
#else
PluginInstanceGUI* RTLSDRPlugin::createSampleSourcePluginInstanceGUI(
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

DeviceSampleSource *RTLSDRPlugin::createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI)
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

