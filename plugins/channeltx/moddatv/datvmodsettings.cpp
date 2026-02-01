///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include "datvmodsettings.h"
#include "dvb-s/dvb-s.h"

const QStringList DATVModSettings::m_codeRateStrings = {"1/2", "2/3", "3/4", "5/6", "7/8", "4/5", "8/9", "9/10", "1/4", "1/3", "2/5", "3/5"};
const QStringList DATVModSettings::m_modulationStrings = {"BPSK", "QPSK", "8PSK", "16APSK", "32APSK"};
const QStringList DATVModSettings::m_codecStrings = {"HEVC", "H264"};

DATVModSettings::DATVModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DATVModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 1000000;
    m_standard = DVB_S;
    m_modulation = QPSK;
    m_fec = FEC12;
    m_symbolRate = 250000;
    m_rollOff = 0.35f;
    m_source = SourceFile;
    m_imageFileName = "";
    m_imageOverlayTimestamp = false;
    m_imageServiceProvider = "SDRangel";
    m_imageServiceName = "SDRangel_TV";
    m_imageCodec = CodecHEVC;
    m_tsFileName = "";
    m_tsFilePlayLoop = false;
    m_tsFilePlay = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 5004;
    m_channelMute = false;
    m_rgbColor = QColor(Qt::magenta).rgb();
    m_title = "DATV Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray DATVModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeS32(3, (int) m_standard);
    s.writeS32(4, (int) m_modulation);
    s.writeS32(5, (int) m_fec);
    s.writeS32(6, m_symbolRate);
    s.writeReal(7, m_rollOff);
    s.writeS32(10, (int) m_source);
    s.writeString(11, m_tsFileName);
    s.writeBool(12, m_tsFilePlayLoop);
    s.writeString(13, m_udpAddress);
    s.writeU32(14, m_udpPort);
    s.writeString(20, m_title);
    s.writeU32(21, m_rgbColor);

    if (m_channelMarker) {
        s.writeBlob(22, m_channelMarker->serialize());
    }

    s.writeBool(23, m_useReverseAPI);
    s.writeString(24, m_reverseAPIAddress);
    s.writeU32(25, m_reverseAPIPort);
    s.writeU32(26, m_reverseAPIDeviceIndex);
    s.writeU32(27, m_reverseAPIChannelIndex);
    s.writeS32(28, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(29, m_rollupState->serialize());
    }

    s.writeS32(30, m_workspaceIndex);
    s.writeBlob(31, m_geometryBytes);
    s.writeBool(32, m_hidden);
    s.writeString(33, m_imageFileName);
    s.writeBool(34, m_imageOverlayTimestamp);
    s.writeString(35, m_imageServiceProvider);
    s.writeString(36, m_imageServiceName);
    s.writeS32(37, (int) m_imageCodec);

    return s.final();
}

