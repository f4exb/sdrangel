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

#include "asset3dmanifest.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

const QString Asset3DManifest::m_manifestURL = "https://sdrangel.org/downloads/3dassets.json";

// Fields added to the manifest in later versions are additive, so a manifest that
// declares a newer format is still read - just without whatever we don't know about
#define ASSET3D_FORMAT_VERSION 1

// The models have individual licensing. See LICENSE on github
#define SDRANGEL_3D_MODELS "https://github.com/srcejon/sdrangel-3d-models/releases/latest/download/sdrangel3dmodels.zip"
// Textures from Bluebell CSL - https://github.com/oktal3700/bluebell
// These are Copyrighted by their authors and shouldn't be uploaded to any other sites.
//
// drive.usercontent.google.com with confirm=t, rather than the drive.google.com/uc
// form these used to use: Drive won't virus scan anything this big, so uc now answers
// with a 2.5kB "are you sure" page, which was being saved as the .zip and then failing
// to unzip. Only BB_GA_PNG, small enough to pass the scan, still worked
#define BB_AIRBUS_PNG   "https://drive.usercontent.google.com/download?export=download&confirm=t&id=10fFhflgWXCu7hmd8wqNdXw1qHJ6ecz9Z"
#define BB_BOEING_PNG   "https://drive.usercontent.google.com/download?export=download&confirm=t&id=1OA3pmAp5jqrjP7kRS1z_zNNyi_iLu9z_"
#define BB_GA_PNG       "https://drive.usercontent.google.com/download?export=download&confirm=t&id=1TZsvlLqT5x3KLkiqtN8LzAzoLxeYTA-1"
#define BB_HELI_PNG     "https://drive.usercontent.google.com/download?export=download&confirm=t&id=1qB2xDVHdooLeLKCPyVnVDDHRlhPVpUYs"
#define BB_JETS_PNG     "https://drive.usercontent.google.com/download?export=download&confirm=t&id=1v1fzTpyjjfcXyoT7vHjnyvuwqrSQzPrg"
#define BB_MIL_PNG      "https://drive.usercontent.google.com/download?export=download&confirm=t&id=1lI-2bAVVxhKvel7_suGVdkky4BQDQE9n"
#define BB_PROPS_PNG    "https://drive.usercontent.google.com/download?export=download&confirm=t&id=1fD8YxKsa9P_z2gL1aM97ZEN-HoI28SLE"
// Aircraft sounds: the engine sound sets and the single recordings in Sounds/aircraft.
// The archive holds the Sounds directory itself, so it unzips into the model directory
// just as the models do
#define SDRANGEL_SOUNDS "https://sdrangel.org/downloads/sounds.zip"

// Sizes as the archives were when this was written, so that the buttons can still say
// what a download will cost when the manifest can't be reached. The manifest's own
// sizes take over as soon as it arrives
static Asset3D makeAsset(const QString& name, const QString& group, const QString& description,
                         const QString& url, const QString& file, const QString& check, qint64 bytes)
{
    Asset3D asset;
    asset.m_name = name;
    asset.m_group = group;
    asset.m_description = description;
    asset.m_url = url;
    asset.m_file = file;
    asset.m_check = check;
    asset.m_bytes = bytes;
    return asset;
}

QList<Asset3D> Asset3DManifest::builtInAssets()
{
    QList<Asset3D> assets;

    assets.append(makeAsset("models", "models", "3D models", SDRANGEL_3D_MODELS, "sdrangel3dmodels.zip", "iss.glb", 751156395));
    assets.append(makeAsset("bb_airbus", "models", "Airbus textures", BB_AIRBUS_PNG, "bb_airbus_png.zip", "BB_Airbus_png", 236350359));
    assets.append(makeAsset("bb_boeing", "models", "Boeing textures", BB_BOEING_PNG, "bb_boeing_png.zip", "BB_Boeing_png", 306895690));
    assets.append(makeAsset("bb_ga", "models", "General aviation textures", BB_GA_PNG, "bb_ga_png.zip", "BB_GA_png", 5343246));
    assets.append(makeAsset("bb_heli", "models", "Helicopter textures", BB_HELI_PNG, "bb_heli_png.zip", "BB_Heli_png", 30776323));
    assets.append(makeAsset("bb_jets", "models", "Business jet textures", BB_JETS_PNG, "bb_jets_png.zip", "BB_Jets_png", 74919369));
    assets.append(makeAsset("bb_mil", "models", "Military textures", BB_MIL_PNG, "bb_mil_png.zip", "BB_Mil_png", 25764));
    assets.append(makeAsset("bb_props", "models", "Propeller aircraft textures", BB_PROPS_PNG, "bb_props_png.zip", "BB_Props_png", 88980206));
    assets.append(makeAsset("sounds", "sounds", "Aircraft sounds", SDRANGEL_SOUNDS, "sdrangelsounds.zip", "Sounds/aircraft", 12434726));

    return assets;
}

Asset3DManifest::Asset3DManifest() :
    m_assets(builtInAssets()),
    m_haveManifest(false)
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &Asset3DManifest::handleReply);
}

