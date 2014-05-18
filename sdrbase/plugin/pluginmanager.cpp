#include <QApplication>
#include <QPluginLoader>
#include <QComboBox>
#include "plugin/pluginmanager.h"
#include "plugin/plugingui.h"
#include "settings/preset.h"
#include "mainwindow.h"
#include "dsp/dspengine.h"
#include "dsp/samplesource/samplesource.h"

PluginManager::PluginManager(MainWindow* mainWindow, DSPEngine* dspEngine, QObject* parent) :
	QObject(parent),
	m_pluginAPI(this, mainWindow, dspEngine),
	m_mainWindow(mainWindow),
	m_dspEngine(dspEngine),
	m_sampleSource(),
	m_sampleSourceInstance(NULL)
{
}

PluginManager::~PluginManager()
{
	freeAll();
}

void PluginManager::loadPlugins()
{
	QDir pluginsDir = QDir(QApplication::instance()->applicationDirPath());

	loadPlugins(pluginsDir);

	qSort(m_plugins);

	for(Plugins::const_iterator it = m_plugins.begin(); it != m_plugins.end(); ++it)
		it->plugin->initPlugin(&m_pluginAPI);

	updateSampleSourceDevices();
}

void PluginManager::registerChannel(const QString& channelName, PluginInterface* plugin, QAction* action)
{
	m_channelRegistrations.append(ChannelRegistration(channelName, plugin));
	m_mainWindow->addChannelCreateAction(action);
}

void PluginManager::registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI)
{
	m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI));
	renameChannelInstances();
}

void PluginManager::addChannelRollup(QWidget* pluginGUI)
{
	m_mainWindow->addChannelRollup(pluginGUI);
}

void PluginManager::removeChannelInstance(PluginGUI* pluginGUI)
{
	for(ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it) {
		if(it->m_gui == pluginGUI) {
			m_channelInstanceRegistrations.erase(it);
			break;
		}
	}
	renameChannelInstances();
}

void PluginManager::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	m_sampleSourceRegistrations.append(SampleSourceRegistration(sourceName, plugin));
}

