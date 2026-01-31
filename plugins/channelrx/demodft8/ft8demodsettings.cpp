///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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
#include <QCoreApplication>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "maincore.h"
#include "util/maidenhead.h"
#include "ft8demodsettings.h"

const int FT8DemodSettings::m_ft8SampleRate = 12000;
#ifdef SDR_RX_SAMPLE_24BIT
const int FT8DemodSettings::m_minPowerThresholdDB = -120;
const float FT8DemodSettings::m_mminPowerThresholdDBf = 120.0f;
#else
const int FT8DemodSettings::m_minPowerThresholdDB = -100;
const float FT8DemodSettings::m_mminPowerThresholdDBf = 100.0f;
#endif

FT8DemodSettings::FT8DemodSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_rollupState(nullptr)
{
    m_filterBank.resize(10);
    resetToDefaults();
}

void FT8DemodSettings::resetToDefaults()
{
    m_agc = false;
    m_recordWav = false;
    m_logMessages = false;
    m_nbDecoderThreads = 3;
    m_decoderTimeBudget = 0.5;
    m_useOSD = false;
    m_osdDepth = 0;
    m_osdLDPCThreshold = 70;
    m_verifyOSD = false;
    m_volume = 1.0;
    m_inputFrequencyOffset = 0;
    m_rgbColor = QColor(0, 192, 255).rgb();
    m_title = "FT8 Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_filterIndex = 0;
    m_enablePSKReporter = false;
    m_pskReporterCallsign = getDefaultReporterCallsign();
    m_pskReporterLocator = getDefaultReporterLocator();
    m_pskReporterSoftware = "SDRangel FT8 Demod";
    resetBandPresets();
}

