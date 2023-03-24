///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#include "aaroniartsainputsettings.h"

AaroniaRTSAInputSettings::AaroniaRTSAInputSettings()
{
    resetToDefaults();
}

void AaroniaRTSAInputSettings::resetToDefaults()
{
    m_centerFrequency = 1450000;
    m_sampleRate = 200000;
	m_serverAddress = "127.0.0.1:8073";
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray AaroniaRTSAInputSettings::serialize() const
{
    SimpleSerializer s(2);

	s.writeString(2, m_serverAddress);
    s.writeS32(3, m_sampleRate);
    s.writeBool(100, m_useReverseAPI);
    s.writeString(101, m_reverseAPIAddress);
    s.writeU32(102, m_reverseAPIPort);
    s.writeU32(103, m_reverseAPIDeviceIndex);

	return s.final();
}

bool AaroniaRTSAInputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 2)
    {
		uint32_t utmp;

		d.readString(2, &m_serverAddress, "127.0.0.1:8073");
        d.readS32(3, &m_sampleRate, 200000);
		d.readBool(100, &m_useReverseAPI, false);
		d.readString(101, &m_reverseAPIAddress, "127.0.0.1");
		d.readU32(102, &utmp, 0);

		if ((utmp > 1023) && (utmp < 65535)) {
			m_reverseAPIPort = utmp;
		}
		else {
			m_reverseAPIPort = 8888;
		}

		d.readU32(103, &utmp, 0);
		m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AaroniaRTSAInputSettings::applySettings(const QStringList& settingsKeys, const AaroniaRTSAInputSettings& settings)
{
    if (settingsKeys.contains("centerFrequency")) {
        m_centerFrequency = settings.m_centerFrequency;
    }
    if (settingsKeys.contains("sampleRate")) {
        m_sampleRate = settings.m_sampleRate;
    }
    if (settingsKeys.contains("serverAddress")) {
        m_serverAddress = settings.m_serverAddress;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
}

QString AaroniaRTSAInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("centerFrequency") || force) {
        ostr << " m_centerFrequency: " << m_centerFrequency;
    }
    if (settingsKeys.contains("sampleRate") || force) {
        ostr << " m_sampleRate: " << m_sampleRate;
    }
    if (settingsKeys.contains("serverAddress") || force) {
        ostr << " m_serverAddress: " << m_serverAddress.toStdString();
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }

    return QString(ostr.str().c_str());
}
