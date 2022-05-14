///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "radioastronomysettings.h"

const QStringList RadioAstronomySettings::m_pipeTypes = {
    QStringLiteral("StarTracker")
};

const QStringList RadioAstronomySettings::m_pipeURIs = {
    QStringLiteral("sdrangel.feature.startracker")
};

RadioAstronomySettings::RadioAstronomySettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void RadioAstronomySettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_sampleRate = 1000000;
    m_rfBandwidth = 1000000;
    m_integration = 4000;
    m_fftSize = 256;
    m_fftWindow = HAN;
    m_filterFreqs = "";

    m_starTracker = "";
    m_rotator = "None";

    m_tempRX = 75.0f;
    m_tempCMB = 2.73;
    m_tempGal = 2.0;
    m_tempSP = 85.0;
    m_tempAtm = 2.0;
    m_tempAir = 15.0;
    m_zenithOpacity = 0.0055;
    m_elevation = 90.0;
    m_tempGalLink = true;
    m_tempAtmLink = true;
    m_tempAirLink = true;
    m_elevationLink = true;

    m_gainVariation = 0.0011f;
    m_sourceType = UNKNOWN;
    m_omegaS = 0.0;
    m_omegaSUnits = DEGREES;
    m_omegaAUnits = DEGREES;

    m_spectrumPeaks = false;
    m_spectrumMarkers = false;
    m_spectrumTemp = false;
    m_spectrumReverseXAxis = false;
    m_spectrumRefLine = false;
    m_spectrumLAB = false;
    m_spectrumDistance = false;
    m_spectrumLegend = false;
    m_spectrumReference = 0.0f;
    m_spectrumRange = 120.0f;
    m_spectrumCenterFreqOffset = 0.0f;
    m_spectrumAutoscale = true;
    m_spectrumSpan = 1.0f;
    m_spectrumYScale = SY_DBFS;
    m_spectrumBaseline = SBL_TSYS0;
    m_recalibrate = false;
    m_tCalHot = 300.0f;
    m_tCalCold = 10.0f;
    m_line = HI;
    m_lineCustomFrequency = 0.0f;
    m_refFrame = LSR;

    m_powerAutoscale = true;
    m_powerReference = 0.0f;
    m_powerRange = 100.0f;
    m_powerPeaks = false;
    m_powerMarkers = false;
    m_powerAvg = false;
    m_powerLegend = false;
    m_powerYData = PY_POWER;
    m_powerYUnits = PY_DBFS;
    m_powerShowTsys0 = false;
    m_powerShowAirTemp = false;
    m_powerShowGaussian = false;

    m_power2DLinkSweep = true;
    m_power2DSweepType = SWP_OFFSET;
    m_power2DHeight = 3;
    m_power2DWidth = 3;
    m_power2DXMin = 0;
    m_power2DXMax = 10;
    m_power2DYMin = 0;
    m_power2DYMax = 10;
    m_powerColourAutoscale = true;
    m_powerColourScaleMin = 0.0f;
    m_powerColourScaleMax = 0.0f;
    m_powerColourPalette = "Colour";

    m_sensorName[0] = "Temperature";
    m_sensorDevice[0] = "";
    m_sensorInit[0] = "UNIT:TEMP C";
    m_sensorMeasure[0] = "MEAS:TEMP?";
    m_sensorEnabled[0] = false;
    m_sensorVisible[0] = false;
    m_sensorName[1] = "Voltage";
    m_sensorDevice[1] = "";
    m_sensorInit[1] = "";
    m_sensorMeasure[1] = "MEAS:VOLT:DC?";
    m_sensorEnabled[1] = false;
    m_sensorVisible[1] = false;
    m_sensorMeasurePeriod = 1.0f;

    m_sunDistanceToGC = 8.1f;
    m_sunOrbitalVelocity = 248.0f;

    m_runMode = CONTINUOUS;
    m_sweepStartAtTime = false;
    m_sweepStartDateTime = QDateTime::currentDateTime();
    m_sweepType = SWP_OFFSET;
    m_sweep1Start = -5.0;
    m_sweep1Stop = 5.0;
    m_sweep1Step = 5.0;
    m_sweep1Delay = 0.0;
    m_sweep2Start = -5.0;
    m_sweep2Stop = 5.0;
    m_sweep2Step = 5.0;
    m_sweep2Delay = 0.0;

    m_gpioEnabled = false;
    m_gpioPin = 0;
    m_gpioSense = 1;
    m_startCalCommand = "";
    m_stopCalCommand = "";
    m_calCommandDelay = 1.0f;

    m_rgbColor = QColor(102, 0, 0).rgb();
    m_title = "Radio Astronomy";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < RADIOASTRONOMY_POWERTABLE_COLUMNS; i++)
    {
        m_powerTableColumnIndexes[i] = i;
        m_powerTableColumnSizes[i] = -1; // Autosize
    }
}

