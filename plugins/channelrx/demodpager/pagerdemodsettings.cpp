///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <QDebug>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "pagerdemodsettings.h"

PagerDemodSettings::PagerDemodSettings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void PagerDemodSettings::resetToDefaults()
{
    m_baud = 1200;
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 20000.0f;
    m_fmDeviation = 4500.0f;
    m_decode = Standard;
    m_filterAddress = "";
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_scopeCh1 = 4;
    m_scopeCh2 = 9;
    m_logFilename = "pager_log.csv";
    m_logEnabled = false;
    m_rgbColor = QColor(200, 191, 231).rgb();
    m_title = "Pager Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_reverse = false;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_filterDuplicates = false;
    m_duplicateMatchMessageOnly = false;
    m_duplicateMatchLastOnly = false;

    for (int i = 0; i < PAGERDEMOD_MESSAGE_COLUMNS; i++)
    {
        m_messageColumnIndexes[i] = i;
        m_messageColumnSizes[i] = -1; // Autosize
    }
}

QByteArray PagerDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeFloat(3, m_fmDeviation);
    s.writeS32(4, m_baud);
    s.writeString(5, m_filterAddress);
    s.writeS32(6, (int)m_decode);
    s.writeBool(7, m_udpEnabled);
    s.writeString(8, m_udpAddress);
    s.writeU32(9, m_udpPort);
    s.writeS32(10, m_scopeCh1);
    s.writeS32(11, m_scopeCh2);
    s.writeU32(12, m_rgbColor);
    s.writeString(13, m_title);

    if (m_channelMarker) {
        s.writeBlob(14, m_channelMarker->serialize());
    }

    s.writeS32(15, m_streamIndex);
    s.writeBool(16, m_useReverseAPI);
    s.writeString(17, m_reverseAPIAddress);
    s.writeU32(18, m_reverseAPIPort);
    s.writeU32(19, m_reverseAPIDeviceIndex);
    s.writeU32(20, m_reverseAPIChannelIndex);
    s.writeBlob(21, m_scopeGUI->serialize());
    s.writeBool(22, m_reverse);
    s.writeBlob(23, serializeIntList(m_sevenbit));
    s.writeBlob(24, serializeIntList(m_unicode));
    s.writeString(25, m_logFilename);
    s.writeBool(26, m_logEnabled);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);

    s.writeList(31, m_notificationSettings);
    s.writeBool(32, m_filterDuplicates);
    s.writeBool(33, m_duplicateMatchMessageOnly);
    s.writeBool(34, m_duplicateMatchLastOnly);

    for (int i = 0; i < PAGERDEMOD_MESSAGE_COLUMNS; i++) {
        s.writeS32(100 + i, m_messageColumnIndexes[i]);
    }
    for (int i = 0; i < PAGERDEMOD_MESSAGE_COLUMNS; i++) {
        s.writeS32(200 + i, m_messageColumnSizes[i]);
    }

    return s.final();
}

bool PagerDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;
        QByteArray blob;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readFloat(2, &m_rfBandwidth, 20000.0f);
        d.readFloat(3, &m_fmDeviation, 4500.0f);
        d.readS32(4, &m_baud, 1200);
        d.readString(5, &m_filterAddress, "");
        d.readS32(6, (int*)&m_decode, (int)Standard);
        d.readBool(7, &m_udpEnabled);
        d.readString(8, &m_udpAddress);
        d.readU32(9, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        d.readS32(10, &m_scopeCh1, 4);
        d.readS32(11, &m_scopeCh2, 9);
        d.readU32(12, &m_rgbColor, QColor(200, 191, 231).rgb());
        d.readString(13, &m_title, "Pager Demodulator");

        if (m_channelMarker)
        {
            d.readBlob(14, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(15, &m_streamIndex, 0);
        d.readBool(16, &m_useReverseAPI, false);
        d.readString(17, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(18, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(19, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(20, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_scopeGUI)
        {
            d.readBlob(21, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readBool(22, &m_reverse, false);
        d.readBlob(23, &blob);
        deserializeIntList(blob, m_sevenbit);
        d.readBlob(24, &blob);
        deserializeIntList(blob, m_unicode);

        d.readString(25, &m_logFilename, "pager_log.csv");
        d.readBool(26, &m_logEnabled, false);

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);

        d.readList(31, &m_notificationSettings);

        d.readBool(32, &m_filterDuplicates);
        d.readBool(33, &m_duplicateMatchMessageOnly);
        d.readBool(34, &m_duplicateMatchLastOnly);

        for (int i = 0; i < PAGERDEMOD_MESSAGE_COLUMNS; i++) {
            d.readS32(100 + i, &m_messageColumnIndexes[i], i);
        }

        for (int i = 0; i < PAGERDEMOD_MESSAGE_COLUMNS; i++) {
            d.readS32(200 + i, &m_messageColumnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QByteArray PagerDemodSettings::serializeIntList(const QList<qint32>& ints) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data,  QIODevice::WriteOnly);
    (*stream) << ints;
    delete stream;
    return data;
}

void PagerDemodSettings::deserializeIntList(const QByteArray& data, QList<qint32>& ints)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> ints;
    delete stream;
}

PagerDemodSettings::NotificationSettings::NotificationSettings() :
    m_matchColumn(PagerDemodSettings::MESSAGE_COL_ADDRESS),
    m_highlight(false),
    m_highlightColor(Qt::red),
    m_plotOnMap(false)
{
}

void PagerDemodSettings::NotificationSettings::updateRegularExpression()
{
    m_regularExpression.setPattern(m_regExp);
    m_regularExpression.optimize();
    if (!m_regularExpression.isValid()) {
        qDebug() << "PagerDemodSettings::NotificationSettings: Regular expression is not valid: " << m_regExp;
    }
}

QByteArray PagerDemodSettings::NotificationSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_matchColumn);
    s.writeString(2, m_regExp);
    s.writeString(3, m_speech);
    s.writeString(4, m_command);
    s.writeBool(5, m_highlight);
    s.writeS32(6, m_highlightColor);
    s.writeBool(7, m_plotOnMap);

    return s.final();
}

bool PagerDemodSettings::NotificationSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray blob;

        d.readS32(1, &m_matchColumn);
        d.readString(2, &m_regExp);
        d.readString(3, &m_speech);
        d.readString(4, &m_command);
        d.readBool(5, &m_highlight, false);
        d.readS32(6, &m_highlightColor, QColor(Qt::red).rgba());
        d.readBool(7, &m_plotOnMap, false);

        updateRegularExpression();

        return true;
    }
    else
    {
        return false;
    }
}

QDataStream& operator<<(QDataStream& out, const PagerDemodSettings::NotificationSettings *settings)
{
    out << settings->serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, PagerDemodSettings::NotificationSettings*& settings)
{
    settings = new PagerDemodSettings::NotificationSettings();
    QByteArray data;
    in >> data;
    settings->deserialize(data);
    return in;
}
