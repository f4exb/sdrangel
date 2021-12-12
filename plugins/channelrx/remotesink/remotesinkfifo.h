///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef REMOTESINK_REMOTESINKFIFO_H_
#define REMOTESINK_REMOTESINKFIFO_H_

#include <vector>

#include <QObject>
#include <QMutex>

#include "channel/remotedatablock.h"

class RemoteSinkFifo : public QObject {
    Q_OBJECT
public:
    RemoteSinkFifo(QObject *parent = nullptr);
    RemoteSinkFifo(unsigned int size, QObject *parent = nullptr);
    ~RemoteSinkFifo();
    void resize(unsigned int size);
    void reset();

    RemoteDataFrame *getDataFrame();
    unsigned int readDataFrame(RemoteDataFrame **dataFrame);
    unsigned int getRemainder();

signals:
    void dataBlockServed();

private:
    std::vector<RemoteDataFrame> m_data;
    int m_size;
    int m_readHead;   //!< index of last data block processed
    int m_servedHead; //!< index of last data block served
    int m_writeHead;  //!< index of next data block to serve
    QMutex m_mutex;

    unsigned int calculateRemainder();
};

#endif // REMOTESINK_REMOTESINKFIFO_H_
