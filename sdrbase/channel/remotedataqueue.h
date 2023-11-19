///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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
#include <QRecursiveMutex>
#include <QQueue>

#include "export.h"

class RemoteDataFrame;

class SDRBASE_API RemoteDataQueue : public QObject {
    Q_OBJECT

public:
    RemoteDataQueue(QObject* parent = nullptr);
    ~RemoteDataQueue();

    void push(RemoteDataFrame* dataFrame, bool emitSignal = true);  //!< Push data frame onto queue
    RemoteDataFrame* pop(); //!< Pop frame from queue

    int size(); //!< Returns queue size
    void clear(); //!< Empty queue

signals:
    void dataBlockEnqueued();

private:
    QRecursiveMutex m_lock;
    QQueue<RemoteDataFrame*> m_queue;
};

#endif /* CHANNEL_REMOTEDATAQUEUE_H_ */
