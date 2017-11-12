#ifndef INCLUDE_PREFERENCES_H
#define INCLUDE_PREFERENCES_H

#include <QString>

class Preferences {
public:
	Preferences();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void setSourceType(const QString& value) { m_sourceType = value; }
	const QString& getSourceType() const { return m_sourceType; }
	void setSourceDevice(const QString& value) { m_sourceDevice= value; }
	const QString& getSourceDevice() const { return m_sourceDevice; }
	void setSourceIndex(const int value) { m_sourceIndex = value; }
	int getSourceIndex() const { return m_sourceIndex; }

	void setAudioType(const QString& value) { m_audioType = value; }
	const QString& getAudioType() const { return m_audioType; }
	void setAudioDevice(const QString& value) { m_audioDevice= value; }
	const QString& getAudioDevice() const { return m_audioDevice; }

	void setLatitude(float latitude) { m_latitude = latitude; }
	void setLongitude(float longitude) { m_longitude = longitude; }
	float getLatitude() const { return m_latitude; }
	float getLongitude() const { return m_longitude; }

	void setConsoleMinLogLevel(const QtMsgType& minLogLevel) { m_consoleMinLogLevel = minLogLevel; }
    void setFileMinLogLevel(const QtMsgType& minLogLevel) { m_fileMinLogLevel = minLogLevel; }
	void setUseLogFile(bool useLogFile) { m_useLogFile = useLogFile; }
	void setLogFileName(const QString& value) { m_logFileName = value; }
	QtMsgType getConsoleMinLogLevel() const { return m_consoleMinLogLevel; }
    QtMsgType getFileMinLogLevel() const { return m_fileMinLogLevel; }
	bool getUseLogFile() const { return m_useLogFile; }
	const QString& getLogFileName() const { return m_logFileName; }

protected:
	QString m_sourceType;
	QString m_sourceDevice;
	int m_sourceIndex;

	QString m_audioType;
	QString m_audioDevice;

	float m_latitude;
	float m_longitude;

	QtMsgType m_consoleMinLogLevel;
    QtMsgType m_fileMinLogLevel;
	bool m_useLogFile;
	QString m_logFileName;
};

#endif // INCLUDE_PREFERENCES_H
