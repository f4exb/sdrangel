#include <QtPlugin>
#include <QAction>
#include <rtl-sdr.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "v4lplugin.h"
#include "v4lgui.h"

const PluginDescriptor V4LPlugin::m_pluginDescriptor = {
	QString("V4L Input"),
	QString("---"),
	QString("(c) librtlsdr authors"),
	QString("http://sdr.osmocom.org/trac/wiki/rtl-sdr"),
	true,
	QString("http://cgit.osmocom.org/cgit/rtl-sdr")
};

V4LPlugin::V4LPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& V4LPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void V4LPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.v4l", this);
}

PluginInterface::SampleSourceDevices V4LPlugin::enumSampleSources()
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
		QString displayedName(QString("RTL-SDR #%1 (%2 #%3)").arg(i + 1).arg(product).arg(serial));
		SimpleSerializer s(1);
		s.writeS32(1, i);
		result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.v4l", s.final()));
	}
	return result;
}

PluginGUI* V4LPlugin::createSampleSource(const QString& sourceName, const QByteArray& address)
{
	if(sourceName == "org.osmocom.sdr.samplesource.v4l") {
		V4LGui* gui = new V4LGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	} else {
		return NULL;
	}
}
