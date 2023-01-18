///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#include <QStandardPaths>
#include <QDir>
#include <QDateTime>

#include "dsp/wavfilerecord.h"

#include "ft8demodsettings.h"
#include "ft8demodworker.h"

FT8DemodWorker::FT8DemodWorker()
{
    QString relPath = "sdrangel/ft8/save";
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    dir.mkpath(relPath);
    m_samplesPath = dir.absolutePath() + "/" + relPath;
    qDebug("FT8DemodWorker::FT8DemodWorker: samples path: %s", qPrintable(m_samplesPath));
}

FT8DemodWorker::~FT8DemodWorker()
{}

void FT8DemodWorker::processBuffer(int16_t *buffer, QDateTime periodTS)
{
    qDebug("FT8DemodWorker::processBuffer: %s", qPrintable(periodTS.toString("yyyy-MM-dd HH:mm:ss")));
    WavFileRecord *wavFileRecord = new WavFileRecord(FT8DemodSettings::m_ft8SampleRate);
    QFileInfo wfi(QDir(m_samplesPath), periodTS.toString("yyyyMMdd_HHmmss"));
    QString wpath = wfi.absoluteFilePath();
    qDebug("FT8DemodWorker::processBuffer: WAV file: %s.wav", qPrintable(wpath));
    wavFileRecord->setFileName(wpath);
    wavFileRecord->setFileBaseIsFileName(true);
    wavFileRecord->setMono(true);
    wavFileRecord->startRecording();
    wavFileRecord->writeMono(buffer, 15*FT8DemodSettings::m_ft8SampleRate);
    wavFileRecord->stopRecording();
    delete wavFileRecord;
}
