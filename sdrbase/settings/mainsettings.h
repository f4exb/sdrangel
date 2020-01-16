#ifndef INCLUDE_SETTINGS_H
#define INCLUDE_SETTINGS_H

#include <QString>
#include "device/deviceuserargs.h"
#include "limerfe/limerfeusbcalib.h"
#include "preferences.h"
#include "preset.h"
#include "export.h"

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

    void addCommand(Command *command);
    void deleteCommand(const Command* command);
    int getCommandCount() const { return m_commands.count(); }
    const Command* getCommand(int index) const { return m_commands[index]; }
    const Command* getCommand(const QString& groupName, const QString& description) const;
    void sortCommands();
    void renameCommandGroup(const QString& oldGroupName, const QString& newGroupName);
    void deleteCommandGroup(const QString& groupName);
    void clearCommands();

    const Preset& getWorkingPresetConst() const { return m_workingPreset; }
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
	DeviceUserArgs& getDeviceUserArgs() { return m_hardwareDeviceUserArgs; }
	LimeRFEUSBCalib& getLimeRFEUSBCalib() { return m_limeRFEUSBCalib; }

	const AudioDeviceManager *getAudioDeviceManager() const { return m_audioDeviceManager; }
	void setAudioDeviceManager(AudioDeviceManager *audioDeviceManager) { m_audioDeviceManager = audioDeviceManager; }
    void setAMBEEngine(AMBEEngine *ambeEngine) { m_ambeEngine = ambeEngine; }

protected:
	Preferences m_preferences;
	AudioDeviceManager *m_audioDeviceManager;
	Preset m_workingPreset;
	typedef QList<Preset*> Presets;
	Presets m_presets;
    typedef QList<Command*> Commands;
    Commands m_commands;
	DeviceUserArgs m_hardwareDeviceUserArgs;
	LimeRFEUSBCalib m_limeRFEUSBCalib;
    AMBEEngine *m_ambeEngine;
};

#endif // INCLUDE_SETTINGS_H
