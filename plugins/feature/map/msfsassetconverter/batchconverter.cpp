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

#include "batchconverter.h"

#include "converter.h"
#include "fsltlresolver.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <atomic>
#include <exception>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace {

struct AircraftTask {
    QString m_config;
    QString m_outputDirectory;
    QString m_relativeConfig;
    QString m_aircraftName;
    QString m_outputNameSuffix;
    int m_flightSimIndex = 0;
};

struct AircraftOutcome {
    enum Status {
        Pending,
        Converted,
        Failed,
        Cancelled
    };

    Status m_status = Pending;
    QString m_failure;
    int m_reusedTextures = 0;
    int m_downscaledTextures = 0;
    int m_synthesizedTextures = 0;
    int m_skippedFallbackDirectories = 0;
    bool m_lowDetail = false;
};

// The model the 3D map draws an aircraft with once it is small on screen: the
// second LOD of the model definition, when that is a different file from the
// first. Returns an empty string when there is none
QString lowDetailLodFile(const AircraftTask& task)
{
    ResolvedFsltlAircraft lod0;
    ResolvedFsltlAircraft lod1;
    QString error;
    if (!resolveFsltlAircraft(task.m_config, task.m_flightSimIndex, 0, &lod0, &error)
        || !resolveFsltlAircraft(task.m_config, task.m_flightSimIndex, 1, &lod1, &error)) {
        return {};
    }
    if (QFileInfo(lod0.m_gltfFile).canonicalFilePath()
        == QFileInfo(lod1.m_gltfFile).canonicalFilePath()) {
        return {};
    }
    return lod1.m_gltfFile;
}

int automaticWorkerCount(int taskCount)
{
    const int idealThreadCount = QThread::idealThreadCount();
    // UASTC encoding is CPU- and memory-intensive. Approximate physical cores
    // on SMT systems and cap the default to keep SDRangel responsive.
    const int cpuWorkers = idealThreadCount > 1 ? idealThreadCount / 2 : 1;
    return qMin(taskCount, qBound(1, cpuWorkers, 8));
}

QString aircraftFamily(const QString& aircraftName)
{
    const QStringList parts = aircraftName.split('_');
    if ((parts.size() >= 2) &&
        (parts[0].compare(QStringLiteral("FSLTL"), Qt::CaseInsensitive) == 0)) {
        return parts[0].toUpper() + '_' + parts[1].toUpper();
    }
    return aircraftName.toUpper();
}

