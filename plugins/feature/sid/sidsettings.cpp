///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "channel/channelwebapiutils.h"
#include "device/deviceset.h"
#include "maincore.h"

#include "sidsettings.h"

// https://medialab.github.io/iwanthue/
// Restricted dark colours and chroma at either end
const QList<QRgb> SIDSettings::m_defaultColors = {
    0xdd4187,
    0x7ce048,
    0xc944db,
    0xd5d851,
    0x826add,
    0x5da242,
    0xc97bc1,
    0x85e49b,
    0xdf5035,
    0x57d6d9,
    0xd28e2e,
    0x7091d3,
    0xa3a052,
    0xd36d76,
    0x4aa47d,
    0xc9895a,
    };

const QList<QRgb> SIDSettings::m_defaultXRayShortColors = {
    0x8a3ffc,
    0x8a3ffc
};

const QList<QRgb> SIDSettings::m_defaultXRayLongColors = {
    0x4589ff,
    0x0f62fe
};

const QList<QRgb> SIDSettings::m_defaultProtonColors = {
    0x9ef0f0,
    0x3ddbd9,
    0x08bdba,
    0x009d9a
};

const QRgb SIDSettings::m_defaultGRBColor = 0xffffff;
const QRgb SIDSettings::m_defaultSTIXColor = 0xcccc00;

SIDSettings::SIDSettings() :
    m_rollupState(nullptr),
    m_workspaceIndex(0)
{
    resetToDefaults();
}

void SIDSettings::resetToDefaults()
{
    m_channelSettings = {};
    m_period = 10.0f;

    m_autosave = true;
    m_autoload = true;
    m_filename = "sid_autosave.csv";
    m_autosavePeriod = 10;

    m_samples = 1;
    m_autoscaleX = true;
    m_autoscaleY = true;
    m_separateCharts = true;
    m_displayLegend = true;
    m_legendAlignment = Qt::AlignTop;
    m_displayAxisTitles = true;
    m_displaySecondaryAxis = true;
    m_plotXRayLongPrimary = true;
    m_plotXRayLongSecondary = false;
    m_plotXRayShortPrimary = true;
    m_plotXRayShortSecondary = false;
    m_plotGRB = true;
    m_plotSTIX = true;
    m_plotProton = true;
    m_y1Min = -100.0f;
    m_y1Max = 0.0f;
    m_startDateTime = QDateTime();
    m_endDateTime = QDateTime();
    m_xrayShortColors = m_defaultXRayShortColors;
    m_xrayLongColors = m_defaultXRayLongColors;
    m_protonColors = m_defaultProtonColors;
    m_grbColor = m_defaultGRBColor;
    m_stixColor  =m_defaultSTIXColor;

    m_sdoEnabled = true;
    m_sdoVideoEnabled = false;
    m_sdoData = "";
    m_sdoNow = true;
    m_sdoDateTime = QDateTime();
    m_map = "";

    m_title = "SID";
    m_rgbColor = QColor(102, 0, 102).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
}

