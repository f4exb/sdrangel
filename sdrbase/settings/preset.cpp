#include "util/simpleserializer.h"
#include "settings/preset.h"

Preset::Preset()
{
	resetToDefaults();
}

void Preset::resetToDefaults()
{
	m_group = "default";
	m_description = "no name";
	m_centerFrequency = 0;
	m_spectrumConfig.clear();
	m_scopeConfig.clear();
	m_dcOffsetCorrection = true;
	m_iqImbalanceCorrection = true;
	m_showScope = true;
	m_layout.clear();
	m_spectrumConfig.clear();
	m_channelConfigs.clear();
	m_source.clear();
	m_sourceConfig.clear();
}

QByteArray Preset::serialize() const
{
	SimpleSerializer s(1);
	s.writeString(1, m_group);
	s.writeString(2, m_description);
	s.writeU64(3, m_centerFrequency);
	s.writeBool(4, m_showScope);
	s.writeBlob(5, m_layout);
	s.writeBlob(6, m_spectrumConfig);
	s.writeBool(7, m_dcOffsetCorrection);
	s.writeBool(8, m_iqImbalanceCorrection);
	s.writeBlob(9, m_scopeConfig);
	s.writeString(10, m_source);
	s.writeBlob(11, m_sourceGeneralConfig);
	s.writeBlob(12, m_sourceConfig);

	s.writeS32(100, m_channelConfigs.size());
	for(int i = 0; i < m_channelConfigs.size(); i++) {
		s.writeString(101 + i * 2, m_channelConfigs[i].m_channel);
		s.writeBlob(102 + i * 2, m_channelConfigs[i].m_config);
	}

	return s.final();
}

bool Preset::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readString(1, &m_group, "default");
		d.readString(2, &m_description, "no name");
		d.readU64(3, &m_centerFrequency, 0);
		d.readBool(4, &m_showScope, true);
		d.readBlob(5, &m_layout);
		d.readBlob(6, &m_spectrumConfig);
		d.readBool(7, &m_dcOffsetCorrection, true);
		d.readBool(8, &m_iqImbalanceCorrection, true);
		d.readBlob(9, &m_scopeConfig);
		d.readString(10, &m_source);
		d.readBlob(11, &m_sourceGeneralConfig);
		d.readBlob(12, &m_sourceConfig);

		qint32 channelCount = 0;
		d.readS32(100, &channelCount, 0);
		for(int i = 0; i < channelCount; i++) {
			QString channel;
			QByteArray config;
			d.readString(101 + i * 2, &channel, "unknown-channel");
			d.readBlob(102 + i * 2, &config);
			m_channelConfigs.append(ChannelConfig(channel, config));
		}
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}
