///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

// Drops vertex attributes nothing reads. Three of them, measured over random samples of the
// converted library:
//
//  - TEXCOORD_1 is present in every model and referenced by no material in any of them.
//  - COLOR_0 is present in every model and pure white in 1,666 of 1,673 accessors - stored
//    as VEC4 float, so sixteen bytes a vertex to say "multiply by one".
//  - TANGENT is present on every primitive and read by 4.1% of them. glTF needs a tangent
//    only where the material samples a normal map, and only 769 of 18,586 primitives have
//    such a material.
//
// The first two were a third of all vertex attribute bytes (COLOR_0 22.2%, TEXCOORD_1 11.1%,
// against POSITION's 16.7%). With those gone TANGENT is 33.2% of what is left, of which
// 95.9% is dead - so about another third.
//
// That matters because the 3D map is memory bandwidth bound rather than short of CPU - an
// unrelated measurement had a memory streaming load cost it 275% of a frame while an equal
// amount of pure computation on its own cores cost 48% - so vertex bytes it never reads are
// paid for on the bottleneck, every frame, for every aircraft drawn.
//
// It also cuts the number of shader programs. Cesium keys a model's shader on which
// attributes the primitive has, and compiles a family of six derived variants (base,
// logDepth, pick, depthOnly, HDR, translucent/MRT) for each distinct set. Removing
// HAS_COLOR_0 and HAS_TEXCOORD_1 merges what were separate families, and a model arriving
// with a family that is not cached compiles it on a visible frame: those frames measured
// 29.5 ms median against 8.7, with 37% of them over 33 ms.
//
// Only provably dead data is removed. A TEXCOORD set goes only when no material references
// it; COLOR_n goes only when every element really is opaque white, read from the buffer
// rather than assumed, because glTF multiplies COLOR_n into the base colour unconditionally;
// and TANGENT goes only from a primitive whose own material has no normal map of any kind.
// The decision has to be per primitive rather than per model: 278 of 300 models have at
// least one material that genuinely needs tangents.
//
// Cesium agrees with the glTF rule - every read of attributes.tangentEC in its model shader
// sits inside HAS_NORMAL_TEXTURE or HAS_CLEARCOAT_NORMAL_TEXTURE, and each has an else
// branch deriving a tangent frame from screen space derivatives when there are no
// bitangents. So the cost of being wrong here is a slightly different normal on a surface
// that samples a normal map, not broken geometry.
//
// The bytes stay in the .bin: this removes the references, and the output buffer is written
// as it stands. Cesium uploads a bufferView only when an attribute references it, so the GPU
// memory, the upload and the per-frame vertex fetch are all saved - but the file keeps its
// size until a compaction pass drops the orphaned accessors and bufferViews.
QString stripUnusedAttributes(QJsonObject *root, const QVector<QByteArray> &buffers);
