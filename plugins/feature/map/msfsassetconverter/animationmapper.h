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
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// What an MSFS animation does, as the model's XML says. The glTF names do not say:
// in the FSLTL A320neo the flaps are custom_anim_SIM_ON_GROUND_05 and the slats
// custom_anim_SIM_ON_GROUND_06 - the XML binds each name to a template or to code that
// reads a simulation variable, and that is the only reliable source
struct MsfsAnimationRole {
    enum Kind { Gear, Flaps, Slats, Engine, Steering };
    Kind m_kind = Gear;
    int m_engine = 0;       // Engine number, from 1, for Engine
    double m_length = 0.0;  // The animation's length in keyframes, where the XML gives it (used for engines)
    double m_neutral = 0.0; // Steering: how far through the animation the wheel points straight ahead
};

// Reads the animation roles from an MSFS model XML (the file that lists the LODs).
// Returns false only when the file can't be read or parsed; a model with nothing
// recognisable gives an empty result
bool readMsfsAnimationRoles(const QString &xmlFile, QHash<QString, MsfsAnimationRole> *roles,
                            QString *error);

// Replaces the animations the roles identify with the libxplanemp ones that the ADS-B
// demodulator plays (gear_ratio, flap_ratio, slat_ratio, engine_rotation_angle_degN),
// retimed to what it expects: 0 to 1 for a ratio, and degrees for an engine. Several
// MSFS animations with one role - three gear legs, or one split per node - become one
// animation, since Cesium plays only the first with a given name. New keyframe data is
// appended to buffers[0]. Nose wheel steering, which the demodulator doesn't play, is
// baked into the model's rest pose pointing straight ahead and the animation dropped:
// a model's rest pose is often full lock. Returns a description of each change made
QStringList mapMsfsAnimations(const QHash<QString, MsfsAnimationRole> &roles, QJsonObject *root,
                              QVector<QByteArray> *buffers);

// Removes the animations the ADS-B demodulator never plays - the spoiler and on-ground
// ones MSFS drives from simulation variables, left behind by mapMsfsAnimations because
// nothing binds them to a role. Cesium unpacks every keyframe of them into an object per
// model instance regardless. Returns what went, or an empty string if nothing did
QString dropUnplayedAnimations(QJsonObject *root);

// Thins each animation to the fewest keyframes that still reproduce it to within
// `tolerance`, measured per component: so for a rotation it is roughly radians, and for a
// translation metres. MSFS bakes an animation at every simulation frame - 100 or 200
// keyframes for a gear leg that is a plain ramp - and Cesium makes a Quaternion object of
// each one, per model instance, which is what a major GC then has to trace. New keyframe
// data is appended to buffers[0]. Returns what changed, or an empty string if nothing did
QString decimateAnimations(QJsonObject *root, QVector<QByteArray> *buffers, double tolerance);
