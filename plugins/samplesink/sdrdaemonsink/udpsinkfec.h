///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESINK_SDRDAEMONSINK_UDPSINKFEC_H_
#define PLUGINS_SAMPLESINK_SDRDAEMONSINK_UDPSINKFEC_H_

#include <string.h>
#include <cstddef>

#include <QObject>
#include <QHostAddress>
#include <QString>
#include <QThread>

#include "cm256.h"

#include "dsp/dsptypes.h"
#include "util/CRC64.h"
#include "util/messagequeue.h"
#include "util/message.h"

#include "UDPSocket.h"

class UDPSinkFECWorker;

class UDPSinkFEC : public QObject
{
    Q_OBJECT
public:
    static const uint32_t m_udpSize = 512;          //!< Size of UDP block in number of bytes
    static const uint32_t m_nbOriginalBlocks = 128; //!< Number of original blocks in a protected block sequence
#pragma pack(push, 1)
    struct MetaDataFEC
    {
        uint32_t m_centerFrequency;   //!<  4 center frequency in kHz
        uint32_t m_sampleRate;        //!<  8 sample rate in Hz
        uint8_t  m_sampleBytes;       //!<  9 MSB(4): indicators, LSB(4) number of bytes per sample
        uint8_t  m_sampleBits;        //!< 10 number of effective bits per sample
        uint8_t  m_nbOriginalBlocks;  //!< 11 number of blocks with original (protected) data
        uint8_t  m_nbFECBlocks;       //!< 12 number of blocks carrying FEC
        uint32_t m_tv_sec;            //!< 16 seconds of timestamp at start time of super-frame processing
        uint32_t m_tv_usec;           //!< 20 microseconds of timestamp at start time of super-frame processing
        uint32_t m_crc32;             //!< 24 CRC32 of the above

        bool operator==(const MetaDataFEC& rhs)
        {
            return (memcmp((const char *) this, (const char *) &rhs, 12) == 0); // Only the 12 first bytes are relevant
        }

        void init()
        {
            memset((char *) this, 0, sizeof(MetaDataFEC));
            m_nbFECBlocks = -1;
        }
    };

    struct Header
    {
        uint16_t frameIndex;
        uint8_t  blockIndex;
        uint8_t  filler;
    };

    static const int samplesPerBlock = (m_udpSize - sizeof(Header)) / sizeof(Sample);

    struct ProtectedBlock
    {
        Sample m_samples[samplesPerBlock];
    };

    struct SuperBlock
    {
        Header         header;
        ProtectedBlock protectedBlock;
    };
#pragma pack(pop)

    /**
     * Construct UDP sink
     */
    UDPSinkFEC();

    /** Destroy UDP sink */
    ~UDPSinkFEC();

    /**
     * Write IQ samples
     */
    void write(const SampleVector::iterator& begin, uint32_t sampleChunkSize);

    /** Return the last error, or return an empty string if there is no error. */
    std::string error()
    {
        std::string ret(m_error);
        m_error.clear();
        return ret;
    }

    /** Set center frequency given in Hz */
    void setCenterFrequency(uint64_t centerFrequency) { m_centerFrequency = centerFrequency / 1000; }

    /** Set sample rate given in Hz */
    void setSampleRate(uint32_t sampleRate) { m_sampleRate = sampleRate; }

    void setSampleBytes(uint8_t sampleBytes) { m_sampleBytes = (sampleBytes & 0x0F) + (m_sampleBytes & 0xF0); }
    void setSampleBits(uint8_t sampleBits) { m_sampleBits = sampleBits; }

    void setNbBlocksFEC(uint32_t nbBlocksFEC);
    void setTxDelay(uint32_t txDelay);
    void setRemoteAddress(const QString& address, uint16_t port);

    /** Return true if the stream is OK, return false if there is an error. */
    operator bool() const
    {
        return m_error.empty();
    }

private:
    std::string  m_error;

    uint32_t     m_centerFrequency;   //!< center frequency in kHz
    uint32_t     m_sampleRate;        //!< sample rate in Hz
    uint8_t      m_sampleBytes;       //!< number of bytes per sample
    uint8_t      m_sampleBits;        //!< number of effective bits per sample
    uint32_t     m_nbSamples;         //!< total number of samples sent int the last frame

