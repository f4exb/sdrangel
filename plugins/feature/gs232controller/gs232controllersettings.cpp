///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "gs232controllersettings.h"
#include "inputcontrollersettings.h"

const QStringList GS232ControllerSettings::m_pipeTypes = {
    QStringLiteral("ADSBDemod"),
    QStringLiteral("Map"),
    QStringLiteral("StarTracker"),
    QStringLiteral("SatelliteTracker")
};

const QStringList GS232ControllerSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.adsbdemod"),
    QStringLiteral("sdrangel.feature.map"),
    QStringLiteral("sdrangel.feature.startracker"),
    QStringLiteral("sdrangel.feature.satellitetracker")
};

GS232ControllerSettings::GS232ControllerSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void GS232ControllerSettings::resetToDefaults()
{
    m_azimuth = 0.0f;
    m_elevation = 0.0f;
    m_serialPort = "";
    m_baudRate = 9600;
    m_host = "127.0.0.1";
    m_port = 4533;
    m_track = false;
    m_source = "";
    m_azimuthOffset = 0.0f;
    m_elevationOffset = 0.0f;
    m_azimuthMin = 0;
    m_azimuthMax = 450;
    m_elevationMin = 0;
    m_elevationMax = 180;
    m_tolerance = 1.0f;
    m_protocol = GS232;
    m_connection = SERIAL;
    m_precision = 0;
    m_coordinates = AZ_EL;
    m_inputController = "None";
    m_targetControlEnabled = true;
    m_offsetControlEnabled = true;
    m_highSensitivity = true;
    m_inputControllerSettings.m_lowSensitivity = 5.0f;
    m_inputControllerSettings.m_highSensitivity = 50.0f;
    for (int i = 0; i < INPUTCONTROLLER_MAX_AXES; i++) {
        m_inputControllerSettings.m_deadzone[i] = 10.0f;
    }
    m_dfmTrackOn = false;
    m_dfmLubePumpsOn = false;
    m_dfmBrakesOn = false;
    m_dfmDrivesOn = false;
    m_title = "Rotator Controller";
    m_rgbColor = QColor(225, 25, 99).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray GS232ControllerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeFloat(1, m_azimuth);
    s.writeFloat(2, m_elevation);
    s.writeString(3, m_serialPort);
    s.writeS32(4, m_baudRate);
    s.writeBool(5, m_track);
    s.writeString(6, m_source);
    s.writeString(8, m_title);
    s.writeU32(9, m_rgbColor);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIFeatureSetIndex);
    s.writeU32(14, m_reverseAPIFeatureIndex);
    s.writeFloat(15, m_azimuthOffset);
    s.writeFloat(16, m_elevationOffset);
    s.writeS32(17, m_azimuthMin);
    s.writeS32(18, m_azimuthMax);
    s.writeS32(19, m_elevationMin);
    s.writeS32(20, m_elevationMax);
    s.writeFloat(21, m_tolerance);
    s.writeS32(22, (int)m_protocol);
    s.writeS32(23, (int)m_connection);
    s.writeString(24, m_host);
    s.writeS32(25, m_port);

    if (m_rollupState) {
        s.writeBlob(26, m_rollupState->serialize());
    }

    s.writeS32(27, m_workspaceIndex);
    s.writeBlob(28, m_geometryBytes);
    s.writeS32(29, m_precision);
    s.writeS32(30, (int)m_coordinates);
    s.writeBool(31, m_dfmTrackOn);
    s.writeBool(32, m_dfmLubePumpsOn);
    s.writeBool(33, m_dfmBrakesOn);
    s.writeBool(34, m_dfmDrivesOn);
    s.writeString(35, m_inputController);
    s.writeBool(37, m_targetControlEnabled);
    s.writeBool(38, m_offsetControlEnabled);
    s.writeBool(39, m_highSensitivity);

    s.writeFloat(50, m_inputControllerSettings.m_lowSensitivity);
    s.writeFloat(51, m_inputControllerSettings.m_highSensitivity);
    for (int i = 0; i < INPUTCONTROLLER_MAX_AXES; i++) {
        s.writeFloat(60+i, m_inputControllerSettings.m_deadzone[i]);
    }

    return s.final();
}

