#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gnuradioplugin.h"
#include "gnuradiogui.h"

const PluginDescriptor GNURadioPlugin::m_pluginDescriptor = {
	QString("GR-OsmoSDR Input"),
	QString("1.0"),
	QString("(c) Edouard Griffiths, F4EXB"),
	QString("https://github.com/f4exb/rtl-sdrangelove/tree/f4exb"),
	true,
	QString("https://github.com/f4exb/rtl-sdrangelove/tree/f4exb")
};

GNURadioPlugin::GNURadioPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& GNURadioPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void GNURadioPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	m_pluginAPI->registerSampleSource("org.osmocom.sdr.samplesource.gr-osmosdr", this);
}

PluginInterface::SampleSourceDevices GNURadioPlugin::enumSampleSources()
{
	SampleSourceDevices result;

	result.append(SampleSourceDevice("GNURadio OsmoSDR Driver", "org.osmocom.sdr.samplesource.gr-osmosdr", QByteArray()));

	return result;
}

PluginGUI* GNURadioPlugin::createSampleSource(const QString& sourceName, const QByteArray& address)
{
	if(sourceName == "org.osmocom.sdr.samplesource.gr-osmosdr") {
		GNURadioGui* gui = new GNURadioGui(m_pluginAPI);
		m_pluginAPI->setInputGUI(gui);
		return gui;
	} else {
		return NULL;
	}
}
