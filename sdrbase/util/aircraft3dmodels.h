///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                      //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, GNU General Public       //
// License for more details.                                                     //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_AIRCRAFT3DMODELS_H
#define INCLUDE_AIRCRAFT3DMODELS_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QSet>
#include <QMutex>
#include <QElapsedTimer>
#include <QRegularExpression>

#include "export.h"

struct AircraftInformation;

// Matches an aircraft's database model name to an ICAO type designator
class SDRBASE_API ModelMatch {
public:
    ModelMatch(const QString &aircraftRegExp, const QString &model) :
        m_aircraftRegExp(aircraftRegExp),
        m_model(model)
    {
        m_aircraftRegExp.optimize();
    }

    virtual ~ModelMatch() = default;

    virtual bool match(const QString &aircraft, const QString &manufacturer, QString &model)
    {
        (void) manufacturer;

        QRegularExpressionMatch match = m_aircraftRegExp.match(aircraft);
        if (match.hasMatch())
        {
            model = m_model;
            return true;
        }
        else
        {
            return false;
        }
    }

protected:
    QRegularExpression m_aircraftRegExp;
    QString m_model;
};

// For very generic aircraft names, also match against manufacturer name
class SDRBASE_API ManufacturerModelMatch : public ModelMatch {
public:
    ManufacturerModelMatch(const QString &modelRegExp, const QString &manufacturerRegExp, const QString &model) :
        ModelMatch(modelRegExp, model),
        m_manufacturerRegExp(manufacturerRegExp)
    {
        m_manufacturerRegExp.optimize();
    }

    virtual bool match(const QString &aircraft, const QString &manufacturer, QString &model) override
    {
        QRegularExpressionMatch matchManufacturer = m_manufacturerRegExp.match(manufacturer);
        if (matchManufacturer.hasMatch())
        {
            QRegularExpressionMatch matchAircraft = m_aircraftRegExp.match(aircraft);
            if (matchAircraft.hasMatch())
            {
                model = m_model;
                return true;
            }
        }
        return false;
    }

protected:
    QRegularExpression m_manufacturerRegExp;
};

// Picks a 3D model for an aircraft, for the 3D map.
//
// The tables are built once, on first use, from the .gltf files in the application data
// directory - so a user without the optional 3D model pack simply gets no models, and
// everything falls back to the flat billboard icons.
class SDRBASE_API Aircraft3DModels
{
public:
    // Built on first use. Callers may be on different worker threads - the ADS-B tracker
    // and the Aircraft tracker are - so construction is guarded and everything is read
    // only afterwards.
    static const Aircraft3DModels& instance();

    // The same matching over the 2D livery icons built into the ADS-B demodulator, for
    // when the model pack isn't installed and instance() matches nothing. The icons are
    // named after the models they were rendered from, so what this picks is the icon the
    // model that would have been picked has - its "models" are icon names, A320_BAW, to
    // look up under :/map/liveries/, and never anything to send to the 3D map. Empty
    // without the ADS-B demodulator, whose resources the icons are
    static const Aircraft3DModels& liveryIcons();

    // A model for an aircraft in the OpenSky database, preferring the operator's livery.
    // False when nothing matches: no model pack, or a type with no model.
    bool modelFor(const AircraftInformation *info, bool favourLivery, bool verbose,
                  QString &model, float &altitudeOffset, float &labelOffset) const;

    // A model for a bare ICAO type designator, with a livery chosen at random
    QString modelForType(const QString &type) const;

    // A model for a type in a particular operator's livery, empty if there is not one
    QString modelForTypeAndOperator(const QString &type, const QString &operatorICAO,
                                    bool verbose = false) const;

    // Where a type's model has to sit so its undercarriage is not underground. The model
    // matters as well as the type: the two model sets put the origin in different places
    void offsetsForType(const QString &type, float &altitudeOffset, float &labelOffset,
                        const QString &model = QString()) const;

    bool isEmpty() const { return m_3DModelsByType.isEmpty(); }

    // Whether a model path, as modelFor() returns it, is one of the models installed
    // now. A model chosen in an earlier session - and kept, as the Aircraft feature
    // keeps them in its database - may since have been removed with its model pack
    bool isInstalled(const QString &model) const { return m_installedModels.contains(model); }

