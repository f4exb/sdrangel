#include <QSettings>
#include <QStringList>
#include <QDebug>

#include <algorithm>

#include "settings/mainsettings.h"
#include "commands/command.h"
#include "audio/audiodevicemanager.h"
#include "ambe/ambeengine.h"

MainSettings::MainSettings() :
    m_audioDeviceManager(nullptr),
    m_ambeEngine(nullptr)
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
	m_workingPreset.deserialize(qUncompress(QByteArray::fromBase64(s.value("current").toByteArray())));
	m_workingFeatureSetPreset.deserialize(qUncompress(QByteArray::fromBase64(s.value("current-featureset").toByteArray())));

	if (m_audioDeviceManager) {
	    m_audioDeviceManager->deserialize(qUncompress(QByteArray::fromBase64(s.value("audio").toByteArray())));
	}

    if (m_ambeEngine) {
        m_ambeEngine->deserialize(qUncompress(QByteArray::fromBase64(s.value("ambe").toByteArray())));
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
	}

    m_hardwareDeviceUserArgs.deserialize(qUncompress(QByteArray::fromBase64(s.value("hwDeviceUserArgs").toByteArray())));
    m_limeRFEUSBCalib.deserialize(qUncompress(QByteArray::fromBase64(s.value("limeRFEUSBCalib").toByteArray())));
}

void MainSettings::save() const
{
	QSettings s;

	s.setValue("preferences", qCompress(m_preferences.serialize()).toBase64());
	s.setValue("current", qCompress(m_workingPreset.serialize()).toBase64());
	s.setValue("current-featureset", qCompress(m_workingFeatureSetPreset.serialize()).toBase64());

	if (m_audioDeviceManager) {
	    s.setValue("audio", qCompress(m_audioDeviceManager->serialize()).toBase64());
	}

    if (m_ambeEngine) {
        s.setValue("ambe", qCompress(m_ambeEngine->serialize()).toBase64());
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

    s.setValue("hwDeviceUserArgs", qCompress(m_hardwareDeviceUserArgs.serialize()).toBase64());
    s.setValue("limeRFEUSBCalib", qCompress(m_limeRFEUSBCalib.serialize()).toBase64());
}

void MainSettings::initialize()
{
    resetToDefaults();
    clearCommands();
    clearPresets();
    clearFeatureSetPresets();
}

void MainSettings::resetToDefaults()
{
	m_preferences.resetToDefaults();
	m_workingPreset.resetToDefaults();
    m_workingFeatureSetPreset.resetToDefaults();
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
        if (getPreset(i)->getGroup() == oldGroupName)
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