QByteArray SIDSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeList(1, m_channelSettings);
    s.writeFloat(2, m_period);

    s.writeBool(10, m_autosave);
    s.writeBool(11, m_autoload);
    s.writeString(12, m_filename);
    s.writeS32(13, m_autosavePeriod);

    s.writeS32(20, m_samples);
    s.writeBool(21, m_autoscaleX);
    s.writeBool(22, m_autoscaleY);
    s.writeBool(23, m_separateCharts);
    s.writeBool(24, m_displayLegend);
    s.writeS32(25, (int) m_legendAlignment);
    s.writeBool(26, m_displayAxisTitles);
    s.writeBool(27, m_displaySecondaryAxis);
    s.writeBool(28, m_plotXRayLongPrimary);
    s.writeBool(29, m_plotXRayLongSecondary);
    s.writeBool(30, m_plotXRayShortPrimary);
    s.writeBool(31, m_plotXRayShortSecondary);
    s.writeBool(32, m_plotGRB);
    s.writeBool(33, m_plotSTIX);
    s.writeBool(34, m_plotProton);

    s.writeFloat(36, m_y1Min);
    s.writeFloat(37, m_y1Max);
    if (m_startDateTime.isValid()) {
        s.writeS64(38, m_startDateTime.toMSecsSinceEpoch());
    }
    if (m_endDateTime.isValid()) {
        s.writeS64(39, m_endDateTime.toMSecsSinceEpoch());
    }

    s.writeList(40, m_xrayShortColors);
    s.writeList(41, m_xrayLongColors);
    s.writeList(42, m_protonColors);
    s.writeU32(43, m_grbColor);
    s.writeU32(44, m_stixColor);

    s.writeBool(50, m_sdoEnabled);
    s.writeBool(51, m_sdoVideoEnabled);
    s.writeString(52, m_sdoData);
    s.writeBool(53, m_sdoNow);
    if (m_sdoDateTime.isValid()) {
        s.writeS64(54, m_sdoDateTime.toMSecsSinceEpoch());
    }
    s.writeString(55, m_map);

    s.writeList(60, m_sdoSplitterSizes);
    s.writeList(61, m_chartSplitterSizes);

    s.writeString(70, m_title);
    s.writeU32(71, m_rgbColor);
    s.writeBool(72, m_useReverseAPI);
    s.writeString(73, m_reverseAPIAddress);
    s.writeU32(74, m_reverseAPIPort);
    s.writeU32(75, m_reverseAPIFeatureSetIndex);
    s.writeU32(76, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(77, m_rollupState->serialize());
    }

    s.writeS32(78, m_workspaceIndex);
    s.writeBlob(79, m_geometryBytes);

    return s.final();
}

