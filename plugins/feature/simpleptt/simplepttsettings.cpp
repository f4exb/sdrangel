///////////////////////////////////////////////////////////////////////////////////
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
#include "audio/audiodevicemanager.h"

#include "simplepttsettings.h"

SimplePTTSettings::SimplePTTSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void SimplePTTSettings::resetToDefaults()
{
    m_title = "Simple PTT";
    m_rgbColor = QColor(255, 0, 0).rgb();
    m_rxDeviceSetIndex = -1;
    m_txDeviceSetIndex = -1;
    m_rx2TxDelayMs = 100;
    m_tx2RxDelayMs = 100;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_voxLevel = -20;
    m_voxHold = 500;
    m_vox = false;
    m_voxEnable = false;
    m_gpioControl = GPIONone;
    m_rx2txCommandEnable = false;
    m_rx2txGPIOMask = 0;
    m_rx2txGPIOValues = 0;
    m_rx2txCommandEnable = false;
    m_rx2txCommand = "";
    m_tx2rxGPIOEnable = false;
    m_tx2rxGPIOMask = 0;
    m_tx2rxGPIOValues = 0;
    m_tx2rxCommandEnable = false;
    m_tx2rxCommand = "";
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray SimplePTTSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_title);
    s.writeU32(2, m_rgbColor);
    s.writeS32(3, m_rxDeviceSetIndex);
    s.writeS32(4, m_txDeviceSetIndex);
    s.writeU32(5, m_rx2TxDelayMs);
    s.writeU32(6, m_tx2RxDelayMs);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIFeatureSetIndex);
    s.writeU32(11, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(12, m_rollupState->serialize());
    }

    s.writeString(13, m_audioDeviceName);
    s.writeS32(14, m_voxLevel);
    s.writeBool(15, m_vox);
    s.writeBool(16, m_voxEnable);
    s.writeS32(17, m_voxHold);
    s.writeS32(18, m_workspaceIndex);
    s.writeBlob(19, m_geometryBytes);
    s.writeS32(20, (int) m_gpioControl);
    s.writeBool(21, m_rx2txGPIOEnable);
    s.writeS32(22, m_rx2txGPIOMask);
    s.writeS32(23, m_rx2txGPIOValues);
    s.writeBool(24, m_rx2txCommandEnable);
    s.writeString(25, m_rx2txCommand);
    s.writeBool(26, m_tx2rxGPIOEnable);
    s.writeS32(27, m_tx2rxGPIOMask);
    s.writeS32(28, m_tx2rxGPIOValues);
    s.writeBool(29, m_tx2rxCommandEnable);
    s.writeString(30, m_tx2rxCommand);

    return s.final();
}

