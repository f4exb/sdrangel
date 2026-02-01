///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "atvmodsettings.h"

ATVModSettings::ATVModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ATVModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 1000000;
    m_rfOppBandwidth = 0;
    m_atvStd = ATVStdPAL625;
    m_nbLines = 625;
    m_fps = 25;
    m_atvModInput = ATVModInputHBars;
    m_uniformLevel = 0.5f;
    m_atvModulation = ATVModulationAM;
    m_videoPlayLoop = false;
    m_videoPlay = false;
    m_cameraPlay = false;
    m_channelMute = false;
    m_invertedVideo = false;
    m_rfScalingFactor = 0.891235351562f * SDR_TX_SCALEF; // -1dB
    m_fmExcursion = 0.5f;         // half bandwidth
    m_forceDecimator = false;
    m_showOverlayText = false;
    m_overlayText = "ATV";
    m_rgbColor = QColor(255, 255, 255).rgb();
    m_title = "ATV Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray ATVModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeS32(3, roundf(m_uniformLevel * 100.0)); // percent
    s.writeS32(4, (int) m_atvStd);
    s.writeS32(5, (int) m_atvModInput);
    s.writeU32(6, m_rgbColor);
    s.writeReal(7, m_rfOppBandwidth);
    s.writeS32(8, (int) m_atvModulation);
    s.writeBool(9, m_invertedVideo);
    s.writeS32(10, m_nbLines);
    s.writeS32(11, m_fps);
    s.writeS32(12, roundf(m_rfScalingFactor / 327.68f));
    s.writeS32(13, roundf(m_fmExcursion * 1000.0)); // pro mill
    s.writeString(14, m_overlayText);

    if (m_channelMarker) {
        s.writeBlob(15, m_channelMarker->serialize());
    }

    s.writeString(16, m_title);
    s.writeBool(17, m_useReverseAPI);
    s.writeString(18, m_reverseAPIAddress);
    s.writeU32(19, m_reverseAPIPort);
    s.writeU32(20, m_reverseAPIDeviceIndex);
    s.writeU32(21, m_reverseAPIChannelIndex);
    s.writeString(22, m_imageFileName);
    s.writeString(23, m_videoFileName);
    s.writeS32(24, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(25, m_rollupState->serialize());
    }

    s.writeS32(26, m_workspaceIndex);
    s.writeBlob(27, m_geometryBytes);
    s.writeBool(28, m_hidden);

    return s.final();
}

