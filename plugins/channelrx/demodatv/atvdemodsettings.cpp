///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2020 Vort <vvort@yandex.ru>                                     //
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
#include "atvdemodsettings.h"

ATVDemodSettings::ATVDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ATVDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_bfoFrequency = 0.0f;
    m_atvModulation = ATV_FM1;
    m_fmDeviation = 0.5f;
    m_amScalingFactor = 100;
    m_amOffsetFactor = 0;
    m_fftFiltering = false;
    m_fftOppBandwidth = 0;
    m_fftBandwidth = 6000;
    m_nbLines = 625;
    m_fps = 25;
    m_atvStd = ATVStdPAL625;
    m_hSync = false;
    m_vSync = false;
    m_invertVideo = false;
    m_halfFrames = false; // m_fltRatioOfRowsToDisplay = 1.0
    m_levelSynchroTop = 0.15f;
    m_levelBlack = 0.3f;
    m_colourSystem = ColourMono;
    m_colourSaturation = 100;
    m_colourHue = 0;
    m_palDelayLine = true;
    m_noiseReduction = 0;
    m_ghostCancellation = false;
    m_rgbColor = QColor(255, 255, 255).rgb();
    m_title = "ATV Demodulator";
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray ATVDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS64(1, m_inputFrequencyOffset);
    s.writeU32(2, m_rgbColor);
    s.writeS32(3, roundf(m_levelSynchroTop*1000.0)); // mV
    s.writeS32(4, roundf(m_levelBlack*1000.0));      // mV
    s.writeS32(7, m_atvModulation);
    s.writeS32(8, m_fps);
    s.writeBool(9, m_hSync);
    s.writeBool(10,m_vSync);
    s.writeBool(11, m_halfFrames);
    s.writeU32(12, m_fftBandwidth);
    s.writeU32(13, m_fftOppBandwidth);
    s.writeS32(14, m_bfoFrequency);
    s.writeBool(15, m_invertVideo);
    s.writeS32(16, m_nbLines);
    s.writeS32(17, roundf(m_fmDeviation * 500.0));
    s.writeS32(18, m_atvStd);

    if (m_channelMarker) {
        s.writeBlob(19, m_channelMarker->serialize());
    }

    s.writeString(20, m_title);
    s.writeS32(21, m_streamIndex);
    s.writeS32(22, m_amScalingFactor);
    s.writeS32(23, m_amOffsetFactor);
    s.writeBool(24, m_fftFiltering);

    if (m_rollupState) {
        s.writeBlob(25, m_rollupState->serialize());
    }

    s.writeBool(26, m_useReverseAPI);
    s.writeString(27, m_reverseAPIAddress);
    s.writeU32(28, m_reverseAPIPort);
    s.writeU32(29, m_reverseAPIDeviceIndex);
    s.writeU32(30, m_reverseAPIChannelIndex);
    s.writeS32(31, m_workspaceIndex);
    s.writeBlob(32, m_geometryBytes);
    s.writeBool(33, m_hidden);
    s.writeS32(34, (int) m_colourSystem);
    s.writeS32(35, m_colourSaturation);
    s.writeS32(36, m_colourHue);
    s.writeBool(37, m_palDelayLine);
    s.writeS32(38, m_noiseReduction);
    s.writeBool(39, m_ghostCancellation);

    return s.final();
}

