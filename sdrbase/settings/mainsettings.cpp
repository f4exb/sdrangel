#include <QSettings>
#include <QStringList>

#include "settings/mainsettings.h"

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
		if(groups[i].startsWith("preset"))
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
		if(groups[i].startsWith("preset"))
		{
			s.remove(groups[i]);
		}
	}

	for(int i = 0; i < m_presets.count(); ++i)
	{
		QString group = QString("preset-%1").arg(i + 1);
		s.beginGroup(group);
		s.setValue("data", qCompress(m_presets[i]->serialize()).toBase64());
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

void MainSettings::sortPresets()
{
    qSort(m_presets.begin(), m_presets.end(), Preset::presetCompare);
}
