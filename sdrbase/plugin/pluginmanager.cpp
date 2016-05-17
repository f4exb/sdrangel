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

PluginManager::PluginManager(MainWindow* mainWindow, QObject* parent) :
	QObject(parent),
	m_pluginAPI(this, mainWindow),
	m_mainWindow(mainWindow)//,
//	m_sampleSourceId(),
//	m_sampleSourceSerial(),
//	m_sampleSourceSequence(0),
//	m_sampleSourcePluginGUI(NULL)
{
}

PluginManager::~PluginManager()
{
//	freeAll();
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

void PluginManager::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleSource "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with source name " << sourceName.toStdString().c_str();

	m_sampleSourceRegistrations.append(SampleSourceRegistration(sourceName, plugin));
}

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

//    deviceAPI->stopAcquisition();
//
//    if(m_sampleSourcePluginGUI != NULL) {
//        deviceAPI->stopAcquisition();
//        deviceAPI->setSource(0);
//        m_sampleSourcePluginGUI->destroy();
//        m_sampleSourcePluginGUI = NULL;
//        m_sampleSourceId.clear();
//    }
//
//	m_sampleSourceId = m_sampleSourceDevices[index].m_sourceId;
//	m_sampleSourceSerial = m_sampleSourceDevices[index].m_sourceSerial;
//	m_sampleSourceSequence = m_sampleSourceDevices[index].m_sourceSequence;

    qDebug() << "PluginManager::selectSampleSourceByIndex: m_sampleSource at index " << index
            << " id: " << m_sampleSourceDevices[index].m_sourceId.toStdString().c_str()
            << " ser: " << m_sampleSourceDevices[index].m_sourceSerial.toStdString().c_str()
            << " seq: " << m_sampleSourceDevices[index].m_sourceSequence;

    deviceAPI->stopAcquisition();
    deviceAPI->setSampleSourcePluginGUI(0); // this effectively destroys the previous GUI if it exists

	QWidget *gui;
	PluginGUI *pluginGUI = m_sampleSourceDevices[index].m_plugin->createSampleSourcePluginGUI(m_sampleSourceDevices[index].m_sourceId, &gui, deviceAPI);

	//	m_sampleSourcePluginGUI = pluginGUI;
	deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_sourceSequence);
	deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_sourceId);
	deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_sourceSerial);
	deviceAPI->setSampleSourcePluginGUI(pluginGUI);
	deviceAPI->setInputGUI(gui, m_sampleSourceDevices[index].m_displayName);

	return index;
}

int PluginManager::selectFirstSampleSource(const QString& sourceId, DeviceAPI *deviceAPI)
{
	qDebug("PluginManager::selectFirstSampleSource by id: [%s]", qPrintable(sourceId));

	int index = -1;

//	deviceAPI->stopAcquisition();
//
//	if(m_sampleSourcePluginGUI != NULL) {
//	    deviceAPI->stopAcquisition();
//	    deviceAPI->setSource(0);
//		m_sampleSourcePluginGUI->destroy();
//		m_sampleSourcePluginGUI = NULL;
//		m_sampleSourceId.clear();
//	}

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

//	m_sampleSourceId = m_sampleSourceDevices[index].m_sourceId;
//	m_sampleSourceSerial = m_sampleSourceDevices[index].m_sourceSerial;
//	m_sampleSourceSequence = m_sampleSourceDevices[index].m_sourceSequence;

    qDebug() << "PluginManager::selectFirstSampleSource: m_sampleSource at index " << index
            << " id: " << m_sampleSourceDevices[index].m_sourceId.toStdString().c_str()
            << " ser: " << m_sampleSourceDevices[index].m_sourceSerial.toStdString().c_str()
            << " seq: " << m_sampleSourceDevices[index].m_sourceSequence;

    deviceAPI->stopAcquisition();
    deviceAPI->setSampleSourcePluginGUI(0); // this effectively destroys the previous GUI if it exists

    QWidget *gui;
	PluginGUI *pluginGUI = m_sampleSourceDevices[index].m_plugin->createSampleSourcePluginGUI(m_sampleSourceDevices[index].m_sourceId, &gui, deviceAPI);

	//	m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_sourceSequence);
    deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_sourceId);
    deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_sourceSerial);
    deviceAPI->setSampleSourcePluginGUI(pluginGUI);
    deviceAPI->setInputGUI(gui, m_sampleSourceDevices[index].m_displayName);

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

//	m_sampleSourceId = m_sampleSourceDevices[index].m_sourceId;
//	m_sampleSourceSerial = m_sampleSourceDevices[index].m_sourceSerial;
//	m_sampleSourceSequence = m_sampleSourceDevices[index].m_sourceSequence;

    qDebug() << "PluginManager::selectSampleSourceBySequence: m_sampleSource at index " << index
            << " id: " << m_sampleSourceDevices[index].m_sourceId.toStdString().c_str()
            << " ser: " << m_sampleSourceDevices[index].m_sourceSerial.toStdString().c_str()
            << " seq: " << m_sampleSourceDevices[index].m_sourceSequence;

    deviceAPI->stopAcquisition();
    deviceAPI->setSampleSourcePluginGUI(0); // this effectively destroys the previous GUI if it exists

    QWidget *gui;
	PluginGUI *pluginGUI = m_sampleSourceDevices[index].m_plugin->createSampleSourcePluginGUI(m_sampleSourceDevices[index].m_sourceId, &gui, deviceAPI);

	//	m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_sourceSequence);
    deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_sourceId);
    deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_sourceSerial);
    deviceAPI->setSampleSourcePluginGUI(pluginGUI);
    deviceAPI->setInputGUI(gui, m_sampleSourceDevices[index].m_displayName);

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
