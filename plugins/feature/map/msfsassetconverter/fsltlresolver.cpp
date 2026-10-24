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

#include "fsltlresolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QTextStream>
#include <QVector>
#include <QXmlStreamReader>

#include <algorithm>

namespace {

using IniValues = QHash<QString, QString>;

QString removeIniComment(const QString &line)
{
    bool inQuotes = false;
    for (qsizetype i = 0; i < line.size(); ++i)
    {
        if (line.at(i) == '"') {
            inQuotes = !inQuotes;
        } else if (line.at(i) == ';' && !inQuotes) {
            return line.left(i);
        }
    }
    return line;
}

QString cleanIniValue(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.mid(1, value.size() - 2);
    }
    return value.trimmed();
}

bool readIni(const QString &fileName, IniValues *values, QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *error =
            QStringLiteral("Cannot open %1: %2").arg(fileName, file.errorString());
        return false;
    }

    QString section;
    QTextStream stream(&file);
    while (!stream.atEnd())
    {
        const QString line = removeIniComment(stream.readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']'))
        {
            section = line.mid(1, line.size() - 2).trimmed().toLower();
            continue;
        }
        const qsizetype equals = line.indexOf('=');
        if (equals <= 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed().toLower();
        values->insert(section + '/' + key, cleanIniValue(line.mid(equals + 1)));
    }
    return true;
}

QString resolveRelative(const QDir &base, QString relative)
{
    relative.replace('\\', '/');
    return QDir::cleanPath(base.absoluteFilePath(relative));
}