bool ATVDemodSettings::deserialize(const QByteArray& arrData)
{
    SimpleDeserializer d(arrData);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        int tmp;
        uint32_t utmp;

        d.readS64(1, &m_inputFrequencyOffset, 0);
        // TODO: rgb color
        d.readS32(3, &tmp, 100);
        m_levelSynchroTop = tmp / 1000.0f;
        d.readS32(4, &tmp, 310);
        m_levelBlack = tmp / 1000.0f;
        d.readS32(7, &tmp, 0);
        m_atvModulation = static_cast<ATVModulation>(tmp);
        d.readS32(8, &tmp, 25);
        int fpsIndex = getFpsIndex(tmp);
        m_fps = getFps(fpsIndex);
        d.readBool(9, &m_hSync, false);
        d.readBool(10, &m_vSync, false);
        d.readBool(11, &m_halfFrames, false);
        d.readU32(12, &m_fftBandwidth, 6000);
        d.readU32(13, &m_fftOppBandwidth, 0);
        d.readS32(14, &m_bfoFrequency, 0);
        d.readBool(15, &m_invertVideo, false);
        d.readS32(16, &tmp, 625);
        int nbLinesIndex = getNumberOfLinesIndex(tmp);
        m_nbLines = getNumberOfLines(nbLinesIndex);
        d.readS32(17, &tmp, 250);
        m_fmDeviation = tmp / 500.0f;
        d.readS32(18, &tmp, 1);

        if (m_channelMarker)
        {
            d.readBlob(19, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        m_atvStd = static_cast<ATVStd>(tmp);
        d.readS32(21, &m_streamIndex, 0);
        d.readS32(22, &m_amScalingFactor, 100);
        d.readS32(23, &m_amOffsetFactor, 0);
        d.readBool(24, &m_fftFiltering, false);

        if (m_rollupState)
        {
            d.readBlob(25, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readBool(26, &m_useReverseAPI, false);
        d.readString(27, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(28, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(29, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(30, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(31, &m_workspaceIndex, 0);
        d.readBlob(32, &m_geometryBytes);
        d.readBool(33, &m_hidden, false);
        d.readS32(34, &tmp, (int) ColourMono);
        m_colourSystem = (tmp >= (int) ColourMono) && (tmp <= (int) ColourPALN) ? (ColourSystem) tmp : ColourMono;
        d.readS32(35, &m_colourSaturation, 100);
        d.readS32(36, &m_colourHue, 0);
        d.readBool(37, &m_palDelayLine, true);
        d.readS32(38, &m_noiseReduction, 0);
        d.readBool(39, &m_ghostCancellation, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int ATVDemodSettings::getFps(int fpsIndex)
{
    switch(fpsIndex)
    {
    case 0:
        return 30;
        break;
    case 2:
        return 20;
        break;
    case 3:
        return 16;
        break;
    case 4:
        return 12;
        break;
    case 5:
        return 10;
        break;
    case 6:
        return 8;
        break;
    case 7:
        return 5;
        break;
    case 8:
        return 2;
        break;
    case 9:
        return 1;
        break;
    case 1:
    default:
        return 25;
        break;
    }
}

int ATVDemodSettings::getFpsIndex(int fps)
{
    if (fps <= 1) {
        return 9;
    } else if (fps <= 2) {
        return 8;
    } else if (fps <= 5) {
        return 7;
    } else if (fps <= 8) {
        return 6;
    } else if (fps <= 10) {
        return 5;
    } else if (fps <= 12) {
        return 4;
    } else if (fps <= 16) {
        return 3;
    } else if (fps <= 20) {
        return 2;
    } else if (fps <= 25) {
        return 1;
    } else {
        return 0;
    }
}

int ATVDemodSettings::getNumberOfLines(int nbLinesIndex)
{
    switch(nbLinesIndex)
    {
    case 0:
        return 819;
        break;
    case 1:
        return 640;
        break;
    case 3:
        return 525;
        break;
    case 4:
        return 480;
        break;
    case 5:
        return 405;
        break;
    case 6:
        return 360;
        break;
    case 7:
        return 343;
        break;
    case 8:
        return 240;
        break;
    case 9:
        return 180;
        break;
    case 10:
        return 120;
        break;
    case 11:
        return 90;
        break;
    case 12:
        return 60;
        break;
    case 13:
        return 32;
        break;
    case 2:
    default:
        return 625;
        break;
    }
}

int ATVDemodSettings::getNumberOfLinesIndex(int nbLines)
{
    if (nbLines <= 32) {
        return 13;
    } else if (nbLines <= 60) {
        return 12;
    } else if (nbLines <= 90) {
        return 11;
    } else if (nbLines <= 120) {
        return 10;
    } else if (nbLines <= 180) {
        return 9;
    } else if (nbLines <= 240) {
        return 8;
    } else if (nbLines <= 343) {
        return 7;
    } else if (nbLines <= 360) {
        return 6;
    } else if (nbLines <= 405) {
        return 5;
    } else if (nbLines <= 480) {
        return 4;
    } else if (nbLines <= 525) {
        return 3;
    } else if (nbLines <= 625) {
        return 2;
    } else if (nbLines <= 640) {
        return 2;
    } else {
        return 0;
    }
}

void ATVDemodSettings::applySettings(const QStringList& settingsKeys, const ATVDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("bfoFrequency")) {
        m_bfoFrequency = settings.m_bfoFrequency;
    }
    if (settingsKeys.contains("atvModulation")) {
        m_atvModulation = settings.m_atvModulation;
    }
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (settingsKeys.contains("amScalingFactor")) {
        m_amScalingFactor = settings.m_amScalingFactor;
    }
    if (settingsKeys.contains("amOffsetFactor")) {
        m_amOffsetFactor = settings.m_amOffsetFactor;
    }
    if (settingsKeys.contains("fftFiltering")) {
        m_fftFiltering = settings.m_fftFiltering;
    }
    if (settingsKeys.contains("fftOppBandwidth")) {
        m_fftOppBandwidth = settings.m_fftOppBandwidth;
    }
    if (settingsKeys.contains("fftBandwidth")) {
        m_fftBandwidth = settings.m_fftBandwidth;
    }
    if (settingsKeys.contains("nbLines")) {
        m_nbLines = settings.m_nbLines;
    }
    if (settingsKeys.contains("fps")) {
        m_fps = settings.m_fps;
    }
    if (settingsKeys.contains("atvStd")) {
        m_atvStd = settings.m_atvStd;
    }
    if (settingsKeys.contains("hSync")) {
        m_hSync = settings.m_hSync;
    }
    if (settingsKeys.contains("vSync")) {
        m_vSync = settings.m_vSync;
    }
    if (settingsKeys.contains("invertVideo")) {
        m_invertVideo = settings.m_invertVideo;
    }
    if (settingsKeys.contains("halfFrames")) {
        m_halfFrames = settings.m_halfFrames;
    }
    if (settingsKeys.contains("levelSynchroTop")) {
        m_levelSynchroTop = settings.m_levelSynchroTop;
    }
    if (settingsKeys.contains("levelBlack")) {
        m_levelBlack = settings.m_levelBlack;
    }
    if (settingsKeys.contains("colourSystem")) {
        m_colourSystem = settings.m_colourSystem;
    }
    if (settingsKeys.contains("colourSaturation")) {
        m_colourSaturation = settings.m_colourSaturation;
    }
    if (settingsKeys.contains("colourHue")) {
        m_colourHue = settings.m_colourHue;
    }
    if (settingsKeys.contains("palDelayLine")) {
        m_palDelayLine = settings.m_palDelayLine;
    }
    if (settingsKeys.contains("noiseReduction")) {
        m_noiseReduction = settings.m_noiseReduction;
    }
    if (settingsKeys.contains("ghostCancellation")) {
        m_ghostCancellation = settings.m_ghostCancellation;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
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
}

QString ATVDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("bfoFrequency") || force) {
        ostr << " m_bfoFrequency: " << m_bfoFrequency;
    }
    if (settingsKeys.contains("atvModulation") || force) {
        ostr << " m_atvModulation: " << m_atvModulation;
    }
    if (settingsKeys.contains("fmDeviation") || force) {
        ostr << " m_fmDeviation: " << m_fmDeviation;
    }
    if (settingsKeys.contains("amScalingFactor") || force) {
        ostr << " m_amScalingFactor: " << m_amScalingFactor;
    }
    if (settingsKeys.contains("amOffsetFactor") || force) {
        ostr << " m_amOffsetFactor: " << m_amOffsetFactor;
    }
    if (settingsKeys.contains("fftFiltering") || force) {
        ostr << " m_fftFiltering: " << m_fftFiltering;
    }
    if (settingsKeys.contains("fftOppBandwidth") || force) {
        ostr << " m_fftOppBandwidth: " << m_fftOppBandwidth;
    }
    if (settingsKeys.contains("fftBandwidth") || force) {
        ostr << " m_fftBandwidth: " << m_fftBandwidth;
    }
    if (settingsKeys.contains("nbLines") || force) {
        ostr << " m_nbLines: " << m_nbLines;
    }
    if (settingsKeys.contains("fps") || force) {
        ostr << " m_fps: " << m_fps;
    }
    if (settingsKeys.contains("atvStd") || force) {
        ostr << " m_atvStd: " << m_atvStd;
    }
    if (settingsKeys.contains("hSync") || force) {
        ostr << " m_hSync: " << m_hSync;
    }
    if (settingsKeys.contains("vSync") || force) {
        ostr << " m_vSync: " << m_vSync;
    }
    if (settingsKeys.contains("invertVideo") || force) {
        ostr << " m_invertVideo: " << m_invertVideo;
    }
    if (settingsKeys.contains("halfFrames") || force) {
        ostr << " m_halfFrames: " << m_halfFrames;
    }
    if (settingsKeys.contains("levelSynchroTop") || force) {
        ostr << " m_levelSynchroTop: " << m_levelSynchroTop;
    }
    if (settingsKeys.contains("levelBlack") || force) {
        ostr << " m_levelBlack: " << m_levelBlack;
    }
    if (settingsKeys.contains("colourSystem") || force) {
        ostr << " m_colourSystem: " << m_colourSystem;
    }
    if (settingsKeys.contains("colourSaturation") || force) {
        ostr << " m_colourSaturation: " << m_colourSaturation;
    }
    if (settingsKeys.contains("colourHue") || force) {
        ostr << " m_colourHue: " << m_colourHue;
    }
    if (settingsKeys.contains("palDelayLine") || force) {
        ostr << " m_palDelayLine: " << m_palDelayLine;
    }
    if (settingsKeys.contains("noiseReduction") || force) {
        ostr << " m_noiseReduction: " << m_noiseReduction;
    }
    if (settingsKeys.contains("ghostCancellation") || force) {
        ostr << " m_ghostCancellation: " << m_ghostCancellation;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("udpAddress") || force) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort") || force) {
        ostr << " m_udpPort: " << m_udpPort;
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

float ATVDemodSettings::getNominalLineTime(int nbLines, int fps)
{
    return 1.0f / ((float) nbLines * (float) fps);
}

int ATVDemodSettings::getRFSliderDivisor(unsigned int sampleRate)
{
    int scaleFactor = (int) std::log10(sampleRate/2);
    return std::pow(10.0, scaleFactor-1);
}

float ATVDemodSettings::getRFBandwidthDivisor(ATVModulation modulation)
{
    switch(modulation)
    {
    case ATV_USB:
    case ATV_LSB:
        return 1.05f;
        break;
    case ATV_FM1:
    case ATV_FM2:
    case ATV_AM:
    default:
        return 2.2f;
    }
}

void ATVDemodSettings::getBaseValues(int sampleRate, int linesPerSecond, uint32_t& nbPointsPerLine)
{
    nbPointsPerLine = sampleRate / linesPerSecond;
    nbPointsPerLine = nbPointsPerLine == 0 ? 1 : nbPointsPerLine;
}

bool ATVDemodSettings::usesVSBFrontEnd() const
{
    if (isColour()) {
        return (m_atvModulation == ATV_AM) || (m_atvModulation == ATV_USB) || (m_atvModulation == ATV_LSB);
    } else {
        // The FFT filter, when enabled, replaces it (for double sideband AM, for example)
        return (m_atvModulation == ATV_AM) && !m_fftFiltering;
    }
}

float ATVDemodSettings::getLineFrequency() const
{
    // 525 line colour systems run at 59.94 fields/s, which the integer fps setting cannot express
    if (isColour() && (m_nbLines == 525)) {
        return 4500000.0f / 286.0f;
    } else {
        return (float) (m_nbLines * m_fps);
    }
}

int ATVDemodSettings::getVideoSampleRate(int channelSampleRate) const
{
    if (isColour() && usesVSBFrontEnd()) {
        return channelSampleRate * getColourUpsampling(channelSampleRate, m_colourSystem);
    } else {
        return channelSampleRate;
    }
}

float ATVDemodSettings::getSubcarrierFrequency(ColourSystem colourSystem)
{
    switch (colourSystem)
    {
    case ColourNTSC:
        return 315.0e6f / 88.0f;   // 3.579545 MHz
    case ColourPALM:
        return 3575611.49f;
    case ColourPALN:
        return 3582056.25f;
    case ColourNTSC443:
    case ColourPAL:
        return 4433618.75f;
    case ColourSECAM:
        return 4406250.0f;         // The higher of the two
    case ColourMono:
    default:
        return 0.0f;
    }
}

float ATVDemodSettings::getWhiteCarrier(ColourSystem colourSystem)
{
    // 12.5% for 525 line systems, 10% for 625 line systems
    if ((colourSystem == ColourNTSC) || (colourSystem == ColourPALM)) {
        return 0.125f;
    } else {
        return 0.1f;
    }
}

float ATVDemodSettings::getWhiteCarrier() const
{
    if (isColour()) {
        return getWhiteCarrier(m_colourSystem);
    } else {
        return m_nbLines == 525 ? 0.125f : 0.1f;
    }
}

float ATVDemodSettings::getVideoBandwidth() const
{
    if (isColour()) {
        return getSubcarrierFrequency(m_colourSystem) + 0.6e6f; // Top of the upper chroma sideband
    } else {
        return m_nbLines == 525 ? 4.2e6f : 5.0e6f; // Below the sound carrier
    }
}

float ATVDemodSettings::getColourBandwidth(ColourSystem colourSystem)
{
    // 0.3 MHz below the vision carrier (Nyquist slope) to 0.6 MHz above the subcarrier (upper chroma sideband)
    return getSubcarrierFrequency(colourSystem) + 0.9e6f;
}

float ATVDemodSettings::getColourBandCentreOffset(ColourSystem colourSystem)
{
    return (getSubcarrierFrequency(colourSystem) + 0.3e6f) / 2.0f;
}

int ATVDemodSettings::getColourUpsampling(int sampleRate, ColourSystem colourSystem)
{
    // The envelope of the VSB signal is real and extends to the top of the chroma,
    // so it needs a sample rate a little over twice that
    float minRate = 2.2f * (getSubcarrierFrequency(colourSystem) + 0.6e6f);

    if (sampleRate >= minRate) {
        return 1;
    } else if (2 * sampleRate >= minRate) {
        return 2;
    } else {
        return 4;
    }
}
