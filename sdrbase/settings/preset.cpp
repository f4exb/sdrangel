#include "util/simpleserializer.h"
#include "settings/preset.h"

#include <QDebug>

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
	m_layout.clear();
	m_spectrumConfig.clear();
	m_channelConfigs.clear();
	m_source.clear();
	m_sourceConfig.clear();
}

QByteArray Preset::serialize() const
{
	qDebug() << "Preset::serialize (" << this->getSource().toStdString().c_str() << ")";

	SimpleSerializer s(1);
	s.writeString(1, m_group);
	s.writeString(2, m_description);
	s.writeU64(3, m_centerFrequency);
	s.writeBlob(4, m_layout);
	s.writeBlob(5, m_spectrumConfig);
	s.writeString(6, m_source);
	s.writeBlob(7, m_sourceConfig);

	s.writeS32(200, m_channelConfigs.size());

	qDebug() << "  m_group: " << m_group.toStdString().c_str();

	for(int i = 0; i < m_channelConfigs.size(); i++) {
		s.writeString(201 + i * 2, m_channelConfigs[i].m_channel);
		s.writeBlob(202 + i * 2, m_channelConfigs[i].m_config);
	}

	return s.final();
}

bool Preset::deserialize(const QByteArray& data)
{
	qDebug() << "Preset::deserialize (" << this->getSource().toStdString().c_str() << ")";
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readString(1, &m_group, "default");
		d.readString(2, &m_description, "no name");
		d.readU64(3, &m_centerFrequency, 0);
		d.readBlob(4, &m_layout);
		d.readBlob(5, &m_spectrumConfig);
		d.readString(6, &m_source);
		d.readBlob(7, &m_sourceConfig);

		qDebug() << "Preset::deserialize: m_group: " << m_group.toStdString().c_str();

		qint32 channelCount = 0;
		d.readS32(200, &channelCount, 0);

		for(int i = 0; i < channelCount; i++) {
			QString channel;
			QByteArray config;
			d.readString(201 + i * 2, &channel, "unknown-channel");
			d.readBlob(202 + i * 2, &config);
			m_channelConfigs.append(ChannelConfig(channel, config));
		}
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}