// The shared texture cache is content addressed: a texture is named by a hash of its source
// and of how it was encoded, so a change of encoding (zstd, say) writes every texture afresh
// under a new name, and nothing ever removed the old ones. After a complete run, cache files
// that no model below the output directory refers to are deleted. Only files with cache names
// are touched, and nothing at all if any model can't be read, so a model never loses a texture
bool removeUnreferencedTextures(const QDir& outputRoot, const QDir& textureDirectory,
                                int *removed, qint64 *removedBytes, QString *error)
{
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    const QString texturePath = QDir::cleanPath(textureDirectory.absolutePath());
    QSet<QString> referenced;
    QDirIterator models(outputRoot.absolutePath(), {QStringLiteral("*.gltf")},
        QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (models.hasNext())
    {
        const QString model = models.next();
        QFile file(model);
        if (!file.open(QIODevice::ReadOnly))
        {
            *error = QStringLiteral("Cannot read %1: %2").arg(model, file.errorString());
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (!document.isObject())
        {
            *error = QStringLiteral("Cannot parse %1: %2").arg(model, parseError.errorString());
            return false;
        }
        const QDir modelDirectory = QFileInfo(model).absoluteDir();
        for (const QJsonValue& image : document.object().value("images").toArray())
        {
            const QString uri = image.toObject().value("uri").toString();
            if (uri.isEmpty() || uri.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
                continue;
            }
            const QFileInfo target(QDir::cleanPath(
                modelDirectory.absoluteFilePath(QUrl::fromPercentEncoding(uri.toUtf8()))));
            if (target.absolutePath().compare(texturePath, pathCase) == 0) {
                referenced.insert(target.fileName().toLower());
            }
        }
    }

    // What cachedKtxName and cachedImageName produce, and the checksum written beside them
    static const QRegularExpression cacheName(QStringLiteral("^[0-9a-f]{64}(\\.[0-9a-z]+)?$"));
    const QFileInfoList files = textureDirectory.entryInfoList(QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo& file : files)
    {
        const QString name = file.fileName().toLower();
        const bool checksum = name.endsWith(QStringLiteral(".sha256"));
        const QString owner = checksum ? name.chopped(7) : name;
        if (!cacheName.match(owner).hasMatch() || referenced.contains(owner)) {
            continue;
        }
        const qint64 size = file.size();
        if (QFile::remove(file.absoluteFilePath()))
        {
            *removedBytes += size;
            if (!checksum) {
                ++*removed;
            }
        }
    }
    return true;
}

} // namespace

bool convertMSFSAircraftDirectory(
    const QString& aircraftDirectory,
    const QString& outputDirectory,
    MSFSBatchConversionSummary *summary,
    QString *error,
    const MSFSBatchProgress& progress,
    const MSFSBatchCancelled& isCancelled,
    int maxThreadCount,
    int maximumTextureSize,
    int lowDetailMaximumTextureSize)
{
    *summary = {};
    error->clear();

    const QFileInfo inputInfo(aircraftDirectory);
    if (!inputInfo.isDir())
    {
        *error = QStringLiteral("Aircraft directory does not exist: %1").arg(aircraftDirectory);
        return false;
    }

    const QDir inputRoot(inputInfo.absoluteFilePath());
    if (progress) {
        progress(0, 0, QStringLiteral("Scanning for aircraft.cfg files…"));
    }

    QStringList aircraftConfigs;
    QDirIterator iterator(
        inputRoot.absolutePath(),
        {QStringLiteral("aircraft.cfg")},
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        if (isCancelled && isCancelled())
        {
            summary->m_cancelled = true;
            return true;
        }
        aircraftConfigs.append(iterator.next());
    }
    std::sort(aircraftConfigs.begin(), aircraftConfigs.end(),
        [](const QString& left, const QString& right) {
            return left.compare(right, Qt::CaseInsensitive) < 0;
        });

    if (aircraftConfigs.isEmpty())
    {
        *error = QStringLiteral("No aircraft.cfg files were found below %1")
            .arg(inputRoot.absolutePath());
        return false;
    }
    const QDir outputRoot(outputDirectory);
    std::vector<AircraftTask> tasks;
    tasks.reserve(static_cast<std::size_t>(aircraftConfigs.size()));
    for (const QString& config : aircraftConfigs)
    {
        const QFileInfo configInfo(config);
        const QString relativeDirectory = inputRoot.relativeFilePath(configInfo.absolutePath());
        QVector<int> flightSimIndices;
        QString variantError;
        if (!findFsltlAircraftVariants(config, &flightSimIndices, &variantError)) {
            // Let the normal conversion path report the detailed failure.
            flightSimIndices.append(0);
        }
        const bool multipleVariants = flightSimIndices.size() > 1;
        for (int flightSimIndex : flightSimIndices) {
            tasks.push_back({
                config,
                outputRoot.filePath(relativeDirectory),
                inputRoot.relativeFilePath(config),
                configInfo.dir().dirName(),
                multipleVariants
                    ? QStringLiteral("_fltsim_%1").arg(flightSimIndex)
                    : QString(),
                flightSimIndex
            });
        }
    }
    const int configurationCount = aircraftConfigs.size();
    summary->m_discovered = static_cast<int>(tasks.size());
    if (progress) {
        progress(0, summary->m_discovered,
            QStringLiteral("Found %1 aircraft configurations containing %2 distinct variants")
                .arg(configurationCount)
                .arg(summary->m_discovered));
    }

    // Interleave aircraft families so the first worker wave does not consist
    // entirely of liveries for one type, all waiting on the same base texture.
    std::map<QString, std::vector<AircraftTask>> aircraftFamilies;
    for (AircraftTask& task : tasks) {
        aircraftFamilies[aircraftFamily(task.m_aircraftName)].push_back(std::move(task));
    }
    const std::size_t taskCount = tasks.size();
    tasks.clear();
    tasks.reserve(taskCount);
    for (std::size_t familyIndex = 0;
        tasks.size() < taskCount;
        ++familyIndex)
    {
        for (auto& family : aircraftFamilies)
        {
            if (familyIndex < family.second.size()) {
                tasks.push_back(std::move(family.second[familyIndex]));
            }
        }
    }

    summary->m_workerCount = maxThreadCount > 0
        ? qMin(maxThreadCount, static_cast<int>(tasks.size()))
        : automaticWorkerCount(static_cast<int>(tasks.size()));
    summary->m_workerCount = qMax(1, summary->m_workerCount);
    if (progress) {
        progress(0, summary->m_discovered,
            QStringLiteral(
                "Converting with %1 parallel worker%2; shared textures are cached in MSFS/_textures")
                .arg(summary->m_workerCount)
                .arg(summary->m_workerCount == 1 ? QString() : QStringLiteral("s")));
    }

    std::vector<AircraftOutcome> outcomes(tasks.size());
    std::atomic<int> nextTask{0};
    std::atomic<int> completedTasks{0};
    std::atomic_bool stopRequested{false};
    std::mutex progressMutex;
    const QString sharedTextureDirectory = outputRoot.filePath(QStringLiteral("_textures"));
    if (!QDir().mkpath(sharedTextureDirectory))
    {
        *error = QStringLiteral("Cannot create shared texture directory: %1")
            .arg(sharedTextureDirectory);
        return false;
    }
    const auto cancelled = [&] {
        return stopRequested.load(std::memory_order_relaxed) ||
            (isCancelled && isCancelled());
    };
    const auto worker = [&] {
        while (!cancelled())
        {
            const int index = nextTask.fetch_add(1, std::memory_order_relaxed);
            if (index >= static_cast<int>(tasks.size())) {
                return;
            }

            const AircraftTask& task = tasks[static_cast<std::size_t>(index)];
            ConversionOptions options;
            options.m_inputGltf = task.m_config;
            options.m_outputDirectory = task.m_outputDirectory;
            options.m_outputNameSuffix = task.m_outputNameSuffix;
            options.m_sharedTextureDirectory = sharedTextureDirectory;
            options.m_flightSimIndex = task.m_flightSimIndex;
            options.m_maximumTextureSize = maximumTextureSize;
            options.m_isCancelled = cancelled;

            ConversionResult conversion;
            QString conversionError;
            AircraftOutcome& outcome = outcomes[static_cast<std::size_t>(index)];
            QString message;
            try
            {
                if (convertMsfsAsset(options, &conversion, &conversionError))
                {
                    outcome.m_status = AircraftOutcome::Converted;
                    outcome.m_reusedTextures = conversion.m_reusedTextures;
                    outcome.m_downscaledTextures = conversion.m_downscaledTextures;
                    outcome.m_synthesizedTextures = conversion.m_synthesizedTextures;
                    outcome.m_skippedFallbackDirectories =
                        conversion.m_skippedFallbackDirectories;
                    message = QStringLiteral("Converted %1%2")
                        .arg(task.m_aircraftName, task.m_outputNameSuffix);

                    // A cheaper model for the map to switch to when the aircraft is
                    // small on screen. It shares the textures just cached
                    const QString lowDetailFile = QFileInfo(conversion.m_outputGltf).path()
                        + "/" + QFileInfo(conversion.m_outputGltf).completeBaseName()
                        + QStringLiteral("_lod1.gltf");
                    if (!lowDetailLodFile(task).isEmpty())
                    {
                        ConversionOptions lowDetailOptions = options;
                        lowDetailOptions.m_lodIndex = 1;
                        // Its own, smaller textures. They stop being shared with the full
                        // detail model, so both are resident while a swap is in flight -
                        // which at a third of a megabyte each costs nothing. The cap is
                        // part of the KTX2 cache fingerprint, so these get their own files
                        lowDetailOptions.m_maximumTextureSize =
                            (lowDetailMaximumTextureSize > 0)
                                ? qMin(lowDetailMaximumTextureSize, maximumTextureSize)
                                : maximumTextureSize;
                        // A low detail model is only drawn small, so its animations can be
                        // thinned harder than the full one's: about a degree of rotation
                        lowDetailOptions.m_animationTolerance =
                            qMax(options.m_animationTolerance, 0.01);
                        lowDetailOptions.m_outputFileSuffix = QStringLiteral("_lod1");
                        ConversionResult lowDetailConversion;
                        QString lowDetailError;
                        if (convertMsfsAsset(lowDetailOptions, &lowDetailConversion, &lowDetailError)) {
                            outcome.m_lowDetail = true;
                        } else if (lowDetailError == QStringLiteral("Conversion cancelled")) {
                            QFile::remove(lowDetailFile);
                            outcome.m_status = AircraftOutcome::Cancelled;
                            return;
                        } else {
                            QFile::remove(lowDetailFile);
                            message += QStringLiteral(" (low detail model failed: %1)").arg(lowDetailError);
                        }
                    }
                    else
                    {
                        // Don't leave one from an earlier run that no longer matches
                        QFile::remove(lowDetailFile);
                    }
                    if (outcome.m_lowDetail) {
                        message += QStringLiteral(" with a low detail model");
                    }
                    if (conversion.m_reusedTextures > 0) {
                        message += QStringLiteral(" (%1 cached texture%2 reused)")
                            .arg(conversion.m_reusedTextures)
                            .arg(conversion.m_reusedTextures == 1 ? QString() : QStringLiteral("s"));
                    }
                    if (conversion.m_synthesizedTextures > 0) {
                        message += QStringLiteral(" (%1 missing optional texture%2 replaced)")
                            .arg(conversion.m_synthesizedTextures)
                            .arg(conversion.m_synthesizedTextures == 1
                                ? QString() : QStringLiteral("s"));
                    }
                    if (conversion.m_skippedFallbackDirectories > 0) {
                        message += QStringLiteral(" (%1 missing fallback director%2 skipped)")
                            .arg(conversion.m_skippedFallbackDirectories)
                            .arg(conversion.m_skippedFallbackDirectories == 1
                                ? QStringLiteral("y") : QStringLiteral("ies"));
                    }
                }
                else if (conversionError == QStringLiteral("Conversion cancelled"))
                {
                    outcome.m_status = AircraftOutcome::Cancelled;
                    return;
                }
                else
                {
                    outcome.m_status = AircraftOutcome::Failed;
                    outcome.m_failure = QStringLiteral("%1 [FLTSIM.%2]: %3")
                        .arg(task.m_relativeConfig)
                        .arg(task.m_flightSimIndex)
                        .arg(conversionError);
                    message = QStringLiteral("Failed: %1").arg(outcome.m_failure);
                }
            }
            catch (const std::exception& exception)
            {
                outcome.m_status = AircraftOutcome::Failed;
                outcome.m_failure = QStringLiteral("%1 [FLTSIM.%2]: unexpected converter exception: %3")
                    .arg(task.m_relativeConfig)
                    .arg(task.m_flightSimIndex)
                    .arg(QString::fromLocal8Bit(exception.what()));
                message = QStringLiteral("Failed: %1").arg(outcome.m_failure);
            }
            catch (...)
            {
                outcome.m_status = AircraftOutcome::Failed;
                outcome.m_failure = QStringLiteral("%1 [FLTSIM.%2]: unexpected converter exception")
                    .arg(task.m_relativeConfig)
                    .arg(task.m_flightSimIndex);
                message = QStringLiteral("Failed: %1").arg(outcome.m_failure);
            }

            {
                const std::lock_guard<std::mutex> guard(progressMutex);
                const int completed =
                    completedTasks.fetch_add(1, std::memory_order_relaxed) + 1;
                if (progress) {
                    progress(completed, summary->m_discovered, message);
                }
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(summary->m_workerCount - 1));
    try
    {
        for (int index = 1; index < summary->m_workerCount; ++index) {
            workers.emplace_back(worker);
        }
    }
    catch (const std::exception& exception)
    {
        stopRequested.store(true, std::memory_order_relaxed);
        for (std::thread& thread : workers) {
            thread.join();
        }
        *error = QStringLiteral("Cannot start MSFS conversion workers: %1")
            .arg(QString::fromLocal8Bit(exception.what()));
        return false;
    }

    worker();
    for (std::thread& thread : workers) {
        thread.join();
    }

    for (const AircraftOutcome& outcome : outcomes)
    {
        if (outcome.m_status == AircraftOutcome::Converted)
        {
            ++summary->m_converted;
            if (outcome.m_lowDetail) {
                ++summary->m_lowDetailModels;
            }
            summary->m_reusedTextures += outcome.m_reusedTextures;
            summary->m_downscaledTextures += outcome.m_downscaledTextures;
            summary->m_synthesizedTextures += outcome.m_synthesizedTextures;
            summary->m_skippedFallbackDirectories += outcome.m_skippedFallbackDirectories;
        }
        else if (outcome.m_status == AircraftOutcome::Failed)
        {
            ++summary->m_failed;
            summary->m_errors.append(outcome.m_failure);
        }
        else if (outcome.m_status == AircraftOutcome::Cancelled)
        {
            summary->m_cancelled = true;
        }
    }
    summary->m_cancelled = summary->m_cancelled || (isCancelled && isCancelled());

    if (!summary->m_cancelled)
    {
        if (progress) {
            progress(summary->m_discovered, summary->m_discovered,
                QStringLiteral("Removing cached textures no model uses…"));
        }
        QString cleanupError;
        if (!removeUnreferencedTextures(outputRoot, QDir(sharedTextureDirectory),
                &summary->m_removedTextures, &summary->m_removedTextureBytes, &cleanupError)
            && progress)
        {
            progress(summary->m_discovered, summary->m_discovered,
                QStringLiteral("WARNING: unused textures not removed: %1").arg(cleanupError));
        }
    }

    return true;
}
