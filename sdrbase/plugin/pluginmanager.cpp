#include <QApplication>
#include <QPluginLoader>
#include <QComboBox>
#include <cstdio>

#include "plugin/pluginmanager.h"
#include "plugin/plugingui.h"
#include "device/deviceapi.h"
#include "settings/preset.h"
#include "mainwindow.h"
#include "gui/glspectrum.h"
#include "dsp/dspdeviceengine.h"
#include "util/message.h"

#include <QDebug>

PluginManager::PluginManager(MainWindow* mainWindow, uint deviceTabIndex, DSPDeviceEngine* dspDeviceEngine, QObject* parent) :
	QObject(parent),
	m_pluginAPI(this, mainWindow),
	m_mainWindow(mainWindow),
	m_deviceTabIndex(deviceTabIndex),
	m_dspDeviceEngine(dspDeviceEngine),
	m_sampleSourceId(),
	m_sampleSourceSerial(),
	m_sampleSourceSequence(0),
	m_sampleSourcePluginGUI(NULL)
{
}

PluginManager::~PluginManager()
{
	freeAll();
}

void PluginManager::loadPlugins()
{
	QString applicationDirPath = QApplication::instance()->applicationDirPath();
	QString applicationLibPath = applicationDirPath + "/../lib";
	qDebug() << "PluginManager::loadPlugins: " << qPrintable(applicationDirPath) << ", " << qPrintable(applicationLibPath);

	QDir pluginsBinDir = QDir(applicationDirPath);
	QDir pluginsLibDir = QDir(applicationLibPath);

	loadPlugins(pluginsBinDir);
	loadPlugins(pluginsLibDir);

	qSort(m_plugins);

	for (Plugins::const_iterator it = m_plugins.begin(); it != m_plugins.end(); ++it)
	{
		it->pluginInterface->initPlugin(&m_pluginAPI);
	}

	updateSampleSourceDevices();
}

void PluginManager::registerChannel(const QString& channelName, PluginInterface* plugin)
{
	m_channelRegistrations.append(PluginAPI::ChannelRegistration(channelName, plugin));
}

//void PluginManager::registerChannelInstance(const QString& channelName, PluginGUI* pluginGUI)
//{
//	m_channelInstanceRegistrations.append(ChannelInstanceRegistration(channelName, pluginGUI));
//	renameChannelInstances();
//}
//
//void PluginManager::removeChannelInstance(PluginGUI* pluginGUI)
//{
//	for(ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it) {
//		if(it->m_gui == pluginGUI) {
//			m_channelInstanceRegistrations.erase(it);
//			break;
//		}
//	}
//	renameChannelInstances();
//}

void PluginManager::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleSource "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with source name " << sourceName.toStdString().c_str();

	m_sampleSourceRegistrations.append(SampleSourceRegistration(sourceName, plugin));
}

