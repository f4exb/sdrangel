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

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QThread>

#include <functional>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"

#include "webapi/webapiadapterinterface.h"
#include "device/deviceset.h"
#include "maincore.h"

#include "mcperror.h"
#include "mcpcapture.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// Where the file system really puts an existing path, with every symbolic link and junction on
// the way resolved. Qt's canonicalFilePath does not see through a junction on Windows, which is
// the easiest of them to make, so the final path is asked of the system there
QString realPath(const QString& path)
{
#ifdef Q_OS_WIN
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return QString();
    }

    wchar_t buffer[32768];
    DWORD length = GetFinalPathNameByHandleW(handle, buffer, sizeof(buffer) / sizeof(buffer[0]), FILE_NAME_NORMALIZED);
    CloseHandle(handle);

    if ((length == 0) || (length >= sizeof(buffer) / sizeof(buffer[0]))) {
        return QString();
    }

    QString result = QString::fromWCharArray(buffer, length);

    if (result.startsWith("\\\\?\\UNC\\")) {
        result = "\\\\" + result.mid(8);
    } else if (result.startsWith("\\\\?\\")) {
        result = result.mid(4);
    }

    return QDir::cleanPath(QDir::fromNativeSeparators(result));
#else
    return QFileInfo(path).canonicalFilePath();
#endif
}

QJsonObject toJson(SWGSDRangel::SWGObject& object)
{
    QJsonObject *obj = object.asJsonObject();
    QJsonObject result = *obj;
    delete obj;
    return result;
}

void check(int httpRC, SWGSDRangel::SWGErrorResponse& error, const QString& what)
{
    if (httpRC / 100 != 2)
    {
        QString message = error.getMessage() ? *error.getMessage() : QString();
        throw MCPToolError(QString("%1 failed (HTTP %2)%3").arg(what).arg(httpRC).arg(message.isEmpty() ? "" : ": " + message));
    }
}

bool waitFor(const std::function<bool()>& condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (!condition())
    {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }

        QThread::msleep(20);
    }

    return true;
}

// Settings key holding the type specific settings, e.g. "fileSinkSettings"
QString settingsKeyOf(const QJsonObject& json)
{
    for (const QString& key : json.keys())
    {
        if (key.endsWith("Settings") && json[key].isObject()) {
            return key;
        }
    }

    return QString();
}

} // namespace

MCPCapture::MCPCapture(WebAPIAdapterInterface *adapter) :
    m_adapter(adapter),
    m_audio(adapter)
{
}

QJsonObject MCPCapture::captureAudio(int deviceSetIndex, int channelIndex, double seconds,
    const QString& fileName, bool inlineAudio)
{
    QString path = resolvePath(fileName, "audio");
    return m_audio.capture(deviceSetIndex, channelIndex, seconds, path, inlineAudio);
}

void MCPCapture::setCaptureDir(const QString& dir)
{
    QMutexLocker locker(&m_mutex);
    m_captureDir = dir;
}

QString MCPCapture::getCaptureDir() const
{
    QMutexLocker locker(&m_mutex);
    return m_captureDir;
}

// Turns a caller supplied name into an absolute path inside the capture directory.
// Relative sub-directories are allowed; anything that escapes the capture directory is refused.
QString MCPCapture::resolvePath(const QString& nameIn, const QString& defaultPrefix)
{
    QString captureDir = getCaptureDir();

    if (captureDir.trimmed().isEmpty()) {
        throw MCPToolError("No capture directory is configured for the MCP Server feature");
    }

    QDir base(captureDir);

    if (!base.exists() && !QDir().mkpath(base.absolutePath())) {
        throw MCPToolError(QString("Cannot create the capture directory %1").arg(base.absolutePath()));
    }

    QString name = nameIn.trimmed();

    if (name.isEmpty()) {
        name = QString("%1_%2").arg(defaultPrefix).arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss"));
    }

    name.replace('\\', '/');
    QFileInfo info(name);

    if (info.isAbsolute()) {
        throw MCPToolError(QString("File name must be relative to the capture directory %1").arg(base.absolutePath()));
    }

    QString baseAbs = QDir::cleanPath(base.absolutePath());
    QString full = QDir::cleanPath(base.absoluteFilePath(name));

    if ((full != baseAbs) && !full.startsWith(baseAbs + "/")) {
        throw MCPToolError(QString("File name must stay inside the capture directory %1").arg(baseAbs));
    }

    // The text can stay inside while the file does not: a symbolic link or junction under the
    // capture directory leads elsewhere. Judge by where the nearest existing ancestor really
    // is, since the directories below it are about to be created as ordinary ones
    QString baseReal = realPath(baseAbs);
    QString ancestor = QFileInfo(full).path();

    while (!QFileInfo::exists(ancestor))
    {
        QString up = QFileInfo(ancestor).path();

        if (up == ancestor) {
            break;
        }

        ancestor = up;
    }

    QString ancestorReal = realPath(ancestor);

    if (baseReal.isEmpty() || ancestorReal.isEmpty()
        || ((ancestorReal != baseReal) && !ancestorReal.startsWith(baseReal + "/")))
    {
        throw MCPToolError(QString("File name must stay inside the capture directory %1; %2 leads outside it")
            .arg(baseAbs).arg(ancestor));
    }

    // SDRangel appends its own timestamp and extension, so strip a supplied one
    if (full.endsWith(".sdriq", Qt::CaseInsensitive)) {
        full.chop(QString(".sdriq").size());
    }

    QDir parent = QFileInfo(full).dir();

    if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
        throw MCPToolError(QString("Cannot create the directory %1").arg(parent.absolutePath()));
    }

    return full;
}