QByteArray RadioAstronomySettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_sampleRate);
    s.writeS32(3, m_rfBandwidth);
    s.writeS32(4, m_integration);
    s.writeS32(5, m_fftSize);
    s.writeS32(6, (int)m_fftWindow);
    s.writeString(7, m_filterFreqs);

    s.writeString(10, m_starTracker);
    s.writeString(11, m_rotator);

    s.writeFloat(20, m_tempRX);
    s.writeFloat(21, m_tempCMB);
    s.writeFloat(22, m_tempGal);
    s.writeFloat(23, m_tempSP);
    s.writeFloat(24, m_tempAtm);
    s.writeFloat(25, m_tempAir);
    s.writeFloat(26, m_zenithOpacity);
    s.writeFloat(27, m_elevation);
    s.writeBool(28, m_tempGalLink);
    s.writeBool(29, m_tempAtmLink);
    s.writeBool(30, m_tempAirLink);
    s.writeBool(31, m_elevationLink);

    s.writeFloat(40, m_gainVariation);
    s.writeS32(41, (int)m_sourceType);
    s.writeFloat(42, m_omegaS);
    s.writeS32(43, (int)m_omegaSUnits);
    s.writeS32(44, (int)m_omegaAUnits);

    s.writeBool(50, m_spectrumPeaks);
    s.writeBool(51, m_spectrumMarkers);
    s.writeBool(52, m_spectrumTemp);
    s.writeBool(53, m_spectrumReverseXAxis);
    s.writeBool(54, m_spectrumRefLine);
    s.writeBool(55, m_spectrumLegend);
    s.writeBool(56, m_spectrumDistance);
    s.writeBool(57, m_spectrumLAB);

    s.writeFloat(60, m_spectrumReference);
    s.writeFloat(61, m_spectrumRange);
    s.writeFloat(62, m_spectrumSpan);
    s.writeFloat(63, m_spectrumCenterFreqOffset);
    s.writeBool(64, m_spectrumAutoscale);
    s.writeS32(65, (int)m_spectrumYScale);
    s.writeS32(66, (int)m_spectrumBaseline);

    s.writeBool(70, m_recalibrate);
    s.writeFloat(71, m_tCalHot);
    s.writeFloat(72, m_tCalCold);

    s.writeS32(73, (int)m_line);
    s.writeFloat(74, m_lineCustomFrequency);
    s.writeS32(75, (int)m_refFrame);

    s.writeFloat(76, m_sunDistanceToGC);
    s.writeFloat(77, m_sunOrbitalVelocity);

    s.writeBool(80, m_powerPeaks);
    s.writeBool(81, m_powerMarkers);
    s.writeBool(82, m_powerAvg);
    s.writeBool(83, m_powerLegend);
    s.writeBool(84, m_powerShowTsys0);
    s.writeBool(85, m_powerShowAirTemp);
    s.writeBool(86, m_powerShowGaussian);
    s.writeFloat(87, m_powerReference);
    s.writeFloat(88, m_powerRange);
    s.writeBool(89, m_powerAutoscale);
    s.writeS32(90, (int)m_powerYData);
    s.writeS32(91, (int)m_powerYUnits);

    s.writeBool(100, m_power2DLinkSweep);
    s.writeS32(102, (int)m_power2DSweepType);
    s.writeS32(103, m_power2DWidth);
    s.writeS32(104, m_power2DHeight);
    s.writeFloat(105, m_power2DXMin);
    s.writeFloat(106, m_power2DXMax);
    s.writeFloat(107, m_power2DYMin);
    s.writeFloat(108, m_power2DYMax);
    s.writeBool(109, m_powerColourAutoscale);
    s.writeFloat(110, m_powerColourScaleMin);
    s.writeFloat(111, m_powerColourScaleMax);
    s.writeString(112, m_powerColourPalette);

    s.writeS32(120, m_runMode);
    s.writeBool(121, m_sweepStartAtTime);
    s.writeS64(122, m_sweepStartDateTime.toMSecsSinceEpoch());
    s.writeS32(123, (int)m_sweepType);
    s.writeFloat(124, m_sweep1Start);
    s.writeFloat(125, m_sweep1Stop);
    s.writeFloat(126, m_sweep1Step);
    s.writeFloat(127, m_sweep1Delay);
    s.writeFloat(128, m_sweep2Start);
    s.writeFloat(129, m_sweep2Stop);
    s.writeFloat(130, m_sweep2Step);
    s.writeFloat(131, m_sweep2Delay);

    s.writeString(140, m_sensorName[0]);
    s.writeString(141, m_sensorDevice[0]);
    s.writeString(142, m_sensorInit[0]);
    s.writeString(143, m_sensorMeasure[0]);
    s.writeBool(144, m_sensorEnabled[0]);
    s.writeBool(145, m_sensorVisible[0]);
    s.writeString(146, m_sensorName[1]);
    s.writeString(147, m_sensorDevice[1]);
    s.writeString(148, m_sensorInit[1]);
    s.writeString(149, m_sensorMeasure[1]);
    s.writeBool(150, m_sensorEnabled[1]);
    s.writeBool(151, m_sensorVisible[1]);
    s.writeFloat(152, m_sensorMeasurePeriod);

    s.writeBool(160, m_gpioEnabled);
    s.writeS32(161, m_gpioPin);
    s.writeS32(162, m_gpioSense);

    s.writeString(167, m_startCalCommand);
    s.writeString(168, m_stopCalCommand);
    s.writeFloat(169, m_calCommandDelay);

    s.writeU32(180, m_rgbColor);
    s.writeString(181, m_title);
    if (m_channelMarker) {
        s.writeBlob(182, m_channelMarker->serialize());
    }
    s.writeS32(183, m_streamIndex);
    s.writeBool(184, m_useReverseAPI);
    s.writeString(185, m_reverseAPIAddress);
    s.writeU32(186, m_reverseAPIPort);
    s.writeU32(187, m_reverseAPIDeviceIndex);
    s.writeU32(188, m_reverseAPIChannelIndex);
    s.writeS32(189, m_workspaceIndex);
    s.writeBlob(190, m_geometryBytes);
    s.writeBool(191, m_hidden);

    for (int i = 0; i < RADIOASTRONOMY_POWERTABLE_COLUMNS; i++) {
        s.writeS32(400 + i, m_powerTableColumnIndexes[i]);
    }
    for (int i = 0; i < RADIOASTRONOMY_POWERTABLE_COLUMNS; i++) {
        s.writeS32(500 + i, m_powerTableColumnSizes[i]);
    }

    return s.final();
}