void FT8DemodSettings::resetBandPresets()
{
    m_bandPresets.clear();
    m_bandPresets.push_back(FT8DemodBandPreset{ "160m",   1840, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "80m",   3573, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "60m",   5357, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "40m",   7074, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "30m",  10136, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "20m",  14074, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "17m",  18100, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "15m",  21074, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "12m",  24915, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{  "10m",  28074, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{   "6m",  50313, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{   "4m",  70100, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{   "2m", 144120, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{"1.25m", 222065, 0});
    m_bandPresets.push_back(FT8DemodBandPreset{ "70cm", 432065, 0});
}

QByteArray FT8DemodSettings::serialize() const
{
    SimpleSerializer s(1);
    QByteArray bytetmp;

    QDataStream *stream = new QDataStream(&bytetmp, QIODevice::WriteOnly);
    *stream << m_bandPresets;
    delete stream;
    s.writeBlob(2, bytetmp);


    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(3, m_volume * 10.0);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);
    s.writeBool(6, m_recordWav);
    s.writeBool(7, m_logMessages);
    s.writeS32(8, m_nbDecoderThreads);
    s.writeFloat(9, m_decoderTimeBudget);
    s.writeBool(11, m_agc);
    s.writeBool(12, m_useOSD);
    s.writeS32(13, m_osdDepth);
    s.writeS32(14, m_osdLDPCThreshold);
    s.writeBool(15, m_verifyOSD);
    s.writeString(16, m_title);
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIDeviceIndex);
    s.writeU32(22, m_reverseAPIChannelIndex);
    s.writeS32(23, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(24, m_rollupState->serialize());
    }

    s.writeS32(25, m_workspaceIndex);
    s.writeBlob(26, m_geometryBytes);
    s.writeBool(27, m_hidden);
    s.writeU32(29, m_filterIndex);
    s.writeBool(30, m_enablePSKReporter);
    s.writeString(31, m_pskReporterCallsign);
    s.writeString(32, m_pskReporterLocator);
    s.writeString(33, m_pskReporterSoftware);

    for (unsigned int i = 0; i <  10; i++)
    {
        s.writeS32(100 + 10*i, m_filterBank[i].m_spanLog2);
        s.writeS32(101 + 10*i, m_filterBank[i].m_rfBandwidth / 100.0);
        s.writeS32(102 + 10*i, m_filterBank[i].m_lowCutoff / 100.0);
        s.writeS32(103 + 10*i, (int) m_filterBank[i].m_fftWindow);
    }

    return s.final();
}

bool FT8DemodSettings::deserialize(const QByteArray& data)
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
        QString strtmp;

        d.readBlob(2, &bytetmp);
        QDataStream readStream(&bytetmp, QIODevice::ReadOnly);
        readStream >> m_bandPresets;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(3, &tmp, 30);
        m_volume = tmp / 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);
        d.readBool(6, &m_recordWav, false);
        d.readBool(7, &m_logMessages, false);
        d.readS32(8, &m_nbDecoderThreads, 3);
        d.readFloat(9, &m_decoderTimeBudget, 0.5);
        d.readBool(11, &m_agc, false);
        d.readBool(12, &m_useOSD, false);
        d.readS32(13, &m_osdDepth, 0);
        d.readS32(14, &m_osdLDPCThreshold, 70);
        d.readBool(15, &m_verifyOSD, false);
        d.readString(16, &m_title, "SSB Demodulator");
        d.readBool(18, &m_useReverseAPI, false);
        d.readString(19, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(20, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(21, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(22, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(23, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(24, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(25, &m_workspaceIndex, 0);
        d.readBlob(26, &m_geometryBytes);
        d.readBool(27, &m_hidden, false);
        d.readU32(29, &utmp, 0);
        m_filterIndex = utmp < 10 ? utmp : 0;
        d.readBool(30, &m_enablePSKReporter, false);
        d.readString(31, &m_pskReporterCallsign, getDefaultReporterCallsign());
        d.readString(32, &m_pskReporterLocator, getDefaultReporterLocator());
        d.readString(33, &m_pskReporterSoftware, getDefaultReporterSoftware());

        for (unsigned int i = 0; (i < 10); i++)
        {
            d.readS32(100 + 10*i, &m_filterBank[i].m_spanLog2, 3);
            d.readS32(101 + 10*i, &tmp, 30);
            tmp = tmp > 58 ? 58 : tmp; // Hard limit upper bandwidth fixes #2339
            m_filterBank[i].m_rfBandwidth = tmp * 100.0;
            d.readS32(102+ 10*i, &tmp, 3);
            m_filterBank[i].m_lowCutoff = tmp * 100.0;
            d.readS32(103 + 10*i, &tmp, (int) FFTWindow::Blackman);
            m_filterBank[i].m_fftWindow =
                (FFTWindow::Function) (tmp < 0 ? 0 : tmp > (int) FFTWindow::BlackmanHarris7 ? (int) FFTWindow::BlackmanHarris7 : tmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void FT8DemodSettings::applySettings(const QStringList& settingsKeys, const FT8DemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("agc")) {
        m_agc = settings.m_agc;
    }
    if (settingsKeys.contains("recordWav")) {
        m_recordWav = settings.m_recordWav;
    }
    if (settingsKeys.contains("logMessages")) {
        m_logMessages = settings.m_logMessages;
    }
    if (settingsKeys.contains("nbDecoderThreads")) {
        m_nbDecoderThreads = settings.m_nbDecoderThreads;
    }
    if (settingsKeys.contains("decoderTimeBudget")) {
        m_decoderTimeBudget = settings.m_decoderTimeBudget;
    }
    if (settingsKeys.contains("useOSD")) {
        m_useOSD = settings.m_useOSD;
    }
    if (settingsKeys.contains("osdDepth")) {
        m_osdDepth = settings.m_osdDepth;
    }
    if (settingsKeys.contains("osdLDPCThreshold")) {
        m_osdLDPCThreshold = settings.m_osdLDPCThreshold;
    }
    if (settingsKeys.contains("verifyOSD")) {
        m_verifyOSD = settings.m_verifyOSD;
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
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
    if (settingsKeys.contains("filterBank")) {
        m_filterBank = settings.m_filterBank;
    }
    if (settingsKeys.contains("filterIndex")) {
        m_filterIndex = settings.m_filterIndex;
    }
    if (settingsKeys.contains("bandPresets")) {
        m_bandPresets = settings.m_bandPresets;
    }
    if (settingsKeys.contains("enablePSKReporter")) {
        m_enablePSKReporter = settings.m_enablePSKReporter;
    }
    if (settingsKeys.contains("pskReporterCallsign")) {
        m_pskReporterCallsign = settings.m_pskReporterCallsign;
    }
    if (settingsKeys.contains("pskReporterLocator")) {
        m_pskReporterLocator = settings.m_pskReporterLocator;
    }
    if (settingsKeys.contains("pskReporterSoftware")) {
        m_pskReporterSoftware = settings.m_pskReporterSoftware;
    }
}

QString FT8DemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("volume") || force) {
        ostr << " m_volume: " << m_volume;
    }
    if (settingsKeys.contains("agc") || force) {
        ostr << " m_agc: " << m_agc;
    }
    if (settingsKeys.contains("recordWav") || force) {
        ostr << " m_recordWav: " << m_recordWav;
    }
    if (settingsKeys.contains("logMessages") || force) {
        ostr << " m_logMessages: " << m_logMessages;
    }
    if (settingsKeys.contains("nbDecoderThreads") || force) {
        ostr << " m_nbDecoderThreads: " << m_nbDecoderThreads;
    }
    if (settingsKeys.contains("decoderTimeBudget") || force) {
        ostr << " m_decoderTimeBudget: " << m_decoderTimeBudget;
    }
    if (settingsKeys.contains("useOSD") || force) {
        ostr << " m_useOSD: " << m_useOSD;
    }
    if (settingsKeys.contains("osdDepth") || force) {
        ostr << " m_osdDepth: " << m_osdDepth;
    }
    if (settingsKeys.contains("osdLDPCThreshold") || force) {
        ostr << " m_osdLDPCThreshold: " << m_osdLDPCThreshold;
    }
    if (settingsKeys.contains("verifyOSD") || force) {
        ostr << " m_verifyOSD: " << m_verifyOSD;
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
    if (settingsKeys.contains("filterIndex") || force) {
        ostr << " m_filterIndex: " << m_filterIndex;
    }
    if (settingsKeys.contains("enablePSKReporter") || force) {
        ostr << " m_enablePSKReporter: " << m_enablePSKReporter;
    }
    if (settingsKeys.contains("pskReporterCallsign") || force) {
        ostr << " m_pskReporterCallsign: " << m_pskReporterCallsign.toStdString();
    }
    if (settingsKeys.contains("pskReporterLocator") || force) {
        ostr << " m_pskReporterLocator: " << m_pskReporterLocator.toStdString();
    }
    if (settingsKeys.contains("pskReporterSoftware") || force) {
        ostr << " m_pskReporterSoftware: " << m_pskReporterSoftware.toStdString();
    }

    return QString(ostr.str().c_str());
}

QDataStream& operator<<(QDataStream& out, const FT8DemodBandPreset& bandPreset)
{
    out << bandPreset.m_name;
    out << bandPreset.m_baseFrequency;
    out << bandPreset.m_channelOffset;
    return out;
}

QDataStream& operator>>(QDataStream& in, FT8DemodBandPreset& bandPreset)
{
    in >> bandPreset.m_name;
    in >> bandPreset.m_baseFrequency;
    in >> bandPreset.m_channelOffset;
    return in;
}

QString FT8DemodSettings::getDefaultReporterCallsign() const
{
    return MainCore::instance()->getSettings().getStationName();
}

QString FT8DemodSettings::getDefaultReporterLocator() const
{
    float lat = MainCore::instance()->getSettings().getLatitude();
    float lon = MainCore::instance()->getSettings().getLongitude();
    return Maidenhead::toMaidenhead(lat, lon);
}

QString FT8DemodSettings::getDefaultReporterSoftware() const
{
    return QCoreApplication::applicationName() + " " + QCoreApplication::applicationVersion();
}