    QHostAddress m_ownAddress;

    CRC64        m_crc64;
    uint8_t*     m_bufMeta;
    uint8_t*     m_buf;

    MetaDataFEC m_currentMetaFEC;        //!< Meta data for current frame
    uint32_t m_nbBlocksFEC;              //!< Variable number of FEC blocks
    uint32_t m_txDelay;                  //!< Delay in microseconds (usleep) between each sending of an UDP datagram
    SuperBlock m_txBlocks[4][256];       //!< UDP blocks to send with original data + FEC
    SuperBlock m_superBlock;             //!< current super block being built
    int m_txBlockIndex;                  //!< Current index in blocks to transmit in the Tx row
    int m_txBlocksIndex;                 //!< Current index of Tx blocks row
    uint16_t m_frameCount;               //!< transmission frame count
    int m_sampleIndex;                   //!< Current sample index in protected block data

    QThread *m_udpThread;
    UDPSinkFECWorker *m_udpWorker;
};


class UDPSinkFECWorker : public QObject
{
    Q_OBJECT
public:
    class MsgUDPFECEncodeAndSend : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        UDPSinkFEC::SuperBlock *getTxBlocks() const { return m_txBlockx; }
        uint32_t getNbBlocsFEC() const { return m_nbBlocksFEC; }
        uint32_t getTxDelay() const { return m_txDelay; }
        uint16_t getFrameIndex() const { return m_frameIndex; }

        static MsgUDPFECEncodeAndSend* create(
                UDPSinkFEC::SuperBlock *txBlocks,
                uint32_t nbBlocksFEC,
                uint32_t txDelay,
                uint16_t frameIndex)
        {
            return new MsgUDPFECEncodeAndSend(txBlocks, nbBlocksFEC, txDelay, frameIndex);
        }

    private:
        UDPSinkFEC::SuperBlock *m_txBlockx;
        uint32_t m_nbBlocksFEC;
        uint32_t m_txDelay;
        uint16_t m_frameIndex;

        MsgUDPFECEncodeAndSend(
                UDPSinkFEC::SuperBlock *txBlocks,
                uint32_t nbBlocksFEC,
                uint32_t txDelay,
                uint16_t frameIndex) :
            m_txBlockx(txBlocks),
            m_nbBlocksFEC(nbBlocksFEC),
            m_txDelay(txDelay),
            m_frameIndex(frameIndex)
        {}
    };

    class MsgConfigureRemoteAddress : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const QString& getAddress() const { return m_address; }
        uint16_t getPort() const { return m_port; }

        static MsgConfigureRemoteAddress* create(const QString& address, uint16_t port)
        {
            return new MsgConfigureRemoteAddress(address, port);
        }

    private:
        QString m_address;
        uint16_t m_port;

        MsgConfigureRemoteAddress(const QString& address, uint16_t port) :
            m_address(address),
            m_port(port)
        {}
    };

    UDPSinkFECWorker();
    ~UDPSinkFECWorker();

    void pushTxFrame(UDPSinkFEC::SuperBlock *txBlocks,
        uint32_t nbBlocksFEC,
        uint32_t txDelay,
        uint16_t frameIndex);
    void setRemoteAddress(const QString& address, uint16_t port);
    void stop();

    MessageQueue m_inputMessageQueue;    //!< Queue for asynchronous inbound communication

signals:
    void finished();

public slots:
    void process();

private slots:
    void handleInputMessages();

private:
    void encodeAndTransmit(UDPSinkFEC::SuperBlock *txBlockx, uint16_t frameIndex, uint32_t nbBlocksFEC, uint32_t txDelay);

    volatile bool m_running;
    CM256 m_cm256;                       //!< CM256 library object
    bool m_cm256Valid;                   //!< true if CM256 library is initialized correctly
    UDPSocket    m_socket;
    QString      m_remoteAddress;
    uint16_t     m_remotePort;
};



#endif /* PLUGINS_SAMPLESINK_SDRDAEMONSINK_UDPSINKFEC_H_ */
