///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for Tx channels                                                           //
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

#ifndef SDRBASE_CHANNEL_CHANNELSOURCEAPI_H_
#define SDRBASE_CHANNEL_CHANNELSOURCEAPI_H_

#include <QString>
#include <stdint.h>

#include "util/export.h"

class SDRANGEL_API ChannelSourceAPI {
public:
    ChannelSourceAPI();
    virtual ~ChannelSourceAPI() {}

    virtual int getDeltaFrequency() const = 0;
    virtual void getIdentifier(QString& id) = 0;
    virtual void getTitle(QString& title) = 0;

    int getIndexInDeviceSet() const { return m_indexInDeviceSet; }
    void setIndexInDeviceSet(int indexInDeviceSet) { m_indexInDeviceSet = indexInDeviceSet; }
    uint64_t getUID() const { return m_uid; }

private:
    int m_indexInDeviceSet;
    uint64_t m_uid;
};



#endif /* SDRBASE_CHANNEL_CHANNELSOURCEAPI_H_ */
