///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#include "dsp/filerecord.h"

#include "sigmffilesinksink.h"

SigMFFileSinkSink::SigMFFileSinkSink() :
    m_record(false)
{}

SigMFFileSinkSink::~SigMFFileSinkSink()
{}

void SigMFFileSinkSink::startRecording()
{
    QString fileBase;
    FileRecordInterface::RecordType recordType = FileRecordInterface::guessTypeFromFileName(m_settings.m_fileRecordName, fileBase);

    if (recordType == FileRecordInterface::RecordTypeSigMF)
    {
        m_fileSink.setFileName(fileBase);
        m_fileSink.startRecording();
        m_record = true;
    }
}

void SigMFFileSinkSink::stopRecording()
{
    m_record = false;
    m_fileSink.stopRecording();
}

void SigMFFileSinkSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    if (m_record) {
        m_fileSink.feed(begin, end, true);
    }
}

void SigMFFileSinkSink::setSampleRate(int sampleRate)
{
}