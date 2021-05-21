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

#ifndef INCLUDE_FILERECORD_INTERFACE_H
#define INCLUDE_FILERECORD_INTERFACE_H

#include <QString>
#include <QObject>

#include "dsp/dsptypes.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "export.h"

class SDRBASE_API FileRecordInterface : public QObject {
    Q_OBJECT
public:
    enum RecordType
    {
        RecordTypeUndefined = 0,
        RecordTypeSdrIQ,
        RecordTypeSigMF,
        RecordTypeWav
    };

    FileRecordInterface();
    virtual ~FileRecordInterface();

	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly) = 0;
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

    virtual void setFileName(const QString &filename) = 0;
    virtual const QString& getCurrentFileName() = 0;
    virtual bool startRecording() = 0;
    virtual bool stopRecording() = 0;
    virtual bool isRecording() const = 0;

    virtual void setMsShift(qint64 msShift) = 0;
    virtual int getBytesPerSample() { return sizeof(Sample); };

    static QString genUniqueFileName(unsigned int deviceUID, int istream = -1);
    static RecordType guessTypeFromFileName(const QString& fileName, QString& fileBase);

protected:
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI

protected slots:
	void handleInputMessages();

};


#endif // INCLUDE_FILERECORD_INTERFACE_H
