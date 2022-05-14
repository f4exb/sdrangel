///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#include "dsp/dspengine.h"
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
