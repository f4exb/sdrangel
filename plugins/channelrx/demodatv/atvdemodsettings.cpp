///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "atvdemodsettings.h"

ATVDemodSettings::ATVDemodSettings() :
    m_channelMarker(0)
{
    resetToDefaults();
}

void ATVDemodSettings::resetToDefaults()
{
    m_lineTimeFactor = 0;
    m_topTimeFactor = 0;
    m_fpsIndex = 1; // 25 FPS
    m_halfImage = false; // m_fltRatioOfRowsToDisplay = 1.0
    m_RFBandwidthFactor = 10;
    m_OppBandwidthFactor = 10;
    m_nbLinesIndex = 1; // 625 lines

    m_intFrequencyOffset = 0;
    m_enmModulation = ATV_FM1;
    m_fltRFBandwidth = 0;
    m_fltRFOppBandwidth = 0;
    m_blnFFTFiltering = false;
    m_blndecimatorEnable = false;
    m_fltBFOFrequency = 0.0f;
    m_fmDeviation = 0.5f;

    m_intSampleRate = 0;
    m_enmATVStandard = ATVStdPAL625;
    m_intNumberOfLines = 625;
    m_fltLineDuration = 0.0f;
    m_fltTopDuration = 0.0f;
    m_fltFramePerS = 25.0f;
    m_fltRatioOfRowsToDisplay = 1.0f;
    m_fltVoltLevelSynchroTop = 0.0f;
    m_fltVoltLevelSynchroBlack = 1.0f;
    m_blnHSync = false;
    m_blnVSync = false;
    m_blnInvertVideo = false;
    m_intVideoTabIndex = 0;

    m_intTVSampleRate = 0;
    m_intNumberSamplePerLine = 0;

    m_rgbColor = QColor(255, 255, 255).rgb();
    m_title = "ATV Demodulator";
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
}

QByteArray ATVDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_intFrequencyOffset);
    s.writeU32(2, m_rgbColor);
    s.writeS32(3, roundf(m_fltVoltLevelSynchroTop*1000.0));   // mV
    s.writeS32(4, roundf(m_fltVoltLevelSynchroBlack*1000.0)); // mV
    s.writeS32(5, m_lineTimeFactor);      // added UI
    s.writeS32(6, m_topTimeFactor);       // added UI
    s.writeS32(7, m_enmModulation);
    s.writeS32(8, m_fpsIndex);            // added UI
    s.writeBool(9, m_blnHSync);
    s.writeBool(10,m_blnVSync);
    s.writeBool(11, m_halfImage);         // added UI
    s.writeS32(12, m_RFBandwidthFactor);  // added UI
    s.writeS32(13, m_OppBandwidthFactor); // added UI
    s.writeS32(14, roundf(m_fltBFOFrequency));
    s.writeBool(15, m_blnInvertVideo);
    s.writeS32(16, m_nbLinesIndex);       // added UI
    s.writeS32(17, roundf(m_fmDeviation * 500.0));
    s.writeS32(18, m_enmATVStandard);

    if (m_channelMarker) {
        s.writeBlob(19, m_channelMarker->serialize());
    }

    s.writeString(20, m_title);

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

        d.readS32(1, &m_intFrequencyOffset, 0);
        // TODO: rgb color
        d.readS32(3, &tmp, 100);
        m_fltVoltLevelSynchroTop = tmp / 1000.0f;
        d.readS32(4, &tmp, 310);
        m_fltVoltLevelSynchroBlack = tmp / 1000.0f;
        d.readS32(5, &m_lineTimeFactor, 0); // added UI
        d.readS32(6, &m_topTimeFactor, 0); // added UI
        d.readS32(7, &tmp, 0);
        m_enmModulation = static_cast<ATVModulation>(tmp);
        d.readS32(8, &m_fpsIndex, 1); // added UI
        d.readBool(9, &m_blnHSync, false);
        d.readBool(10, &m_blnVSync, false);
        d.readBool(11, &m_halfImage, false); // added UI
        d.readS32(12, &m_RFBandwidthFactor, 10); // added UI
        d.readS32(13, &m_OppBandwidthFactor, 10); // added UI
        d.readS32(14, &tmp, 10);
        m_fltBFOFrequency = static_cast<float>(tmp);
        d.readBool(15, &m_blnInvertVideo, false);
        d.readS32(16, &m_nbLinesIndex, 1); // added UI
        d.readS32(17, &tmp, 250);
        m_fmDeviation = tmp / 500.0f;
        d.readS32(18, &tmp, 1);
        m_enmATVStandard = static_cast<ATVStd>(tmp);

        // TODO: calculate values from UI values

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int ATVDemodSettings::getEffectiveSampleRate()
{
    return m_blndecimatorEnable ? m_intTVSampleRate : m_intSampleRate;
}


float ATVDemodSettings::getFps(int fpsIndex)
{
    switch(fpsIndex)
    {
    case 0:
        return 30.0f;
        break;
    case 2:
        return 20.0f;
        break;
    case 3:
        return 16.0f;
        break;
    case 4:
        return 12.0f;
        break;
    case 5:
        return 10.0f;
        break;
    case 6:
        return 8.0f;
        break;
    case 7:
        return 5.0f;
        break;
    case 8:
        return 2.0f;
        break;
    case 9:
        return 1.0f;
        break;
    case 1:
    default:
        return 25.0f;
        break;
    }
}

