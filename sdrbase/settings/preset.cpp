#include "util/simpleserializer.h"
#include "settings/preset.h"

#include <QDebug>

Preset::Preset()
{
	resetToDefaults();
}

void Preset::resetToDefaults()
{
    m_sourcePreset = true;
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
//	qDebug("Preset::serialize: m_group: %s mode: %s m_description: %s m_centerFrequency: %llu",
//			qPrintable(m_group),
//			m_sourcePreset ? "Rx" : "Tx",
//			qPrintable(m_description),
//			m_centerFrequency);

	SimpleSerializer s(1);

	s.writeString(1, m_group);
	s.writeString(2, m_description);
	s.writeU64(3, m_centerFrequency);
	s.writeBlob(4, m_layout);
	s.writeBlob(5, m_spectrumConfig);
	s.writeBool(6, m_sourcePreset);

	s.writeS32(20, m_deviceConfigs.size());

	for (int i = 0; i < m_deviceConfigs.size(); i++)
	{
		s.writeString(24 + i*4, m_deviceConfigs[i].m_deviceId);
		s.writeString(25 + i*4, m_deviceConfigs[i].m_deviceSerial);
		s.writeS32(26 + i*4, m_deviceConfigs[i].m_deviceSequence);
		s.writeBlob(27 + i*4, m_deviceConfigs[i].m_config);

//		qDebug("Preset::serialize:  source: id: %s, ser: %s, seq: %d",
//			qPrintable(m_deviceConfigs[i].m_deviceId),
//			qPrintable(m_deviceConfigs[i].m_deviceSerial),
//			m_deviceConfigs[i].m_deviceSequence);

		if (i >= (200-23)/4) // full!
		{
		    qWarning("Preset::serialize: too many sources");
			break;
		}
	}

	s.writeS32(200, m_channelConfigs.size());

	for(int i = 0; i < m_channelConfigs.size(); i++)
	{
//		qDebug("Preset::serialize:  channel: id: %s", qPrintable(m_channelConfigs[i].m_channel));

		s.writeString(201 + i * 2, m_channelConfigs[i].m_channelIdURI);
		s.writeBlob(202 + i * 2, m_channelConfigs[i].m_config);
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
		d.readBlob(5, &m_spectrumConfig);
		d.readBool(6, &m_sourcePreset, true);

//		qDebug("Preset::deserialize: m_group: %s mode: %s m_description: %s m_centerFrequency: %llu",
//				qPrintable(m_group),
//				m_sourcePreset ? "Rx" : "Tx",
//				qPrintable(m_description),
//				m_centerFrequency);

		qint32 sourcesCount = 0;
		d.readS32(20, &sourcesCount, 0);

		if (sourcesCount >= (200-23)/4) // limit was hit!
		{
			sourcesCount = ((200-23)/4) - 1;
		}

		m_deviceConfigs.clear();

		for (int i = 0; i < sourcesCount; i++)
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
//				qDebug("Preset::deserialize:  source: id: %s, ser: %s, seq: %d",
//					qPrintable(sourceId),
//					qPrintable(sourceSerial),
//					sourceSequence);

				m_deviceConfigs.append(DeviceConfig(sourceId, sourceSerial, sourceSequence, sourceConfig));
			}
		}

        qint32 channelCount;
        d.readS32(200, &channelCount, 0);

		m_channelConfigs.clear();

		for (int i = 0; i < channelCount; i++)
		{
			QString channel;
			QByteArray config;

			d.readString(201 + i * 2, &channel, "unknown-channel");
			d.readBlob(202 + i * 2, &config);

//			qDebug("Preset::deserialize:  channel: id: %s", qPrintable(channel));
			m_channelConfigs.append(ChannelConfig(channel, config));
		}

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

void Preset::addOrUpdateDeviceConfig(const QString& sourceId,
		const QString& sourceSerial,
		int sourceSequence,
		const QByteArray& config)
{
	DeviceeConfigs::iterator it = m_deviceConfigs.begin();

	for (; it != m_deviceConfigs.end(); ++it)
	{
		if (it->m_deviceId == sourceId)
		{
			if (sourceSerial.isNull() || sourceSerial.isEmpty())
			{
				if (it->m_deviceSequence == sourceSequence)
				{
					break;
				}
			}
			else
			{
				if (it->m_deviceSerial == sourceSerial)
				{
					break;
				}
			}
		}
	}

	if (it == m_deviceConfigs.end())
	{
		m_deviceConfigs.append(DeviceConfig(sourceId, sourceSerial, sourceSequence, config));
	}
	else
	{
		it->m_config = config;
	}
}

const QByteArray* Preset::findBestDeviceConfig(const QString& sourceId,
		const QString& sourceSerial,
		int sourceSequence) const
{
	DeviceeConfigs::const_iterator it = m_deviceConfigs.begin();
	DeviceeConfigs::const_iterator itFirstOfKind = m_deviceConfigs.end();
	DeviceeConfigs::const_iterator itMatchSequence = m_deviceConfigs.end();

	for (; it != m_deviceConfigs.end(); ++it)
	{
		if (it->m_deviceId == sourceId)
		{
			if (itFirstOfKind == m_deviceConfigs.end())
			{
				itFirstOfKind = it;
			}

			if (sourceSerial.isNull() || sourceSerial.isEmpty())
			{
				if (it->m_deviceSequence == sourceSequence)
				{
					break;
				}
			}
			else
			{
				if (it->m_deviceSerial == sourceSerial)
				{
					break;
				}
				else if(it->m_deviceSequence == sourceSequence)
				{
					itMatchSequence = it;
				}
			}
		}
	}

	if (it == m_deviceConfigs.end()) // no exact match
	{
		if (itMatchSequence != m_deviceConfigs.end()) // match sequence ?
		{
			qDebug("Preset::findBestSourceConfig: sequence matched: id: %s seq: %d", qPrintable(itMatchSequence->m_deviceId), itMatchSequence->m_deviceSequence);
			return &(itMatchSequence->m_config);
		}
		else if (itFirstOfKind != m_deviceConfigs.end()) // match source type ?
		{
			qDebug("Preset::findBestSourceConfig: first of kind matched: id: %s", qPrintable(itFirstOfKind->m_deviceId));
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
		qDebug("Preset::findBestSourceConfig: serial matched (exact): id: %s ser: %s", qPrintable(it->m_deviceId), qPrintable(it->m_deviceSerial));
		return &(it->m_config);
	}
}
