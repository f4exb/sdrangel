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

import{a as u}from"./chunk-KTSPJZ6P.js";import"./chunk-V23IN42C.js";import"./chunk-PS7DC2PQ.js";import{a as f}from"./chunk-JJVL7F22.js";import"./chunk-MQTDGHHT.js";import"./chunk-WEQVPSKS.js";import"./chunk-B7V7FH4F.js";import"./chunk-UNU6WKWY.js";import"./chunk-DV5LK4D5.js";import"./chunk-PZQGG7DO.js";import"./chunk-SIBAZPZR.js";import"./chunk-AZINMULF.js";import"./chunk-AN2UKNGO.js";import"./chunk-QQWQKL3T.js";import"./chunk-XGWWWND5.js";import"./chunk-HLXNFZAZ.js";import"./chunk-WCOEOHX5.js";import"./chunk-ZJRS2FK6.js";import"./chunk-23IOGON7.js";import"./chunk-KP72RGCH.js";import"./chunk-OSVSBQDY.js";import"./chunk-LZC73BOP.js";import"./chunk-OU2IPMYE.js";import"./chunk-H7SJWKIY.js";function a(c,d){return u.createMesh(c).then(function(e){let t=e.vertices.buffer,r=e.indices.buffer,s=e.westIndicesSouthToNorth.buffer,o=e.southIndicesEastToWest.buffer,i=e.eastIndicesNorthToSouth.buffer,n=e.northIndicesWestToEast.buffer;return d.push(t,r,s,o,i,n),{verticesBuffer:t,indicesBuffer:r,vertexCountWithoutSkirts:e.vertexCountWithoutSkirts,indexCountWithoutSkirts:e.indexCountWithoutSkirts,encoding:e.encoding,westIndicesBuffer:s,southIndicesBuffer:o,eastIndicesBuffer:i,northIndicesBuffer:n}})}var T=f(a);export{T as default};
