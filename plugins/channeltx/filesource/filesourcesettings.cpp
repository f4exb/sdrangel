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

#include "filesourcesettings.h"

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"


FileSourceSettings::FileSourceSettings()
{
    resetToDefaults();
}

void FileSourceSettings::resetToDefaults()
{
    m_fileName = "test.sdriq";
    m_loop = false;
    m_log2Interp = 0;
    m_filterChainHash = 0;
    m_gainDB = 0;
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "File source";
    m_channelMarker = nullptr;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
}

QByteArray FileSourceSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeString(1, m_fileName);
    s.writeBool(2, m_loop);
    s.writeU32(3, m_log2Interp);
    s.writeU32(4, m_filterChainHash);
    s.writeS32(5, m_gainDB);
    s.writeU32(6, m_rgbColor);
    s.writeString(7, m_title);
    s.writeBool(8, m_useReverseAPI);
    s.writeString(9, m_reverseAPIAddress);
    s.writeU32(10, m_reverseAPIPort);
    s.writeU32(11, m_reverseAPIDeviceIndex);
    s.writeU32(12, m_reverseAPIChannelIndex);
    s.writeS32(13, m_streamIndex);

    return s.final();
}

bool FileSourceSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t tmp;
        int itmp;
        QString strtmp;

        d.readString(1, &m_fileName, "test.sdriq");
        d.readBool(2, &m_loop, false);
        d.readU32(3, &tmp, 0);
        m_log2Interp = tmp > 6 ? 6 : tmp;
        d.readU32(4, &m_filterChainHash, 0);
        d.readS32(5, &itmp, 20);
        m_gainDB = itmp < -10 ? -10 : itmp > 50 ? 50 : itmp;
        d.readU32(6, &m_rgbColor, QColor(140, 4, 4).rgb());
        d.readString(7, &m_title, "File source");
        d.readBool(8, &m_useReverseAPI, false);
        d.readString(9, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(10, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_reverseAPIPort = tmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(11, &tmp, 0);
        m_reverseAPIDeviceIndex = tmp > 99 ? 99 : tmp;
        d.readU32(12, &tmp, 0);
        m_reverseAPIChannelIndex = tmp > 99 ? 99 : tmp;
        d.readS32(13, &m_streamIndex, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
