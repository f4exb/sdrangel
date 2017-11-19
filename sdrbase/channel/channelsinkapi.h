///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for Rx channels                                                           //
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

#ifndef SDRBASE_CHANNEL_CHANNELSINKAPI_H_
#define SDRBASE_CHANNEL_CHANNELSINKAPI_H_

#include <QString>
#include "util/export.h"

class SDRANGEL_API ChannelSinkAPI {
public:
    ChannelSinkAPI();
    virtual ~ChannelSinkAPI() {}

    virtual int getDeltaFrequency() const = 0;
    virtual void getIdentifier(QString& id) = 0;
    virtual void getTitle(QString& title) = 0;

    int getIndexInDeviceSet() const { return m_indexInDeviceSet; }
    void setIndexInDeviceSet(int indexInDeviceSet) { m_indexInDeviceSet = indexInDeviceSet; }

private:
    int m_indexInDeviceSet;
};



#endif /* SDRBASE_CHANNEL_CHANNELSINKAPI_H_ */
