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

#include "wefaxdemodimageworker.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QImageWriter>

#include "wefaximageadjust.h"

WefaxDemodImageWorker::WefaxDemodImageWorker(QObject *parent) :
    QObject(parent),
    m_queuedRows(0),
    m_processQueued(false),
    m_overflowCount(0),
    m_imageRows(0),
    m_nextPublishRow(1),
    m_imageId(0),
    m_slantRows(0),
    m_phaseAligned(true),
    m_alignmentOffset(0)
{
}

bool WefaxDemodImageWorker::enqueueLines(quint64 imageId, Lines&& lines)
{
    if (lines.empty()) {
        return true;
    }

    bool schedule = false;
    {
        QMutexLocker locker(&m_queueMutex);
        const int incomingRows = static_cast<int>(lines.size());
        if ((m_queuedRows + incomingRows) > MaxQueuedRows)
        {
            ++m_overflowCount;
            emit queueOverloaded(m_overflowCount);
            return false;
        }

        m_queuedRows += incomingRows;
        m_pendingLines.push_back(PendingLines{imageId, std::move(lines)});
        if (!m_processQueued)
        {
            m_processQueued = true;
            schedule = true;
        }
    }

    if (schedule) {
        QMetaObject::invokeMethod(this, "processQueue", Qt::QueuedConnection);
    }
    return true;
}

void WefaxDemodImageWorker::enqueueClear(quint64 imageId)
{
    QMetaObject::invokeMethod(this, [this, imageId]() {
        m_imageId = imageId;
        clearImage();
        emit imageUpdated(imageId, QImage(), 0, 0.0, 0);

        bool schedule = false;
        {
            QMutexLocker locker(&m_queueMutex);
            for (auto it = m_pendingLines.begin(); it != m_pendingLines.end();)
            {
                if (it->imageId < imageId)
                {
                    m_queuedRows -= static_cast<int>(it->lines.size());
                    it = m_pendingLines.erase(it);
                }
                else 
                {
                    ++it;
                }
            }
            if (!m_pendingLines.empty() && !m_processQueued)
            {
                m_processQueued = true;
                schedule = true;
            }
        }
        if (schedule) {
            QMetaObject::invokeMethod(this, "processQueue", Qt::QueuedConnection);
        }
    }, Qt::QueuedConnection);
}

void WefaxDemodImageWorker::enqueueSave(
    quint64 imageId,
    qint64 frequencyHz,
    const QString& captureStartTime,
    int ioc,
    int linesPerMinute,
    const QString& fileName)
{
    QMetaObject::invokeMethod(this, [this, imageId, frequencyHz, captureStartTime,
        ioc, linesPerMinute, fileName]() {
        const QString resolvedName = resolveSaveFileName(
            imageId, frequencyHz, ioc, linesPerMinute, fileName);
        QString error;
        const bool current = imageId == m_imageId;
        const bool success = current && saveImageToDisk(
            resolvedName, imageId, frequencyHz, captureStartTime,
            ioc, linesPerMinute, error);
        if (!current) {
            error = tr("Image was replaced before it could be saved");
        }
        emit saveCompleted(imageId, success, resolvedName, error);
    }, Qt::QueuedConnection);
}

void WefaxDemodImageWorker::enqueuePublish(quint64 imageId)
{
    QMetaObject::invokeMethod(this, [this, imageId]() {
        if (imageId == m_imageId) {
            publishImage(true);
        }
    }, Qt::QueuedConnection);
}

void WefaxDemodImageWorker::enqueueConfigure(const WefaxDemodSettings& settings)
{
    QMetaObject::invokeMethod(this, [this, settings]() {
        applySettings(settings);
    }, Qt::QueuedConnection);
}

void WefaxDemodImageWorker::applySettings(const WefaxDemodSettings& settings)
{
    const bool autoSlantChanged = settings.m_autoSlant != m_settings.m_autoSlant;
    m_settings = settings;
    m_settings.validate();

    if (autoSlantChanged && (rowCount() > 0)) {
        publishImage(true);
    }
}