Asset3DManifest::~Asset3DManifest()
{
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &Asset3DManifest::handleReply);
    delete m_networkManager;
}

void Asset3DManifest::getData()
{
    QUrl url(m_manifestURL);
    QNetworkRequest request(url);

    // A few hundred bytes, read once a session, and the whole point is to see a new
    // release the day it is published - so don't let a proxy or the disk cache answer
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    m_networkManager->get(request);
}

void Asset3DManifest::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            if (parse(reply->readAll())) {
                emit dataUpdated(m_assets);
            }
        }
        else
        {
            // Not worth telling the user about: the built-in list still works, it just
            // has no versions, so nothing is reported as out of date
            qDebug() << "Asset3DManifest::handleReply: error: " << reply->errorString();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "Asset3DManifest::handleReply: reply is null";
    }
}

bool Asset3DManifest::parse(const QByteArray& bytes)
{
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &error);

    if (!document.isObject())
    {
        qWarning() << "Asset3DManifest::parse: Not a JSON object:" << error.errorString();
        return false;
    }

    const QJsonObject manifest = document.object();
    const int formatVersion = manifest.value(QStringLiteral("formatVersion")).toInt(ASSET3D_FORMAT_VERSION);

    if (formatVersion > ASSET3D_FORMAT_VERSION) {
        qInfo() << "Asset3DManifest::parse: Manifest format" << formatVersion
                << "is newer than this version of SDRangel understands - some asset details may be ignored";
    }

    if (!manifest.value(QStringLiteral("assets")).isArray())
    {
        qWarning() << "Asset3DManifest::parse: No assets array";
        return false;
    }

    QList<Asset3D> assets;

    for (auto valRef : manifest.value(QStringLiteral("assets")).toArray())
    {
        if (!valRef.isObject())
        {
            qWarning() << "Asset3DManifest::parse: Asset is not an object:" << valRef;
            continue;
        }

        const QJsonObject assetObj = valRef.toObject();
        Asset3D asset;

        asset.m_name = assetObj.value(QStringLiteral("name")).toString();
        asset.m_group = assetObj.value(QStringLiteral("group")).toString();
        asset.m_description = assetObj.value(QStringLiteral("description")).toString();
        asset.m_version = assetObj.value(QStringLiteral("version")).toString();
        asset.m_url = assetObj.value(QStringLiteral("url")).toString();
        asset.m_file = assetObj.value(QStringLiteral("file")).toString();
        // toDouble, as a JSON number big enough for an archive doesn't fit toInt
        asset.m_bytes = (qint64) assetObj.value(QStringLiteral("bytes")).toDouble(0.0);
        asset.m_notes = assetObj.value(QStringLiteral("notes")).toString();
        asset.m_check = assetObj.value(QStringLiteral("check")).toString();

        for (auto removeRef : assetObj.value(QStringLiteral("removes")).toArray())
        {
            const QString path = removeRef.toString();
            if (!path.isEmpty()) {
                asset.m_removes.append(path);
            }
        }

        // Without a name there is nowhere to record it, and without a URL there is
        // nothing to fetch, so neither is any use to us
        if (asset.m_name.isEmpty() || asset.m_url.isEmpty())
        {
            qWarning() << "Asset3DManifest::parse: Asset has no name or no url - ignoring:" << assetObj;
            continue;
        }
        if (asset.m_group.isEmpty()) {
            asset.m_group = QStringLiteral("models");
        }
        if (asset.m_file.isEmpty()) {
            asset.m_file = asset.m_name + ".zip";
        }
        if (asset.m_description.isEmpty()) {
            asset.m_description = asset.m_name;
        }

        assets.append(asset);
    }

    if (assets.isEmpty())
    {
        qWarning() << "Asset3DManifest::parse: No usable assets - keeping the built-in list";
        return false;
    }

    m_assets = assets;
    m_haveManifest = true;

    return true;
}

QList<Asset3D> Asset3DManifest::group(const QList<Asset3D>& assets, const QString& group)
{
    QList<Asset3D> matching;

    for (const auto& asset : assets)
    {
        if (asset.m_group == group) {
            matching.append(asset);
        }
    }

    return matching;
}

qint64 Asset3DManifest::totalBytes(const QList<Asset3D>& assets)
{
    qint64 total = 0;

    for (const auto& asset : assets) {
        total += asset.m_bytes;
    }

    return total;
}

QString Asset3DManifest::sizeString(const QList<Asset3D>& assets)
{
    for (const auto& asset : assets)
    {
        if (asset.m_bytes <= 0) {
            return QString();
        }
    }
    if (assets.isEmpty()) {
        return QString();
    }

    return QLocale().formattedDataSize(totalBytes(assets), 1, QLocale::DataSizeSIFormat);
}

Asset3DInstalled::Asset3DInstalled(const QString& dir) :
    m_dir(dir)
{
    load();
}

QString Asset3DInstalled::filename(const QString& dir)
{
    return dir + "/installed.json";
}

