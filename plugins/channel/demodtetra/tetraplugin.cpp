#include <QtPlugin>
#include <QAction>
#include "plugin/pluginapi.h"
#include "tetraplugin.h"
#include "tetrademodgui.h"

const PluginDescriptor TetraPlugin::m_pluginDescriptor = {
	QString("Tetra Demodulator"),
	QString("---"),
	QString("(c) maintech GmbH (written by Christian Daniel)"),
	QString("http://www.maintech.de"),
	true,
	QString("http://www.maintech.de")
};

TetraPlugin::TetraPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& TetraPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void TetraPlugin::initPlugin(PluginAPI* pluginAPI)
{
	m_pluginAPI = pluginAPI;

	// register Tetra demodulator
	QAction* action = new QAction(tr("&Tetra"), this);
	connect(action, SIGNAL(triggered()), this, SLOT(createInstanceTetra()));
	m_pluginAPI->registerDemodulator("de.maintech.sdrangelove.demod.tetra", this, action);
}

PluginGUI* TetraPlugin::createDemod(const QString& demodName)
{
	if(demodName == "de.maintech.sdrangelove.demod.tetra") {
		PluginGUI* gui = TetraDemodGUI::create(m_pluginAPI);
		m_pluginAPI->registerDemodulatorInstance("de.maintech.sdrangelove.demod.tetra", gui);
		return gui;
	} else {
		return NULL;
	}
}

void TetraPlugin::createInstanceTetra()
{
	m_pluginAPI->registerDemodulatorInstance("de.maintech.sdrangelove.demod.tetra", TetraDemodGUI::create(m_pluginAPI));
}
