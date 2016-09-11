#ifndef INCLUDE_PRESET_H
#define INCLUDE_PRESET_H

#include <QString>
#include <QList>
#include <QMetaType>

class Preset {
public:
	struct ChannelConfig {
		QString m_channel;
		QByteArray m_config;

		ChannelConfig(const QString& channel, const QByteArray& config) :
			m_channel(channel),
			m_config(config)
		{ }
	};
	typedef QList<ChannelConfig> ChannelConfigs;

	struct SourceConfig
	{
		QString m_sourceId;
		QString m_sourceSerial;
		int m_sourceSequence;
		QByteArray m_config;

		SourceConfig(const QString& sourceId,
				const QString& sourceSerial,
				int sourceSequence,
				const QByteArray& config) :
					m_sourceId(sourceId),
					m_sourceSerial(sourceSerial),
					m_sourceSequence(sourceSequence),
					m_config(config)
		{ }
	};
	typedef QList<SourceConfig> SourceConfigs;

	Preset();

	void resetToDefaults();
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

	void setSourceConfig(const QString& sourceId, const QString& sourceSerial, int sourceSequence, const QByteArray& config)
	{
		addOrUpdateSourceConfig(sourceId, sourceSerial, sourceSequence, config);
	}

	void addOrUpdateSourceConfig(const QString& sourceId,
			const QString& sourceSerial,
			int sourceSequence,
			const QByteArray& config);

	const QByteArray* findBestSourceConfig(const QString& sourceId,
			const QString& sourceSerial,
			int sourceSequence) const;

	static bool presetCompare(const Preset *p1, Preset *p2)
	{
	    if (p1->m_centerFrequency != p2->m_centerFrequency) {
	        return p1->m_centerFrequency < p2->m_centerFrequency;
	    } else {
	        return p1->m_description < p2->m_description;
	    }
	}

protected:
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

	// sources and configurations
	SourceConfigs m_sourceConfigs;

	// screen and dock layout
	QByteArray m_layout;
};

Q_DECLARE_METATYPE(const Preset*);
Q_DECLARE_METATYPE(Preset*);

#endif // INCLUDE_PRESET_H
