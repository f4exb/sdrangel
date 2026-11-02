///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct ResolvedFsltlAircraft {
    QString m_gltfFile;
    QString m_modelXml;     // Model definition the glTF was chosen from, if there is one
    QString m_outputBaseName;
    QStringList m_aircraftHierarchy;
    QStringList m_textureDirectories;
    QStringList m_warnings;
};

// Returns one representative FLTSIM index for every distinct model/texture
// combination in the aircraft configuration.
bool findFsltlAircraftVariants(const QString &aircraftCfg,
                               QVector<int> *flightSimIndices,
                               QString *error);

bool resolveFsltlAircraft(const QString &aircraftCfg, int flightSimIndex,
                          int lodIndex, ResolvedFsltlAircraft *result,
                          QString *error);
