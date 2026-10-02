///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_WEFAXDEMODIMAGEWORKER_H
#define INCLUDE_WEFAXDEMODIMAGEWORKER_H

#include <cstdint>
#include <deque>
#include <vector>

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>

#include "wefaxdemodsettings.h"
#include "wefaxslant.h"

class WefaxDemodImageWorker : public QObject
{
    Q_OBJECT

public:
    using Lines = std::vector<std::vector<std::uint8_t>>;
    // Room for a complete maximum-height image, so rows are only rejected if
    // the worker stalls for an entire capture rather than during a PNG save.
    static constexpr int MaxQueuedRows = WefaxDemodSettings::MaxRowsLimit;

    explicit WefaxDemodImageWorker(QObject *parent = nullptr);

    bool enqueueLines(quint64 imageId, Lines&& lines);
    void enqueueClear(quint64 imageId);
    void enqueuePublish(quint64 imageId);
    void enqueueSave(quint64 imageId, qint64 frequencyHz, const QString& captureStartTime,
        int ioc, int linesPerMinute, const QString& fileName = QString());
    void enqueueConfigure(const WefaxDemodSettings& settings);
    // Whether the current image's line start came from phasing. Without
    // phasing, automatic correction also aligns the image on a dead sector.
    void enqueueAlignmentHint(quint64 imageId, bool phaseAligned);

    // These methods run in the worker thread. They are public so the bounded
    // image store can be exercised deterministically without an event loop.
    void applySettings(const WefaxDemodSettings& settings);
    void appendLines(Lines&& lines);
    void clearImage();
    QImage image() const;
    int rowCount() const;

    // The image as displayed and saved: slant corrected when enabled and
    // measurable. refresh forces a new slant measurement.
    QImage outputImage(bool refresh);
    double slantCorrectionPpm() const;
    int alignmentOffset() const;

signals:
    // slantPpm is the line-clock correction applied by automatic slant
    // correction, or 0 when none is applied. alignmentPx is the applied
    // dead-sector alignment.
    void imageUpdated(quint64 imageId, const QImage& image, int rowCount, double slantPpm, int alignmentPx);
    void saveCompleted(quint64 imageId, bool success, const QString& fileName, const QString& error);
    void queueOverloaded(quint64 overflowCount);

private:
    mutable QMutex m_imageMutex;
    QMutex m_queueMutex;
    struct PendingLines { quint64 imageId = 0; Lines lines; };
    std::deque<PendingLines> m_pendingLines;
    int m_queuedRows;
    bool m_processQueued;
    quint64 m_overflowCount;

    QImage m_image;
    int m_imageRows;
    int m_nextPublishRow;
    quint64 m_imageId;
    WefaxDemodSettings m_settings;
    WefaxSlant::Estimate m_slant;   // Last valid estimate for the current image
    int m_slantRows;                // Rows when the slant was last measured
    bool m_phaseAligned;            // Line start of the current image came from phasing
    int m_alignmentOffset;          // Applied dead-sector alignment in pixels

    void publishImage(bool final = false);
    QString resolveSaveFileName(quint64 imageId, qint64 frequencyHz, int ioc,
        int linesPerMinute, const QString& fileName) const;
    bool saveImageToDisk(const QString& fileName, quint64 imageId, qint64 frequencyHz,
        const QString& captureStartTime, int ioc, int linesPerMinute, QString& error);

private slots:
    void processQueue();
};

#endif // INCLUDE_WEFAXDEMODIMAGEWORKER_H
