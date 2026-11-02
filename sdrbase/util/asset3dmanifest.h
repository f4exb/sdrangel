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

#ifndef INCLUDE_ASSET3DMANIFEST_H
#define INCLUDE_ASSET3DMANIFEST_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// One downloadable archive of 3D map assets: the models, a texture pack, or the sounds.
// Each is versioned on its own, so a new release of one doesn't make users fetch the rest
struct SDRBASE_API Asset3D
{
    QString m_name;         // Stable key, recorded on disk. Never shown to the user
    QString m_group;        // Which download button it belongs to: "models" or "sounds"
    QString m_description;  // For the UI, e.g. "3D models"
    QString m_version;      // Opaque - only ever compared for equality. Empty if unknown
    QString m_url;
    QString m_file;         // What the download is saved as
    qint64 m_bytes = 0;     // 0 if unknown, in which case no size is shown
    QString m_notes;        // What changed in this version, for the prompt and tooltip
    // A path, relative to the model directory, that exists once this asset has been
    // extracted. Only used to tell a first download from an update, so it needs to be
    // something long lived rather than exhaustive
    QString m_check;
    // Files and directories this version of the asset drops, relative to the model
    // directory. A trailing / means a directory, removed recursively. Only the paths
    // named here are deleted, so models the user has added are never touched
    QStringList m_removes;
};

// The list of 3D map assets and their current versions, from sdrangel.org.
//
// The archives are hosted in three different places and only some of them return usable
// ETag or Last-Modified headers - the Google Drive hosted texture packs return none at
// all - so the versions can't come from the downloads themselves. Nor is age any use
// here: these change once every few years, unlike the databases HttpDownloadManager
// refreshes by file age.
//
// Until the manifest arrives - or if it never does - assets() is the list built in to
// this class, with no versions, so the download buttons work offline exactly as they did
// before there was a manifest
class SDRBASE_API Asset3DManifest : public QObject
{
    Q_OBJECT

public:

    Asset3DManifest();
    ~Asset3DManifest();

    void getData();
    bool hasManifest() const { return m_haveManifest; }
    const QList<Asset3D>& assets() const { return m_assets; }

    // Read a manifest. Public so that one from somewhere other than the download - a
    // local file, a test - can be used in its place
    bool parse(const QByteArray& bytes);

    static const QString m_manifestURL;
    static QList<Asset3D> builtInAssets();
    static QList<Asset3D> group(const QList<Asset3D>& assets, const QString& group);
    static qint64 totalBytes(const QList<Asset3D>& assets);
    // Empty when any of the assets has an unknown size, as a part total would mislead
    static QString sizeString(const QList<Asset3D>& assets);

public slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const QList<Asset3D>& assets);  // Emitted when the manifest is read

private:
    QNetworkAccessManager *m_networkManager;
    QList<Asset3D> m_assets;
    bool m_haveManifest;
};

// What is actually in the model directory. Recorded alongside the assets rather than in
// the Map settings, so that it describes the directory itself: it survives a settings
// reset, and is discarded when the user deletes the directory
class SDRBASE_API Asset3DInstalled
{
public:

    explicit Asset3DInstalled(const QString& dir);

    QString version(const QString& name) const;
    void setVersion(const QString& name, const QString& version);
    // The version the user has already been prompted about, so each release is only
    // mentioned once however many times SDRangel is restarted
    QString notifiedVersion(const QString& name) const;
    void setNotifiedVersion(const QString& name, const QString& version);
    bool save();

    bool isInstalled(const Asset3D& asset) const;
    // Assets whose installed version differs from the manifest's. An asset the manifest
    // gives no version for is never stale, as there is nothing to compare it against
    QList<Asset3D> stale(const QList<Asset3D>& assets) const;

    static QString filename(const QString& dir);
    // Delete the paths this version of the asset drops. Returns how many went
    static int applyRemoves(const QString& dir, const Asset3D& asset);

private:
    QString m_dir;
    QHash<QString, QString> m_versions;
    QHash<QString, QString> m_notified;

    void load();
};

#endif /* INCLUDE_ASSET3DMANIFEST_H */