    // A cheaper model of the same aircraft and livery, for the 3D map to draw it with
    // when it is only a few dozen pixels across. Empty when there is none: only the
    // MSFS models converted with a second level of detail have one
    QString lowDetailModel(const QString &model) const { return m_lowDetailModels.value(model); }

    // A stand-in for an aircraft the matcher has no model for. Only a fraction of the
    // types that fly have a model, so without this a Falcon 900, a King Air or anything
    // else outside the set shows as a flat billboard next to fully modelled airliners.
    // With knownTypesOnly, false for a type it has no shape group for - so a caller that
    // has something better to go on, such as an ADS-B emitter category, can use that
    bool defaultModel(const QString &icaoType, QString &model,
                      float &altitudeOffset, float &labelOffset,
                      bool knownTypesOnly = false) const;

    static QString dataDir();

    // The 2D livery icon name for a model. The icons are named after the Bluebell
    // models, <type>_<operator>; the models converted from FSLTL/MSFS are named
    // FSLTL_<type>_<operator>_cesium, so neither the prefix nor the suffix belongs
    // in an icon name and both are taken off
    static QString liveryIconName(const QString &model);

    // Whether a model has a named animation, so an animation is only asked for from a
    // model that can play it. The models differ: 111 of the 183 A320 liveries have no
    // engine animation and 72 do, and the category stand-ins have only some of the set.
    // Read from the model's own glTF the first time a model is asked about, and
    // remembered on disk, so each model is parsed once per machine rather than once per
    // session - and never at start up, where reading 1,758 files would be felt.
    // Unknown - no such file, or unreadable - answers true: asking is harmless, as the
    // map declines what a model lacks, where wrongly suppressing is not.
    bool hasAnimation(const QString &model, const QString &animation) const;

    // Whether every texture a model refers to is installed. Some liveries in the model
    // pack name an image that was never shipped, and Cesium draws nothing for such a
    // model, so it is passed over when a model is chosen and the type's other liveries
    // are used instead. Read from the glTF alongside the animations, and remembered
    // the same way. Unknown answers true, as hasAnimation() does.
    bool hasTextures(const QString &model) const;

private:

    // What is read from a model's own glTF: only worth having once, so kept on disk
    struct ModelInfo {
        bool readable = false;      // False for a model that could not be read
        bool textures = true;
        QSet<QString> animations;
    };

    Aircraft3DModels(bool verbose, bool liveryIcons = false);
    void findModels(bool verbose);
    void findLiveryIcons();
    ~Aircraft3DModels();

    ModelInfo modelInfo(const QString &model) const;
    void loadModelInfoCache() const;
    void saveModelInfoCache() const;
    static QString modelInfoCachePath();

    // Built on demand, from whichever worker thread asks first - hence mutable, and the
    // mutex. Entries are stamped with the file's size and modification time, so a
    // replaced model pack is re-read rather than trusted.
    mutable QMutex m_modelInfoMutex;
    mutable bool m_modelInfoLoaded = false;
    mutable QHash<QString, ModelInfo> m_modelInfo;        // Model path to what its glTF says
    mutable QHash<QString, QString> m_modelInfoStamp;     // Model path to the size/mtime it was read at
    mutable QHash<QString, qint64> m_modelInfoCheckedMs;  // When each model was last stat()ed.
    mutable QElapsedTimer m_modelInfoClock;
    static constexpr qint64 m_modelInfoRecheckMs = 30000;
    mutable QSet<QString> m_unreadableModels;             // Not persisted: tried again next session

    bool m_liveryIcons;                             // Built from the icons rather than the models
    QHash<QString, QString> m_3DModels;             // "TYPE_LIVERY" to model path
    QHash<QString, QStringList> m_3DModelsByType;   // Type to every livery of it
    QHash<QString, QString> m_lowDetailModels;      // Model path to its low detail sibling
    QSet<QString> m_installedModels;                // Every model path found
    QList<ModelMatch *> m_3DModelMatch;             // Database names to type designators
    QHash<QString, float> m_modelAltitudeOffset;
    QHash<QString, float> m_msfsModelAltitudeOffset;    // For the models in MSFS/
    QHash<QString, float> m_labelAltitudeOffset;
};

#endif // INCLUDE_AIRCRAFT3DMODELS_H
