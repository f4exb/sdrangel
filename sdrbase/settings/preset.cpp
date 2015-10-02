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
	qDebug("Preset::serialize: m_group: %s m_description: %s m_centerFrequency: %llu",
			qPrintable(m_group),
			qPrintable(m_description),
			m_centerFrequency);

	SimpleSerializer s(1);

	s.writeString(1, m_group);
	s.writeString(2, m_description);
	s.writeU64(3, m_centerFrequency);
	s.writeBlob(4, m_layout);

	s.writeS32(20, m_sourceConfigs.size());

	for (int i = 0; i < m_sourceConfigs.size(); i++)
	{
		s.writeString(24 + i*4, m_sourceConfigs[i].m_sourceId);
		s.writeString(25 + i*4, m_sourceConfigs[i].m_sourceSerial);
		s.writeS32(26 + i*4, m_sourceConfigs[i].m_sourceSequence);
		s.writeBlob(27 + i*4, m_sourceConfigs[i].m_config);

		qDebug("Preset::serialize:  source: id: %ss, ser: %s, seq: %d",
			qPrintable(m_sourceConfigs[i].m_sourceId),
			qPrintable(m_sourceConfigs[i].m_sourceSerial),
			m_sourceConfigs[i].m_sourceSequence);

		if (i >= (200-23)/4) // full!
		{
			break;
		}
	}

	return s.final();
}

bool Preset::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		d.readString(1, &m_group, "default");
		d.readString(2, &m_description, "no name");
		d.readU64(3, &m_centerFrequency, 0);
		d.readBlob(4, &m_layout);

		qDebug("Preset::deserialize: m_group: %s m_description: %s m_centerFrequency: %llu",
				qPrintable(m_group),
				qPrintable(m_description),
				m_centerFrequency);

		qint32 sourcesCount = 0;
		d.readS32(20, &sourcesCount, 0);

		if (sourcesCount >= (200-23)/4) // limit was hit!
		{
			sourcesCount = ((200-23)/4) - 1;
		}

		for(int i = 0; i < sourcesCount; i++)
		{
			QString sourceId, sourceSerial;
			int sourceSequence;
			QByteArray sourceConfig;

			d.readString(24 + i*4, &sourceId, "");
			d.readString(25 + i*4, &sourceSerial, "");
			d.readS32(26 + i*4, &sourceSequence, 0);
			d.readBlob(27 + i*4, &sourceConfig);

			if (!sourceId.isEmpty())
			{
				qDebug("Preset::deserialize:  source: id: %ss, ser: %s, seq: %d",
					qPrintable(sourceId),
					qPrintable(sourceSerial),
					sourceSequence);

				m_sourceConfigs.append(SourceConfig(sourceId, sourceSerial, sourceSequence, sourceConfig));
			}
		}

		return true;
	}
	else
	{
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
		m_sourceConfigs.append(SourceConfig(sourceId, sourceSerial, sourceSequence, config));
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
			qDebug("Preset::findBestSourceConfig: sequence matched: id: %s seq: %d", qPrintable(it->m_sourceId), it->m_sourceSequence);
			return &(itMatchSequence->m_config);
		}
		else if (itFirstOfKind != m_sourceConfigs.end()) // match source type ?
		{
			qDebug("Preset::findBestSourceConfig: first of kind matched: id: %s", qPrintable(it->m_sourceId));
			return &(itFirstOfKind->m_config);
		}
		else // definitely not found !
		{
			qDebug("Preset::findBestSourceConfig: no match");
			return 0;
		}
	}
	else // exact match
	{
		qDebug("Preset::findBestSourceConfig: serial matched (exact): id: %s ser: %s", qPrintable(it->m_sourceId), qPrintable(it->m_sourceSerial));
		return &(it->m_config);
	}
}
