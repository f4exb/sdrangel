#include <QtPlugin>
#include <QAction>
#include <rtl-sdr.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "rtlsdrplugin.h"
#include "rtlsdrgui.h"

const PluginDescriptor RTLSDRPlugin::m_pluginDescriptor = {
	QString("RTL-SDR Input"),
	QString("---"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/sdrangel"),
	true,
	QString("https://github.com/f4exb/sdrangel")
};

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
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

PluginInterface::SampleSourceDevices RTLSDRPlugin::enumSampleSources()
{
	SampleSourceDevices result;
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

		result.append(SampleSourceDevice(displayedName,
				m_deviceTypeID,
				QString(serial),
				i));
	}
	return result;
}

PluginGUI* RTLSDRPlugin::createSampleSourcePluginGUI(const QString& sourceId)
{
	if(sourceId == m_deviceTypeID) {
		RTLSDRGui* gui = new RTLSDRGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	} else {
		return NULL;
	}
}