void WefaxDemodImageWorker::appendLines(Lines&& lines)
{
    QMutexLocker locker(&m_imageMutex);

    for (const auto& line : lines)
    {
        const int width = static_cast<int>(line.size());
        if (width <= 0) {
            continue;
        }
        if (!m_image.isNull() && (m_image.width() != width))
        {
            m_image = QImage();
            m_imageRows = 0;
        }
        if (m_imageRows >= m_settings.m_maxRows) {
            break;
        }

        if (m_image.isNull() || (m_imageRows >= m_image.height()))
        {
            const int capacity = std::min(
                m_settings.m_maxRows,
                std::max(m_imageRows + 1, std::max(256, m_image.height() * 2)));
            QImage expanded(width, capacity, QImage::Format_Grayscale8);
            expanded.fill(255);
            for (int row = 0; row < m_imageRows; ++row) {
                std::memcpy(expanded.scanLine(row), m_image.constScanLine(row), width);
            }
            m_image = std::move(expanded);
        }

        std::memcpy(m_image.scanLine(m_imageRows), line.data(), line.size());
        ++m_imageRows;
    }
}

void WefaxDemodImageWorker::clearImage()
{
    QMutexLocker locker(&m_imageMutex);
    m_image = QImage();
    m_imageRows = 0;
    m_nextPublishRow = 1;
    m_slant = WefaxSlant::Estimate();
    m_slantRows = 0;
    m_phaseAligned = true;
    m_alignmentOffset = 0;
}

QImage WefaxDemodImageWorker::image() const
{
    QMutexLocker locker(&m_imageMutex);
    return m_imageRows > 0 ? m_image.copy(0, 0, m_image.width(), m_imageRows) : QImage();
}

int WefaxDemodImageWorker::rowCount() const
{
    QMutexLocker locker(&m_imageMutex);
    return m_imageRows;
}

void WefaxDemodImageWorker::processQueue()
{
    bool changed = false;

    for (;;)
    {
        PendingLines pending;
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_pendingLines.empty())
            {
                m_processQueued = false;
                break;
            }
            // A clear for this newer generation is already queued ahead of
            // any corresponding process request. Leave its rows queued until
            // that clear has advanced m_imageId.
            if (m_pendingLines.front().imageId > m_imageId)
            {
                m_processQueued = false;
                break;
            }
            pending = std::move(m_pendingLines.front());
            m_pendingLines.pop_front();
            m_queuedRows -= static_cast<int>(pending.lines.size());
        }
        if (pending.imageId == m_imageId)
        {
            appendLines(std::move(pending.lines));
            changed = true;
        }
    }

    if (changed && (rowCount() >= m_nextPublishRow)) {
        publishImage();
    }
}

QImage WefaxDemodImageWorker::outputImage(bool refresh)
{
    const QImage raw = image();
    const int rows = raw.height();
    if (!m_settings.m_autoSlant || (rows < WefaxSlant::MinRows)) {
        return raw;
    }

    // Measuring takes a fraction of a second on a full chart, so repeat it
    // only as the image grows by a useful amount. Keep the last valid
    // measurements if a later one is inconclusive.
    if (refresh || (m_slantRows == 0) || (rows - m_slantRows >= std::max(32, m_slantRows / 10)))
    {
        const WefaxSlant::Estimate estimate = WefaxSlant::estimate(raw, rows);
        if (estimate.valid) {
            m_slant = estimate;
        }
        m_slantRows = rows;

        // Phasing already places the line start; otherwise look for the
        // station's blank dead sector in the straightened image.
        if (!m_phaseAligned)
        {
            const double drift = m_slant.valid ? m_slant.driftPerRow : 0.0;
            const int start = WefaxSlant::deadSectorStart(WefaxSlant::correct(raw, rows, drift), rows);
            if (start >= 0) {
                m_alignmentOffset = (start > raw.width() / 2) ? (start - raw.width()) : start;
            }
        }
    }

    if (!m_slant.valid && (m_alignmentOffset == 0)) {
        return raw;
    }
    return WefaxSlant::correct(raw, rows, m_slant.valid ? m_slant.driftPerRow : 0.0, m_alignmentOffset);
}

double WefaxDemodImageWorker::slantCorrectionPpm() const
{
    return (m_settings.m_autoSlant && m_slant.valid && !m_image.isNull())
        ? WefaxSlant::driftToPpm(m_slant.driftPerRow, m_image.width())
        : 0.0;
}