QJsonObject MCPCapture::channelSettings(int deviceSetIndex, int channelIndex)
{
    SWGSDRangel::SWGChannelSettings response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelSettingsGet(deviceSetIndex, channelIndex, response, error), error,
        QString("Get settings of channel %1:%2").arg(deviceSetIndex).arg(channelIndex));
    return toJson(response);
}

QString MCPCapture::channelType(const QJsonObject& settings)
{
    return settings["channelType"].toString();
}

void MCPCapture::patchChannel(int deviceSetIndex, int channelIndex, const QString& type, const QJsonObject& partial)
{
    QJsonObject json = channelSettings(deviceSetIndex, channelIndex);
    QString key = settingsKeyOf(json);

    if (key.isEmpty()) {
        throw MCPToolError(QString("Cannot find the settings of channel %1:%2").arg(deviceSetIndex).arg(channelIndex));
    }

    QJsonObject sub = json[key].toObject();
    QStringList keys;

    for (const QString& k : partial.keys())
    {
        sub[k] = partial[k];
        keys.append(k);
    }

    json[key] = sub;
    json["channelType"] = type;

    SWGSDRangel::SWGChannelSettings request;
    request.fromJsonObject(json);
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelSettingsPutPatch(deviceSetIndex, channelIndex, false, keys, request, error), error,
        QString("Set settings of channel %1:%2").arg(deviceSetIndex).arg(channelIndex));
}

void MCPCapture::recordAction(int deviceSetIndex, int channelIndex, const QString& type, bool record)
{
    QJsonObject actions;
    actions["record"] = record ? 1 : 0;
    QJsonObject json;
    json["channelType"] = type;
    json["direction"] = 0;
    json[type + "Actions"] = actions;

    SWGSDRangel::SWGChannelActions query;
    query.fromJsonObject(json);
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelActionsPost(deviceSetIndex, channelIndex, QStringList{"record"}, query, response, error), error,
        record ? "Start recording" : "Stop recording");
}

QJsonObject MCPCapture::channelReport(int deviceSetIndex, int channelIndex)
{
    SWGSDRangel::SWGChannelReport response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelReportGet(deviceSetIndex, channelIndex, response, error), error,
        QString("Get report of channel %1:%2").arg(deviceSetIndex).arg(channelIndex));
    return toJson(response);
}

const void *MCPCapture::channelObject(int deviceSetIndex, int channelIndex)
{
    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    if ((deviceSetIndex < 0) || (deviceSetIndex >= (int) deviceSets.size())) {
        return nullptr;
    }

    return deviceSets[deviceSetIndex]->getChannelAt(channelIndex);
}

bool MCPCapture::locateChannel(const void *channel, int& deviceSetIndex, int& channelIndex)
{
    if (!channel) {
        return false;
    }

    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    for (int d = 0; d < (int) deviceSets.size(); d++)
    {
        for (int c = 0; c < deviceSets[d]->getNumberOfChannels(); c++)
        {
            if (deviceSets[d]->getChannelAt(c) == channel)
            {
                deviceSetIndex = d;
                channelIndex = c;
                return true;
            }
        }
    }

    return false;
}