bool DATVModSettings::deserialize(const QByteArray& data)
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
        qint32 tmp;
        uint32_t utmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 1000000);
        d.readS32(3, (int *) &m_standard, (int)DVB_S);
        d.readS32(4, (int *) &m_modulation, (int)QPSK);
        d.readS32(5, (int *) &m_fec, (int)FEC12);
        d.readS32(6, &m_symbolRate, 250000);
        d.readReal(7, &m_rollOff, 0.35f);
        d.readS32(10, (int *)&m_source, (int)SourceFile);
        d.readString(11, &m_tsFileName);
        d.readBool(12, &m_tsFilePlayLoop, false);
        d.readString(13, &m_udpAddress, "127.0.0.1");
        d.readU32(14, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65536)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 5004;
        }

        d.readString(20, &m_title, "DATV Modulator");
        d.readU32(21, &m_rgbColor, QColor(Qt::magenta).rgb());

        if (m_channelMarker)
        {
            d.readBlob(22, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readBool(23, &m_useReverseAPI, false);
        d.readString(24, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(25, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(26, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(27, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(28, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(29, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(30, &m_workspaceIndex, 0);
        d.readBlob(31, &m_geometryBytes);
        d.readBool(32, &m_hidden, false);
        d.readString(33, &m_imageFileName, "");
        d.readBool(34, &m_imageOverlayTimestamp, false);
        d.readString(35, &m_imageServiceProvider, "SDRangel");
        d.readString(36, &m_imageServiceName, "SDRangel_TV");
        d.readS32(37, (int *)&m_imageCodec, (int)DATVModSettings::CodecHEVC);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

DATVModSettings::DATVCodeRate DATVModSettings::mapCodeRate(const QString& string)
{
    for (int i = 0; i < m_codeRateStrings.size(); i++)
    {
        if (string == m_codeRateStrings[i])
            return (DATVCodeRate)i;
    }
    return FEC12;
}

QString DATVModSettings::mapCodeRate(DATVCodeRate codeRate)
{
    return m_codeRateStrings[codeRate];
}

DATVModSettings::DATVModulation DATVModSettings::mapModulation(const QString& string)
{
    for (int i = 0; i < m_modulationStrings.size(); i++)
    {
        if (string == m_modulationStrings[i])
            return (DATVModulation)i;
    }
    return QPSK;
}

QString DATVModSettings::mapModulation(DATVModulation modulation)
{
    return m_modulationStrings[modulation];
}

int DATVModSettings::getDVBSDataBitrate() const
{
    float fecFactor;
    float plFactor;
    float bitsPerSymbol;

    switch (m_modulation)
    {
    case DATVModSettings::BPSK:
        bitsPerSymbol = 1.0f;
        break;
    case DATVModSettings::QPSK:
        bitsPerSymbol = 2.0f;
        break;
    case DATVModSettings::PSK8:
        bitsPerSymbol = 3.0f;
        break;
    case DATVModSettings::APSK16:
        bitsPerSymbol = 4.0f;
        break;
    case DATVModSettings::APSK32:
        bitsPerSymbol = 5.0f;
        break;
    }

    if (m_standard == DATVModSettings::DVB_S)
    {
        float rsFactor;
        float convFactor;

        rsFactor = DVBS::tsPacketLen/(float)DVBS::rsPacketLen;
        switch (m_fec)
        {
        case DATVModSettings::FEC12:
            convFactor = 1.0f/2.0f;
            break;
        case DATVModSettings::FEC23:
            convFactor = 2.0f/3.0f;
            break;
        case DATVModSettings::FEC34:
            convFactor = 3.0f/4.0f;
            break;
        case DATVModSettings::FEC56:
            convFactor = 5.0f/6.0f;
            break;
        case DATVModSettings::FEC78:
            convFactor = 7.0f/8.0f;
            break;
        case DATVModSettings::FEC45:
            convFactor = 4.0f/5.0f;
            break;
        case DATVModSettings::FEC89:
            convFactor = 8.0f/9.0f;
            break;
        case DATVModSettings::FEC910:
            convFactor = 9.0f/10.0f;
            break;
        case DATVModSettings::FEC14:
            convFactor = 1.0f/4.0f;
            break;
        case DATVModSettings::FEC13:
            convFactor = 1.0f/3.0f;
            break;
        case DATVModSettings::FEC25:
            convFactor = 2.0f/5.0f;
            break;
        case DATVModSettings::FEC35:
            convFactor = 3.0f/5.0f;
            break;
        }
        fecFactor = rsFactor * convFactor;
        plFactor = 1.0f;
    }
    else
    {
        // For normal frames
        int codedBlockSize = 64800;
        int bbHeaderBits = 80;
        int uncodedBlockSize = codedBlockSize + bbHeaderBits; // Yields a fec factor of 1.0 if no FEC applied
        // See table 5a in DVBS2 spec
        switch (m_fec)
        {
        case DATVModSettings::FEC12:
            uncodedBlockSize = 32208;
            break;
        case DATVModSettings::FEC23:
            uncodedBlockSize = 43040;
            break;
        case DATVModSettings::FEC34:
            uncodedBlockSize = 48408;
            break;
        case DATVModSettings::FEC56:
            uncodedBlockSize = 53840;
            break;
        case DATVModSettings::FEC45:
            uncodedBlockSize = 51648;
            break;
        case DATVModSettings::FEC89:
            uncodedBlockSize = 57472;
            break;
        case DATVModSettings::FEC910:
            uncodedBlockSize = 58192;
            break;
        case DATVModSettings::FEC14:
            uncodedBlockSize = 16008;
            break;
        case DATVModSettings::FEC13:
            uncodedBlockSize = 21408;
            break;
        case DATVModSettings::FEC25:
            uncodedBlockSize = 25728;
            break;
        case DATVModSettings::FEC35:
            uncodedBlockSize = 38688;
            break;
        default:
            qWarning("DATVModSettings::getDVBSDataBitrate: Unsupported DVB-S2 code rate");
            break;
        }
        fecFactor = (uncodedBlockSize-bbHeaderBits)/(float)codedBlockSize;
        float symbolsPerFrame = codedBlockSize/bitsPerSymbol;
        // 90 symbols for PL header
        plFactor = symbolsPerFrame / (symbolsPerFrame + 90.0f);
    }

    return std::round(m_symbolRate * bitsPerSymbol * fecFactor * plFactor);
}

void DATVModSettings::applySettings(const QStringList& settingsKeys, const DATVModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("standard")) {
        m_standard = settings.m_standard;
    }
    if (settingsKeys.contains("modulation")) {
        m_modulation = settings.m_modulation;
    }
    if (settingsKeys.contains("fec")) {
        m_fec = settings.m_fec;
    }
    if (settingsKeys.contains("symbolRate")) {
        m_symbolRate = settings.m_symbolRate;
    }
    if (settingsKeys.contains("rollOff")) {
        m_rollOff = settings.m_rollOff;
    }
    if (settingsKeys.contains("source")) {
        m_source = settings.m_source;
    }
    if (settingsKeys.contains("imageFileName")) {
        m_imageFileName = settings.m_imageFileName;
    }
    if (settingsKeys.contains("imageOverlayTimestamp")) {
        m_imageOverlayTimestamp = settings.m_imageOverlayTimestamp;
    }
    if (settingsKeys.contains("imageServiceProvider")) {
        m_imageServiceProvider = settings.m_imageServiceProvider;
    }
    if (settingsKeys.contains("imageServiceName")) {
        m_imageServiceName = settings.m_imageServiceName;
    }
    if (settingsKeys.contains("imageCodec")) {
        m_imageCodec = settings.m_imageCodec;
    }
    if (settingsKeys.contains("tsFileName")) {
        m_tsFileName = settings.m_tsFileName;
    }
    if (settingsKeys.contains("tsFilePlayLoop")) {
        m_tsFilePlayLoop = settings.m_tsFilePlayLoop;
    }
    if (settingsKeys.contains("tsFilePlay")) {
        m_tsFilePlay = settings.m_tsFilePlay;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
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
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString DATVModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("standard") || force) {
        ostr << " m_standard: " << m_standard;
    }
    if (settingsKeys.contains("modulation") || force) {
        ostr << " m_modulation: " << m_modulation;
    }
    if (settingsKeys.contains("fec") || force) {
        ostr << " m_fec: " << m_fec;
    }
    if (settingsKeys.contains("symbolRate") || force) {
        ostr << " m_symbolRate: " << m_symbolRate;
    }
    if (settingsKeys.contains("rollOff") || force) {
        ostr << " m_rollOff: " << m_rollOff;
    }
    if (settingsKeys.contains("source") || force) {
        ostr << " m_source: " << m_source;
    }
    if (settingsKeys.contains("imageFileName") || force) {
        ostr << " m_imageFileName: " << m_imageFileName.toStdString();
    }
    if (settingsKeys.contains("imageOverlayTimestamp") || force) {
        ostr << " m_imageOverlayTimestamp: " << m_imageOverlayTimestamp;
    }
    if (settingsKeys.contains("imageServiceProvider") || force) {
        ostr << " m_imageServiceProvider: " << m_imageServiceProvider.toStdString();
    }
    if (settingsKeys.contains("imageServiceName") || force) {
        ostr << " m_imageServiceName: " << m_imageServiceName.toStdString();
    }
    if (settingsKeys.contains("imageCodec") || force) {
        ostr << " m_imageCodec: " << m_imageCodec;
    }
    if (settingsKeys.contains("tsFileName") || force) {
        ostr << " m_tsFileName: " << m_tsFileName.toStdString();
    }
    if (settingsKeys.contains("tsFilePlayLoop") || force) {
        ostr << " m_tsFilePlayLoop: " << m_tsFilePlayLoop;
    }
    if (settingsKeys.contains("tsFilePlay") || force) {
        ostr << " m_tsFilePlay: " << m_tsFilePlay;
    }
    if (settingsKeys.contains("udpAddress") || force) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort") || force) {
        ostr << " m_udpPort: " << m_udpPort;
    }
    if (settingsKeys.contains("channelMute") || force) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
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
    if (settingsKeys.contains("reverseAPIChannelIndex") || force) {
        ostr << " m_reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
