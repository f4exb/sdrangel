///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef _AUDIOCATSISO_AUDIOCATSISOHAMLIB_H_
#define _AUDIOCATSISO_AUDIOCATSISOHAMLIB_H_

#include <QMap>

struct rig_caps;

class AudioCATSISOHamlib
{
public:
    AudioCATSISOHamlib();
    ~AudioCATSISOHamlib();

    const QMap<uint32_t, QString>& getRigModels() const { return m_rigModels; }
    const QMap<QString, uint32_t>& getRigNames() const { return m_rigNames; }

private:
    QMap<uint32_t, QString> m_rigModels;
    QMap<QString, uint32_t> m_rigNames;
    static int hash_model_list(const struct rig_caps *caps, void *data);
};


#endif // _AUDIOCATSISO_AUDIOCATSISOHAMLIB_H_
