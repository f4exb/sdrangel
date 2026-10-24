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

#include <QJsonObject>
#include <QSet>
#include <QString>

// Parts MSFS shows only some of the time, by rules in the model XML that Cesium can't
// apply, so it would draw them all of the time:
//  - parts the XML hides outright, with a visibility code of { 0 } - FSLTL uses this for
//    the ground power and fuel hoses Asobo's aircraft come with, which otherwise float
//    above them
//  - frost, which MSFS lays over the airframe (GeoDecalFrosted materials) only in icing

// Names of the nodes the XML never shows, lower case. Empty if there are none, or the
// file can't be read
QSet<QString> readMsfsNeverVisibleNodes(const QString &xmlFile);

// Removes those nodes' meshes, with their children's, and every frost primitive. Returns
// a description of what went, or an empty string when nothing did
QString dropHiddenParts(QJsonObject *root, const QSet<QString> &neverVisible);
