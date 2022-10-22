///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef _AUDIOINPUT_AUDIOINPUTSETTINGS_H_
#define _AUDIOINPUT_AUDIOINPUTSETTINGS_H_

#include <QString>
#include <QAudioDeviceInfo>

struct AudioInputSettings {

    QString m_deviceName;       // Including realm, as from getFullDeviceName below
    int m_sampleRate;
    float m_volume;
    quint32 m_log2Decim;
    enum IQMapping {
        L,
        R,
        LR,
        RL
    } m_iqMapping;

    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    AudioInputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

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
    void applySettings(const QStringList& settingsKeys, const AudioInputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* _AUDIOINPUT_AUDIOINPUTSETTINGS_H_ */