void PluginManager::loadChannelSettings(const Preset* preset, DeviceAPI *deviceAPI)
{
	fprintf(stderr, "PluginManager::loadSettings: Loading preset [%s | %s]\n", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

	// copy currently open channels and clear list
	ChannelInstanceRegistrations openChannels = m_channelInstanceRegistrations;
	m_channelInstanceRegistrations.clear();

	for(int i = 0; i < preset->getChannelCount(); i++)
	{
		const Preset::ChannelConfig& channelConfig = preset->getChannelConfig(i);
		ChannelInstanceRegistration reg;

		// if we have one instance available already, use it

		for(int i = 0; i < openChannels.count(); i++)
		{
			qDebug("PluginManager::loadSettings: channels compare [%s] vs [%s]", qPrintable(openChannels[i].m_channelName), qPrintable(channelConfig.m_channel));

			if(openChannels[i].m_channelName == channelConfig.m_channel)
			{
				qDebug("PluginManager::loadSettings: channel [%s] found", qPrintable(openChannels[i].m_channelName));
				reg = openChannels.takeAt(i);
				m_channelInstanceRegistrations.append(reg);
				break;
			}
		}

		// if we haven't one already, create one

		if(reg.m_gui == NULL)
		{
			for(int i = 0; i < m_channelRegistrations.count(); i++)
			{
				if(m_channelRegistrations[i].m_channelName == channelConfig.m_channel)
				{
					qDebug("PluginManager::loadSettings: creating new channel [%s]", qPrintable(channelConfig.m_channel));
					reg = ChannelInstanceRegistration(channelConfig.m_channel, m_channelRegistrations[i].m_plugin->createChannel(channelConfig.m_channel, deviceAPI));
					break;
				}
			}
		}

		if(reg.m_gui != NULL)
		{
			qDebug("PluginManager::loadSettings: deserializing channel [%s]", qPrintable(channelConfig.m_channel));
			reg.m_gui->deserialize(channelConfig.m_config);
		}
	}

	// everything, that is still "available" is not needed anymore
	for(int i = 0; i < openChannels.count(); i++)
	{
		qDebug("PluginManager::loadSettings: destroying spare channel [%s]", qPrintable(openChannels[i].m_channelName));
		openChannels[i].m_gui->destroy();
	}

	renameChannelInstances();

//	loadSourceSettings(preset); // FIXME
}

//void PluginManager::loadSourceSettings(const Preset* preset)
//{
//	fprintf(stderr, "PluginManager::loadSourceSettings: Loading preset [%s | %s]\n", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
//
//	if(m_sampleSourcePluginGUI != 0)
//	{
//		const QByteArray* sourceConfig = preset->findBestSourceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence);
//
//		if (sourceConfig != 0)
//		{
//			qDebug() << "PluginManager::loadSettings: deserializing source " << qPrintable(m_sampleSourceId);
//			m_sampleSourcePluginGUI->deserialize(*sourceConfig);
//		}
//
//		qint64 centerFrequency = preset->getCenterFrequency();
//		m_sampleSourcePluginGUI->setCenterFrequency(centerFrequency);
//	}
//}

// sort by increasing delta frequency and type (i.e. name)
bool PluginManager::ChannelInstanceRegistration::operator<(const ChannelInstanceRegistration& other) const {
	if (m_gui && other.m_gui) {
		if (m_gui->getCenterFrequency() == other.m_gui->getCenterFrequency()) {
			return m_gui->getName() < other.m_gui->getName();
		} else {
			return m_gui->getCenterFrequency() < other.m_gui->getCenterFrequency();
		}
	} else {
		return false;
	}
}

void PluginManager::saveSettings(Preset* preset)
{
	qDebug("PluginManager::saveSettings");
//	saveSourceSettings(preset); // FIXME

	qSort(m_channelInstanceRegistrations.begin(), m_channelInstanceRegistrations.end()); // sort by increasing delta frequency and type

	for(int i = 0; i < m_channelInstanceRegistrations.count(); i++)
	{
		preset->addChannel(m_channelInstanceRegistrations[i].m_channelName, m_channelInstanceRegistrations[i].m_gui->serialize());
	}
}

//void PluginManager::saveSourceSettings(Preset* preset)
//{
//	qDebug("PluginManager::saveSourceSettings");
//
//	if(m_sampleSourcePluginGUI != NULL)
//	{
//		preset->addOrUpdateSourceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence, m_sampleSourcePluginGUI->serialize());
//		//preset->setSourceConfig(m_sampleSourceId, m_sampleSourceSerial, m_sampleSourceSequence, m_sampleSourcePluginGUI->serialize());
//		preset->setCenterFrequency(m_sampleSourcePluginGUI->getCenterFrequency());
//	}
//}

void PluginManager::freeAll()
{
    m_dspDeviceEngine->stopAcquistion();

	while(!m_channelInstanceRegistrations.isEmpty()) {
		ChannelInstanceRegistration reg(m_channelInstanceRegistrations.takeLast());
		reg.m_gui->destroy();
	}

	if(m_sampleSourcePluginGUI != NULL) {
	    m_dspDeviceEngine->setSource(NULL);
		m_sampleSourcePluginGUI->destroy();
		m_sampleSourcePluginGUI = NULL;
		m_sampleSourceId.clear();
	}
}

//bool PluginManager::handleMessage(const Message& message)
//{
//	if (m_sampleSourcePluginGUI != 0)
//	{
//		if ((message.getDestination() == 0) || (message.getDestination() == m_sampleSourcePluginGUI))
//		{
//			if (m_sampleSourcePluginGUI->handleMessage(message))
//			{
//				return true;
//			}
//		}
//	}
//
//	for (ChannelInstanceRegistrations::iterator it = m_channelInstanceRegistrations.begin(); it != m_channelInstanceRegistrations.end(); ++it)
//	{
//		if ((message.getDestination() == 0) || (message.getDestination() == it->m_gui))
//		{
//			if (it->m_gui->handleMessage(message))
//			{
//				return true;
//			}
//		}
//	}
//
//	return false;
//}

void PluginManager::updateSampleSourceDevices()
{
	m_sampleSourceDevices.clear();

	for(int i = 0; i < m_sampleSourceRegistrations.count(); ++i)
	{
		PluginInterface::SampleSourceDevices ssd = m_sampleSourceRegistrations[i].m_plugin->enumSampleSources();

		for(int j = 0; j < ssd.count(); ++j)
		{
			m_sampleSourceDevices.append(SampleSourceDevice(m_sampleSourceRegistrations[i].m_plugin,
					ssd[j].displayedName,
					ssd[j].id,
					ssd[j].serial,
					ssd[j].sequence));
		}
	}
}

void PluginManager::fillSampleSourceSelector(QComboBox* comboBox)
{
	comboBox->clear();

	for(int i = 0; i < m_sampleSourceDevices.count(); i++)
	{
		comboBox->addItem(m_sampleSourceDevices[i].m_displayName, i);
	}
}

int PluginManager::selectSampleSourceByIndex(int index, DeviceAPI *deviceAPI)
{
	qDebug("PluginManager::selectSampleSourceByIndex: index: %d", index);

	deviceAPI->stopAcquisition();

	if(m_sampleSourcePluginGUI != NULL) {
	    deviceAPI->stopAcquisition();
        deviceAPI->setSource(0);
		m_sampleSourcePluginGUI->destroy();
		m_sampleSourcePluginGUI = NULL;
		m_sampleSourceId.clear();
	}

	if (m_sampleSourceDevices.count() == 0)
	{
		return -1;
	}

	if (index < 0)
	{
		return -1;
	}

	if (index >= m_sampleSourceDevices.count())
	{
		index = 0;
	}

	m_sampleSourceId = m_sampleSourceDevices[index].m_sourceId;
	m_sampleSourceSerial = m_sampleSourceDevices[index].m_sourceSerial;
	m_sampleSourceSequence = m_sampleSourceDevices[index].m_sourceSequence;

	qDebug() << "PluginManager::selectSampleSourceByIndex: m_sampleSource at index " << index
			<< " id: " << m_sampleSourceId.toStdString().c_str()
			<< " ser: " << m_sampleSourceSerial.toStdString().c_str()
			<< " seq: " << m_sampleSourceSequence;

	PluginGUI *pluginGUI = m_sampleSourceDevices[index].m_plugin->createSampleSourcePluginGUI(m_sampleSourceId, m_sampleSourceDevices[index].m_displayName, deviceAPI);
	m_sampleSourcePluginGUI = pluginGUI;
	deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_sourceSequence);
	deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_sourceId);
	deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_sourceSerial);
	deviceAPI->setSampleSourcePluginGUI(pluginGUI);

	return index;
}

