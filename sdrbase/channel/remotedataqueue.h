///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Remote sink channel (Rx) data blocks queue                                    //
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

#ifndef CHANNEL_REMOTEDATAQUEUE_H_
#define CHANNEL_REMOTEDATAQUEUE_H_

#include <QObject>
#include <QMutex>
#include <QQueue>

#include "export.h"

class RemoteDataBlock;

class SDRBASE_API RemoteDataQueue : public QObject {
    Q_OBJECT

public:
    RemoteDataQueue(QObject* parent = NULL);
    ~RemoteDataQueue();

    void push(RemoteDataBlock* dataBlock, bool emitSignal = true);  //!< Push daa block onto queue
    RemoteDataBlock* pop(); //!< Pop message from queue

    int size(); //!< Returns queue size
    void clear(); //!< Empty queue

signals:
    void dataBlockEnqueued();

private:
    QMutex m_lock;
    QQueue<RemoteDataBlock*> m_queue;
};

#endif /* CHANNEL_REMOTEDATAQUEUE_H_ */
