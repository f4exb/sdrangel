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
	m_sourceId.clear();
	m_sourceConfig.clear();
}

QByteArray Preset::serialize() const
{
	qDebug() << "Preset::serialize (" << this->getSourceId().toStdString().c_str() << ")";

	SimpleSerializer s(1);
	s.writeString(1, m_group);
	s.writeString(2, m_description);
	s.writeU64(3, m_centerFrequency);
	s.writeBlob(4, m_layout);
	s.writeBlob(5, m_spectrumConfig);
	s.writeString(6, m_sourceId);
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
	qDebug() << "Preset::deserialize (" << this->getSourceId().toStdString().c_str() << ")";
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
		d.readString(6, &m_sourceId);
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

void Preset::addOrUpdateSourceConfig(const QString& sourceId,
		const QString& sourceSerial,
		int sourceSequence,
		const QByteArray& config)
{
	SourceConfigs::iterator it = m_sourceConfigs.begin();

	for (; it != m_sourceConfigs.end(); ++it)
	{
		if (it->m_sourceId == sourceId)
		{
			if (sourceSerial.isNull() || sourceSerial.isEmpty())
			{
				if (it->m_sourceSequence == sourceSequence)
				{
					break;
				}
			}
			else
			{
				if (it->m_sourceSerial == sourceSerial)
				{
					break;
				}
			}
		}
	}

	if (it == m_sourceConfigs.end())
	{
		m_sourceConfigs.push_back(SourceConfig(sourceId, sourceSerial, sourceSequence, config));
	}
	else
	{
		it->m_config = config;
	}
}

const QByteArray* Preset::findBestSourceConfig(const QString& sourceId,
		const QString& sourceSerial,
		int sourceSequence)
{
	SourceConfigs::const_iterator it = m_sourceConfigs.begin();
	SourceConfigs::const_iterator itFirstOfKind = m_sourceConfigs.end();
	SourceConfigs::const_iterator itMatchSequence = m_sourceConfigs.end();

	for (; it != m_sourceConfigs.end(); ++it)
	{
		if (it->m_sourceId == sourceId)
		{
			if (itFirstOfKind == m_sourceConfigs.end())
			{
				itFirstOfKind = it;
			}

			if (sourceSerial.isNull() || sourceSerial.isEmpty())
			{
				if (it->m_sourceSequence == sourceSequence)
				{
					break;
				}
			}
			else
			{
				if (it->m_sourceSerial == sourceSerial)
				{
					break;
				}
				else if(it->m_sourceSequence == sourceSequence)
				{
					itMatchSequence = it;
				}
			}
		}
	}

	if (it == m_sourceConfigs.end()) // no exact match
	{
		if (itMatchSequence != m_sourceConfigs.end()) // match sequence ?
		{
			return &(itMatchSequence->m_config);
		}
		else if (itFirstOfKind != m_sourceConfigs.end()) // match source type ?
		{
			return &(itFirstOfKind->m_config);
		}
		else // definitely not found !
		{
			return 0;
		}
	}
	else // exact match
	{
		return &(it->m_config);
	}
}

