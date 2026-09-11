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

#ifndef INCLUDE_FEATURE_MCPCAPTURE_H_
#define INCLUDE_FEATURE_MCPCAPTURE_H_

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QAtomicInt>
#include <QMutex>
#include <QObject>
#include <QString>

class WebAPIAdapterInterface;
class DataFifo;
class MessageQueue;
class ObjectPipe;

// Captures demodulated audio from a demodulator through its "demod" data pipe, the same
// pipe the Demod Analyzer feature uses. Only some channel types provide it.
//
// The pipes are registered and removed on the main thread; the samples are read from the
// calling (HTTP) thread, which is safe because the FIFO is mutex protected.
class MCPAudioCapture : public QObject
{
    Q_OBJECT
public:
    explicit MCPAudioCapture(WebAPIAdapterInterface *adapter, QObject *parent = nullptr);

    //!< Blocks for the requested duration. Called from an HTTP thread.
    QJsonObject capture(int deviceSetIndex, int channelIndex, double seconds, const QString& path, bool inlineAudio);

private slots:
    void handlePipeToBeDeleted(int reason, QObject *object);

private:
    struct Attachment
    {
        DataFifo *m_fifo;
        MessageQueue *m_reportQueue;

        Attachment() : m_fifo(nullptr), m_reportQueue(nullptr) {}
    };

    WebAPIAdapterInterface *m_adapter;
    QMutex m_captureMutex;      //!< One capture at a time
    QMutex m_attachMutex;       //!< Guards the attachment map

    // Pipes are registered once per channel and kept until the channel is destroyed. Repeatedly
    // registering and unregistering is not safe: the pipe registry leaves a stale entry behind
    // when its garbage collector deletes a pipe, which the next unregister would dereference.
    QMap<QObject *, Attachment> m_attachments;

    QObject *m_pendingChannel;  //!< Arguments and results of attach(), which runs on the main thread
    DataFifo *m_pendingFifo;
    MessageQueue *m_pendingReportQueue;
    QString m_attachError;

    // Set when the channel being captured from goes away, so the capture loop stops touching
    // the FIFO before the pipe registry destroys it
    QObject *m_capturingChannel;
    QAtomicInt m_captureAborted;

    // Held while the capture is inside either pipe element, the sample FIFO or the report
    // queue. The pipe teardown handler takes it after setting the abort flag, so it cannot
    // return (and the registry cannot free either element) while a read is in progress.
    QMutex m_fifoMutex;

    Q_INVOKABLE bool attach(int deviceSetIndex, int channelIndex);

    int reportedSampleRate(MessageQueue *queue, int timeoutMs);
    int sampleRateFromReport(int deviceSetIndex, int channelIndex);
};

// Recording of baseband IQ to files through a File Sink channel.
//
// All files are written under a capture directory; paths that would escape it are refused,
// so an agent cannot write anywhere on the machine. The channel is created and deleted by
// MCPTools, which owns the waiting on SDRangel's asynchronous channel operations.
class MCPCapture
{
public:
    explicit MCPCapture(WebAPIAdapterInterface *adapter);

    void setCaptureDir(const QString& dir);
    QString getCaptureDir() const;

    QJsonObject startIQRecording(int deviceSetIndex, int channelIndex, const QString& fileName,
        int frequencyOffset, int log2Decim);

    //!< Stops the recording of the channel currently at this index
    QJsonObject stopIQRecording(int deviceSetIndex, int channelIndex);

    //!< Stops the recording of this channel wherever it has moved to
    QJsonObject stopIQRecording(const void *channel);

    //!< Drops the entry for a channel that is being deleted
    void forgetRecording(const void *channel);

    QJsonObject captureAudio(int deviceSetIndex, int channelIndex, double seconds, const QString& fileName, bool inlineAudio);
    QJsonObject status();

    //!< Current index of a channel, or false if it has gone. Indices shift when a lower
    //!< numbered channel is deleted, so they are resolved again before every action.
    static bool locateChannel(const void *channel, int& deviceSetIndex, int& channelIndex);

    //!< The channel object at an index, or null
    static const void *channelObject(int deviceSetIndex, int channelIndex);

    //!< Inline audio is only offered for short clips, as it is base64 encoded into the reply
    static constexpr int m_maxInlineSeconds = 10;

    //!< Blocking captures hold an HTTP connection open, so they are kept short
    static constexpr int m_maxBlockingSeconds = 30;

private:
    struct Recording
    {
        QString m_fileBase;     //!< Path without the timestamp and extension SDRangel appends
        QDateTime m_started;
        bool m_valid;

        Recording() : m_valid(false) {}
    };

    WebAPIAdapterInterface *m_adapter;
    mutable QMutex m_mutex;
    QString m_captureDir;

    // Keyed by the channel object rather than its index, because indices are renumbered
    // whenever a lower numbered channel is deleted, which can happen while a recording runs
    QMap<const void *, Recording> m_recordings;
    MCPAudioCapture m_audio;

    QString resolvePath(const QString& name, const QString& defaultPrefix);
    QJsonObject channelSettings(int deviceSetIndex, int channelIndex);
    QString channelType(const QJsonObject& settings);
    void patchChannel(int deviceSetIndex, int channelIndex, const QString& type, const QJsonObject& settings);
    void recordAction(int deviceSetIndex, int channelIndex, const QString& type, bool record);
    QJsonObject channelReport(int deviceSetIndex, int channelIndex);
    void pruneRecordings();     //!< Drops entries whose channel has been deleted
    static QJsonObject filesFor(const Recording& recording);
};

#endif // INCLUDE_FEATURE_MCPCAPTURE_H_
