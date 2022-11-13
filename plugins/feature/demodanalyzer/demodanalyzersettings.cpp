///////////////////////////////////////////////////////////////////////////////////
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "demodanalyzersettings.h"

const QStringList DemodAnalyzerSettings::m_channelTypes = {
    QStringLiteral("AISDemod"),
    QStringLiteral("AISMod"),
    QStringLiteral("AMDemod"),
    QStringLiteral("AMMod"),
    QStringLiteral("BFMDemod"),
    QStringLiteral("DABDemod"),
    QStringLiteral("DSDDemod"),
    QStringLiteral("M17Demod"),
    QStringLiteral("M17Mmod"),
    QStringLiteral("NFMDemod"),
    QStringLiteral("NFMMod"),
    QStringLiteral("PacketDemod"),
    QStringLiteral("PacketMod"),
    QStringLiteral("RadiosondeDemod"),
    QStringLiteral("SSBDemod"),
    QStringLiteral("SSBMod"),
    QStringLiteral("WFMDemod"),
    QStringLiteral("WFMMod"),
};

const QStringList DemodAnalyzerSettings::m_channelURIs = {
    QStringLiteral("sdrangel.channel.aisdemod"),
    QStringLiteral("sdrangel.channel.modais"),
    QStringLiteral("sdrangel.channel.amdemod"),
    QStringLiteral("sdrangel.channeltx.modam"),
    QStringLiteral("sdrangel.channel.bfm"),
    QStringLiteral("sdrangel.channel.dabdemod"),
    QStringLiteral("sdrangel.channel.dsddemod"),
    QStringLiteral("sdrangel.channel.m17demod"),
    QStringLiteral("sdrangel.channeltx.modm17"),
    QStringLiteral("sdrangel.channel.nfmdemod"),
    QStringLiteral("sdrangel.channeltx.modnfm"),
    QStringLiteral("sdrangel.channel.packetdemod"),
    QStringLiteral("sdrangel.channeltx.modpacket"),
    QStringLiteral("sdrangel.channel.radiosondedemod"),
    QStringLiteral("sdrangel.channel.ssbdemod"),
    QStringLiteral("sdrangel.channeltx.modssb"),
    QStringLiteral("sdrangel.channel.wfmdemod"),
    QStringLiteral("sdrangel.channeltx.modwfm"),
};

DemodAnalyzerSettings::DemodAnalyzerSettings() :
    m_spectrumGUI(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DemodAnalyzerSettings::resetToDefaults()
{
    m_log2Decim = 0;
    m_title = "Demod Analyzer";
    m_rgbColor = QColor(128, 128, 128).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
    m_fileRecordName.clear();
    m_recordToFile = false;
    m_recordSilenceTime = 0;
}

QByteArray DemodAnalyzerSettings::serialize() const
{
    SimpleSerializer s(1);

    if (m_spectrumGUI) {
        s.writeBlob(1, m_spectrumGUI->serialize());
    }

    if (m_scopeGUI) {
        s.writeBlob(2, m_scopeGUI->serialize());
    }

    s.writeS32(3, m_log2Decim);
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
    s.writeString(15, m_fileRecordName);
    s.writeBool(16, m_recordToFile);
    s.writeS32(17, m_recordSilenceTime);

    return s.final();
}

bool DemodAnalyzerSettings::deserialize(const QByteArray& data)
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

        if (m_spectrumGUI)
        {
            d.readBlob(1, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        if (m_scopeGUI)
        {
            d.readBlob(2, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readS32(3, &m_log2Decim, 0);
        d.readString(5, &m_title, "Demod Analyzer");
        d.readU32(6, &m_rgbColor, QColor(128, 128, 128).rgb());
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
        d.readString(15, &m_fileRecordName);
        d.readBool(16, &m_recordToFile, false);
        d.readS32(17, &m_recordSilenceTime, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