int PluginManager::selectFirstSampleSource(const QString& sourceId, DeviceAPI *deviceAPI)
{
	qDebug("PluginManager::selectFirstSampleSource by id: [%s]", qPrintable(sourceId));

	int index = -1;

	deviceAPI->stopAcquisition();

	if(m_sampleSourcePluginGUI != NULL) {
	    deviceAPI->stopAcquisition();
	    deviceAPI->setSource(0);
		m_sampleSourcePluginGUI->destroy();
		m_sampleSourcePluginGUI = NULL;
		m_sampleSourceId.clear();
	}

	for (int i = 0; i < m_sampleSourceDevices.count(); i++)
	{
		qDebug("*** %s vs %s", qPrintable(m_sampleSourceDevices[i].m_sourceId), qPrintable(sourceId));

		if(m_sampleSourceDevices[i].m_sourceId == sourceId)
		{
			index = i;
			break;
		}
	}

	if(index == -1)
	{
		if(m_sampleSourceDevices.count() > 0)
		{
			index = 0;
		}
		else
		{
			return -1;
		}
	}

	m_sampleSourceId = m_sampleSourceDevices[index].m_sourceId;
	m_sampleSourceSerial = m_sampleSourceDevices[index].m_sourceSerial;
	m_sampleSourceSequence = m_sampleSourceDevices[index].m_sourceSequence;

	qDebug() << "m_sampleSource at index " << index
			<< " id: " << m_sampleSourceId.toStdString().c_str()
			<< " ser: " << m_sampleSourceSerial.toStdString().c_str()
			<< " seq: " << m_sampleSourceSequence;

	PluginGUI *pluginGUI = m_sampleSourceDevices[index].m_plugin->createSampleSourcePluginGUI(m_sampleSourceId, m_sampleSourceDevices[index].m_displayName, deviceAPI);
	m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_sourceSequence);
    deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_sourceId);
    deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_sourceSerial);
    deviceAPI->setSampleSourcePluginGUI(pluginGUI);

	return index;
}

