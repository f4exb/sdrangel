#include <QtPlugin>
#include <QAction>
#include <rtl-sdr.h>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "v4lplugin.h"
#include "v4lgui.h"

const PluginDescriptor V4LPlugin::m_pluginDescriptor = {
	QString("V4L Input"),
	QString("3.18"),
	QString("(c) 2014 John Greb"),
	QString("http://palosaari.fi/linux/"),
	true,
	QString("github.com/hexameron/rtl-sdrangelove")
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

	QString displayedName(QString("Kernel Source #1"));
	SimpleSerializer s(1);
	s.writeS32(1, 0);
	result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.v4l", s.final()));

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
