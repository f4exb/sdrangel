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
#include <QString>

// A converted MSFS aircraft arrives with about 106 glTF nodes but only 22 of them carry
// geometry: the rest are the empty transforms and one-child chains the exporter leaves
// behind, and dropHiddenParts() makes more of them by taking the mesh off a node it hides.
// Cesium builds a ModelRuntimeNode for every one and walks them all every frame, so with a
// couple of hundred aircraft that is tens of thousands of nodes a frame doing nothing.
// Measured in map3dperf with 150 copies of one model, identical geometry and identical draw
// commands either way: 161 nodes an instance ran at 44.9 fps, 28 at 57.8
//
// Removes the nodes nothing needs and folds each remaining pass-through transform into the
// child it ends at, so the geometry lands in exactly the same place. A node is kept when it
// carries a mesh, a camera or anything else beyond a name, children and a transform, when an
// animation drives it, or when a skin uses it as a joint. A pass-through whose child is
// animated is kept too: animation overwrites that child's translation/rotation/scale, which
// would throw away a transform folded into it.
//
// Run after dropHiddenParts() and the animation passes, so it sees the nodes they leave.
// Returns a description of what went, or an empty string when nothing did
QString stripRedundantNodes(QJsonObject *root);