int PluginManager::selectSampleSourceBySerialOrSequence(const QString& sourceId, const QString& sourceSerial, int sourceSequence, DeviceAPI *deviceAPI)
{
	qDebug("PluginManager::selectSampleSourceBySequence by sequence: id: %s ser: %s seq: %d", qPrintable(sourceId), qPrintable(sourceSerial), sourceSequence);

	int index = -1;
	int index_matchingSequence = -1;
	int index_firstOfKind = -1;

	for (int i = 0; i < m_sampleSourceDevices.count(); i++)
	{
		if (m_sampleSourceDevices[i].m_sourceId == sourceId)
		{
			index_firstOfKind = i;

			if (m_sampleSourceDevices[i].m_sourceSerial == sourceSerial)
			{
				index = i; // exact match
				break;
			}

			if (m_sampleSourceDevices[i].m_sourceSequence == sourceSequence)
			{
				index_matchingSequence = i;
			}
		}
	}

	if(index == -1) // no exact match
	{
		if (index_matchingSequence == -1) // no matching sequence
		{
			if (index_firstOfKind == -1) // no matching device type
			{
				if(m_sampleSourceDevices.count() > 0) // take first if any
				{
					index = 0;
				}
				else
				{
					return -1; // return if no device attached
				}
			}
			else
			{
				index = index_firstOfKind; // take first that matches device type
			}
		}
		else
		{
			index = index_matchingSequence; // take the one that matches the sequence in the device type
		}
	}

	m_sampleSourceId = m_sampleSourceDevices[index].m_sourceId;
	m_sampleSourceSerial = m_sampleSourceDevices[index].m_sourceSerial;
	m_sampleSourceSequence = m_sampleSourceDevices[index].m_sourceSequence;

	qDebug() << "PluginManager::selectSampleSourceBySequence by sequence:  m_sampleSource at index " << index
			<< " id: " << qPrintable(m_sampleSourceId)
			<< " ser: " << qPrintable(m_sampleSourceSerial)
			<< " seq: " << m_sampleSourceSequence;

	PluginGUI *pluginGUI = m_sampleSourceDevices[index].m_plugin->createSampleSourcePluginGUI(m_sampleSourceId, m_sampleSourceDevices[index].m_displayName, deviceAPI);
	m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_sourceSequence);
    deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_sourceId);
    deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_sourceSerial);
    deviceAPI->setSampleSourcePluginGUI(pluginGUI);

	return index;
}

void PluginManager::loadPlugins(const QDir& dir)
{
	QDir pluginsDir(dir);

	foreach (QString fileName, pluginsDir.entryList(QDir::Files))
	{
		if (fileName.endsWith(".so") || fileName.endsWith(".dll"))
		{
			qDebug() << "PluginManager::loadPlugins: fileName: " << qPrintable(fileName);

			QPluginLoader* loader = new QPluginLoader(pluginsDir.absoluteFilePath(fileName));
			PluginInterface* plugin = qobject_cast<PluginInterface*>(loader->instance());

			if (loader->isLoaded())
			{
				qWarning("PluginManager::loadPlugins: loaded plugin %s", qPrintable(fileName));
			}
			else
			{
				qWarning() << "PluginManager::loadPlugins: " << qPrintable(loader->errorString());
			}

			if (plugin != 0)
			{
				m_plugins.append(Plugin(fileName, loader, plugin));
			}
			else
			{
				loader->unload();
			}

			delete loader; // Valgrind memcheck
		}
	}

	// recursive calls on subdirectories

	foreach (QString dirName, pluginsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		loadPlugins(pluginsDir.absoluteFilePath(dirName));
	}
}

void PluginManager::renameChannelInstances()
{
	for(int i = 0; i < m_channelInstanceRegistrations.count(); i++) {
		m_channelInstanceRegistrations[i].m_gui->setName(QString("%1:%2").arg(m_channelInstanceRegistrations[i].m_channelName).arg(i));
	}
}

void PluginManager::populateChannelComboBox(QComboBox *channels)
{
    for(PluginAPI::ChannelRegistrations::iterator it = m_channelRegistrations.begin(); it != m_channelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDescipror = it->m_plugin->getPluginDescriptor();
        channels->addItem(pluginDescipror.displayedName);
    }
}

void PluginManager::createChannelInstance(int channelPluginIndex, DeviceAPI *deviceAPI)
{
    if (channelPluginIndex < m_channelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_channelRegistrations[channelPluginIndex].m_plugin;
        pluginInterface->createChannel(m_channelRegistrations[channelPluginIndex].m_channelName, deviceAPI);
    }
}
