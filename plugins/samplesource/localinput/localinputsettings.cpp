///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include "localinputsettings.h"

LocalInputSettings::LocalInputSettings()
{
    resetToDefaults();
}

void LocalInputSettings::resetToDefaults()
{
    m_dcBlock = false;
    m_iqCorrection = false;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray LocalInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeBool(1, m_dcBlock);
    s.writeBool(2, m_iqCorrection);
    s.writeBool(3, m_useReverseAPI);
    s.writeString(4, m_reverseAPIAddress);
    s.writeU32(5, m_reverseAPIPort);
    s.writeU32(6, m_reverseAPIDeviceIndex);

    return s.final();
}

bool LocalInputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        quint32 uintval;

        d.readBool(1, &m_dcBlock, false);
        d.readBool(2, &m_iqCorrection, false);
        d.readBool(3, &m_useReverseAPI, false);
        d.readString(4, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(5, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(6, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void LocalInputSettings::applySettings(const QStringList& settingsKeys, const LocalInputSettings& settings)
{
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
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

QString LocalInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
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
