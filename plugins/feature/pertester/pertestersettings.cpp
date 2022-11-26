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

void PERTesterSettings::applySettings(const QStringList& settingsKeys, const PERTesterSettings& settings)
{
    if (settingsKeys.contains("packetCount")) {
        m_packetCount = settings.m_packetCount;
    }
    if (settingsKeys.contains("interval")) {
        m_interval = settings.m_interval;
    }
    if (settingsKeys.contains("packet")) {
        m_packet = settings.m_packet;
    }
    if (settingsKeys.contains("txUDPAddress")) {
        m_txUDPAddress = settings.m_txUDPAddress;
    }
    if (settingsKeys.contains("txUDPPort")) {
        m_txUDPPort = settings.m_txUDPPort;
    }
    if (settingsKeys.contains("rxUDPAddress")) {
        m_rxUDPAddress = settings.m_rxUDPAddress;
    }
    if (settingsKeys.contains("rxUDPPort")) {
        m_rxUDPPort = settings.m_rxUDPPort;
    }
    if (settingsKeys.contains("ignoreLeadingBytes")) {
        m_ignoreLeadingBytes = settings.m_ignoreLeadingBytes;
    }
    if (settingsKeys.contains("ignoreTrailingBytes")) {
        m_ignoreTrailingBytes = settings.m_ignoreTrailingBytes;
    }
    if (settingsKeys.contains("start")) {
        m_start = settings.m_start;
    }
    if (settingsKeys.contains("satellites")) {
        m_satellites = settings.m_satellites;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
}

QString PERTesterSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("packetCount") || force) {
        ostr << " m_packetCount: " << m_packetCount;
    }
    if (settingsKeys.contains("interval") || force) {
        ostr << " m_interval: " << m_interval;
    }
    if (settingsKeys.contains("packet") || force) {
        ostr << " m_packet: " << m_packet.toStdString();
    }
    if (settingsKeys.contains("txUDPAddress") || force) {
        ostr << " m_txUDPAddress: " << m_txUDPAddress.toStdString();
    }
    if (settingsKeys.contains("txUDPPort") || force) {
        ostr << " m_txUDPPort: " << m_txUDPPort;
    }
    if (settingsKeys.contains("rxUDPAddress") || force) {
        ostr << " m_rxUDPAddress: " << m_rxUDPAddress.toStdString();
    }
    if (settingsKeys.contains("rxUDPPort") || force) {
        ostr << " m_rxUDPPort: " << m_rxUDPPort;
    }
    if (settingsKeys.contains("ignoreLeadingBytes") || force) {
        ostr << " m_ignoreLeadingBytes: " << m_ignoreLeadingBytes;
    }
    if (settingsKeys.contains("ignoreTrailingBytes") || force) {
        ostr << " m_ignoreTrailingBytes: " << m_ignoreTrailingBytes;
    }
    if (settingsKeys.contains("start") || force) {
        ostr << " m_start: " << m_start;
    }
    if (settingsKeys.contains("satellites") || force)
    {
        ostr << " m_satellites:";

        for (auto satellite : m_satellites) {
            ostr << " " << satellite.toStdString();
        }
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
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
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        ostr << " m_reverseAPIFeatureSetIndex: " << m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex") || force) {
        ostr << " m_reverseAPIFeatureIndex: " << m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }

    return QString(ostr.str().c_str());
}
