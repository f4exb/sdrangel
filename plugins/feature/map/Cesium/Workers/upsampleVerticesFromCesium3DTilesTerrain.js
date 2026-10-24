/**
 * @license
 * Cesium - https://github.com/CesiumGS/cesium
 * Version 1.145.0
 *
 * Copyright 2011-2022 Cesium Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Columbus View (Pat. Pend.)
 *
 * Portions licensed separately.
 * See https://github.com/CesiumGS/cesium/blob/main/LICENSE.md for full licensing details.
 */

import{a as f}from"./chunk-3AERAMWF.js";import"./chunk-V23IN42C.js";import"./chunk-WVRWFNLS.js";import{a as u}from"./chunk-JJVL7F22.js";import"./chunk-MQTDGHHT.js";import"./chunk-XAERMS6D.js";import"./chunk-QCE33JV5.js";import"./chunk-C5UTI7P4.js";import"./chunk-DV5LK4D5.js";import"./chunk-ZNBU4MZZ.js";import"./chunk-SIBAZPZR.js";import"./chunk-AZINMULF.js";import"./chunk-THB3EPLO.js";import"./chunk-QQWQKL3T.js";import"./chunk-YZ5ZCI55.js";import"./chunk-HLXNFZAZ.js";import"./chunk-WCOEOHX5.js";import"./chunk-ZJRS2FK6.js";import"./chunk-23IOGON7.js";import"./chunk-KP72RGCH.js";import"./chunk-OSVSBQDY.js";import"./chunk-LZC73BOP.js";import"./chunk-OU2IPMYE.js";import"./chunk-H7SJWKIY.js";function h(c,d){let e=f.upsampleMesh(c),t=e.vertices.buffer,i=e.indices.buffer,s=e.westIndicesSouthToNorth.buffer,o=e.southIndicesEastToWest.buffer,r=e.eastIndicesNorthToSouth.buffer,n=e.northIndicesWestToEast.buffer;return d.push(t,i,s,o,r,n),{verticesBuffer:t,indicesBuffer:i,vertexCountWithoutSkirts:e.vertexCountWithoutSkirts,indexCountWithoutSkirts:e.indexCountWithoutSkirts,encoding:e.encoding,westIndicesBuffer:s,southIndicesBuffer:o,eastIndicesBuffer:r,northIndicesBuffer:n,minimumHeight:e.minimumHeight,maximumHeight:e.maximumHeight,boundingSphere:e.boundingSphere3D,orientedBoundingBox:e.orientedBoundingBox,horizonOcclusionPoint:e.horizonOcclusionPoint}}var I=u(h);export{I as default};
