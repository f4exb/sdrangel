#ifndef INCLUDE_SETTINGS_H
#define INCLUDE_SETTINGS_H

#include <QString>
#include "preferences.h"
#include "preset.h"

class Settings {
public:
	Settings();
	~Settings();

	void load();
	void save() const;

	void resetToDefaults();

	Preset* newPreset(const QString& group, const QString& description);
	void deletePreset(const Preset* preset);
	int getPresetCount() const { return m_presets.count(); }
	const Preset* getPreset(int index) const { return m_presets[index]; }

	Preset* getCurrent() { return &m_current; }

protected:
	Preferences m_preferences;
	Preset m_current;
	typedef QList<Preset*> Presets;
	Presets m_presets;
};

#endif // INCLUDE_SETTINGS_H
