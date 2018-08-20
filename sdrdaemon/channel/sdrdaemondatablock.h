///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) data block                                        //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#ifndef SDRDAEMON_CHANNEL_SDRDAEMONDATABLOCK_H_
#define SDRDAEMON_CHANNEL_SDRDAEMONDATABLOCK_H_

#include <stdint.h>
#include <string.h>
#include "dsp/dsptypes.h"

#define UDPSINKFEC_UDPSIZE 512
#define UDPSINKFEC_NBORIGINALBLOCKS 128
//#define UDPSINKFEC_NBTXBLOCKS 8

#pragma pack(push, 1)
struct SDRDaemonMetaDataFEC
{
    uint32_t m_centerFrequency;   //!<  4 center frequency in kHz
    uint32_t m_sampleRate;        //!<  8 sample rate in Hz
    uint8_t  m_sampleBytes;       //!<  9 number of bytes per sample (2 or 3)
    uint8_t  m_sampleBits;        //!< 10 number of effective bits per sample (8 t0 24)
    uint8_t  m_nbOriginalBlocks;  //!< 11 number of blocks with original (protected) data
    uint8_t  m_nbFECBlocks;       //!< 12 number of blocks carrying FEC
    uint32_t m_tv_sec;            //!< 16 seconds of timestamp at start time of super-frame processing
    uint32_t m_tv_usec;           //!< 20 microseconds of timestamp at start time of super-frame processing
    uint32_t m_crc32;             //!< 24 CRC32 of the above

    bool operator==(const SDRDaemonMetaDataFEC& rhs)
    {
        return (memcmp((const void *) this, (const void *) &rhs, 16) == 0); // Only the 16 first bytes are relevant
    }

    void init()
    {
        memset((void *) this, 0, sizeof(SDRDaemonMetaDataFEC));
        m_nbFECBlocks = -1;
    }
};

struct SDRDaemonHeader
{
    uint16_t m_frameIndex;
    uint8_t  m_blockIndex;
    uint8_t  m_filler;
};

static const int SDRDaemonUdpSize = UDPSINKFEC_UDPSIZE;
static const int SDRDaemonNbOrginalBlocks = UDPSINKFEC_NBORIGINALBLOCKS;
static const int SDRDaemonSamplesPerBlock = (UDPSINKFEC_UDPSIZE - sizeof(SDRDaemonHeader)) / (SDR_RX_SAMP_SZ/4);

struct SDRDaemonProtectedBlock
{
    Sample m_samples[SDRDaemonSamplesPerBlock];
};

struct SDRDaemonSuperBlock
{
    SDRDaemonHeader         m_header;
    SDRDaemonProtectedBlock m_protectedBlock;
};
#pragma pack(pop)

struct SDRDaemonTxControlBlock
{
    bool m_processed;
    uint16_t m_frameIndex;
    int m_nbBlocksFEC;
    int m_txDelay;
};

class SDRDaemonDataBlock
{
public:
    SDRDaemonTxControlBlock m_controlBlock;
    SDRDaemonSuperBlock     m_superBlocks[256];
};

#endif /* SDRDAEMON_CHANNEL_SDRDAEMONDATABLOCK_H_ */