// A channel deleted without stopping its recording would otherwise leave an entry behind for
// as long as the server runs, and a later channel could be allocated at the same address
void MCPCapture::pruneRecordings()
{
    QMutexLocker locker(&m_mutex);

    for (auto it = m_recordings.begin(); it != m_recordings.end(); )
    {
        int deviceSetIndex = -1;
        int channelIndex = -1;

        if (locateChannel(it.key(), deviceSetIndex, channelIndex)) {
            ++it;
        } else {
            it = m_recordings.erase(it);
        }
    }
}

void MCPCapture::forgetRecording(const void *channel)
{
    QMutexLocker locker(&m_mutex);
    m_recordings.remove(channel);
}

QJsonObject MCPCapture::startIQRecording(int deviceSetIndex, int channelIndex, const QString& fileName,
    int frequencyOffset, int log2Decim)
{
    const void *channel = channelObject(deviceSetIndex, channelIndex);

    if (!channel) {
        throw MCPToolError(QString("There is no channel %1:%2").arg(deviceSetIndex).arg(channelIndex));
    }

    pruneRecordings();
    QJsonObject settings = channelSettings(deviceSetIndex, channelIndex);
    QString type = channelType(settings);

    if ((type != "FileSink") && (type != "SigMFFileSink")) {
        throw MCPToolError(QString("Channel %1:%2 is a %3, not a FileSink").arg(deviceSetIndex).arg(channelIndex).arg(type));
    }

    QString fileBase = resolvePath(fileName, "iq");
    QJsonObject partial;
    partial["fileRecordName"] = fileBase + ".sdriq";
    partial["inputFrequencyOffset"] = frequencyOffset;

    if (log2Decim >= 0) {
        partial["log2Decim"] = log2Decim;
    }

    patchChannel(deviceSetIndex, channelIndex, type, partial);

    // The record action goes straight to the channel's DSP thread while the settings go through
    // the channel itself first, so the action can overtake them and start recording to the
    // file name the sink had before: for a fresh FileSink an empty one, which gives a stray
    // file in the working directory. Wait until the channel has taken the settings, after
    // which the DSP thread sees them before the action
    QElapsedTimer settled;
    settled.start();

    while (settled.elapsed() < 3000)
    {
        QJsonObject now = channelSettings(deviceSetIndex, channelIndex);
        QString key = settingsKeyOf(now);

        if (!key.isEmpty() && (now[key].toObject()["fileRecordName"].toString() == partial["fileRecordName"].toString())) {
            break;
        }

        QThread::msleep(50);
    }

    Recording recording;
    recording.m_fileBase = fileBase;
    recording.m_started = QDateTime::currentDateTime();
    recording.m_valid = true;

    {
        QMutexLocker locker(&m_mutex);
        m_recordings[channel] = recording;
    }

    recordAction(deviceSetIndex, channelIndex, type, true);

    QJsonObject result;
    result["deviceSetIndex"] = deviceSetIndex;
    result["channelIndex"] = channelIndex;
    result["channelType"] = type;
    result["fileBase"] = fileBase;
    result["note"] = "SDRangel appends a timestamp and the .sdriq extension to the file name. "
                     "Call stop_iq_recording to stop and list the files written.";
    return result;
}

QJsonObject MCPCapture::stopIQRecording(int deviceSetIndex, int channelIndex)
{
    const void *channel = channelObject(deviceSetIndex, channelIndex);

    if (!channel) {
        throw MCPToolError(QString("There is no channel %1:%2").arg(deviceSetIndex).arg(channelIndex));
    }

    return stopIQRecording(channel);
}

