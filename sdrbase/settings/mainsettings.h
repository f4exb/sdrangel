#ifndef INCLUDE_SETTINGS_H
#define INCLUDE_SETTINGS_H

#include <QString>
#include "preferences.h"
#include "preset.h"
#include "audio/audiodeviceinfo.h"

class MainSettings {
public:
	MainSettings();
	~MainSettings();

	void load();
	void save() const;

	void resetToDefaults();

	Preset* newPreset(const QString& group, const QString& description);
	void deletePreset(const Preset* preset);
	int getPresetCount() const { return m_presets.count(); }
	const Preset* getPreset(int index) const { return m_presets[index]; }
	void sortPresets();

	Preset* getWorkingPreset() { return &m_workingPreset; }
	int getSourceIndex() const { return m_preferences.getSourceIndex(); }
	void setSourceIndex(int value) { m_preferences.setSourceIndex(value); }
	const QString& getSourceDeviceId() const { return m_preferences.getSourceDevice(); }
	void setSourceDeviceId(const QString& deviceId) { m_preferences.setSourceDevice(deviceId); }

	void setLatitude(float latitude) { m_preferences.setLatitude(latitude); }
	void setLongitude(float longitude) { m_preferences.setLongitude(longitude); }
	float getLatitude() const { return m_preferences.getLatitude(); }
	float getLongitude() const { return m_preferences.getLongitude(); }

    void setConsoleMinLogLevel(const QtMsgType& minLogLevel) { m_preferences.setConsoleMinLogLevel(minLogLevel); }
    void setFileMinLogLevel(const QtMsgType& minLogLevel) { m_preferences.setFileMinLogLevel(minLogLevel); }
    void setUseLogFile(bool useLogFile) { m_preferences.setUseLogFile(useLogFile); }
    void setLogFileName(const QString& value) { m_preferences.setLogFileName(value); }
    QtMsgType getConsoleMinLogLevel() const { return m_preferences.getConsoleMinLogLevel(); }
    QtMsgType getFileMinLogLevel() const { return m_preferences.getFileMinLogLevel(); }
    bool getUseLogFile() const { return m_preferences.getUseLogFile(); }
    const QString& getLogFileName() const { return m_preferences.getLogFileName(); }

	const AudioDeviceInfo *getAudioDeviceInfo() const { return m_audioDeviceInfo; }
	void setAudioDeviceInfo(AudioDeviceInfo *audioDeviceInfo) { m_audioDeviceInfo = audioDeviceInfo; }

protected:
	Preferences m_preferences;
	AudioDeviceInfo *m_audioDeviceInfo;
	Preset m_workingPreset;
	typedef QList<Preset*> Presets;
	Presets m_presets;
};

#endif // INCLUDE_SETTINGS_H