int WefaxDemodImageWorker::alignmentOffset() const
{
    return m_settings.m_autoSlant ? m_alignmentOffset : 0;
}

void WefaxDemodImageWorker::enqueueAlignmentHint(quint64 imageId, bool phaseAligned)
{
    QMetaObject::invokeMethod(this, [this, imageId, phaseAligned]() {
        if (imageId == m_imageId) {
            m_phaseAligned = phaseAligned;
        }
    }, Qt::QueuedConnection);
}

void WefaxDemodImageWorker::publishImage(bool final)
{
    const QImage completedImage = outputImage(final);
    const int rows = completedImage.height();
    if (rows > 0) {
        // Keep early reception responsive, then grow the batch with the image.
        // The number of full snapshots is logarithmic for long captures rather
        // than proportional to every received row.
        m_nextPublishRow = rows + std::max(8, rows / 50);
    }
    emit imageUpdated(m_imageId, completedImage, rows, slantCorrectionPpm(), alignmentOffset());
}

QString WefaxDemodImageWorker::resolveSaveFileName(
    quint64 imageId,
    qint64 frequencyHz,
    int ioc,
    int linesPerMinute,
    const QString& fileName) const
{
    if (!fileName.isEmpty())
    {
        if (QFileInfo(fileName).suffix().compare("png", Qt::CaseInsensitive) == 0) {
            return fileName;
        }
        return fileName + QStringLiteral(".png");
    }

    QString target = m_settings.m_autoSavePath;
    if (target.isEmpty()) {
        target = QDir::currentPath();
    }

    QDir directory(target);
    if (!directory.exists() && !QDir().mkpath(target)) {
        return QString();
    }
    return directory.filePath(QString("wefax-%1-%2Hz-img%3-ioc%4-%5lpm.png")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz"))
        .arg(frequencyHz)
        .arg(imageId)
        .arg(ioc)
        .arg(linesPerMinute));
}

bool WefaxDemodImageWorker::saveImageToDisk(
    const QString& fileName,
    quint64 imageId,
    qint64 frequencyHz,
    const QString& captureStartTime,
    int ioc,
    int linesPerMinute,
    QString& error)
{
    QImage output = outputImage(true);
    if (fileName.isEmpty())
    {
        error = tr("The output directory could not be created");
        return false;
    }
    if (output.isNull()) 
    {
        error = tr("There is no received image to save");
        return false;
    }
    // The user's alignment, slant, contrast, inversion, threshold and
    // rotation, as shown in the display.
    if (WefaxImageAdjust::active(m_settings)) {
        output = WefaxImageAdjust::apply(output, m_settings);
    }
    output.setText("WEFAX image ID", QString::number(imageId));
    output.setText("RF frequency Hz", QString::number(frequencyHz));
    output.setText("IOC", QString::number(ioc));
    output.setText("Lines per minute", QString::number(linesPerMinute));
    output.setText("Capture start UTC", captureStartTime);
    output.setText("Timestamp source", "processing-clock");
    output.setText("Slant correction ppm", QString::number(slantCorrectionPpm(), 'f', 1));
    if (m_settings.m_horizontalAlignment != 0) {
        output.setText("Horizontal alignment pixels", QString::number(m_settings.m_horizontalAlignment));
    }
    if (std::abs(m_settings.m_displaySlantCorrectionPpm) > 0.001) {
        output.setText("Manual slant correction ppm", QString::number(m_settings.m_displaySlantCorrectionPpm, 'f', 1));
    }
    if (m_settings.m_displayContrast != 0) {
        output.setText("Contrast", QString::number(m_settings.m_displayContrast));
    }
    if (m_settings.m_displayInverted) {
        output.setText("Inverted", "yes");
    }
    if (m_settings.m_displayThreshold >= 0) {
        output.setText("Threshold", QString::number(m_settings.m_displayThreshold));
    }
    if (m_settings.m_displayRotation != 0) {
        output.setText("Rotation degrees", QString::number(m_settings.m_displayRotation));
    }
    output.setText("Alignment offset pixels", QString::number(alignmentOffset()));
    QImageWriter writer(fileName, "PNG");
    if (!writer.write(output)) 
    {
        error = writer.errorString();
        return false;
    }
    error.clear();
    return true;
}
