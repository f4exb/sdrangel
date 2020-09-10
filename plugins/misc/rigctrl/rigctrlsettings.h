///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_RIGCTRLSETTINGS_H
#define INCLUDE_RIGCTRLSETTINGS_H

#include <QByteArray>
#include <QString>

struct RigCtrlSettings {

    bool m_enabled;
    int m_rigCtrlPort;
    int m_maxFrequencyOffset;
    int m_deviceIndex;
    int m_channelIndex;

    RigCtrlSettings()
    {
        resetToDefaults();
    };
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

private:
    static const bool m_EnabledDefault;
    static const int m_RigCtrlPortDefault;
    static const int m_MaxFrequencyOffsetDefault;
    static const int m_DeviceIndexDefault;
    static const int m_ChannelIndexDefault;    
};

#endif /* INCLUDE_RIGCTRLSETTINGS_H */
