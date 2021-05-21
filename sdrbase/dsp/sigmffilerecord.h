///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// File recorder in SigMF format single channel for SI plugins                   //
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

#ifndef INCLUDE_SIGMF_FILERECORD_H
#define INCLUDE_SIGMF_FILERECORD_H

#include <string>
#include <iostream>
#include <fstream>
#include <ctime>

#include <QDateTime>

#include "dsp/sigmf_forward.h"
#include "dsp/filerecordinterface.h"
#include "export.h"

class Message;

class SDRBASE_API SigMFFileRecord : public FileRecordInterface {
public:
    SigMFFileRecord();
    SigMFFileRecord(const QString& filename, const QString& hardwareId);
    virtual ~SigMFFileRecord();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly) override;
	virtual void start() override;
	virtual void stop() override;
	virtual bool handleMessage(const Message& message) override;

    virtual void setFileName(const QString& filename) override;
    virtual const QString& getCurrentFileName() override { return m_fileName; }
    virtual bool startRecording() override;
    virtual bool stopRecording() override;
    virtual bool isRecording() const override { return m_recordOn; }

    void setHardwareId(const QString& hardwareId) { m_hardwareId = hardwareId; }
    void setMsShift(qint64 msShift) override { m_msShift = msShift; }
    unsigned int getNbCaptures() const;
    uint64_t getInitialMsCount() const { return m_initialMsCount; }
    uint64_t getInitialBytesCount() const { return m_initialBytesCount; }

private:
    QString m_hardwareId;
	QString m_fileName;
    QString m_sampleFileName;
    QString m_metaFileName;
    quint32 m_sampleRate;
    quint64 m_centerFrequency;
    qint64 m_msShift;
    bool m_recordOn;
    bool m_recordStart;
    QDateTime m_captureStartDT;
    std::ofstream m_metaFile;
    std::ofstream m_sampleFile;
    quint64 m_sampleStart;
    quint64 m_sampleCount;
    quint64 m_initialMsCount;
    quint64 m_initialBytesCount;
    sigmf::SigMF<sigmf::Global<core::DescrT, sdrangel::DescrT>,
            sigmf::Capture<core::DescrT, sdrangel::DescrT>,
            sigmf::Annotation<core::DescrT> > *m_metaRecord;
    void makeHeader();
    void makeCapture();
    void clearMeta();
};

#endif // INCLUDE_SIGMF_FILERECORD_H