bool GS232ControllerSettings::deserialize(const QByteArray& data)
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

        d.readFloat(1, &m_azimuth, 0);
        d.readFloat(2, &m_elevation, 0);
        d.readString(3, &m_serialPort, "");
        d.readS32(4, &m_baudRate, 9600);
        d.readBool(5, &m_track, false);
        d.readString(6, &m_source, "");
        d.readString(8, &m_title, "Rotator Controller");
        d.readU32(9, &m_rgbColor, QColor(225, 25, 99).rgb());
        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(13, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(14, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readFloat(15, &m_azimuthOffset, 0.0f);
        d.readFloat(16, &m_elevationOffset, 0.0f);
        d.readS32(17, &m_azimuthMin, 0);
        d.readS32(18, &m_azimuthMax, 450);
        d.readS32(19, &m_elevationMin, 0);
        d.readS32(20, &m_elevationMax, 180);
        d.readFloat(21, &m_tolerance, 1.0f);
        d.readS32(22, (int*)&m_protocol, GS232);
        d.readS32(23, (int*)&m_connection, SERIAL);
        d.readString(24, &m_host, "127.0.0.1");
        d.readS32(25, &m_port, 4533);

        if (m_rollupState)
        {
            d.readBlob(26, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(27, &m_workspaceIndex, 0);
        d.readBlob(28, &m_geometryBytes);
        d.readS32(29, &m_precision, 0);
        d.readS32(30, (int *)&m_coordinates, (int)AZ_EL);
        d.readBool(31, &m_dfmTrackOn);
        d.readBool(32, &m_dfmLubePumpsOn);
        d.readBool(33, &m_dfmBrakesOn);
        d.readBool(34, &m_dfmDrivesOn);
        d.readString(35, &m_inputController, "None");
        d.readBool(37, &m_targetControlEnabled, true);
        d.readBool(38, &m_offsetControlEnabled, true);
        d.readBool(39, &m_highSensitivity, true);

        d.readFloat(50, &m_inputControllerSettings.m_lowSensitivity, 5.0f);
        d.readFloat(51, &m_inputControllerSettings.m_highSensitivity, 50.0f);
        for (int i = 0; i < INPUTCONTROLLER_MAX_AXES; i++) {
            d.readFloat(60+i, &m_inputControllerSettings.m_deadzone[i], 10.0f);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void GS232ControllerSettings::calcTargetAzEl(float& targetAz, float& targetEl) const
{
    // Apply offset then clamp

    targetAz = m_azimuth;
    targetAz += m_azimuthOffset;
    targetAz = std::max(targetAz, (float)m_azimuthMin);
    targetAz = std::min(targetAz, (float)m_azimuthMax);

    targetEl = m_elevation;
    targetEl += m_elevationOffset;
    targetEl = std::max(targetEl, (float)m_elevationMin);
    targetEl = std::min(targetEl, (float)m_elevationMax);
}

void GS232ControllerSettings::applySettings(const QStringList& settingsKeys, const GS232ControllerSettings& settings)
{
    if (settingsKeys.contains("azimuth")) {
        m_azimuth = settings.m_azimuth;
    }
    if (settingsKeys.contains("elevation")) {
        m_elevation = settings.m_elevation;
    }
    if (settingsKeys.contains("serialPort")) {
        m_serialPort = settings.m_serialPort;
    }
    if (settingsKeys.contains("baudRate")) {
        m_baudRate = settings.m_baudRate;
    }
    if (settingsKeys.contains("host")) {
        m_host = settings.m_host;
    }
    if (settingsKeys.contains("port")) {
        m_port = settings.m_port;
    }
    if (settingsKeys.contains("track")) {
        m_track = settings.m_track;
    }
    if (settingsKeys.contains("source")) {
        m_source = settings.m_source;
    }
    if (settingsKeys.contains("azimuthOffset")) {
        m_azimuthOffset = settings.m_azimuthOffset;
    }
    if (settingsKeys.contains("elevationOffset")) {
        m_elevationOffset = settings.m_elevationOffset;
    }
    if (settingsKeys.contains("azimuthMin")) {
        m_azimuthMin = settings.m_azimuthMin;
    }
    if (settingsKeys.contains("azimuthMax")) {
        m_azimuthMax = settings.m_azimuthMax;
    }
    if (settingsKeys.contains("elevationMin")) {
        m_elevationMin = settings.m_elevationMin;
    }
    if (settingsKeys.contains("elevationMax")) {
        m_elevationMax = settings.m_elevationMax;
    }
    if (settingsKeys.contains("tolerance")) {
        m_tolerance = settings.m_tolerance;
    }
    if (settingsKeys.contains("protocol")) {
        m_protocol = settings.m_protocol;
    }
    if (settingsKeys.contains("connection")) {
        m_connection = settings.m_connection;
    }
    if (settingsKeys.contains("precision")) {
        m_precision = settings.m_precision;
    }
    if (settingsKeys.contains("coordinates")) {
        m_coordinates = settings.m_coordinates;
    }
    if (settingsKeys.contains("inputController")) {
        m_inputController = settings.m_inputController;
    }
    if (settingsKeys.contains("inputControllerSettings")) {
        m_inputControllerSettings = settings.m_inputControllerSettings;
    }
    if (settingsKeys.contains("targetControlEnabled")) {
        m_targetControlEnabled = settings.m_targetControlEnabled;
    }
    if (settingsKeys.contains("offsetControlEnabled")) {
        m_offsetControlEnabled = settings.m_offsetControlEnabled;
    }
    if (settingsKeys.contains("highSensitivity")) {
        m_highSensitivity = settings.m_highSensitivity;
    }
    if (settingsKeys.contains("dfmTrackOn")) {
        m_dfmTrackOn = settings.m_dfmTrackOn;
    }
    if (settingsKeys.contains("dfmLubePumpsOn")) {
        m_dfmLubePumpsOn = settings.m_dfmLubePumpsOn;
    }
    if (settingsKeys.contains("dfmBrakesOn")) {
        m_dfmBrakesOn = settings.m_dfmBrakesOn;
    }
    if (settingsKeys.contains("dfmDrivesOn")) {
        m_dfmDrivesOn = settings.m_dfmDrivesOn;
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

QString GS232ControllerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("azimuth") || force) {
        ostr << " m_azimuth: " << m_azimuth;
    }
    if (settingsKeys.contains("elevation") || force) {
        ostr << " m_elevation: " << m_elevation;
    }
    if (settingsKeys.contains("serialPort") || force) {
        ostr << " m_serialPort: " << m_serialPort.toStdString();
    }
    if (settingsKeys.contains("baudRate") || force) {
        ostr << " m_baudRate: " << m_baudRate;
    }
    if (settingsKeys.contains("host") || force) {
        ostr << " m_host: " << m_host.toStdString();
    }
    if (settingsKeys.contains("port") || force) {
        ostr << " m_port: " << m_port;
    }
    if (settingsKeys.contains("track") || force) {
        ostr << " m_track: " << m_track;
    }
    if (settingsKeys.contains("source") || force) {
        ostr << " m_source: " << m_source.toStdString();
    }
    if (settingsKeys.contains("azimuthOffset") || force) {
        ostr << " m_azimuthOffset: " << m_azimuthOffset;
    }
    if (settingsKeys.contains("elevationOffset") || force) {
        ostr << " m_elevationOffset: " << m_elevationOffset;
    }
    if (settingsKeys.contains("azimuthMin") || force) {
        ostr << " m_azimuthMin: " << m_azimuthMin;
    }
    if (settingsKeys.contains("azimuthMax") || force) {
        ostr << " m_azimuthMax: " << m_azimuthMax;
    }
    if (settingsKeys.contains("elevationMin") || force) {
        ostr << " m_elevationMin: " << m_elevationMin;
    }
    if (settingsKeys.contains("elevationMax") || force) {
        ostr << " m_elevationMax: " << m_elevationMax;
    }
    if (settingsKeys.contains("tolerance") || force) {
        ostr << " m_tolerance: " << m_tolerance;
    }
    if (settingsKeys.contains("protocol") || force) {
        ostr << " m_protocol: " << m_protocol;
    }
    if (settingsKeys.contains("connection") || force) {
        ostr << " m_connection: " << m_connection;
    }
    if (settingsKeys.contains("precision") || force) {
        ostr << " m_precision: " << m_precision;
    }
    if (settingsKeys.contains("coordinates") || force) {
        ostr << " m_coordinates: " << m_precision;
    }
    if (settingsKeys.contains("inputController") || force) {
        ostr << " m_inputController: " << m_inputController.toStdString();
    }
    if (settingsKeys.contains("inputControllerSettings") || force) {
        ostr << " m_inputControllerSettings.m_lowSensitivity: " << m_inputControllerSettings.m_lowSensitivity;
        ostr << " m_inputControllerSettings.m_highSensitivity: " << m_inputControllerSettings.m_highSensitivity;
        for (int i = 0; i < INPUTCONTROLLER_MAX_AXES; i++) {
            ostr << " m_inputControllerSettings.m_deadzone: " << m_inputControllerSettings.m_deadzone[i];
        }
    }
    if (settingsKeys.contains("targetControlEnabled") || force) {
        ostr << " m_targetControlEnabled: " << m_targetControlEnabled;
    }
    if (settingsKeys.contains("offsetControlEnabled") || force) {
        ostr << " m_offsetControlEnabled: " << m_offsetControlEnabled;
    }
    if (settingsKeys.contains("highSensitivity") || force) {
        ostr << " m_highSensitivity: " << m_highSensitivity;
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
    if (settingsKeys.contains("azimuth") || force) {
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