bool RadioAstronomySettings::deserialize(const QByteArray& data)
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
        qint64 dttmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_sampleRate, 1000000);
        d.readS32(3, &m_rfBandwidth, 1000000);
        d.readS32(4, &m_integration, 4000);
        d.readS32(5, &m_fftSize, 256);
        d.readS32(6, (int*)&m_fftWindow, (int)HAN);
        d.readString(7, &m_filterFreqs, "");

        d.readString(10, &m_starTracker, "");
        d.readString(11, &m_rotator, "None");

        d.readFloat(20, &m_tempRX, 75.0f);
        d.readFloat(21, &m_tempCMB, 2.73f);
        d.readFloat(22, &m_tempGal, 2.0f);
        d.readFloat(23, &m_tempSP, 85.0f);
        d.readFloat(24, &m_tempAtm, 2.0f);
        d.readFloat(25, &m_tempAir, 15.0f);
        d.readFloat(26, &m_zenithOpacity, 0.0055f);
        d.readFloat(27, &m_elevation, 90.0f);
        d.readBool(28, &m_tempGalLink, true);
        d.readBool(29, &m_tempAtmLink, true);
        d.readBool(30, &m_tempAirLink, true);
        d.readBool(31, &m_elevationLink, true);

        d.readFloat(40, &m_gainVariation, 0.0011f);
        d.readS32(41, (int*)&m_sourceType, UNKNOWN);
        d.readFloat(42, &m_omegaS, 0.0f);
        d.readS32(43, (int*)&m_omegaSUnits, DEGREES);
        d.readS32(44, (int*)&m_omegaAUnits, DEGREES);

        d.readBool(50, &m_spectrumPeaks, false);
        d.readBool(51, &m_spectrumMarkers, false);
        d.readBool(52, &m_spectrumTemp, false);
        d.readBool(53, &m_spectrumReverseXAxis, false);
        d.readBool(54, &m_spectrumRefLine, false);
        d.readBool(55, &m_spectrumLegend, false);
        d.readBool(56, &m_spectrumDistance, false);
        d.readBool(57, &m_spectrumLAB, false);

        d.readFloat(60, &m_spectrumReference, 0.0f);
        d.readFloat(61, &m_spectrumRange, 120.0f);
        d.readFloat(62, &m_spectrumSpan, 1.0f);
        d.readFloat(63, &m_spectrumCenterFreqOffset, 0.0f);
        d.readBool(64, &m_spectrumAutoscale, false);
        d.readS32(65, (int*)&m_spectrumYScale, SY_DBFS);
        d.readS32(66, (int*)&m_spectrumBaseline, SBL_TSYS0);

        d.readBool(70, &m_recalibrate, false);
        d.readFloat(71, &m_tCalHot, 300.0f);
        d.readFloat(72, &m_tCalCold, 10.0f);

        d.readS32(73, (int*)&m_line, (int)HI);
        d.readFloat(74, &m_lineCustomFrequency, 0.0f);
        d.readS32(75, (int*)&m_refFrame, LSR);

        d.readFloat(76, &m_sunDistanceToGC, 8.1f);
        d.readFloat(77, &m_sunOrbitalVelocity, 248.0f);

        d.readBool(80, &m_powerPeaks, false);
        d.readBool(81, &m_powerMarkers, false);
        d.readBool(82, &m_powerAvg, false);
        d.readBool(83, &m_powerLegend, false);
        d.readBool(84, &m_powerShowTsys0, false);
        d.readBool(85, &m_powerShowAirTemp, false);
        d.readBool(86, &m_powerShowGaussian, false);
        d.readFloat(87, &m_powerReference, 0.0f);
        d.readFloat(88, &m_powerRange, 100.0f);
        d.readBool(89, &m_powerAutoscale, true);
        d.readS32(90, (int*)&m_powerYData, PY_POWER);
        d.readS32(91, (int*)&m_powerYUnits, PY_DBFS);

        d.readBool(100, &m_power2DLinkSweep, true);
        d.readS32(102, (int*)&m_power2DSweepType, SWP_OFFSET);
        d.readS32(103, &m_power2DWidth, 3);
        d.readS32(104, &m_power2DHeight, 3);
        d.readFloat(105, &m_power2DXMin, 0);
        d.readFloat(106, &m_power2DXMax, 10);
        d.readFloat(107, &m_power2DYMin, 0);
        d.readFloat(108, &m_power2DYMax, 10);
        d.readBool(109, &m_powerColourAutoscale, true);
        d.readFloat(110, &m_powerColourScaleMin, 0.0f);
        d.readFloat(111, &m_powerColourScaleMax, 0.0f);
        d.readString(112, &m_powerColourPalette, "Colour");

        d.readS32(120, (int *)&m_runMode, CONTINUOUS);
        d.readBool(121, &m_sweepStartAtTime, false);
        d.readS64(122, &dttmp, QDateTime::currentDateTime().toMSecsSinceEpoch());
        m_sweepStartDateTime = QDateTime::fromMSecsSinceEpoch(dttmp);
        d.readS32(123, (int*)&m_sweepType, SWP_OFFSET);
        d.readFloat(124, &m_sweep1Start, -5.0f);
        d.readFloat(125, &m_sweep1Stop, 5.0f);
        d.readFloat(126, &m_sweep1Step, 5.0f);
        d.readFloat(127, &m_sweep1Delay, 0.0f);
        d.readFloat(128, &m_sweep2Start, -5.0f);
        d.readFloat(129, &m_sweep2Stop, 5.0);
        d.readFloat(130, &m_sweep2Step, 5.0f);
        d.readFloat(131, &m_sweep2Delay, 0.0f);

        d.readString(140, &m_sensorName[0], "");
        d.readString(141, &m_sensorDevice[0], "");
        d.readString(142, &m_sensorInit[0], "");
        d.readString(143, &m_sensorMeasure[0], "");
        d.readBool(144, &m_sensorEnabled[0], false);
        d.readBool(145, &m_sensorVisible[0], false);
        d.readString(146, &m_sensorName[1], "");
        d.readString(147, &m_sensorDevice[1], "");
        d.readString(148, &m_sensorInit[1], "");
        d.readString(149, &m_sensorMeasure[1], "");
        d.readBool(150, &m_sensorEnabled[1], false);
        d.readBool(151, &m_sensorVisible[1], false);
        d.readFloat(152, &m_sensorMeasurePeriod, 1.0f);

        d.readBool(160, &m_gpioEnabled, false);
        d.readS32(161, &m_gpioPin, 0);
        d.readS32(162, &m_gpioSense, 1);

        d.readString(167, &m_startCalCommand, "");
        d.readString(168, &m_stopCalCommand, "");
        d.readFloat(169, &m_calCommandDelay, 1.0f);

        d.readU32(180, &m_rgbColor, QColor(102, 0, 0).rgb());
        d.readString(181, &m_title, "Radio Astronomy");
        d.readBlob(182, &bytetmp);
        if (m_channelMarker) {
            m_channelMarker->deserialize(bytetmp);
        }
        d.readS32(183, &m_streamIndex, 0);
        d.readBool(184, &m_useReverseAPI, false);
        d.readString(185, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(186, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }
        d.readU32(187, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(188, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(189, &m_workspaceIndex, 0);
        d.readBlob(190, &m_geometryBytes);
        d.readBool(191, &m_hidden, false);

        for (int i = 0; i < RADIOASTRONOMY_POWERTABLE_COLUMNS; i++) {
            d.readS32(400 + i, &m_powerTableColumnIndexes[i], i);
        }
        for (int i = 0; i < RADIOASTRONOMY_POWERTABLE_COLUMNS; i++) {
            d.readS32(500 + i, &m_powerTableColumnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}


