///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef PLUGINS_SAMPLESOURCE_FILEINPUT_FILEINPUTSETTINGS_H_
#define PLUGINS_SAMPLESOURCE_FILEINPUT_FILEINPUTSETTINGS_H_

#include <QString>
#include <QByteArray>

struct FileInputSettings {
    QString m_fileName;
    quint32 m_accelerationFactor;
    bool m_loop;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    static const unsigned int m_accelerationMaxScale; //!< Max power of 10 multiplier to 2,5,10 base ex: 2 -> 2,5,10,20,50,100,200,500,1000

    FileInputSettings();
    ~FileInputSettings() {}

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const FileInputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    static int getAccelerationIndex(int averaging);
    static int getAccelerationValue(int averagingIndex);
};

#endif /* PLUGINS_SAMPLESOURCE_FILEINPUT_FILEINPUTSETTINGS_H_ */
