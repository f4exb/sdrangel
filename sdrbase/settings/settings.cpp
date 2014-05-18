#include <QSettings>
#include <QStringList>
#include "settings/settings.h"

Settings::Settings()
{
	resetToDefaults();
}

Settings::~Settings()
{
	for(int i = 0; i < m_presets.count(); ++i)
		delete m_presets[i];
}

void Settings::load()
{
	QSettings s;

	m_preferences.deserialize(qUncompress(QByteArray::fromBase64(s.value("preferences").toByteArray())));
	m_current.deserialize(qUncompress(QByteArray::fromBase64(s.value("current").toByteArray())));

	QStringList groups = s.childGroups();
	for(int i = 0; i < groups.size(); ++i) {
		if(groups[i].startsWith("preset")) {
			s.beginGroup(groups[i]);
			Preset* preset = new Preset;
			if(preset->deserialize(qUncompress(QByteArray::fromBase64(s.value("data").toByteArray()))))
				m_presets.append(preset);
			else delete preset;
			s.endGroup();
		}
	}
}

void Settings::save() const
{
	QSettings s;

	s.setValue("preferences", qCompress(m_preferences.serialize()).toBase64());
	s.setValue("current", qCompress(m_current.serialize()).toBase64());

	QStringList groups = s.childGroups();
	for(int i = 0; i < groups.size(); ++i) {
		if(groups[i].startsWith("preset"))
			s.remove(groups[i]);
	}

	for(int i = 0; i < m_presets.count(); ++i) {
		QString group = QString("preset-%1").arg(i + 1);
		s.beginGroup(group);
		s.setValue("data", qCompress(m_presets[i]->serialize()).toBase64());
		s.endGroup();
	}
}

void Settings::resetToDefaults()
{
	m_preferences.resetToDefaults();
	m_current.resetToDefaults();
}

Preset* Settings::newPreset(const QString& group, const QString& description)
{
	Preset* preset = new Preset();
	preset->setGroup(group);
	preset->setDescription(description);
	m_presets.append(preset);
	return preset;
}

void Settings::deletePreset(const Preset* preset)
{
	m_presets.removeAll((Preset*)preset);
	delete (Preset*)preset;
}
