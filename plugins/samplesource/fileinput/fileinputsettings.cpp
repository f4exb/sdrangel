///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2019 Edouard Griffiths, F4EXB                              //
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

#include <QDebug>
#include "util/simpleserializer.h"

#include "fileinputsettings.h"

const unsigned int FileInputSettings::m_accelerationMaxScale = 2;

FileInputSettings::FileInputSettings()
{
    resetToDefaults();
}

void FileInputSettings::resetToDefaults()
{
    m_fileName = "";
    m_accelerationFactor = 1;
    m_loop = true;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray FileInputSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeString(1, m_fileName);
    s.writeU32(2, m_accelerationFactor);
    s.writeBool(3, m_loop);
    s.writeBool(4, m_useReverseAPI);
    s.writeString(5, m_reverseAPIAddress);
    s.writeU32(6, m_reverseAPIPort);
    s.writeU32(7, m_reverseAPIDeviceIndex);

    return s.final();
}

bool FileInputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        uint32_t uintval;

        d.readString(1, &m_fileName, "");
        d.readU32(2, &m_accelerationFactor, 1);
        d.readBool(3, &m_loop, true);
        d.readBool(4, &m_useReverseAPI, false);
        d.readString(5, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(6, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(7, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int FileInputSettings::getAccelerationIndex(int accelerationValue)
{
    if (accelerationValue <= 1) {
        return 0;
    }

    int v = accelerationValue;
    int j = 0;

    for (int i = 0; i <= accelerationValue; i++)
    {
        if (v < 20)
        {
            if (v < 2) {
                j = 0;
            } else if (v < 5) {
                j = 1;
            } else if (v < 10) {
                j = 2;
            } else {
                j = 3;
            }

            return 3*i + j;
        }

        v /= 10;
    }

    return 3*m_accelerationMaxScale + 3;
}

int FileInputSettings::getAccelerationValue(int accelerationIndex)
{
    if (accelerationIndex <= 0) {
        return 1;
    }

    unsigned int v = accelerationIndex - 1;
    int m = pow(10.0, v/3 > m_accelerationMaxScale ? m_accelerationMaxScale : v/3);
    int x = 1;

    if (v % 3 == 0) {
        x = 2;
    } else if (v % 3 == 1) {
        x = 5;
    } else if (v % 3 == 2) {
        x = 10;
    }

    return x * m;
}

void FileInputSettings::applySettings(const QStringList& settingsKeys, const FileInputSettings& settings)
{
    if (settingsKeys.contains("fileName")) {
        m_fileName = settings.m_fileName;
    }
    if (settingsKeys.contains("accelerationFactor")) {
        m_accelerationFactor = settings.m_accelerationFactor;
    }
    if (settingsKeys.contains("loop")) {
        m_loop = settings.m_loop;
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

QString FileInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("m_fileName") || force) {
        ostr << " m_fileName: " << m_fileName.toStdString();
    }
    if (settingsKeys.contains("accelerationFactor") || force) {
        ostr << " m_accelerationFactor: " << m_accelerationFactor;
    }
    if (settingsKeys.contains("loop") || force) {
        ostr << " m_loop: " << m_loop;
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
