#ifndef INCLUDE_PRESET_H
#define INCLUDE_PRESET_H

#include <QString>
#include <QList>
#include <QMetaType>

class Preset {
public:
	struct ChannelConfig {
		QString m_channelIdURI; //!< Channel type ID in URI form
		QString m_channelId;    //!< Channel type ID in short form from object name TODO: use in the future
		QByteArray m_config;

		ChannelConfig(const QString& channelIdURI, const QByteArray& config) :
			m_channelIdURI(channelIdURI),
			m_config(config)
		{ }
	};
	typedef QList<ChannelConfig> ChannelConfigs;

	struct DeviceConfig
	{
		QString m_deviceId;
		QString m_deviceSerial;
		int m_deviceSequence;
		QByteArray m_config;

		DeviceConfig(const QString& deviceId,
				const QString& deviceSerial,
				int deviceSequence,
				const QByteArray& config) :
					m_deviceId(deviceId),
					m_deviceSerial(deviceSerial),
					m_deviceSequence(deviceSequence),
					m_config(config)
		{ }
	};
	typedef QList<DeviceConfig> DeviceeConfigs;

	Preset();

	void resetToDefaults();

	void setSourcePreset(bool isSourcePreset) { m_sourcePreset = isSourcePreset; }
	bool isSourcePreset() const { return m_sourcePreset; }

	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void setGroup(const QString& group) { m_group = group; }
	const QString& getGroup() const { return m_group; }
	void setDescription(const QString& description) { m_description = description; }
	const QString& getDescription() const { return m_description; }
	void setCenterFrequency(const quint64 centerFrequency) { m_centerFrequency = centerFrequency; }
	quint64 getCenterFrequency() const { return m_centerFrequency; }

	void setSpectrumConfig(const QByteArray& data) { m_spectrumConfig = data; }
	const QByteArray& getSpectrumConfig() const { return m_spectrumConfig; }

	void setLayout(const QByteArray& data) { m_layout = data; }
	const QByteArray& getLayout() const { return m_layout; }

	void clearChannels() { m_channelConfigs.clear(); }
	void addChannel(const QString& channel, const QByteArray& config) { m_channelConfigs.append(ChannelConfig(channel, config)); }
	int getChannelCount() const { return m_channelConfigs.count(); }
	const ChannelConfig& getChannelConfig(int index) const { return m_channelConfigs.at(index); }

	void setDeviceConfig(const QString& deviceId, const QString& deviceSerial, int deviceSequence, const QByteArray& config)
	{
		addOrUpdateDeviceConfig(deviceId, deviceSerial, deviceSequence, config);
	}

	void addOrUpdateDeviceConfig(const QString& deviceId,
			const QString& deviceSerial,
			int deviceSequence,
			const QByteArray& config);

	const QByteArray* findBestDeviceConfig(const QString& deviceId,
			const QString& deviceSerial,
			int deviceSequence) const;

	static bool presetCompare(const Preset *p1, Preset *p2)
	{
	    if (p1->m_group != p2->m_group)
	    {
	        return p1->m_group < p2->m_group;
	    }
	    else
	    {
            if (p1->m_centerFrequency != p2->m_centerFrequency) {
                return p1->m_centerFrequency < p2->m_centerFrequency;
            } else {
                return p1->m_description < p2->m_description;
            }
	    }
	}

protected:
    bool m_sourcePreset;

    // group and preset description
	QString m_group;
	QString m_description;
	quint64 m_centerFrequency;

	// general configuration
	QByteArray m_spectrumConfig;

	// dc offset and i/q imbalance correction TODO: move it into the source data
	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;

	// sample source and sample source configuration
	QString m_sourceId;
	QString m_sourceSerial;
	int m_sourceSequence;
	QByteArray m_sourceConfig;

	// channels and configurations
	ChannelConfigs m_channelConfigs;

	// devices and configurations
	DeviceeConfigs m_deviceConfigs;

	// screen and dock layout
	QByteArray m_layout;
};

Q_DECLARE_METATYPE(const Preset*);
Q_DECLARE_METATYPE(Preset*);

#endif // INCLUDE_PRESET_H
