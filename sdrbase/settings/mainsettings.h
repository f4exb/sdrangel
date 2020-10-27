#ifndef INCLUDE_SETTINGS_H
#define INCLUDE_SETTINGS_H

#include <QString>
#include "device/deviceuserargs.h"
#include "limerfe/limerfeusbcalib.h"
#include "preferences.h"
#include "preset.h"
#include "featuresetpreset.h"
#include "export.h"
#include "plugin/pluginmanager.h"

class Command;
class AudioDeviceManager;
class AMBEEngine;


class SDRBASE_API MainSettings {
public:
	MainSettings();
	~MainSettings();

	void load();
	void save() const;

	void resetToDefaults();
    void initialize();
	QString getFileLocation() const;
	int getFileFormat() const; //!< see QSettings::Format for the values

    const Preferences& getPreferences() const { return m_preferences; }
    void setPreferences(const Preferences& preferences) { m_preferences = preferences; }

	Preset* newPreset(const QString& group, const QString& description);
    void addPreset(Preset *preset);
	void deletePreset(const Preset* preset);
	int getPresetCount() const { return m_presets.count(); }
	const Preset* getPreset(int index) const { return m_presets[index]; }
	const Preset* getPreset(const QString& groupName, quint64 centerFrequency, const QString& description, const QString& type) const;
	void sortPresets();
	void renamePresetGroup(const QString& oldGroupName, const QString& newGroupName);
	void deletePresetGroup(const QString& groupName);
    void clearPresets();
    const Preset& getWorkingPresetConst() const { return m_workingPreset; }
	Preset* getWorkingPreset() { return &m_workingPreset; }

    void addCommand(Command *command);
    void deleteCommand(const Command* command);
    int getCommandCount() const { return m_commands.count(); }
    const Command* getCommand(int index) const { return m_commands[index]; }
    const Command* getCommand(const QString& groupName, const QString& description) const;
    void sortCommands();
    void renameCommandGroup(const QString& oldGroupName, const QString& newGroupName);
    void deleteCommandGroup(const QString& groupName);
    void clearCommands();

	FeatureSetPreset* newFeatureSetPreset(const QString& group, const QString& description);
    void addFeatureSetPreset(FeatureSetPreset *preset);
	void deleteFeatureSetPreset(const FeatureSetPreset* preset);
	int getFeatureSetPresetCount() const { return m_featureSetPresets.count(); }
	const FeatureSetPreset* getFeatureSetPreset(int index) const { return m_featureSetPresets[index]; }
	const FeatureSetPreset* getFeatureSetPreset(const QString& groupName, const QString& description) const;
	void sortFeatureSetPresets();
	void renameFeatureSetPresetGroup(const QString& oldGroupName, const QString& newGroupName);
	void deleteFeatureSetPresetGroup(const QString& groupName);
    void clearFeatureSetPresets();
    const FeatureSetPreset& getWorkingFeatureSetPresetConst() const { return m_workingFeatureSetPreset; }
	FeatureSetPreset* getWorkingFeatureSetPreset() { return &m_workingFeatureSetPreset; }
	QList<FeatureSetPreset*> *getFeatureSetPresets() { return &m_featureSetPresets; }

	int getSourceIndex() const { return m_preferences.getSourceIndex(); }
	void setSourceIndex(int value) { m_preferences.setSourceIndex(value); }
	const QString& getSourceDeviceId() const { return m_preferences.getSourceDevice(); }
	void setSourceDeviceId(const QString& deviceId) { m_preferences.setSourceDevice(deviceId); }

	void setStationName(const QString& name) { m_preferences.setStationName(name); }
        void setLatitude(float latitude) { m_preferences.setLatitude(latitude); }
	void setLongitude(float longitude) { m_preferences.setLongitude(longitude); }
	void setAltitude(float altitude) { m_preferences.setAltitude(altitude); }
        QString getStationName() const { return m_preferences.getStationName(); }
	float getLatitude() const { return m_preferences.getLatitude(); }
	float getLongitude() const { return m_preferences.getLongitude(); }
	float getAltitude() const { return m_preferences.getAltitude(); }

    void setConsoleMinLogLevel(const QtMsgType& minLogLevel) { m_preferences.setConsoleMinLogLevel(minLogLevel); }
    void setFileMinLogLevel(const QtMsgType& minLogLevel) { m_preferences.setFileMinLogLevel(minLogLevel); }
    void setUseLogFile(bool useLogFile) { m_preferences.setUseLogFile(useLogFile); }
    void setLogFileName(const QString& value) { m_preferences.setLogFileName(value); }
    QtMsgType getConsoleMinLogLevel() const { return m_preferences.getConsoleMinLogLevel(); }
    QtMsgType getFileMinLogLevel() const { return m_preferences.getFileMinLogLevel(); }
    bool getUseLogFile() const { return m_preferences.getUseLogFile(); }
    const QString& getLogFileName() const { return m_preferences.getLogFileName(); }
	DeviceUserArgs& getDeviceUserArgs() { return m_hardwareDeviceUserArgs; }
	LimeRFEUSBCalib& getLimeRFEUSBCalib() { return m_limeRFEUSBCalib; }

	const AudioDeviceManager *getAudioDeviceManager() const { return m_audioDeviceManager; }
	void setAudioDeviceManager(AudioDeviceManager *audioDeviceManager) { m_audioDeviceManager = audioDeviceManager; }
    void setAMBEEngine(AMBEEngine *ambeEngine) { m_ambeEngine = ambeEngine; }

protected:
	Preferences m_preferences;
	AudioDeviceManager *m_audioDeviceManager;
	Preset m_workingPreset;
	FeatureSetPreset m_workingFeatureSetPreset;
	typedef QList<Preset*> Presets;
	Presets m_presets;
    typedef QList<Command*> Commands;
    Commands m_commands;
    typedef QList<FeatureSetPreset*> FeatureSetPresets;
    FeatureSetPresets m_featureSetPresets;
	DeviceUserArgs m_hardwareDeviceUserArgs;
	LimeRFEUSBCalib m_limeRFEUSBCalib;
    AMBEEngine *m_ambeEngine;
};

#endif // INCLUDE_SETTINGS_H
