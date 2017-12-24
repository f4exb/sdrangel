///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_FILESOURCE_FILESOURCESETTINGS_H_
#define PLUGINS_SAMPLESOURCE_FILESOURCE_FILESOURCESETTINGS_H_

#include <QString>
#include <QByteArray>

struct FileSourceSettings {
    quint64 m_centerFrequency;
    qint32  m_sampleRate;
    QString m_fileName;

    FileSourceSettings();
    ~FileSourceSettings() {}

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* PLUGINS_SAMPLESOURCE_FILESOURCE_FILESOURCESETTINGS_H_ */
