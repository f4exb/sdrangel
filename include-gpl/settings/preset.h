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

	void setShowScope(bool value) { m_showScope = value; }
	bool getShowScope() const { return m_showScope; }

	void setLayout(const QByteArray& data) { m_layout = data; }
	const QByteArray& getLayout() const { return m_layout; }

	void setDCOffsetCorrection(bool value) { m_dcOffsetCorrection = value; }
	bool getDCOffsetCorrection() const { return m_dcOffsetCorrection; }

	void setIQImbalanceCorrection(bool value) { m_iqImbalanceCorrection = value; }
	bool getIQImbalanceCorrection() const { return m_iqImbalanceCorrection; }

	void setScopeConfig(const QByteArray& data) { m_scopeConfig = data; }
	const QByteArray& getScopeConfig() const { return m_scopeConfig; }

	void clearChannels() { m_channelConfigs.clear(); }
	void addChannel(const QString& channel, const QByteArray& config) { m_channelConfigs.append(ChannelConfig(channel, config)); }
	int getChannelCount() const { return m_channelConfigs.count(); }
	const ChannelConfig& getChannelConfig(int index) const { return m_channelConfigs.at(index); }

	void setSourceConfig(const QString& source, const QByteArray& generalConfig, const QByteArray& config)
	{
		m_source = source;
		m_sourceGeneralConfig = generalConfig;
		m_sourceConfig = config;
	}
	const QString& getSource() const { return m_source; }
	const QByteArray& getSourceGeneralConfig() const { return m_sourceGeneralConfig; }
	const QByteArray& getSourceConfig() const { return m_sourceConfig; }

protected:
	// group and preset description
	QString m_group;
	QString m_description;
	quint64 m_centerFrequency;

	// general configuration
	QByteArray m_spectrumConfig;
	QByteArray m_scopeConfig;

	// dc offset and i/q imbalance correction
	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;

	// display scope dock
	bool m_showScope;

	// sample source and sample source configuration
	QString m_source;
	QByteArray m_sourceGeneralConfig;
	QByteArray m_sourceConfig;

	// channels and configurations
	ChannelConfigs m_channelConfigs;

	// screen and dock layout
	QByteArray m_layout;
};

Q_DECLARE_METATYPE(const Preset*)

#endif // INCLUDE_PRESET_H