void PluginManager::loadSettings(const Preset* preset)
{
	qDebug("-------- [%s | %s] --------", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

	// copy currently open channels and clear list
	ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
	m_channelInstanceRegistrations.clear();

	for(int i = 0; i < preset->getChannelCount(); i++) {
		const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
		ChannelInstanceRegistration reg;
		// if we have one instance available already, use it
		for(int i = 0; i < openChannels.count(); i++) {
			qDebug("compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channel));
			if(openChannels[i].m_channelName == channelConfig.m_channel) {
				qDebug("channel [%s] found", qPrintable(openChannels[i].m_channelName));
				reg = openChannels.takeAt(i);
				m_channelInstanceRegistrations.append(reg);
				break;
			}
		}
		// if we haven't one already, create one
		if(reg.m_gui == NULL) {
			for(int i = 0; i < m_channelRegistrations.count(); i++) {
				if(m_channelRegistrations[i].m_channelName == channelConfig.m_channel) {
					qDebug("creating new channel [%s]", qPrintable(channelConfig.m_channel));
					reg = ChannelInstanceRegistration(channelConfig.m_channel, m_channelRegistrations[i].m_plugin->createChannel(channelConfig.m_channel));
					break;
				}
			}
		}
		if(reg.m_gui != NULL)
			reg.m_gui->deserialize(channelConfig.m_config);
	}

	// everything, that is still "available" is not needed anymore
	for(int i = 0; i < openChannels.count(); i++) {
		qDebug("destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
		openChannels[i].m_gui->destroy();
	}

	renameChannelInstances();

	if(m_sampleSourceInstance != NULL) {
		m_sampleSourceInstance->deserializeGeneral(preset->getSourceGeneralConfig());
		if(m_sampleSource == preset->getSource()) {
			m_sampleSourceInstance->deserialize(preset->getSourceConfig());
		}
	}
}

void PluginManager::saveSettings(Preset* preset) const
{
	if(m_sampleSourceInstance != NULL) {
		preset->setSourceConfig(m_sampleSource, m_sampleSourceInstance->serializeGeneral(), m_sampleSourceInstance->serialize());
		preset->setCenterFrequency(m_sampleSourceInstance->getCenterFrequency());
	} else {
		preset->setSourceConfig(QString::null, QByteArray(), QByteArray());
	}
	for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
		preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
}

void PluginManager::freeAll()
{
	m_dspEngine->stopAcquistion();

	while(!m_channelInstanceRegistrations.isEmpty()) {
		ChannelInstanceRegistration reg(m_channelInstanceRegistrations.takeLast());
		reg.m_gui->destroy();
	}

	if(m_sampleSourceInstance != NULL) {
		m_dspEngine->setSource(NULL);
		m_sampleSourceInstance->destroy();
		m_sampleSourceInstance = NULL;
		m_sampleSource.clear();
	}
}

bool PluginManager::handleMessage(Message* message)
{
	if(m_sampleSourceInstance != NULL) {
		if((message->getDestination() == NULL) || (message->getDestination() == m_sampleSourceInstance)) {
			if(m_sampleSourceInstance->handleMessage(message))
				return true;
		}
	}

	for(ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it) {
		if((message->getDestination() == NULL) || (message->getDestination() == it->m_gui)) {
			if(it->m_gui->handleMessage(message))
				return true;
		}
	}
	return false;
}

void PluginManager::updateSampleSourceDevices()
{
	m_sampleSourceDevices.clear();
	for(int i = 0; i < m_sampleSourceRegistrations.count(); ++i) {
		PluginInterface::SampleSourceDevices ssd = m_sampleSourceRegistrations[i].m_plugin->enumSampleSources();
		for(int j = 0; j < ssd.count(); ++j)
			m_sampleSourceDevices.append(SampleSourceDevice(m_sampleSourceRegistrations[i].m_plugin, ssd[j].displayedName, ssd[j].name, ssd[j].address));
	}
}

void PluginManager::fillSampleSourceSelector(QComboBox* comboBox)
{
	comboBox->clear();
	for(int i = 0; i < m_sampleSourceDevices.count(); i++)
		comboBox->addItem(m_sampleSourceDevices[i].m_displayName, i);
}

int PluginManager::selectSampleSource(int index)
{
	m_dspEngine->stopAcquistion();

	if(m_sampleSourceInstance != NULL) {
		m_dspEngine->stopAcquistion();
		m_dspEngine->setSource(NULL);
		m_sampleSourceInstance->destroy();
		m_sampleSourceInstance = NULL;
		m_sampleSource.clear();
	}

	if(index == -1) {
		if(!m_sampleSource.isEmpty()) {
			for(int i = 0; i < m_sampleSourceDevices.count(); i++) {
				if(m_sampleSourceDevices[i].m_sourceName == m_sampleSource) {
					index = i;
					break;
				}
			}
		}
		if(index == -1) {
			if(m_sampleSourceDevices.count() > 0)
				index = 0;
		}
	}
	if(index == -1)
		return -1;

	m_sampleSource = m_sampleSourceDevices[index].m_sourceName;
	m_sampleSourceInstance = m_sampleSourceDevices[index].m_plugin->createSampleSource(m_sampleSource, m_sampleSourceDevices[index].m_address);
	return index;
}

int PluginManager::selectSampleSource(const QString& source)
{
	int index = -1;

	m_dspEngine->stopAcquistion();

	if(m_sampleSourceInstance != NULL) {
		m_dspEngine->stopAcquistion();
		m_dspEngine->setSource(NULL);
		m_sampleSourceInstance->destroy();
		m_sampleSourceInstance = NULL;
		m_sampleSource.clear();
	}

	qDebug("finding sample source [%s]", qPrintable(source));
	for(int i = 0; i < m_sampleSourceDevices.count(); i++) {
		qDebug("*** %s vs %s", qPrintable(m_sampleSourceDevices[i].m_sourceName), qPrintable(source));
		if(m_sampleSourceDevices[i].m_sourceName == source) {
			index = i;
			break;
		}
	}
	if(index == -1) {
		if(m_sampleSourceDevices.count() > 0)
			index = 0;
	}
	if(index == -1)
		return -1;

	m_sampleSource = m_sampleSourceDevices[index].m_sourceName;
	m_sampleSourceInstance = m_sampleSourceDevices[index].m_plugin->createSampleSource(m_sampleSource, m_sampleSourceDevices[index].m_address);
	return index;
}

void PluginManager::loadPlugins(const QDir& dir)
{
	QDir pluginsDir(dir);
	foreach(QString fileName, pluginsDir.entryList(QDir::Files)) {
		QPluginLoader* loader = new QPluginLoader(pluginsDir.absoluteFilePath(fileName));
		PluginInterface* plugin = qobject_cast<PluginInterface*>(loader->instance());
		if(loader->isLoaded())
			qDebug("loaded plugin %s", qPrintable(fileName));
		if(plugin != NULL) {
			m_plugins.append(Plugin(fileName, loader, plugin));
		} else {
			loader->unload();
			delete loader;
		}
	}
	foreach(QString dirName, pluginsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		loadPlugins(pluginsDir.absoluteFilePath(dirName));
}

void PluginManager::renameChannelInstances()
{
	for(int i = 0; i < m_channelInstanceRegistrations.count(); i++) {
		m_channelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i].m_channelName).arg(i));
	}
}