QString canonicalDirectory(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool buildAircraftHierarchy(const QString &aircraftCfg, QStringList *hierarchy,
                            QString *error)
{
    QString currentDirectory = QFileInfo(aircraftCfg).absolutePath();
    QSet<QString> visited;
    for (int depth = 0; depth < 32; ++depth)
    {
        currentDirectory = canonicalDirectory(currentDirectory);
        const QString key = currentDirectory.toLower();
        if (visited.contains(key))
        {
            *error = QStringLiteral("Cycle in base_container hierarchy at %1")
                         .arg(currentDirectory);
            return false;
        }
        visited.insert(key);
        hierarchy->append(currentDirectory);

        const QString cfg = QDir(currentDirectory).filePath("aircraft.cfg");
        IniValues values;
        if (!readIni(cfg, &values, error)) {
            return false;
        }
        const QString baseContainer = values.value("variation/base_container");
        if (baseContainer.isEmpty()) {
            return true;
        }
        currentDirectory = resolveRelative(QDir(currentDirectory), baseContainer);
        if (!QFileInfo(currentDirectory).isDir())
        {
            *error = QStringLiteral("base_container does not exist: %1")
                         .arg(currentDirectory);
            return false;
        }
    }
    *error = QStringLiteral("base_container hierarchy is too deep");
    return false;
}

QString findVariantDirectory(const QStringList &hierarchy,
                             const QString &prefix, const QString &variant)
{
    const QString folderName =
        variant.isEmpty() ? prefix : prefix + '.' + variant;
    for (const QString &aircraftDirectory : hierarchy) {
        const QString candidate = QDir(aircraftDirectory).filePath(folderName);
        if (QFileInfo(candidate).isDir()) {
            return canonicalDirectory(candidate);
        }
    }
    return {};
}

bool selectLod(const QString &xmlFile, int lodIndex, QString *gltfFile,
               QString *error)
{
    QFile file(xmlFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        *error =
            QStringLiteral("Cannot open %1: %2").arg(xmlFile, file.errorString());
        return false;
    }
    QXmlStreamReader xml(&file);
    QVector<QPair<double, QString>> models;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (!xml.isStartElement() ||
            xml.name().compare(QStringLiteral("LOD"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        const auto attributes = xml.attributes();
        QString modelFile;
        double minSize = 0.0;
        for (const QXmlStreamAttribute &attribute : attributes)
        {
            if (attribute.name().compare(QStringLiteral("ModelFile"),
                                         Qt::CaseInsensitive) == 0) {
                modelFile = attribute.value().toString();
            } else if (attribute.name().compare(QStringLiteral("minSize"),
                                                Qt::CaseInsensitive) == 0) {
                bool ok = false;
                const double parsed = attribute.value().toDouble(&ok);
                if (ok) {
                    minSize = parsed;
                }
            }
        }
        if (!modelFile.isEmpty()) {
            models.append({minSize, modelFile});
        }
    }
    if (xml.hasError())
    {
        *error = QStringLiteral("Invalid model XML %1: %2")
                     .arg(xmlFile, xml.errorString());
        return false;
    }
    std::sort(models.begin(), models.end(),
              [](const auto &left, const auto &right) {
                  return left.first > right.first;
              });
    if (models.isEmpty())
    {
        *error = QStringLiteral("No LOD ModelFile found in %1").arg(xmlFile);
        return false;
    }
    if (lodIndex < 0 || lodIndex >= models.size())
    {
        *error = QStringLiteral(
                     "LOD index %1 is out of range; %2 LODs are available in %3")
                     .arg(lodIndex)
                     .arg(models.size())
                     .arg(xmlFile);
        return false;
    }
    *gltfFile =
        resolveRelative(QFileInfo(xmlFile).dir(), models.at(lodIndex).second);
    if (!QFileInfo(*gltfFile).isFile())
    {
        *error = QStringLiteral("LOD glTF does not exist: %1").arg(*gltfFile);
        return false;
    }
    return true;
}

bool resolveModel(const QStringList &hierarchy, const QString &variant,
                  int lodIndex, QString *gltfFile, QString *modelXml, QString *error)
{
    const QString modelDirectory =
        findVariantDirectory(hierarchy, QStringLiteral("model"), variant);
    if (modelDirectory.isEmpty())
    {
        *error = QStringLiteral("Cannot find model.%1 in the aircraft hierarchy")
                     .arg(variant);
        return false;
    }
    IniValues modelValues;
    const QString modelCfg = QDir(modelDirectory).filePath("model.cfg");
    if (!readIni(modelCfg, &modelValues, error)) {
        return false;
    }
    // Asobo's newer aircraft call the outside model "exterior" rather than "normal"
    // (the King Air 350, C172, CJ4, DA40, DR400 and Bonanza among FSLTL's)
    QString normalModel = modelValues.value("models/normal");
    if (normalModel.isEmpty()) {
        normalModel = modelValues.value("models/exterior");
    }
    if (normalModel.isEmpty())
    {
        *error = QStringLiteral("No [models] normal or exterior entry in %1").arg(modelCfg);
        return false;
    }
    const QString modelDefinition =
        resolveRelative(QDir(modelDirectory), normalModel);
    if (modelDefinition.endsWith(".gltf", Qt::CaseInsensitive))
    {
        *gltfFile = modelDefinition;
        if (!QFileInfo(*gltfFile).isFile())
        {
            *error = QStringLiteral("Model glTF does not exist: %1").arg(*gltfFile);
            return false;
        }
        return true;
    }
    *modelXml = modelDefinition;
    return selectLod(modelDefinition, lodIndex, gltfFile, error);
}

bool appendTextureFallbacks(const QString &directory, QStringList *result,
                            QSet<QString> *visited, QStringList *warnings,
                            QString *error)
{
    const QString resolved = canonicalDirectory(directory);
    const QString key = resolved.toLower();
    if (visited->contains(key)) {
        return true;
    }
    visited->insert(key);
    if (!QFileInfo(resolved).isDir())
    {
        warnings->append(
            QStringLiteral("Skipping missing texture fallback directory: %1")
                .arg(resolved));
        return true;
    }
    result->append(resolved);

    const QString textureCfg = QDir(resolved).filePath("texture.cfg");
    if (!QFileInfo(textureCfg).isFile()) {
        return true;
    }
    IniValues values;
    if (!readIni(textureCfg, &values, error)) {
        return false;
    }
    for (int index = 1; index <= 64; ++index) {
        const QString fallback =
            values.value(QStringLiteral("fltsim/fallback.%1").arg(index));
        if (fallback.isEmpty()) {
            continue;
        }
        if (!appendTextureFallbacks(resolveRelative(QDir(resolved), fallback),
                                    result, visited, warnings, error)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool findFsltlAircraftVariants(const QString &aircraftCfg,
                               QVector<int> *flightSimIndices,
                               QString *error)
{
    flightSimIndices->clear();

    IniValues aircraftValues;
    if (!readIni(aircraftCfg, &aircraftValues, error)) {
        return false;
    }

    QMap<int, QPair<QString, QString>> variants;
    for (auto it = aircraftValues.constBegin(); it != aircraftValues.constEnd();
         ++it) {
        const QString &key = it.key();
        if (!key.startsWith(QStringLiteral("fltsim."))) {
            continue;
        }
        const qsizetype slash = key.indexOf('/');
        if (slash < 8) {
            continue;
        }
        bool validIndex = false;
        const int index = key.mid(7, slash - 7).toInt(&validIndex);
        if (!validIndex) {
            continue;
        }
        const QString field = key.mid(slash + 1);
        if (field == QStringLiteral("model")) {
            variants[index].first = it.value();
        } else if (field == QStringLiteral("texture")) {
            variants[index].second = it.value();
        }
    }

    QSet<QString> seen;
    for (auto it = variants.constBegin(); it != variants.constEnd(); ++it) {
        const QString identity = it.value().first.toCaseFolded() + QChar(0) +
                                 it.value().second.toCaseFolded();
        if (!seen.contains(identity))
        {
            seen.insert(identity);
            flightSimIndices->append(it.key());
        }
    }
    if (flightSimIndices->isEmpty())
    {
        *error = QStringLiteral("No FLTSIM model/texture entries in %1")
                     .arg(aircraftCfg);
        return false;
    }
    return true;
}

bool resolveFsltlAircraft(const QString &aircraftCfg, int flightSimIndex,
                          int lodIndex, ResolvedFsltlAircraft *result,
                          QString *error)
{
    *result = {};
    const QFileInfo cfgInfo(aircraftCfg);
    if (!cfgInfo.isFile())
    {
        *error = QStringLiteral("aircraft.cfg does not exist: %1").arg(aircraftCfg);
        return false;
    }
    IniValues aircraftValues;
    if (!readIni(cfgInfo.absoluteFilePath(), &aircraftValues, error)) {
        return false;
    }
    if (!buildAircraftHierarchy(cfgInfo.absoluteFilePath(),
                                &result->m_aircraftHierarchy, error)) {
        return false;
    }

    const QString section = QStringLiteral("fltsim.%1/").arg(flightSimIndex);
    const QString modelVariant = aircraftValues.value(section + "model");
    const QString textureVariant = aircraftValues.value(section + "texture");
    if (!aircraftValues.contains(section + "model") &&
        !aircraftValues.contains(section + "texture")) {
        *error = QStringLiteral("No [FLTSIM.%1] model/texture entries in %2")
                     .arg(flightSimIndex)
                     .arg(aircraftCfg);
        return false;
    }
    if (!resolveModel(result->m_aircraftHierarchy, modelVariant, lodIndex,
                      &result->m_gltfFile, &result->m_modelXml, error)) {
        return false;
    }

    const QString textureDirectory = findVariantDirectory(
        result->m_aircraftHierarchy, QStringLiteral("texture"), textureVariant);
    if (textureDirectory.isEmpty())
    {
        *error = QStringLiteral("Cannot find texture.%1 in the aircraft hierarchy")
                     .arg(textureVariant);
        return false;
    }
    QSet<QString> visitedTextures;
    if (!appendTextureFallbacks(textureDirectory, &result->m_textureDirectories,
                                &visitedTextures, &result->m_warnings, error)) {
        return false;
    }
    result->m_outputBaseName = cfgInfo.dir().dirName();
    return true;
}
