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

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTimeZone>
#include <QSaveFile>
#include <QDebug>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutexLocker>
#include <QtEndian>

#include "util/aircraft3dmodels.h"
#include "util/osndb.h"

// Whether the one-off table build logs what it matched. Read from the environment
// rather than from settings because the tables are shared by every plugin and built
// once, before any of their settings exist.
static bool verboseFromEnvironment()
{
    return qEnvironmentVariableIsSet("SDRANGEL_VERBOSE_MODEL_MATCHING");
}

const Aircraft3DModels& Aircraft3DModels::instance()
{
    // Function local static: the C++11 guarantee that this is constructed exactly once,
    // even when two worker threads arrive together. That matters here - the ADS-B tracker
    // and the Aircraft tracker each run on their own thread and both want these tables.
    static Aircraft3DModels models(verboseFromEnvironment());
    return models;
}

const Aircraft3DModels& Aircraft3DModels::liveryIcons()
{
    static Aircraft3DModels icons(verboseFromEnvironment(), true);
    return icons;
}

QString Aircraft3DModels::modelInfoCachePath()
{
    return dataDir() + "/3d_model_info.json";
}

// A glTF's JSON: the whole file for .gltf, the JSON chunk of a binary .glb
static QJsonObject gltfJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    QByteArray bytes = file.readAll();
    if (bytes.startsWith("glTF"))
    {
        // A 12 byte header (magic, version, length), then chunks of length, type, data;
        // the first chunk is the JSON
        if (bytes.size() < 20) {
            return QJsonObject();
        }
        const quint32 length = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + 12));
        if (20 + length > (quint32) bytes.size()) {
            return QJsonObject();
        }
        bytes = bytes.mid(20, length);
    }
    return QJsonDocument::fromJson(bytes).object();
}

bool Aircraft3DModels::hasAnimation(const QString &model, const QString &animation) const
{
    if (model.isEmpty() || m_liveryIcons) {
        return true;
    }
    const ModelInfo info = modelInfo(model);
    return !info.readable || info.animations.contains(animation);
}

bool Aircraft3DModels::hasTextures(const QString &model) const
{
    // An icon has no glTF to read, and is always complete
    if (model.isEmpty() || m_liveryIcons) {
        return true;
    }
    const ModelInfo info = modelInfo(model);
    return !info.readable || info.textures;
}

Aircraft3DModels::ModelInfo Aircraft3DModels::modelInfo(const QString &model) const
{
    QMutexLocker locker(&m_modelInfoMutex);

    if (!m_modelInfoLoaded)
    {
        loadModelInfoCache();
        m_modelInfoLoaded = true;
    }
    if (m_unreadableModels.contains(model)) {
        return ModelInfo();
    }

    const QString path = dataDir() + "/3d/" + model;

    // Answer from the cache without touching the disk, unless it is time to look again.
    // This runs per aircraft per update, and a stat plus a local-time QDateTime
    // conversion each time is not free: QDateTime takes a process-wide lock to reach the
    // C library's time functions, so every thread doing anything with local time queues
    // behind these calls
    if (!m_modelInfoClock.isValid()) {
        m_modelInfoClock.start();
    }
    const qint64 nowMs = m_modelInfoClock.elapsed();
    const qint64 checkedMs = m_modelInfoCheckedMs.value(model, -1);
    auto cached = m_modelInfo.constFind(model);
    if ((cached != m_modelInfo.constEnd()) && (checkedMs >= 0)
        && ((nowMs - checkedMs) < m_modelInfoRecheckMs)) {
        return cached.value();
    }

    const QFileInfo fileInfo(path);
    // UTC, so the conversion does not go through the shared local-time machinery.
    // The QTimeZone overload only exists from Qt 6.6
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    const QString stamp = QString("%1/%2").arg(fileInfo.size())
                              .arg(fileInfo.lastModified(QTimeZone::UTC).toSecsSinceEpoch());
#else
    const QString stamp = QString("%1/%2").arg(fileInfo.size())
                              .arg(fileInfo.lastModified().toSecsSinceEpoch());
#endif
    m_modelInfoCheckedMs.insert(model, nowMs);

    auto it = m_modelInfo.constFind(model);
    if ((it != m_modelInfo.constEnd()) && (m_modelInfoStamp.value(model) == stamp)) {
        return it.value();
    }

    // Not seen before, or the file has changed since it was
    if (!fileInfo.exists())
    {
        m_unreadableModels.insert(model);
        return ModelInfo();
    }
    const QJsonObject gltf = gltfJson(path);
    if (gltf.isEmpty())
    {
        qWarning() << "Aircraft3DModels: Can't read" << path;
        m_unreadableModels.insert(model);
        return ModelInfo();
    }
    ModelInfo info;
    info.readable = true;
    for (const QJsonValue &value : gltf.value("animations").toArray())
    {
        const QString name = value.toObject().value("name").toString();
        if (!name.isEmpty()) {
            info.animations.insert(name);
        }
    }
    // Textures are referred to by a URI relative to the glTF, unless embedded in it,
    // as a data URI or in a buffer, where there is no uri at all
    const QDir modelDir = fileInfo.dir();
    for (const QJsonValue &value : gltf.value("images").toArray())
    {
        const QString uri = value.toObject().value("uri").toString();
        if (uri.isEmpty() || uri.startsWith("data:")) {
            continue;
        }
        if (!modelDir.exists(QUrl::fromPercentEncoding(uri.toUtf8())))
        {
            qWarning() << "Aircraft3DModels:" << model << "refers to a texture that is not installed:" << uri << "- not used";
            info.textures = false;
        }
    }
    m_modelInfo.insert(model, info);
    m_modelInfoStamp.insert(model, stamp);
    saveModelInfoCache();
    return info;
}

void Aircraft3DModels::loadModelInfoCache() const
{
    QFile file(modelInfoCachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;     // The first run on this machine
    }
    const QJsonObject cache = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = cache.constBegin(); it != cache.constEnd(); ++it)
    {
        const QJsonObject entry = it.value().toObject();
        ModelInfo info;
        info.readable = true;
        info.textures = entry.value("textures").toBool(true);
        for (const QJsonValue &value : entry.value("animations").toArray()) {
            info.animations.insert(value.toString());
        }
        m_modelInfo.insert(it.key(), info);
        m_modelInfoStamp.insert(it.key(), entry.value("stamp").toString());
    }
}

