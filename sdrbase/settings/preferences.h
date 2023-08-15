#ifndef INCLUDE_PREFERENCES_H
#define INCLUDE_PREFERENCES_H

#include <QString>

#include "export.h"

class SDRBASE_API Preferences {
public:
    enum ElementType
    {
        SourceDevice = 2,
        AudioType,
        AudioDevice,
        SourceIndex,
        Latitude,
        Longitude,
        ConsoleMinLogLevel,
        UseLogFile,
        LogFileName,
        FileMinLogLevel,
        StationName,
        Altitude,
        SourceItemIndex,
        Multisampling,
        AutoUpdatePosition,
        MapMultisampling,
        MapSmoothing,
        FFTEngine
    };

    Preferences();
    Preferences(const Preferences&) = default;
    Preferences& operator=(const Preferences&) = default;

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	const QString& getSourceDevice() const { return m_sourceDevice; }
	void setSourceDevice(const QString& value) { m_sourceDevice = value; }

	int getSourceIndex() const { return m_sourceIndex; }
	void setSourceIndex(const int value) { m_sourceIndex = value; }

	int getSourceItemIndex() const { return m_sourceItemIndex; }
	void setSourceItemIndex(const int value) { m_sourceItemIndex = value; }

	const QString& getAudioType() const { return m_audioType; }
	void setAudioType(const QString& value) { m_audioType = value; }

	const QString& getAudioDevice() const { return m_audioDevice; }
	void setAudioDevice(const QString& value) { m_audioDevice = value; }

    QString getStationName() const { return m_stationName; }
	void setStationName(const QString& name) { m_stationName = name; }

	float getLatitude() const { return m_latitude; }
    void setLatitude(float latitude) { m_latitude = latitude; }

	float getLongitude() const { return m_longitude; }
	void setLongitude(float longitude) { m_longitude = longitude; }

	float getAltitude() const { return m_altitude; }
	void setAltitude(float altitude) { m_altitude = altitude; }

	bool getAutoUpdatePosition() const { return m_autoUpdatePosition; }
	void setAutoUpdatePosition(bool autoUpdatePosition) { m_autoUpdatePosition = autoUpdatePosition; }

	QtMsgType getConsoleMinLogLevel() const { return m_consoleMinLogLevel; }
	void setConsoleMinLogLevel(const QtMsgType& minLogLevel) { m_consoleMinLogLevel = minLogLevel; }

    QtMsgType getFileMinLogLevel() const { return m_fileMinLogLevel; }
    void setFileMinLogLevel(const QtMsgType& minLogLevel) { m_fileMinLogLevel = minLogLevel; }

	bool getUseLogFile() const { return m_useLogFile; }
	void setUseLogFile(bool useLogFile) { m_useLogFile = useLogFile; }

	const QString& getLogFileName() const { return m_logFileName; }
	void setLogFileName(const QString& value) { m_logFileName = value; }

    int getMultisampling() const { return m_multisampling; }
    void setMultisampling(int samples) { m_multisampling = samples; }

    int getMapMultisampling() const { return m_mapMultisampling; }
    void setMapMultisampling(int samples) { m_mapMultisampling = samples; }

    bool getMapSmoothing() const { return m_mapSmoothing; }
    void setMapSmoothing(bool smoothing) { m_mapSmoothing = smoothing; }

    const QString& getFFTEngine() const { return m_fftEngine; }
    void setFFTEngine(const QString& fftEngine) { m_fftEngine = fftEngine; }

protected:
	QString m_sourceDevice; //!< Identification of the source used in R0 tab (GUI flavor) at startup
	int m_sourceIndex;      //!< Index of the source used in R0 tab (GUI flavor) at startup
	int m_sourceItemIndex;  //!< Index of device item in the source used in R0 tab (GUI flavor) at startup

	QString m_audioType;
	QString m_audioDevice;

	QString m_stationName;  //!< Name of the station (for drawing on the map)
    float m_latitude;       //!< Position of the station
	float m_longitude;
    float m_altitude;       //!< Altitude in metres
    bool m_autoUpdatePosition; //!< Automatically update position from GPS

	QtMsgType m_consoleMinLogLevel;
    QtMsgType m_fileMinLogLevel;
	bool m_useLogFile;
	QString m_logFileName;

    int m_multisampling;    //!< Number of samples to use for multisampling anti-aliasing for spectrums (typically 0 or 4)
    int m_mapMultisampling; //!< Number of samples to use for multisampling anti-aliasing for 2D maps (16 gives best text, if not using mapSmoothing)
    bool m_mapSmoothing;    //!< Whether to use smoothing for text boxes on 2D maps

    QString m_fftEngine;    //!< FFT Engine (FFTW, Kiss, vkFFT)
};

#endif // INCLUDE_PREFERENCES_H
