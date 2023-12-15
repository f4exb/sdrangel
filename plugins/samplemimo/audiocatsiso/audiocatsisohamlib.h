///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2016, 2018-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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
#include <hamlib/rig.h>

#ifdef RIGCAPS_NOT_CONST

  /* Since this commit:
   *   https://github.com/Hamlib/Hamlib/commit/ed941939359da9f8734dbdf4a21a9b01622a1a6e
   * a 'struct rig_caps' is no longer constant (as passed to 'rig_list_foreach()' etc.).
   */

  #define HAMLIB_RIG_CAPS struct rig_caps
#else
  #define HAMLIB_RIG_CAPS const struct rig_caps
#endif

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
    static int hash_model_list(HAMLIB_RIG_CAPS *caps, void *data);
};


#endif // _AUDIOCATSISO_AUDIOCATSISOHAMLIB_H_
