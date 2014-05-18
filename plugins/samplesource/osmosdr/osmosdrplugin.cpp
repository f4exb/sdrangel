#include <QtPlugin>
#include <QAction>
#include <osmosdr.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "osmosdrplugin.h"
#include "osmosdrgui.h"

const PluginDescriptor OsmoSDRPlugin::m_pluginDescriptor = {
	QString("OsmoSDR Input"),
	QString("---"),
	QString("(c) Christian Daniel"),
	QString("http://sdr.osmocom.org/trac/wiki/osmo-sdr"),
	true,
	QString("http://cgit.osmocom.org/cgit/osmo-sdr")
};

OsmoSDRPlugin::OsmoSDRPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& OsmoSDRPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void OsmoSDRPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.osmo-sdr", this);
}

PluginInterface::SampleSourceDevices OsmoSDRPlugin::enumSampleSources()
{
	SampleSourceDevices result;
	int count = osmosdr_get_device_count();
	char vendor[256];
	char product[256];
	char serial[256];

	for(int i = 0; i < count; i++) {
		vendor[0] = '\0';
		product[0] = '\0';
		serial[0] = '\0';

		if(osmosdr_get_device_usb_strings(i, vendor, product, serial) != 0)
			continue;
		QString displayedName(QString("OsmoSDR #%1 (#%2)").arg(i + 1).arg(serial));
		qDebug("found %s", qPrintable(displayedName));
		SimpleSerializer s(1);
		s.writeS32(1, i);
		result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.osmo-sdr", s.final()));
	}

	return result;
}

PluginGUI* OsmoSDRPlugin::createSampleSource(const QString& sourceName, const QByteArray& address)
{
	if(sourceName == "org.osmocom.sdr.samplesource.osmo-sdr") {
		OsmoSDRGui* gui = new OsmoSDRGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	} else {
		return NULL;
	}
}
