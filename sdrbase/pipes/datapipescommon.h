///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_PIPES_DATAPIPESCOMON_H_
#define SDRBASE_PIPES_DATAPIPESCOMON_H_

#include <QHash>
#include <QMap>
#include <QMutex>

#include "export.h"
#include "util/message.h"

#include "elementpipescommon.h"

class ChannelAPI;
class Feature;
class DataFifo;

class SDRBASE_API DataPipesCommon
{
public:
    typedef ElementPipesCommon::RegistrationKey<ChannelAPI> ChannelRegistrationKey;

    /** Send this message to stakeholders when the garbage collector finds that a channel was deleted */
    class SDRBASE_API MsgReportChannelDeleted : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DataFifo *getFifo() const { return m_fifo; }
        const ChannelRegistrationKey& getChannelRegistrationKey() const { return m_channelRegistrationKey; }

        static MsgReportChannelDeleted* create(const DataFifo *fifo, const ChannelRegistrationKey& channelRegistrationKey) {
            return new MsgReportChannelDeleted(fifo, channelRegistrationKey);
        }

    private:
        const DataFifo *m_fifo;
        ChannelRegistrationKey m_channelRegistrationKey;

        MsgReportChannelDeleted(const DataFifo *fifo, const ChannelRegistrationKey& channelRegistrationKey) :
            Message(),
            m_fifo(fifo),
            m_channelRegistrationKey(channelRegistrationKey)
        { }
    };
};

#endif // SDRBASE_PIPES_DATAPIPESCOMON_H_
