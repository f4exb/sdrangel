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

#include "M17Modulator.h"
#include "dsp/dsptypes.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "m17modfifo.h"
#include "m17moddecimator.h"

class M17ModProcessor : public QObject
{
    Q_OBJECT
public:
    class MsgSendSMS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getSourceCall() const { return m_sourceCall; }
        const QString& getDestCall() const { return m_destCall; }
        uint8_t getCAN() const { return m_can; }
        const QString& getSMSText() const { return m_smsText; }

        static MsgSendSMS* create(const QString& sourceCall, const QString& destCall, uint8_t can, const QString& smsText) {
            return new MsgSendSMS(sourceCall, destCall, can, smsText);
        }

    private:
        QString m_sourceCall;
        QString m_destCall;
        uint8_t m_can;
        QString m_smsText;

        MsgSendSMS(const QString& sourceCall, const QString& destCall, uint8_t can, const QString& smsText) :
            Message(),
            m_sourceCall(sourceCall),
            m_destCall(destCall),
            m_can(can),
            m_smsText(smsText)
        { }
    };

    class MsgSendAPRS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getSourceCall() const { return m_sourceCall; }
        const QString& getDestCall() const { return m_destCall; }
        uint8_t getCAN() const { return m_can; }
        const QString& getCall() const { return m_call; }
        const QString& getTo() const { return m_to; }
        const QString& getVia() const { return m_via; }
        const QString& getData() const { return m_data; }
        bool getInsertPosition() const { return m_insertPosition; }

        static MsgSendAPRS* create(
            const QString& sourceCall,
            const QString& destCall,
            uint8_t can,
            const QString& call,
            const QString& to,
            const QString& via,
            const QString& data,
            bool insertPosition
        )
        {
            return new MsgSendAPRS(sourceCall, destCall, can, call, to, via, data, insertPosition);
        }

    private:
        QString m_sourceCall;
        QString m_destCall;
        uint8_t m_can;
        QString m_call;
        QString m_to;
        QString m_via;
        QString m_data;
        bool m_insertPosition;

        MsgSendAPRS(
            const QString& sourceCall,
            const QString& destCall,
            uint8_t can,
            const QString& call,
            const QString& to,
            const QString& via,
            const QString& data,
            bool insertPosition
        ) :
            Message(),
            m_sourceCall(sourceCall),
            m_destCall(destCall),
            m_can(can),
            m_call(call),
            m_to(to),
            m_via(via),
            m_data(data),
            m_insertPosition(insertPosition)
        { }
    };

    class MsgSendAudioFrame : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getSourceCall() const { return m_sourceCall; }
        const QString& getDestCall() const { return m_destCall; }
        std::array<int16_t, 1920>& getAudioFrame() { return m_audioFrame; }

        static MsgSendAudioFrame* create(const QString& sourceCall, const QString& destCall) {
            return new MsgSendAudioFrame(sourceCall, destCall);
        }

    private:
        QString m_sourceCall;
        QString m_destCall;
        std::array<int16_t, 1920> m_audioFrame;

        MsgSendAudioFrame(const QString& sourceCall, const QString& destCall) :
            Message(),
            m_sourceCall(sourceCall),
            m_destCall(destCall)
        { }
    };

    class MsgStartAudio : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getSourceCall() const { return m_sourceCall; }
        const QString& getDestCall() const { return m_destCall; }
        uint8_t getCAN() const { return m_can; }

        static MsgStartAudio* create(const QString& sourceCall, const QString& destCall, uint8_t can) {
            return new MsgStartAudio(sourceCall, destCall, can);
        }

    private:
        QString m_sourceCall;
        QString m_destCall;
        uint8_t m_can;

        MsgStartAudio(const QString& sourceCall, const QString& destCall, uint8_t can) :
            Message(),
            m_sourceCall(sourceCall),
            m_destCall(destCall),
            m_can(can)
        { }
    };

    class MsgStopAudio : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgStopAudio* create() {
            return new MsgStopAudio();
        }

    private:

        MsgStopAudio() :
            Message()
        { }
    };

    class MsgStartBERT : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgStartBERT* create() {
            return new MsgStartBERT();
        }

    private:

        MsgStartBERT() :
            Message()
        { }
    };

    class MsgSendBERTFrame : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgSendBERTFrame* create() {
            return new MsgSendBERTFrame();
        }

    private:

        MsgSendBERTFrame() :
            Message()
        { }
    };

    class MsgStopBERT : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgStopBERT* create() {
            return new MsgStopBERT();
        }

    private:

        MsgStopBERT() :
            Message()
        { }
    };

    class MsgSetGNSS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getLat() const { return m_lat; }
        float getLon() const { return m_lon; }
        float getAlt() const { return m_alt; }

        static MsgSetGNSS* create(float lat, float lon, float alt) {
            return new MsgSetGNSS(lat, lon, alt);
        }

    private:
        float m_lat;
        float m_lon;
        float m_alt;

        MsgSetGNSS(float lat, float lon, float alt) :
            Message(),
            m_lat(lat),
            m_lon(lon),
            m_alt(alt)
        { }
    };

    class MsgStopGNSS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgStopGNSS* create() {
            return new MsgStopGNSS();
        }

    private:

        MsgStopGNSS() :
            Message()
        { }
    };

    M17ModProcessor();
    ~M17ModProcessor();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    M17ModFIFO *getBasebandFifo() { return &m_basebandFifo; }

private:
    MessageQueue m_inputMessageQueue;
    M17ModFIFO m_basebandFifo; //!< Samples are 16 bit integer baseband 48 kS/s samples
    int m_basebandFifoHigh;
    int m_basebandFifoLow;
    M17ModDecimator m_decimator; //!< 48k -> 8k decimator
    modemm17::M17Modulator m_m17Modulator;
    std::array<modemm17::M17Modulator::lich_segment_t, 6> m_lich; //!< LICH bits
    int m_lichSegmentIndex;
    std::array<int16_t, 320*6> m_audioFrame;
    int m_audioFrameIndex;
    uint16_t m_audioFrameNumber;
    struct CODEC2 *m_codec2;
    modemm17::PRBS9 m_prbs;
    bool m_insertPositionToggle;

    bool handleMessage(const Message& cmd);
    void processPacket(const QString& sourceCall, const QString& destCall, uint8_t can, const QByteArray& packetBytes);
    void audioStart(const QString& sourceCall, const QString& destCall, uint8_t can);
    void audioStop();
    void processAudioFrame();
    std::array<uint8_t, 16> encodeAudio(std::array<int16_t, 320*6>& audioFrame);
    void processBERTFrame();
    void test(const QString& sourceCall, const QString& destCall);
    void send_preamble();
    void send_eot();
    void output_baseband(std::array<uint8_t, 2> sync_word, const std::array<int8_t, 368>& frame);
    QString formatAPRSPosition();

private slots:
    void handleInputMessages();
};

#endif // INCLUDE_M17MODPROCESSOR_H