void Asset3DInstalled::load()
{
    QFile file(filename(m_dir));

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(file.readAll());

    if (!document.isObject())
    {
        qWarning() << "Asset3DInstalled::load: " << filename(m_dir) << " is not a JSON object";
        return;
    }

    const QJsonObject root = document.object();
    const QJsonObject assets = root.value(QStringLiteral("assets")).toObject();
    const QJsonObject notified = root.value(QStringLiteral("notified")).toObject();

    for (auto it = assets.begin(); it != assets.end(); ++it) {
        m_versions.insert(it.key(), it.value().toString());
    }
    for (auto it = notified.begin(); it != notified.end(); ++it) {
        m_notified.insert(it.key(), it.value().toString());
    }
}

bool Asset3DInstalled::save()
{
    QJsonObject assets;
    QJsonObject notified;

    for (auto it = m_versions.begin(); it != m_versions.end(); ++it) {
        assets.insert(it.key(), it.value());
    }
    for (auto it = m_notified.begin(); it != m_notified.end(); ++it) {
        notified.insert(it.key(), it.value());
    }

    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), ASSET3D_FORMAT_VERSION);
    root.insert(QStringLiteral("assets"), assets);
    root.insert(QStringLiteral("notified"), notified);

    QDir().mkpath(m_dir);

    QFile file(filename(m_dir));

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "Asset3DInstalled::save: Failed to write " << filename(m_dir) << ": " << file.errorString();
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

QString Asset3DInstalled::version(const QString& name) const
{
    return m_versions.value(name);
}

void Asset3DInstalled::setVersion(const QString& name, const QString& version)
{
    m_versions.insert(name, version);
}

QString Asset3DInstalled::notifiedVersion(const QString& name) const
{
    return m_notified.value(name);
}

void Asset3DInstalled::setNotifiedVersion(const QString& name, const QString& version)
{
    m_notified.insert(name, version);
}

bool Asset3DInstalled::isInstalled(const Asset3D& asset) const
{
    // A recorded version means we extracted it ourselves. The check path covers the
    // installs made before versions were recorded - which is every install so far
    if (m_versions.contains(asset.m_name)) {
        return true;
    }
    if (asset.m_check.isEmpty()) {
        return false;
    }

    return QFileInfo::exists(m_dir + "/" + asset.m_check);
}

QList<Asset3D> Asset3DInstalled::stale(const QList<Asset3D>& assets) const
{
    QList<Asset3D> out;

    for (const auto& asset : assets)
    {
        // Nothing to compare against until the manifest has been read
        if (asset.m_version.isEmpty()) {
            continue;
        }
        if (version(asset.m_name) != asset.m_version) {
            out.append(asset);
        }
    }

    return out;
}

// Only a path that stays inside the model directory, so a mistake or a tampered-with
// manifest can't reach anything else. Relative, no .. segments, no drive or scheme
static bool safeRemovePath(const QString& path)
{
    const QString cleaned = QDir::fromNativeSeparators(path);

    if (cleaned.isEmpty() || cleaned.startsWith('/') || cleaned.contains(':')) {
        return false;
    }
    for (const QString& part : cleaned.split('/'))
    {
        if (part == QStringLiteral("..")) {
            return false;
        }
    }

    return true;
}

int Asset3DInstalled::applyRemoves(const QString& dir, const Asset3D& asset)
{
    int removed = 0;

    // Deliberately only the paths the manifest names. The directory also holds whatever
    // models the user has added - and their MSFS conversions - so nothing is ever
    // cleared out wholesale
    for (const QString& path : asset.m_removes)
    {
        if (!safeRemovePath(path))
        {
            qWarning() << "Asset3DInstalled::applyRemoves: Refusing to remove " << path << " for asset " << asset.m_name;
            continue;
        }

        // A trailing / says the manifest meant a directory, so a file is never taken for
        // one and removed with everything beside it
        const bool isDir = path.endsWith('/');
        const QString relative = isDir ? path.left(path.size() - 1) : path;
        const QString target = QDir::cleanPath(dir + "/" + relative);

        if (!target.startsWith(QDir::cleanPath(dir) + "/"))
        {
            qWarning() << "Asset3DInstalled::applyRemoves: " << target << " is outside " << dir << " - not removing";
            continue;
        }

        QFileInfo info(target);

        if (!info.exists()) {
            continue;
        }
        if (isDir != info.isDir())
        {
            qWarning() << "Asset3DInstalled::applyRemoves: " << target
                       << (info.isDir() ? " is a directory but the manifest named a file" : " is a file but the manifest named a directory")
                       << " - not removing";
            continue;
        }

        const bool ok = isDir ? QDir(target).removeRecursively() : QFile::remove(target);

        if (ok)
        {
            qDebug() << "Asset3DInstalled::applyRemoves: Removed " << target << " for asset " << asset.m_name;
            removed++;
        }
        else
        {
            qWarning() << "Asset3DInstalled::applyRemoves: Failed to remove " << target;
        }
    }

    return removed;
}
