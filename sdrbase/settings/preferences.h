#ifndef INCLUDE_PREFERENCES_H
#define INCLUDE_PREFERENCES_H

#include <QString>

#include "export.h"

class SDRBASE_API Preferences {
public:
	Preferences();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void setSourceDevice(const QString& value) { m_sourceDevice = value; }
	const QString& getSourceDevice() const { return m_sourceDevice; }
	void setSourceIndex(const int value) { m_sourceIndex = value; }
	int getSourceIndex() const { return m_sourceIndex; }

	void setAudioType(const QString& value) { m_audioType = value; }
	const QString& getAudioType() const { return m_audioType; }
	void setAudioDevice(const QString& value) { m_audioDevice = value; }
	const QString& getAudioDevice() const { return m_audioDevice; }

	void setStationName(const QString& name) { m_stationName = name; }
        void setLatitude(float latitude) { m_latitude = latitude; }
	void setLongitude(float longitude) { m_longitude = longitude; }
	void setAltitude(float altitude) { m_altitude = altitude; }
        QString getStationName() const { return m_stationName; }
	float getLatitude() const { return m_latitude; }
	float getLongitude() const { return m_longitude; }
	float getAltitude() const { return m_altitude; }

	void setConsoleMinLogLevel(const QtMsgType& minLogLevel) { m_consoleMinLogLevel = minLogLevel; }
    void setFileMinLogLevel(const QtMsgType& minLogLevel) { m_fileMinLogLevel = minLogLevel; }
	void setUseLogFile(bool useLogFile) { m_useLogFile = useLogFile; }
	void setLogFileName(const QString& value) { m_logFileName = value; }
	QtMsgType getConsoleMinLogLevel() const { return m_consoleMinLogLevel; }
    QtMsgType getFileMinLogLevel() const { return m_fileMinLogLevel; }
	bool getUseLogFile() const { return m_useLogFile; }
	const QString& getLogFileName() const { return m_logFileName; }

protected:
	QString m_sourceDevice; //!< Identification of the source used in R0 tab (GUI flavor) at startup
	int m_sourceIndex;      //!< Index of the source used in R0 tab (GUI flavor) at startup

	QString m_audioType;
	QString m_audioDevice;

	QString m_stationName;  //!< Name of the station (for drawing on the map)
        float m_latitude;       //!< Position of the station
	float m_longitude;
        float m_altitude;       //!< Altitude in metres

	QtMsgType m_consoleMinLogLevel;
    QtMsgType m_fileMinLogLevel;
	bool m_useLogFile;
	QString m_logFileName;
};

#endif // INCLUDE_PREFERENCES_H
