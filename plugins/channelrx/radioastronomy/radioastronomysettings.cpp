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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "radioastronomysettings.h"

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
    m_powerShowFiltered = false;
    m_powerFilter = FILT_MEDIAN;
    m_powerFilterN = 10;
    m_powerShowMeasurement = true;

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

    m_spectrumAutoSaveCSVFilename = "";
    m_powerAutoSaveCSVFilename = "";

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
    s.writeBool(92, m_powerShowFiltered);
    s.writeS32(93, (int)m_powerFilter);
    s.writeS32(94, m_powerFilterN);
    s.writeBool(95, m_powerShowMeasurement);

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

    s.writeString(170, m_spectrumAutoSaveCSVFilename);
    s.writeString(171, m_powerAutoSaveCSVFilename);

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
        d.readBool(92, &m_powerShowFiltered, false);
        d.readS32(93, (int*)&m_powerFilter, FILT_MEDIAN);
        d.readS32(94, &m_powerFilterN, 10);
        d.readBool(95, &m_powerShowMeasurement, true);

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

        d.readString(170, &m_spectrumAutoSaveCSVFilename, "");
        d.readString(171, &m_powerAutoSaveCSVFilename, "");

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

void RadioAstronomySettings::applySettings(const QStringList& settingsKeys, const RadioAstronomySettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("sampleRate")) {
        m_sampleRate = settings.m_sampleRate;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("integration")) {
        m_integration = settings.m_integration;
    }
    if (settingsKeys.contains("fftSize")) {
        m_fftSize = settings.m_fftSize;
    }
    if (settingsKeys.contains("fftWindow")) {
        m_fftWindow = settings.m_fftWindow;
    }
    if (settingsKeys.contains("filterFreqs")) {
        m_filterFreqs = settings.m_filterFreqs;
    }
    if (settingsKeys.contains("starTracker")) {
        m_starTracker = settings.m_starTracker;
    }
    if (settingsKeys.contains("rotator")) {
        m_rotator = settings.m_rotator;
    }
    if (settingsKeys.contains("tempRX")) {
        m_tempRX = settings.m_tempRX;
    }
    if (settingsKeys.contains("tempCMB")) {
        m_tempCMB = settings.m_tempCMB;
    }
    if (settingsKeys.contains("tempGal")) {
        m_tempGal = settings.m_tempGal;
    }
    if (settingsKeys.contains("tempSP")) {
        m_tempSP = settings.m_tempSP;
    }
    if (settingsKeys.contains("tempAtm")) {
        m_tempAtm = settings.m_tempAtm;
    }
    if (settingsKeys.contains("tempAir")) {
        m_tempAir = settings.m_tempAir;
    }
    if (settingsKeys.contains("zenithOpacity")) {
        m_zenithOpacity = settings.m_zenithOpacity;
    }
    if (settingsKeys.contains("elevation")) {
        m_elevation = settings.m_elevation;
    }
    if (settingsKeys.contains("tempGalLink")) {
        m_tempGalLink = settings.m_tempGalLink;
    }
    if (settingsKeys.contains("tempAtmLink")) {
        m_tempAtmLink = settings.m_tempAtmLink;
    }
    if (settingsKeys.contains("tempAirLink")) {
        m_tempAirLink = settings.m_tempAirLink;
    }
    if (settingsKeys.contains("elevationLink")) {
        m_elevationLink = settings.m_elevationLink;
    }
    if (settingsKeys.contains("gainVariation")) {
        m_gainVariation = settings.m_gainVariation;
    }
    if (settingsKeys.contains("sourceType")) {
        m_sourceType = settings.m_sourceType;
    }
    if (settingsKeys.contains("omegaS")) {
        m_omegaS = settings.m_omegaS;
    }
    if (settingsKeys.contains("omegaSUnits")) {
        m_omegaSUnits = settings.m_omegaSUnits;
    }
    if (settingsKeys.contains("omegaAUnits")) {
        m_omegaAUnits = settings.m_omegaAUnits;
    }
    if (settingsKeys.contains("spectrumPeaks")) {
        m_spectrumPeaks = settings.m_spectrumPeaks;
    }
    if (settingsKeys.contains("spectrumMarkers")) {
        m_spectrumMarkers = settings.m_spectrumMarkers;
    }
    if (settingsKeys.contains("spectrumTemp")) {
        m_spectrumTemp = settings.m_spectrumTemp;
    }
    if (settingsKeys.contains("spectrumReverseXAxis")) {
        m_spectrumReverseXAxis = settings.m_spectrumReverseXAxis;
    }
    if (settingsKeys.contains("spectrumRefLine")) {
        m_spectrumRefLine = settings.m_spectrumRefLine;
    }
    if (settingsKeys.contains("spectrumLAB")) {
        m_spectrumLAB = settings.m_spectrumLAB;
    }
    if (settingsKeys.contains("spectrumDistance")) {
        m_spectrumDistance = settings.m_spectrumDistance;
    }
    if (settingsKeys.contains("spectrumLegend")) {
        m_spectrumLegend = settings.m_spectrumLegend;
    }
    if (settingsKeys.contains("spectrumReference")) {
        m_spectrumReference = settings.m_spectrumReference;
    }
    if (settingsKeys.contains("spectrumRange")) {
        m_spectrumRange = settings.m_spectrumRange;
    }
    if (settingsKeys.contains("spectrumSpan")) {
        m_spectrumSpan = settings.m_spectrumSpan;
    }
    if (settingsKeys.contains("spectrumCenterFreqOffset")) {
        m_spectrumCenterFreqOffset = settings.m_spectrumCenterFreqOffset;
    }
    if (settingsKeys.contains("spectrumAutoscale")) {
        m_spectrumAutoscale = settings.m_spectrumAutoscale;
    }
    if (settingsKeys.contains("spectrumYScale")) {
        m_spectrumYScale = settings.m_spectrumYScale;
    }
    if (settingsKeys.contains("spectrumBaseline")) {
        m_spectrumBaseline = settings.m_spectrumBaseline;
    }
    if (settingsKeys.contains("recalibrate")) {
        m_recalibrate = settings.m_recalibrate;
    }
    if (settingsKeys.contains("tCalHot")) {
        m_tCalHot = settings.m_tCalHot;
    }
    if (settingsKeys.contains("tCalCold")) {
        m_tCalCold = settings.m_tCalCold;
    }
    if (settingsKeys.contains("line")) {
        m_line = settings.m_line;
    }
    if (settingsKeys.contains("lineCustomFrequency")) {
        m_lineCustomFrequency = settings.m_lineCustomFrequency;
    }
    if (settingsKeys.contains("refFrame")) {
        m_refFrame = settings.m_refFrame;
    }
    if (settingsKeys.contains("sunDistanceToGC")) {
        m_sunDistanceToGC = settings.m_sunDistanceToGC;
    }
    if (settingsKeys.contains("sunOrbitalVelocity")) {
        m_sunOrbitalVelocity = settings.m_sunOrbitalVelocity;
    }
    if (settingsKeys.contains("powerPeaks")) {
        m_powerPeaks = settings.m_powerPeaks;
    }
    if (settingsKeys.contains("powerMarkers")) {
        m_powerMarkers = settings.m_powerMarkers;
    }
    if (settingsKeys.contains("powerAvg")) {
        m_powerAvg = settings.m_powerAvg;
    }
    if (settingsKeys.contains("powerLegend")) {
        m_powerLegend = settings.m_powerLegend;
    }
    if (settingsKeys.contains("powerShowTsys0")) {
        m_powerShowTsys0 = settings.m_powerShowTsys0;
    }
    if (settingsKeys.contains("powerShowAirTemp")) {
        m_powerShowAirTemp = settings.m_powerShowAirTemp;
    }
    if (settingsKeys.contains("powerShowGaussian")) {
        m_powerShowGaussian = settings.m_powerShowGaussian;
    }
    if (settingsKeys.contains("powerShowFiltered")) {
        m_powerShowFiltered = settings.m_powerShowFiltered;
    }
    if (settingsKeys.contains("powerShowMeasurement")) {
        m_powerShowMeasurement = settings.m_powerShowMeasurement;
    }
    if (settingsKeys.contains("powerReference")) {
        m_powerReference = settings.m_powerReference;
    }
    if (settingsKeys.contains("powerRange")) {
        m_powerRange = settings.m_powerRange;
    }
    if (settingsKeys.contains("powerAutoscale")) {
        m_powerAutoscale = settings.m_powerAutoscale;
    }
    if (settingsKeys.contains("powerYData")) {
        m_powerYData = settings.m_powerYData;
    }
    if (settingsKeys.contains("powerYUnits")) {
        m_powerYUnits = settings.m_powerYUnits;
    }
    if (settingsKeys.contains("powerFilter")) {
        m_powerFilter = settings.m_powerFilter;
    }
    if (settingsKeys.contains("powerFilterN")) {
        m_powerFilterN = settings.m_powerFilterN;
    }
    if (settingsKeys.contains("power2DLinkSweep")) {
        m_power2DLinkSweep = settings.m_power2DLinkSweep;
    }
    if (settingsKeys.contains("power2DSweepType")) {
        m_power2DSweepType = settings.m_power2DSweepType;
    }
    if (settingsKeys.contains("power2DWidth")) {
        m_power2DWidth = settings.m_power2DWidth;
    }
    if (settingsKeys.contains("power2DHeight")) {
        m_power2DHeight = settings.m_power2DHeight;
    }
    if (settingsKeys.contains("power2DXMin")) {
        m_power2DXMin = settings.m_power2DXMin;
    }
    if (settingsKeys.contains("power2DXMax")) {
        m_power2DXMax = settings.m_power2DXMax;
    }
    if (settingsKeys.contains("power2DYMin")) {
        m_power2DYMin = settings.m_power2DYMin;
    }
    if (settingsKeys.contains("power2DYMax")) {
        m_power2DYMax = settings.m_power2DYMax;
    }
    if (settingsKeys.contains("powerColourAutoscale")) {
        m_powerColourAutoscale = settings.m_powerColourAutoscale;
    }
    if (settingsKeys.contains("powerColourScaleMin")) {
        m_powerColourScaleMin = settings.m_powerColourScaleMin;
    }
    if (settingsKeys.contains("powerColourScaleMax")) {
        m_powerColourScaleMax = settings.m_powerColourScaleMax;
    }
    if (settingsKeys.contains("powerColourPalette")) {
        m_powerColourPalette = settings.m_powerColourPalette;
    }
    if (settingsKeys.contains("runMode")) {
        m_runMode = settings.m_runMode;
    }
    if (settingsKeys.contains("sweepStartAtTime")) {
        m_sweepStartAtTime = settings.m_sweepStartAtTime;
    }
    if (settingsKeys.contains("sweepStartDateTime")) {
        m_sweepStartDateTime = settings.m_sweepStartDateTime;
    }
    if (settingsKeys.contains("sweepType")) {
        m_sweepType = settings.m_sweepType;
    }
    if (settingsKeys.contains("sweep1Start")) {
        m_sweep1Start = settings.m_sweep1Start;
    }
    if (settingsKeys.contains("sweep1Stop")) {
        m_sweep1Stop = settings.m_sweep1Stop;
    }
    if (settingsKeys.contains("sweep1Step")) {
        m_sweep1Step = settings.m_sweep1Step;
    }
    if (settingsKeys.contains("sweep1Delay")) {
        m_sweep1Delay = settings.m_sweep1Delay;
    }
    if (settingsKeys.contains("sweep2Start")) {
        m_sweep2Start = settings.m_sweep2Start;
    }
    if (settingsKeys.contains("sweep2Stop")) {
        m_sweep2Stop = settings.m_sweep2Stop;
    }
    if (settingsKeys.contains("sweep2Step")) {
        m_sweep2Step = settings.m_sweep2Step;
    }
    if (settingsKeys.contains("sweep2Delay")) {
        m_sweep2Delay = settings.m_sweep2Delay;
    }
    if (settingsKeys.contains("gpioEnabled")) {
        m_gpioEnabled = settings.m_gpioEnabled;
    }
    if (settingsKeys.contains("gpioPin")) {
        m_gpioPin = settings.m_gpioPin;
    }
    if (settingsKeys.contains("gpioSense")) {
        m_gpioSense = settings.m_gpioSense;
    }
    if (settingsKeys.contains("startCalCommand")) {
        m_startCalCommand = settings.m_startCalCommand;
    }
    if (settingsKeys.contains("stopCalCommand")) {
        m_stopCalCommand = settings.m_stopCalCommand;
    }
    if (settingsKeys.contains("calCommandDelay")) {
        m_calCommandDelay = settings.m_calCommandDelay;
    }
    if (settingsKeys.contains("sensorMeasurePeriod")) {
        m_sensorMeasurePeriod = settings.m_sensorMeasurePeriod;
    }
    if (settingsKeys.contains("spectrumAutoSaveCSVFilename")) {
        m_spectrumAutoSaveCSVFilename = settings.m_spectrumAutoSaveCSVFilename;
    }
    if (settingsKeys.contains("powerAutoSaveCSVFilename")) {
        m_powerAutoSaveCSVFilename = settings.m_powerAutoSaveCSVFilename;
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

QString RadioAstronomySettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("sampleRate") || force) {
        ostr << " m_sampleRate: " << m_sampleRate;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("integration") || force) {
        ostr << " m_integration: " << m_integration;
    }
    if (settingsKeys.contains("fftSize") || force) {
        ostr << " m_fftSize: " << m_fftSize;
    }
    if (settingsKeys.contains("fftWindow") || force) {
        ostr << " m_fftWindow: " << m_fftWindow;
    }
    if (settingsKeys.contains("filterFreqs") || force) {
        ostr << " m_filterFreqs: " << m_filterFreqs.toStdString();
    }
    if (settingsKeys.contains("starTracker") || force) {
        ostr << " m_starTracker: " << m_starTracker.toStdString();
    }
    if (settingsKeys.contains("rotator") || force) {
        ostr << " m_rotator: " << m_rotator.toStdString();
    }
    if (settingsKeys.contains("tempRX") || force) {
        ostr << " m_tempRX: " << m_tempRX;
    }
    if (settingsKeys.contains("tempCMB") || force) {
        ostr << " m_tempCMB: " << m_tempCMB;
    }
    if (settingsKeys.contains("tempGal") || force) {
        ostr << " m_tempGal: " << m_tempGal;
    }
    if (settingsKeys.contains("tempSP") || force) {
        ostr << " m_tempSP: " << m_tempSP;
    }
    if (settingsKeys.contains("tempAtm") || force) {
        ostr << " m_tempAtm: " << m_tempAtm;
    }
    if (settingsKeys.contains("tempAir") || force) {
        ostr << " m_tempAir: " << m_tempAir;
    }
    if (settingsKeys.contains("zenithOpacity") || force) {
        ostr << " m_zenithOpacity: " << m_zenithOpacity;
    }
    if (settingsKeys.contains("elevation") || force) {
        ostr << " m_elevation: " << m_elevation;
    }
    if (settingsKeys.contains("tempGalLink") || force) {
        ostr << " m_tempGalLink: " << m_tempGalLink;
    }
    if (settingsKeys.contains("tempAtmLink") || force) {
        ostr << " m_tempAtmLink: " << m_tempAtmLink;
    }
    if (settingsKeys.contains("tempAirLink") || force) {
        ostr << " m_tempAirLink: " << m_tempAirLink;
    }
    if (settingsKeys.contains("elevationLink") || force) {
        ostr << " m_elevationLink: " << m_elevationLink;
    }
    if (settingsKeys.contains("gainVariation") || force) {
        ostr << " m_gainVariation: " << m_gainVariation;
    }
    if (settingsKeys.contains("sourceType") || force) {
        ostr << " m_sourceType: " << m_sourceType;
    }
    if (settingsKeys.contains("omegaS") || force) {
        ostr << " m_omegaS: " << m_omegaS;
    }
    if (settingsKeys.contains("omegaSUnits") || force) {
        ostr << " m_omegaSUnits: " << m_omegaSUnits;
    }
    if (settingsKeys.contains("omegaAUnits") || force) {
        ostr << " m_omegaAUnits: " << m_omegaAUnits;
    }
    if (settingsKeys.contains("spectrumPeaks") || force) {
        ostr << " m_spectrumPeaks: " << m_spectrumPeaks;
    }
    if (settingsKeys.contains("spectrumMarkers") || force) {
        ostr << " m_spectrumMarkers: " << m_spectrumMarkers;
    }
    if (settingsKeys.contains("spectrumTemp") || force) {
        ostr << " m_spectrumTemp: " << m_spectrumTemp;
    }
    if (settingsKeys.contains("spectrumReverseXAxis") || force) {
        ostr << " m_spectrumReverseXAxis: " << m_spectrumReverseXAxis;
    }
    if (settingsKeys.contains("spectrumRefLine") || force) {
        ostr << " m_spectrumRefLine: " << m_spectrumRefLine;
    }
    if (settingsKeys.contains("spectrumLAB") || force) {
        ostr << " m_spectrumLAB: " << m_spectrumLAB;
    }
    if (settingsKeys.contains("spectrumDistance") || force) {
        ostr << " m_spectrumDistance: " << m_spectrumDistance;
    }
    if (settingsKeys.contains("spectrumLegend") || force) {
        ostr << " m_spectrumLegend: " << m_spectrumLegend;
    }
    if (settingsKeys.contains("spectrumReference") || force) {
        ostr << " m_spectrumReference: " << m_spectrumReference;
    }
    if (settingsKeys.contains("spectrumRange") || force) {
        ostr << " m_spectrumRange: " << m_spectrumRange;
    }
    if (settingsKeys.contains("spectrumSpan") || force) {
        ostr << " m_spectrumSpan: " << m_spectrumSpan;
    }
    if (settingsKeys.contains("spectrumCenterFreqOffset") || force) {
        ostr << " m_spectrumCenterFreqOffset: " << m_spectrumCenterFreqOffset;
    }
    if (settingsKeys.contains("spectrumAutoscale") || force) {
        ostr << " m_spectrumAutoscale: " << m_spectrumAutoscale;
    }
    if (settingsKeys.contains("spectrumYScale") || force) {
        ostr << " m_spectrumYScale: " << m_spectrumYScale;
    }
    if (settingsKeys.contains("spectrumBaseline") || force) {
        ostr << " m_spectrumBaseline: " << m_spectrumBaseline;
    }
    if (settingsKeys.contains("recalibrate") || force) {
        ostr << " m_recalibrate: " << m_recalibrate;
    }
    if (settingsKeys.contains("tCalHot") || force) {
        ostr << " m_tCalHot: " << m_tCalHot;
    }
    if (settingsKeys.contains("tCalCold") || force) {
        ostr << " m_tCalCold: " << m_tCalCold;
    }
    if (settingsKeys.contains("line") || force) {
        ostr << " m_line: " << m_line;
    }
    if (settingsKeys.contains("lineCustomFrequency") || force) {
        ostr << " m_lineCustomFrequency: " << m_lineCustomFrequency;
    }
    if (settingsKeys.contains("refFrame") || force) {
        ostr << " m_refFrame: " << m_refFrame;
    }
    if (settingsKeys.contains("sunDistanceToGC") || force) {
        ostr << " m_sunDistanceToGC: " << m_sunDistanceToGC;
    }
    if (settingsKeys.contains("sunOrbitalVelocity") || force) {
        ostr << " m_sunOrbitalVelocity: " << m_sunOrbitalVelocity;
    }
    if (settingsKeys.contains("powerPeaks") || force) {
        ostr << " m_powerPeaks: " << m_powerPeaks;
    }
    if (settingsKeys.contains("powerMarkers") || force) {
        ostr << " m_powerMarkers: " << m_powerMarkers;
    }
    if (settingsKeys.contains("powerAvg") || force) {
        ostr << " m_powerAvg: " << m_powerAvg;
    }
    if (settingsKeys.contains("powerLegend") || force) {
        ostr << " m_powerLegend: " << m_powerLegend;
    }
    if (settingsKeys.contains("powerShowTsys0") || force) {
        ostr << " m_powerShowTsys0: " << m_powerShowTsys0;
    }
    if (settingsKeys.contains("powerShowAirTemp") || force) {
        ostr << " m_powerShowAirTemp: " << m_powerShowAirTemp;
    }
    if (settingsKeys.contains("powerShowGaussian") || force) {
        ostr << " m_powerShowGaussian: " << m_powerShowGaussian;
    }
    if (settingsKeys.contains("powerShowFiltered") || force) {
        ostr << " m_powerShowFiltered: " << m_powerShowFiltered;
    }
    if (settingsKeys.contains("powerShowMeasurement") || force) {
        ostr << " m_powerShowMeasurement: " << m_powerShowMeasurement;
    }
    if (settingsKeys.contains("powerReference") || force) {
        ostr << " m_powerReference: " << m_powerReference;
    }
    if (settingsKeys.contains("powerRange") || force) {
        ostr << " m_powerRange: " << m_powerRange;
    }
    if (settingsKeys.contains("powerAutoscale") || force) {
        ostr << " m_powerAutoscale: " << m_powerAutoscale;
    }
    if (settingsKeys.contains("powerYData") || force) {
        ostr << " m_powerYData: " << m_powerYData;
    }
    if (settingsKeys.contains("powerYUnits") || force) {
        ostr << " m_powerYUnits: " << m_powerYUnits;
    }
    if (settingsKeys.contains("powerFilter") || force) {
        ostr << " m_powerFilter: " << m_powerFilter;
    }
    if (settingsKeys.contains("powerFilterN") || force) {
        ostr << " m_powerFilterN: " << m_powerFilterN;
    }
    if (settingsKeys.contains("power2DLinkSweep") || force) {
        ostr << " m_power2DLinkSweep: " << m_power2DLinkSweep;
    }
    if (settingsKeys.contains("power2DSweepType") || force) {
        ostr << " m_power2DSweepType: " << m_power2DSweepType;
    }
    if (settingsKeys.contains("power2DWidth") || force) {
        ostr << " m_power2DWidth: " << m_power2DWidth;
    }
    if (settingsKeys.contains("power2DHeight") || force) {
        ostr << " m_power2DHeight: " << m_power2DHeight;
    }
    if (settingsKeys.contains("power2DXMin") || force) {
        ostr << " m_power2DXMin: " << m_power2DXMin;
    }
    if (settingsKeys.contains("power2DXMax") || force) {
        ostr << " m_power2DXMax: " << m_power2DXMax;
    }
    if (settingsKeys.contains("power2DYMin") || force) {
        ostr << " m_power2DYMin: " << m_power2DYMin;
    }
    if (settingsKeys.contains("power2DYMax") || force) {
        ostr << " m_power2DYMax: " << m_power2DYMax;
    }
    if (settingsKeys.contains("powerColourAutoscale") || force) {
        ostr << " m_powerColourAutoscale: " << m_powerColourAutoscale;
    }
    if (settingsKeys.contains("powerColourScaleMin") || force) {
        ostr << " m_powerColourScaleMin: " << m_powerColourScaleMin;
    }
    if (settingsKeys.contains("powerColourScaleMax") || force) {
        ostr << " m_powerColourScaleMax: " << m_powerColourScaleMax;
    }
    if (settingsKeys.contains("powerColourPalette") || force) {
        ostr << " m_powerColourPalette: " << m_powerColourPalette.toStdString();
    }
    if (settingsKeys.contains("runMode") || force) {
        ostr << " m_runMode: " << m_runMode;
    }
    if (settingsKeys.contains("sweepStartAtTime") || force) {
        ostr << " m_sweepStartAtTime: " << m_sweepStartAtTime;
    }
    if (settingsKeys.contains("sweepStartDateTime") || force) {
        ostr << " m_sweepStartDateTime: " << m_sweepStartDateTime.toString().toStdString();
    }
    if (settingsKeys.contains("sweepType") || force) {
        ostr << " m_sweepType: " << m_sweepType;
    }
    if (settingsKeys.contains("sweep1Start") || force) {
        ostr << " m_sweep1Start: " << m_sweep1Start;
    }
    if (settingsKeys.contains("sweep1Stop") || force) {
        ostr << " m_sweep1Stop: " << m_sweep1Stop;
    }
    if (settingsKeys.contains("sweep1Step") || force) {
        ostr << " m_sweep1Step: " << m_sweep1Step;
    }
    if (settingsKeys.contains("sweep1Delay") || force) {
        ostr << " m_sweep1Delay: " << m_sweep1Delay;
    }
    if (settingsKeys.contains("sweep2Start") || force) {
        ostr << " m_sweep2Start: " << m_sweep2Start;
    }
    if (settingsKeys.contains("sweep2Stop") || force) {
        ostr << " m_sweep2Stop: " << m_sweep2Stop;
    }
    if (settingsKeys.contains("sweep2Step") || force) {
        ostr << " m_sweep2Step: " << m_sweep2Step;
    }
    if (settingsKeys.contains("sweep2Delay") || force) {
        ostr << " m_sweep2Delay: " << m_sweep2Delay;
    }
    if (settingsKeys.contains("gpioEnabled") || force) {
        ostr << " m_gpioEnabled: " << m_gpioEnabled;
    }
    if (settingsKeys.contains("gpioPin") || force) {
        ostr << " m_gpioPin: " << m_gpioPin;
    }
    if (settingsKeys.contains("gpioSense") || force) {
        ostr << " m_gpioSense: " << m_gpioSense;
    }
    if (settingsKeys.contains("startCalCommand") || force) {
        ostr << " m_startCalCommand: " << m_startCalCommand.toStdString();
    }
    if (settingsKeys.contains("stopCalCommand") || force) {
        ostr << " m_stopCalCommand: " << m_stopCalCommand.toStdString();
    }
    if (settingsKeys.contains("calCommandDelay") || force) {
        ostr << " m_calCommandDelay: " << m_calCommandDelay;
    }
    if (settingsKeys.contains("sensorMeasurePeriod") || force) {
        ostr << " m_sensorMeasurePeriod: " << m_sensorMeasurePeriod;
    }
    if (settingsKeys.contains("spectrumAutoSaveCSVFilename") || force) {
        ostr << " m_spectrumAutoSaveCSVFilename: " << m_spectrumAutoSaveCSVFilename.toStdString();
    }
    if (settingsKeys.contains("powerAutoSaveCSVFilename") || force) {
        ostr << " m_powerAutoSaveCSVFilename: " << m_powerAutoSaveCSVFilename.toStdString();
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