bool SIDSettings::deserialize(const QByteArray& data)
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
        qint64 tmp64;
        QString strtmp;
        QByteArray blob;

        d.readList(1, &m_channelSettings);
        d.readFloat(2, &m_period, 10.0f);

        d.readBool(10, &m_autosave, true);
        d.readBool(11, &m_autoload, true);
        d.readString(12, &m_filename, "sid_autosave.csv");
        d.readS32(13, &m_autosavePeriod, 10);


        d.readS32(20, &m_samples, 1);
        d.readBool(21, &m_autoscaleX, true);
        d.readBool(22, &m_autoscaleY, true);
        d.readBool(23, &m_separateCharts, true);
        d.readBool(24, &m_displayLegend, true);
        d.readS32(25, (int *) &m_legendAlignment, Qt::AlignTop);
        d.readBool(26, &m_displayAxisTitles, true);
        d.readBool(27, &m_displaySecondaryAxis, true);
        d.readBool(28, &m_plotXRayLongPrimary, true);
        d.readBool(29, &m_plotXRayLongSecondary, false);
        d.readBool(30, &m_plotXRayShortPrimary, true);
        d.readBool(31, &m_plotXRayShortSecondary, false);
        d.readBool(32, &m_plotGRB, true);
        d.readBool(33, &m_plotSTIX, true);
        d.readBool(34, &m_plotProton, false);

        d.readFloat(36, &m_y1Min, -100.0f);
        d.readFloat(37, &m_y1Max, 0.0f);
        if (d.readS64(38, &tmp64)) {
            m_startDateTime = QDateTime::fromMSecsSinceEpoch(tmp64);
        } else {
            m_startDateTime = QDateTime();
        }
        if (d.readS64(39, &tmp64)) {
            m_endDateTime = QDateTime::fromMSecsSinceEpoch(tmp64);
        } else {
            m_endDateTime = QDateTime();
        }

        d.readList(40, &m_xrayShortColors);
        if (m_xrayShortColors.size() != 2) {
            m_xrayShortColors = m_defaultXRayShortColors;
        }
        d.readList(41, &m_xrayLongColors);
        if (m_xrayLongColors.size() != 2) {
            m_xrayLongColors = m_defaultXRayLongColors;
        }
        d.readList(42, &m_protonColors);
        if (m_protonColors.size() != 4) {
            m_protonColors = m_defaultProtonColors;
        }
        d.readU32(43, &m_grbColor, m_defaultGRBColor);
        d.readU32(44, &m_stixColor, m_defaultSTIXColor);

        d.readBool(50, &m_sdoEnabled, true);
        d.readBool(51, &m_sdoVideoEnabled, false);
        d.readString(52, &m_sdoData, "");
        d.readBool(53, &m_sdoNow);
        if (d.readS64(54, &tmp64)) {
            m_sdoDateTime = QDateTime::fromMSecsSinceEpoch(tmp64);
        } else {
            m_sdoDateTime = QDateTime();
        }
        d.readString(55, &m_map, "");

        d.readList(60, &m_sdoSplitterSizes);
        d.readList(61, &m_chartSplitterSizes);

        d.readString(70, &m_title, "SID");
        d.readU32(71, &m_rgbColor, QColor(102, 0, 102).rgb());
        d.readBool(72, &m_useReverseAPI, false);
        d.readString(73, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(74, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(75, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(76, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(77, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(78, &m_workspaceIndex, 0);
        d.readBlob(79, &m_geometryBytes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void SIDSettings::applySettings(const QStringList& settingsKeys, const SIDSettings& settings)
{
    if (settingsKeys.contains("channelSettings")) {
        m_channelSettings = settings.m_channelSettings;
    }
    if (settingsKeys.contains("period")) {
        m_period = settings.m_period;
    }
    if (settingsKeys.contains("autosave")) {
        m_autosave = settings.m_autosave;
    }
    if (settingsKeys.contains("autoload")) {
        m_autoload = settings.m_autoload;
    }
    if (settingsKeys.contains("autosavePeriod")) {
        m_autosavePeriod = settings.m_autosavePeriod;
    }
    if (settingsKeys.contains("filename")) {
        m_filename = settings.m_filename;
    }
    if (settingsKeys.contains("samples")) {
        m_samples = settings.m_samples;
    }
    if (settingsKeys.contains("autoscaleX")) {
        m_autoscaleX = settings.m_autoscaleX;
    }
    if (settingsKeys.contains("autoscaleY")) {
        m_autoscaleY = settings.m_autoscaleY;
    }
    if (settingsKeys.contains("separateCharts")) {
        m_separateCharts = settings.m_separateCharts;
    }
    if (settingsKeys.contains("displayLegend")) {
        m_displayLegend = settings.m_displayLegend;
    }
    if (settingsKeys.contains("legendAlignment")) {
        m_legendAlignment = settings.m_legendAlignment;
    }
    if (settingsKeys.contains("displayAxisTitles")) {
        m_displayAxisTitles = settings.m_displayAxisTitles;
    }
    if (settingsKeys.contains("displayAxisLabels")) {
        m_displaySecondaryAxis = settings.m_displaySecondaryAxis;
    }
    if (settingsKeys.contains("plotXRayLongPrimary")) {
        m_plotXRayLongPrimary = settings.m_plotXRayLongPrimary;
    }
    if (settingsKeys.contains("plotXRayLongSecondary")) {
        m_plotXRayLongSecondary = settings.m_plotXRayLongSecondary;
    }
    if (settingsKeys.contains("plotXRayShortPrimary")) {
        m_plotXRayShortPrimary = settings.m_plotXRayShortPrimary;
    }
    if (settingsKeys.contains("plotXRayShorSecondary")) {
        m_plotXRayShortSecondary = settings.m_plotXRayShortSecondary;
    }
    if (settingsKeys.contains("plotGRB")) {
        m_plotGRB = settings.m_plotGRB;
    }
    if (settingsKeys.contains("plotSTIX")) {
        m_plotSTIX = settings.m_plotSTIX;
    }
    if (settingsKeys.contains("plotProton")) {
        m_plotProton = settings.m_plotProton;
    }
    if (settingsKeys.contains("startDateTime")) {
        m_startDateTime = settings.m_startDateTime;
    }
    if (settingsKeys.contains("endDateTime")) {
        m_endDateTime = settings.m_endDateTime;
    }
    if (settingsKeys.contains("y1Min")) {
        m_y1Min = settings.m_y1Min;
    }
    if (settingsKeys.contains("y1Max")) {
        m_y1Max = settings.m_y1Max;
    }
    if (settingsKeys.contains("xrayShortColors")) {
        m_xrayShortColors = settings.m_xrayShortColors;
    }
    if (settingsKeys.contains("xrayLongColors")) {
        m_xrayLongColors = settings.m_xrayLongColors;
    }
    if (settingsKeys.contains("protonColors")) {
        m_protonColors = settings.m_protonColors;
    }
    if (settingsKeys.contains("grbColor")) {
        m_grbColor = settings.m_grbColor;
    }
    if (settingsKeys.contains("stixColor")) {
        m_stixColor = settings.m_stixColor;
    }
    if (settingsKeys.contains("sdoEnabled")) {
        m_sdoEnabled = settings.m_sdoEnabled;
    }
    if (settingsKeys.contains("sdoVideoEnabled")) {
        m_sdoVideoEnabled = settings.m_sdoVideoEnabled;
    }
    if (settingsKeys.contains("sdoData")) {
        m_sdoData = settings.m_sdoData;
    }
    if (settingsKeys.contains("sdoNow")) {
        m_sdoNow = settings.m_sdoNow;
    }
    if (settingsKeys.contains("sdoDateTime")) {
        m_sdoDateTime = settings.m_sdoDateTime;
    }
    if (settingsKeys.contains("map")) {
        m_map = settings.m_map;
    }
    if (settingsKeys.contains("sdoSplitterSizes")) {
        m_sdoSplitterSizes = settings.m_sdoSplitterSizes;
    }
    if (settingsKeys.contains("chartSplitterSizes")) {
        m_chartSplitterSizes = settings.m_chartSplitterSizes;
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

QString SIDSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("channelSettings"))
    {
        QStringList s;
        for (auto cs : m_channelSettings) {
            s.append(cs.m_id);
        }
        ostr << " m_channelSettings: " << s.join(",").toStdString();
    }
    if (settingsKeys.contains("period") || force) {
        ostr << " m_period: " << m_period;
    }
    if (settingsKeys.contains("autosave") || force) {
        ostr << " m_autosave: " << m_autosave;
    }
    if (settingsKeys.contains("autoload") || force) {
        ostr << " m_autoload: " << m_autoload;
    }
    if (settingsKeys.contains("filename") || force) {
        ostr << " m_filename: " << m_filename.toStdString();
    }
    if (settingsKeys.contains("samples") || force) {
        ostr << " m_samples: " << m_samples;
    }
    if (settingsKeys.contains("autoscaleX") || force) {
        ostr << " m_autoscaleX: " << m_autoscaleX;
    }
    if (settingsKeys.contains("autoscaleY") || force) {
        ostr << " m_autoscaleY: " << m_autoscaleY;
    }
    if (settingsKeys.contains("separateCharts") || force) {
        ostr << " m_separateCharts: " << m_separateCharts;
    }
    if (settingsKeys.contains("displayLegend") || force) {
        ostr << " m_displayLegend: " << m_displayLegend;
    }
    if (settingsKeys.contains("legendAlignment") || force) {
        ostr << " m_legendAlignment: " << m_legendAlignment;
    }
    if (settingsKeys.contains("displayAxisTitles") || force) {
        ostr << " m_displayAxisTitles: " << m_displayAxisTitles;
    }
    if (settingsKeys.contains("displayAxisLabels") || force) {
        ostr << " m_displaySecondaryAxis: " << m_displaySecondaryAxis;
    }
    if (settingsKeys.contains("plotXRayLongPrimary") || force) {
        ostr << " m_plotXRayLongPrimary: " << m_plotXRayLongPrimary;
    }
    if (settingsKeys.contains("plotXRayLongSecondary") || force) {
        ostr << " m_plotXRayLongSecondary: " << m_plotXRayLongSecondary;
    }
    if (settingsKeys.contains("plotXRayShortPrimary") || force) {
        ostr << " m_plotXRayShortPrimary: " << m_plotXRayShortPrimary;
    }
    if (settingsKeys.contains("plotXRayShortSecondary") || force) {
        ostr << " m_plotXRayShortSecondary: " << m_plotXRayShortSecondary;
    }
    if (settingsKeys.contains("plotGRB") || force) {
        ostr << " m_plotGRB: " << m_plotGRB;
    }
    if (settingsKeys.contains("plotSTIX") || force) {
        ostr << " m_plotSTIX: " << m_plotSTIX;
    }
    if (settingsKeys.contains("plotProton") || force) {
        ostr << " m_plotProton: " << m_plotProton;
    }
    if (settingsKeys.contains("startDateTime") || force) {
        ostr << " m_startDateTime: " << m_startDateTime.toString().toStdString();
    }
    if (settingsKeys.contains("endDateTime") || force) {
        ostr << " m_endDateTime: " << m_endDateTime.toString().toStdString();
    }
    if (settingsKeys.contains("y1Min") || force) {
        ostr << " m_y1Min: " << m_y1Min;
    }
    if (settingsKeys.contains("y1Max") || force) {
        ostr << " m_y1Max: " << m_y1Max;
    }
    if (settingsKeys.contains("sdoEnabled") || force) {
        ostr << " m_sdoEnabled: " << m_sdoEnabled;
    }
    if (settingsKeys.contains("sdoVideoEnabled") || force) {
        ostr << " m_sdoVideoEnabled: " << m_sdoVideoEnabled;
    }
    if (settingsKeys.contains("sdoData") || force) {
        ostr << " m_sdoData: " << m_sdoData.toStdString();
    }
    if (settingsKeys.contains("sdoNow") || force) {
        ostr << " m_sdoNow: " << m_sdoNow;
    }
    if (settingsKeys.contains("sdoDateTime") || force) {
        ostr << " m_sdoDateTime: " << m_sdoDateTime.toString().toStdString();
    }
    if (settingsKeys.contains("map") || force) {
        ostr << " m_map: " << m_map.toStdString();
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

SIDSettings::ChannelSettings *SIDSettings::getChannelSettings(const QString& id)
{
    for (int i = 0; i < m_channelSettings.size(); i++)
    {
        if (m_channelSettings[i].m_id == id) {
            return &m_channelSettings[i];
        }
    }
    return nullptr;
}

bool SIDSettings::createChannelSettings()
{
    bool settingsChanged = false;
    QStringList ids;
    QStringList titles;

    getChannels(ids, titles);

    // Create settings for channels we don't currently have settings for
    for (int i = 0; i < ids.size(); i++)
    {
        SIDSettings::ChannelSettings *channelSettings = getChannelSettings(ids[i]);
        if (!channelSettings)
        {
            SIDSettings::ChannelSettings newSettings;
            newSettings.m_id = ids[i];
            newSettings.m_enabled = true;
            newSettings.m_label = titles[i];
            newSettings.m_color = SIDSettings::m_defaultColors[i % SIDSettings::m_defaultColors.size()];
            m_channelSettings.append(newSettings);
            settingsChanged = true;
        }
    }

    return settingsChanged;
}

// Get channels that have channelPowerDB value in their report
void SIDSettings::getChannels(QStringList& ids, QStringList& titles)
{
    MainCore *mainCore = MainCore::instance();

    std::vector<DeviceSet*> deviceSets = mainCore->getDeviceSets();
    for (unsigned int deviceSetIndex = 0; deviceSetIndex < deviceSets.size(); deviceSetIndex++)
    {
        DeviceSet *deviceSet = deviceSets[deviceSetIndex];

        for (int channelIndex = 0; channelIndex < deviceSet->getNumberOfChannels(); channelIndex++)
        {
            QString title;
            ChannelWebAPIUtils::getChannelSetting(deviceSetIndex, channelIndex, "title", title);

            double power;
            if (ChannelWebAPIUtils::getChannelReportValue(deviceSetIndex, channelIndex, "channelPowerDB", power))
            {
                ChannelAPI *channel = mainCore->getChannel(deviceSetIndex, channelIndex);

                QString id = mainCore->getChannelId(channel);

                ids.append(id);
                titles.append(title);
            }
        }
    }
}

QByteArray SIDSettings::ChannelSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_id);
    s.writeBool(2, m_enabled);
    s.writeString(3, m_label);
    s.writeU32(4, m_color.rgb());

    return s.final();
}

bool SIDSettings::ChannelSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray blob;
        quint32 utmp;

        d.readString(1, &m_id);
        d.readBool(2, &m_enabled, true);
        d.readString(3, &m_label);
        d.readU32(4, &utmp);
        m_color = utmp;

        return true;
    }
    else
    {
        return false;
    }
}

QDataStream& operator<<(QDataStream& out, const SIDSettings::ChannelSettings& settings)
{
    out << settings.serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, SIDSettings::ChannelSettings& settings)
{
    QByteArray data;
    in >> data;
    settings.deserialize(data);
    return in;
}