int ATVDemodSettings::getFpsIndex(float fps)
{
    int fpsi = roundf(fps);

    if (fpsi <= 1) {
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
        return 640;
        break;
    case 2:
        return 525;
        break;
    case 3:
        return 480;
        break;
    case 4:
        return 405;
        break;
    case 5:
        return 360;
        break;
    case 6:
        return 343;
        break;
    case 7:
        return 240;
        break;
    case 8:
        return 180;
        break;
    case 9:
        return 120;
        break;
    case 10:
        return 90;
        break;
    case 11:
        return 60;
        break;
    case 12:
        return 32;
        break;
    case 1:
    default:
        return 625;
        break;
    }
}

int ATVDemodSettings::getNumberOfLinesIndex(int nbLines)
{
    if (nbLines <= 32) {
        return 12;
    } else if (nbLines <= 60) {
        return 11;
    } else if (nbLines <= 90) {
        return 10;
    } else if (nbLines <= 120) {
        return 9;
    } else if (nbLines <= 180) {
        return 8;
    } else if (nbLines <= 240) {
        return 7;
    } else if (nbLines <= 343) {
        return 6;
    } else if (nbLines <= 360) {
        return 5;
    } else if (nbLines <= 405) {
        return 4;
    } else if (nbLines <= 480) {
        return 3;
    } else if (nbLines <= 525) {
        return 2;
    } else if (nbLines <= 625) {
        return 1;
    } else {
        return 0;
    }
}

float ATVDemodSettings::getNominalLineTime(int nbLinesIndex, int fpsIndex)
{
    float fps = getFps(fpsIndex);
    int   nbLines = getNumberOfLines(nbLinesIndex);

    return 1.0f / (nbLines * fps);
}

/**
 * calculates m_fltLineTimeMultiplier
 */
void ATVDemodSettings::lineTimeUpdate()
{
    float nominalLineTime = getNominalLineTime(m_nbLinesIndex, m_fpsIndex);
    int lineTimeScaleFactor = (int) std::log10(nominalLineTime);

    if (getEffectiveSampleRate() == 0) {
        m_fltLineTimeMultiplier = std::pow(10.0, lineTimeScaleFactor-3);
    } else {
        m_fltLineTimeMultiplier = 1.0f / getEffectiveSampleRate();
    }
}

float ATVDemodSettings::getLineTime()
{
    return getNominalLineTime(m_nbLinesIndex, m_fpsIndex) + m_fltLineTimeMultiplier * m_lineTimeFactor;
}

int ATVDemodSettings::getLineTimeFactor()
{
    return roundf((m_fltLineDuration - getNominalLineTime(m_nbLinesIndex, m_fpsIndex)) / m_fltLineTimeMultiplier);
}

/**
 * calculates m_fltTopTimeMultiplier
 */
void ATVDemodSettings::topTimeUpdate()
{
    float nominalTopTime = getNominalLineTime(m_nbLinesIndex, m_fpsIndex) * (4.7f / 64.0f);
    int topTimeScaleFactor = (int) std::log10(nominalTopTime);

    if (getEffectiveSampleRate() == 0) {
        m_fltTopTimeMultiplier = std::pow(10.0, topTimeScaleFactor-3);
    } else {
        m_fltTopTimeMultiplier = 1.0f / getEffectiveSampleRate();
    }
}

float ATVDemodSettings::getTopTime()
{
    return getNominalLineTime(m_nbLinesIndex, m_fpsIndex) * (4.7f / 64.0f) + m_fltTopTimeMultiplier * m_topTimeFactor;
}

int ATVDemodSettings::getTopTimeFactor()
{
    return roundf((m_fltTopDuration - getNominalLineTime(m_nbLinesIndex, m_fpsIndex) * (4.7f / 64.0f)) / m_fltLineTimeMultiplier);
}

void ATVDemodSettings::convertFromUIValues()
{
    lineTimeUpdate();
    m_fltLineDuration = getLineTime();
    topTimeUpdate();
    m_fltTopDuration = getTopTime();
    m_fltRFBandwidth = m_RFBandwidthFactor * getRFSliderDivisor() * 1.0f;
    m_fltRFOppBandwidth = m_OppBandwidthFactor * getRFSliderDivisor() * 1.0f;
    m_fltFramePerS = getFps(m_fpsIndex);
    m_intNumberOfLines = getNumberOfLines(m_nbLinesIndex);
}

void ATVDemodSettings::convertToUIValues()
{
    m_lineTimeFactor = getTopTimeFactor();
    m_topTimeFactor = getTopTimeFactor();
    m_RFBandwidthFactor = roundf(m_fltRFBandwidth / getRFSliderDivisor());
    m_RFBandwidthFactor = m_RFBandwidthFactor < 1 ? 1 : m_RFBandwidthFactor;
    m_OppBandwidthFactor = roundf(m_fltRFOppBandwidth / getRFSliderDivisor());
    m_fpsIndex = getFpsIndex(m_fltFramePerS);
    m_nbLinesIndex = getNumberOfLinesIndex(m_intNumberOfLines);
}

int ATVDemodSettings::getRFSliderDivisor()
{
    int sampleRate = getEffectiveSampleRate();
    int scaleFactor = (int) std::log10(sampleRate/2);
    return std::pow(10.0, scaleFactor-1);
}