bool SimplePTTSettings::deserialize(const QByteArray& data)
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
        int32_t tmp;

        d.readString(1, &m_title, "Simple PTT");
        d.readU32(2, &m_rgbColor, QColor(255, 0, 0).rgb());
        d.readS32(3, &m_rxDeviceSetIndex, -1);
        d.readS32(4, &m_txDeviceSetIndex, -1);
        d.readU32(5, &m_rx2TxDelayMs, 100);
        d.readU32(6, &m_tx2RxDelayMs, 100);
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(11, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(12, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readString(13, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(14, &m_voxLevel, -20);
        d.readBool(15, &m_vox, false);
        d.readBool(16, &m_voxEnable, false);
        d.readS32(17, &m_voxHold, 500);
        d.readS32(18, &m_workspaceIndex, 0);
        d.readBlob(19, &m_geometryBytes);
        d.readS32(20, &tmp, 0);
        m_gpioControl = (GPIOControl) tmp;
        d.readBool(21, &m_rx2txGPIOEnable, false);
        d.readS32(22, &m_rx2txGPIOMask, 0);
        d.readS32(23, &m_rx2txGPIOValues, 0);
        d.readBool(24, &m_rx2txCommandEnable, false);
        d.readString(25, &m_rx2txCommand, "");
        d.readBool(26, &m_tx2rxGPIOEnable, false);
        d.readS32(27, &m_tx2rxGPIOMask, 0);
        d.readS32(28, &m_tx2rxGPIOValues, 0);
        d.readBool(29, &m_tx2rxCommandEnable, false);
        d.readString(30, &m_tx2rxCommand, "");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void SimplePTTSettings::applySettings(const QStringList& settingsKeys, const SimplePTTSettings& settings)
{
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("rxDeviceSetIndex")) {
        m_rxDeviceSetIndex = settings.m_rxDeviceSetIndex;
    }
    if (settingsKeys.contains("txDeviceSetIndex")) {
        m_txDeviceSetIndex = settings.m_txDeviceSetIndex;
    }
    if (settingsKeys.contains("rx2TxDelayMs")) {
        m_rx2TxDelayMs = settings.m_rx2TxDelayMs;
    }
    if (settingsKeys.contains("tx2RxDelayMs")) {
        m_tx2RxDelayMs = settings.m_tx2RxDelayMs;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("voxLevel")) {
        m_voxLevel = settings.m_voxLevel;
    }
    if (settingsKeys.contains("voxHold")) {
        m_voxHold = settings.m_voxHold;
    }
    if (settingsKeys.contains("vox")) {
        m_vox = settings.m_vox;
    }
    if (settingsKeys.contains("voxEnable")) {
        m_voxEnable = settings.m_voxEnable;
    }
    if (settingsKeys.contains("gpioControl")) {
        m_gpioControl = settings.m_gpioControl;
    }
    if (settingsKeys.contains("rx2txGPIOMask")) {
        m_rx2txGPIOMask = settings.m_rx2txGPIOMask;
    }
    if (settingsKeys.contains("rx2txGPIOValues")) {
        m_rx2txGPIOValues = settings.m_rx2txGPIOValues;
    }
    if (settingsKeys.contains("rx2txCommand")) {
        m_rx2txCommand = settings.m_rx2txCommand;
    }
    if (settingsKeys.contains("tx2rxGPIOMask")) {
        m_tx2rxGPIOMask = settings.m_tx2rxGPIOMask;
    }
    if (settingsKeys.contains("tx2rxGPIOValues")) {
        m_tx2rxGPIOValues = settings.m_tx2rxGPIOValues;
    }
    if (settingsKeys.contains("tx2rxCommand")) {
        m_tx2rxCommand = settings.m_tx2rxCommand;
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

QString SimplePTTSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("rxDeviceSetIndex") || force) {
        ostr << " m_rxDeviceSetIndex: " << m_rxDeviceSetIndex;
    }
    if (settingsKeys.contains("txDeviceSetIndex") || force) {
        ostr << " m_txDeviceSetIndex: " << m_txDeviceSetIndex;
    }
    if (settingsKeys.contains("rx2TxDelayMs") || force) {
        ostr << " m_rx2TxDelayMs: " << m_rx2TxDelayMs;
    }
    if (settingsKeys.contains("tx2RxDelayMs") || force) {
        ostr << " m_tx2RxDelayMs: " << m_tx2RxDelayMs;
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        ostr << " m_audioDeviceName: " << m_audioDeviceName.toStdString();
    }
    if (settingsKeys.contains("voxLevel") || force) {
        ostr << " m_voxLevel: " << m_voxLevel;
    }
    if (settingsKeys.contains("gpioControl") || force) {
        ostr << " m_gpioControl: " << m_gpioControl;
    }
    if (settingsKeys.contains("rx2txGPIOMask") || force) {
        ostr << " m_rx2txGPIOMask: " << m_rx2txGPIOMask;
    }
    if (settingsKeys.contains("rx2txGPIOValues") || force) {
        ostr << " m_rx2txGPIOValues: " << m_rx2txGPIOValues;
    }
    if (settingsKeys.contains("rx2txCommand") || force) {
        ostr << " m_rx2txCommand: " << m_rx2txCommand.toStdString();
    }
    if (settingsKeys.contains("tx2rxGPIOMask") || force) {
        ostr << " m_tx2rxGPIOMask: " << m_tx2rxGPIOMask;
    }
    if (settingsKeys.contains("tx2rxGPIOValues") || force) {
        ostr << " m_tx2rxGPIOValues: " << m_tx2rxGPIOValues;
    }
    if (settingsKeys.contains("tx2rxCommand") || force) {
        ostr << " m_tx2rxCommand: " << m_tx2rxCommand.toStdString();
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        ostr << " m_reverseAPIFeatureSetIndex: " << m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_reverseAPIFeatureIndex: " << m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }

    return QString(ostr.str().c_str());
}
