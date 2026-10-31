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

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

// Several FSLTL airliners - the A380, A350, 747-8, 767, 777s, 787-10, A220 and Saab 340,
// from different authors - have the cabin windows on their right (-x) side set into the
// fuselage, by 0.17 m on the 767 up to 0.6 m on the A380, so that side is drawn with no
// windows at all. Where the windows' extent across the fuselage's centre plane is
// lopsided, this drops the inset side's window triangles and puts a mirror image of the
// other side's in their place. New vertex data is appended to buffers[0]. Returns a
// description of what was done, or an empty string when the windows were symmetric
QString mirrorInsetPortholes(QJsonObject *root, QVector<QByteArray> *buffers);
