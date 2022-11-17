///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QColor>
#include <QDataStream>
#include <QIODevice>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "pertestersettings.h"

PERTesterSettings::PERTesterSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void PERTesterSettings::resetToDefaults()
{
    m_packetCount = 10;
    m_interval = 1.0f;
    m_packet = "%{ax25.dst=MYCALL} %{ax25.src=MYCALL} 03 f0 %{num} %{data=0,100}";
    m_txUDPAddress = "127.0.0.1";
    m_txUDPPort = 9998;
    m_rxUDPAddress = "127.0.0.1";
    m_rxUDPPort = 9999;
    m_ignoreLeadingBytes = 0;
    m_ignoreTrailingBytes = 2; // Ignore CRC
    m_start = START_IMMEDIATELY;
    m_satellites = {QString("ISS")};
    m_title = "Packet Error Rate Tester";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray PERTesterSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_packetCount);
    s.writeFloat(2, m_interval);
    s.writeString(3, m_txUDPAddress);
    s.writeU32(4, m_txUDPPort);
    s.writeString(5, m_rxUDPAddress);
    s.writeU32(6, m_rxUDPPort);
    s.writeS32(7, m_ignoreLeadingBytes);
    s.writeS32(8, m_ignoreTrailingBytes);
    s.writeS32(9, (int)m_start);
    s.writeBlob(10, serializeStringList(m_satellites));
    s.writeString(20, m_title);
    s.writeU32(21, m_rgbColor);
    s.writeBool(22, m_useReverseAPI);
    s.writeString(23, m_reverseAPIAddress);
    s.writeU32(24, m_reverseAPIPort);
    s.writeU32(25, m_reverseAPIFeatureSetIndex);
    s.writeU32(26, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);

    return s.final();
}

bool PERTesterSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;
        QByteArray blob;

        d.readS32(1, &m_packetCount);
        d.readFloat(2, &m_interval, 1.0f);
        d.readString(3, &m_txUDPAddress);
        d.readU32(4, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_txUDPPort = utmp;
        } else {
            m_txUDPPort = 8888;
        }

        d.readString(5, &m_rxUDPAddress);
        d.readU32(6, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_rxUDPPort = utmp;
        } else {
            m_rxUDPPort = 8888;
        }

        d.readS32(7, &m_ignoreLeadingBytes, 0);
        d.readS32(8, &m_ignoreTrailingBytes, 2);
        d.readS32(9, (int*)&m_start, (int)START_IMMEDIATELY);
        d.readBlob(10, &blob);
        deserializeStringList(blob, m_satellites);
        d.readString(20, &m_title, "Packet Error Rate Tester");
        d.readU32(21, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(25, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(26, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QByteArray PERTesterSettings::serializeStringList(const QList<QString>& strings) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data,  QIODevice::WriteOnly);
    (*stream) << strings;
    delete stream;
    return data;
}

void PERTesterSettings::deserializeStringList(const QByteArray& data, QList<QString>& strings)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> strings;
    delete stream;
}
