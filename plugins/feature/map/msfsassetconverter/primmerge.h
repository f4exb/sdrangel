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
#include <QVector>

// A converted aircraft draws about 26 primitives but uses only one or two materials: the
// geometry is split not because it looks different but because it hangs off a couple of
// dozen separate nodes. Cesium issues a draw command per primitive, so that is ~26 per
// aircraft to paint what is one textured surface.
//
// Merges the primitives that share a material and sit under nodes nothing animates, baking
// each node's world transform into the vertices. The result is the same pixels from fewer
// draw commands. Measured in the standalone harness with 150 copies of FSLTL_B738_RYR at
// minimumPixelSize 50, three interleaved runs with the order alternating: 4500 draw
// commands at 8.96 ms a frame became 3450 at 7.61, so 23% fewer commands gave 15% less
// frame time. Across the fleet the estimate is 25.7 -> 17.9 primitives a model.
//
// What is left alone, because baking a transform into vertices freezes it:
//  - anything an animation drives, and everything below it. gear_ratio, flap_ratio and
//    slat_ratio reach about 24 nodes a model, whose subtrees hold ~65% of the primitives,
//    which is why this takes 30% off and not 95%
//  - skins and their joints, morph targets, and non-triangle or non-indexed primitives
//  - a mesh more than one node refers to, since baking one node's transform into it would
//    move it for the other
//
// A node whose world transform mirrors (negative determinant) has its triangle winding
// reversed as it is baked: Cesium picks the cull face per draw command from that
// determinant, and once the geometry is in a shared mesh with no transform there is
// nothing left to pick it from.
//
// Run after the material and geometry passes, which look for primitives by material, and
// follow it with stripRedundantNodes() to clear the nodes this empties.
// Returns a description of what merged, or an empty string when nothing did
QString mergeStaticPrimitives(QJsonObject *root, QVector<QByteArray> *buffers);