bool ATVModSettings::deserialize(const QByteArray& data)
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
        d.readS32(3, &tmp, 100);
        m_uniformLevel = tmp / 100.0; // percent
        d.readS32(4, &tmp, 0);
        m_atvStd = (ATVStd) tmp;
        d.readS32(5, &tmp, 0);
        m_atvModInput = (ATVModInput) tmp;
        d.readU32(6, &m_rgbColor, 0);
        d.readReal(7, &m_rfOppBandwidth, 0);
        d.readS32(8, &tmp, 0);
        m_atvModulation = (ATVModulation) tmp;
        d.readBool(9, &m_invertedVideo, false);
        d.readS32(10, &m_nbLines, 625);
        d.readS32(11, &m_fps, 25);
        d.readS32(12, &tmp, 80);
        m_rfScalingFactor = tmp * 327.68f;
        d.readS32(13, &tmp, 250);
        m_fmExcursion = tmp / 1000.0; // pro mill
        d.readString(14, &m_overlayText, "ATV");

        if (m_channelMarker)
        {
            d.readBlob(15, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(16, &m_title, "ATV Modulator");
        d.readBool(17, &m_useReverseAPI, false);
        d.readString(18, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(19, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(20, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(21, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(22, &m_imageFileName);
        d.readString(23, &m_videoFileName);
        d.readS32(24, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(25, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(26, &m_workspaceIndex, 0);
        d.readBlob(27, &m_geometryBytes);
        d.readBool(28, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void ATVModSettings::applySettings(const QStringList& settingsKeys, const ATVModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("rfOppBandwidth")) {
        m_rfOppBandwidth = settings.m_rfOppBandwidth;
    }
    if (settingsKeys.contains("atvStd")) {
        m_atvStd = settings.m_atvStd;
    }
    if (settingsKeys.contains("nbLines")) {
        m_nbLines = settings.m_nbLines;
    }
    if (settingsKeys.contains("fps")) {
        m_fps = settings.m_fps;
    }
    if (settingsKeys.contains("atvModInput")) {
        m_atvModInput = settings.m_atvModInput;
    }
    if (settingsKeys.contains("uniformLevel")) {
        m_uniformLevel = settings.m_uniformLevel;
    }
    if (settingsKeys.contains("atvModulation")) {
        m_atvModulation = settings.m_atvModulation;
    }
    if (settingsKeys.contains("videoPlayLoop")) {
        m_videoPlayLoop = settings.m_videoPlayLoop;
    }
    if (settingsKeys.contains("videoPlay")) {
        m_videoPlay = settings.m_videoPlay;
    }
    if (settingsKeys.contains("cameraPlay")) {
        m_cameraPlay = settings.m_cameraPlay;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("invertedVideo")) {
        m_invertedVideo = settings.m_invertedVideo;
    }
    if (settingsKeys.contains("rfScalingFactor")) {
        m_rfScalingFactor = settings.m_rfScalingFactor;
    }
    if (settingsKeys.contains("fmExcursion")) {
        m_fmExcursion = settings.m_fmExcursion;
    }
    if (settingsKeys.contains("forceDecimator")) {
        m_forceDecimator = settings.m_forceDecimator;
    }
    if (settingsKeys.contains("showOverlayText")) {
        m_showOverlayText = settings.m_showOverlayText;
    }
    if (settingsKeys.contains("overlayText")) {
        m_overlayText = settings.m_overlayText;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("imageFileName")) {
        m_imageFileName = settings.m_imageFileName;
    }
    if (settingsKeys.contains("videoFileName")) {
        m_videoFileName = settings.m_videoFileName;
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

QString ATVModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("rfOppBandwidth") || force) {
        ostr << " m_rfOppBandwidth: " << m_rfOppBandwidth;
    }
    if (settingsKeys.contains("atvStd") || force) {
        ostr << " m_atvStd: " << m_atvStd;
    }
    if (settingsKeys.contains("nbLines") || force) {
        ostr << " m_nbLines: " << m_nbLines;
    }
    if (settingsKeys.contains("fps") || force) {
        ostr << " m_fps: " << m_fps;
    }
    if (settingsKeys.contains("atvModInput") || force) {
        ostr << " m_atvModInput: " << m_atvModInput;
    }
    if (settingsKeys.contains("uniformLevel") || force) {
        ostr << " m_uniformLevel: " << m_uniformLevel;
    }
    if (settingsKeys.contains("atvModulation") || force) {
        ostr << " m_atvModulation: " << m_atvModulation;
    }
    if (settingsKeys.contains("videoPlayLoop") || force) {
        ostr << " m_videoPlayLoop: " << m_videoPlayLoop;
    }
    if (settingsKeys.contains("videoPlay") || force) {
        ostr << " m_videoPlay: " << m_videoPlay;
    }
    if (settingsKeys.contains("cameraPlay") || force) {
        ostr << " m_cameraPlay: " << m_cameraPlay;
    }
    if (settingsKeys.contains("channelMute") || force) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (settingsKeys.contains("invertedVideo") || force) {
        ostr << " m_invertedVideo: " << m_invertedVideo;
    }
    if (settingsKeys.contains("rfScalingFactor") || force) {
        ostr << " m_rfScalingFactor: " << m_rfScalingFactor;
    }
    if (settingsKeys.contains("fmExcursion") || force) {
        ostr << " m_fmExcursion: " << m_fmExcursion;
    }
    if (settingsKeys.contains("forceDecimator") || force) {
        ostr << " m_forceDecimator: " << m_forceDecimator;
    }
    if (settingsKeys.contains("showOverlayText") || force) {
        ostr << " m_showOverlayText: " << m_showOverlayText;
    }
    if (settingsKeys.contains("overlayText") || force) {
        ostr << " m_overlayText: " << m_overlayText.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("imageFileName") || force) {
        ostr << " m_imageFileName: " << m_imageFileName.toStdString();
    }
    if (settingsKeys.contains("videoFileName") || force) {
        ostr << " m_videoFileName: " << m_videoFileName.toStdString();
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
