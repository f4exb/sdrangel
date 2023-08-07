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

#include <QDateTime>
#include <QFileInfo>

#include "filerecordinterface.h"

FileRecordInterface::FileRecordInterface()
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

FileRecordInterface::~FileRecordInterface()
{}

QString FileRecordInterface::genUniqueFileName(unsigned int deviceUID, int istream)
{
    if (istream < 0) {
        return QString("rec%1.%2.sdriq").arg(deviceUID).arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz"));
    } else {
        return QString("rec%1_%2.%3.sdriq").arg(deviceUID).arg(istream).arg(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH_mm_ss_zzz"));
    }
}

FileRecordInterface::RecordType FileRecordInterface::guessTypeFromFileName(const QString& fileName, QString& fileBase)
{
    QFileInfo fileInfo(fileName);
    QString extension = fileInfo.suffix();

    fileBase = fileName;
    if (!extension.isEmpty())
    {
        fileBase.chop(extension.size() + 1);
        if (extension == "sdriq")
        {
            return RecordTypeSdrIQ;
        }
        else if (extension == "sigmf-meta")
        {
            return RecordTypeSigMF;
        }
        else if (extension == "wav")
        {
            return RecordTypeWav;
        }
        else
        {
            return RecordTypeUndefined;
        }
    }
    else
    {
        return RecordTypeUndefined;
    }
}

void FileRecordInterface::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message))
		{
			delete message;
		}
	}
}
