///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESINK_REMOTEOUTPUT_UDPSINKFEC_H_
#define PLUGINS_SAMPLESINK_REMOTEOUTPUT_UDPSINKFEC_H_

#include <channel/remotedatablock.h>
#include <string.h>
#include <cstddef>

#include <QObject>
#include <QHostAddress>
#include <QString>

#include "dsp/dsptypes.h"
#include "util/CRC64.h"

class QThread;
class RemoteOutputSender;

class UDPSinkFEC : public QObject
{
    Q_OBJECT
public:
    static const uint32_t m_udpSize = 512;          //!< Size of UDP block in number of bytes
    static const uint32_t m_nbOriginalBlocks = 128; //!< Number of original blocks in a protected block sequence

    /**
     * Construct UDP sink
     */
    UDPSinkFEC();

    /** Destroy UDP sink */
    ~UDPSinkFEC();

    void startSender();
    void stopSender();

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

    /** Set sample rate given in S/s */
    void setSampleRate(uint32_t sampleRate);

    void setNbBlocksFEC(uint32_t nbBlocksFEC);
    void setTxDelay(float txDelayRatio);
    void setRemoteAddress(const QString& address, uint16_t port);

    /** Return true if the stream is OK, return false if there is an error. */
    operator bool() const
    {
        return m_error.empty();
    }

private:
    std::string  m_error;

    uint32_t     m_sampleRate;        //!< sample rate in Hz
    uint32_t     m_nbSamples;         //!< total number of samples sent int the last frame
    QHostAddress m_ownAddress;

    CRC64        m_crc64;
    RemoteMetaDataFEC m_currentMetaFEC;  //!< Meta data for current frame
    uint32_t m_nbBlocksFEC;                 //!< Variable number of FEC blocks
    float m_txDelayRatio;                   //!< Delay in ratio of nominal frame period
    uint32_t m_txDelay;                     //!< Delay in microseconds (usleep) between each sending of an UDP datagram
    RemoteDataBlock *m_dataBlock;
    RemoteSuperBlock m_superBlock;       //!< current super block being built
    int m_txBlockIndex;                     //!< Current index in blocks to transmit in the Tx row
    int m_txBlocksIndex;                    //!< Current index of Tx blocks row
    uint16_t m_frameCount;                  //!< transmission frame count
    int m_sampleIndex;                      //!< Current sample index in protected block data

    RemoteOutputSender *m_remoteOutputSender;
    QThread *m_senderThread;
    QString m_remoteAddress;
    uint16_t m_remotePort;
};

#endif /* PLUGINS_SAMPLESINK_REMOTEOUTPUT_UDPSINKFEC_H_ */
