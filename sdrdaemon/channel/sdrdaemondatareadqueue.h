///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) data blocks to read queue                         //
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

#ifndef SDRDAEMON_CHANNEL_SDRDAEMONDATAREADQUEUE_H_
#define SDRDAEMON_CHANNEL_SDRDAEMONDATAREADQUEUE_H_

#include <QQueue>

class SDRDaemonDataBlock;
class Sample;

class SDRDaemonDataReadQueue
{
public:
    SDRDaemonDataReadQueue();
    ~SDRDaemonDataReadQueue();

    void push(SDRDaemonDataBlock* dataBlock); //!< push block on the queue
    SDRDaemonDataBlock* pop();                //!< Pop block from the queue
    void readSample(Sample& s);               //!< Read sample from queue
    uint32_t size() const;                    //!< Returns queue size
    void setSize(uint32_t size);              //!< Sets the queue size

    static const uint32_t MinimumMaxSize;

private:
    QQueue<SDRDaemonDataBlock*> m_dataReadQueue;
    SDRDaemonDataBlock *m_dataBlock;
    uint32_t m_maxSize;
    uint32_t m_blockIndex;
    uint32_t m_sampleIndex;
    bool m_full; //!< full condition was hit
};



#endif /* SDRDAEMON_CHANNEL_SDRDAEMONDATAREADQUEUE_H_ */
