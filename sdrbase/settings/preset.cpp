///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB.                             //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "util/simpleserializer.h"
#include "settings/preset.h"

#include <QDebug>

Preset::Preset()
{
	resetToDefaults();
}

Preset::Preset(const Preset& other) :
	m_group(other.m_group),
	m_description(other.m_description),
	m_centerFrequency(other.m_centerFrequency),
	m_spectrumConfig(other.m_spectrumConfig),
	m_dcOffsetCorrection(other.m_dcOffsetCorrection),
	m_iqImbalanceCorrection(other.m_iqImbalanceCorrection),
	m_channelConfigs(other.m_channelConfigs),
	m_deviceConfigs(other.m_deviceConfigs),
	m_showSpectrum(other.m_showSpectrum),
	m_layout(other.m_layout)
{}

void Preset::resetToDefaults()
{
    m_presetType = PresetSource; // Rx
	m_group = "default";
	m_description = "no name";
	m_centerFrequency = 0;
	m_spectrumConfig.clear();
	m_layout.clear();
	m_channelConfigs.clear();
	m_dcOffsetCorrection = false;
	m_iqImbalanceCorrection = false;
	m_showSpectrum = true;
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
    s.writeBool(6, m_presetType == PresetSource);
	s.writeS32(7, (int) m_presetType);
	s.writeBool(8, m_showSpectrum);
    s.writeBlob(9, m_spectrumGeometry);
    s.writeS32(10, m_spectrumWorkspaceIndex);
    s.writeBlob(11, m_deviceGeometry);
    s.writeS32(12, m_deviceWorkspaceIndex);
    s.writeString(13, m_selectedDevice.m_deviceId);
    s.writeString(14, m_selectedDevice.m_deviceSerial);
    s.writeS32(15, m_selectedDevice.m_deviceSequence);
    s.writeS32(16, m_selectedDevice.m_deviceItemIndex);

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
        bool tmpBool;
        int tmp;

		d.readString(1, &m_group, "default");
		d.readString(2, &m_description, "no name");
		d.readU64(3, &m_centerFrequency, 0);
		d.readBlob(4, &m_layout);
		d.readBlob(5, &m_spectrumConfig);
		d.readBool(6, &tmpBool, true);
        d.readS32(7, &tmp, PresetSource);
        m_presetType = tmp < (int) PresetSource ? PresetSource : tmp > (int) PresetMIMO ? PresetMIMO : (PresetType) tmp;

        if (m_presetType != PresetMIMO) {
            m_presetType = tmpBool ? PresetSource : PresetSink;
        }

		d.readBool(8, &m_showSpectrum, true);
        d.readBlob(9, &m_spectrumGeometry);
        d.readS32(10, &m_spectrumWorkspaceIndex, 0);
        d.readBlob(11, &m_deviceGeometry);
        d.readS32(12, &m_deviceWorkspaceIndex, 0);
        d.readString(13, &m_selectedDevice.m_deviceId);
        d.readString(14, &m_selectedDevice.m_deviceSerial);
        d.readS32(15, &m_selectedDevice.m_deviceSequence);
        d.readS32(16, &m_selectedDevice.m_deviceItemIndex);

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
	DeviceConfigs::iterator it = m_deviceConfigs.begin();

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

const QByteArray* Preset::findDeviceConfig(
        const QString& deviceId,
        const QString& deviceSerial,
        int deviceSequence) const
{
    DeviceConfigs::const_iterator it = m_deviceConfigs.begin();

    for (; it != m_deviceConfigs.end(); ++it)
	{
        if ((it->m_deviceId == deviceId) && (it->m_deviceSerial == deviceSerial) && (it->m_deviceSequence == deviceSequence)) {
            return &it->m_config;
        }
    }

    return nullptr;
}
//_samplingDeviceId, m_samplingDeviceSerial, m_samplingDeviceSequence
const QByteArray* Preset::findBestDeviceConfig(
        const QString& deviceId,
		const QString& deviceSerial,
		int deviceSequence) const
{
	// Special case for SoapySDR based on serial (driver name)
	if (deviceId == "sdrangel.samplesource.soapysdrinput") {
		return findBestDeviceConfigSoapy(deviceId, deviceSerial);
	} else if (deviceId == "sdrangel.samplesource.soapysdroutput") {
		return findBestDeviceConfigSoapy(deviceId, deviceSerial);
	}

	DeviceConfigs::const_iterator it = m_deviceConfigs.begin();
	DeviceConfigs::const_iterator itFirstOfKind = m_deviceConfigs.end();
	DeviceConfigs::const_iterator itMatchSequence = m_deviceConfigs.end();

	for (; it != m_deviceConfigs.end(); ++it)
	{
		if (it->m_deviceId == deviceId)
		{
			if (itFirstOfKind == m_deviceConfigs.end())
			{
				itFirstOfKind = it;
			}

			if (deviceSerial.isNull() || deviceSerial.isEmpty())
			{
				if (it->m_deviceSequence == deviceSequence)
				{
					break;
				}
			}
			else
			{
				if (it->m_deviceSerial == deviceSerial)
				{
					break;
				}
				else if(it->m_deviceSequence == deviceSequence)
				{
					itMatchSequence = it;
				}
			}
		}
	}

	if (it == m_deviceConfigs.end()) // no exact match
	{
		if (itMatchSequence != m_deviceConfigs.end()) // match device type and sequence ?
		{
			qDebug("Preset::findBestDeviceConfig: sequence matched: id: %s ser: %s seq: %d",
				qPrintable(itMatchSequence->m_deviceId), qPrintable(itMatchSequence->m_deviceSerial), itMatchSequence->m_deviceSequence);
			return &(itMatchSequence->m_config);
		}
		else if (itFirstOfKind != m_deviceConfigs.end()) // match just device type ?
		{
			qDebug("Preset::findBestDeviceConfig: first of kind matched: id: %s ser: %s seq: %d",
				qPrintable(itFirstOfKind->m_deviceId), qPrintable(itFirstOfKind->m_deviceSerial), itFirstOfKind->m_deviceSequence);
			return &(itFirstOfKind->m_config);
		}
		else // definitely not found !
		{
			qDebug("Preset::findBestDeviceConfig: no match");
			return nullptr;
		}
	}
	else // exact match
	{
		qDebug("Preset::findBestDeviceConfig: serial matched (exact): id: %s ser: %s",
			qPrintable(it->m_deviceId), qPrintable(it->m_deviceSerial));
		return &(it->m_config);
	}
}

const QByteArray* Preset::findBestDeviceConfigSoapy(const QString& sourceId, const QString& sourceSerial) const
{
	QStringList sourceSerialPieces = sourceSerial.split("-");

	if (sourceSerialPieces.size() == 0) {
		return 0; // unable to process
	}

	DeviceConfigs::const_iterator it = m_deviceConfigs.begin();
	DeviceConfigs::const_iterator itFirstOfKind = m_deviceConfigs.end();

	for (; it != m_deviceConfigs.end(); ++it)
	{
		if (it->m_deviceId != sourceId) // skip non matching device
		{
			continue;
		}
		else if (it->m_deviceSerial == sourceSerial) // exact match
		{
			break;
		}
		else // try to find best match on driver id (first part of serial)
		{
			QStringList serialPieces = it->m_deviceSerial.split("-");

			if (serialPieces.size() == 0)
			{
				continue;
			}
			else if (sourceSerialPieces[0] == serialPieces[0])
			{
				if (itFirstOfKind == m_deviceConfigs.end())
				{
					itFirstOfKind = it;
					break;
				}
			}
		}
	}

	if (it == m_deviceConfigs.end()) // no exact match
	{
		if (itFirstOfKind == m_deviceConfigs.end())
		{
			qDebug("Preset::findBestDeviceConfigSoapy: no match");
			return 0;
		}
		else
		{
			qDebug("Preset::findBestDeviceConfigSoapy: first of kind matched: id: %s ser: %s seq: %d",
				qPrintable(itFirstOfKind->m_deviceId), qPrintable(itFirstOfKind->m_deviceSerial), itFirstOfKind->m_deviceSequence);
			return &(itFirstOfKind->m_config);
		}
	}
	else // exact match
	{
		qDebug("Preset::findBestDeviceConfigSoapy: serial matched (exact): id: %s ser: %s seq: %d",
			qPrintable(it->m_deviceId), qPrintable(it->m_deviceSerial), it->m_deviceSequence);
		return &(it->m_config);
	}
}

QString Preset::getPresetTypeChar(PresetType presetType)
{
    if (presetType == PresetSource) {
        return "R";
    } else if (presetType == PresetSink) {
        return "T";
    } else if (presetType == PresetMIMO) {
        return "M";
    } else {
        return "X";
    }
}
