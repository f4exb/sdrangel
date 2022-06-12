///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_M17MODPROCESSOR_H
#define INCLUDE_M17MODPROCESSOR_H

#include <QObject>
#include <QByteArray>

#include "util/message.h"
#include "util/messagequeue.h"
#include "audio/audiofifo.h"

class M17ModProcessor : public QObject
{
    Q_OBJECT
public:
    class MsgSendSMS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getSourceCall() const { return m_sourceCall; }
        const QString& getDestCall() const { return m_destCall; }
        const QString& getSMSText() const { return m_smsText; }

        static MsgSendSMS* create(const QString& sourceCall, const QString& destCall, const QString& smsText) {
            return new MsgSendSMS(sourceCall, destCall, smsText);
        }

    private:
        QString m_sourceCall;
        QString m_destCall;
        QString m_smsText;

        MsgSendSMS(const QString& sourceCall, const QString& destCall, const QString& smsText) :
            Message(),
            m_sourceCall(sourceCall),
            m_destCall(destCall),
            m_smsText(smsText)
        { }
    };

    M17ModProcessor();
    ~M17ModProcessor();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    AudioFifo *getSymbolFifo() { return &m_basebandFifo; }

private:
    MessageQueue m_inputMessageQueue;
    AudioFifo m_basebandFifo; //!< Samples are 16 bit integer baseband 48 kS/s samples

    bool handleMessage(const Message& cmd);
    void processPacket(const QString& sourceCall, const QString& destCall, const QByteArray& packetBytes);

private slots:
    void handleInputMessages();
};

#endif // INCLUDE_M17MODPROCESSOR_H
