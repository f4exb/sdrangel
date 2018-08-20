///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) data blocks queue                                 //
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

#ifndef SDRDAEMON_CHANNEL_SDRDAEMONDATAQUEUE_H_
#define SDRDAEMON_CHANNEL_SDRDAEMONDATAQUEUE_H_

#include <QObject>

class DataBlock;

namespace SDRDaemon {

class DataQueue : public QObject {
    Q_OBJECT

public:
    DataQueue(QObject* parent = NULL);
    ~DataQueue();

    void push(DataBlock* message, bool emitSignal = true);  //!< Push daa block onto queue
    DataBlock* pop(); //!< Pop message from queue

    int size(); //!< Returns queue size
    void clear(); //!< Empty queue

signals:
    void dataBlockEnqueued();

private:
    QMutex m_lock;
    QQueue<DataBlock*> m_queue;
};

} // namespace SDRDaemon

#endif /* SDRDAEMON_CHANNEL_SDRDAEMONDATAQUEUE_H_ */
