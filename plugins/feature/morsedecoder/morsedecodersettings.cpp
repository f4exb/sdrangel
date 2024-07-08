///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include <QStringList>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "morsedecodersettings.h"

const QStringList MorseDecoderSettings::m_channelURIs = {
    QStringLiteral("sdrangel.channel.amdemod"),
    QStringLiteral("sdrangel.channel.nfmdemod"),
    QStringLiteral("sdrangel.channel.ssbdemod"),
    QStringLiteral("sdrangel.channel.wfmdemod"),
    QStringLiteral("sdrangel.channel.wdsprx"),
};

MorseDecoderSettings::MorseDecoderSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void MorseDecoderSettings::resetToDefaults()
{
    m_title = "Morse Decoder";
    m_rgbColor = QColor(0, 255, 0).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_logFilename = "cw_log.txt";
    m_logEnabled = false;
    m_auto = true;
    m_showThreshold = false;
}

QByteArray MorseDecoderSettings::serialize() const
{
    SimpleSerializer s(1);

    if (m_scopeGUI) {
        s.writeBlob(2, m_scopeGUI->serialize());
    }

    s.writeString(5, m_title);
    s.writeU32(6, m_rgbColor);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIFeatureSetIndex);
    s.writeU32(11, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(12, m_rollupState->serialize());
    }

    s.writeS32(13, m_workspaceIndex);
    s.writeBlob(14, m_geometryBytes);

    s.writeBool(22, m_udpEnabled);
    s.writeString(23, m_udpAddress);
    s.writeU32(24, m_udpPort);
    s.writeString(25, m_logFilename);
    s.writeBool(26, m_logEnabled);
    s.writeBool(27, m_auto);
    s.writeBool(28, m_showThreshold);

    return s.final();
}

bool MorseDecoderSettings::deserialize(const QByteArray& data)
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

        if (m_scopeGUI)
        {
            d.readBlob(2, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readString(5, &m_title, "Demod Analyzer");
        d.readU32(6, &m_rgbColor, QColor(0, 255, 0).rgb());
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(11, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(12, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(13, &m_workspaceIndex, 0);
        d.readBlob(14, &m_geometryBytes);

        d.readBool(22, &m_udpEnabled);
        d.readString(23, &m_udpAddress);
        d.readU32(24, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        d.readString(25, &m_logFilename, "cw_log.txt");
        d.readBool(26, &m_logEnabled, false);
        d.readBool(27, &m_auto, true);
        d.readBool(28, &m_showThreshold, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void MorseDecoderSettings::applySettings(const QStringList& settingsKeys, const MorseDecoderSettings& settings)
{
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
    if (settingsKeys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (settingsKeys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
    if (settingsKeys.contains("auto")) {
        m_auto = settings.m_auto;
    }
    if (settingsKeys.contains("showThreshold")) {
        m_showThreshold = settings.m_showThreshold;
    }
    if (settingsKeys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (settingsKeys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
}

QString MorseDecoderSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

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
    if (settingsKeys.contains("udpEnabled") || force) {
        ostr << " m_udpEnabled: " << m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress") || force) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort") || force) {
        ostr << " m_udpPort: " << m_udpPort;
    }
    if (settingsKeys.contains("logEnabled") || force) {
        ostr << " m_logEnabled: " << m_logEnabled;
    }
    if (settingsKeys.contains("logFilename") || force) {
        ostr << " m_logFilename: " << m_logFilename.toStdString();
    }
    if (settingsKeys.contains("auto") || force) {
        ostr << " m_auto: " << m_auto;
    }
    if (settingsKeys.contains("showThreshold") || force) {
        ostr << " m_showThreshold: " << m_showThreshold;
    }
    if (settingsKeys.contains("logEnabled") || force) {
        ostr << " m_logEnabled: " << m_logEnabled;
    }
    if (settingsKeys.contains("logFilename") || force) {
        ostr << " m_logFilename: " << m_logFilename.toStdString();
    }

    return QString(ostr.str().c_str());
}

QString MorseDecoderSettings::formatText(const QString& text)
{
    // Format text
    QString showText = text.simplified();
    QString spaceFirst;
    QString spaceLast;

    if (text.size() >= 2)
    {
        if (text.right(1)[0].isSpace()) {
            spaceLast = text.right(1);
        }
        if (text.left(1)[0].isSpace()) {
            spaceFirst = text.left(1);
        }
    }

    if (spaceFirst.size() != 0) {
        showText = spaceFirst + showText;
    }

    if (spaceLast.size() != 0) {
        showText.append(spaceLast);
    }

    return showText;
}