void Aircraft3DModels::saveModelInfoCache() const
{
    QJsonObject cache;
    for (auto it = m_modelInfo.constBegin(); it != m_modelInfo.constEnd(); ++it)
    {
        QJsonArray names;
        for (const QString &name : it.value().animations) {
            names.append(name);
        }
        cache.insert(it.key(), QJsonObject {
            {"stamp", m_modelInfoStamp.value(it.key())},
            {"textures", it.value().textures},
            {"animations", names}
        });
    }
    // Written whole and swapped in, so a crash mid-write cannot leave a half a cache
    QSaveFile file(modelInfoCachePath());
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(cache).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

QString Aircraft3DModels::dataDir()
{
    // Where the aircraft and airport databases and the 3D models live
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    return locations[0];
}

QString Aircraft3DModels::liveryIconName(const QString &model)
{
    QString name = QFileInfo(model).completeBaseName();
    static const QString prefix = QStringLiteral("FSLTL_");
    static const QString lowDetail = QStringLiteral("_cesium_lod1");
    static const QString suffix = QStringLiteral("_cesium");
    if (name.startsWith(prefix, Qt::CaseInsensitive)) {
        name = name.mid(prefix.size());
    }
    if (name.endsWith(lowDetail, Qt::CaseInsensitive)) {
        name.chop(lowDetail.size());
    } else if (name.endsWith(suffix, Qt::CaseInsensitive)) {
        name.chop(suffix.size());
    }
    return name;
}

Aircraft3DModels::~Aircraft3DModels()
{
    qDeleteAll(m_3DModelMatch);
}

// The ICAO type and operator a model converted from FSLTL/MSFS is named after, with the
// FSLTL_ prefix and _cesium suffix already off: A20N_AAN, Asobo_208B_ACN, B744F_CLX. The
// operator is empty for a model of the type in no particular livery. Variant is set for a
// special livery of the operator's, named with something after it: A319_BAW_BEA, BA's
// retro BEA scheme, beside A319_BAW, its everyday one. False for a name that doesn't read
// as a type
static bool typeAndOperator(const QString &name, QString &type, QString &operatorICAO, bool &variant)
{
    const QStringList parts = name.split('_');

    int typeIndex = 0;
    if ((parts[typeIndex].compare("Asobo", Qt::CaseInsensitive) == 0) && (parts.size() > 1)) {
        ++typeIndex;
    }
    type = parts[typeIndex].toUpper();
    // The Asobo aircraft FSLTL reuses are named after the product, not the designator
    static const QHash<QString, QString> asoboTypes = {
        {"208B", "C208"}, {"TBM930", "TBM9"}, {"C172SP", "C172"}, {"G36", "BE36"}, {"DR400", "DR40"}
    };
    type = asoboTypes.value(type, type);
    if ((type.size() == 5) && type.endsWith('F')) {
        type.chop(1); // FSLTL freighter suffix; the ICAO type remains four characters
    }
    if ((type.size() < 3) || (type.size() > 4)) {
        return false;
    }

    operatorICAO.clear();
    variant = false;
    if (parts.size() > typeIndex + 1)
    {
        const QString candidate = parts[typeIndex + 1].toUpper();
        if (candidate.size() == 3)
        {
            operatorICAO = candidate;
            variant = parts.size() > typeIndex + 2;
        }
    }
    return true;
}

// Only one model can stand for an operator's livery on a type, and where the set has
// the operator's plain livery that is the one: a variant only stands in where there is
// none. Without this the last found won, which for BA's A319 was the retro BEA scheme,
// on every BA A319 - the scan finds it after the everyday one. Otherwise a model still
// replaces what was found before it, as the FSLTL models replace the Bluebell ones: a
// variant is as often the everyday fleet (A320_BAW_IAE, by its engines) as a special
// scheme. Plain holds the keys a plain livery has been found for.
static void insertLivery(QHash<QString, QString> &liveries, QSet<QString> &plain,
                         const QString &key, const QString &model, bool variant)
{
    if (!variant)
    {
        liveries.insert(key, model);
        plain.insert(key);
    }
    else if (!plain.contains(key))
    {
        liveries.insert(key, model);
    }
}

// The installed 3D models: the downloadable Bluebell set and any converted from FSLTL/MSFS
void Aircraft3DModels::findModels(bool verbose)
{
    // Look for all aircraft gltfs in 3d directory
    QString modelDir = dataDir() + "/3d";
    static const QStringList subDirs = {"BB_Airbus_png", "BB_Boeing_png", "BB_Jets_png", "BB_Props_png", "BB_GA_png", "BB_Mil_png", "BB_Heli_png"};

    for (auto subDir : subDirs)
    {
        QString dirName = modelDir + "/" + subDir;
        QDir dir(dirName);
        QStringList aircrafts = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for (auto aircraft : aircrafts)
        {
            if (verbose) {
                qDebug() << "Aircraft3DModels:" << aircraft;
            }
            QDir aircraftDir(dir.filePath(aircraft));
            QStringList gltfs = aircraftDir.entryList({"*.gltf"});
            QStringList allAircraft;
            for (auto gltf : gltfs)
            {
                QStringList filenameParts = gltf.split(".")[0].split("_");
                if (filenameParts.size() == 2)
                {
                    QString livery = filenameParts[1];
                    if (verbose) {
                        qDebug() << "Aircraft3DModels:" << aircraft << "Livery " << livery;
                    }
                    // Only use relative path, as Map feature will add the prefix
                    QString filename = subDir + "/" + aircraft + "/" + gltf;
                    m_3DModels.insert(aircraft + "_" + livery, filename);
                    allAircraft.append(filename);
                }
            }
            if (gltfs.size() > 0) {
                m_3DModelsByType.insert(aircraft, allAircraft);
            }
        }
    }

    // Models converted from FSLTL/MSFS are stored separately from the downloadable
    // Bluebell model set. Their directory names encode the ICAO type and, for
    // liveries, the operator: FSLTL_A20N_AAN. Index them using the same keys as the
    // existing models so ADS-B and Aircraft can select them without special cases.
    const QString msfsDir = modelDir + "/MSFS";
    // The converter writes a second, lower level of detail beside the model as
    // NAME_cesium_lod1.gltf, which the *_cesium.gltf scan below doesn't pick up
    QSet<QString> lowDetailFiles;
    QDirIterator lowDetailModels(msfsDir, {"*_cesium_lod1.gltf"}, QDir::Files, QDirIterator::Subdirectories);
    while (lowDetailModels.hasNext()) {
        lowDetailFiles.insert(lowDetailModels.next());
    }
    QDirIterator msfsModels(msfsDir, {"*_cesium.gltf"}, QDir::Files, QDirIterator::Subdirectories);
    QSet<QString> plainLiveries;
    while (msfsModels.hasNext())
    {
        const QString gltf = msfsModels.next();
        const QString aircraft = QFileInfo(gltf).completeBaseName().chopped(QString("_cesium").size());
        static const QString prefix = QStringLiteral("FSLTL_");
        QString type;
        QString operatorICAO;
        bool variant;
        if (!aircraft.startsWith(prefix, Qt::CaseInsensitive)
            || !typeAndOperator(aircraft.mid(prefix.size()), type, operatorICAO, variant)) {
            continue;
        }

        const QString filename = QDir(modelDir).relativeFilePath(gltf);
        m_3DModelsByType[type].append(filename);
        const QString lowDetail = gltf.chopped(QString(".gltf").size()) + "_lod1.gltf";
        if (lowDetailFiles.contains(lowDetail)) {
            m_lowDetailModels.insert(filename, QDir(modelDir).relativeFilePath(lowDetail));
        }
        if (!operatorICAO.isEmpty()) {
            insertLivery(m_3DModels, plainLiveries, type + "_" + operatorICAO, filename, variant);
        }
    }
}

// The 2D livery icons built into the ADS-B demodulator, standing in for the models. They
// were rendered from the models and are named after them (see liveryIconName()) -
// Bluebell's A320_BAW and FSLTL's A20N_AAN and Asobo_208B_ACN alike, all of which the
// FSLTL naming rules read - so the tables come out as if every model were installed,
// with an icon's name where a model's path would be. Of the plain stand-ins
// (aircraft_large, helicopter and the rest) only fire_truck passes for a type, as FIRE,
// which no aircraft is, so it is never asked for. Empty when the ADS-B demodulator,
// whose resources these are, isn't loaded
void Aircraft3DModels::findLiveryIcons()
{
    const QStringList icons = QDir(":/map/liveries").entryList({"*.png"}, QDir::Files);
    QSet<QString> plainLiveries;
    for (const QString &icon : icons)
    {
        const QString name = QFileInfo(icon).completeBaseName();
        QString type;
        QString operatorICAO;
        bool variant;
        if (!typeAndOperator(name, type, operatorICAO, variant)) {
            continue;
        }
        m_3DModelsByType[type].append(name);
        if (!operatorICAO.isEmpty()) {
            insertLivery(m_3DModels, plainLiveries, type + "_" + operatorICAO, name, variant);
        }
    }
}

Aircraft3DModels::Aircraft3DModels(bool verbose, bool liveryIcons) :
    m_liveryIcons(liveryIcons)
{
    // Timed and logged unconditionally: it runs once per session, reads a directory tree
    // that can be well over a gigabyte, and is the first thing to suspect when start up
    // is slow. A silent multi-second pause is precisely what makes that hard to find.
    QElapsedTimer timer;
    timer.start();

    if (liveryIcons) {
        findLiveryIcons();
    } else {
        findModels(verbose);
    }

    const QHash<QString, QStringList> &modelsByType = m_3DModelsByType;
    for (const QStringList &models : modelsByType)
    {
        for (const QString &model : models) {
            m_installedModels.insert(model);
        }
    }

    // Vertical offset so undercarriage isn't underground, because 0,0,0 is in the middle of the model
    // rather than at the bottom
    m_modelAltitudeOffset.insert("A306", 4.6f);
    m_modelAltitudeOffset.insert("A310", 4.6f);
    m_modelAltitudeOffset.insert("A318", 3.7f);
    m_modelAltitudeOffset.insert("A319", 3.5f);
    m_modelAltitudeOffset.insert("A320", 3.5f);
    m_modelAltitudeOffset.insert("A321", 3.5f);
    m_modelAltitudeOffset.insert("A332", 5.52f);
    m_modelAltitudeOffset.insert("A333", 5.52f);
    m_modelAltitudeOffset.insert("A334", 5.52f);
    m_modelAltitudeOffset.insert("A343", 4.65f);
    m_modelAltitudeOffset.insert("A345", 4.65f);
    m_modelAltitudeOffset.insert("A346", 4.65f);
    m_modelAltitudeOffset.insert("A388", 5.75f);
    m_modelAltitudeOffset.insert("B717", 0.0f);
    m_modelAltitudeOffset.insert("B733", 3.1f);
    m_modelAltitudeOffset.insert("B734", 3.27f);
    m_modelAltitudeOffset.insert("B737", 3.0f);
    m_modelAltitudeOffset.insert("B738", 3.31f);
    m_modelAltitudeOffset.insert("B739", 3.32f);
    m_modelAltitudeOffset.insert("B74F", 5.3f);
    m_modelAltitudeOffset.insert("B744", 5.25f);
    m_modelAltitudeOffset.insert("B752", 3.6f);
    m_modelAltitudeOffset.insert("B763", 4.44f);
    m_modelAltitudeOffset.insert("B772", 5.57f);
    m_modelAltitudeOffset.insert("B773", 5.6f);
    m_modelAltitudeOffset.insert("B77L", 5.57f);
    m_modelAltitudeOffset.insert("B77W", 5.57f);
    m_modelAltitudeOffset.insert("B788", 4.1f);
    m_modelAltitudeOffset.insert("BE20", 1.48f);
    m_modelAltitudeOffset.insert("C150", 1.05f);
    m_modelAltitudeOffset.insert("C172", 1.16f);
    m_modelAltitudeOffset.insert("C421", 1.16f);
    m_modelAltitudeOffset.insert("H25B", 1.45f);
    m_modelAltitudeOffset.insert("LJ45", 1.27f);
    m_modelAltitudeOffset.insert("B462", 1.8f);
    m_modelAltitudeOffset.insert("B463", 1.9f);
    m_modelAltitudeOffset.insert("CRJ2", 1.3f);
    m_modelAltitudeOffset.insert("CRJ7", 1.66f);
    m_modelAltitudeOffset.insert("CRJ9", 2.27f);
    m_modelAltitudeOffset.insert("CRJX", 2.49f);
    m_modelAltitudeOffset.insert("DC10", 5.2f);
    m_modelAltitudeOffset.insert("E135", 1.88f);
    m_modelAltitudeOffset.insert("E145", 1.86f);
    m_modelAltitudeOffset.insert("E170", 2.3f);
    m_modelAltitudeOffset.insert("E190", 3.05f);
    m_modelAltitudeOffset.insert("E195", 2.97f);
    m_modelAltitudeOffset.insert("F28", 2.34f);
    m_modelAltitudeOffset.insert("F70", 2.43f);
    m_modelAltitudeOffset.insert("F100", 2.23f);
    m_modelAltitudeOffset.insert("J328", 1.01f);
    m_modelAltitudeOffset.insert("MD11", 5.22f);
    m_modelAltitudeOffset.insert("MD83", 2.71f);
    m_modelAltitudeOffset.insert("MD90", 2.62f);
    m_modelAltitudeOffset.insert("AT42", 1.75f);
    m_modelAltitudeOffset.insert("AT72", 1.83f);
    m_modelAltitudeOffset.insert("D328", 0.99f);
    m_modelAltitudeOffset.insert("DH8D", 1.65f);
    m_modelAltitudeOffset.insert("F50", 2.16f);
    m_modelAltitudeOffset.insert("JS41", 1.9f);
    m_modelAltitudeOffset.insert("L410", 1.1f);
    m_modelAltitudeOffset.insert("SB20", 2.0f);
    m_modelAltitudeOffset.insert("SF34", 1.89f);

    // The models converted from FSLTL have their origin at MSFS's reference datum rather
    // than where the Bluebell models have theirs, so the same type needs a different
    // height - a Bluebell 737-800's 3.31 m left an FSLTL one floating 0.6 m up, and its
    // E170 2.3 m up, as that model's origin is at the wheels. These are the main wheels'
    // contact points below the datum, from each type's flight_model.cfg [CONTACT_POINTS],
    // checked against the models' own geometry with the gear down (A20N, B738, CRJ7,
    // BCS3, DH8D, E170, E190, SU95, LJ25, P28A within 0.15 m). The Asobo light aircraft
    // FSLTL reuses don't agree with their contact points (the Caravan's are 1.1 m out),
    // so theirs are the lowest point of their wheels, measured with the skinning their
    // gear legs use applied - not of the whole model, which has parts MSFS hides
    m_msfsModelAltitudeOffset = {
        {"A20N", 3.80f}, {"A21N", 3.93f}, {"A306", 5.24f}, {"A30B", 5.32f}, {"A319", 3.84f},
        {"A320", 3.80f}, {"A321", 3.89f}, {"A332", 5.64f}, {"A333", 5.61f}, {"A343", 5.45f},
        {"A346", 5.50f}, {"A359", 3.99f}, {"A388", 4.99f}, {"AT45", 2.01f}, {"AT46", 2.01f},
        {"AT75", 2.01f}, {"AT76", 2.01f}, {"B733", 2.49f}, {"B734", 2.30f}, {"B737", 2.73f},
        {"B738", 2.71f}, {"B739", 2.75f}, {"B73X", 2.59f}, {"B744", 4.82f}, {"B748", 4.98f},
        {"B763", 4.90f}, {"B772", 5.01f}, {"B77L", 5.01f}, {"B77W", 5.01f}, {"B788", 5.31f},
        {"B789", 5.30f}, {"B78X", 4.86f}, {"BCS1", 2.32f}, {"BCS3", 2.29f}, {"CRJ7", 2.43f},
        {"DH8C", 3.78f}, {"DH8D", 4.11f}, {"E170", 0.0f}, {"E190", 2.65f}, {"LJ25", 1.32f},
        {"MD11", 5.25f}, {"P28A", 1.18f}, {"SF34", 2.06f}, {"SU95", 0.08f},
        // Asobo, from their wheels
        {"B350", 1.46f}, {"C152", 0.97f}, {"C172", 1.33f}, {"C208", 2.46f}, {"C25C", 0.88f},
        {"DA40", 1.07f}, {"DA62", 1.01f}, {"DR40", 0.93f}, {"TBM9", 1.12f}, {"VL3", 1.02f}
    };

    // Label offsets (from bottom of aircraft)
    m_labelAltitudeOffset.insert("A306", 10.0f);
    m_labelAltitudeOffset.insert("A310", 15.0f);
    m_labelAltitudeOffset.insert("A318", 10.0f);
    m_labelAltitudeOffset.insert("A319", 10.0f);
    m_labelAltitudeOffset.insert("A320", 10.0f);
    m_labelAltitudeOffset.insert("A321", 10.0f);
    m_labelAltitudeOffset.insert("A332", 14.0f);
    m_labelAltitudeOffset.insert("A333", 14.0f);
    m_labelAltitudeOffset.insert("A334", 14.0f);
    m_labelAltitudeOffset.insert("A343", 14.0f);
    m_labelAltitudeOffset.insert("A345", 14.0f);
    m_labelAltitudeOffset.insert("A346", 14.0f);
    m_labelAltitudeOffset.insert("A388", 20.0f);
    m_labelAltitudeOffset.insert("B717", 7.5f);
    m_labelAltitudeOffset.insert("B733", 10.0f);
    m_labelAltitudeOffset.insert("B734", 10.0f);
    m_labelAltitudeOffset.insert("B737", 10.0f);
    m_labelAltitudeOffset.insert("B738", 10.0f);
    m_labelAltitudeOffset.insert("B739", 10.0f);
    m_labelAltitudeOffset.insert("B74F", 15.0f);
    m_labelAltitudeOffset.insert("B744", 15.0f);
    m_labelAltitudeOffset.insert("B752", 12.0f);
    m_labelAltitudeOffset.insert("B763", 14.0f);
    m_labelAltitudeOffset.insert("B772", 14.0f);
    m_labelAltitudeOffset.insert("B773", 14.0f);
    m_labelAltitudeOffset.insert("B77L", 14.0f);
    m_labelAltitudeOffset.insert("B77W", 14.0f);
    m_labelAltitudeOffset.insert("B788", 14.0f);
    m_labelAltitudeOffset.insert("BE20", 4.0f);
    m_labelAltitudeOffset.insert("C150", 3.0f);
    m_labelAltitudeOffset.insert("C172", 3.0f);
    m_labelAltitudeOffset.insert("C421", 4.0f);
    m_labelAltitudeOffset.insert("H25B", 5.0f);
    m_labelAltitudeOffset.insert("LJ45", 5.0f);
    m_labelAltitudeOffset.insert("B462", 7.0f);
    m_labelAltitudeOffset.insert("B463", 7.0f);
    m_labelAltitudeOffset.insert("CRJ2", 5.5f);
    m_labelAltitudeOffset.insert("CRJ7", 6.0f);
    m_labelAltitudeOffset.insert("CRJ9", 6.0f);
    m_labelAltitudeOffset.insert("CRJX", 6.0f);
    m_labelAltitudeOffset.insert("DC10", 15.0f);
    m_labelAltitudeOffset.insert("E135", 5.0f);
    m_labelAltitudeOffset.insert("E145", 5.0f);
    m_labelAltitudeOffset.insert("E170", 8.0f);
    m_labelAltitudeOffset.insert("E190", 8.5f);
    m_labelAltitudeOffset.insert("E195", 8.5f);
    m_labelAltitudeOffset.insert("F28", 7.0f);
    m_labelAltitudeOffset.insert("F70", 6.5f);
    m_labelAltitudeOffset.insert("F100", 6.5f);
    m_labelAltitudeOffset.insert("J328", 5.0f);  // Check
    m_labelAltitudeOffset.insert("MD11", 15.0f);
    m_labelAltitudeOffset.insert("MD83", 7.5f);
    m_labelAltitudeOffset.insert("MD90", 7.5f);
    m_labelAltitudeOffset.insert("AT42", 7.0f);
    m_labelAltitudeOffset.insert("AT72", 7.0f);
    m_labelAltitudeOffset.insert("D328", 6.0f);
    m_labelAltitudeOffset.insert("DH8D", 6.5f);
    m_labelAltitudeOffset.insert("F50",  7.0f);
    m_labelAltitudeOffset.insert("JS41", 5.0f);
    m_labelAltitudeOffset.insert("L410", 5.0f);
    m_labelAltitudeOffset.insert("SB20", 6.5f);
    m_labelAltitudeOffset.insert("SF34", 6.0f);
    // Types only the FSLTL models have, sized like their nearest relatives above
    m_labelAltitudeOffset.insert("A20N", 10.0f);
    m_labelAltitudeOffset.insert("A21N", 10.0f);
    m_labelAltitudeOffset.insert("A30B", 10.0f);
    m_labelAltitudeOffset.insert("A359", 14.0f);
    m_labelAltitudeOffset.insert("AT45", 7.0f);
    m_labelAltitudeOffset.insert("AT46", 7.0f);
    m_labelAltitudeOffset.insert("AT75", 7.0f);
    m_labelAltitudeOffset.insert("AT76", 7.0f);
    m_labelAltitudeOffset.insert("B73X", 10.0f);
    m_labelAltitudeOffset.insert("B748", 15.0f);
    m_labelAltitudeOffset.insert("B789", 14.0f);
    m_labelAltitudeOffset.insert("B78X", 14.0f);
    m_labelAltitudeOffset.insert("BCS1", 8.0f);
    m_labelAltitudeOffset.insert("BCS3", 8.5f);
    m_labelAltitudeOffset.insert("C208", 4.0f);
    m_labelAltitudeOffset.insert("DA62", 3.0f);
    m_labelAltitudeOffset.insert("DH8C", 6.5f);
    m_labelAltitudeOffset.insert("LJ25", 4.0f);
    m_labelAltitudeOffset.insert("P28A", 3.0f);
    m_labelAltitudeOffset.insert("SU95", 8.5f);
    m_labelAltitudeOffset.insert("TBM9", 3.5f);
    m_labelAltitudeOffset.insert("VL3", 2.5f);
    m_labelAltitudeOffset.insert("B350", 4.5f);
    m_labelAltitudeOffset.insert("C152", 3.0f);
    m_labelAltitudeOffset.insert("C25C", 4.5f);
    m_labelAltitudeOffset.insert("DA40", 3.0f);
    m_labelAltitudeOffset.insert("DR40", 3.0f);

    // Map from OpenSky database names to 3D model names. Only needed where the database
    // gives no ICAO type designator (0.2% of airline aircraft), which modelFor() prefers.
    // A pattern names the type the aircraft is, and modelFor() steps to a relative where
    // that has no model - so these name the A320neo, 787-9 and so on as what they are.
    // First match wins, so each specific pattern comes before the general one it is in
    m_3DModelMatch.append(new ModelMatch("A.?319.?1\\d\\dN", "A19N"));
    m_3DModelMatch.append(new ModelMatch("A.?320.?2\\d\\dN", "A20N"));
    m_3DModelMatch.append(new ModelMatch("A.?321.?2\\d\\dN", "A21N"));
    m_3DModelMatch.append(new ModelMatch("A350.?10\\d\\d", "A35K"));
    m_3DModelMatch.append(new ModelMatch("A220.?100|C.?Series.*100|CS100", "BCS1"));
    m_3DModelMatch.append(new ModelMatch("A220.?300|C.?Series.*300|CS300", "BCS3"));
    m_3DModelMatch.append(new ModelMatch("737.?MAX.?10", "B3XM"));
    m_3DModelMatch.append(new ModelMatch("737.?MAX.?7", "B37M"));
    m_3DModelMatch.append(new ModelMatch("737.?MAX.?8", "B38M"));
    m_3DModelMatch.append(new ModelMatch("737.?MAX.?9", "B39M"));
    m_3DModelMatch.append(new ModelMatch("747.?8", "B748"));
    m_3DModelMatch.append(new ModelMatch("787.?10", "B78X"));
    m_3DModelMatch.append(new ModelMatch("787.?9", "B789"));
    m_3DModelMatch.append(new ModelMatch("ATR.*42.*600", "AT46"));
    m_3DModelMatch.append(new ModelMatch("ATR.*42.*500", "AT45"));
    m_3DModelMatch.append(new ModelMatch("ATR.*72.*600", "AT76"));
    m_3DModelMatch.append(new ModelMatch("ATR.*72.*500", "AT75"));
    m_3DModelMatch.append(new ModelMatch("Superjet|RRJ.?95", "SU95"));
    m_3DModelMatch.append(new ModelMatch("EMB.?190.?E2|E190.?E2", "E290"));
    m_3DModelMatch.append(new ModelMatch("EMB.?195.?E2|E195.?E2", "E295"));
    m_3DModelMatch.append(new ModelMatch("EMB.?175|ERJ.?175|Embraer 175", "E75L"));
    m_3DModelMatch.append(new ModelMatch("DHC-8.?1\\d\\d", "DH8A"));
    m_3DModelMatch.append(new ModelMatch("DHC-8.?2\\d\\d", "DH8B"));
    m_3DModelMatch.append(new ModelMatch("DHC-8.?3\\d\\d", "DH8C"));
    m_3DModelMatch.append(new ManufacturerModelMatch("^208", "Cessna|Textron", "C208"));

    m_3DModelMatch.append(new ModelMatch("A300.*", "A306")); // A300 B4 is A300-600, but use for others as closest match
    m_3DModelMatch.append(new ModelMatch("A310.*", "A310"));
    m_3DModelMatch.append(new ModelMatch("A318.*", "A318"));
    m_3DModelMatch.append(new ModelMatch("A.?319.*", "A319"));
    m_3DModelMatch.append(new ModelMatch("A.?320.*", "A320"));
    m_3DModelMatch.append(new ModelMatch("A.?321.*", "A321"));
    m_3DModelMatch.append(new ModelMatch("A330.2.*", "A332"));
    m_3DModelMatch.append(new ModelMatch("A330.3.*", "A333"));
    m_3DModelMatch.append(new ModelMatch("A330.7.*", "A333")); // BelugaXL
    m_3DModelMatch.append(new ModelMatch("A330.8.*", "A332")); // 200 Neo
    m_3DModelMatch.append(new ModelMatch("A330.9.*", "A333")); // 300 Neo
    m_3DModelMatch.append(new ModelMatch("A340.3.*", "A343"));
    m_3DModelMatch.append(new ModelMatch("A340.5.*", "A345"));
    m_3DModelMatch.append(new ModelMatch("A340.6.*", "A346"));
    m_3DModelMatch.append(new ModelMatch("A350.*", "A359"));   // modelFor() falls back to the A330 without one
    m_3DModelMatch.append(new ModelMatch("A380.*", "A388"));

    m_3DModelMatch.append(new ModelMatch("737.2.*", "B733"));  // No 200 model
    m_3DModelMatch.append(new ModelMatch("737.3.*", "B733"));
    m_3DModelMatch.append(new ModelMatch("737.4.*", "B734"));
    m_3DModelMatch.append(new ModelMatch("737.5.*", "B734"));  // No 500 model
    m_3DModelMatch.append(new ModelMatch("737.6.*", "B737"));  // No 600 model
    m_3DModelMatch.append(new ModelMatch("737NG.6.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737.7.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737NG.7.*", "B737"));
    m_3DModelMatch.append(new ModelMatch("737.8.*", "B738"));
    m_3DModelMatch.append(new ModelMatch("737NG.8.*", "B738"));
    m_3DModelMatch.append(new ModelMatch("737.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("737NG.9", "B739"));
    m_3DModelMatch.append(new ModelMatch("B747.*F", "B74F"));
    m_3DModelMatch.append(new ModelMatch("B747.*\\(F\\)", "B74F"));
    m_3DModelMatch.append(new ModelMatch("747.*", "B744"));
    m_3DModelMatch.append(new ModelMatch("757.*", "B752"));
    m_3DModelMatch.append(new ModelMatch("767.*", "B763"));
    m_3DModelMatch.append(new ModelMatch("777.2.*LR.*", "B77L"));
    m_3DModelMatch.append(new ModelMatch("777.2.*", "B772"));
    m_3DModelMatch.append(new ModelMatch("777.3.*ER.*", "B77W"));
    m_3DModelMatch.append(new ModelMatch("777.3.*", "B773"));
    m_3DModelMatch.append(new ModelMatch("777.*", "B772"));
    m_3DModelMatch.append(new ModelMatch("787.*", "B788"));
    m_3DModelMatch.append(new ModelMatch("717.*", "B717"));
    // No 727 model

    // Jets
    m_3DModelMatch.append(new ModelMatch(".*EMB.135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch("Embraer 135.*", "E135"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch("Embraer 145.*", "E145"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch("Embraer 170.*", "E170"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch("Embraer 190.*", "E190"));
    m_3DModelMatch.append(new ModelMatch(".*EMB.195.*", "E195"));
    m_3DModelMatch.append(new ModelMatch(".*ERJ.195.*", "E195"));
    m_3DModelMatch.append(new ModelMatch("Embraer 195.*", "E195"));

    m_3DModelMatch.append(new ModelMatch(".*CRJ.200.*", "CRJ2"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.700.*", "CRJ7"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.900.*", "CRJ9"));
    m_3DModelMatch.append(new ModelMatch(".*CRJ.1000.*", "CRJX"));

    // PNGs missing
    //m_3DModelMatch.append(new ModelMatch("(BAE )?146.2.*", "B462"));
    //m_3DModelMatch.append(new ModelMatch("(BAE )?146.3.*", "B463"));

    m_3DModelMatch.append(new ModelMatch("DC-10.*", "DC10"));

    m_3DModelMatch.append(new ModelMatch(".*MD.11.*", "MD11"));
    m_3DModelMatch.append(new ModelMatch(".*MD.83.*", "MD83"));
    m_3DModelMatch.append(new ModelMatch(".*MD.90.*", "MD90"));

    m_3DModelMatch.append(new ModelMatch(".*F28.*", "F28"));
    m_3DModelMatch.append(new ModelMatch(".*F70.*", "F70"));
    m_3DModelMatch.append(new ModelMatch(".*F100.*", "F100"));

    // GA
    m_3DModelMatch.append(new ModelMatch(".*B200.*", "BE20"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*200.*", ".*Beech.*", "BE20"));
    m_3DModelMatch.append(new ModelMatch(".*150.*", "C150"));
    m_3DModelMatch.append(new ModelMatch(".*172.*", "C172"));
    m_3DModelMatch.append(new ModelMatch(".*421.*", "C421"));
    m_3DModelMatch.append(new ModelMatch(".*125.*", "H25B"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*400.*", "Hawker.*", "H25B"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*400.*", "Raytheon.*", "H25B"));
    m_3DModelMatch.append(new ModelMatch(".*Learjet.*", "LJ45"));

    // Props
    m_3DModelMatch.append(new ModelMatch("ATR.*42.*", "AT42"));
    m_3DModelMatch.append(new ModelMatch("ATR.*72.*", "AT72"));
    m_3DModelMatch.append(new ModelMatch("Do 328.*", "D328"));
    m_3DModelMatch.append(new ModelMatch("DHC-8.*", "DH8D"));
    m_3DModelMatch.append(new ModelMatch(".*F50.*", "F50"));
    m_3DModelMatch.append(new ModelMatch("Jetstream 41.*", "JS41"));
    m_3DModelMatch.append(new ModelMatch(".*L.410.*", "L410"));
    m_3DModelMatch.append(new ModelMatch("SAAB.2000.*", "SB20"));
    m_3DModelMatch.append(new ManufacturerModelMatch(".*340.*", "Saab.*", "SF34"));

    qDebug() << "Aircraft3DModels: scanned" << (liveryIcons ? QString(":/map/liveries") : dataDir() + "/3d")
             << "in" << timer.elapsed() << "ms -" << m_3DModelsByType.size() << "types," << m_3DModels.size()
             << "liveries," << m_3DModelMatch.size() << "name matches";
    (void) verbose;
}

QString Aircraft3DModels::modelForTypeAndOperator(const QString &type, const QString &operatorICAO, bool verbose) const
{
    QString typeOperator = type + "_" + operatorICAO;
    if (m_3DModels.contains(typeOperator))
    {
        const QString model = m_3DModels.value(typeOperator);
        if (hasTextures(model)) {
            return model;
        }
        if (verbose) {
            qDebug() << "Aircraft3DModels: livery" << model << "is missing its texture";
        }
    }
    else if (verbose) {
        qDebug() << "Aircraft3DModels: no livery for" << typeOperator;
    }
    return "";
}

QString Aircraft3DModels::modelForType(const QString &type) const
{
    if (m_3DModelsByType.contains(type))
    {
        // Choose a random livery - the first from a random start whose textures are
        // installed. QRandomGenerator::global() is thread safe, which the ADS-B tracker's
        // own generator was not once these tables became shared.
        const QStringList models = m_3DModelsByType.value(type);
        const int start = QRandomGenerator::global()->bounded(models.size());
        for (int i = 0; i < models.size(); i++)
        {
            const QString &model = models[(start + i) % models.size()];
            if (hasTextures(model)) {
                return model;
            }
        }
    }
    return "";
}

void Aircraft3DModels::offsetsForType(const QString &type, float &altitudeOffset, float &labelOffset,
                                      const QString &model) const
{
    const bool msfs = model.startsWith("MSFS/");
    if (msfs && m_msfsModelAltitudeOffset.contains(type)) {
        altitudeOffset = m_msfsModelAltitudeOffset.value(type);
    } else if (m_modelAltitudeOffset.contains(type)) {
        altitudeOffset = m_modelAltitudeOffset.value(type);
    } else {
        return;
    }
    labelOffset = m_labelAltitudeOffset.value(type, labelOffset);
}

// Types are grouped by what they LOOK like rather than by what they are, because the
// stand-in only has to be the right shape and size at map distances: a four engine
// heavy, a widebody twin, a narrowbody twin, a regional jet or turboprop, a business
// jet or a light piston/turboprop. Within a group the candidates are ordered by how
// close a match they are, and the search widens rather than giving up - a
// wrong-but-plausible aircraft reads far better than a billboard among modelled ones.
// The models span both sets: the Bluebell one has the business jets and Cessnas, the
// FSLTL one the Learjet 25, Caravan, TBM and DA62 instead.
bool Aircraft3DModels::defaultModel(const QString &icaoType, QString &model,
                                    float &altitudeOffset, float &labelOffset,
                                    bool knownTypesOnly) const
{
    if (m_3DModelsByType.isEmpty()) {
        return false;
    }

    static const QStringList fourEngine = {
        "A342", "A343", "A345", "A346", "A388", "B741", "B742", "B743", "B744",
        "B748", "B74F", "BLCF", "C130", "C17", "IL76", "AN12", "E3TF", "K35R"};
    static const QStringList heavyTwin = {
        "B762", "B763", "B764", "B772", "B773", "B77L", "B77W", "B788", "B789",
        "B78X", "A306", "A30B", "A310", "A332", "A333", "A338", "A339", "A359",
        "A35K", "MD11", "DC10", "B778", "B779"};
    static const QStringList narrowBody = {
        "B712", "B717", "B732", "B735", "B736", "B752", "B753", "MD81", "MD82",
        "MD83", "MD87", "MD88", "MD90", "A318", "C919", "B37M", "B38M", "B39M",
        "B3XM"};
    static const QStringList regionalJet = {
        "E135", "E145", "E35L", "E170", "E75L", "E75S", "E190", "E195", "E290",
        "E295", "CRJ1", "CRJ2", "CRJ7", "CRJ9", "CRJX", "F28", "F70", "F100",
        "B461", "B462", "B463", "RJ70", "RJ85", "RJ1H", "J328", "SU95", "BCS1",
        "BCS3", "A148"};
    static const QStringList turboprop = {
        "AT42", "AT43", "AT45", "AT46", "AT72", "AT73", "AT75", "AT76", "DH8A",
        "DH8B", "DH8C", "DH8D", "SF34", "SB20", "JS41", "JS32", "D328", "F50",
        "L410", "E120", "B190", "SW4", "DHC6", "AN26", "C295"};
    static const QStringList smallJet = {
        "GLF4", "GLF5", "GLF6", "GLEX", "GL5T", "GL7T", "CL30", "CL35", "CL60",
        "C25A", "C25B", "C25C", "C25M", "C501", "C510", "C525", "C550", "C560",
        "C56X", "C650", "C680", "C68A", "C700", "C750", "E50P", "E55P", "E545",
        "E550", "F2TH", "F900", "FA7X", "FA8X", "FA50", "LJ25", "LJ31", "LJ35",
        "LJ40", "LJ45", "LJ55", "LJ60", "LJ75", "H25A", "H25B", "H25C", "PC24",
        "PRM1", "HDJT", "SF50", "BE40", "G150", "G280", "ASTR", "GALX", "EA50"};
    static const QStringList smallProp = {
        "BE9L", "BE20", "BE30", "B350", "BE36", "BE33", "BE35", "BE55", "BE58",
        "BE76", "C150", "C152", "C162", "C170", "C172", "C177", "C182", "C206",
        "C208", "C210", "C310", "C340", "C414", "C421", "PA18", "PA22", "PA24",
        "PA28", "P28A", "P28B", "P28R", "PA30", "PA31", "PA32", "PA34", "PA44",
        "PA46", "SR20", "SR22", "DA20", "DA40", "DA42", "DA62", "TBM7", "TBM8",
        "TBM9", "PC6T", "PC12", "M20P", "M20T", "AA5", "DR40", "VL3", "P180",
        "TB20", "TOBA", "RV7", "RV8", "RV9"};

    // Preference order per group. First one with a model installed wins
    static const QStringList fourEngineModels  = {"B744", "A388", "A343", "B74F", "B742", "B748"};
    static const QStringList heavyTwinModels   = {"B77W", "A333", "B788", "A332", "B763", "B772", "B789", "A359"};
    static const QStringList narrowBodyModels  = {"A320", "B738", "A319", "A321", "B737", "B739", "B733", "A20N", "A21N"};
    static const QStringList regionalJetModels = {"CRJ7", "E190", "E170", "CRJ9", "CRJ2", "E145", "BCS1", "BCS3"};
    static const QStringList turbopropModels   = {"AT75", "AT72", "AT76", "DH8D", "DH8C", "SF34", "AT45", "AT42"};
    static const QStringList smallJetModels    = {"GLF5", "GLEX", "C750", "CL60", "LJ45", "C680", "C25C", "LJ25"};
    static const QStringList smallPropModels   = {"B350", "BE20", "DA62", "C421", "C208", "TBM9", "DA40", "P28A",
                                                  "C172", "C152", "C150", "VL3"};

    QList<const QStringList *> order;
    if (fourEngine.contains(icaoType)) {
        order = {&fourEngineModels, &heavyTwinModels, &narrowBodyModels, &regionalJetModels, &smallJetModels};
    } else if (heavyTwin.contains(icaoType)) {
        order = {&heavyTwinModels, &fourEngineModels, &narrowBodyModels, &regionalJetModels, &smallJetModels};
    } else if (narrowBody.contains(icaoType)) {
        order = {&narrowBodyModels, &regionalJetModels, &heavyTwinModels, &turbopropModels, &smallJetModels};
    } else if (regionalJet.contains(icaoType)) {
        order = {&regionalJetModels, &narrowBodyModels, &turbopropModels, &smallJetModels, &heavyTwinModels};
    } else if (turboprop.contains(icaoType)) {
        order = {&turbopropModels, &regionalJetModels, &smallPropModels, &narrowBodyModels};
    } else if (smallJet.contains(icaoType)) {
        order = {&smallJetModels, &smallPropModels, &regionalJetModels, &narrowBodyModels};
    } else if (smallProp.contains(icaoType)) {
        order = {&smallPropModels, &smallJetModels, &turbopropModels, &narrowBodyModels};
    } else if (knownTypesOnly) {
        return false;
    } else {
        // Unknown or blank type. Most of what flies with a transponder and no model of
        // its own is an airliner-shaped thing, so that is the safer guess
        order = {&narrowBodyModels, &heavyTwinModels, &regionalJetModels, &smallJetModels, &smallPropModels,
                 &fourEngineModels, &turbopropModels};
    }

    for (const QStringList *candidates : order)
    {
        for (const QString& candidate : *candidates)
        {
            const QString found = modelForType(candidate);
            if (!found.isEmpty())
            {
                model = found;
                offsetsForType(candidate, altitudeOffset, labelOffset, model);
                return true;
            }
        }
    }

    // Nothing from any preference list is installed, so take whatever is. Sorted so the
    // same aircraft does not change shape from one run to the next.
    QStringList types = m_3DModelsByType.keys();
    types.sort();
    const QString type = types.first();
    model = modelForType(type);
    offsetsForType(type, altitudeOffset, labelOffset, model);
    return !model.isEmpty();
}

// A type's nearest relatives, closest first, to stand in when it has no model installed.
// Both ways round, so a model set with only one generation of an aircraft serves the
// other: the FSLTL set has the ATR 72-500 and -600 (AT75, AT76) but not the AT72 the
// Bluebell set has, and the Bluebell set has no A320neo or 787-9 at all
static const QHash<QString, QStringList>& similarTypes()
{
    static const QHash<QString, QStringList> similar = {
        // Airbus narrowbodies
        {"A318", {"A319", "A19N", "A320"}},
        {"A319", {"A19N", "A320", "A318"}},
        {"A19N", {"A319", "A20N", "A320"}},
        {"A320", {"A20N", "A319", "A321"}},
        {"A20N", {"A320", "A21N", "A321"}},
        {"A321", {"A21N", "A320", "A20N"}},
        {"A21N", {"A321", "A20N", "A320"}},
        // Airbus widebodies
        {"A306", {"A30B", "A310"}},
        {"A30B", {"A306", "A310"}},
        {"A310", {"A306", "A30B"}},
        {"A332", {"A338", "A333"}},
        {"A333", {"A339", "A332"}},
        {"A338", {"A332", "A339", "A333"}},
        {"A339", {"A333", "A338", "A332"}},
        {"A342", {"A343", "A345", "A346"}},
        {"A343", {"A342", "A345", "A346"}},
        {"A345", {"A346", "A343", "A342"}},
        {"A346", {"A345", "A343", "A342"}},
        {"A359", {"A35K", "A339", "A333"}},
        {"A35K", {"A359", "A339", "A333"}},
        // Boeing narrowbodies
        {"B732", {"B733", "B734"}},
        {"B733", {"B734", "B735", "B737"}},
        {"B734", {"B733", "B735", "B738"}},
        {"B735", {"B734", "B733", "B737"}},
        {"B736", {"B737", "B735"}},
        {"B737", {"B37M", "B736", "B738"}},
        {"B738", {"B38M", "B739", "B737"}},
        {"B739", {"B39M", "B738", "B73X"}},
        {"B37M", {"B737", "B38M", "B738"}},
        {"B38M", {"B738", "B39M", "B739"}},
        {"B39M", {"B739", "B38M", "B738"}},
        {"B3XM", {"B73X", "B39M", "B739"}},
        {"B73X", {"B3XM", "B39M", "B739"}},
        {"B752", {"B753"}},
        {"B753", {"B752"}},
        // Boeing widebodies
        {"B741", {"B742", "B744"}},
        {"B742", {"B744", "B741"}},
        {"B743", {"B744", "B742"}},
        {"B744", {"B748", "B743"}},
        {"B748", {"B744"}},
        {"B762", {"B763", "B764"}},
        {"B763", {"B762", "B764"}},
        {"B764", {"B763", "B762"}},
        {"B772", {"B77L", "B77W", "B773"}},
        {"B77L", {"B772", "B77W", "B773"}},
        {"B773", {"B77W", "B772", "B77L"}},
        {"B77W", {"B773", "B77L", "B772"}},
        {"B778", {"B77W", "B773"}},
        {"B779", {"B77W", "B773"}},
        {"B788", {"B789", "B78X"}},
        {"B789", {"B788", "B78X"}},
        {"B78X", {"B789", "B788"}},
        {"DC10", {"MD11"}},
        {"MD11", {"DC10"}},
        {"MD81", {"MD83", "MD90"}},
        {"MD82", {"MD83", "MD90"}},
        {"MD83", {"MD90"}},
        {"MD87", {"MD83", "MD90"}},
        {"MD88", {"MD83", "MD90"}},
        {"MD90", {"MD83"}},
        // Regional
        {"AT42", {"AT45", "AT43", "AT46"}},
        {"AT43", {"AT45", "AT42", "AT46"}},
        {"AT45", {"AT46", "AT43", "AT42"}},
        {"AT46", {"AT45", "AT43", "AT42"}},
        {"AT72", {"AT75", "AT73", "AT76"}},
        {"AT73", {"AT75", "AT72", "AT76"}},
        {"AT75", {"AT76", "AT73", "AT72"}},
        {"AT76", {"AT75", "AT73", "AT72"}},
        {"BCS1", {"BCS3"}},
        {"BCS3", {"BCS1"}},
        {"CRJ1", {"CRJ2"}},
        {"CRJ2", {"CRJ1", "CRJ7"}},
        {"CRJ7", {"CRJ9", "CRJ2"}},
        {"CRJ9", {"CRJ7", "CRJX"}},
        {"CRJX", {"CRJ9", "CRJ7"}},
        {"DH8A", {"DH8B", "DH8C", "DH8D"}},
        {"DH8B", {"DH8C", "DH8A", "DH8D"}},
        {"DH8C", {"DH8B", "DH8D"}},
        {"DH8D", {"DH8C"}},
        {"E135", {"E145"}},
        {"E145", {"E135"}},
        {"E35L", {"E135", "E145"}},
        {"E170", {"E75S", "E75L", "E190"}},
        {"E75L", {"E75S", "E170", "E190"}},
        {"E75S", {"E75L", "E170", "E190"}},
        {"E190", {"E290", "E195", "E170"}},
        {"E195", {"E295", "E190"}},
        {"E290", {"E190", "E295", "E195"}},
        {"E295", {"E195", "E290", "E190"}},
        {"F70", {"F100"}},
        {"F100", {"F70"}},
        {"B461", {"B462", "B463"}},
        {"B462", {"B463", "B461"}},
        {"B463", {"B462", "B461"}},
        // Light aircraft
        {"BE9L", {"BE20", "B350"}},
        {"BE20", {"B350", "BE30", "BE9L"}},
        {"BE30", {"B350", "BE20"}},
        {"B350", {"BE30", "BE20"}},
        {"C150", {"C152", "C172"}},
        {"C152", {"C150", "C172"}},
        {"C172", {"C152", "C150"}},
        {"LJ25", {"LJ35", "LJ45"}},
        {"LJ35", {"LJ45", "LJ25"}},
        {"LJ45", {"LJ35", "LJ25"}},
    };
    return similar;
}

bool Aircraft3DModels::modelFor(const AircraftInformation *info, bool favourLivery, bool verbose,
                                QString &model, float &altitudeOffset, float &labelOffset) const
{
    if (!info || (info->m_model.isEmpty() && info->m_type.isEmpty())) {
        return false;
    }

    model = "";

    // The types to try, best first. The ICAO designator the database gives comes first:
    // it tells an A320neo from an A320 and a 787-9 from a 787-8, which the name patterns
    // can't - "A320 251NSL" is an A20N - and it is there for 99.8% of airline aircraft.
    // Then the type the model's name suggests, for the rest. Each is followed by its
    // nearest relatives, for when it has no model of its own installed
    QStringList candidates;
    const auto addCandidate = [&candidates](const QString &candidate) {
        if (candidate.isEmpty() || candidates.contains(candidate)) {
            return;
        }
        candidates.append(candidate);
        for (const QString &similar : similarTypes().value(candidate))
        {
            if (!candidates.contains(similar)) {
                candidates.append(similar);
            }
        }
    };
    static const QRegularExpression designator("^[A-Z0-9]{2,4}$");
    const QString icaoType = info->m_type.trimmed().toUpper();
    if (designator.match(icaoType).hasMatch()) {
        addCandidate(icaoType);
    }
    for (auto mm : m_3DModelMatch)
    {
        QString matched;
        if (mm->match(info->m_model, info->m_manufacturerName, matched))
        {
            addCandidate(matched);
            break;
        }
    }

    QString type;
    for (const QString &candidate : candidates)
    {
        if (!modelForType(candidate).isEmpty())
        {
            type = candidate;
            break;
        }
    }

    if (!type.isEmpty())
    {
        QString operatorICAO = info->m_operatorICAO;

        // The operator's livery, on the type or one of its nearest relatives: an airline's
        // A320 is a better match for its A320neo than another airline's A320neo
        QStringList liveryTypes = {type};
        liveryTypes.append(similarTypes().value(type));
        const auto liveryFor = [&](const QString &op, const QStringList &types) {
            for (const QString &candidate : types)
            {
                const QString found = modelForTypeAndOperator(candidate, op, verbose);
                if (!found.isEmpty())
                {
                    type = candidate;
                    return found;
                }
            }
            return QString();
        };

        if (!operatorICAO.isEmpty()) {
            model = liveryFor(operatorICAO, liveryTypes);
        }

        if (model.isEmpty())
        {
            // Try similar operator (E.g. EasyJet instead of EasyJet Europe)
            static const QHash<QString, QString> alternateOperator = {
                {"EJU", "EZY"},
                {"WUK", "WZZ"},
                {"TFL", "TOM"},
                {"NOZ", "NAX"},
                {"NSZ", "NAX"},
                {"BCS", "DHK"},
            };

            if (alternateOperator.contains(operatorICAO))
            {
                operatorICAO = alternateOperator.value(operatorICAO);
                model = liveryFor(operatorICAO, liveryTypes);
            }

            if (model.isEmpty() && favourLivery && !operatorICAO.isEmpty())
            {
                // Try to find a different aircraft of about the same size in the livery
                static const QHash<QString, QStringList> alternateTypes = {
                    {"B788", {"B789", "B78X", "B77W", "B77L", "B772", "B773", "B763", "A332", "A333", "A359"}},
                    {"B789", {"B788", "B78X", "B77W", "B77L", "B772", "B773", "B763", "A332", "A333", "A359"}},
                    {"B78X", {"B789", "B788", "B77W", "B77L", "B772", "B773", "B763", "A333", "A359"}},
                    {"B77W", {"B77L", "B772", "B773", "B788", "B789", "B763", "A332", "A333", "A359"}},
                    {"B77L", {"B77W", "B772", "B773", "B788", "B789", "B763", "A332", "A333", "A359"}},
                    {"B772", {"B77W", "B77L", "B773", "B788", "B789", "B763", "A332", "A333", "A359"}},
                    {"B773", {"B77W", "B77L", "B772", "B788", "B789", "B763", "A332", "A333", "A359"}},
                    {"A332", {"A333", "A359", "B77W", "B77L", "B773", "B772", "B788", "B789", "B763"}},
                    {"A333", {"A332", "A359", "B77W", "B77L", "B773", "B772", "B788", "B789", "B763"}},
                    {"A359", {"A333", "A332", "B789", "B788", "B77W", "B77L", "B772", "B773"}},
                    {"A342", {"A343", "A345", "A346"}},
                    {"A343", {"A342", "A345", "A346"}},
                    {"A345", {"A343", "A342", "A346"}},
                    {"A346", {"A345", "A343", "A342"}},
                    {"B744", {"B74F", "B748"}},
                    {"B74F", {"B744", "B748"}},
                    {"B748", {"B744", "B74F"}},
                    {"B733", {"B734", "B737", "B738", "B739", "B752", "A320", "A20N", "A319", "A321", "A21N"}},
                    {"B734", {"B733", "B737", "B738", "B739", "B752", "A320", "A20N", "A319", "A321", "A21N"}},
                    {"B737", {"B733", "B734", "B738", "B739", "B752", "A320", "A20N", "A319", "A321", "A21N"}},
                    {"B738", {"B733", "B734", "B737", "B739", "B752", "A320", "A20N", "A319", "A321", "A21N"}},
                    {"B739", {"B733", "B734", "B737", "B738", "B752", "A320", "A20N", "A319", "A321", "A21N"}},
                    {"A319", {"A320", "A20N", "A321", "A21N", "B733", "B734", "B737", "B738", "B739"}},
                    {"A320", {"A20N", "A319", "A321", "A21N", "B733", "B734", "B737", "B738", "B739"}},
                    {"A20N", {"A320", "A21N", "A319", "A321", "B738", "B737", "B739", "B733", "B734"}},
                    {"A321", {"A21N", "A320", "A20N", "A319", "B739", "B738", "B737", "B733", "B734"}},
                    {"A21N", {"A321", "A20N", "A320", "A319", "B739", "B738", "B737", "B733", "B734"}},
                    {"A306", {"A30B", "A332", "A333", "B763"}},
                    {"AT75", {"AT76", "AT72", "AT45", "AT46", "AT42", "DH8D"}},
                    {"AT76", {"AT75", "AT72", "AT46", "AT45", "AT42", "DH8D"}},
                    {"AT72", {"AT75", "AT76", "AT45", "AT46", "AT42", "DH8D"}},
                    {"AT45", {"AT46", "AT42", "AT75", "AT76", "AT72"}},
                    {"AT46", {"AT45", "AT42", "AT76", "AT75", "AT72"}},
                    {"AT42", {"AT45", "AT46", "AT72", "AT75", "AT76"}},
                    {"BCS1", {"BCS3", "E190", "E195", "A319"}},
                    {"BCS3", {"BCS1", "E195", "E190", "A319", "A20N"}},
                    {"E190", {"E195", "E170", "BCS1", "BCS3", "CRJ9"}},
                    {"E170", {"E190", "CRJ7", "CRJ9"}},
                };

                if (alternateTypes.contains(type)) {
                    model = liveryFor(operatorICAO, alternateTypes.value(type));
                }
            }

            if (model.isEmpty())
            {
                // Try for aircraft without specific livery
                model = modelForType(type);
            }
        }
    }

    if (verbose)
    {
        if (model.isEmpty()) {
            qDebug() << "Aircraft3DModels: no model for" << info->m_type << info->m_model << info->m_operatorICAO
                     << "- tried" << candidates;
        } else {
            qDebug() << "Aircraft3DModels: matched" << info->m_type << info->m_model << info->m_operatorICAO << "to" << model;
        }
    }

    if (model.isEmpty()) {
        return false;
    }
    offsetsForType(type, altitudeOffset, labelOffset, model);
    return true;
}
