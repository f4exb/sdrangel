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

#ifndef _AUDIOOUTPUT_AUDIOOUTPUTSETTINGS_H_
#define _AUDIOOUTPUT_AUDIOOUTPUTSETTINGS_H_

#include <QString>
#include <QAudioDeviceInfo>

struct AudioOutputSettings {

    QString m_deviceName;       // Including realm, as from getFullDeviceName below
    float m_volume;
    enum IQMapping {
        LR,
        RL
    } m_iqMapping;

    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    AudioOutputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const AudioOutputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    // Append realm to device names, because there may be multiple devices with the same name on Windows
    static QString getFullDeviceName(const QAudioDeviceInfo &deviceInfo)
    {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
        return deviceInfo.deviceName();
#else
        QString realm = deviceInfo.realm();
        if (realm != "" && realm != "default" && realm != "alsa")
            return deviceInfo.deviceName() + " " + realm;
        else
            return deviceInfo.deviceName();
#endif
    }
};

#endif /* _AUDIOOUTPUT_AUDIOOUTPUTSETTINGS_H_ */
