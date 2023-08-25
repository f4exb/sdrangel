#include <QSettings>
#include <QStringList>
#include <QDebug>

#include <algorithm>

#include "settings/mainsettings.h"
#include "commands/command.h"
#include "audio/audiodevicemanager.h"

MainSettings::MainSettings() :
    m_audioDeviceManager(nullptr)
{
	resetToDefaults();
    qInfo("MainSettings::MainSettings: settings file: format: %d location: %s", getFileFormat(), qPrintable(getFileLocation()));
}

MainSettings::~MainSettings()
{
	for (int i = 0; i < m_presets.count(); ++i)
	{
		delete m_presets[i];
	}

	for (int i = 0; i < m_commands.count(); ++i)
    {
        delete m_commands[i];
    }

	for (int i = 0; i < m_featureSetPresets.count(); ++i)
    {
        delete m_featureSetPresets[i];
    }
    for (int i = 0; i < m_pluginPresets.count(); ++i)
    {
        delete m_pluginPresets[i];
    }
}

QString MainSettings::getFileLocation() const
{
    QSettings s;
    return s.fileName();
}

int MainSettings::getFileFormat() const
{
    QSettings s;
    return (int) s.format();
}

void MainSettings::load()
{
	QSettings s;

	m_preferences.deserialize(qUncompress(QByteArray::fromBase64(s.value("preferences").toByteArray())));
	// m_workingPreset.deserialize(qUncompress(QByteArray::fromBase64(s.value("current").toByteArray())));
	// m_workingFeatureSetPreset.deserialize(qUncompress(QByteArray::fromBase64(s.value("current-featureset").toByteArray())));
    m_workingConfiguration.deserialize(qUncompress(QByteArray::fromBase64(s.value("current-configuration").toByteArray())));

	if (m_audioDeviceManager) {
	    m_audioDeviceManager->deserialize(qUncompress(QByteArray::fromBase64(s.value("audio").toByteArray())));
	}

	QStringList groups = s.childGroups();

	for (int i = 0; i < groups.size(); ++i)
	{
		if (groups[i].startsWith("preset"))
		{
			s.beginGroup(groups[i]);
			Preset* preset = new Preset;

			if (preset->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
			{
				m_presets.append(preset);
			}
			else
			{
				delete preset;
			}

			s.endGroup();
		}
		else if (groups[i].startsWith("command"))
        {
            s.beginGroup(groups[i]);
            Command* command = new Command;

            if (command->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
            {
                m_commands.append(command);
            }
            else
            {
                delete command;
            }

            s.endGroup();
        }
		else if (groups[i].startsWith("featureset"))
        {
            s.beginGroup(groups[i]);
            FeatureSetPreset* featureSetPreset = new FeatureSetPreset;

            if (featureSetPreset->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
            {
                m_featureSetPresets.append(featureSetPreset);
            }
            else
            {
                delete featureSetPreset;
            }

            s.endGroup();
        }
        else if (groups[i].startsWith("pluginpreset"))
        {
            s.beginGroup(groups[i]);
            PluginPreset* pluginPreset = new PluginPreset;

            if (pluginPreset->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
            {
                m_pluginPresets.append(pluginPreset);
            }
            else
            {
                delete pluginPreset;
            }

            s.endGroup();
        }
		else if (groups[i].startsWith("configuration"))
        {
            s.beginGroup(groups[i]);
            Configuration* configuration = new Configuration;

            if (configuration->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
            {
                m_configurations.append(configuration);
            }
            else
            {
                delete configuration;
            }

            s.endGroup();
        }
	}

    m_hardwareDeviceUserArgs.deserialize(qUncompress(QByteArray::fromBase64(s.value("hwDeviceUserArgs").toByteArray())));
}

void MainSettings::save() const
{
	QSettings s;

	s.setValue("preferences", qCompress(m_preferences.serialize()).toBase64());
    s.setValue("current-configuration", qCompress(m_workingConfiguration.serialize()).toBase64());

	if (m_audioDeviceManager) {
	    s.setValue("audio", qCompress(m_audioDeviceManager->serialize()).toBase64());
	}

	QStringList groups = s.childGroups();

	for(int i = 0; i < groups.size(); ++i)
	{
		if ((groups[i].startsWith("preset")) || (groups[i].startsWith("command")))
		{
			s.remove(groups[i]);
		}
	}

	for (int i = 0; i < m_presets.count(); ++i)
	{
		QString group = QString("preset-%1").arg(i + 1);
		s.beginGroup(group);
		s.setValue("data", qCompress(m_presets[i]->serialize()).toBase64());
		s.endGroup();
	}

    for (int i = 0; i < m_commands.count(); ++i)
    {
        QString group = QString("command-%1").arg(i + 1);
        s.beginGroup(group);
        s.setValue("data", qCompress(m_commands[i]->serialize()).toBase64());
        s.endGroup();
    }

	for (int i = 0; i < m_featureSetPresets.count(); ++i)
	{
		QString group = QString("featureset-%1").arg(i + 1);
		s.beginGroup(group);
		s.setValue("data", qCompress(m_featureSetPresets[i]->serialize()).toBase64());
		s.endGroup();
	}

    for (int i = 0; i < m_pluginPresets.count(); ++i)
    {
        QString group = QString("pluginpreset-%1").arg(i + 1);
        s.beginGroup(group);
        s.setValue("data", qCompress(m_pluginPresets[i]->serialize()).toBase64());
        s.endGroup();
    }

	for (int i = 0; i < m_configurations.count(); ++i)
	{
		QString group = QString("configuration-%1").arg(i + 1);
		s.beginGroup(group);
		s.setValue("data", qCompress(m_configurations[i]->serialize()).toBase64());
		s.endGroup();
	}

    s.setValue("hwDeviceUserArgs", qCompress(m_hardwareDeviceUserArgs.serialize()).toBase64());
}

void MainSettings::initialize()
{
    resetToDefaults();
    clearCommands();
    clearPresets();
    clearFeatureSetPresets();
    clearPluginPresets();
    clearConfigurations();
}

void MainSettings::resetToDefaults()
{
	m_preferences.resetToDefaults();
	m_workingPreset.resetToDefaults();
    m_workingFeatureSetPreset.resetToDefaults();
    m_workingPluginPreset.resetToDefaults();
    m_workingConfiguration.resetToDefaults();
}

// DeviceSet presets

Preset* MainSettings::newPreset(const QString& group, const QString& description)
{
	Preset* preset = new Preset();
	preset->setGroup(group);
	preset->setDescription(description);
	addPreset(preset);
	return preset;
}

void MainSettings::addPreset(Preset *preset)
{
    m_presets.append(preset);
}

void MainSettings::deletePreset(const Preset* preset)
{
	m_presets.removeAll((Preset*)preset);
	delete (Preset*)preset;
}

QByteArray MainSettings::serializePreset(const Preset* preset) const
{
    return preset->serialize();
}

bool MainSettings::deserializePreset(const QByteArray& blob, Preset* preset)
{
    return preset->deserialize(blob);
}

void MainSettings::deletePresetGroup(const QString& groupName)
{
    Presets::iterator it = m_presets.begin();

    while (it != m_presets.end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_presets.erase(it);
        } else {
            ++it;
        }
    }
}

void MainSettings::sortPresets()
{
    std::sort(m_presets.begin(), m_presets.end(), Preset::presetCompare);
}

void MainSettings::renamePresetGroup(const QString& oldGroupName, const QString& newGroupName)
{
    int nbPresets = getPresetCount();

    for (int i = 0; i < nbPresets; i++)
    {
        if (getPreset(i)->getGroup() == oldGroupName)
        {
            Preset *preset_mod = const_cast<Preset*>(getPreset(i));
            preset_mod->setGroup(newGroupName);
        }
    }
}

const Preset* MainSettings::getPreset(const QString& groupName, quint64 centerFrequency, const QString& description, const QString& type) const
{
    int nbPresets = getPresetCount();

    for (int i = 0; i < nbPresets; i++)
    {
        if ((getPreset(i)->getGroup() == groupName) &&
            (getPreset(i)->getCenterFrequency() == centerFrequency) &&
            (getPreset(i)->getDescription() == description))
        {
            if (type == "R" && getPreset(i)->isSourcePreset()) {
                return getPreset(i);
            } else if (type == "T" && getPreset(i)->isSinkPreset()) {
                return getPreset(i);
            } else if (type == "M" && getPreset(i)->isMIMOPreset()) {
                return getPreset(i);
            }
        }
    }

    return nullptr;
}

void MainSettings::clearPresets()
{
    foreach (Preset *preset, m_presets) {
        delete preset;
    }

    m_presets.clear();
}

// Commands

void MainSettings::addCommand(Command *command)
{
    m_commands.append(command);
}

void MainSettings::deleteCommand(const Command* command)
{
    m_commands.removeAll((Command*)command);
    delete (Command*)command;
}

void MainSettings::deleteCommandGroup(const QString& groupName)
{
    Commands::iterator it = m_commands.begin();

    while (it != m_commands.end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_commands.erase(it);
        } else {
            ++it;
        }
    }
}

void MainSettings::sortCommands()
{
    std::sort(m_commands.begin(), m_commands.end(), Command::commandCompare);
}

void MainSettings::renameCommandGroup(const QString& oldGroupName, const QString& newGroupName)
{
    int nbCommands = getCommandCount();

    for (int i = 0; i < nbCommands; i++)
    {
        if (getCommand(i)->getGroup() == oldGroupName)
        {
            Command *command_mod = const_cast<Command*>(getCommand(i));
            command_mod->setGroup(newGroupName);
        }
    }
}

const Command* MainSettings::getCommand(const QString& groupName, const QString& description) const
{
    int nbCommands = getCommandCount();

    for (int i = 0; i < nbCommands; i++)
    {
        if ((getCommand(i)->getGroup() == groupName) &&
            (getCommand(i)->getDescription() == description))
        {
            return getCommand(i);
        }
    }

    return nullptr;
}

void MainSettings::clearCommands()
{
    foreach (Command *command, m_commands) {
        delete command;
    }

    m_commands.clear();
}

// FeatureSet presets

FeatureSetPreset* MainSettings::newFeatureSetPreset(const QString& group, const QString& description)
{
	FeatureSetPreset* preset = new FeatureSetPreset();
	preset->setGroup(group);
	preset->setDescription(description);
	addFeatureSetPreset(preset);
	return preset;
}

void MainSettings::addFeatureSetPreset(FeatureSetPreset *preset)
{
    m_featureSetPresets.append(preset);
}

void MainSettings::deleteFeatureSetPreset(const FeatureSetPreset* preset)
{
	m_featureSetPresets.removeAll((FeatureSetPreset*) preset);
	delete (FeatureSetPreset*) preset;
}

void MainSettings::deleteFeatureSetPresetGroup(const QString& groupName)
{
    FeatureSetPresets::iterator it = m_featureSetPresets.begin();

    while (it != m_featureSetPresets.end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_featureSetPresets.erase(it);
        } else {
            ++it;
        }
    }
}

void MainSettings::sortFeatureSetPresets()
{
    std::sort(m_featureSetPresets.begin(), m_featureSetPresets.end(), FeatureSetPreset::presetCompare);
}

void MainSettings::renameFeatureSetPresetGroup(const QString& oldGroupName, const QString& newGroupName)
{
    int nbPresets = getFeatureSetPresetCount();

    for (int i = 0; i < nbPresets; i++)
    {
        if (getFeatureSetPreset(i)->getGroup() == oldGroupName)
        {
            FeatureSetPreset *preset_mod = const_cast<FeatureSetPreset*>(getFeatureSetPreset(i));
            preset_mod->setGroup(newGroupName);
        }
    }
}

const FeatureSetPreset* MainSettings::getFeatureSetPreset(const QString& groupName, const QString& description) const
{
    int nbPresets = getFeatureSetPresetCount();

    for (int i = 0; i < nbPresets; i++)
    {
        if ((getFeatureSetPreset(i)->getGroup() == groupName) &&
            (getFeatureSetPreset(i)->getDescription() == description))
        {
            return getFeatureSetPreset(i);
        }
    }

    return nullptr;
}

void MainSettings::clearFeatureSetPresets()
{
    foreach (FeatureSetPreset *preset, m_featureSetPresets) {
        delete preset;
    }

    m_featureSetPresets.clear();
}

// FeatureSet presets

PluginPreset* MainSettings::newPluginPreset(const QString& group, const QString& description)
{
	PluginPreset* preset = new PluginPreset();
	preset->setGroup(group);
	preset->setDescription(description);
	addPluginPreset(preset);
	return preset;
}

void MainSettings::addPluginPreset(PluginPreset *preset)
{
    m_pluginPresets.append(preset);
}

void MainSettings::deletePluginPreset(const PluginPreset* preset)
{
	m_pluginPresets.removeAll((PluginPreset*) preset);
	delete (PluginPreset*) preset;
}

void MainSettings::deletePluginPresetGroup(const QString& groupName)
{
    PluginPresets::iterator it = m_pluginPresets.begin();

    while (it != m_pluginPresets.end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_pluginPresets.erase(it);
        } else {
            ++it;
        }
    }
}

void MainSettings::sortPluginPresets()
{
    std::sort(m_pluginPresets.begin(), m_pluginPresets.end(), PluginPreset::presetCompare);
}

void MainSettings::renamePluginPresetGroup(const QString& oldGroupName, const QString& newGroupName)
{
    int nbPresets = getPluginPresetCount();

    for (int i = 0; i < nbPresets; i++)
    {
        if (getPluginPreset(i)->getGroup() == oldGroupName)
        {
            PluginPreset *preset_mod = const_cast<PluginPreset*>(getPluginPreset(i));
            preset_mod->setGroup(newGroupName);
        }
    }
}

const PluginPreset* MainSettings::getPluginPreset(const QString& groupName, const QString& description) const
{
    int nbPresets = getPluginPresetCount();

    for (int i = 0; i < nbPresets; i++)
    {
        if ((getPluginPreset(i)->getGroup() == groupName) &&
            (getPluginPreset(i)->getDescription() == description))
        {
            return getPluginPreset(i);
        }
    }

    return nullptr;
}

void MainSettings::clearPluginPresets()
{
    foreach (PluginPreset *preset, m_pluginPresets) {
        delete preset;
    }

    m_pluginPresets.clear();
}

// Configurations

Configuration* MainSettings::newConfiguration(const QString& group, const QString& description)
{
	Configuration* configuration = new Configuration();
	configuration->setGroup(group);
	configuration->setDescription(description);
    m_configurations.append(configuration);
	return configuration;
}

void MainSettings::addConfiguration(Configuration *configuration)
{
    m_configurations.append(configuration);
}

void MainSettings::deleteConfiguration(const Configuration *configuration)
{
	m_configurations.removeAll((Configuration*) configuration);
	delete (Configuration*) configuration;
}

QByteArray MainSettings::serializeConfiguration(const Configuration *configuration) const
{
    return configuration->serialize();
}

bool MainSettings::deserializeConfiguration(const QByteArray& blob, Configuration *configuration)
{
    return configuration->deserialize(blob);
}

const Configuration* MainSettings::getConfiguration(const QString& groupName, const QString& description) const
{
    int nbConfigurations = getConfigurationCount();

    for (int i = 0; i < nbConfigurations; i++)
    {
        if ((getConfiguration(i)->getGroup() == groupName) &&
            (getConfiguration(i)->getDescription() == description))
        {
            return getConfiguration(i);
        }
    }

    return nullptr;
}

void MainSettings::sortConfigurations()
{
    std::sort(m_configurations.begin(), m_configurations.end(), Configuration::configCompare);
}

void MainSettings::renameConfigurationGroup(const QString& oldGroupName, const QString& newGroupName)
{
    int nbConfigurations = getConfigurationCount();

    for (int i = 0; i < nbConfigurations; i++)
    {
        if (getConfiguration(i)->getGroup() == oldGroupName)
        {
            Configuration *configuration_mod = const_cast<Configuration*>(getConfiguration(i));
            configuration_mod->setGroup(newGroupName);
        }
    }
}

void MainSettings::deleteConfigurationGroup(const QString& groupName)
{
    Configurations::iterator it = m_configurations.begin();

    while (it != m_configurations.end())
    {
        if ((*it)->getGroup() == groupName) {
            it = m_configurations.erase(it);
        } else {
            ++it;
        }
    }
}

void MainSettings::clearConfigurations()
{
    foreach (Configuration *configuration, m_configurations) {
        delete configuration;
    }

    m_configurations.clear();
}
