#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "fcdplugin.h"
#include "fcdgui.h"

const PluginDescriptor FCDPlugin::m_pluginDescriptor = {
	QString("FunCube Input"),
	QString("V20"),
	QString("(c) John Greb"),
	QString("http://funcubedongle.com"),
	true,
	QString("http://www.oz9aec.net/index.php/funcube-dongle")
};

FCDPlugin::FCDPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& FCDPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void FCDPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.fcd", this);
}

PluginInterface::SampleSourceDevices FCDPlugin::enumSampleSources()
{
	SampleSourceDevices result;

	QString displayedName(QString("Funcube Dongle #1"));
	SimpleSerializer s(1);
	s.writeS32(1, 0);
	result.append(SampleSourceDevice(displayedName, "org.osmocom.sdr.samplesource.fcd", s.final()));

	return result;
}

PluginGUI* FCDPlugin::createSampleSource(const QString& sourceName, const QByteArray& address)
{
	if(sourceName == "org.osmocom.sdr.samplesource.fcd") {
		FCDGui* gui = new FCDGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	} else {
		return NULL;
	}
}
