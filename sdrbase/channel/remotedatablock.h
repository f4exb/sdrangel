///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Remote sink channel (Rx) data block                                           //
//                                                                               //
// SDRangel can serve as a remote SDR front end that handles the interface       //
// with a physical device and sends or receives the I/Q samples stream via UDP   //
// to or from another SDRangel instance or any program implementing the same     //
// protocol. The remote SDRangel is controlled via its Web REST API.             //
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

#ifndef CHANNEL_REMOTEDATABLOCK_H_
#define CHANNEL_REMOTEDATABLOCK_H_

#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <QString>
#include "dsp/dsptypes.h"

#define UDPSINKFEC_UDPSIZE 512
#define UDPSINKFEC_NBORIGINALBLOCKS 128
//#define UDPSINKFEC_NBTXBLOCKS 8

#pragma pack(push, 1)
struct RemoteMetaDataFEC
{
    uint64_t m_centerFrequency;       //!<  8 center frequency in kHz
    uint32_t m_sampleRate;            //!< 12 sample rate in Hz
    uint8_t  m_sampleBytes;           //!< 13 4 LSB: number of bytes per sample (2 or 4)
    uint8_t  m_sampleBits;            //!< 14 number of effective bits per sample (deprecated)
    uint8_t  m_nbOriginalBlocks;      //!< 15 number of blocks with original (protected) data
    uint8_t  m_nbFECBlocks;           //!< 16 number of blocks carrying FEC
    uint8_t  m_deviceIndex;           //!< 29 index of device set in instance
    uint8_t  m_channelIndex;          //!< 30 index of channel in device set

    uint32_t m_tv_sec;                //!< 34 seconds of timestamp at start time of super-frame processing
    uint32_t m_tv_usec;               //!< 38 microseconds of timestamp at start time of super-frame processing
    uint32_t m_crc32;                 //!< 42 CRC32 of the above

    bool operator==(const RemoteMetaDataFEC& rhs)
    {
        // Only the first 6 fields are relevant
        return (m_centerFrequency == rhs.m_centerFrequency)
            && (m_sampleRate == rhs.m_sampleRate)
            && (m_sampleBytes == rhs.m_sampleBytes)
            && (m_sampleBits == rhs.m_sampleBits)
            && (m_nbOriginalBlocks == rhs.m_nbOriginalBlocks)
            && (m_nbFECBlocks == rhs.m_nbFECBlocks);
    }

    void init()
    {
        m_centerFrequency = 0;
        m_sampleRate = 0;
        m_sampleBytes = 0;
        m_sampleBits = 0;
        m_nbOriginalBlocks = 0;
        m_nbFECBlocks = 0;
        m_deviceIndex = 0;
        m_channelIndex = 0;
        m_tv_sec = 0;
        m_tv_usec = 0;
        m_crc32 = 0;
    }
};

struct RemoteHeader
{
    uint16_t m_frameIndex;
    uint8_t  m_blockIndex;
    uint8_t  m_sampleBytes; //!<  number of bytes per sample (2 or 4) for this block
    uint8_t  m_sampleBits;  //!<  number of bits per sample
    uint8_t  m_filler;
    uint16_t m_filler2;

    void init()
    {
        m_frameIndex = 0;
        m_blockIndex = 0;
        m_sampleBytes = 2;
        m_sampleBits = 16;
        m_filler = 0;
        m_filler2 = 0;
    }
};

static const int RemoteUdpSize = UDPSINKFEC_UDPSIZE;
static const int RemoteNbOrginalBlocks = UDPSINKFEC_NBORIGINALBLOCKS;
static const int RemoteNbBytesPerBlock = UDPSINKFEC_UDPSIZE - sizeof(RemoteHeader);

struct RemoteProtectedBlock
{
    uint8_t buf[RemoteNbBytesPerBlock];

    void init() {
        std::fill(buf, buf+RemoteNbBytesPerBlock, 0);
    }
};

struct RemoteSuperBlock
{
    RemoteHeader         m_header;
    RemoteProtectedBlock m_protectedBlock;

    void init()
    {
        m_header.init();
        m_protectedBlock.init();
    }
};
#pragma pack(pop)

struct RemoteTxControlBlock
{
    bool m_complete;
    bool m_processed;
    uint16_t m_frameIndex;
    int m_nbBlocksFEC;
    QString m_dataAddress;
    uint16_t m_dataPort;

    RemoteTxControlBlock()
    {
        m_complete = false;
        m_processed = false;
        m_frameIndex = 0;
        m_nbBlocksFEC = 0;
        m_dataAddress = "127.0.0.1";
        m_dataPort = 9090;
    }
};

struct RemoteRxControlBlock
{
    int  m_blockCount;    //!< number of blocks received for this frame
    int  m_originalCount; //!< number of original blocks received
    int  m_recoveryCount; //!< number of recovery blocks received
    bool m_metaRetrieved; //!< true if meta data (block zero) was retrieved
    int  m_frameIndex;    //!< this frame index or -1 if unset

    RemoteRxControlBlock() {
        m_blockCount = 0;
        m_originalCount = 0;
        m_recoveryCount = 0;
        m_metaRetrieved = false;
        m_frameIndex = -1;
    }
};

class RemoteDataFrame
{
public:
    RemoteDataFrame() {
        m_superBlocks = new RemoteSuperBlock[256]; //!< 128 original bloks + 128 possible recovery blocks
    }
    ~RemoteDataFrame() {
        delete[] m_superBlocks;
    }
    RemoteTxControlBlock m_txControlBlock;
    RemoteRxControlBlock m_rxControlBlock;
    RemoteSuperBlock     *m_superBlocks;
};

#endif /* CHANNEL_REMOTEDATABLOCK_H_ */
