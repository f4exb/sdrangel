#include <QSettings>
#include <QStringList>

#include "settings/mainsettings.h"
#include "commands/command.h"

MainSettings::MainSettings()
{
	resetToDefaults();
}

MainSettings::~MainSettings()
{
	for(int i = 0; i < m_presets.count(); ++i)
	{
		delete m_presets[i];
	}

	for(int i = 0; i < m_commands.count(); ++i)
    {
        delete m_commands[i];
    }
}

void MainSettings::load()
{
	QSettings s;

	m_preferences.deserialize(qUncompress(QByteArray::fromBase64(s.value("preferences").toByteArray())));
	m_workingPreset.deserialize(qUncompress(QByteArray::fromBase64(s.value("current").toByteArray())));

	if (m_audioDeviceInfo)
	{
	    m_audioDeviceInfo->deserialize(qUncompress(QByteArray::fromBase64(s.value("audio").toByteArray())));
	}

	QStringList groups = s.childGroups();

	for(int i = 0; i < groups.size(); ++i)
	{
		if (groups[i].startsWith("preset"))
		{
			s.beginGroup(groups[i]);
			Preset* preset = new Preset;

			if(preset->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
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

            if(command->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
            {
                m_commands.append(command);
            }
            else
            {
                delete command;
            }

            s.endGroup();
        }
	}
}

void MainSettings::save() const
{
	QSettings s;

	s.setValue("preferences", qCompress(m_preferences.serialize()).toBase64());
	s.setValue("current", qCompress(m_workingPreset.serialize()).toBase64());

	if (m_audioDeviceInfo)
	{
	    s.setValue("audio", qCompress(m_audioDeviceInfo->serialize()).toBase64());
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
}

void MainSettings::resetToDefaults()
{
	m_preferences.resetToDefaults();
	m_workingPreset.resetToDefaults();
}

Preset* MainSettings::newPreset(const QString& group, const QString& description)
{
	Preset* preset = new Preset();
	preset->setGroup(group);
	preset->setDescription(description);
	m_presets.append(preset);
	return preset;
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
    qSort(m_presets.begin(), m_presets.end(), Preset::presetCompare);
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

const Preset* MainSettings::getPreset(const QString& groupName, quint64 centerFrequency, const QString& description) const
{
    int nbPresets = getPresetCount();

    for (int i = 0; i < nbPresets; i++)
    {
        if ((getPreset(i)->getGroup() == groupName) &&
            (getPreset(i)->getCenterFrequency() == centerFrequency) &&
            (getPreset(i)->getDescription() == description))
        {
            return getPreset(i);
        }
    }

    return 0;
}

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
    qSort(m_commands.begin(), m_commands.end(), Command::commandCompare);
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

    return 0;
}