QJsonObject MCPCapture::stopIQRecording(const void *channel)
{
    int deviceSetIndex = -1;
    int channelIndex = -1;

    // Another request can delete a channel between any two steps here, renumbering the ones
    // above it, so the index is resolved from the channel itself before each one
    auto locate = [&]()
    {
        if (!locateChannel(channel, deviceSetIndex, channelIndex)) {
            throw MCPToolError("The channel that was recording no longer exists");
        }
    };

    locate();
    QJsonObject settings = channelSettings(deviceSetIndex, channelIndex);
    QString type = channelType(settings);

    if ((type != "FileSink") && (type != "SigMFFileSink")) {
        throw MCPToolError(QString("Channel %1:%2 is a %3, not a FileSink").arg(deviceSetIndex).arg(channelIndex).arg(type));
    }

    locate();
    recordAction(deviceSetIndex, channelIndex, type, false);

    Recording recording;
    {
        QMutexLocker locker(&m_mutex);
        recording = m_recordings.take(channel);
    }

    // Recording stops asynchronously in the baseband sink. Wait for it to close the file,
    // otherwise the sizes reported below are those of a file still being written.
    QJsonObject report;
    bool stopped = waitFor([&]()
    {
        locate();
        report = channelReport(deviceSetIndex, channelIndex);

        for (const QString& key : report.keys())
        {
            if (key.endsWith("Report") && report[key].isObject()) {
                return report[key].toObject()["recording"].toInt(0) == 0;
            }
        }

        return true;
    }, 5000);

    QString reportKey;

    for (const QString& key : report.keys())
    {
        if (key.endsWith("Report") && report[key].isObject()) {
            reportKey = key;
        }
    }

    QJsonObject result;
    result["deviceSetIndex"] = deviceSetIndex;
    result["channelIndex"] = channelIndex;

    if (!reportKey.isEmpty())
    {
        QJsonObject sub = report[reportKey].toObject();
        result["recordTimeMs"] = sub["recordTimeMs"];
        result["recordSizeBytes"] = sub["recordSize"];
        result["captures"] = sub["recordCaptures"];
        result["sinkSampleRate"] = sub["sinkSampleRate"];
    }

    if (!stopped) {
        result["warning"] = "The channel still reports that it is recording; the file sizes below may be incomplete";
    }

    // The channel may have been pointed at a different file since the recording started, and a
    // deleted channel's address can be reused by a new one. Only list files when the channel is
    // still writing to the file this entry recorded.
    QString settingsKey = settingsKeyOf(settings);
    QString currentFile = settingsKey.isEmpty() ? QString() : settings[settingsKey].toObject()["fileRecordName"].toString();
    bool matches = recording.m_valid && !currentFile.isEmpty()
        && (QFileInfo(currentFile).absoluteFilePath() == QFileInfo(recording.m_fileBase + ".sdriq").absoluteFilePath());

    if (matches) {
        result["files"] = filesFor(recording)["files"];
    } else if (recording.m_valid) {
        result["warning"] = QString("This channel is now recording to %1 rather than the file this recording started, "
            "so the files written cannot be listed.").arg(currentFile);
    } else {
        result["warning"] = "This recording was not started through start_iq_recording, so the files written cannot be listed";
    }

    return result;
}

// SDRangel names each capture "<fileBase>.<timestamp>.sdriq", so list what appeared since the
// recording started
QJsonObject MCPCapture::filesFor(const Recording& recording)
{
    QFileInfo baseInfo(recording.m_fileBase);
    QDir dir = baseInfo.dir();
    QJsonArray files;
    QStringList filters;
    filters << baseInfo.fileName() + ".*";

    for (const QFileInfo& info : dir.entryInfoList(filters, QDir::Files, QDir::Name))
    {
        // Allow a second of slack: the file is created just after the action is sent
        if (info.lastModified().addSecs(1) < recording.m_started) {
            continue;
        }

        QJsonObject file;
        file["path"] = info.absoluteFilePath();
        file["sizeBytes"] = (double) info.size();
        file["modified"] = info.lastModified().toString(Qt::ISODate);
        files.append(file);
    }

    QJsonObject result;
    result["files"] = files;
    return result;
}

QJsonObject MCPCapture::status()
{
    pruneRecordings();
    QMutexLocker locker(&m_mutex);
    QJsonObject result;
    result["captureDir"] = m_captureDir;
    result["captureDirExists"] = QDir(m_captureDir).exists();
    result["maxBlockingSeconds"] = m_maxBlockingSeconds;
    QJsonArray active;

    for (auto it = m_recordings.begin(); it != m_recordings.end(); ++it)
    {
        QJsonObject recording;
        int deviceSetIndex = -1;
        int channelIndex = -1;

        // The index is where the channel is now, which is not necessarily where it was started
        if (locateChannel(it.key(), deviceSetIndex, channelIndex))
        {
            recording["deviceSetIndex"] = deviceSetIndex;
            recording["channelIndex"] = channelIndex;
            recording["channel"] = QString("%1:%2").arg(deviceSetIndex).arg(channelIndex);
        }

        recording["fileBase"] = it->m_fileBase;
        recording["started"] = it->m_started.toString(Qt::ISODate);
        active.append(recording);
    }

    result["activeRecordings"] = active;
    return result;
}
