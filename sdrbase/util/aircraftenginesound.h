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

#ifndef INCLUDE_AIRCRAFTENGINESOUND_H
#define INCLUDE_AIRCRAFTENGINESOUND_H

#include <QString>
#include <QRegularExpression>

// Pick the exterior engine sound the 3D map loops for an aircraft, from its ICAO type
// designator and, on ADS-B, the transmitted emitter category. A designator does not
// say what sort of aircraft it is, so this is pattern matching over the common ones -
// anything unrecognised is taken to be a jet, which is what most of the traffic is.
// Shared by the ADS-B demodulator and the Aircraft feature, like the 3D model
// matching in Aircraft3DModels.
class AircraftEngineSound
{
public:
    // Returns a directory below the Map's 3d/Sounds directory. Its engine.json
    // describes the N1/N2 OGG layers, volume and pitch curves, and spatial cones.
    // Propeller aircraft and helicopters keep their single recording, which is in
    // the Sounds/aircraft directory beside the sound sets.
    // The model is the database's model designation (e.g. "A320 232"), which for
    // Airbus can identify the engine variant.
    static QString engineSound(const QString& type, const QString& emitterCategory = QString(),
                               const QString& model = QString())
    {
        // The category is transmitted by the aircraft itself, so it wins for
        // rotorcraft - the helicopter designator list below cannot be complete
        if (emitterCategory == QLatin1String("Rotorcraft")) {
            return QStringLiteral("helicopter.wav");
        }

        const QString normalizedType = type.trimmed().toUpper();
        const QString normalizedModel = model.trimmed().toUpper();

        if (!normalizedType.isEmpty())
        {
            // Airbus jets. Deliberately not plain A..., which would take in Antonovs
            // (A124, A225) and Agustas (A109, A139).
            static const QRegularExpression airbusRe(QStringLiteral("^(A3[0-9]{2}|A(19|20|21)N)"));
            static const QRegularExpression a320neoRe(QStringLiteral("^A(19|20|21)N$"));
            static const QRegularExpression a320ceoRe(QStringLiteral("^A3(18|19|20|21)$"));
            static const QRegularExpression b737ngRe(QStringLiteral("^B73[6-9]$"));
            // Widebody Boeings - 747 and 767 use the available large-turbofan fallback.
            static const QRegularExpression widebodyBoeingRe(QStringLiteral("^B7[467]"));
            // Common helicopter designators: Robinson, Airbus Helicopters/Eurocopter,
            // Aerospatiale, Agusta/Leonardo, Sikorsky, military H-, Bell, and others
            static const QRegularExpression helicopterRe(QStringLiteral(
                "^(R(22|44|66)$|EC[0-9]|AS(32|50|55|65)|A(109|119|129|139|149|169|189)"
                "|S(61|64|76|92)$|H(47|53|60|64)$|H500|B(06|105|212|214|222|407|412|427|429|430|505)"
                "|NH90|LYNX|PUMA|TIGR|GAZL|EXPL|KA32|MI[0-9]|EH10|MD52|MD60|V22)"));
            // Common piston and turboprop designators: Cessna singles, Pipers,
            // Beechcraft, Diamond, Cirrus, ATR, Dash 8, de Havilland Canada, Pilatus,
            // TBM, Mooney, Saab, Metroliner, Jetstream, Fokker 50, Bandeirante,
            // Brasilia, Antonov, Hercules, A400M, Let 410, Dornier, Reims, Partenavia
            static const QRegularExpression propellerRe(QStringLiteral(
                "^(C1[0-9]{2}|C2[0-9]{2}|P28|P32|P46|PA3|PA4|BE[0-9]|B350|DA4|DA62|DV20"
                "|SR2|AT[47]|DH8|DHC|PC12|PC6|PC7|PC9|PC21|TBM|M20|SF34|SW[34]|JS[34]"
                "|F50$|E110|E120|AN[0-9]|C30J|A400|L410|D228|D328|F406|P68)"));

            if (a320neoRe.match(normalizedType).hasMatch()) {
                // Some neos have PW1100Gs, but LEAP is the only installed neo set.
                return QStringLiteral("A320n_CFMLeap");
            }
            if (a320ceoRe.match(normalizedType).hasMatch())
            {
                // The A320ceo family flies with either CFM56s or IAE V2500s, and the
                // model designation says which: the middle digit of the variant is
                // the engine manufacturer, 1 = CFM International, 3 = IAE - so an
                // A320-214 is a CFM56 and an A320-232 a V2500.
                static const QRegularExpression v2500Re(QStringLiteral("A3(?:18|19|20|21)\\D{0,3}[0-9]3[0-9]"));
                if (v2500Re.match(normalizedModel).hasMatch()) {
                    return QStringLiteral("A320_IAE_V2500");
                }
                return QStringLiteral("A320_CFM56");
            }
            if (normalizedType.startsWith(QLatin1String("B78"))) {
                // A GEnx/Trent distinction is not available in ordinary ICAO data.
                return QStringLiteral("B787_RR");
            }
            if (normalizedType.startsWith(QLatin1String("B77"))) {
                return QStringLiteral("B777_GE90");
            }
            if (b737ngRe.match(normalizedType).hasMatch()
                || normalizedType.startsWith(QLatin1String("B38M"))
                || normalizedType.startsWith(QLatin1String("B39M"))) {
                return QStringLiteral("B737NG_CFM56");
            }
            if (airbusRe.match(normalizedType).hasMatch()
                || widebodyBoeingRe.match(normalizedType).hasMatch()) {
                return QStringLiteral("B777_GE90");
            }
            if (helicopterRe.match(normalizedType).hasMatch()) {
                return QStringLiteral("helicopter.wav");
            }
            // The Beechjet is the one BExx that is not a propeller aircraft
            if (propellerRe.match(normalizedType).hasMatch() && !normalizedType.startsWith(QLatin1String("BE40"))) {
                return QStringLiteral("engine-prop.wav");
            }
            return emitterCategory == QLatin1String("Heavy")
                ? QStringLiteral("B777_GE90") : QStringLiteral("B737NG_CFM56");
        }

        // No type known - the emitter category is the only clue
        if ((emitterCategory == QLatin1String("Light")) || (emitterCategory == QLatin1String("Ultralight"))) {
            return QStringLiteral("engine-prop.wav");
        }
        if (!emitterCategory.isEmpty()) {
            return emitterCategory == QLatin1String("Heavy")
                ? QStringLiteral("B777_GE90") : QStringLiteral("B737NG_CFM56");
        }
        return QString();
    }
};

#endif // INCLUDE_AIRCRAFTENGINESOUND_H
